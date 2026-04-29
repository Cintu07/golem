#pragma once

#include "golem/detail/lifetime.hpp"
#include <compare>
#include <exception>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace golem {

struct bad_optional_access : std::exception
{
    const char* what() const noexcept override
    {
        return "bad optional access: optional is disengaged";
    }
};

struct nullopt_t
{
    explicit constexpr nullopt_t(int) noexcept {}
};

inline constexpr nullopt_t nullopt{ 0 };

struct in_place_t
{
    explicit constexpr in_place_t() noexcept = default;
};

inline constexpr in_place_t in_place{};

template<typename T>
class optional
{
    static_assert(!std::is_reference_v<T>,    "optional<T&> is not supported");
    static_assert(!std::is_void_v<T>,         "optional<void> is not supported");
    static_assert(!std::is_array_v<T>,        "optional<T[]> is not supported");

    detail::raw_storage<T> storage_;
    bool engaged_ = false;

    T*       ptr()       noexcept { return storage_.ptr(); }
    const T* ptr() const noexcept { return storage_.ptr(); }

    void destroy_if_engaged() noexcept
    {
        if (engaged_) {
            detail::destroy_at(ptr());
            engaged_ = false;
        }
    }

public:
    using value_type = T;

    constexpr optional() noexcept : engaged_(false) {}

    constexpr optional(nullopt_t) noexcept : engaged_(false) {}

    optional(const optional& other)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires std::is_copy_constructible_v<T>
    {
        if (other.engaged_) {
            detail::construct_at(ptr(), *other.ptr());
            engaged_ = true;
        }
    }

    optional(optional&& other)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        requires std::is_move_constructible_v<T>
    {
        if (other.engaged_) {
            detail::construct_at(ptr(), std::move(*other.ptr()));
            engaged_ = true;
        }
    }

