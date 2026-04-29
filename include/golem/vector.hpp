#pragma once

#include "golem/detail/allocator.hpp"
#include "golem/detail/lifetime.hpp"
#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace golem {

template<typename T, typename Alloc = std::allocator<T>>
class vector
{
    static_assert(!std::is_const_v<T>, "vector<const T> is not allowed");

    using traits = detail::alloc_traits<Alloc>;

public:
    using value_type             = T;
    using allocator_type         = Alloc;
    using size_type              = typename traits::size_type;
    using difference_type        = typename traits::difference_type;
    using reference              = T&;
    using const_reference        = const T&;
    using pointer                = typename traits::pointer;
    using const_pointer          = typename traits::const_pointer;
    using iterator               = T*;
    using const_iterator         = const T*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    // The three core members. compressed_pair holds the allocator with EBO.
    detail::compressed_pair<Alloc, T*> alloc_and_begin_{ Alloc{}, nullptr };
    size_type size_     = 0;
    size_type capacity_ = 0;

    Alloc& alloc()       noexcept { return alloc_and_begin_.first(); }
    const Alloc& alloc() const noexcept { return alloc_and_begin_.first(); }

    T*& begin_ptr() noexcept { return alloc_and_begin_.second; }
    T*  begin_ptr() const noexcept { return alloc_and_begin_.second; }

    // Allocate raw memory for n objects and return the pointer.
    T* allocate(size_type n)
    {
        if (n > max_size())
            throw std::length_error("golem::vector: requested size exceeds max_size");
        return traits::allocate(alloc(), n);
    }

    void deallocate(T* p, size_type n) noexcept
    {
        if (p) traits::deallocate(alloc(), p, n);
    }

    // Transfer existing elements into a new buffer.
    // Uses move-if-noexcept to maintain the strong exception guarantee
    // during reallocation: if T's move constructor can throw, we fall
    // back to copy so that the original buffer stays valid.
    void transfer_to(T* new_buf, size_type count)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T>) {
            for (size_type i = 0; i < count; ++i)
                detail::construct_at(new_buf + i, std::move(begin_ptr()[i]));
        } else {
            // Copy path: if a copy throws partway through, the rollback
            // guard destroys already-constructed elements in the new buffer.
            size_type constructed = 0;
            detail::scope_fail guard{ [&] {
                detail::destroy_range(new_buf, new_buf + constructed);
            }};
            for (size_type i = 0; i < count; ++i) {
                detail::construct_at(new_buf + i, begin_ptr()[i]);
                ++constructed;
            }
            guard.dismiss();
        }
    }

    // Grow to at least new_cap, preserving elements.
    // Gives the strong guarantee: if anything throws, *this is unchanged.
    void reallocate(size_type new_cap)
    {
        T* new_buf = allocate(new_cap);
        detail::scope_fail guard{ [&] { deallocate(new_buf, new_cap); }};

        transfer_to(new_buf, size_);
        guard.dismiss();

        // Old buffer cleanup: destroy old elements, free old memory.
        detail::destroy_range(begin_ptr(), begin_ptr() + size_);
        deallocate(begin_ptr(), capacity_);

        begin_ptr() = new_buf;
        capacity_   = new_cap;
    }

    // Geometric growth policy. Standard says "at least n" and typical
    // implementations use factor 1.5 or 2. We use 2 here.
    size_type next_capacity(size_type needed) const
    {
        const size_type cur = capacity_;
        if (cur >= max_size() / 2)
            return max_size();
        return std::max(needed, cur == 0 ? 1 : cur * 2);
    }

