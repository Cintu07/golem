#pragma once

#include <memory>
#include <type_traits>

namespace golem::detail {

// Thin wrapper around std::allocator_traits so callers do not repeat
// the verbose trait lookups everywhere.
template<typename Alloc>
struct alloc_traits : std::allocator_traits<Alloc>
{
    using base = std::allocator_traits<Alloc>;
    using typename base::value_type;
    using typename base::pointer;
    using typename base::size_type;

    static pointer allocate(Alloc& a, size_type n)
    {
        return base::allocate(a, n);
    }

    static void deallocate(Alloc& a, pointer p, size_type n) noexcept
    {
        base::deallocate(a, p, n);
    }

    template<typename T, typename... Args>
    static void construct(Alloc& a, T* p, Args&&... args)
        noexcept(noexcept(base::construct(a, p, std::forward<Args>(args)...)))
    {
        base::construct(a, p, std::forward<Args>(args)...);
    }

    template<typename T>
    static void destroy(Alloc& a, T* p) noexcept
    {
        base::destroy(a, p);
    }
};

// compressed_pair stores two members but collapses the size when one is
// an empty class (stateless allocator, empty comparator, etc.).
// Uses EBO (empty base optimization) via inheritance.
template<typename T, typename U, bool = std::is_empty_v<T> && !std::is_final_v<T>>
struct compressed_pair;

// EBO active: T is empty, inherit from it.
template<typename T, typename U>
struct compressed_pair<T, U, true> : private T
{
    U second;

    template<typename T1, typename U1>
    compressed_pair(T1&& t, U1&& u)
        : T(std::forward<T1>(t)), second(std::forward<U1>(u)) {}

    T&       first()       noexcept { return *this; }
    const T& first() const noexcept { return *this; }
};

// EBO not active: T is not empty or is final.
template<typename T, typename U>
struct compressed_pair<T, U, false>
{
    T first_;
    U second;

    template<typename T1, typename U1>
    compressed_pair(T1&& t, U1&& u)
        : first_(std::forward<T1>(t)), second(std::forward<U1>(u)) {}

    T&       first()       noexcept { return first_; }
    const T& first() const noexcept { return first_; }
};

} // namespace golem::detail
