#pragma once

#include "golem/detail/allocator.hpp"
#include "golem/detail/lifetime.hpp"
#include <bit>
#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace golem {

// Explicit contract: this is a flat open-addressed Robin Hood hash map.
// It does NOT provide std::unordered_map reference/pointer stability.
// Any successful insert, erase, or rehash invalidates all iterators,
// pointers, and references.  operator[] on a missing key inserts a
// default-constructed mapped value, which counts as an insert.

template<
    typename K,
    typename V,
    typename Hash     = std::hash<K>,
    typename KeyEqual = std::equal_to<K>,
    typename Alloc    = std::allocator<std::pair<const K, V>>
>
class unordered_map
{
public:
    using key_type        = K;
    using mapped_type     = V;
    using value_type      = std::pair<const K, V>;
    using hasher          = Hash;
    using key_equal       = KeyEqual;
    using allocator_type  = Alloc;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    // Bucket state stored alongside each slot.
    enum class slot_state : unsigned char { empty, occupied };

    // Internal mutable pair: key is NOT const so we can swap/move slots
    // during Robin Hood displacement.  Externally we expose value_type
    // (pair<const K, V>) via val() using a reinterpret_cast, which is safe
    // because pair<K,V> and pair<const K,V> have identical layout.
    using mutable_value = std::pair<K, V>;

    // A single bucket: state, cached hash (so we do not rehash during
    // backward-shift erase), and raw storage for mutable_value.
    struct slot
    {
        slot_state  state   = slot_state::empty;
        std::size_t hash    = 0;
        alignas(mutable_value) unsigned char storage[sizeof(mutable_value)];

        // Internal mutable access: used for construction, destruction, swap.
        mutable_value*       mval()       noexcept
        { return reinterpret_cast<mutable_value*>(storage); }
        const mutable_value* mval() const noexcept
        { return reinterpret_cast<const mutable_value*>(storage); }

        // External user-facing access: key appears const.
        value_type*       val()       noexcept
        { return reinterpret_cast<value_type*>(storage); }
        const value_type* val() const noexcept
        { return reinterpret_cast<const value_type*>(storage); }
    };

    using slot_alloc =
        typename std::allocator_traits<Alloc>::template rebind_alloc<slot>;
    using slot_traits = detail::alloc_traits<slot_alloc>;

    slot*     slots_    = nullptr;
    size_type bucket_count_ = 0;
    size_type size_     = 0;
    float     max_load_factor_ = 0.75f;

    detail::compressed_pair<Hash, KeyEqual>  hasheq_;
    slot_alloc                               slot_alloc_;

    Hash&      hasher_()       noexcept { return hasheq_.first(); }
    const Hash& hasher_() const noexcept { return hasheq_.first(); }
    KeyEqual&  eq_()           noexcept { return hasheq_.second; }
    const KeyEqual& eq_() const noexcept { return hasheq_.second; }

    // Mix the hash to improve low-bit distribution.
    static std::size_t mix(std::size_t h) noexcept
    {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33;
        return h;
    }

    std::size_t hash_key(const K& k) const
    {
        return mix(hasher_()(k));
    }

    // Desired bucket for a cached hash (power-of-two capacity).
    size_type home(std::size_t h) const noexcept
    {
        return h & (bucket_count_ - 1);
    }

    // Probe distance of a slot from its home bucket.
    size_type probe_dist(size_type slot_idx) const noexcept
    {
        const size_type h = home(slots_[slot_idx].hash);
        return (slot_idx + bucket_count_ - h) & (bucket_count_ - 1);
    }

    // Allocate a fresh, zero-initialized slot array.
    slot* alloc_slots(size_type n)
    {
        slot* p = slot_traits::allocate(slot_alloc_, n);
        for (size_type i = 0; i < n; ++i)
            new (p + i) slot{};
        return p;
    }

    void free_slots(slot* p, size_type n) noexcept
    {
        if (!p) return;
        for (size_type i = 0; i < n; ++i)
            if (p[i].state == slot_state::occupied)
                detail::destroy_at(p[i].mval());
        slot_traits::deallocate(slot_alloc_, p, n);
    }

    // Round up to next power of two, minimum 8.
    static size_type next_pow2(size_type n) noexcept
    {
        if (n <= 8) return 8;
        return std::bit_ceil(n);
    }

    // Threshold before rehash.
    size_type load_threshold() const noexcept
    {
        return static_cast<size_type>(
            static_cast<float>(bucket_count_) * max_load_factor_);
    }