public:
    // --- constructors ---

    vector() noexcept(std::is_nothrow_default_constructible_v<Alloc>) = default;

    explicit vector(const Alloc& a) noexcept
        : alloc_and_begin_(a, nullptr) {}

    explicit vector(size_type count, const Alloc& a = Alloc{})
        : alloc_and_begin_(a, nullptr)
    {
        if (count == 0) return;
        begin_ptr() = allocate(count);
        capacity_   = count;
        detail::scope_fail guard{ [&] { deallocate(begin_ptr(), capacity_); }};
        for (size_type i = 0; i < count; ++i) {
            traits::construct(alloc(), begin_ptr() + i);
            ++size_;
        }
        guard.dismiss();
    }

    vector(size_type count, const T& value, const Alloc& a = Alloc{})
        : alloc_and_begin_(a, nullptr)
    {
        if (count == 0) return;
        begin_ptr() = allocate(count);
        capacity_   = count;
        detail::scope_fail guard{ [&] { deallocate(begin_ptr(), capacity_); }};
        for (size_type i = 0; i < count; ++i) {
            traits::construct(alloc(), begin_ptr() + i, value);
            ++size_;
        }
        guard.dismiss();
    }

    template<std::input_iterator It>
    vector(It first, It last, const Alloc& a = Alloc{})
        : alloc_and_begin_(a, nullptr)
    {
        for (; first != last; ++first)
            push_back(*first);
    }

    vector(std::initializer_list<T> il, const Alloc& a = Alloc{})
        : vector(il.begin(), il.end(), a) {}

    vector(const vector& other)
        : alloc_and_begin_(
            traits::select_on_container_copy_construction(other.alloc()), nullptr)
    {
        if (other.size_ == 0) return;
        begin_ptr() = allocate(other.size_);
        capacity_   = other.size_;
        detail::scope_fail guard{ [&] { deallocate(begin_ptr(), capacity_); }};
        for (size_type i = 0; i < other.size_; ++i) {
            traits::construct(alloc(), begin_ptr() + i, other.begin_ptr()[i]);
            ++size_;
        }
        guard.dismiss();
    }

    vector(vector&& other) noexcept
        : alloc_and_begin_(std::move(other.alloc()), other.begin_ptr())
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        other.begin_ptr() = nullptr;
        other.size_       = 0;
        other.capacity_   = 0;
    }

    vector(const vector& other, const Alloc& a)
        : alloc_and_begin_(a, nullptr)
    {
        if (other.size_ == 0) return;
        begin_ptr() = allocate(other.size_);
        capacity_   = other.size_;
        detail::scope_fail guard{ [&] { deallocate(begin_ptr(), capacity_); }};
        for (size_type i = 0; i < other.size_; ++i) {
            traits::construct(alloc(), begin_ptr() + i, other.begin_ptr()[i]);
            ++size_;
        }
        guard.dismiss();
    }

    vector(vector&& other, const Alloc& a)
        : alloc_and_begin_(a, nullptr)
    {
        if (alloc() == other.alloc()) {
            begin_ptr()       = other.begin_ptr();
            size_             = other.size_;
            capacity_         = other.capacity_;
            other.begin_ptr() = nullptr;
            other.size_       = 0;
            other.capacity_   = 0;
        } else {
            // Different allocator: must move-construct element by element.
            if (other.size_ == 0) return;
            begin_ptr() = allocate(other.size_);
            capacity_   = other.size_;
            detail::scope_fail guard{ [&] { deallocate(begin_ptr(), capacity_); }};
            for (size_type i = 0; i < other.size_; ++i) {
                traits::construct(alloc(), begin_ptr() + i, std::move(other.begin_ptr()[i]));
                ++size_;
            }
            guard.dismiss();
        }
    }

    // --- destructor ---

    ~vector() noexcept
    {
        detail::destroy_range(begin_ptr(), begin_ptr() + size_);
        deallocate(begin_ptr(), capacity_);
    }

    // --- assignment ---

    vector& operator=(const vector& other)
    {
        if (this == &other) return *this;

        if constexpr (traits::propagate_on_container_copy_assignment::value) {
            if (alloc() != other.alloc()) {
                // Must free with old allocator before switching.
                detail::destroy_range(begin_ptr(), begin_ptr() + size_);
                deallocate(begin_ptr(), capacity_);
                begin_ptr() = nullptr;
                size_       = 0;
                capacity_   = 0;
                alloc()     = other.alloc();
            }
        }

        assign(other.begin(), other.end());
        return *this;
    }

    vector& operator=(vector&& other)
        noexcept(traits::propagate_on_container_move_assignment::value
              || traits::is_always_equal::value)
    {
        if (this == &other) return *this;

        if constexpr (traits::propagate_on_container_move_assignment::value) {
            detail::destroy_range(begin_ptr(), begin_ptr() + size_);
            deallocate(begin_ptr(), capacity_);
            alloc()     = std::move(other.alloc());
            begin_ptr() = other.begin_ptr();
            size_       = other.size_;
            capacity_   = other.capacity_;
            other.begin_ptr() = nullptr;
            other.size_       = 0;
            other.capacity_   = 0;
        } else if (alloc() == other.alloc()) {
            detail::destroy_range(begin_ptr(), begin_ptr() + size_);
            deallocate(begin_ptr(), capacity_);
            begin_ptr() = other.begin_ptr();
            size_       = other.size_;
            capacity_   = other.capacity_;
            other.begin_ptr() = nullptr;
            other.size_       = 0;
            other.capacity_   = 0;
        } else {
            // Allocators differ and POCMA is false: fall back to element-wise move.
            assign_move_range(other.begin_ptr(), other.begin_ptr() + other.size_);
        }
        return *this;
    }

    vector& operator=(std::initializer_list<T> il)
    {
        assign(il.begin(), il.end());
        return *this;
    }

    // --- assign helpers ---

    void assign(size_type count, const T& value)
    {
        clear();
        reserve(count);
        for (size_type i = 0; i < count; ++i)
            push_back(value);
    }

    template<std::input_iterator It>
    void assign(It first, It last)
    {
        clear();
        for (; first != last; ++first)
            push_back(*first);
    }

    void assign(std::initializer_list<T> il)
    {
        assign(il.begin(), il.end());
    }

