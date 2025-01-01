/************************************************************************************

  CC BY-SA 4.0 License

  Copyright (c) 2024 XiongHui Guo (gz_shines at msn.com)

  https://github.com/shines77/cluster_flat_map
  https://gitee.com/shines77/cluster_flat_map

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

#define NOMINMAX
#include <stdint.h>

#include <cstdint>
#include <memory>           // For std::allocator<T>
#include <limits>           // For std::numeric_limits<T>
#include <initializer_list>
#include <type_traits>
#include <utility>          // For std::pair<F, S>

#include <assert.h>

#include "jstd/basic/stddef.h"

#include "jstd/support/Power2.h"
#include "jstd/support/BitUtils.h"
#include "jstd/support/CPUPrefetch.h"

#include "jstd/hasher/hashes.h"
#include "jstd/utility/utility.h"

#include "jstd/hashmap/flat_map_iterator.hpp"
#include "jstd/hashmap/flat_map_cluster.hpp"

#include "jstd/hashmap/flat_map_type_policy.hpp"
#include "jstd/hashmap/flat_map_slot_policy.hpp"
#include "jstd/hashmap/slot_policy_traits.h"
#include "jstd/hashmap/flat_map_slot_storage.hpp"

#define CLUSTER_USE_SEPARATE_SLOTS  1

namespace jstd {

template <typename TypePolicy, typename Hash,
          typename KeyEqual, typename Allocator>
class JSTD_DLL cluster_flat_table
{
public:
    typedef TypePolicy                          type_policy;
    typedef std::size_t                         size_type;
    typedef std::intptr_t                       ssize_type;
    typedef std::ptrdiff_t                      difference_type;

    typedef typename type_policy::key_type      key_type;
    typedef typename type_policy::mapped_type   mapped_type;
    typedef typename type_policy::value_type    value_type;
    typedef typename type_policy::init_type     init_type;
    typedef Hash                                hasher;
    typedef KeyEqual                            key_equal;
    typedef Allocator                           allocator_type;
    typedef typename Hash::result_type          hash_result_t;

    typedef value_type &                        reference;
    typedef value_type const &                  const_reference;

    typedef typename std::allocator_traits<allocator_type>::pointer         pointer;
    typedef typename std::allocator_traits<allocator_type>::const_pointer   const_pointer;

    using this_type = cluster_flat_table<TypePolicy, Hash, KeyEqual, Allocator>;

    static constexpr bool kUseIndexSalt = false;
    static constexpr bool kEnableExchange = true;

    static constexpr bool kIsTransparent = (detail::is_transparent<Hash>::value && detail::is_transparent<KeyEqual>::value);
    static constexpr bool kIsLayoutCompatible = jstd::is_layout_compatible_kv<key_type, mapped_type>::value;

    static constexpr size_type npos = static_cast<size_type>(-1);

    using ctrl_type = cluster_meta_ctrl;
    using group_type = flat_map_cluster16<cluster_meta_ctrl>;

    static constexpr std::uint8_t kHashMask     = ctrl_type::kHashMask;
    static constexpr std::uint8_t kEmptySlot    = ctrl_type::kEmptySlot;
    static constexpr std::uint8_t kOverflowMask = ctrl_type::kOverflowMask;

    static constexpr size_type kGroupSize = group_type::kGroupSize;

    static constexpr bool kIsPlainKey    = detail::is_plain_type<key_type>::value;
    static constexpr bool kIsPlainMapped = detail::is_plain_type<mapped_type>::value;

    static constexpr bool kIsPlainKV = kIsPlainKey && kIsPlainMapped;

    static constexpr bool kHasSwapKey    = jstd::has_member_swap<key_type, key_type &>::value;
    static constexpr bool kHasSwapMapped = jstd::has_member_swap<mapped_type, mapped_type &>::value;

    static constexpr bool kIsSwappableKey    = detail::is_swappable<key_type>::value;
    static constexpr bool kIsSwappableMapped = detail::is_swappable<mapped_type>::value;

    static constexpr bool kIsSwappableKV = kIsSwappableKey && kIsSwappableMapped;

    static constexpr bool kIsNoexceptMoveAssignKey    = jstd::is_noexcept_move_assignable<key_type>::value;
    static constexpr bool kIsNoexceptMoveAssignMapped = jstd::is_noexcept_move_assignable<mapped_type>::value;

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

    static constexpr size_type kSizeTypeLength = sizeof(std::size_t);

    static constexpr bool kIsSmallKeyType   = (sizeof(key_type)    <= kSizeTypeLength * 2);
    static constexpr bool kIsSmallValueType = (sizeof(mapped_type) <= kSizeTypeLength * 2);

    static constexpr bool kDetectIsIndirectKey = !(detail::is_plain_type<key_type>::value ||
                                                  (sizeof(key_type) <= kSizeTypeLength * 2) ||
                                                 ((sizeof(key_type) <= (kSizeTypeLength * 4)) &&
                                                   is_slot_trivial_destructor));

    static constexpr bool kDetectIsIndirectValue = !(detail::is_plain_type<mapped_type>::value ||
                                                    (sizeof(mapped_type) <= kSizeTypeLength * 2) ||
                                                   ((sizeof(mapped_type) <= (kSizeTypeLength * 4)) &&
                                                     is_slot_trivial_destructor));

    static constexpr bool kIsIndirectKey = false;
    static constexpr bool kIsIndirectValue = false;
    static constexpr bool kIsIndirectKV = kIsIndirectKey | kIsIndirectValue;
    static constexpr bool kNeedStoreHash = true;

    using slot_type = map_slot_type<key_type, mapped_type>;
    using slot_policy_t = flat_map_slot_policy<key_type, mapped_type, slot_type>;
    using SlotPolicyTraits = slot_policy_traits<slot_policy_t>;

    //using slot_type = flat_map_slot_storage<type_policy, kIsIndirectKey, kIsIndirectValue>;

    static constexpr size_type kCacheLineSize = 64;
    static constexpr size_type kActualSlotAlignment = alignof(slot_type);
    static constexpr size_type kSlotAlignment = compile_time::is_pow2<alignof(slot_type)>::value ?
                                                jstd::cmax(alignof(slot_type), kCacheLineSize) :
                                                alignof(slot_type);

    using iterator       = flat_map_iterator<this_type, value_type, kIsIndirectKV>;
    using const_iterator = flat_map_iterator<this_type, const value_type, kIsIndirectKV>;

    static constexpr size_type kDefaultCapacity = 0;
    // kMinCapacity must be >= 2
    static constexpr size_type kMinCapacity = 4;

    // Default load factor = 224 / 256 = 0.875
    static constexpr size_type kDefaultMaxLoadFactor = 224;
    static constexpr size_type kLoadFactorAmplify = 256;

    static constexpr size_type kSkipGroupsLimit = 5;

    using group_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<group_type>;
    using ctrl_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<ctrl_type>;
    using slot_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<slot_type>;

    using AllocTraits = std::allocator_traits<allocator_type>;

    using GroupAllocTraits = typename std::allocator_traits<allocator_type>::template rebind_traits<group_type>;
    using CtrlAllocTraits = typename std::allocator_traits<allocator_type>::template rebind_traits<ctrl_type>;    
    using SlotAllocTraits = typename std::allocator_traits<allocator_type>::template rebind_traits<slot_type>;

    using hash_policy_t = typename hash_policy_selector<Hash>::type;

private:
    group_type *    groups_;
    ctrl_type *     ctrls_;
    size_type       slot_size_;
    size_type       slot_mask_;     // capacity = slot_mask + 1
    size_type       slot_threshold_;
    slot_type *     slots_;
    key_type *      key_arrays_;
    mapped_type *   value_arrays_;
    size_type       mlf_;

#if CLUSTER_USE_HASH_POLICY
    hash_policy_t           hash_policy_;
#endif

    hasher                  hasher_;
    key_equal               key_equal_;

    allocator_type          allocator_;
    group_allocator_type    group_allocator_;
    ctrl_allocator_type     ctrl_allocator_;
    slot_allocator_type     slot_allocator_;

    enum FindResult {
        kNeedGrow = -1,
        kIsNotExists = 0,
        kIsExists = 1
    };

    static constexpr size_type calcDefaultLoadFactor(size_type capacity) {
        return capacity * kDefaultMaxLoadFactor / kLoadFactorAmplify;
    }

public:
    cluster_flat_table() : cluster_flat_table(kDefaultCapacity) {}

    explicit cluster_flat_table(size_type capacity, hasher const & hash = hasher(),
                                key_equal const & pred = key_equal(),
                                allocator_type const & allocator = allocator_type())
        : groups_(nullptr), ctrls_(nullptr), slots_(nullptr), slot_size_(0), slot_mask_(static_cast<size_type>(capacity - 1)),
          slot_threshold_(calcDefaultLoadFactor(capacity)), mlf_(kDefaultMaxLoadFactor) {
    }

    cluster_flat_table(cluster_flat_table const & other) {
    }

    cluster_flat_table(cluster_flat_table const & other, allocator_type const & allocator) {
    }

    cluster_flat_table(cluster_flat_table && other) {
    }

    cluster_flat_table(cluster_flat_table && other, allocator_type const & allocator) {
    }

    ~cluster_flat_table() {
        this->destroy();
    }

    ///
    /// Observers
    ///
    hasher hash_function() const noexcept {
        return this->hasher_;
    }

    key_equal key_eq() const noexcept {
        return this->key_equal_;
    }

    allocator_type get_allocator() const noexcept {
        return this->allocator_;
    }

    group_allocator_type get_group_allocator_() noexcept {
        return this->group_allocator_;
    }

    ctrl_allocator_type get_ctrl_allocator() const noexcept {
        return this->ctrl_allocator_;
    }

    slot_allocator_type get_slot_allocator() const noexcept {
        return this->slot_allocator_;
    }

    hasher & hash_function_ref() noexcept {
        return this->hasher_;
    }

    key_equal & key_eq_ref() noexcept {
        return this->key_equal_;
    }

#if CLUSTER_USE_HASH_POLICY
    hash_policy_t & hash_policy_ref() noexcept {
        return this->hash_policy_;
    }
#endif

    allocator_type & get_allocator_ref() noexcept {
        return this->allocator_;
    }

    group_allocator_type & get_group_allocator_ref() noexcept {
        return this->group_allocator_;
    }

    ctrl_allocator_type & get_ctrl_allocator_ref() noexcept {
        return this->ctrl_allocator_;
    }

    slot_allocator_type & get_slot_allocator_ref() noexcept {
        return this->slot_allocator_;
    }

    static const char * name() noexcept {
        return "jstd::cluster_flat_map";
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
    size_type size() const noexcept { return this->slot_size(); }
    size_type capacity() const noexcept { return this->slot_capacity(); }
    size_type max_size() const noexcept {
        return std::numeric_limits<difference_type>::max() / sizeof(value_type);
    }

    size_type slot_size() const { return this->slot_size_; }
    size_type slot_mask() const { return this->slot_mask_; }
    size_type slot_capacity() const { return (this->slot_mask_ + 1); }
    size_type slot_threshold() const { return this->slot_threshold_; }

    bool is_valid() const { return (this->groups() != nullptr); }
    bool is_empty() const { return (this->size() == 0); }

    ///
    /// Bucket interface
    ///
    size_type bucket_size(size_type n) const noexcept { return 1; }
    size_type bucket_count() const noexcept { return this->slot_capacity(); }
    size_type max_bucket_count() const noexcept {
        return std::numeric_limits<difference_type>::max() / sizeof(ctrl_type);
    }

    size_type bucket(const key_type & key) const {
        size_type ctrl_index = this->find_index(key);
        return ctrl_index;
    }

    size_type group_capacity() const {
        return (this->slot_capacity() / kGroupSize);
    }

    ///
    /// Hash policy
    ///
    double load_factor() const {
        if (this->slot_capacity() != 0)
            return ((double)this->slot_size() / this->slot_capacity());
        else
            return 0.0;
    }

    double max_load_factor() const {
        return ((double)this->mlf_ / kLoadFactorAmplify);
    }

    ///
    /// Pointers
    ///
    group_type * groups() { return this->groups_; }
    const group_type * groups() const {
        return const_cast<const group_type *>(this->groups_);
    }

    group_type * last_group() {
        return (this->groups() + this->group_capacity());
    }
    const group_type * last_group() const {
        return (this->groups() + this->group_capacity());
    }

    ctrl_type * ctrls() {
        return reinterpret_cast<ctrl_type *>(this->groups());
    }
    const ctrl_type * ctrls() const {
        return reinterpret_cast<const ctrl_type *>(this->groups());
    }

    ctrl_type * last_ctrl() {
        return (this->ctrls() + this->slot_capacity());
    }
    const ctrl_type * last_ctrl() const {
        return (this->ctrls() + this->slot_capacity());
    }

    slot_type * slots() { return this->slots_; }
    const slot_type * slots() const {
        return const_cast<const slot_type *>(this->slots_);
    }

    slot_type * last_slot() {
        return (this->slots() + this->slot_capacity());
    }
    const slot_type * last_slot() const {
        return (this->slots() + this->slot_capacity());
    }

    ///
    /// Hash policy
    ///
    void reserve(size_type new_capacity, bool read_only = false) {
        this->rehash(new_capacity, read_only);
    }

    void resize(size_type new_capacity, bool read_only = false) {
        this->rehash(new_capacity, read_only);
    }

    void rehash(size_type new_capacity, bool read_only = false) {
        if (!read_only)
            new_capacity = (std::max)(this->min_require_capacity(new_capacity), this->slot_size());
        else
            new_capacity = (std::max)(new_capacity, this->slot_size());
        this->rehash_impl<true, false>(new_capacity);
    }

    void shrink_to_fit(bool read_only = false) {
        size_type new_capacity;
        if (!read_only)
            new_capacity = this->min_require_capacity(this->slot_size());
        else
            new_capacity = this->slot_size();
        this->rehash_impl<true, false>(new_capacity);
    }

    ///
    /// Lookup
    ///
    size_type count(const key_type & key) const {
        const slot_type * slot = this->find_impl(key);
        return (slot != nullptr) ? 1 : 0;
    }

    bool contains(const key_type & key) const {
        const slot_type * slot = this->find_impl(key);
        return (slot != nullptr);
    }

    ///
    /// find(key)
    ///
    iterator find(const key_type & key) {
        return const_cast<const this_type *>(this)->find(key);
    }

    const_iterator find(const key_type & key) const {
        const slot_type * slot = this->find_impl(key);
        if (!kIsIndirectKV)
            return this->iterator_at(this->index_of(slot));
        else
            return this->iterator_at(slot);
    }

    template <typename KeyT>
    iterator find(const KeyT & key) {
        return const_cast<const this_type *>(this)->find(key);
    }

    template <typename KeyT>
    const_iterator find(const KeyT & key) const {
        const slot_type * slot = this->find_impl(key);
        if (!kIsIndirectKV)
            return this->iterator_at(this->index_of(slot));
        else
            return this->iterator_at(slot);
    }

    ///
    /// Modifiers
    ///
    void clear(bool need_destroy = false) noexcept {
        if (this->slot_capacity() > kDefaultCapacity) {
            if (need_destroy) {
                this->destroy_data();
                this->create_slots<false>(kDefaultCapacity);
                assert(this->slot_size() == 0);
                return;
            }
        }
        this->clear_data();
        assert(this->slot_size() == 0);
    }

    ///
    /// insert(value)
    ///
    std::pair<iterator, bool> insert(const value_type & value) {
        return this->emplace_impl<false>(value);
    }

    std::pair<iterator, bool> insert(value_type && value) {
        return this->emplace_impl<false>(std::move(value));
    }

    template <typename P>
    std::pair<iterator, bool> insert(P && value) {
        return this->emplace_impl<false>(std::forward<P>(value));
    }

    iterator insert(const_iterator hint, const value_type & value) {
        return this->emplace_impl<false>(value).first;
    }

    iterator insert(const_iterator hint, value_type && value) {
        return this->emplace_impl<false>(std::move(value)).first;
    }

    template <typename P>
    iterator insert(const_iterator hint, P && value) {
        return this->emplace_impl<false>(std::forward<P>(value)).first;
    }

    template <typename InputIter>
    void insert(InputIter first, InputIter last) {
        for (; first != last; ++first) {
            this->emplace_impl<false>(*first);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        this->insert(ilist.begin(), ilist.end());
    }

    ///
    /// insert_or_assign(key, value)
    ///
    template <typename MappedT>
    std::pair<iterator, bool> insert_or_assign(const key_type & key, MappedT && value) {
        return this->emplace_impl<true>(key, std::forward<MappedT>(value));
    }

    template <typename MappedT>
    std::pair<iterator, bool> insert_or_assign(key_type && key, MappedT && value) {
        return this->emplace_impl<true>(std::move(key), std::forward<MappedT>(value));
    }

    template <typename KeyT, typename MappedT>
    std::pair<iterator, bool> insert_or_assign(KeyT && key, MappedT && value) {
        return this->emplace_impl<true>(std::move(key), std::forward<MappedT>(value));
    }

    template <typename MappedT>
    iterator insert_or_assign(const_iterator hint, const key_type & key, MappedT && value) {
        return this->emplace_impl<true>(key, std::forward<MappedT>(value))->first;
    }

    template <typename MappedT>
    iterator insert_or_assign(const_iterator hint, key_type && key, MappedT && value) {
        return this->emplace_impl<true>(std::move(key), std::forward<MappedT>(value))->first;
    }

    template <typename KeyT, typename MappedT>
    iterator insert_or_assign(const_iterator hint, KeyT && key, MappedT && value) {
        return this->emplace_impl<true>(std::move(key), std::forward<MappedT>(value))->first;
    }

    ///
    /// emplace(args...)
    ///
    template <typename ... Args>
    std::pair<iterator, bool> emplace(Args && ... args) {
        return this->emplace_impl<false>(std::forward<Args>(args)...);
    }

    template <typename ... Args>
    iterator emplace_hint(const_iterator hint, Args && ... args) {
        return this->emplace_impl<false>(std::forward<Args>(args)...).first;
    }

    ///
    /// try_emplace(key, args...)
    ///
    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(const key_type & key, Args && ... args) {
        return this->try_emplace_impl(key, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(key_type && key, Args && ... args) {
        return this->try_emplace_impl(std::move(key), std::forward<Args>(args)...);
    }

    template <typename KeyT, typename ... Args>
    std::pair<iterator, bool> try_emplace(KeyT && key, Args && ... args) {
        return this->try_emplace_impl(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(const_iterator hint, const key_type & key, Args && ... args) {
        return this->try_emplace_impl(key, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    std::pair<iterator, bool> try_emplace(const_iterator hint, key_type && key, Args && ... args) {
        return this->try_emplace_impl(std::move(key), std::forward<Args>(args)...);
    }

    template <typename KeyT, typename ... Args>
    std::pair<iterator, bool> try_emplace(const_iterator hint, KeyT && key, Args && ... args) {
        return this->try_emplace_impl(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    ///
    /// For iterator
    ///
    inline size_type next_index(size_type index) const noexcept {
        assert(index < this->slot_capacity());
        return ((index + 1) & this->slot_mask_);
    }

    inline size_type next_index(size_type index, size_type slot_mask) const noexcept {
        assert(index < this->slot_capacity());
        return ((index + 1) & slot_mask);
    }

    inline ctrl_type * ctrl_at(size_type slot_index) noexcept {
        assert(slot_index <= this->slot_capacity());
        return (this->ctrls() + std::ptrdiff_t(slot_index));
    }

    inline const ctrl_type * ctrl_at(size_type slot_index) const noexcept {
        assert(slot_index <= this->slot_capacity());
        return (this->ctrls() + std::ptrdiff_t(slot_index));
    }

    inline group_type group_at(size_type slot_index) noexcept {
        assert(slot_index < this->slot_capacity());
        size_type group_idx = slot_index / kGroupSize;
        return (this->groups() + std::ptrdiff_t(group_idx));
    }

    inline const group_type group_at(size_type slot_index) const noexcept {
        assert(slot_index < this->slot_capacity());
        size_type group_idx = slot_index / kGroupSize;
        return (this->groups() + std::ptrdiff_t(group_idx));
    }

    inline slot_type * slot_at(size_type slot_index) noexcept {
        assert(slot_index <= this->slot_capacity());
        return (this->slots() + std::ptrdiff_t(slot_index));
    }

    inline const slot_type * slot_at(size_type slot_index) const noexcept {
        assert(slot_index <= this->slot_capacity());
        return (this->slots() + std::ptrdiff_t(slot_index));
    }

    inline slot_type * slot_at(ctrl_type * ctrl) noexcept {
        size_type slot_index;
        if (kIsIndirectKV)
            slot_index = ctrl->get_index();
        else
            slot_index = this->index_of(ctrl);
        assert(slot_index <= this->slot_capacity());
        return (this->slots() + std::ptrdiff_t(slot_index));
    }

    inline const slot_type * slot_at(const ctrl_type * ctrl) const noexcept {
        size_type slot_index;
        if (kIsIndirectKV)
            slot_index = ctrl->get_index();
        else
            slot_index = this->index_of(ctrl);
        assert(slot_index <= this->slot_capacity());
        return (this->slots() + std::ptrdiff_t(slot_index));
    }

private:
    static ctrl_type * default_empty_ctrls() {
        alignas(16) static const ctrl_type s_empty_ctrls[16] = {
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot },
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot },
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot },
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot }
        };

        return const_cast<ctrl_type *>(&s_empty_ctrls[0]);
    }

    JSTD_FORCED_INLINE
    size_type calc_capacity(size_type init_capacity) const noexcept {
        size_type new_capacity = (std::max)(init_capacity, kMinCapacity);
        if (!pow2::is_pow2(new_capacity)) {
            new_capacity = pow2::round_up<size_type, kMinCapacity>(new_capacity);
        }
        return new_capacity;
    }

    size_type calc_slot_threshold(size_type now_slot_capacity) const {
        return (now_slot_capacity * this->mlf_ / kLoadFactorAmplify);
    }

    size_type calc_max_lookups(size_type new_capacity) const {
        assert(new_capacity > 1);
        assert(pow2::is_pow2(new_capacity));
#if 1
        // Fast to get log2_int, if the new_size is power of 2.
        // Use bsf(n) has the same effect.
        size_type max_lookups = size_type(BitUtils::bsr(new_capacity));
#else
        size_type max_lookups = size_type(pow2::log2_int<size_type, size_type(2)>(new_capacity));
#endif
        return max_lookups;
    }

    inline size_type min_require_capacity(size_type init_capacity) const {
        size_type new_capacity = init_capacity * kLoadFactorAmplify / this->mlf_;
        return new_capacity;
    }

    bool is_positive(size_type value) const {
        return (reinterpret_cast<intptr_t>(value) >= 0);
    }

    inline iterator iterator_at(size_type index) noexcept {
        if (!kIsIndirectKV)
            return { this, index };
        else
            return { this->slot_at(index) };
    }

    inline const_iterator iterator_at(size_type index) const noexcept {
        if (!kIsIndirectKV)
            return { this, index };
        else
            return { this->slot_at(index) };
    }

    inline iterator iterator_at(ctrl_type * ctrl) noexcept {
        if (!kIsIndirectKV)
            return { this, this->index_of(ctrl) };
        else
            return { this->slot_at(ctrl->get_index()) };
    }

    inline const_iterator iterator_at(const ctrl_type * ctrl) const noexcept {
        if (!kIsIndirectKV)
            return { this, this->index_of(ctrl) };
        else
            return { this->slot_at(ctrl->get_index()) };
    }

    inline iterator iterator_at(slot_type * slot) noexcept {
        if (!kIsIndirectKV)
            return { this, this->index_of(slot) };
        else
            return { slot };
    }

    inline const_iterator iterator_at(const slot_type * slot) const noexcept {
        if (!kIsIndirectKV)
            return { this, this->index_of(slot) };
        else
            return { slot };
    }

    inline iterator next_valid_iterator(ctrl_type * ctrl, iterator iter) {
        if (ctrl->is_used())
            return iter;
        else
            return ++iter;
    }

    inline const_iterator next_valid_iterator(ctrl_type * ctrl, const_iterator iter) {
        if (ctrl->is_used())
            return iter;
        else
            return ++iter;
    }

    inline iterator next_valid_iterator(iterator iter) {
        size_type index = this->index_of(iter);
        if (!kIsIndirectKV) {
            ctrl_type * ctrl = this->ctrl_at(index);
            return this->next_valid_iterator(ctrl, iter);
        } else {
            ++iter;
            return iter;
        }
    }

    inline const_iterator next_valid_iterator(const_iterator iter) {
        size_type index = this->index_of(iter);
        if (!kIsIndirectKV) {
            ctrl_type * ctrl = this->ctrl_at(index);
            return this->next_valid_iterator(ctrl, iter);
        } else {
            ++iter;
            return iter;
        }
    }

    inline size_type index_salt() const noexcept {
        return (size_type)((std::uintptr_t)this->ctrls() >> 12);
    }

    inline std::size_t get_hash(const key_type & key) const
        noexcept(noexcept(this->hasher_(key))) {
#if 1
        std::size_t hash_code = static_cast<std::size_t>(this->hasher_(key));
#else
        std::size_t hash_code = static_cast<std::size_t>(
            this->hash_policy_.get_hash_code(key)
        );
#endif
        return hash_code;
    }

    //
    // Do the second hash on the basis of hash code for the index_for_hash().
    //
    inline std::size_t second_hasher(std::size_t value) const noexcept {
        return value;
    }

    //
    // Do the third hash on the basis of hash code for the ctrl hash.
    //
    inline std::size_t ctrl_hasher(std::size_t hash_code) const noexcept {
#if CLUSTER_USE_HASH_POLICY
        return hash_code;
#elif 0
        return (size_type)hashes::mum_hash64((std::uint64_t)hash_code, 11400714819323198485ull);
#elif 1
        return (size_type)hashes::fibonacci_hash64((size_type)hash_code);
#endif
    }

    // Maybe call the second hash
    inline size_type index_for_hash(std::size_t hash_code) const noexcept {
        std::size_t hash_value = hash_code;
#if CLUSTER_USE_HASH_POLICY
        if (kUseIndexSalt) {
            hash_value ^= this->index_salt();
        }
        size_type index = this->hash_policy_.template index_for_hash<key_type>(hash_value, this->slot_mask());
        return index;
#else
        hash_value = this->second_hasher(hash_value);
        if (kUseIndexSalt) {
            hash_value ^= this->index_salt();
        }
        return (hash_value & this->slot_mask());
#endif
    }

    // Maybe call the third hash to calculate the ctrl hash.
    inline std::uint8_t get_ctrl_hash(std::size_t hash_code) const noexcept {
        std::uint8_t ctrl_hash;
        if (kNeedStoreHash)
            ctrl_hash = ctrl_type::hash_bits(this->ctrl_hasher(hash_code));
        else
            ctrl_hash = std::uint8_t(0);
        return ctrl_hash;
    }

    size_type index_of(iterator pos) const {
        if (!kIsIndirectKV) {
            return pos.index();
        } else {
            const slot_type * slot = pos.slot();
            size_type ctrl_index = this->bucket(slot->key);
            return ctrl_index;
        }
    }

    size_type index_of(const_iterator pos) const {
        if (!kIsIndirectKV) {
            return pos.index();
        } else {
            const slot_type * slot = pos.slot();
            size_type ctrl_index = this->bucket(slot->key);
            return ctrl_index;
        }
    }

    size_type index_of(ctrl_type * ctrl) const {
        assert(ctrl != nullptr);
        assert(ctrl >= this->ctrls());
        size_type index;
        if (!kIsIndirectKV)
            index = (size_type)(ctrl - this->ctrls());
        else
            index = ctrl->get_index();
        assert(is_positive(index));
        return index;
    }

    size_type index_of(const ctrl_type * ctrl) const {
        return this->index_of(reinterpret_cast<ctrl_type *>(ctrl));
    }

    size_type index_of(slot_type * slot) const {
        assert(slot != nullptr);
        assert(slot >= this->slots());
        size_type index = (size_type)(slot - this->slots());
        assert(is_positive(index));
        return index;
    }

    size_type index_of(const slot_type * slot) const {
        return this->index_of(reinterpret_cast<slot_type *>(slot));
    }

    size_type index_of_ctrl(ctrl_type * ctrl) const {
        assert(ctrl != nullptr);
        assert(ctrl >= this->ctrls());
        size_type ctrl_index = (size_type)(ctrl - this->ctrls());
        assert(is_positive(ctrl_index));
        return ctrl_index;
    }

    size_type index_of_ctrl(const ctrl_type * ctrl) const {
        return this->index_of_ctrl(reinterpret_cast<ctrl_type *>(ctrl));
    }

    template <typename U>
    char * PtrOffset(U * ptr, std::ptrdiff_t offset) {
        return (reinterpret_cast<char *>(ptr) + offset);
    }

    template <typename U>
    const char * PtrOffset(U * ptr, std::ptrdiff_t offset) const {
        return const_cast<const char *>(reinterpret_cast<char *>(ptr) + offset);
    }

    static void placement_new_slot(slot_type * slot) {
        // The construction of union doesn't do anything at runtime but it allows us
        // to access its members without violating aliasing rules.
        new (slot) slot_type;
    }

    void destroy() {
        this->destroy_data();
    }

    void destroy_data() {
        // Note!!: destroy_slots() need use this->ctrls()
        this->destroy_slots();
        this->destroy_ctrls();
    }

    void destroy_slots() {
        this->clear_slots();

        if (this->slots_ != nullptr) {
#if CLUSTER_USE_SEPARATE_SLOTS
            SlotAllocTraits::deallocate(this->slot_allocator_, this->slots_, this->slot_capacity());
#endif
        }
        this->slots_ = nullptr;
        this->slot_size_ = 0;
    }

    void destroy_ctrls() noexcept {
        if (this->ctrls_ != this_type::default_empty_ctrls()) {
            size_type max_ctrl_capacity = this->group_capacity() * kGroupSize;
#if CLUSTER_USE_SEPARATE_SLOTS
            CtrlAllocTraits::deallocate(this->ctrl_allocator_, this->ctrls_, max_ctrl_capacity);
#else
            size_type total_alloc_size = this->TotalAllocSize<kSlotAlignment>(
                                               max_ctrl_capacity, this->slot_capacity());
            CtrlAllocTraits::deallocate(this->ctrl_allocator_, this->ctrls_, total_alloc_size);
#endif
        }
        this->ctrls_ = this_type::default_empty_ctrls();
    }

    void clear_data() {
        // Note!!: clear_slots() need use this->ctrls()
        this->clear_slots();
        this->clear_ctrls(this->ctrls(), this->slot_capacity());
    }

    JSTD_FORCED_INLINE
    void clear_slots() {
        if (!is_slot_trivial_destructor) {
            if (!kIsIndirectKV) {
                ctrl_type * ctrl = this->ctrls();
                assert(ctrl != nullptr);
                for (size_type index = 0; index < this->slot_capacity(); index++) {
                    if (ctrl->is_used()) {
                        if (!kIsIndirectKV) {
                            this->destroy_slot(index);
                        } else {
                            size_type slot_index = ctrl->get_index();
                            this->destroy_slot(slot_index);
                        }
                    }
                    ctrl++;
                }
            } else {
                for (size_type slot_index = 0; slot_index < this->size(); slot_index++) {
                    this->destroy_slot(slot_index);
                }
            }
        }

        this->slot_size_ = 0;
    }

    JSTD_FORCED_INLINE
    void clear_ctrls(ctrl_type * ctrls, size_type slot_capacity) {
        ctrl_type * ctrl = ctrls;
        ctrl_type * last_ctrl = ctrls + slot_capacity;
        group_type * group = reinterpret_cast<group_type *>(ctrls);
        group_type * last_group = reinterpret_cast<group_type *>(last_ctrl);
        for (; group < last_group; ++group) {
            group->init();
        }
    }

    inline bool need_grow() const {
        return (this->slot_size() >= this->slot_threshold());
    }

    void grow_if_necessary() {
        size_type new_capacity = (this->slot_mask_ + 1) * 2;
        this->rehash_impl<false, true>(new_capacity);
    }

    template <bool Initialize = false>
    void reset() noexcept {
        if (Initialize) {
            this->ctrls_ = this_type::default_empty_ctrls();
            this->slots_ = nullptr;
            this->slot_size_ = 0;
        }
        this->slot_mask_ = 0;
        this->slot_threshold_ = 0;
#if CLUSTER_USE_HASH_POLICY
        this->hash_policy_.reset();
#endif
    }

    bool isValidCapacity(size_type capacity) const {
        return ((capacity >= kMinCapacity) && pow2::is_pow2(capacity));
    }

    //
    // Given the pointer of ctrls and the capacity of ctrl, computes the padding of
    // between ctrls and slots (from the start of the backing allocation)
    // and return the beginning of slots.
    //
    template <size_type SlotAlignment>
    inline slot_type * AlignedSlots(const ctrl_type * ctrls, size_type ctrl_capacity) {
        static_assert((SlotAlignment > 0),
                      "jstd::cluster_flat_map::AlignedSlots<N>(): SlotAlignment must bigger than 0.");
        static_assert(((SlotAlignment & (SlotAlignment - 1)) == 0),
                      "jstd::cluster_flat_map::AlignedSlots<N>(): SlotAlignment must be power of 2.");
        const ctrl_type * last_ctrls = ctrls + ctrl_capacity;
        size_type last_ctrl = reinterpret_cast<size_type>(last_ctrls);
        size_type slots_first = (last_ctrl + SlotAlignment - 1) & (~(SlotAlignment - 1));
        size_type slots_padding = static_cast<size_type>(slots_first - last_ctrl);
        slot_type * slots = reinterpret_cast<slot_type *>((char *)last_ctrl + slots_padding);
        return slots;
    }

    //
    // Given the pointer of ctrls, the capacity of a ctrl and slot,
    // computes the total allocate size of the backing array.
    //
    template <size_type SlotAlignment>
    inline size_type TotalAllocSize(size_type ctrl_capacity, size_type slot_capacity) {
        const size_type num_ctrl_bytes = ctrl_capacity * sizeof(ctrl_type);
        const size_type num_slot_bytes = slot_capacity * sizeof(slot_type);
        const size_type total_bytes = num_ctrl_bytes + SlotAlignment + num_slot_bytes;
        return (total_bytes + sizeof(ctrl_type) - 1) / sizeof(ctrl_type);
    }

    template <bool Initialize = false>
    void create_slots(size_type init_capacity) {
        if (init_capacity == 0) {
            if (!Initialize) {
                this->destroy_data();
            }
            this->reset<Initialize>();
            return;
        }

        size_type new_capacity;
        if (Initialize) {
            new_capacity = this->calc_capacity(init_capacity);
            assert(new_capacity > 0);
            assert(new_capacity >= kMinCapacity);
        } else {
            new_capacity = init_capacity;
        }

#if CLUSTER_USE_HASH_POLICY
        auto hash_policy_setting = this->hash_policy_.calc_next_capacity(new_capacity);
        this->hash_policy_.commit(hash_policy_setting);
#endif
        size_type new_ctrl_capacity = new_capacity;
        size_type new_group_count = (new_ctrl_capacity + (kGroupSize - 1)) / kGroupSize;
        assert(new_group_count > 0);
        size_type ctrl_alloc_size = new_group_count * kGroupSize;

#if CLUSTER_USE_SEPARATE_SLOTS
        ctrl_type * new_ctrls = CtrlAllocTraits::allocate(this->ctrl_allocator_, ctrl_alloc_size);
#else
        size_type new_slot_capacity = new_capacity * this->mlf_ / kLoadFactorAmplify;
        size_type total_alloc_size = this->TotalAllocSize<kSlotAlignment>(ctrl_alloc_size,
                                           kIsIndirectKV ? new_slot_capacity : new_ctrl_capacity);

        ctrl_type * new_ctrls = CtrlAllocTraits::allocate(this->ctrl_allocator_, total_alloc_size);
#endif
        // Prefetch for resolve potential ctrls TLB misses.
        //Prefetch_Write_T2(new_ctrls);

        // Reset ctrls to default state
        this->clear_ctrls(new_ctrls, new_capacity);

#if CLUSTER_USE_SEPARATE_SLOTS
        slot_type * new_slots = SlotAllocTraits::allocate(this->slot_allocator_, new_ctrl_capacity);
#else
        slot_type * new_slots = this->AlignedSlots<kSlotAlignment>(new_ctrls, ctrl_alloc_size);
#endif
        // Prefetch for resolve potential ctrls TLB misses.
        Prefetch_Write_T2(new_slots);

        this->ctrls_ = new_ctrls;

        this->slots_ = new_slots;
        if (Initialize) {
            assert(this->slot_size_ == 0);
        } else {
            this->slot_size_ = 0;
        }
        this->slot_mask_ = new_capacity - 1;
        this->slot_threshold_ = this->calc_slot_threshold(new_capacity);
    }

    template <bool AllowShrink, bool AlwaysResize>
    JSTD_NO_INLINE
    void rehash_impl(size_type new_capacity) {
        new_capacity = this->calc_capacity(new_capacity);
        assert(new_capacity > 0);
        assert(new_capacity >= kMinCapacity);
        if (AlwaysResize ||
            (!AllowShrink && (new_capacity > this->slot_capacity())) ||
            (AllowShrink && (new_capacity != this->slot_capacity()))) {
            if (!AlwaysResize && !AllowShrink) {
                assert(new_capacity >= this->slot_size());
            }

            ctrl_type * old_ctrls = this->ctrls();
            group_type * old_groups = this->groups();
            size_type old_group_capacity = this->group_capacity();

            slot_type * old_slots = this->slots();
            size_type old_slot_size = this->slot_size();
            size_type old_slot_mask = this->slot_mask();
            size_type old_slot_capacity = this->slot_capacity();

            this->create_slots<false>(new_capacity);

            if (!kIsIndirectKV) {
                // kIsIndirectKV = false
                if (old_slot_capacity >= kGroupSize) {
                    group_type * group = old_groups;
                    group_type * last_group = old_groups + old_group_capacity;
                    slot_type * slot_base = old_slots;

                    for (; group < last_group; ++group) {
                        uint32_t used_mask = group->match_used();
                        while (used_mask != 0) {
                            std::uint32_t used_pos = BitUtils::bsf32(used_mask);
                            used_mask = BitUtils::clearLowBit32(used_mask);
                            slot_type * old_slot = slot_base + used_pos;
                            this->insert_no_grow(old_slot);
                            this->destroy_slot(old_slot);
                        }
                        slot_base += kGroupSize;
                    }
                } else if (old_ctrls != default_empty_ctrls()) {
                    ctrl_type * last_ctrl = old_ctrls + old_slot_capacity;
                    slot_type * old_slot = old_slots;
                    for (ctrl_type * ctrl = old_ctrls; ctrl != last_ctrl; ctrl++) {
                        if (likely(ctrl->is_used())) {
                            if (!kNeedStoreHash)
                                this->insert_no_grow(old_slot);
                            else
                                this->insert_no_grow(old_slot, ctrl->get_hash());
                            this->destroy_slot(old_slot);
                        }
                        old_slot++;
                    }
                }
            } else {
                // kIsIndirectKV = true
                if (old_ctrls != default_empty_ctrls()) {
                    slot_type * last_slot = old_slots + old_slot_size;
                    for (slot_type * old_slot = old_slots; old_slot != last_slot; old_slot++) {
                        this->indirect_insert_no_grow(old_slot);
                        this->destroy_slot(old_slot);
                    }
                }
            }

            assert(this->slot_size() == old_slot_size);

            if (old_ctrls != this->default_empty_ctrls()) {
                size_type old_ctrl_capacity = old_group_capacity * kGroupSize;
#if CLUSTER_USE_SEPARATE_SLOTS
                CtrlAllocTraits::deallocate(this->ctrl_allocator_, old_ctrls, old_ctrl_capacity);
#else
                size_type total_alloc_size = this->TotalAllocSize<kSlotAlignment>(
                                                   old_ctrl_capacity, old_slot_capacity);
                CtrlAllocTraits::deallocate(this->ctrl_allocator_, old_ctrls, total_alloc_size);
#endif
            }

#if CLUSTER_USE_SEPARATE_SLOTS
            if (old_slots != nullptr) {
                SlotAllocTraits::deallocate(this->slot_allocator_, old_slots, old_slot_capacity);
            }
#endif
        }
    }

    JSTD_FORCED_INLINE
    void construct_slot(size_type index) {
        slot_type * slot = this->slot_at(index);
        this->construct_slot(slot);
    }

    JSTD_FORCED_INLINE
    void construct_slot(slot_type * slot) {
        SlotPolicyTraits::construct(&this->allocator_, slot);
    }

    JSTD_FORCED_INLINE
    void destroy_slot(size_type index) {
        slot_type * slot = this->slot_at(index);
        this->destroy_slot(slot);
    }

    JSTD_FORCED_INLINE
    void destroy_slot(slot_type * slot) {
        if (!is_slot_trivial_destructor) {
            SlotPolicyTraits::destroy(&this->allocator_, slot);
        }
    }

    JSTD_FORCED_INLINE
    void destroy_slot_data(ctrl_type * ctrl, slot_type * slot) {
        this->set_empty(ctrl);
        this->destroy_slot(slot);
    }

    JSTD_FORCED_INLINE
    void construct_empty_slot(slot_type * empty) {
        static constexpr bool isNoexceptMoveAssignable = is_noexcept_move_assignable<value_type>::value;
        static constexpr bool isMutableNoexceptMoveAssignable = is_noexcept_move_assignable<init_type>::value;

        if ((!is_slot_trivial_destructor) && (!kIsPlainKV) &&
            (!kIsSwappableKV) && (!kIsSmallValueType) && kEnableExchange) {
            if (kIsLayoutCompatible) {
                if (isMutableNoexceptMoveAssignable) {
                    this->construct_slot(empty);
                    return;
                }
            } else {
                if (isNoexceptMoveAssignable) {
                    this->construct_slot(empty);
                    return;
                }
            }
        }

        // If we don't called construct_slot(empty), then use placement new.
        this->placement_new_slot(empty);
    }

    JSTD_FORCED_INLINE
    void destroy_empty_slot(slot_type * empty) {
        static constexpr bool isNoexceptMoveAssignable = is_noexcept_move_assignable<value_type>::value;
        static constexpr bool isMutableNoexceptMoveAssignable = is_noexcept_move_assignable<init_type>::value;

        if ((!is_slot_trivial_destructor) && (!kIsPlainKV) &&
            (!kIsSwappableKV) && (!kIsSmallValueType) && kEnableExchange) {
            if (kIsLayoutCompatible) {
                if (isMutableNoexceptMoveAssignable) {
                    this->destroy_slot(empty);
                }
            } else {
                if (isNoexceptMoveAssignable) {
                    this->destroy_slot(empty);
                }
            }
        }
    }

    template <typename T, bool hasSwapMethod, bool isNoexceptMoveAssign>
    struct swap_traits;

    template <typename T>
    struct swap_traits<T, true, true> {
        template <typename Alloc>
        static void swap(Alloc * alloc, T * obj1, T * obj2)
            noexcept(noexcept(std::declval<typename std::remove_const<T>::type>()
                              .swap(*const_cast<typename std::remove_const<T>::type *>(obj2))))
        {
            // hasSwapMethod = true
            typedef typename std::remove_const<T>::type non_const_type;
            const_cast<non_const_type *>(obj1)->swap(*const_cast<non_const_type *>(obj2));
        }
    };

    template <typename T>
    struct swap_traits<T, true, false> {
        template <typename Alloc>
        static void swap(Alloc * alloc, T * obj1, T * obj2)
            noexcept(noexcept(std::declval<typename std::remove_const<T>::type>()
                              .swap(*const_cast<typename std::remove_const<T>::type *>(obj2))))
        {
            // hasSwapMethod = true
            typedef typename std::remove_const<T>::type non_const_type;
            const_cast<non_const_type *>(obj1)->swap(*const_cast<non_const_type *>(obj2));
        }
    };

    template <typename T>
    struct swap_traits<T, false, true> {
        template <typename Alloc>
        static void swap(Alloc * alloc, T * obj1, T * obj2)
            noexcept(std::is_nothrow_move_constructible<typename std::remove_const<T>::type>::value &&
                     std::is_nothrow_move_assignable<typename std::remove_const<T>::type>::value)
        {
            // hasSwapMethod = false, isNoexceptMoveAssign = true
            typedef typename std::remove_const<T>::type non_const_type;

            using std::swap;
            swap(*const_cast<non_const_type *>(obj1), *const_cast<non_const_type *>(obj2));
        }
    };

    template <typename T>
    struct swap_traits<T, false, false> {
        template <typename Alloc>
        static void swap(Alloc * alloc, T * obj1, T * obj2)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            // hasSwapMethod = false, isNoexceptMoveAssign = false
            static constexpr size_type kMinAlignment = 16;
            static constexpr size_type kAlignment = cmax(std::alignment_of<T>::value, kMinAlignment);

            alignas(kAlignment) char raw[sizeof(T)];
            T * tmp = reinterpret_cast<T *>(&raw);
            AllocTraits::construct(*alloc, tmp, std::move(*obj2));
            AllocTraits::destroy(*alloc, obj2);
            AllocTraits::construct(*alloc, obj2, std::move(*obj1));
            AllocTraits::destroy(*alloc, obj1);
            AllocTraits::construct(*alloc, obj1, std::move(*tmp));
            AllocTraits::destroy(*alloc, tmp);
        }
    };

    template <typename T, bool isLayoutCompatible, bool isNoexceptMoveAssign>
    struct slot_adapter;

    template <typename T /*, bool isLayoutCompatible, bool isNoexceptMoveAssign */>
    struct slot_adapter<T, true, true> {
        typedef typename T::first_type                          first_type;
        typedef typename T::second_type                         second_type;
        typedef typename std::remove_const<first_type>::type    mutable_first_type;

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2)
            noexcept(std::is_nothrow_move_constructible<T>::value &&
                     std::is_nothrow_move_assignable<T>::value)
        {
            static constexpr bool isNoexceptMoveAssignKey    = is_noexcept_move_assignable<mutable_first_type>::value;
            static constexpr bool isNoexceptMoveAssignMapped = is_noexcept_move_assignable<second_type>::value;
#if CLUSTER_USE_SWAP_TRAITS
            swap_traits<mutable_first_type, kHasSwapKey, isNoexceptMoveAssignKey>::
                swap(alloc, &slot1->mutable_value.first, &slot2->mutable_value.first);

            swap_traits<second_type, kHasSwapMapped, isNoexceptMoveAssignMapped>::
                swap(alloc, &slot1->mutable_value.second, &slot2->mutable_value.second);
#else
            using std::swap;
            swap(slot1->mutable_value, slot2->mutable_value);
#endif
        }

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void swap_plain(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_assignable<T>::value)
        {
#if 1
            swap(alloc, slot1, slot2, tmp);
#else
            SlotPolicyTraits::move_assign_swap(alloc, slot1, slot2, tmp);
#endif
        }

        template <typename Alloc, typename SlotType>
        static void exchange(Alloc * alloc, SlotType * src, SlotType * dest, SlotType * empty)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::exchange(alloc, src, dest, empty);
        }
    };

    template <typename T /* , bool isLayoutCompatible, bool isNoexceptMoveAssign */>
    struct slot_adapter<T, true, false> {
        typedef typename T::first_type                          first_type;
        typedef typename T::second_type                         second_type;
        typedef typename std::remove_const<first_type>::type    mutable_first_type;

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2)
            noexcept(std::is_nothrow_move_constructible<T>::value &&
                     std::is_nothrow_move_assignable<T>::value)
        {
            static constexpr bool isNoexceptMoveAssignKey    = is_noexcept_move_assignable<mutable_first_type>::value;
            static constexpr bool isNoexceptMoveAssignMapped = is_noexcept_move_assignable<second_type>::value;
#if CLUSTER_USE_SWAP_TRAITS
            swap_traits<mutable_first_type, kHasSwapKey, isNoexceptMoveAssignKey>::
                swap(alloc, &slot1->mutable_value.first, &slot2->mutable_value.first);

            swap_traits<second_type, kHasSwapMapped, isNoexceptMoveAssignMapped>::
                swap(alloc, &slot1->mutable_value.second, &slot2->mutable_value.second);
#else
            using std::swap;
            swap(slot1->mutable_value, slot2->mutable_value);
#endif
        }

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void swap_plain(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void exchange(Alloc * alloc, SlotType * src, SlotType * dest, SlotType * empty)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::exchange(alloc, src, dest, empty);
        }
    };

    template <typename T /*, bool isLayoutCompatible, bool isNoexceptMoveAssign */>
    struct slot_adapter<T, false, true> {
        typedef typename T::first_type                          first_type;
        typedef typename T::second_type                         second_type;
        typedef typename std::remove_const<first_type>::type    mutable_first_type;

        static mutable_first_type * mutable_key(T * value) {
            // Still check for isLayoutCompatible so that we can avoid calling jstd::launder
            // unless necessary because it can interfere with optimizations.
            return jstd::launder(const_cast<mutable_first_type *>(std::addressof(value->first)));
        }

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2)
            noexcept(std::is_nothrow_move_constructible<T>::value &&
                     std::is_nothrow_move_assignable<T>::value)
        {
            static constexpr bool isNoexceptMoveAssignKey    = is_noexcept_move_assignable<first_type>::value;
            static constexpr bool isNoexceptMoveAssignMapped = is_noexcept_move_assignable<second_type>::value;
#if CLUSTER_USE_SWAP_TRAITS
            swap_traits<first_type, kHasSwapKey, isNoexceptMoveAssignKey>::
                swap(alloc, &slot1->value.first, &slot2->value.first);

            swap_traits<second_type, kHasSwapMapped, isNoexceptMoveAssignMapped>::
                swap(alloc, &slot1->value.second, &slot2->value.second);
#else
            using std::swap;
            swap(slot1->value.first,  slot2->value.first);
            swap(slot1->value.second, slot2->value.second);
#endif
        }

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void swap_plain(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_assignable<T>::value)
        {
            swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void exchange(Alloc * alloc, SlotType * src, SlotType * dest, SlotType * empty)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::exchange(alloc, src, dest, empty);
        }
    };

    template <typename T /*, bool isLayoutCompatible, bool isNoexceptMoveAssign */>
    struct slot_adapter<T, false, false> {
        typedef typename T::first_type                          first_type;
        typedef typename T::second_type                         second_type;
        typedef typename std::remove_const<first_type>::type    mutable_first_type;

        static mutable_first_type * mutable_key(T * value) {
            // Still check for isLayoutCompatible so that we can avoid calling jstd::launder
            // unless necessary because it can interfere with optimizations.
            return jstd::launder(const_cast<mutable_first_type *>(std::addressof(value->first)));
        }

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2)
            noexcept(std::is_nothrow_move_constructible<T>::value &&
                     std::is_nothrow_move_assignable<T>::value)
        {
            static constexpr bool isNoexceptMoveAssignKey    = is_noexcept_move_assignable<first_type>::value;
            static constexpr bool isNoexceptMoveAssignMapped = is_noexcept_move_assignable<second_type>::value;
#if CLUSTER_USE_SWAP_TRAITS
            swap_traits<first_type, kHasSwapKey, isNoexceptMoveAssignKey>::
                swap(alloc, &slot1->value.first, &slot2->value.first);

            swap_traits<second_type, kHasSwapMapped, isNoexceptMoveAssignMapped>::
                swap(alloc, &slot1->value.second, &slot2->value.second);
#else
            using std::swap;
            swap(*mutable_key(&slot1->value), *mutable_key(&slot2->value));
            swap(slot1->value.second, slot2->value.second);
#endif
        }

        template <typename Alloc, typename SlotType>
        static void swap(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            SlotPolicyTraits::swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void swap_plain(Alloc * alloc, SlotType * slot1, SlotType * slot2, SlotType * tmp)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
            swap(alloc, slot1, slot2, tmp);
        }

        template <typename Alloc, typename SlotType>
        static void exchange(Alloc * alloc, SlotType * src, SlotType * dest, SlotType * empty)
            noexcept(std::is_nothrow_move_constructible<T>::value)
        {
             SlotPolicyTraits::exchange(alloc, src, dest, empty);
        }
    };

    JSTD_FORCED_INLINE
    void transfer_slot(slot_type * new_slot, slot_type * old_slot) {
        SlotPolicyTraits::transfer(&this->allocator_, new_slot, old_slot);
    }

    JSTD_FORCED_INLINE
    void transfer_slot(slot_type * new_slot, const slot_type * old_slot) {
        SlotPolicyTraits::transfer(this->allocator_, new_slot, old_slot);
    }

    JSTD_FORCED_INLINE
    void exchange_slot(slot_type * src, slot_type * dest, slot_type * empty) {
        if (kIsLayoutCompatible) {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<init_type>::value;
            slot_adapter<init_type, true, isNoexceptMoveAssign>
                ::exchange(&this->allocator_, src, dest, empty);
        } else {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<value_type>::value;
            slot_adapter<value_type, false, isNoexceptMoveAssign>
                ::exchange(&this->allocator_, src, dest, empty);
        }
    }

    JSTD_FORCED_INLINE
    void swap_slot(slot_type * slot1, slot_type * slot2) {
        if (kIsLayoutCompatible) {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<init_type>::value;
            slot_adapter<init_type, true, isNoexceptMoveAssign>
                ::swap(&this->allocator_, slot1, slot2);
        } else {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<value_type>::value;
            slot_adapter<value_type, false, isNoexceptMoveAssign>
                ::swap(&this->allocator_, slot1, slot2);
        }
    }

    JSTD_FORCED_INLINE
    void swap_slot(slot_type * slot1, slot_type * slot2, slot_type * tmp) {
        if (kIsLayoutCompatible) {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<init_type>::value;
            slot_adapter<init_type, true, isNoexceptMoveAssign>
                ::swap(&this->allocator_, slot1, slot2, tmp);
        } else {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<value_type>::value;
            slot_adapter<value_type, false, isNoexceptMoveAssign>
                ::swap(&this->allocator_, slot1, slot2, tmp);
        }
    }

    JSTD_FORCED_INLINE
    void swap_plain_slot(slot_type * slot1, slot_type * slot2, slot_type * tmp) {
        if (kIsLayoutCompatible) {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<init_type>::value;
            slot_adapter<init_type, true, isNoexceptMoveAssign>
                ::swap_plain(&this->allocator_, slot1, slot2, tmp);
        } else {
            static constexpr bool isNoexceptMoveAssign = is_noexcept_move_assignable<value_type>::value;
            slot_adapter<value_type, false, isNoexceptMoveAssign>
                ::swap_plain(&this->allocator_, slot1, slot2, tmp);
        }
    }

    inline void set_used_ctrl(size_type index, std::uint8_t hash) {
        ctrl_type * ctrl = this->ctrl_at(index);
        this->set_used_ctrl(ctrl, hash);
    }

    inline void set_used_ctrl(ctrl_type * ctrl, std::uint8_t hash) {
        ctrl->set_hash(hash);
    }

    inline void set_used_ctrl(size_type index, const ctrl_type & new_ctrl) {
        ctrl_type * ctrl = this->ctrl_at(index);
        this->set_used_ctrl(ctrl, new_ctrl);
    }

    inline void set_used_ctrl(ctrl_type * ctrl, const ctrl_type & new_ctrl) {
        ctrl->set_value(new_ctrl.get_value());
    }

    inline void set_empty_ctrl(size_type index) {
        ctrl_type * ctrl = this->ctrl_at(index);
        this->set_empty_ctrl(ctrl);
    }

    inline void set_empty_ctrl(ctrl_type * ctrl) {
        assert(ctrl->is_used());
        ctrl->set_empty();
    }

    template <typename KeyT>
    slot_type * find_impl(const KeyT & key) {
        return const_cast<slot_type *>(
            const_cast<const this_type *>(this)->find_impl(key)
        );
    }

    template <typename KeyT>
    const slot_type * find_impl(const KeyT & key) const {
        if (!kIsIndirectKV) {
            return this->direct_find(key);
        } else {
            return this->indirect_find(key);
        }
    }

    template <typename KeyT>
    const slot_type * direct_find(const KeyT & key) const {
        std::size_t hash_code = this->get_hash(key);
        size_type slot_index = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->get_ctrl_hash(hash_code);
        size_type group_index = slot_index / kGroupSize;
        size_type group_pos = slot_index % kGroupSize;
        const group_type * group = this->group_at(group_index);
        const group_type * last_group = this->last_group();
        
        size_type slot_base = group_index * kGroupSize;
        size_type skip_groups = 0;

        for (;;) {
            std::uint32_t match_mask = group->match_hash(ctrl_hash);
            if (match_mask != 0) {
                do {
                    std::uint32_t match_pos = BitUtils::bsf32(match_mask);
                    std::uint32_t match_bit = BitUtils::ls1b32(match_mask);
                    match_mask ^= match_bit;

                    size_type slot_pos = slot_base + match_pos;
                    const slot_type * slot = this->slot_at(slot_pos);
                    if (this->key_equal_(slot->value.first, key)) {
                        return slot;
                    }
                } while (match_mask != 0);
            } else {
                // If it doesn't overflow, means it hasn't been found.
                if (!group->is_overflow(group_pos)) {
                    return this->last_slot();
                }
            }
            slot_base += kGroupSize;
            group++;
            if (group >= last_group) {
                group = this->groups();
                slot_base = 0;
            }
            skip_groups++;
            if (skip_groups > kSkipGroupsLimit) {
                std::cout << "direct_find(): key = " << key <<
                             ", skip_groups = " << skip_groups << std::endl;
            }
            if (skip_groups >= this->group_capacity()) {
                return this->last_slot();
            }
        }
    }

    template <typename KeyT>
    const slot_type * indirect_find(const KeyT & key) const {
        std::size_t hash_code = this->get_hash(key);
        size_type slot_index = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->get_ctrl_hash(hash_code);
        size_type group_index = slot_index / kGroupSize;
        size_type group_pos = slot_index % kGroupSize;
        const group_type * group = this->group_at(group_index);

        size_type slot_base = group_index * kGroupSize;
        size_type skip_groups = 0;

        for (;;) {
            std::uint32_t match_mask = group->match_hash(ctrl_hash);
            if (match_mask != 0) {
                do {
                    std::uint32_t match_pos = BitUtils::bsf32(match_mask);
                    std::uint32_t match_bit = BitUtils::ls1b32(match_mask);
                    match_mask ^= match_bit;

                    size_type slot_pos = slot_base + match_pos;
                    const slot_type * slot = this->slot_at(slot_pos);
                    if (this->key_equal_(slot->value.first, key)) {
                        return slot;
                    }
                } while (match_mask != 0);
            } else {
                // If it doesn't overflow, means it hasn't been found.
                if (!group->is_overflow(group_pos)) {
                    return this->last_slot();
                }
            }
            slot_base += kGroupSize;
            group++;
            if (group >= last_group) {
                group = this->groups();
                slot_base = 0;
            }
            skip_groups++;
            if (skip_groups > kSkipGroupsLimit) {
                std::cout << "indirect_find(): key = " << key <<
                             ", skip_groups = " << skip_groups << std::endl;
            }
            if (skip_groups >= this->group_capacity()) {
                return this->last_slot();
            }
        }
    }

    template <typename KeyT>
    size_type find_index(const KeyT & key) {
        return const_cast<const this_type *>(this)->find_index<KeyT>(key);
    }

    template <typename KeyT>
    size_type find_index(const KeyT & key) const {
        if (!kIsIndirectKV) {
            return this->direct_find_index<KeyT>(key);
        } else {
            return this->indirect_find_index<KeyT>(key);
        }
    }

    template <typename KeyT>
    size_type direct_find_index(const KeyT & key) const {
        std::size_t hash_code = this->get_hash(key);
        size_type slot_index = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->get_ctrl_hash(hash_code);
        size_type group_index = slot_index / kGroupSize;
        size_type group_pos = slot_index % kGroupSize;
        const group_type * group = this->group_at(group_index);
        const group_type * last_group = this->last_group();
        
        size_type slot_base = group_index * kGroupSize;
        size_type skip_groups = 0;

        for (;;) {
            std::uint32_t match_mask = group->match_hash(ctrl_hash);
            if (match_mask != 0) {
                do {
                    std::uint32_t match_pos = BitUtils::bsf32(match_mask);
                    std::uint32_t match_bit = BitUtils::ls1b32(match_mask);
                    match_mask ^= match_bit;

                    size_type slot_pos = slot_base + match_pos;
                    const slot_type * slot = this->slot_at(slot_pos);
                    if (this->key_equal_(slot->value.first, key)) {
                        return slot_pos;
                    }
                } while (match_mask != 0);
            } else {
                // If it doesn't overflow, means it hasn't been found.
                if (!group->is_overflow(group_pos)) {
                    return this->slot_capacity();
                }
            }
            slot_base += kGroupSize;
            group++;
            if (group >= last_group) {
                group = this->groups();
                slot_base = 0;
            }
            skip_groups++;
            if (skip_groups > kSkipGroupsLimit) {
                std::cout << "direct_find_index(): key = " << key <<
                             ", skip_groups = " << skip_groups << std::endl;
            }
            if (skip_groups >= this->group_capacity()) {
                return this->slot_capacity();
            }
        }
    }

    template <typename KeyT>
    KeyT & slot_key_at(size_type index) {
        return this->key_arrays_[index];
    }

    template <typename KeyT>
    size_type indirect_find_index(const KeyT & key) const {
        if (!kIsIndirectKV) {
            assert(false);
        }

        std::size_t hash_code = this->get_hash(key);
        size_type slot_index = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->get_ctrl_hash(hash_code);
        size_type group_index = slot_index / kGroupSize;
        size_type group_pos = slot_index % kGroupSize;
        const group_type * group = this->group_at(group_index);
        const group_type * last_group = this->last_group();
        
        size_type slot_base = group_index * kGroupSize;
        size_type skip_groups = 0;

        for (;;) {
            std::uint32_t match_mask = group->match_hash(ctrl_hash);
            if (match_mask != 0) {
                do {
                    std::uint32_t match_pos = BitUtils::bsf32(match_mask);
                    std::uint32_t match_bit = BitUtils::ls1b32(match_mask);
                    match_mask ^= match_bit;

                    size_type slot_pos = slot_base + match_pos;
                    const slot_type * slot = this->slot_at(slot_pos);
                    if (!kIsIndirectKey) {
                        if (this->key_equal_(slot->value.first, key)) {
                            return slot_pos;
                        }
                    } else {
                        KeyT & ind_key = this->slot_key_at<KeyT>(slot->value.first);
                        if (this->key_equal_(ind_key, key)) {
                            return slot_pos;
                        }
                    }
                } while (match_mask != 0);
            } else {
                // If it doesn't overflow, means it hasn't been found.
                if (!group->is_overflow(group_pos)) {
                    return this->slot_capacity();
                }
            }
            slot_base += kGroupSize;
            group++;
            if (group >= last_group) {
                group = this->groups();
                slot_base = 0;
            }
            skip_groups++;
            if (skip_groups > kSkipGroupsLimit) {
                std::cout << "indirect_find_index(): key = " << key <<
                             ", skip_groups = " << skip_groups << std::endl;
            }
            if (skip_groups >= this->group_capacity()) {
                return this->slot_capacity();
            }
        }
    }

    template <typename KeyT>
    std::pair<slot_type *, FindResult>
    find_or_insert(const KeyT & key) {
        if (!kIsIndirectKV) {
            return this->direct_find_or_insert(key);
        } else {
            return this->indirect_find_or_insert(key);
        }
    }

    template <typename KeyT>
    JSTD_FORCED_INLINE
    std::pair<slot_type *, FindResult>
    direct_find_or_insert(const KeyT & key) {
        size_type slot_index = this->find_index(key);
        if (slot_index != this->slot_capacity()) {
            const slot_type * slot = this->slot_at(slot_index);
            return { slot, kIsExists };
        }

        if (this->need_grow()) {
            // The size of slot reach the slot threshold or hashmap is full.
            this->grow_if_necessary();
        }

        slot_index = this->find_failed(key);
        slot_type * slot = this->slot_at(slot_index);
        ctrl_type * ctrl = this->ctrl_at(slot_index);

        std::uint8_t ctrl_hash = 0;

        assert(ctrl->is_empty());
        this->set_used_ctrl(ctrl, ctrl_hash);
        return { slot, kIsNotExists };
    }

    template <typename KeyT>
    JSTD_FORCED_INLINE
    std::pair<slot_type *, FindResult>
    indirect_find_or_insert(const KeyT & key) {
        size_type slot_index = this->find_index(key);
        if (slot_index != this->slot_capacity()) {
            const slot_type * slot = this->slot_at(slot_index);
            return { slot, kIsExists };
        }

        if (this->need_grow()) {
            // The size of slot reach the slot threshold or hashmap is full.
            this->grow_if_necessary();
        }

        slot_index = this->find_failed(key);
        ctrl_type * ctrl = this->ctrl_at(slot_index);

        size_type new_slot_index = this->slot_size_;
        slot_type * new_slot = this->slot_at(new_slot_index);

        std::uint8_t ctrl_hash = 0;

        assert(ctrl->is_empty());
        this->set_used_ctrl(ctrl, ctrl_hash);
        return { new_slot, kIsNotExists };
    }

    template <typename KeyT>
    JSTD_FORCED_INLINE
    size_type find_failed(const KeyT & key) {
        std::size_t hash_code = this->get_hash(key);
        size_type slot_index = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->get_ctrl_hash(hash_code);
        size_type group_index = slot_index / kGroupSize;
        size_type group_pos = slot_index % kGroupSize;
        group_type * group = this->group_at(group_index);
        group_type * last_group = this->last_group();
        
        size_type skip_groups = 0;
        size_type slot_base = group_index * kGroupSize;

        for (;;) {
            std::uint32_t empty_mask = group->match_empty();
            if (empty_mask != 0) {
                std::uint32_t empty_pos = BitUtils::bsf32(empty_mask);
                size_type slot_pos = slot_base + empty_pos;
                return slot_pos;
            } else {
                // If it doesn't overflow, set the overflow bit.
                if (!group->is_overflow(group_pos)) {
                    group->set_overflow(group_pos);
                }
            }
            slot_base += kGroupSize;
            group++;
            if (group >= last_group) {
                group = this->groups();
                slot_base = 0;
            }
            skip_groups++;
            if (skip_groups > kSkipGroupsLimit) {
                std::cout << "direct_find_failed(): key = " << key <<
                             ", skip_groups = " << skip_groups << std::endl;
            }
            if (skip_groups >= this->group_capacity()) {
                return this->slot_capacity();
            }
        }
    }

    template <bool AlwaysUpdate>
    std::pair<iterator, bool> emplace_impl(const init_type & value) {
        auto find_info = this->find_or_insert(value.first);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot, value);
            this->slot_size_++;
            return { this->iterator_at(slot), true };
        } else if (result == kIsExists) {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                slot->value.second = value.second;
            }
            return { this->iterator_at(slot), false };
        } else {
            assert(result == kNeedGrow);
            this->grow_if_necessary();
            return this->emplace_impl<AlwaysUpdate>(value);
        }
    }

    template <bool AlwaysUpdate>
    std::pair<iterator, bool> emplace_impl(init_type && value) {
        auto find_info = this->find_or_insert(value.first);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot, std::forward<init_type>(value));
            this->slot_size_++;
            return { this->iterator_at(slot), true };
        } else if (result == kIsExists) {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                static constexpr bool is_rvalue_ref = std::is_rvalue_reference<decltype(value)>::value;
                if (is_rvalue_ref)
                    slot->value.second = std::move(value.second);
                else
                    slot->value.second = value.second;
            }
            return { this->iterator_at(slot), false };
        } else {
            assert(result == kNeedGrow);
            this->grow_if_necessary();
            return this->emplace_impl<AlwaysUpdate>(std::forward<init_type>(value));
        }
    }

    template <bool AlwaysUpdate, typename KeyT, typename MappedT>
    std::pair<iterator, bool> emplace_impl(KeyT && key, MappedT && value) {
        auto find_info = this->find_or_insert(key);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot,
                                        std::forward<KeyT>(key),
                                        std::forward<MappedT>(value));
            this->slot_size_++;
            return { this->iterator_at(slot), true };
        } else if (result == kIsExists) {
            // The key to be inserted already exists.
            static constexpr bool isMappedType = jstd::is_same_ex<MappedT, mapped_type>::value;
            if (AlwaysUpdate) {
                if (isMappedType) {
                    slot->value.second = std::forward<MappedT>(value);
                } else {
                    mapped_type mapped_value(std::forward<MappedT>(value));
                    slot->value.second = std::move(mapped_value);
                }
            }
            return { this->iterator_at(slot), false };
        } else {
            assert(result == kNeedGrow);
            this->grow_if_necessary();
            return this->emplace_impl<AlwaysUpdate, KeyT, MappedT>(
                    std::forward<KeyT>(key), std::forward<MappedT>(value)
                );
        }
    }

    template <bool AlwaysUpdate, typename KeyT, typename ... Args>
    std::pair<iterator, bool> emplace_impl(KeyT && key, Args && ... args) {
        auto find_info = this->find_or_insert(key);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward_as_tuple(std::forward<KeyT>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            this->slot_size_++;
            return { this->iterator_at(slot), true };
        } else if (result == kIsExists) {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                mapped_type mapped_value(std::forward<Args>(args)...);
                slot->value.second = std::move(mapped_value);
            }
            return { this->iterator_at(slot), false };
        } else {
            assert (result == kNeedGrow);
            this->grow_if_necessary();
            return this->emplace(std::piecewise_construct,
                                 std::forward_as_tuple(std::forward<KeyT>(key)),
                                 std::forward_as_tuple(std::forward<Args>(args)...));
        }
    }

    template <bool AlwaysUpdate, typename PieceWise,
              typename ... Ts1, typename ... Ts2>
    std::pair<iterator, bool> emplace_impl(PieceWise && hint,
                                           std::tuple<Ts1...> && first,
                                           std::tuple<Ts2...> && second) {
        tuple_wrapper2<key_type> key_wrapper(first);
        auto find_info = this->find_or_insert(key_wrapper.value());
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward<std::tuple<Ts1...>>(first),
                                        std::forward<std::tuple<Ts2...>>(second));
            this->slot_size_++;
            return { this->iterator_at(slot), true };
        } else if (result == kIsExists) {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                tuple_wrapper2<mapped_type> mapped_wrapper(std::move(second));
                slot->value.second = std::move(mapped_wrapper.value());
            }
            return { this->iterator_at(slot), false };
        } else {
            assert (result == kNeedGrow);
            this->grow_if_necessary();
            return this->emplace(std::piecewise_construct,
                                 std::forward<std::tuple<Ts1...>>(first),
                                 std::forward<std::tuple<Ts2...>>(second));
        }
    }

