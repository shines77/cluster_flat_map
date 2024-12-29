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

#include "jstd/hashmap/flat_map_iterator.hpp"

namespace jstd {

template <typename TypePolicy, typename Hash,
          typename KeyEqual, typename Allocator>
class cluster_flat_table
{
public:
    typedef TypePolicy                          type_policy;
    typedef std::size_t                         size_type;
    typedef std::intptr_t                       ssize_type;
    typedef std::ptrdiff_t                      difference_type;

    typedef Key                                 key_type;
    typedef Value                               mapped_type;
    typedef typename type_policy::value_type    value_type;
    typedef typename type_policy::init_type     init_type;
    typedef Hash                                hasher_t;
    typedef KeyEqual                            key_equal_t;
    typedef Allocator                           allocator_type;

    typedef value_type &                        reference;
    typedef value_type const &                  const_reference;

    typedef typename std::allocator_traits<allocator_type>::pointer         pointer;
    typedef typename std::allocator_traits<allocator_type>::const_pointer   const_pointer;

    using this_type = cluster_flat_table<TypePolicy, Hash, KeyEqual, Allocator>;

    static constexpr bool kUseIndexSalt = false;

    static constexpr bool kIsTransparent = (detail::is_transparent<Hash>::value && detail::is_transparent<KeyEqual>::value);

    static constexpr std::uint8_t kEmptySlot  = 0b1111111111;
    static constexpr std::uint8_t kUnusedSlot = 0b1111111110;

    static constexpr size_type npos = static_cast<size_type>(-1);

    struct meta_ctrl {
        std::uint8_t hash;

        meta_ctrl(std::uint8_t hash = kEmptySlot) : hash(hash) {}
        ~meta_ctrl() {}

        bool is_empty_slot() const {
            return (hash == kEmptySlot);
        }

        bool is_unused_slot() const {
            return (hash == kUnusedSlot);
        }

        bool is_mark_slot() const {
            return (hash >= kUnusedSlot);
        }

        bool is_normal_slot() const {
            return (hash < kUnusedSlot);
        }

        bool is_equals(std::uint8_t hash) {
            return (hash == this->hash);
        }
    };

    using ctrl_type = meta_ctrl;

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

    static constexpr bool kIsSmallKeyType   = (sizeof(key_type)    <= sizeof(std::size_t) * 2);
    static constexpr bool kIsSmallValueType = (sizeof(mapped_type) <= sizeof(std::size_t) * 2);

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

    template <typename key_type, typename mapped_type>
    class slot_storge {
        //
    };

    using slot_type = slot_storge<key_type, mapped_type>;

    using iterator       = flat_map_iterator<this_type, value_type>;
    using const_iterator = flat_map_iterator<this_type, const value_type>;

private:
    // Default load factor = 224 / 256 = 0.875
    static constexpr size_type kDefaultMaxLoadFactorNum = 224;
    static constexpr size_type kDefaultMaxLoadFactorDen = 256;

    ctrl_type *     ctrls_;
    slot_type *     slots_;
    size_type       slot_size_;
    size_type       slot_mask_;     // capacity = slot_mask + 1
    size_type       slot_threshold_;
    size_type       max_load_factor_;

    allocator_type  al_;

    static constexpr size_type calcDefaultLoadFactor(size_type capacity) {
        return capacity * kDefaultMaxLoadFactorNum / kDefaultMaxLoadFactorDen;
    }

public:
    cluster_flat_table() : cluster_flat_table(0) {}

    explicit cluster_flat_map(size_type capacity, hasher_t const & hash = hasher_t(),
                              key_equal_t const & pred = key_equal_t(),
                              allocator_type const & allocator = allocator_type())
        : ctrls_(nullptr), slots_(nullptr), slot_size_(0), slot_mask_(static_cast<size_type>(capacity - 1)),
          slot_threshold_(calcDefaultLoadFactor(capacity)), max_load_factor_(kDefaultMaxLoadFactorNum) {
    }

    cluster_flat_table(cluster_flat_table const & other) {
    }

    cluster_flat_table(cluster_flat_table const & other, allocator_type const & allocator) {
    }

    cluster_flat_table(cluster_flat_table && other) {
    }

    cluster_flat_table(cluster_flat_table && other, allocator_type const & allocator) {
    }

    allocator_type get_allocator() const noexcept {
        return al_;
    }

    ///
    /// Iterators
    ///
    iterator begin() noexcept { return iterator(); }
    iterator end() noexcept { return iterator(); }

    const_iterator begin() const noexcept { return const_iterator(); }
    const_iterator end() const noexcept { return const_iterator(); }

    const_iterator cbegin() const noexcept { return const_iterator(); }
    const_iterator cend() const noexcept { return const_iterator(); }

    ///
    /// Capacity
    ///
    bool empty() const noexcept { return (this->size() == 0); }
    size_type size() const noexcept { return this->slot_size_; }
    size_type capacity() const noexcept { return (this->slot_mask_ + 1); }
    size_type max_size() const noexcept { return this->capacity(); }

    size_type bucket_count() const noexcept { return this->capacity(); }
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
