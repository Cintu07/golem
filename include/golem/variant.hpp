#pragma once

#include "golem/detail/lifetime.hpp"
#include "golem/detail/type_traits.hpp"
#include <cstddef>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace golem {

struct bad_variant_access : std::exception
{
    const char* what() const noexcept override
    {
        return "bad variant access";
    }
};

// Sentinel type for variant default construction when the first alternative
// is not default-constructible, or as an explicit "empty" state.
struct monostate {};
constexpr bool operator==(monostate, monostate) noexcept { return true; }

// Sentinel index meaning the variant is valueless_by_exception.
inline constexpr std::size_t variant_npos = static_cast<std::size_t>(-1);

// in_place_type and in_place_index tags for variant construction.
template<typename T>  struct in_place_type_t  { explicit in_place_type_t()  = default; };
template<std::size_t I> struct in_place_index_t { explicit in_place_index_t() = default; };

template<typename T>    inline constexpr in_place_type_t<T>  in_place_type{};
template<std::size_t I> inline constexpr in_place_index_t<I> in_place_index{};

namespace detail {

// Max size and alignment of a type pack for the raw storage.
template<typename... Ts>
inline constexpr std::size_t variant_storage_size = std::max({ sizeof(Ts)... });

template<typename... Ts>
inline constexpr std::size_t variant_storage_align = std::max({ alignof(Ts)... });

// Visit implementation via an index-dispatched recursive walk.
// For each index I, if the runtime index equals I, call f with the
// correct type.  Uses compile-time expansion over the index sequence.
template<std::size_t I, typename Storage, typename F, typename... Ts>
decltype(auto) visit_impl(std::size_t idx, Storage& storage, F&& f)
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            return std::invoke(std::forward<F>(f),
                *reinterpret_cast<T*>(storage));
        } else {
            return visit_impl<I + 1, Storage, F, Ts...>(idx, storage, std::forward<F>(f));
        }
    } else {
        throw bad_variant_access{};
    }
}

template<std::size_t I, typename Storage, typename F, typename... Ts>
decltype(auto) visit_impl_const(std::size_t idx, const Storage& storage, F&& f)
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            return std::invoke(std::forward<F>(f),
                *reinterpret_cast<const T*>(storage));
        } else {
            return visit_impl_const<I + 1, Storage, F, Ts...>(idx, storage, std::forward<F>(f));
        }
    } else {
        throw bad_variant_access{};
    }
}

// Destroy the currently active alternative at index idx.
template<std::size_t I, typename Storage, typename... Ts>
void destroy_active(std::size_t idx, Storage& storage) noexcept
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            detail::destroy_at(reinterpret_cast<T*>(storage));
        } else {
            destroy_active<I + 1, Storage, Ts...>(idx, storage);
        }
    }
}

// Copy-construct the alternative at index idx from src into dst.
template<std::size_t I, typename Storage, typename... Ts>
void copy_active(std::size_t idx, const Storage& src, Storage& dst)
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            detail::construct_at(reinterpret_cast<T*>(dst),
                                 *reinterpret_cast<const T*>(src));
        } else {
            copy_active<I + 1, Storage, Ts...>(idx, src, dst);
        }
    }
}

// Move-construct the alternative at index idx from src into dst.
template<std::size_t I, typename Storage, typename... Ts>
void move_active(std::size_t idx, Storage& src, Storage& dst)
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            detail::construct_at(reinterpret_cast<T*>(dst),
                                 std::move(*reinterpret_cast<T*>(src)));
        } else {
            move_active<I + 1, Storage, Ts...>(idx, src, dst);
        }
    }
}

// Copy-assign: same-index path only.
template<std::size_t I, typename Storage, typename... Ts>
void copy_assign_active(std::size_t idx, const Storage& src, Storage& dst)
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            *reinterpret_cast<T*>(dst) = *reinterpret_cast<const T*>(src);
        } else {
            copy_assign_active<I + 1, Storage, Ts...>(idx, src, dst);
        }
    }
}

