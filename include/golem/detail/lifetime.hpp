#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace golem::detail {

// Construct an object of type T at address p using placement new.
// Equivalent to std::construct_at but explicit about what is happening.
template<typename T, typename... Args>
constexpr T* construct_at(T* p, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>)
{
    return ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
}

// Destroy the object at p without freeing memory.
template<typename T>
constexpr void destroy_at(T* p) noexcept
{
    p->~T();
}

// Destroy a range [first, last). Each element must be live.
template<typename T>
constexpr void destroy_range(T* first, T* last) noexcept
{
    for (; first != last; ++first)
        destroy_at(first);
}

// Raw aligned storage for one object of type T.
// Does not construct or destroy anything on its own.
template<typename T>
struct alignas(T) raw_storage
{
    alignas(T) unsigned char bytes[sizeof(T)];

    T* ptr() noexcept
    {
        return reinterpret_cast<T*>(bytes);
    }

    const T* ptr() const noexcept
    {
        return reinterpret_cast<const T*>(bytes);
    }
};

// Scope guard that calls a rollback function unless dismissed.
// Used to maintain invariants when construction can throw partway through.
template<typename F>
struct scope_fail
{
    F fn;
    bool active = true;

    explicit scope_fail(F f) : fn(std::move(f)) {}

    scope_fail(const scope_fail&) = delete;
    scope_fail& operator=(const scope_fail&) = delete;

    void dismiss() noexcept { active = false; }

    ~scope_fail() noexcept
    {
        if (active)
            fn();
    }
};

template<typename F>
scope_fail(F) -> scope_fail<F>;

} // namespace golem::detail
