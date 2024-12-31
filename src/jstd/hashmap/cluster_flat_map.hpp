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
    typedef Hash                                hasher;
    typedef KeyEqual                            key_equal;
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

    explicit cluster_flat_map(size_type capacity, hasher const & hash = hasher(),
                              key_equal const & pred = key_equal(),
                              allocator_type const & allocator = allocator_type())
        : table_(capacity, hash, pred, allocator) {
    }

    cluster_flat_map(size_type capacity, allocator_type const & allocator)
        : cluster_flat_map(capacity, hasher(), key_equal(), allocator) {
    }

    cluster_flat_map(size_type capacity, hasher const & hash, allocator_type const & allocator)
        : cluster_flat_map(capacity, hash, key_equal(), allocator) {
    }

    template <typename InputIterator>
    cluster_flat_map(InputIterator first, InputIterator last, allocator_type const & allocator)
        : cluster_flat_map(first, last, size_type(0), hasher(), key_equal(), allocator) {
    }

    explicit cluster_flat_map(allocator_type const & allocator)
        : cluster_flat_map(0, allocator) {
    }

    template <typename Iterator>
    cluster_flat_map(Iterator first, Iterator last, size_type capacity = 0,
                     hasher const & hash = hasher(), key_equal const & pred = key_equal(),
                     allocator_type const & allocator = allocator_type())
        : cluster_flat_map(capacity, hash, pred, allocator) {
        this->insert(first, last);
    }

    template <typename Iterator>
    cluster_flat_map(Iterator first, Iterator last, size_type capacity, allocator_type const & allocator)
        : cluster_flat_map(first, last, size, hasher(), key_equal(), allocator) {
    }

    template <typename Iterator>
    cluster_flat_map(Iterator first, Iterator last, size_type capacity,
                     hasher const & hash, allocator_type const & allocator)
        : cluster_flat_map(first, last, size, hash, key_equal(), allocator) {
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
                     size_type capacity = 0, hasher const & hash = hasher(),
                     key_equal const & pred = key_equal(),
                     allocator_type const & allocator = allocator_type())
        : cluster_flat_map(ilist.begin(), ilist.end(), size, hash, pred, allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> ilist, allocator_type const & allocator)
        : cluster_flat_map(ilist, size_type(0), hasher(), key_equal(), allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> init, size_type capacity,
                     allocator_type const & allocator)
        : cluster_flat_map(init, size, hasher(), key_equal(), allocator) {
    }

    cluster_flat_map(std::initializer_list<value_type> init, size_type capacity,
                     hasher const & hash, allocator_type const & allocator)
        : cluster_flat_map(init, size, hash, key_equal(), allocator) {
    }

    ~cluster_flat_map() = default;

    ///
    /// Observers
    ///
    allocator_type get_allocator() const noexcept {
        return table_.get_allocator();
    }

    hasher hash_function() const noexcept {
        return table_.hash_function();
    }

    key_equal key_eq() const noexcept {
        return table_.key_eq();
    }

    static const char * name() noexcept {
        return table_.name();
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

    size_type slot_size() const { return table_.slot_size(); }
    size_type slot_mask() const { return table_.slot_mask(); }
    size_type slot_capacity() const { return table_.slot_capacity(); }
    size_type slot_threshold() const { return table_.slot_threshold(); }

    bool is_valid() const { return table_.is_valid(); }
    bool is_empty() const { return table_.is_empty(); }

    ///
    /// Bucket interface
    ///
    size_type bucket_size(size_type n) const noexcept {
        return table_.bucket_size(n);
    }
    size_type bucket_count() const noexcept {
        return table_.bucket_count();
    }
    size_type max_bucket_count() const noexcept {
        return table_.max_bucket_count();
    }

    size_type bucket(const key_type & key) const {
        return table_.bucket(key);
    }

    ///
    /// Hash policy
    ///
    double load_factor() const { return table_.load_factor(); }
    double max_load_factor() const { return table_.max_load_factor(); }

    ///
    /// Hash policy
    ///
    void reserve(size_type new_capacity, bool read_only = false) {
        table_.reserve(new_capacity, read_only);
    }

    void resize(size_type new_capacity, bool read_only = false) {
        table_.resize(new_capacity, read_only);
    }

    void rehash(size_type new_capacity, bool read_only = false) {
        table_.resize(new_capacity, read_only);
    }

    void shrink_to_fit(bool read_only = false) {
        table_.shrink_to_fit(read_only);
    }

    ///
    /// Lookup
    ///
    size_type count(const key_type & key) const {
        return table_.count(key);
    }

    bool contains(const key_type & key) const {
        return table_.contains(key);
    }

    ///
    /// find(key)
    ///
    iterator find(const key_type & key) {
        return table_.find(key);
    }

    const_iterator find(const key_type & key) const {
        return table_.find(key);
    }

    template <typename KeyT>
    iterator find(const KeyT & key) {
        return table_.find(key);
    }

    template <typename KeyT>
    const_iterator find(const KeyT & key) const {
        return table_.find(key);
    }

    ///
    /// Modifiers
    ///
    void clear(bool need_destroy = false) noexcept {
        table_.clear(need_destroy);
    }

    ///
    /// insert(value)
    ///
    std::pair<iterator, bool> insert(const value_type & value) {
        return table_.emplace(value);
    }

    std::pair<iterator, bool> insert(value_type && value) {
        return table_.emplace(std::move(value));
    }

    template <typename P>
    std::pair<iterator, bool> insert(P && value) {
        return table_.emplace(std::forward<P>(value));
    }

    iterator insert(const_iterator hint, const value_type & value) {
        return table_.emplace(hint, value);
    }

    iterator insert(const_iterator hint, value_type && value) {
        return table_.emplace(hint, std::move(value));
    }

    template <typename P>
    iterator insert(const_iterator hint, P && value) {
        return table_.emplace(hint, std::forward<P>(value));
    }

    template <typename InputIter>
    void insert(InputIter first, InputIter last) {
        table_.insert(first, last);
    }

    void insert(std::initializer_list<value_type> ilist) {
        table_.insert(ilist);
    }

    ///
    /// emplace(args...)
    ///
    template <typename ... Args>
    std::pair<iterator, bool> emplace(Args && ... args) {
        return table_.emplace(std::forward<Args>(args)...);
    }

    template <typename ... Args>
    iterator emplace_hint(const_iterator hint, Args && ... args) {
        return table_.emplace(hint, std::forward<Args>(args)...);
    }

    ///
    /// try_emplace(key, args...)
    ///
    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(const Key & key, Args && ... args) {
        return table_.try_emplace(key, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(Key && key, Args && ... args) {
        return table_.try_emplace(std::move(key), std::forward<Args>(args)...);
    }

    template <typename KeyT, typename ... Args>
    std::pair<iterator, bool> try_emplace(KeyT && key, Args && ... args) {
        return table_.try_emplace(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(const_iterator hint, const Key & key, Args && ... args) {
        return table_.try_emplace(hint, key, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(const_iterator hint, Key && key, Args && ... args) {
        return table_.try_emplace(hint, std::move(key), std::forward<Args>(args)...);
    }

    template <typename KeyT, typename ... Args>
    std::pair<iterator, bool> try_emplace(const_iterator hint, KeyT && key, Args && ... args) {
        return table_.try_emplace(hint, std::forward<KeyT>(key), std::forward<Args>(args)...);
    }
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_MAP_HPP