// Move-assign: same-index path only.
template<std::size_t I, typename Storage, typename... Ts>
void move_assign_active(std::size_t idx, Storage& src, Storage& dst)
{
    if constexpr (I < sizeof...(Ts)) {
        if (idx == I) {
            using T = nth_type_t<I, Ts...>;
            *reinterpret_cast<T*>(dst) = std::move(*reinterpret_cast<T*>(src));
        } else {
            move_assign_active<I + 1, Storage, Ts...>(idx, src, dst);
        }
    }
}

} // namespace detail


template<typename... Ts>
class variant
{
    static_assert(sizeof...(Ts) > 0, "variant must have at least one alternative");
    static_assert(detail::all_unique_v<Ts...>, "variant alternatives must be unique types");
    static_assert(((!std::is_reference_v<Ts>) && ...), "variant alternatives must not be references");
    static_assert(((!std::is_void_v<Ts>)      && ...), "variant alternatives must not be void");
    static_assert(((!std::is_array_v<Ts>)     && ...), "variant alternatives must not be arrays");

    alignas(detail::variant_storage_align<Ts...>)
    unsigned char storage_[detail::variant_storage_size<Ts...>];

    std::size_t index_ = variant_npos;

    void* raw() noexcept { return storage_; }
    const void* raw() const noexcept { return storage_; }

    void destroy_current() noexcept
    {
        if (index_ != variant_npos)
            detail::destroy_active<0, unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
                index_,
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
    }

public:

    variant() noexcept(std::is_nothrow_default_constructible_v<detail::nth_type_t<0, Ts...>>)
        requires std::is_default_constructible_v<detail::nth_type_t<0, Ts...>>
    {
        using T0 = detail::nth_type_t<0, Ts...>;
        detail::construct_at(reinterpret_cast<T0*>(storage_));
        index_ = 0;
    }

    template<typename T, typename... Args>
        requires (detail::index_of_v<T, Ts...> < sizeof...(Ts))
              && std::is_constructible_v<T, Args...>
    explicit variant(in_place_type_t<T>, Args&&... args)
    {
        detail::construct_at(reinterpret_cast<T*>(storage_), std::forward<Args>(args)...);
        index_ = detail::index_of_v<T, Ts...>;
    }

    template<std::size_t I, typename... Args>
        requires (I < sizeof...(Ts))
              && std::is_constructible_v<detail::nth_type_t<I, Ts...>, Args...>
    explicit variant(in_place_index_t<I>, Args&&... args)
    {
        using T = detail::nth_type_t<I, Ts...>;
        detail::construct_at(reinterpret_cast<T*>(storage_), std::forward<Args>(args)...);
        index_ = I;
    }