#if 1
    template <bool AlwaysUpdate, typename First, typename std::enable_if<
              (!jstd::is_same_ex<First, value_type>::value &&
               !std::is_constructible<value_type, First &&>::value) &&
              (!jstd::is_same_ex<First, init_type>::value &&
               !std::is_constructible<init_type, First &&>::value) &&
              (!jstd::is_same_ex<First, std::piecewise_construct_t>::value) &&
              (!jstd::is_same_ex<First, key_type>::value &&
               !std::is_constructible<key_type, First &&>::value)>::type * = nullptr,
                typename ... Args>
    std::pair<iterator, bool> emplace_impl(First && first, Args && ... args) {
        alignas(slot_type) unsigned char raw[sizeof(slot_type)];
        slot_type * tmp_slot = reinterpret_cast<slot_type *>(&raw);

        SlotPolicyTraits::construct(&this->allocator_, tmp_slot,
                                    std::forward<First>(first),
                                    std::forward<Args>(args)...);

        auto find_info = this->find_or_insert(tmp_slot->value.first);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);

            SlotPolicyTraits::transfer(&this->allocator_, slot, tmp_slot);
            this->slot_size_++;
            return { this->iterator_at(slot), true };
        } else if (result == kIsExists) {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                slot->value.second = std::move(tmp_slot->value.second);
            }
            SlotPolicyTraits::destroy(&this->allocator_, tmp_slot);
            return { this->iterator_at(slot), false };
        } else {
            assert (result == kNeedGrow);
            this->grow_if_necessary();
            if (kIsLayoutCompatible)
                return this->emplace(std::move(tmp_slot->mutable_value));
            else
                return this->emplace(std::move(tmp_slot->value));
        }
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////////////////

    template <typename KeyT, typename ... Args>
    std::pair<iterator, bool> try_emplace_impl(const KeyT & key, Args && ... args) {
        auto find_info = this->find_or_insert(key);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward_as_tuple(key),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            this->slot_size_++;
        } else if (result == kNeedGrow) {
            this->grow_if_necessary();
            return this->try_emplace_impl(key, std::forward<Args>(args)...);
        }
        return { this->iterator_at(slot), (result == kIsNotExists) };
    }

    template <typename KeyT, typename ... Args>
    std::pair<iterator, bool> try_emplace_impl(KeyT && key, Args && ... args) {
        auto find_info = this->find_or_insert(key);
        slot_type * slot = find_info.first;
        FindResult result = find_info.second;
        if (result == kIsNotExists) {
            // The key to be inserted is not exists.
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward_as_tuple(std::forward<KeyT>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            this->slot_size_++;
        } else if (result == kNeedGrow) {
            this->grow_if_necessary();
            return this->try_emplace_impl(std::forward<KeyT>(key),
                                          std::forward<Args>(args)...);
        }
        return { this->iterator_at(slot), (result == kIsNotExists) };
    }
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
