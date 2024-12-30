/************************************************************************************

  CC BY-SA 4.0 License

  Copyright (c) 2024 XiongHui Guo (gz_shines at msn.com)

  https://github.com/shines77/cluster_flat_table
  https://gitee.com/shines77/cluster_flat_table

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
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "jstd/hashmap/detail/hashmap_traits.h"
#include "jstd/hashmap/flat_map_type_policy.hpp"
#include "jstd/hashmap/cluster_flat_table.hpp"

namespace jstd {

template <typename TypePolicy, typename Hash,
          typename KeyEqual, typename Allocator>
class cluster_flat_table;

template <typename Key, typename Value,
          typename Hash = std::hash< typename std::remove_const<Key>::type >,
          typename KeyEqual = std::equal_to< typename std::remove_const<Key>::type >,
          typename Allocator = std::allocator< std::pair<const typename std::remove_const<Key>::type,
                                                         typename std::remove_const<Value>::type> > >
class cluster_flat_map
{
    //
public:
    typedef flat_map_type_policy<Key, Value>    type_policy;
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

    typedef cluster_flat_table<type_policy, Hash, KeyEqual,
        typename std::allocator_traits<Allocator>::template rebind_alloc<value_type>>
                                                table_type;

    typedef typename table_type::ctrl_type      ctrl_type;
    typedef typename table_type::slot_type      slot_type;

    typedef typename table_type::iterator       iterator;
    typedef typename table_type::const_iterator const_iterator;

private:
    table_type table_;

public:
    ///
    /// Constructors
    ///
    cluster_flat_map() : cluster_flat_map(0) {}

    explicit cluster_flat_map(size_type capacity, hasher_t const & hash = hasher_t(),
                              key_equal_t const & pred = key_equal_t(),
                              allocator_type const & allocator = allocator_type())
        : table_(capacity, hash, pred, allocator) {
    }

    cluster_flat_map(size_type capacity, allocator_type const & allocator)
        : cluster_flat_map(capacity, hasher_t(), key_equal_t(), allocator) {
    }

    cluster_flat_map(size_type capacity, hasher_t const & hash, allocator_type const & allocator)
        : cluster_flat_map(capacity, hash, key_equal_t(), allocator) {
    }

    template <typename InputIterator>
    cluster_flat_map(InputIterator first, InputIterator last, allocator_type const & allocator)
        : cluster_flat_map(first, last, size_type(0), hasher_t(), key_equal_t(), allocator) {
    }

    explicit cluster_flat_map(allocator_type const & allocator)
        : cluster_flat_map(0, allocator) {
    }

    template <typename Iterator>
    cluster_flat_map(Iterator first, Iterator last, size_type capacity = 0,
                     hasher_t const & hash = hasher_t(), key_equal_t const & pred = key_equal_t(),
                     allocator_type const & allocator = allocator_type())
        : cluster_flat_map(capacity, hash, pred, allocator) {
        this->insert(first, last);
    }

    template <typename Iterator>
    cluster_flat_map(Iterator first, Iterator last, size_type capacity, allocator_type const & allocator)
        : cluster_flat_map(first, last, size, hasher_t(), key_equal_t(), allocator) {
    }

    template <typename Iterator>
    cluster_flat_map(Iterator first, Iterator last, size_type capacity,
                     hasher_t const & hash, allocator_type const & allocator)
        : cluster_flat_map(first, last, size, hash, key_equal_t(), allocator) {
    }

    cluster_flat_map(cluster_flat_map const & other) : table_(other.table_) {
    }

    cluster_flat_map(cluster_flat_map const & other, allocator_type const & allocator)
        : table_(other.table_, allocator) {
    }

    cluster_flat_map(cluster_flat_map && other)
        noexcept(std::is_nothrow_move_constructible<table_type>::value)
        : table_(std::move(other.table_)) {
    }

    cluster_flat_map(cluster_flat_map && other, allocator_type const & allocator)
        : table_(std::move(other.table_), allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> ilist,
                     size_type capacity = 0, hasher_t const & hash = hasher_t(),
                     key_equal_t const & pred = key_equal_t(),
                     allocator_type const & allocator = allocator_type())
        : cluster_flat_map(ilist.begin(), ilist.end(), size, hash, pred, allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> ilist, allocator_type const & allocator)
        : cluster_flat_map(ilist, size_type(0), hasher_t(), key_equal_t(), allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> init, size_type capacity,
                     allocator_type const & allocator)
        : cluster_flat_map(init, size, hasher_t(), key_equal_t(), allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> init, size_type capacity,
                     hasher_t const & hash, allocator_type const & allocator)
        : cluster_flat_map(init, size, hash, key_equal_t(), allocator) {
    }

    ~cluster_flat_map() = default;

    allocator_type get_allocator() const noexcept {
        return table_.get_allocator();
    }

    ///
    /// Iterators
    ///
    iterator begin() noexcept { return table_.begin(); }
    iterator end() noexcept { return table_.end(); }

    const_iterator begin() const noexcept { return table_.begin(); }
    const_iterator end() const noexcept { return table_.end(); }

    const_iterator cbegin() const noexcept { return table_.cbegin(); }
    const_iterator cend() const noexcept { return table_.cend(); }

    ///
    /// Capacity
    ///
    bool empty() const noexcept { return table_.empty(); }
    size_type size() const noexcept { return table_.size(); }
    size_type capacity() const noexcept { return table_.capacity(); }
    size_type max_size() const noexcept { return table_.max_size(); }

    size_type bucket_count() const noexcept { return table_.bucket_count(); }

    ///
    /// Modifiers
    ///
    void clear() noexcept { table_.clear(); }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        //
    }
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_MAP_HPP