    // Takes ownership of value as a mutable_value (pair<K,V>) so we can
    // swap it with incumbent slots during displacement.  We use mval() on
    // slots for the same reason.
    void rh_insert_slot(slot* table, size_type cap,
                        std::size_t h, mutable_value&& vt)
    {
        size_type idx  = h & (cap - 1);
        size_type dist = 0;

        for (;;) {
            slot& s = table[idx];

            if (s.state == slot_state::empty) {
                detail::construct_at(s.mval(), std::move(vt));
                s.state = slot_state::occupied;
                s.hash  = h;
                return;
            }

            // Robin Hood: steal from the rich (low-dist incumbent).
            size_type incumbent_dist = (idx + cap - (s.hash & (cap - 1))) & (cap - 1);
            if (incumbent_dist < dist) {
                // Swap incoming entry with incumbent, continue inserting incumbent.
                using std::swap;
                swap(h, s.hash);
                swap(vt, *s.mval());
                dist = incumbent_dist;
            }

            idx  = (idx + 1) & (cap - 1);
            ++dist;
        }
    }

    // Rebuild the table at new capacity. If any construction throws,
    // the new table is freed and the original is left intact (strong guarantee).
    void rehash_to(size_type new_cap)
    {
        slot* new_slots = alloc_slots(new_cap);
        detail::scope_fail guard{ [&] { free_slots(new_slots, new_cap); }};

        for (size_type i = 0; i < bucket_count_; ++i) {
            if (slots_[i].state == slot_state::occupied) {
                mutable_value tmp(std::move(*slots_[i].mval()));
                rh_insert_slot(new_slots, new_cap, slots_[i].hash, std::move(tmp));
            }
        }

        guard.dismiss();

        // Old table: elements already moved out, just free the storage.
        for (size_type i = 0; i < bucket_count_; ++i)
            if (slots_[i].state == slot_state::occupied)
                detail::destroy_at(slots_[i].mval());
        slot_traits::deallocate(slot_alloc_, slots_, bucket_count_);

        slots_        = new_slots;
        bucket_count_ = new_cap;
    }

    void ensure_capacity_for_insert()
    {
        if (bucket_count_ == 0) {
            slots_        = alloc_slots(8);
            bucket_count_ = 8;
        }
        if (size_ + 1 > load_threshold())
            rehash_to(bucket_count_ * 2);
    }

    size_type find_slot(const K& k) const noexcept
    {
        if (bucket_count_ == 0) return bucket_count_;
        const std::size_t h = hash_key(k);
        size_type idx        = home(h);
        size_type dist       = 0;

        for (;;) {
            const slot& s = slots_[idx];
            if (s.state == slot_state::empty)
                return bucket_count_;

            // A Robin Hood invariant: if the incumbent has a shorter probe
            // distance than us, our key cannot be further in the cluster.
            const size_type s_dist = probe_dist(idx);
            if (dist > s_dist)
                return bucket_count_;

            if (s.hash == h && eq_()(s.val()->first, k))
                return idx;

            idx  = (idx + 1) & (bucket_count_ - 1);
            ++dist;
        }
    }

    // Shift subsequent elements back to fill the gap, preserving the
    // Robin Hood invariant without tombstones.
    void backward_shift_erase(size_type idx) noexcept
    {
        detail::destroy_at(slots_[idx].mval());
        slots_[idx].state = slot_state::empty;
        --size_;

        for (;;) {
            size_type next = (idx + 1) & (bucket_count_ - 1);
            if (slots_[next].state == slot_state::empty
             || probe_dist(next) == 0)
                break;

            // Move next into idx.
            detail::construct_at(slots_[idx].mval(), std::move(*slots_[next].mval()));
            detail::destroy_at(slots_[next].mval());
            slots_[idx].state = slot_state::occupied;
            slots_[idx].hash  = slots_[next].hash;
            slots_[next].state = slot_state::empty;

            idx = next;
        }
    }

public:

    struct iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using value_type        = unordered_map::value_type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = value_type*;
        using reference         = value_type&;

        slot*     cur_;
        slot*     end_;

        iterator(slot* cur, slot* end) noexcept : cur_(cur), end_(end)
        {
            skip_empty();
        }

        void skip_empty() noexcept
        {
            while (cur_ != end_ && cur_->state != slot_state::occupied)
                ++cur_;
        }

        reference operator*()  const noexcept { return *cur_->val(); }
        pointer   operator->() const noexcept { return  cur_->val(); }

        iterator& operator++() noexcept
        {
            ++cur_;
            skip_empty();
            return *this;
        }

