
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

} // namespace detail
} // namespace jstd

#endif // JSTD_HASHMAP_DETAIL_HASHMAP_TRAITS_H
