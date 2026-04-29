#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace golem::detail {

// --- type_list helpers used by variant ---

template<typename... Ts>
struct type_list {};

// nth_type<N, Ts...>: get the Nth type in a pack.
template<std::size_t N, typename... Ts>
struct nth_type;

template<std::size_t N, typename T, typename... Rest>
struct nth_type<N, T, Rest...> : nth_type<N - 1, Rest...> {};

template<typename T, typename... Rest>
struct nth_type<0, T, Rest...> { using type = T; };

template<std::size_t N, typename... Ts>
using nth_type_t = typename nth_type<N, Ts...>::type;

// index_of<T, Ts...>: index of the first occurrence of T in pack.
// Produces a compile error via static_assert if T is not in Ts.
template<typename T, typename... Ts>
struct index_of;

template<typename T, typename Head, typename... Tail>
struct index_of<T, Head, Tail...>
{
    static constexpr std::size_t value =
        std::is_same_v<T, Head> ? 0 : 1 + index_of<T, Tail...>::value;
};

template<typename T>
struct index_of<T>
{
    static_assert(sizeof(T) == 0, "type not found in variant alternative list");
    static constexpr std::size_t value = 0;
};

template<typename T, typename... Ts>
inline constexpr std::size_t index_of_v = index_of<T, Ts...>::value;

// all_unique<Ts...>: true if no duplicates in pack.
template<typename... Ts>
struct all_unique : std::true_type {};

template<typename T, typename... Rest>
struct all_unique<T, Rest...>
    : std::bool_constant<!(std::is_same_v<T, Rest> || ...) && all_unique<Rest...>::value> {};

template<typename... Ts>
inline constexpr bool all_unique_v = all_unique<Ts...>::value;

// variant_size<Ts...>
template<typename... Ts>
inline constexpr std::size_t variant_size_v = sizeof...(Ts);

} // namespace golem::detail