private:
    void assign_move_range(T* first, T* last)
    {
        clear();
        for (; first != last; ++first)
            push_back(std::move(*first));
    }

public:
    // --- allocator ---

    allocator_type get_allocator() const noexcept { return alloc(); }

    // --- element access ---

    reference at(size_type i)
    {
        if (i >= size_) throw std::out_of_range("golem::vector::at: index out of range");
        return begin_ptr()[i];
    }

    const_reference at(size_type i) const
    {
        if (i >= size_) throw std::out_of_range("golem::vector::at: index out of range");
        return begin_ptr()[i];
    }

    reference       operator[](size_type i)       noexcept { return begin_ptr()[i]; }
    const_reference operator[](size_type i) const noexcept { return begin_ptr()[i]; }

    reference       front()       noexcept { return begin_ptr()[0]; }
    const_reference front() const noexcept { return begin_ptr()[0]; }

    reference       back()        noexcept { return begin_ptr()[size_ - 1]; }
    const_reference back()  const noexcept { return begin_ptr()[size_ - 1]; }

    T*       data()       noexcept { return begin_ptr(); }
    const T* data() const noexcept { return begin_ptr(); }

    // --- iterators ---

    iterator begin()  noexcept { return begin_ptr(); }
    iterator end()    noexcept { return begin_ptr() + size_; }

    const_iterator begin()  const noexcept { return begin_ptr(); }
    const_iterator end()    const noexcept { return begin_ptr() + size_; }
    const_iterator cbegin() const noexcept { return begin_ptr(); }
    const_iterator cend()   const noexcept { return begin_ptr() + size_; }

    reverse_iterator rbegin()  noexcept { return reverse_iterator(end()); }
    reverse_iterator rend()    noexcept { return reverse_iterator(begin()); }

    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    const_reverse_iterator crend()   const noexcept { return const_reverse_iterator(cbegin()); }

    // --- capacity ---

    bool      empty()    const noexcept { return size_ == 0; }
    size_type size()     const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }

    size_type max_size() const noexcept
    {
        return std::min(
            static_cast<size_type>(std::numeric_limits<difference_type>::max()),
            traits::max_size(alloc())
        );
    }

    void reserve(size_type new_cap)
    {
        if (new_cap <= capacity_) return;
        reallocate(new_cap);
    }

    void shrink_to_fit()
    {
        if (size_ == capacity_) return;
        if (size_ == 0) {
            deallocate(begin_ptr(), capacity_);
            begin_ptr() = nullptr;
            capacity_   = 0;
            return;
        }
        reallocate(size_);
    }

    // --- modifiers ---

    void clear() noexcept
    {
        detail::destroy_range(begin_ptr(), begin_ptr() + size_);
        size_ = 0;
    }

    void push_back(const T& value)
    {
        // Detect self-reference before potential reallocation moves the object.
        if (size_ == capacity_) {
            // If value lives inside our buffer, copy it first.
            if (static_cast<const void*>(&value) >= begin_ptr() &&
                static_cast<const void*>(&value) < begin_ptr() + size_)
            {
                T copy(value);
                reallocate(next_capacity(size_ + 1));
                traits::construct(alloc(), end(), std::move(copy));
            } else {
                reallocate(next_capacity(size_ + 1));
                traits::construct(alloc(), end(), value);
            }
        } else {
            traits::construct(alloc(), end(), value);
        }
        ++size_;
    }

    void push_back(T&& value)
    {
        if (size_ == capacity_)
            reallocate(next_capacity(size_ + 1));
        traits::construct(alloc(), end(), std::move(value));
        ++size_;
    }

    template<typename... Args>
    reference emplace_back(Args&&... args)
    {
        if (size_ == capacity_)
            reallocate(next_capacity(size_ + 1));
        traits::construct(alloc(), end(), std::forward<Args>(args)...);
        ++size_;
        return back();
    }

    void pop_back() noexcept
    {
        --size_;
        traits::destroy(alloc(), begin_ptr() + size_);
    }

    // Single-element insert at position.
    // Invalidates all iterators at and after the insertion point.
    // Basic guarantee if T's move/copy throws; strong guarantee only on push_back.
    iterator insert(const_iterator pos, const T& value)
    {
        size_type idx = static_cast<size_type>(pos - begin_ptr());
        push_back(value);
        std::rotate(begin_ptr() + idx, begin_ptr() + size_ - 1, begin_ptr() + size_);
        return begin_ptr() + idx;
    }

    iterator insert(const_iterator pos, T&& value)
    {
        size_type idx = static_cast<size_type>(pos - begin_ptr());
        push_back(std::move(value));
        std::rotate(begin_ptr() + idx, begin_ptr() + size_ - 1, begin_ptr() + size_);
        return begin_ptr() + idx;
    }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args)
    {
        size_type idx = static_cast<size_type>(pos - begin_ptr());
        emplace_back(std::forward<Args>(args)...);
        std::rotate(begin_ptr() + idx, begin_ptr() + size_ - 1, begin_ptr() + size_);
        return begin_ptr() + idx;
    }

    // Erase element at pos. Invalidates iterators at and after the erased element.
    iterator erase(const_iterator pos) noexcept
    {
        size_type idx = static_cast<size_type>(pos - begin_ptr());
        std::move(begin_ptr() + idx + 1, begin_ptr() + size_, begin_ptr() + idx);
        pop_back();
        return begin_ptr() + idx;
    }

    iterator erase(const_iterator first, const_iterator last) noexcept
    {
        if (first == last) return begin_ptr() + (first - begin_ptr());
        size_type idx_first = static_cast<size_type>(first - begin_ptr());
        size_type idx_last  = static_cast<size_type>(last  - begin_ptr());
        size_type n_erased  = idx_last - idx_first;
        std::move(begin_ptr() + idx_last, begin_ptr() + size_, begin_ptr() + idx_first);
        for (size_type i = 0; i < n_erased; ++i)
            pop_back();
        return begin_ptr() + idx_first;
    }

    void resize(size_type count)
    {
        if (count < size_) {
            detail::destroy_range(begin_ptr() + count, begin_ptr() + size_);
            size_ = count;
        } else if (count > size_) {
            reserve(count);
            for (size_type i = size_; i < count; ++i) {
                traits::construct(alloc(), begin_ptr() + i);
                ++size_;
            }
        }
    }

    void resize(size_type count, const T& value)
    {
        if (count < size_) {
            detail::destroy_range(begin_ptr() + count, begin_ptr() + size_);
            size_ = count;
        } else if (count > size_) {
            reserve(count);
            for (size_type i = size_; i < count; ++i) {
                traits::construct(alloc(), begin_ptr() + i, value);
                ++size_;
            }
        }
    }

    void swap(vector& other)
        noexcept(traits::propagate_on_container_swap::value
              || traits::is_always_equal::value)
    {
        using std::swap;
        if constexpr (traits::propagate_on_container_swap::value)
            swap(alloc(), other.alloc());
        swap(alloc_and_begin_.second, other.alloc_and_begin_.second);
        swap(size_,     other.size_);
        swap(capacity_, other.capacity_);
    }
};

// --- free swap ---

template<typename T, typename Alloc>
void swap(vector<T, Alloc>& a, vector<T, Alloc>& b)
    noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

// --- comparisons ---

template<typename T, typename Alloc>
bool operator==(const vector<T, Alloc>& a, const vector<T, Alloc>& b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

template<typename T, typename Alloc>
auto operator<=>(const vector<T, Alloc>& a, const vector<T, Alloc>& b)
{
    return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end());
}

} // namespace golem
