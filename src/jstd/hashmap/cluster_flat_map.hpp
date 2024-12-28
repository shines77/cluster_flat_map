/************************************************************************************

  CC BY-SA 4.0 License

  Copyright (c) 2024 XiongHui Guo (gz_shines at msn.com)

  https://github.com/shines77/jstd_cluster_flat_map
  https://gitee.com/shines77/jstd_cluster_flat_map

*************************************************************************************

  CC Attribution-ShareAlike 4.0 International

  https://creativecommons.org/licenses/by-sa/4.0/deed.en

  You are free to:

    1. Share -- copy and redistribute the material in any medium or format.

    2. Adapt -- remix, transforn, and build upon the material for any purpose,
    even commerically.

    The licensor cannot revoke these freedoms as long as you follow the license terms.

  Under the following terms:

    * Attribution -- You must give appropriate credit, provide a link to the license,
    and indicate if changes were made. You may do so in any reasonable manner,
    but not in any way that suggests the licensor endorses you or your use.

    * ShareAlike -- If you remix, transform, or build upon the material, you must
    distribute your contributions under the same license as the original.

    * No additional restrictions -- You may not apply legal terms or technological
    measures that legally restrict others from doing anything the license permits.

  Notices:

    * You do not have to comply with the license for elements of the material
    in the public domain or where your use is permitted by an applicable exception
    or limitation.

    * No warranties are given. The license may not give you all of the permissions
    necessary for your intended use. For example, other rights such as publicity,
    privacy, or moral rights may limit how you use the material.

************************************************************************************/

#ifndef JSTD_HASHMAP_CLUSTER_FLAT_MAP_HPP
#define JSTD_HASHMAP_CLUSTER_FLAT_MAP_HPP

#pragma once

#include <stdint.h>

#include <cstdint>
#include <memory>

#include <cstdint>
#include <type_traits>
#include <utility>

#include "jstd/hashmap/detail/hashmap_traits.h"

namespace jstd {

template <typename Key, typename Value,
          typename Hash = std::hash< std::remove_const_t<Key> >,
          typename KeyEqual = std::equal_to< std::remove_const_t<Key> >,
          typename Allocator = std::allocator< std::pair<const Key, Value> > >
class cluster_flat_map
{
    //
public:
    typedef Key             key_type;
    typedef Value           value_type;
    typedef Hash            hasher_t;
    typedef KeyEqual        key_equal_t;
    typedef Allocator       allocator_type;

    static constexpr bool kUseIndexSalt = false;

    static constexpr bool kIsTransparent = (detail::is_transparent<Hash>::value && detail::is_transparent<KeyEqual>::value);

    static constexpr std::uint8_t kEmptySlot    = 0b1111111111;
    static constexpr std::uint8_t kDelectedSlot = 0b1111111110;

    struct meta_data {
        std::uint8_t hash;

        meta_data(std::uint8_t hash = kEmptySlot) : hash(hash) {}
        ~meta_data() {}

        bool is_empty_slot() const {
            return (hash == kEmptySlot);
        }

        bool is_delected_slot() const {
            return (hash == kDelectedSlot);
        }

        bool is_mark_slot() const {
            return (hash >= kDelectedSlot);
        }

        bool is_normal_slot() const {
            return (hash < kDelectedSlot);
        }

        bool is_equals(std::uint8_t hash) {
            return (hash == this->hash);
        }
    };

    static constexpr bool kIsSmallKeyType   = (sizeof(key_type)   <= sizeof(std::size_t) * 2);
    static constexpr bool kIsSmallValueType = (sizeof(value_type) <= sizeof(std::size_t) * 2);

    template <typename Key, typename Value>
    class slot_layout {
        //
    };
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_MAP_HPP
