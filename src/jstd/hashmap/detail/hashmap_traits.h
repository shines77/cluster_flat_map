
#ifndef JSTD_HASHMAP_DETAIL_HASHMAP_TRAITS_H
#define JSTD_HASHMAP_DETAIL_HASHMAP_TRAITS_H

#pragma once

#include <cstdint>
#include <type_traits>

namespace jstd {
namespace detail {

// Struct void_wrapper
struct void_wrapper {
    void_wrapper() {}

    template <typename ... Args>
    void operator () (Args && ... args) const {
        return void();
    }
};

template <typename... Ts>
struct make_void {
    typedef void type;
};

// Alias template void_t
template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

template <typename T, typename = void>
struct is_transparent : std::false_type {};

template <typename T>
struct is_transparent<T, void_t<typename T::is_transparent>>
    : std::true_type {};

//
// From: https://github.com/chxuan/easypack/blob/master/easypack/boost_serialization/traits_util.hpp
//

// has_begin_end
template <typename T>
struct has_begin_end {
    template <typename U>
    static constexpr auto check(void *) -> decltype(std::declval<U>().begin(), std::declval<U>().end(), std::true_type());

    template <typename U>
    static constexpr std::false_type check(...);

    static constexpr bool value = std::is_same<decltype(check<T>(nullptr)), std::true_type>::value;
};

template <typename T>
struct has_iterator {
    template <typename U>
    static constexpr std::true_type check(typename U::iterator *);

    template <typename U>
    static constexpr std::false_type check(...);

    static constexpr bool value = std::is_same<decltype(check<T>(nullptr)), std::true_type>::value;
};

template <typename T>
struct has_const_iterator {
    template <typename U>
    static constexpr std::true_type check(typename U::const_iterator *);

    template <typename U>
    static constexpr std::false_type check(...);

    static constexpr bool value = std::is_same<decltype(check<T>(nullptr)), std::true_type>::value;
};

template <typename T>
struct has_mapped_type {
    template <typename U>
    static constexpr std::true_type check(typename U::mapped_type *);

    template <typename U>
    static constexpr std::false_type check(...);

    static constexpr bool value = std::is_same<decltype(check<T>(nullptr)), std::true_type>::value;
};

template <typename T, typename ... Args>
struct has_member_swap {
    template <typename U>
    static constexpr auto check(void *)
        -> decltype(std::declval<U>().swap(std::declval<Args>()...), std::true_type()) {
        return std::true_type();
    }

    template <typename>
    static constexpr std::false_type check(...) {
        return std::false_type();
    }

    static constexpr bool value = std::is_same<decltype(check<T>(nullptr)), std::true_type>::value;
};

template <typename T>
struct is_plain_type {
    static constexpr bool value = (std::is_arithmetic<T>::value || std::is_enum<T>::value);
};

template <typename T>
struct is_swappable {
    static constexpr bool value = is_plain_type<T>::value || has_member_swap<T, T &>::value;
};

} // namespace detail
} // namespace jstd

#endif // JSTD_HASHMAP_DETAIL_HASHMAP_TRAITS_H