    // Selects the first Ts... type that T implicitly converts to.
    template<typename T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, variant>)
    variant(T&& value)
    {
        using U = std::remove_cvref_t<T>;
        constexpr std::size_t I = detail::index_of_v<U, Ts...>;
        detail::construct_at(reinterpret_cast<U*>(storage_), std::forward<T>(value));
        index_ = I;
    }

    variant(const variant& other)
        requires (std::is_copy_constructible_v<Ts> && ...)
    {
        if (other.index_ == variant_npos) {
            index_ = variant_npos;
            return;
        }
        detail::copy_active<0,
            unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
            other.index_,
            reinterpret_cast<const unsigned char(&)[detail::variant_storage_size<Ts...>]>(other.storage_),
            reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
        index_ = other.index_;
    }

    variant(variant&& other)
        noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
        requires (std::is_move_constructible_v<Ts> && ...)
    {
        if (other.index_ == variant_npos) {
            index_ = variant_npos;
            return;
        }
        detail::move_active<0,
            unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
            other.index_,
            reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(other.storage_),
            reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
        index_ = other.index_;
    }

    ~variant() noexcept { destroy_current(); }

    variant& operator=(const variant& other)
        requires (std::is_copy_constructible_v<Ts> && ...)
              && (std::is_copy_assignable_v<Ts>    && ...)
    {
        if (this == &other) return *this;

        if (other.index_ == variant_npos) {
            destroy_current();
            index_ = variant_npos;
            return *this;
        }

        if (index_ == other.index_) {
            detail::copy_assign_active<0,
                unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
                index_,
                reinterpret_cast<const unsigned char(&)[detail::variant_storage_size<Ts...>]>(other.storage_),
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
        } else {
            // Different active alternatives.
            // Destroy old, then construct the new one.
            // If construction throws, become valueless_by_exception.
            destroy_current();
            index_ = variant_npos;
            detail::copy_active<0,
                unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
                other.index_,
                reinterpret_cast<const unsigned char(&)[detail::variant_storage_size<Ts...>]>(other.storage_),
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
            index_ = other.index_;
        }
        return *this;
    }

    variant& operator=(variant&& other)
        noexcept((std::is_nothrow_move_constructible_v<Ts> && ...)
              && (std::is_nothrow_move_assignable_v<Ts>    && ...))
        requires (std::is_move_constructible_v<Ts> && ...)
              && (std::is_move_assignable_v<Ts>    && ...)
    {
        if (this == &other) return *this;

        if (other.index_ == variant_npos) {
            destroy_current();
            index_ = variant_npos;
            return *this;
        }

        if (index_ == other.index_) {
            detail::move_assign_active<0,
                unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
                index_,
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(other.storage_),
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
        } else {
            destroy_current();
            index_ = variant_npos;
            detail::move_active<0,
                unsigned char[detail::variant_storage_size<Ts...>], Ts...>(
                other.index_,
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(other.storage_),
                reinterpret_cast<unsigned char(&)[detail::variant_storage_size<Ts...>]>(storage_));
            index_ = other.index_;
        }
        return *this;
    }

    template<typename T, typename... Args>
        requires (detail::index_of_v<T, Ts...> < sizeof...(Ts))
              && std::is_constructible_v<T, Args...>
    T& emplace(Args&&... args)
    {
        destroy_current();
        index_ = variant_npos;
        detail::construct_at(reinterpret_cast<T*>(storage_), std::forward<Args>(args)...);
        index_ = detail::index_of_v<T, Ts...>;
        return *reinterpret_cast<T*>(storage_);
    }

    template<std::size_t I, typename... Args>
        requires (I < sizeof...(Ts))
              && std::is_constructible_v<detail::nth_type_t<I, Ts...>, Args...>
    detail::nth_type_t<I, Ts...>& emplace(Args&&... args)
    {
        using T = detail::nth_type_t<I, Ts...>;
        destroy_current();
        index_ = variant_npos;
        detail::construct_at(reinterpret_cast<T*>(storage_), std::forward<Args>(args)...);
        index_ = I;
        return *reinterpret_cast<T*>(storage_);
    }

    std::size_t index() const noexcept { return index_; }
    bool valueless_by_exception() const noexcept { return index_ == variant_npos; }

    void swap(variant& other)
        noexcept((std::is_nothrow_move_constructible_v<Ts> && ...)
              && (std::is_nothrow_swappable_v<Ts>          && ...))
    {
        if (index_ == other.index_) {
            if (index_ == variant_npos) return;
            visit([&other](auto& self_val) {
                using T = std::remove_reference_t<decltype(self_val)>;
                using std::swap;
                swap(self_val, *reinterpret_cast<T*>(other.storage_));
            });
        } else {
            variant tmp(std::move(*this));
            *this = std::move(other);
            other = std::move(tmp);
        }
    }

    template<typename F>
    decltype(auto) visit(F&& f)
    {
        if (index_ == variant_npos) throw bad_variant_access{};
        return detail::visit_impl<0,
            unsigned char[detail::variant_storage_size<Ts...>], F, Ts...>(
            index_, storage_, std::forward<F>(f));
    }

    template<typename F>
    decltype(auto) visit(F&& f) const
    {
        if (index_ == variant_npos) throw bad_variant_access{};
        return detail::visit_impl_const<0,
            unsigned char[detail::variant_storage_size<Ts...>], F, Ts...>(
            index_, storage_, std::forward<F>(f));
    }
};

