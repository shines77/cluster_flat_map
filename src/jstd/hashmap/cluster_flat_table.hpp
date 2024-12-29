/************************************************************************************

  CC BY-SA 4.0 License

  Copyright (c) 2024 XiongHui Guo (gz_shines at msn.com)

  https://github.com/shines77/jstd_cluster_flat_table
  https://gitee.com/shines77/jstd_cluster_flat_table

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

#ifndef JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
#define JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP

#pragma once

namespace jstd {

template <typename map_types, typename Hash,
          typename KeyEqual, typename Allocator>
class cluster_flat_table
{
public:
    typedef flat_map_types<Key, Value>          hashmap_types;
    typedef std::size_t                         size_type;
    typedef std::intptr_t                       ssize_type;

    typedef Key                                 key_type;
    typedef Value                               mapped_type;
    typedef typename hashmap_types::value_type  value_type;
    typedef typename hashmap_types::init_type   init_type;
    typedef Hash                                hasher_t;
    typedef KeyEqual                            key_equal_t;
    typedef Allocator                           allocator_type;

    typedef value_type &                        reference;
    typedef value_type const &                  const_reference;

    typedef typename std::allocator_traits<allocator_type>::pointer         pointer;
    typedef typename std::allocator_traits<allocator_type>::const_pointer   const_pointer;

    static constexpr bool kUseIndexSalt = false;

    static constexpr bool kIsTransparent = (detail::is_transparent<Hash>::value && detail::is_transparent<KeyEqual>::value);

    static constexpr std::uint8_t kEmptySlot    = 0b1111111111;
    static constexpr std::uint8_t kDelectedSlot = 0b1111111110;

    static constexpr size_type npos = static_cast<size_type>(-1);

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

    static constexpr bool is_slot_trivial_copyable =
            (std::is_trivially_copyable<value_type>::value ||
            (std::is_trivially_copyable<key_type>::value &&
             std::is_trivially_copyable<mapped_type>::value) ||
            (std::is_scalar<key_type>::value && std::is_scalar<mapped_type>::value));

    static constexpr bool is_slot_trivial_destructor =
            (std::is_trivially_destructible<value_type>::value ||
            (std::is_trivially_destructible<key_type>::value &&
             std::is_trivially_destructible<mapped_type>::value) ||
            (detail::is_plain_type<key_type>::value &&
             detail::is_plain_type<mapped_type>::value));

    static constexpr bool kIsSmallKeyType   = (sizeof(key_type)   <= sizeof(std::size_t) * 2);
    static constexpr bool kIsSmallValueType = (sizeof(value_type) <= sizeof(std::size_t) * 2);

    static constexpr bool kDetectIsIndirectKey = !(detail::is_plain_type<key_type>::value ||
                                                   (sizeof(key_type) <= sizeof(std::size_t)) ||
                                                   (sizeof(key_type) <= sizeof(std::uint64_t)) ||
                                                  ((sizeof(key_type) <= (sizeof(std::size_t) * 2)) &&
                                                    is_slot_trivial_destructor));

    static constexpr bool kDetectIsIndirectValue = !(detail::is_plain_type<mapped_type>::value ||
                                                     (sizeof(mapped_type) <= sizeof(std::size_t)) ||
                                                     (sizeof(mapped_type) <= sizeof(std::uint64_t)) ||
                                                    ((sizeof(mapped_type) <= (sizeof(std::size_t) * 2)) &&
                                                      is_slot_trivial_destructor));

    template <typename Key, typename Value>
    class slot_storge {
        //
    };

    cluster_flat_table() : cluster_flat_table(0) {}

    explicit cluster_flat_map(size_type size, hasher_t const & hash = hasher_t(),
                              key_equal_t const & pred = key_equal_t(),
                              allocator_type const & allocator = allocator_type()) {
    }

    cluster_flat_table(cluster_flat_table const & other) {
    }

    cluster_flat_table(cluster_flat_table const & other, allocator_type const & allocator) {
    }

    cluster_flat_table(cluster_flat_table && other) {
    }

    cluster_flat_table(cluster_flat_table && other, allocator_type const & allocator) {
    }
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