    template<typename U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, optional>
               && !std::is_same_v<std::remove_cvref_t<U>, in_place_t>
               && std::is_constructible_v<T, U>)
    explicit(!std::is_convertible_v<U, T>)
    optional(U&& value)
        noexcept(std::is_nothrow_constructible_v<T, U>)
    {
        detail::construct_at(ptr(), std::forward<U>(value));
        engaged_ = true;
    }

    template<typename... Args>
        requires std::is_constructible_v<T, Args...>
    explicit optional(in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        detail::construct_at(ptr(), std::forward<Args>(args)...);
        engaged_ = true;
    }

    template<typename U, typename... Args>
        requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    explicit optional(in_place_t, std::initializer_list<U> il, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
    {
        detail::construct_at(ptr(), il, std::forward<Args>(args)...);
        engaged_ = true;
    }

    ~optional() noexcept { destroy_if_engaged(); }

    optional& operator=(nullopt_t) noexcept
    {
        destroy_if_engaged();
        return *this;
    }

    optional& operator=(const optional& other)
        requires std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>
    {
        if (this == &other) return *this;

        if (engaged_ && other.engaged_) {
            *ptr() = *other.ptr();
        } else if (!engaged_ && other.engaged_) {
            detail::construct_at(ptr(), *other.ptr());
            engaged_ = true;
        } else {
            destroy_if_engaged();
        }
        return *this;
    }

    optional& operator=(optional&& other)
        noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
        requires std::is_move_constructible_v<T> && std::is_move_assignable_v<T>
    {
        if (this == &other) return *this;

        if (engaged_ && other.engaged_) {
            *ptr() = std::move(*other.ptr());
        } else if (!engaged_ && other.engaged_) {
            detail::construct_at(ptr(), std::move(*other.ptr()));
            engaged_ = true;
        } else {
            destroy_if_engaged();
        }
        return *this;
    }

    template<typename U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, optional>
               && std::is_constructible_v<T, U>
               && std::is_assignable_v<T&, U>)
    optional& operator=(U&& value)
    {
        if (engaged_) {
            *ptr() = std::forward<U>(value);
        } else {
            detail::construct_at(ptr(), std::forward<U>(value));
            engaged_ = true;
        }
        return *this;
    }

    template<typename... Args>
        requires std::is_constructible_v<T, Args...>
    T& emplace(Args&&... args)
    {
        destroy_if_engaged();
        detail::construct_at(ptr(), std::forward<Args>(args)...);
        engaged_ = true;
        return *ptr();
    }

    template<typename U, typename... Args>
        requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    T& emplace(std::initializer_list<U> il, Args&&... args)
    {
        destroy_if_engaged();
        detail::construct_at(ptr(), il, std::forward<Args>(args)...);
        engaged_ = true;
        return *ptr();
    }

    void reset() noexcept { destroy_if_engaged(); }

    void swap(optional& other)
        noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>)
        requires std::is_move_constructible_v<T> && std::is_swappable_v<T>
    {
        if (engaged_ && other.engaged_) {
            using std::swap;
            swap(*ptr(), *other.ptr());
        } else if (engaged_) {
            detail::construct_at(other.ptr(), std::move(*ptr()));
            other.engaged_ = true;
            destroy_if_engaged();
        } else if (other.engaged_) {
            detail::construct_at(ptr(), std::move(*other.ptr()));
            engaged_ = true;
            other.destroy_if_engaged();
        }
    }

    bool has_value() const noexcept { return engaged_; }
    explicit operator bool() const noexcept { return engaged_; }

    T& operator*() & noexcept { return *ptr(); }
    const T& operator*() const& noexcept { return *ptr(); }
    T&& operator*() && noexcept { return std::move(*ptr()); }
    const T&& operator*() const&& noexcept { return std::move(*ptr()); }

    T* operator->() noexcept { return ptr(); }
    const T* operator->() const noexcept { return ptr(); }

    T& value() &
    {
        if (!engaged_) throw bad_optional_access{};
        return *ptr();
    }

    const T& value() const&
    {
        if (!engaged_) throw bad_optional_access{};
        return *ptr();
    }

    T&& value() &&
    {
        if (!engaged_) throw bad_optional_access{};
        return std::move(*ptr());
    }

    const T&& value() const&&
    {
        if (!engaged_) throw bad_optional_access{};
        return std::move(*ptr());
    }

    template<typename U>
    T value_or(U&& fallback) const&
        requires std::is_copy_constructible_v<T> && std::is_convertible_v<U, T>
    {
        return engaged_ ? *ptr() : static_cast<T>(std::forward<U>(fallback));
    }

    template<typename U>
    T value_or(U&& fallback) &&
        requires std::is_move_constructible_v<T> && std::is_convertible_v<U, T>
    {
        return engaged_ ? std::move(*ptr()) : static_cast<T>(std::forward<U>(fallback));
    }

    // and_then: if engaged, call f(*this) and return its result (must be optional).
    template<typename F>
    auto and_then(F&& f) &
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
        if (engaged_)
            return std::invoke(std::forward<F>(f), *ptr());
        return Result{};
    }

    template<typename F>
    auto and_then(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if (engaged_)
            return std::invoke(std::forward<F>(f), *ptr());
        return Result{};
    }

    template<typename F>
    auto and_then(F&& f) &&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
        if (engaged_)
            return std::invoke(std::forward<F>(f), std::move(*ptr()));
        return Result{};
    }

    template<typename F>
    auto and_then(F&& f) const&&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const T&&>>;
        if (engaged_)
            return std::invoke(std::forward<F>(f), std::move(*ptr()));
        return Result{};
    }

    // transform: if engaged, call f(*this) and wrap the result in optional.
    template<typename F>
    auto transform(F&& f) &
    {
        using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
        if (engaged_)
            return optional<U>{ std::invoke(std::forward<F>(f), *ptr()) };
        return optional<U>{};
    }

    template<typename F>
    auto transform(F&& f) const&
    {
        using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
        if (engaged_)
            return optional<U>{ std::invoke(std::forward<F>(f), *ptr()) };
        return optional<U>{};
    }

    template<typename F>
    auto transform(F&& f) &&
    {
        using U = std::remove_cv_t<std::invoke_result_t<F, T&&>>;
        if (engaged_)
            return optional<U>{ std::invoke(std::forward<F>(f), std::move(*ptr())) };
        return optional<U>{};
    }

    template<typename F>
    auto transform(F&& f) const&&
    {
        using U = std::remove_cv_t<std::invoke_result_t<F, const T&&>>;
        if (engaged_)
            return optional<U>{ std::invoke(std::forward<F>(f), std::move(*ptr())) };
        return optional<U>{};
    }

    // or_else: if disengaged, call f() and return its result (must be optional<T>).
    template<typename F>
        requires std::is_same_v<std::remove_cvref_t<std::invoke_result_t<F>>, optional>
    optional or_else(F&& f) const&
    {
        return engaged_ ? *this : std::invoke(std::forward<F>(f));
    }

    template<typename F>
        requires std::is_same_v<std::remove_cvref_t<std::invoke_result_t<F>>, optional>
    optional or_else(F&& f) &&
    {
        return engaged_ ? std::move(*this) : std::invoke(std::forward<F>(f));
    }
};

template<typename T>
    requires std::is_move_constructible_v<T> && std::is_swappable_v<T>
void swap(optional<T>& a, optional<T>& b)
    noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

template<typename T>
optional<std::decay_t<T>> make_optional(T&& value)
{
    return optional<std::decay_t<T>>{ std::forward<T>(value) };
}

template<typename T, typename... Args>
optional<T> make_optional(Args&&... args)
{
    return optional<T>{ in_place, std::forward<Args>(args)... };
}

template<typename T, typename U>
bool operator==(const optional<T>& a, const optional<U>& b)
{
    if (a.has_value() != b.has_value()) return false;
    if (!a.has_value()) return true;
    return *a == *b;
}

template<typename T>
bool operator==(const optional<T>& o, nullopt_t) noexcept { return !o.has_value(); }

template<typename T>
bool operator==(nullopt_t, const optional<T>& o) noexcept { return !o.has_value(); }

template<typename T, typename U>
bool operator==(const optional<T>& o, const U& value)
{
    return o.has_value() && *o == value;
}

template<typename T, typename U>
bool operator==(const U& value, const optional<T>& o)
{
    return o.has_value() && *o == value;
}

} // namespace golem