template<typename F, typename... Ts>
decltype(auto) visit(F&& f, variant<Ts...>& v)
{
    return v.visit(std::forward<F>(f));
}

template<typename F, typename... Ts>
decltype(auto) visit(F&& f, const variant<Ts...>& v)
{
    return v.visit(std::forward<F>(f));
}

template<typename F, typename... Ts>
decltype(auto) visit(F&& f, variant<Ts...>&& v)
{
    return std::move(v).visit(std::forward<F>(f));
}

template<typename T, typename... Ts>
bool holds_alternative(const variant<Ts...>& v) noexcept
{
    return v.index() == detail::index_of_v<T, Ts...>;
}

// get<I>/get<T> use a visitor-based helper to access storage without
// exposing variant internals as public members.
namespace detail {

template<std::size_t I, typename... Ts>
auto* variant_storage_ptr(variant<Ts...>& v) noexcept
{
    // Access storage via visit: find the right pointer without exposing internals.
    // We use a raw-access helper here that is only valid when index() == I.
    using T = nth_type_t<I, Ts...>;
    T* result = nullptr;
    if (v.index() == I) {
        v.visit([&result](auto& val) {
            if constexpr (std::is_same_v<std::remove_reference_t<decltype(val)>, T>)
                result = &val;
        });
    }
    return result;
}

template<std::size_t I, typename... Ts>
const auto* variant_storage_ptr(const variant<Ts...>& v) noexcept
{
    using T = nth_type_t<I, Ts...>;
    const T* result = nullptr;
    if (v.index() == I) {
        v.visit([&result](const auto& val) {
            if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(val)>>, T>)
                result = &val;
        });
    }
    return result;
}

} // namespace detail

template<std::size_t I, typename... Ts>
detail::nth_type_t<I, Ts...>& get(variant<Ts...>& v)
{
    auto* p = detail::variant_storage_ptr<I>(v);
    if (!p) throw bad_variant_access{};
    return *p;
}

template<std::size_t I, typename... Ts>
const detail::nth_type_t<I, Ts...>& get(const variant<Ts...>& v)
{
    auto* p = detail::variant_storage_ptr<I>(v);
    if (!p) throw bad_variant_access{};
    return *p;
}

template<std::size_t I, typename... Ts>
detail::nth_type_t<I, Ts...>&& get(variant<Ts...>&& v)
{
    auto* p = detail::variant_storage_ptr<I>(v);
    if (!p) throw bad_variant_access{};
    return std::move(*p);
}

template<typename T, typename... Ts>
T& get(variant<Ts...>& v)
{
    return get<detail::index_of_v<T, Ts...>>(v);
}

template<typename T, typename... Ts>
const T& get(const variant<Ts...>& v)
{
    return get<detail::index_of_v<T, Ts...>>(v);
}

template<typename T, typename... Ts>
T&& get(variant<Ts...>&& v)
{
    return get<detail::index_of_v<T, Ts...>>(std::move(v));
}

template<std::size_t I, typename... Ts>
detail::nth_type_t<I, Ts...>* get_if(variant<Ts...>* v) noexcept
{
    if (!v || v->index() != I) return nullptr;
    return detail::variant_storage_ptr<I>(*v);
}

template<std::size_t I, typename... Ts>
const detail::nth_type_t<I, Ts...>* get_if(const variant<Ts...>* v) noexcept
{
    if (!v || v->index() != I) return nullptr;
    return detail::variant_storage_ptr<I>(*v);
}

template<typename T, typename... Ts>
T* get_if(variant<Ts...>* v) noexcept
{
    return get_if<detail::index_of_v<T, Ts...>>(v);
}

template<typename T, typename... Ts>
const T* get_if(const variant<Ts...>* v) noexcept
{
    return get_if<detail::index_of_v<T, Ts...>>(v);
}

template<typename... Ts>
void swap(variant<Ts...>& a, variant<Ts...>& b)
    noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

} // namespace golem