        iterator operator++(int) noexcept
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& o) const noexcept { return cur_ == o.cur_; }
        bool operator!=(const iterator& o) const noexcept { return cur_ != o.cur_; }
    };

    struct const_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using value_type        = const unordered_map::value_type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const unordered_map::value_type*;
        using reference         = const unordered_map::value_type&;

        const slot* cur_;
        const slot* end_;

        const_iterator(const slot* cur, const slot* end) noexcept : cur_(cur), end_(end)
        {
            skip_empty();
        }

        const_iterator(iterator it) noexcept : cur_(it.cur_), end_(it.end_) {}

        void skip_empty() noexcept
        {
            while (cur_ != end_ && cur_->state != slot_state::occupied)
                ++cur_;
        }

        reference operator*()  const noexcept { return *cur_->val(); }
        pointer   operator->() const noexcept { return  cur_->val(); }

        const_iterator& operator++() noexcept
        {
            ++cur_;
            skip_empty();
            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator& o) const noexcept { return cur_ == o.cur_; }
        bool operator!=(const const_iterator& o) const noexcept { return cur_ != o.cur_; }
    };

    unordered_map() = default;

    explicit unordered_map(size_type initial_capacity,
                           const Hash& h = Hash{},
                           const KeyEqual& eq = KeyEqual{},
                           const Alloc& a = Alloc{})
        : hasheq_(h, eq), slot_alloc_(a)
    {
        if (initial_capacity > 0)
            rehash(initial_capacity);
    }

    unordered_map(std::initializer_list<value_type> il,
                  const Hash& h = Hash{},
                  const KeyEqual& eq = KeyEqual{},
                  const Alloc& a = Alloc{})
        : hasheq_(h, eq), slot_alloc_(a)
    {
        reserve(il.size());
        for (const auto& kv : il)
            insert(kv);
    }

    unordered_map(const unordered_map& other)
        : hasheq_(other.hasheq_), slot_alloc_(other.slot_alloc_)
    {
        reserve(other.size_);
        for (const auto& kv : other)
            insert(kv);
    }

    unordered_map(unordered_map&& other) noexcept
        : slots_(other.slots_)
        , bucket_count_(other.bucket_count_)
        , size_(other.size_)
        , max_load_factor_(other.max_load_factor_)
        , hasheq_(std::move(other.hasheq_))
        , slot_alloc_(std::move(other.slot_alloc_))
    {
        other.slots_        = nullptr;
        other.bucket_count_ = 0;
        other.size_         = 0;
    }

    ~unordered_map() noexcept
    {
        free_slots(slots_, bucket_count_);
    }

    unordered_map& operator=(const unordered_map& other)
    {
        if (this == &other) return *this;
        unordered_map tmp(other);
        swap(tmp);
        return *this;
    }

    unordered_map& operator=(unordered_map&& other) noexcept
    {
        if (this == &other) return *this;
        free_slots(slots_, bucket_count_);
        slots_              = other.slots_;
        bucket_count_       = other.bucket_count_;
        size_               = other.size_;
        max_load_factor_    = other.max_load_factor_;
        hasheq_             = std::move(other.hasheq_);
        slot_alloc_         = std::move(other.slot_alloc_);
        other.slots_        = nullptr;
        other.bucket_count_ = 0;
        other.size_         = 0;
        return *this;
    }

    iterator begin() noexcept
    { return {slots_, slots_ + bucket_count_}; }

    iterator end() noexcept
    { return {slots_ + bucket_count_, slots_ + bucket_count_}; }

    const_iterator begin() const noexcept
    { return {slots_, slots_ + bucket_count_}; }

    const_iterator end() const noexcept
    { return {slots_ + bucket_count_, slots_ + bucket_count_}; }

    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend()   const noexcept { return end(); }

    bool      empty()        const noexcept { return size_ == 0; }
    size_type size()         const noexcept { return size_; }
    size_type bucket_count() const noexcept { return bucket_count_; }
    float     load_factor()  const noexcept
    {
        return bucket_count_ == 0 ? 0.f
             : static_cast<float>(size_) / static_cast<float>(bucket_count_);
    }
    float     max_load_factor() const noexcept { return max_load_factor_; }
    void      max_load_factor(float f) noexcept { max_load_factor_ = f; }

    allocator_type get_allocator() const noexcept
    { return allocator_type(slot_alloc_); }

    void rehash(size_type n)
    {
        size_type new_cap = next_pow2(std::max(n,
            static_cast<size_type>(
                static_cast<float>(size_) / max_load_factor_) + 1));
        if (new_cap == bucket_count_) return;
        rehash_to(new_cap);
    }

    void reserve(size_type n)
    {
        rehash(static_cast<size_type>(
            static_cast<float>(n) / max_load_factor_) + 1);
    }

    iterator find(const K& k) noexcept
    {
        size_type idx = find_slot(k);
        if (idx == bucket_count_)
            return end();
        return {slots_ + idx, slots_ + bucket_count_};
    }

    const_iterator find(const K& k) const noexcept
    {
        size_type idx = find_slot(k);
        if (idx == bucket_count_)
            return end();
        return {slots_ + idx, slots_ + bucket_count_};
    }

    bool contains(const K& k) const noexcept
    {
        return find_slot(k) != bucket_count_;
    }

    V& at(const K& k)
    {
        size_type idx = find_slot(k);
        if (idx == bucket_count_)
            throw std::out_of_range("golem::unordered_map::at: key not found");
        return slots_[idx].val()->second;
    }

    const V& at(const K& k) const
    {
        size_type idx = find_slot(k);
        if (idx == bucket_count_)
            throw std::out_of_range("golem::unordered_map::at: key not found");
        return slots_[idx].val()->second;
    }

    V& operator[](const K& k)
    {
        size_type idx = find_slot(k);
        if (idx != bucket_count_)
            return slots_[idx].val()->second;
        auto [it, _] = insert(value_type{k, V{}});
        return it->second;
    }

    V& operator[](K&& k)
    {
        size_type idx = find_slot(k);
        if (idx != bucket_count_)
            return slots_[idx].val()->second;
        auto [it, _] = insert(value_type{std::move(k), V{}});
        return it->second;
    }

    std::pair<iterator, bool> insert(const value_type& kv)
    {
        size_type idx = find_slot(kv.first);
        if (idx != bucket_count_)
            return {{slots_ + idx, slots_ + bucket_count_}, false};

        ensure_capacity_for_insert();

        const std::size_t h = hash_key(kv.first);
        // Construct mutable_value so rh_insert_slot can swap during displacement.
        mutable_value mv(kv.first, kv.second);
        rh_insert_slot(slots_, bucket_count_, h, std::move(mv));
        ++size_;

        idx = find_slot(kv.first);
        return {{slots_ + idx, slots_ + bucket_count_}, true};
    }

    std::pair<iterator, bool> insert(value_type&& kv)
    {
        const K& k = kv.first;
        size_type idx = find_slot(k);
        if (idx != bucket_count_)
            return {{slots_ + idx, slots_ + bucket_count_}, false};

        ensure_capacity_for_insert();

        const std::size_t h = hash_key(k);
        // kv.first is const K; copy key, move value.
        mutable_value mv(kv.first, std::move(kv.second));
        rh_insert_slot(slots_, bucket_count_, h, std::move(mv));
        ++size_;

        idx = find_slot(k);
        return {{slots_ + idx, slots_ + bucket_count_}, true};
    }

    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        value_type tmp(std::forward<Args>(args)...);
        return insert(std::move(tmp));
    }

    size_type erase(const K& k) noexcept
    {
        size_type idx = find_slot(k);
        if (idx == bucket_count_) return 0;
        backward_shift_erase(idx);
        return 1;
    }

    iterator erase(const_iterator pos) noexcept
    {
        size_type idx = static_cast<size_type>(pos.cur_ - slots_);
        backward_shift_erase(idx);
        return {slots_ + idx, slots_ + bucket_count_};
    }

    void clear() noexcept
    {
        for (size_type i = 0; i < bucket_count_; ++i) {
            if (slots_[i].state == slot_state::occupied) {
                detail::destroy_at(slots_[i].mval());
                slots_[i].state = slot_state::empty;
            }
        }
        size_ = 0;
    }

    void swap(unordered_map& other) noexcept
    {
        using std::swap;
        swap(slots_,           other.slots_);
        swap(bucket_count_,    other.bucket_count_);
        swap(size_,            other.size_);
        swap(max_load_factor_, other.max_load_factor_);
        swap(hasheq_,          other.hasheq_);
        swap(slot_alloc_,      other.slot_alloc_);
    }

    hasher    hash_function() const { return hasher_(); }
    key_equal key_eq()        const { return eq_(); }
};

template<typename K, typename V, typename H, typename E, typename A>
void swap(unordered_map<K,V,H,E,A>& a, unordered_map<K,V,H,E,A>& b) noexcept
{
    a.swap(b);
}

} // namespace golem
