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
#include <algorithm>        // For std::max()
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

#define CLUSTER_USE_HASH_POLICY     0
#define CLUSTER_USE_SEPARATE_SLOTS  1
#define CLUSTER_USE_SWAP_TRAITS     1

#define CLUSTER_USE_GROUP_SCAN      1

#ifdef _DEBUG
#define CLUSTER_DISPLAY_DEBUG_INFO  1
#endif

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

    static constexpr bool kIsTransparent = (jstd::is_transparent<Hash>::value && jstd::is_transparent<KeyEqual>::value);
    static constexpr bool kIsLayoutCompatible = jstd::is_layout_compatible_kv<key_type, mapped_type>::value;

    static constexpr size_type npos = static_cast<size_type>(-1);

    using ctrl_type = cluster_meta_ctrl;
    using group_type = flat_map_cluster16<cluster_meta_ctrl>;

    static constexpr std::uint8_t kHashMask     = ctrl_type::kHashMask;
    static constexpr std::uint8_t kEmptySlot    = ctrl_type::kEmptySlot;
    static constexpr std::uint8_t kOverflowMask = ctrl_type::kOverflowMask;

    static constexpr size_type kGroupWidth = group_type::kGroupWidth;

    static constexpr bool kIsPlainKey    = jstd::is_plain_type<key_type>::value;
    static constexpr bool kIsPlainMapped = jstd::is_plain_type<mapped_type>::value;

    static constexpr bool kIsPlainKV = kIsPlainKey && kIsPlainMapped;

    static constexpr bool kHasSwapKey    = jstd::has_member_swap<key_type, key_type &>::value;
    static constexpr bool kHasSwapMapped = jstd::has_member_swap<mapped_type, mapped_type &>::value;

    static constexpr bool kIsSwappableKey    = jstd::is_swappable<key_type>::value;
    static constexpr bool kIsSwappableMapped = jstd::is_swappable<mapped_type>::value;

    static constexpr bool kIsSwappableKV = kIsSwappableKey && kIsSwappableMapped;

    static constexpr bool kIsMoveAssignKey    = std::is_move_assignable<key_type>::value;
    static constexpr bool kIsMoveAssignMapped = std::is_move_assignable<mapped_type>::value;

    static constexpr bool is_slot_trivial_copyable =
            (std::is_trivially_copyable<value_type>::value ||
            (std::is_trivially_copyable<key_type>::value &&
             std::is_trivially_copyable<mapped_type>::value) ||
            (std::is_scalar<key_type>::value && std::is_scalar<mapped_type>::value));

    static constexpr bool is_slot_trivial_destructor =
            (std::is_trivially_destructible<value_type>::value ||
            (std::is_trivially_destructible<key_type>::value &&
             std::is_trivially_destructible<mapped_type>::value) ||
            (jstd::is_plain_type<key_type>::value &&
             jstd::is_plain_type<mapped_type>::value));

    static constexpr size_type kSizeTypeLength = sizeof(std::size_t);

    static constexpr bool kIsSmallKeyType   = (sizeof(key_type)    <= kSizeTypeLength * 2);
    static constexpr bool kIsSmallValueType = (sizeof(mapped_type) <= kSizeTypeLength * 2);

    static constexpr bool kDetectIsIndirectKey = !(jstd::is_plain_type<key_type>::value ||
                                                  (sizeof(key_type) <= kSizeTypeLength * 2) ||
                                                 ((sizeof(key_type) <= (kSizeTypeLength * 4)) &&
                                                   is_slot_trivial_destructor));

    static constexpr bool kDetectIsIndirectValue = !(jstd::is_plain_type<mapped_type>::value ||
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
    static constexpr size_type kGroupAlignment = compile_time::is_pow2<alignof(group_type)>::value ?
                                                 jstd::cmax(alignof(group_type), kCacheLineSize) :
                                                 alignof(group_type);
    static constexpr size_type kSlotAlignment = compile_time::is_pow2<alignof(slot_type)>::value ?
                                                jstd::cmax(alignof(slot_type), kCacheLineSize) :
                                                alignof(slot_type);

    using iterator       = flat_map_iterator<this_type, value_type, kIsIndirectKV>;
    using const_iterator = flat_map_iterator<this_type, const value_type, kIsIndirectKV>;

    static constexpr size_type kDefaultCapacity = 0;
    // kMinCapacity must be >= 2
    static constexpr size_type kMinCapacity = 2;

    static constexpr float kMinLoadFactorF = 0.5f;
    static constexpr float kMaxLoadFactorF = 0.875f;
    static constexpr float kDefaultLoadFactorF = 0.8f;
    // Default load factor = 224 / 256 = 0.875
    static constexpr size_type kLoadFactorAmplify = 256;
    static constexpr size_type kDefaultMaxLoadFactor =
        static_cast<size_type>((double)kLoadFactorAmplify * (double)kDefaultLoadFactorF + 0.5);

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
    slot_type *     slots_;
    size_type       slot_size_;
    size_type       slot_mask_;     // slot_capacity = slot_mask + 1
    size_type       slot_threshold_;

    size_type       mlf_;

#if CLUSTER_USE_SEPARATE_SLOTS
    group_type *    groups_alloc_;
#endif

#if CLUSTER_USE_HASH_POLICY
    hash_policy_t           hash_policy_;
#endif

    hasher                  hasher_;
    key_equal               key_equal_;

    allocator_type          allocator_;
    group_allocator_type    group_allocator_;
    ctrl_allocator_type     ctrl_allocator_;
    slot_allocator_type     slot_allocator_;

    static constexpr bool kIsExists = false;
    static constexpr bool kNeedInsert = true;

public:
    cluster_flat_table() : cluster_flat_table(kDefaultCapacity) {}

    explicit cluster_flat_table(size_type capacity, hasher const & hash = hasher(),
                                key_equal const & pred = key_equal(),
                                allocator_type const & allocator = allocator_type())
        : groups_(nullptr), slots_(nullptr), slot_size_(0), slot_mask_(static_cast<size_type>(capacity - 1)),
          slot_threshold_(calc_slot_threshold(kDefaultMaxLoadFactor, capacity)), mlf_(kDefaultMaxLoadFactor)
#if CLUSTER_USE_SEPARATE_SLOTS
          , groups_alloc_(nullptr)
#endif
    {
        this->create_slots<true>(capacity);
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
    iterator begin() noexcept {
        size_type slot_index = this->find_first_used_index();
        return this->iterator_at(slot_index);
    }

    iterator end() noexcept {
        return this->iterator_at(this->slot_capacity());
    }

    const_iterator begin() const noexcept {
        return const_cast<this_type *>(this)->begin();
    }
    const_iterator end() const noexcept {
        return const_cast<this_type *>(this)->end();
    }

    const_iterator cbegin() const noexcept { this->begin(); }
    const_iterator cend() const noexcept { this->end(); }

    ///
    /// Capacity
    ///
    bool empty() const noexcept { return (this->size() == 0); }
    size_type size() const noexcept { return this->slot_size(); }
    size_type capacity() const noexcept { return this->slot_capacity(); }
    size_type max_size() const noexcept {
        return (std::numeric_limits<difference_type>::max)() / sizeof(value_type);
    }

    size_type slot_size() const { return this->slot_size_; }
    size_type slot_mask() const { return this->slot_mask_; }
    size_type slot_capacity() const { return (this->slot_mask_ + 1); }
    size_type slot_threshold() const { return this->slot_threshold_; }

    size_type ctrl_capacity() const { return this->slot_capacity(); }
    size_type max_ctrl_capacity() const { return (this->group_capacity() * kGroupWidth); }

    size_type group_capacity() const {
        return ((this->slot_capacity() + (kGroupWidth - 1)) / kGroupWidth);
    }

    bool is_valid() const { return (this->groups() != nullptr); }
    bool is_empty() const { return (this->size() == 0); }

    ///
    /// Bucket interface
    ///
    size_type bucket_size(size_type n) const noexcept { return 1; }
    size_type bucket_count() const noexcept { return this->slot_capacity(); }
    size_type max_bucket_count() const noexcept {
        return (std::numeric_limits<difference_type>::max)() / sizeof(ctrl_type);
    }

    size_type bucket(const key_type & key) const {
        size_type ctrl_index = this->find_index(key);
        return ctrl_index;
    }

    ///
    /// Hash policy
    ///
    float load_factor() const {
        if (this->slot_capacity() != 0)
            return ((float)this->slot_size() / this->slot_capacity());
        else
            return 0.0;
    }

    float max_load_factor() const {
        return ((float)this->mlf_ / kLoadFactorAmplify);
    }

    void max_load_factor(float mlf) const {
        // mlf: [0.2, 0.875]
        if (mlf < kMinLoadFactorF)
            mlf = kMinLoadFactorF;
        if (mlf > kMaxLoadFactorF)
            mlf = kMaxLoadFactorF;
        size_type mlf_int = static_cast<size_type>((float)kLoadFactorAmplify * mlf);
        this->mlf_ = mlf_int;

        size_type new_slot_threshold = this->calc_slot_threshold(this->slot_size());
        size_type new_slot_capacity = this->calc_capacity(new_slot_threshold);
        if (new_slot_capacity > this->slot_capacity()) {
            this->rehash(new_slot_capacity);
        }
    }

    ///
    /// Pointers
    ///
    group_type * groups() { return this->groups_; }
    const group_type * groups() const {
        return const_cast<const group_type *>(this->groups_);
    }

#if CLUSTER_USE_SEPARATE_SLOTS
    group_type * groups_alloc() { return this->groups_alloc_; }
    const group_type * groups_alloc() const {
        return const_cast<const group_type *>(this->groups_alloc_);
    }
#else
    group_type * groups_alloc() { return nullptr; }
    const group_type * groups_alloc() const {
        return nullptr;
    }
#endif

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
        if (!kIsIndirectKV)
            return (this->slots() + this->slot_capacity());
        else
            return (this->slots() + this->slot_size());
    }
    const slot_type * last_slot() const {
        if (!kIsIndirectKV)
            return (this->slots() + this->slot_capacity());
        else
            return (this->slots() + this->slot_size());
    }

    ///
    /// Hash policy
    ///

    //
    // See: https://en.cppreference.com/w/cpp/container/unordered_map/reserve
    //
    // Sets the number of buckets to the number needed to accommodate
    // at least count elements without exceeding maximum load factor
    // and rehashes the container, i.e. puts the elements into
    // appropriate buckets considering that total number of buckets has changed.
    // Effectively calls rehash(std::ceil(new_capacity / max_load_factor())).
    //
    void reserve(size_type new_capacity) {
        if (likely(new_capacity != 0)) {
            new_capacity = this->shrink_to_fit_capacity(new_capacity);
            this->rehash_impl<false>(new_capacity);
        } else {
            this->reset<false>();
        }
    }

    //
    // See: https://en.cppreference.com/w/cpp/container/unordered_map/rehash
    //
    // Changes the number of buckets to a value n that is not less than count
    // and satisfies n >= std::ceil(size() / max_load_factor()), then rehashes the container,
    // i.e. puts the elements into appropriate buckets considering that
    // total number of buckets has changed.
    //
    void rehash(size_type new_capacity) {
        size_type fit_to_now = this->shrink_to_fit_capacity(this->size());
        new_capacity = (std::max)(fit_to_now, new_capacity);
        this->rehash_impl<true>(new_capacity);
    }

    void shrink_to_fit(bool read_only = false) {
        size_type new_capacity;
        if (likely(!read_only))
            new_capacity = this->shrink_to_fit_capacity(this->slot_size());
        else
            new_capacity = this->slot_size();
        this->rehash_impl<true>(new_capacity);
    }

    ///
    /// Lookup
    ///
    size_type count(const key_type & key) const {
        const slot_type * slot = this->find_impl(key);
        return (slot != this->last_slot()) ? 1 : 0;
    }

    bool contains(const key_type & key) const {
        const slot_type * slot = this->find_impl(key);
        return (slot != this->last_slot());
    }

    ///
    /// find(key)
    ///
    iterator find(const key_type & key) {
        return const_cast<const this_type *>(this)->find(key);
    }

    const_iterator find(const key_type & key) const {
        size_type slot_index = this->find_index(key);
        return this->iterator_at(slot_index);
    }

    template <typename KeyT>
    iterator find(const KeyT & key) {
        return const_cast<const this_type *>(this)->find(key);
    }

    template <typename KeyT>
    const_iterator find(const KeyT & key) const {
        size_type slot_index = this->find_index(key);
        return this->iterator_at(slot_index);
    }

    ///
    /// Modifiers
    ///
    void clear(bool need_destroy = false) noexcept {
        if (need_destroy) {
            this->create_slots<false>(kDefaultCapacity);
            assert(this->slot_size() == 0);
            return;
        } else {
            this->clear_data();
            assert(this->slot_size() == 0);
        }
    }

    ///
    /// insert(value)
    ///
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> insert(const value_type & value) {
        return this->emplace_impl<false>(value);
    }

    JSTD_FORCED_INLINE
    std::pair<iterator, bool> insert(value_type && value) {
        return this->emplace_impl<false>(std::move(value));
    }

    JSTD_FORCED_INLINE
    std::pair<iterator, bool> insert(const init_type & value) {
        return this->emplace_impl<false>(value);
    }

    JSTD_FORCED_INLINE
    std::pair<iterator, bool> insert(init_type && value) {
        return this->emplace_impl<false>(std::move(value));
    }

    template <typename P, typename std::enable_if<
              (!std::is_same<P, value_type>::value &&
               !std::is_same<P, init_type >::value) &&
               (std::is_constructible<value_type, P &&>::value ||
                std::is_constructible<init_type,  P &&>::value)>::type * = nullptr>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> insert(P && value) {
        return this->emplace_impl<false>(std::forward<P>(value));
    }

    JSTD_FORCED_INLINE
    iterator insert(const_iterator hint, const value_type & value) {
        return this->emplace_impl<false>(value).first;
    }

    JSTD_FORCED_INLINE
    iterator insert(const_iterator hint, value_type && value) {
        return this->emplace_impl<false>(std::move(value)).first;
    }

    JSTD_FORCED_INLINE
    iterator insert(const_iterator hint, const init_type & value) {
        return this->emplace_impl<false>(value).first;
    }

    JSTD_FORCED_INLINE
    iterator insert(const_iterator hint, init_type && value) {
        return this->emplace_impl<false>(std::move(value)).first;
    }

    template <typename P, typename std::enable_if<
              (!std::is_same<P, value_type>::value &&
               !std::is_same<P, init_type >::value) &&
               (std::is_constructible<value_type, P &&>::value ||
                std::is_constructible<init_type,  P &&>::value)>::type * = nullptr>
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
    /// erase(key)
    ///
    JSTD_FORCED_INLINE
    size_type erase(const key_type & key) {
        size_type num_deleted = this->find_and_erase(key);
        return num_deleted;
    }

    JSTD_FORCED_INLINE
    iterator erase(iterator pos) {
        size_type slot_index = pos.index();
        this->erase_index(slot_index);
        ctrl_type * ctrl = this->ctrl_at(slot_index);
        return this->next_valid_iterator(ctrl, pos);
    }

    JSTD_FORCED_INLINE
    iterator erase(const_iterator pos) {
        return this->erase(iterator(pos));
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

    inline group_type * group_at(size_type group_index) noexcept {
        assert(group_index <= this->group_capacity());
        return (this->groups() + std::ptrdiff_t(group_index));
    }

    inline const group_type * group_at(size_type group_index) const noexcept {
        assert(group_index <= this->slot_capacity());
        return (this->groups() + std::ptrdiff_t(group_index));
    }

    inline group_type * group_by_slot_index(size_type slot_index) noexcept {
        assert(slot_index <= this->slot_capacity());
        size_type group_index = slot_index / kGroupWidth;
        return (this->groups() + std::ptrdiff_t(group_index));
    }

    inline const group_type * group_by_slot_index(size_type slot_index) const noexcept {
        assert(slot_index <= this->slot_capacity());
        size_type group_idx = slot_index / kGroupWidth;
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
    static group_type * default_empty_groups() {
        alignas(16) static const ctrl_type s_empty_ctrls[16] = {
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot },
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot },
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot },
            { kEmptySlot }, { kEmptySlot }, { kEmptySlot }, { kEmptySlot }
        };

        return reinterpret_cast<group_type *>(const_cast<ctrl_type *>(&s_empty_ctrls[0]));
    }

    static ctrl_type * default_empty_ctrls() {
        return reinterpret_cast<ctrl_type *>(this_type::default_empty_groups());
    }

    JSTD_FORCED_INLINE
    size_type calc_capacity(size_type init_capacity) const noexcept {
        size_type new_capacity = (std::max)(init_capacity, kMinCapacity);
                  new_capacity = (std::max)(new_capacity, this->slot_size());
        if (!pow2::is_pow2(new_capacity)) {
            new_capacity = pow2::round_up<size_type, kMinCapacity>(new_capacity);
        }
        return new_capacity;
    }

    static size_type calc_slot_threshold(size_type mlf, size_type slot_capacity) {
        static constexpr size_type kSmallCapacity = kGroupWidth * 2;

        if (likely(slot_capacity > kSmallCapacity)) {
            return (slot_capacity * mlf / kLoadFactorAmplify);
        } else {
            /* When capacity is small, we allow 100% usage. */
            return slot_capacity;
        }
    }

    size_type calc_slot_threshold(size_type slot_capacity) const {
        return this_type::calc_slot_threshold(this->mlf_, slot_capacity);
    }

    inline size_type shrink_to_fit_capacity(size_type init_capacity) const {
        size_type new_capacity = init_capacity * kLoadFactorAmplify / this->mlf_;
        return new_capacity;
    }

    bool is_positive(size_type value) const {
        return (static_cast<intptr_t>(value) >= 0);
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

    inline iterator next_valid_iterator(ctrl_type * ctrl, const_iterator iter) {
        if (ctrl->is_used())
            return iter;
        else
            return iterator(++iter);
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

    inline iterator next_valid_iterator(const_iterator iter) {
        size_type index = this->index_of(iter);
        if (!kIsIndirectKV) {
            ctrl_type * ctrl = this->ctrl_at(index);
            return this->next_valid_iterator(ctrl, iter);
        } else {
            ++iter;
            return iterator(iter);
        }
    }

    inline size_type index_salt() const noexcept {
        return (size_type)((std::uintptr_t)this->ctrls() >> 12);
    }

    inline std::size_t hash_for(const key_type & key) const
        noexcept(noexcept(this->hasher_(key))) {
#if CLUSTER_USE_HASH_POLICY
        std::size_t hash_code = static_cast<std::size_t>(
            this->hash_policy_.get_hash_code(key)
        );
#elif defined(__GNUC__) || (defined(__clang__) && !defined(_MSC_VER))
        std::size_t hash_code;
        if (std::is_integral<key_type>::value && jstd::is_default_std_hash<Hash, key_type>::value)
            hash_code = hashes::msvc_fnv_1a((const unsigned char *)&key, sizeof(key_type));
        else
            hash_code = static_cast<std::size_t>(this->hasher_(key));
#else
        std::size_t hash_code = static_cast<std::size_t>(this->hasher_(key));
#endif
        return hash_code;
    }

    //
    // Do the index hash on the basis of hash code for the index_for_hash().
    //
    inline std::size_t index_hasher(std::size_t value) const noexcept {
        return value;
    }

    //
    // Do the ctrl hash on the basis of hash code for the ctrl hash.
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

    inline size_type index_for_hash(std::size_t hash_code) const noexcept {
#if CLUSTER_USE_HASH_POLICY
        if (kUseIndexSalt) {
            hash_code ^= this->index_salt();
        }
        size_type index = this->hash_policy_.template index_for_hash<key_type>(hash_code, this->slot_mask());
        return index;
#else
        hash_code = this->index_hasher(hash_code);
        if (kUseIndexSalt) {
            hash_code ^= this->index_salt();
        }
        size_type index = hash_code & this->slot_mask();
        return index;
#endif
    }

    inline std::uint8_t ctrl_for_hash(std::size_t hash_code) const noexcept {
        std::size_t ctrl_hash = this->ctrl_hasher(hash_code);
        std::uint8_t ctrl_hash8 = ctrl_type::hash_bits(ctrl_hash);
        return ((ctrl_hash8 != kEmptySlot) ? ctrl_hash8 : std::uint8_t(8));
    }

    size_type index_of(iterator iter) const {
        if (!kIsIndirectKV) {
            return iter.index();
        } else {
            const slot_type * slot = iter.slot();
            size_type ctrl_index = this->bucket(slot->key);
            return ctrl_index;
        }
    }

    size_type index_of(const_iterator iter) const {
        if (!kIsIndirectKV) {
            return iter.index();
        } else {
            const slot_type * slot = iter.slot();
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

    JSTD_FORCED_INLINE
    void init_groups(group_type * groups, size_type group_capacity) {
        if (groups != this_type::default_empty_groups()) {
            group_type * group = groups;
            group_type * last_group = group + group_capacity;
            for (; group < last_group; ++group) {
                group->init();
            }
        }
    }

    void destroy() {
        this->destroy_data();
    }

    void destroy_data() {
        // Note!!: destroy_slots() need use this->ctrls(), so must destroy slots first.
        size_type group_capacity = this->group_capacity();
        this->destroy_slots();
        this->destroy_groups(group_capacity);
    }

    void destroy_groups(size_type group_capacity) noexcept {
        if (this->groups_ != this_type::default_empty_groups()) {
            size_type total_group_alloc_count = this->TotalGroupAllocCount<kGroupAlignment>(group_capacity);
#if CLUSTER_USE_SEPARATE_SLOTS
            GroupAllocTraits::deallocate(this->group_allocator_, this->groups_alloc_, total_group_alloc_count);
            this->groups_alloc_ = this_type::default_empty_groups();
#endif
            this->groups_ = this_type::default_empty_groups();
        }
    }

    void destroy_slots() {
        this->clear_slots();

        if (this->slots_ != nullptr) {
#if CLUSTER_USE_SEPARATE_SLOTS
            SlotAllocTraits::deallocate(this->slot_allocator_, this->slots_, this->slot_capacity());
#else
            size_type total_slot_alloc_size = this->TotalSlotAllocCount<kGroupAlignment>(
                                                    this->group_capacity(), this->slot_capacity());
            SlotAllocTraits::deallocate(this->slot_allocator_, this->slots_, total_slot_alloc_size);
#endif
            this->slots_ = nullptr;
            this->slot_size_ = 0;
            this->slot_mask_ = 0;
            this->slot_threshold_ = 0;
        }
    }

    void clear_data() {
        // Note!!: clear_slots() need use this->ctrls(), so must clear slots first.
        this->clear_slots();
        this->clear_ctrls();
    }

    void clear_groups(group_type * groups, size_type group_capacity) {
        init_groups(groups, group_capacity);
    }

    JSTD_FORCED_INLINE
    void clear_slots() {
        if (!is_slot_trivial_destructor && (this->slots_ != nullptr)) {
            if (!kIsIndirectKV) {
#if CLUSTER_USE_GROUP_SCAN
                group_type * group = this->groups();
                group_type * last_group = this->last_group();
                slot_type * slot_base = this->slots();
                for (; group < last_group; ++group) {
                    std::uint32_t used_mask = group->match_used();
                    while (used_mask != 0) {
                        std::uint32_t used_pos = BitUtils::bsf32(used_mask);
                        used_mask = BitUtils::clearLowBit32(used_mask);
                        slot_type * slot = slot_base + used_pos;
                        this->destroy_slot(slot);
                    }
                    slot_base += kGroupWidth;
                }
#else
                ctrl_type * ctrl = this->ctrls();
                for (size_type slot_index = 0; slot_index < this->slot_capacity(); slot_index++) {
                    if (ctrl->is_used()) {
                        this->destroy_slot(slot_index);
                    }
                    ctrl++;
                }
#endif
            } else {
                slot_type * slot = this->slots();
                slot_type * last_slot = this->last_slot();
                for (; slot < last_slot; ++slot) {
                    this->destroy_slot(slot);
                }
            }
        }

        this->slot_size_ = 0;
    }

    inline bool need_grow() const {
        return (this->slot_size() >= this->slot_threshold());
    }

    inline void grow_if_necessary() {
        // The growth rate is 2 times
        size_type new_capacity = this->slot_capacity() * 2;
        this->rehash_impl<false>(new_capacity);
    }

    bool is_valid_capacity(size_type capacity) const {
        return ((capacity >= kMinCapacity) && pow2::is_pow2(capacity));
    }

    //
    // Given the pointer of actual allocated groups, computes the padding of
    // first groups (from the start of the backing allocation)
    // and return the beginning of groups.
    //
    template <size_type GroupAlignment>
    inline group_type * AlignedGroups(const group_type * groups_alloc) {
        static_assert((GroupAlignment > 0),
                      "jstd::cluster_flat_map::AlignedGroups<N>(): GroupAlignment must bigger than 0.");
        static_assert(((GroupAlignment & (GroupAlignment - 1)) == 0),
                      "jstd::cluster_flat_map::AlignedGroups<N>(): GroupAlignment must be power of 2.");
        size_type groups_start = reinterpret_cast<size_type>(groups_alloc);
        size_type groups_first = (groups_start + GroupAlignment - 1) & (~(GroupAlignment - 1));
        size_type groups_padding = static_cast<size_type>(groups_first - groups_start);
        group_type * groups = reinterpret_cast<group_type *>(
                                    reinterpret_cast<char *>(groups_start) + groups_padding);
        return groups;
    }

    //
    // Given the pointer of slots and the capacity of slot, computes the padding of
    // between slots and groups (from the start of the backing allocation)
    // and return the beginning of groups.
    //
    template <size_type GroupAlignment>
    inline group_type * AlignedSlotsAndGroups(const slot_type * slots, size_type slot_capacity) {
        static_assert((GroupAlignment > 0),
                      "jstd::cluster_flat_map::AlignedSlotsAndGroups<N>(): GroupAlignment must bigger than 0.");
        static_assert(((GroupAlignment & (GroupAlignment - 1)) == 0),
                      "jstd::cluster_flat_map::AlignedSlotsAndGroups<N>(): GroupAlignment must be power of 2.");
        const slot_type * last_slots = slots + slot_capacity;
        size_type last_slot = reinterpret_cast<size_type>(last_slots);
        size_type groups_first = (last_slot + GroupAlignment - 1) & (~(GroupAlignment - 1));
        size_type groups_padding = static_cast<size_type>(groups_first - last_slot);
        group_type * groups = reinterpret_cast<group_type *>(
                                    reinterpret_cast<char *>(last_slot) + groups_padding);
        return groups;
    }

    //
    // Given the pointer of groups, the capacity of a group,
    // computes the total allocate count of the backing group array.
    //
    template <size_type GroupAlignment>
    inline size_type TotalGroupAllocCount(size_type group_capacity) {
        const size_type num_group_bytes = group_capacity * sizeof(group_type);
        const size_type total_bytes = num_group_bytes + GroupAlignment;
        const size_type total_alloc_count = (total_bytes + sizeof(group_type) - 1) / sizeof(group_type);
        return total_alloc_count;
    }

    //
    // Given the pointer of slots, the capacity of a group and slot,
    // computes the total allocate count of the backing slot array.
    //
    template <size_type GroupAlignment>
    inline size_type TotalSlotAllocCount(size_type group_capacity, size_type slot_capacity) {
        const size_type num_group_bytes = group_capacity * sizeof(group_type);
        const size_type num_slot_bytes = slot_capacity * sizeof(slot_type);
        const size_type total_bytes = num_slot_bytes + GroupAlignment + num_group_bytes;
        const size_type total_alloc_count = (total_bytes + sizeof(slot_type) - 1) / sizeof(slot_type);
        return total_alloc_count;
    }

    template <bool NeedDestory>
    void reset() noexcept {
        if (!NeedDestory) {
            this->groups_ = this_type::default_empty_groups();
            this->slots_ = nullptr;
            this->slot_size_ = 0;
            this->slot_mask_ = 0;
            this->slot_threshold_ = 0;
#if CLUSTER_USE_SEPARATE_SLOTS
            this->groups_alloc_ = this_type::default_empty_groups();
#endif
        } else {
            this->destroy_data();
        }

#if CLUSTER_USE_HASH_POLICY
        this->hash_policy_.reset();
#endif
    }

    template <bool isInitialize = false>
    void create_slots(size_type init_capacity) {
        if (unlikely(init_capacity == 0)) {
            this->reset<false>();
            return;
        }

        size_type new_capacity;
        if (isInitialize) {
            new_capacity = this->shrink_to_fit_capacity(init_capacity);
            new_capacity = this->calc_capacity(new_capacity);
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
        size_type new_group_capacity = (new_ctrl_capacity + (kGroupWidth - 1)) / kGroupWidth;
        assert(new_group_capacity > 0);

        size_type new_max_slot_size = new_capacity * this->mlf_ / kLoadFactorAmplify;
        size_type new_slot_capacity = (!kIsIndirectKV) ? new_capacity : new_max_slot_size;

#if CLUSTER_USE_SEPARATE_SLOTS
        size_type total_group_alloc_count = this->TotalGroupAllocCount<kGroupAlignment>(new_group_capacity);
        group_type * new_groups_alloc = GroupAllocTraits::allocate(this->group_allocator_, total_group_alloc_count);
        group_type * new_groups = this->AlignedGroups<kGroupAlignment>(new_groups_alloc);

        slot_type * new_slots = SlotAllocTraits::allocate(this->slot_allocator_, new_slot_capacity);
#else
        size_type total_slot_alloc_count = this->TotalSlotAllocCount<kGroupAlignment>(new_group_capacity, new_slot_capacity);

        slot_type * new_slots = SlotAllocTraits::allocate(this->slot_allocator_, total_slot_alloc_count);
        group_type * new_groups = this->AlignedSlotsAndGroups<kGroupAlignment>(new_slots, new_slot_capacity);
#endif

        // Reset groups to default state
        this->clear_groups(new_groups, new_group_capacity);

        this->groups_ = new_groups;
        this->slots_ = new_slots;
#if CLUSTER_USE_SEPARATE_SLOTS
        this->groups_alloc_ = new_groups_alloc;
#endif

        if (isInitialize) {
            assert(this->slot_size_ == 0);
        } else {
            this->slot_size_ = 0;
        }
        this->slot_mask_ = new_capacity - 1;
        this->slot_threshold_ = this->calc_slot_threshold(new_capacity);
    }

    template <bool AllowShrink>
    JSTD_NO_INLINE
    void rehash_impl(size_type new_capacity) {
        new_capacity = this->calc_capacity(new_capacity);
        assert(new_capacity > 0);
        assert(new_capacity >= kMinCapacity);
        if ((!AllowShrink && (new_capacity > this->slot_capacity())) ||
            (AllowShrink && (new_capacity != this->slot_capacity()))) {
            if (!AllowShrink) {
                assert(new_capacity >= this->slot_size());
            }

            group_type * old_groups = this->groups();
            group_type * old_groups_alloc = this->groups_alloc();
            size_type old_group_capacity = this->group_capacity();

            slot_type * old_slots = this->slots();
            slot_type * old_last_slot = this->last_slot();
            size_type old_slot_size = this->slot_size();
            size_type old_slot_mask = this->slot_mask();
            size_type old_slot_capacity = this->slot_capacity();
            size_type old_slot_threshold = this->slot_threshold();

            this->create_slots<false>(new_capacity);

            if (old_groups != this_type::default_empty_groups()) {
                group_type * group = old_groups;
                group_type * last_group = old_groups + old_group_capacity;
                slot_type * slot_base = old_slots;

                for (; group < last_group; ++group) {
                    uint32_t used_mask = group->match_used();
                    while (used_mask != 0) {
                        std::uint32_t used_pos = BitUtils::bsf32(used_mask);
                        used_mask = BitUtils::clearLowBit32(used_mask);
                        slot_type * old_slot = slot_base + used_pos;
                        assert(old_slot < old_last_slot);
                        this->insert_unique_and_no_grow(old_slot);
                        this->destroy_slot(old_slot);
                    }
                    slot_base += kGroupWidth;
                }
            }

            assert(this->slot_size() == old_slot_size);

#if CLUSTER_USE_SEPARATE_SLOTS
            if (old_groups != this_type::default_empty_groups()) {
                assert(old_groups_alloc != nullptr);
                size_type total_group_alloc_count = this->TotalGroupAllocCount<kGroupAlignment>(old_group_capacity);
                GroupAllocTraits::deallocate(this->group_allocator_, old_groups_alloc, total_group_alloc_count);
            }
            if (old_slots != nullptr) {
                SlotAllocTraits::deallocate(this->slot_allocator_, old_slots, old_slot_capacity);
            }
#else
            if (old_slots != nullptr) {
                size_type total_slot_alloc_count = this->TotalSlotAllocCount<kGroupAlignment>(
                                                        old_group_capacity, old_slot_capacity);
                SlotAllocTraits::deallocate(this->slot_allocator_, old_slots, total_slot_alloc_count);
            }
#endif
        }
    }

    JSTD_FORCED_INLINE
    void construct_slot(slot_type * slot) {
        SlotPolicyTraits::construct(&this->slot_allocator_, slot);
    }

    JSTD_FORCED_INLINE
    void construct_slot(size_type index) {
        slot_type * slot = this->slot_at(index);
        this->construct_slot(slot);
    }

    JSTD_FORCED_INLINE
    void destroy_slot(slot_type * slot) {
        if (!is_slot_trivial_destructor) {
            SlotPolicyTraits::destroy(&this->allocator_, slot);
        }
    }

    JSTD_FORCED_INLINE
    void destroy_slot(size_type index) {
        slot_type * slot = this->slot_at(index);
        this->destroy_slot(slot);
    }

    JSTD_FORCED_INLINE
    void destroy_slot_data(ctrl_type * ctrl, slot_type * slot) {
        assert(ctrl->is_used());
        ctrl->set_empty();
        this->destroy_slot(slot);
    }

    JSTD_FORCED_INLINE
    void destroy_slot_data(size_type index) {
        ctrl_type * ctrl = this->ctrl_at(index);
        slot_type * slot = this->slot_at(index);
        this->destroy_slot_data(ctrl, slot);
    }

    JSTD_FORCED_INLINE
    size_type find_first_used_index() const {
        if (this->size() != 0) {
            group_type * group = this->groups();
            group_type * last_group = this->last_group();
            size_type slot_base_index = 0;
            for (; group < last_group; ++group) {
                std::uint32_t used_mask = group->match_used();
                if (likely(used_mask != 0)) {
                    std::uint32_t used_pos = BitUtils::bsf32(used_mask);
                    size_type slot_index = slot_base_index + used_pos;
                    return slot_index;
                }
                slot_base_index += kGroupWidth;
            }
        }
        return this->slot_capacity();
    }

    JSTD_FORCED_INLINE
    size_type skip_empty_slots(size_type start_slot_index) const {
        if (this->size() != 0) {
            group_type * group = this->group_by_slot_index(start_slot_index);
            group_type * last_group = this->last_group();
            size_type slot_pos = start_slot_index % kGroupWidth;
            size_type slot_base_index = start_slot_index - slot_pos;
            // Last 4 items use ctrl seek, maybe faster.
            static const size_type kCtrlFasterSeekPos = 4;
            if (likely(slot_pos < (kGroupWidth - kCtrlFasterSeekPos))) {
                if (group < last_group) {
                    std::uint32_t used_mask = group->match_used();
                    // Filter out the bits in the leading position
                    // std::uint32_t non_excluded_mask = ~((std::uint32_t(1) << std::uint32_t(slot_pos)) - 1);
                    std::uint32_t non_excluded_mask = (std::uint32_t(0xFFFFFFFFu) << std::uint32_t(slot_pos));
                    used_mask &= non_excluded_mask;
                    if (likely(used_mask != 0)) {
                        std::uint32_t used_pos = BitUtils::bsf32(used_mask);
                        size_type slot_index = slot_base_index + used_pos;
                        return slot_index;
                    }
                    slot_base_index += kGroupWidth;
                    group++;
                }
            } else {
                size_type last_index = slot_base_index + kGroupWidth;
                ctrl_type * ctrl = this->ctrl_at(start_slot_index);
                while (start_slot_index < last_index) {
                    if (ctrl->is_used()) {
                        return start_slot_index;
                    }
                    ++ctrl;
                    ++start_slot_index;
                }
            }
            for (; group < last_group; ++group) {
                std::uint32_t used_mask = group->match_used();
                if (likely(used_mask != 0)) {
                    std::uint32_t used_pos = BitUtils::bsf32(used_mask);
                    size_type slot_index = slot_base_index + used_pos;
                    return slot_index;
                }
                slot_base_index += kGroupWidth;
            }
        }
        return this->slot_capacity();
    }

    template <typename KeyT>
    slot_type * find_impl(const KeyT & key) {
        return const_cast<slot_type *>(
            const_cast<const this_type *>(this)->find_impl(key)
        );
    }

    template <typename KeyT>
    const slot_type * find_impl(const KeyT & key) const {
        std::size_t hash_code = this->hash_for(key);
        size_type slot_index = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->ctrl_for_hash(hash_code);
        size_type group_index = slot_index / kGroupWidth;
        size_type group_pos = slot_index % kGroupWidth;
        const group_type * group = this->group_at(group_index);
        const group_type * first_group = group;
        const group_type * last_group = this->last_group();

        size_type slot_base = group_index * kGroupWidth;
        size_type skip_groups = 0;

        for (;;) {
            std::uint32_t match_mask = group->match_hash(ctrl_hash);
            if (match_mask != 0) {
                do {
                    std::uint32_t match_pos = BitUtils::bsf32(match_mask);
                    match_mask = BitUtils::clearLowBit32(match_mask);

                    size_type slot_pos = slot_base + match_pos;
                    const slot_type * slot = this->slot_at(slot_pos);
                    if (likely(this->key_equal_(key, slot->value.first))) {
                        return slot;
                    }
                } while (match_mask != 0);
            }

            // If it's not overflow, means it hasn't been found.
            if (likely(!group->is_overflow(group_pos))) {
                return this->last_slot();
            }

            slot_base += kGroupWidth;
            group++;
            if (unlikely(group >= last_group)) {
                group = this->groups();
                slot_base = 0;
            }
#if 0
            if (unlikely(group == first_group)) {
                return this->last_slot();
            }
#endif
#if CLUSTER_DISPLAY_DEBUG_INFO
            skip_groups++;
            if (unlikely(skip_groups > kSkipGroupsLimit)) {
                std::cout << "find_impl(): key = " << key <<
                             ", skip_groups = " << skip_groups <<
                             ", load_factor = " << this->load_factor() << std::endl;
            }
#endif
        }
    }

    template <typename KeyT>
    size_type find_index(const KeyT & key) {
        return const_cast<const this_type *>(this)->find_index<KeyT>(key);
    }

    template <typename KeyT>
    size_type find_index(const KeyT & key) const {
        std::size_t hash_code = this->hash_for(key);
        size_type slot_pos = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->ctrl_for_hash(hash_code);
        return this->find_index(key, slot_pos, ctrl_hash);
    }

    template <typename KeyT>
    JSTD_NO_INLINE
    size_type find_index(const KeyT & key, size_type slot_pos, std::uint8_t ctrl_hash) const {
        size_type group_index = slot_pos / kGroupWidth;
        size_type group_pos = slot_pos % kGroupWidth;
        const group_type * group = this->group_at(group_index);
        const group_type * first_group = group;
        const group_type * last_group = this->last_group();

        size_type slot_base = group_index * kGroupWidth;
        size_type skip_groups = 0;

        for (;;) {
            std::uint32_t match_mask = group->match_hash(ctrl_hash);
            if (match_mask != 0) {
                do {
                    std::uint32_t match_pos = BitUtils::bsf32(match_mask);
                    match_mask = BitUtils::clearLowBit32(match_mask);

                    size_type slot_index = slot_base + match_pos;
                    const slot_type * slot = this->slot_at(slot_index);
                    if (likely(this->key_equal_(key, slot->value.first))) {
                        return slot_index;
                    }
                } while (match_mask != 0);
            }

            // If it's not overflow, means it hasn't been found.
            if (likely(!group->is_overflow(group_pos))) {
                return this->slot_capacity();
            }

            slot_base += kGroupWidth;
            group++;
            if (unlikely(group >= last_group)) {
                group = this->groups();
                slot_base = 0;
            }
#if 0
            if (unlikely(group == first_group)) {
                return this->slot_capacity();
            }
#endif
#if CLUSTER_DISPLAY_DEBUG_INFO
            skip_groups++;
            if (unlikely(skip_groups > kSkipGroupsLimit)) {
                std::cout << "find_index(): key = " << key <<
                             ", skip_groups = " << skip_groups <<
                             ", load_factor = " << this->load_factor() << std::endl;
            }
#endif
        }
    }

    void display_meta_datas(group_type * group) {
        ctrl_type * ctrl = reinterpret_cast<ctrl_type *>(group);
        printf("[");
        for (std::size_t i = 0; i < kGroupWidth; i++) {
            if (i < kGroupWidth - 1)
                printf(" %02x,", (int)ctrl->get_value());
            else
                printf(" %02x", (int)ctrl->get_value());
            ctrl++;
        }
        printf(" ]\n");
    }

    template <typename KeyT>
    JSTD_FORCED_INLINE
    size_type find_first_empty_to_insert(const KeyT & key, size_type slot_pos, std::uint8_t ctrl_hash) {
        size_type group_index = slot_pos / kGroupWidth;
        size_type group_pos = slot_pos % kGroupWidth;
        group_type * group = this->group_at(group_index);
        group_type * first_group = group;
        group_type * last_group = this->last_group();

        group_type * prev_group = nullptr;
        size_type skip_groups = 0;
        size_type slot_base = group_index * kGroupWidth;

        for (;;) {
            std::uint32_t empty_mask = group->match_empty();
            if (empty_mask != 0) {
                std::uint32_t empty_pos = BitUtils::bsf32(empty_mask);
                assert(group->is_empty(empty_pos));
                group->set_used(empty_pos, ctrl_hash);
                size_type slot_index = slot_base + empty_pos;
                return slot_index;
            } else {
                // If it's not overflow, set the overflow bit.
                if (likely(!group->is_overflow(group_pos))) {
                    group->set_overflow(group_pos);
                }
            }
#if CLUSTER_DISPLAY_DEBUG_INFO
            prev_group = group;
#endif
            slot_base += kGroupWidth;
            group++;
            if (unlikely(group >= last_group)) {
                group = this->groups();
                slot_base = 0;
            }
#if 0
            if (unlikely(group == first_group)) {
                return this->slot_capacity();
            }
#endif
#if CLUSTER_DISPLAY_DEBUG_INFO
            skip_groups++;
            if (unlikely(skip_groups > kSkipGroupsLimit)) {
                std::cout << "find_first_empty_to_insert(): key = " << key <<
                             ", skip_groups = " << skip_groups <<
                             ", load_factor = " << this->load_factor() << std::endl;
                display_meta_datas(prev_group);
            }
#endif
        }
    }

    template <typename KeyT>
    JSTD_NO_INLINE
    std::pair<size_type, bool>
    find_and_insert(const KeyT & key) {
        std::size_t hash_code = this->hash_for(key);
        size_type slot_pos = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->ctrl_for_hash(hash_code);

        size_type slot_index = this->find_index(key, slot_pos, ctrl_hash);
        if (slot_index != this->slot_capacity()) {
            return { slot_index, kIsExists };
        }

        if (this->need_grow()) {
            // The size of slot reach the slot threshold or hashmap is full.
            this->grow_if_necessary();

            slot_pos = this->index_for_hash(hash_code);
            // Ctrl hash will not change
            // ctrl_hash = this->ctrl_for_hash(hash_code);
        }

        slot_index = this->find_first_empty_to_insert(key, slot_pos, ctrl_hash);
        assert(slot_index < this->slot_capacity());
        return { slot_index, kNeedInsert };
    }

    JSTD_FORCED_INLINE
    size_type insert_unique_and_no_grow(const key_type & key) {
        std::size_t hash_code = this->hash_for(key);
        size_type slot_pos = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->ctrl_for_hash(hash_code);

        size_type slot_index = this->find_first_empty_to_insert(key, slot_pos, ctrl_hash);
        return slot_index;
    }

    ///
    /// Use in rehash_impl()
    ///
    JSTD_FORCED_INLINE
    void insert_unique_and_no_grow(slot_type * old_slot) {
        assert(old_slot != nullptr);
        size_type slot_index = this->insert_unique_and_no_grow(old_slot->value.first);
        slot_type * new_slot = this->slot_at(slot_index);
        assert(new_slot != nullptr);

        SlotPolicyTraits::construct(&this->slot_allocator_, new_slot, old_slot);
        this->slot_size_++;
        assert(this->slot_size() <= this->slot_capacity());
    }

    template <bool AlwaysUpdate, typename ValueT, typename std::enable_if<
              (std::is_same<ValueT, value_type>::value ||
               std::is_constructible<value_type, const ValueT &>::value) ||
              (std::is_same<ValueT, init_type>::value ||
               std::is_constructible<init_type, const ValueT &>::value)>::type * = nullptr>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> emplace_impl(const ValueT & value) {
        auto find_info = this->find_and_insert(value.first);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;        
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->slot_allocator_, slot, value);
            this->slot_size_++;
        } else {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                slot_type * slot = this->slot_at(slot_index);
                slot->value.second = value.second;
            }
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    template <bool AlwaysUpdate, typename ValueT, typename std::enable_if<
              (jstd::is_same_ex<ValueT, value_type>::value ||
               std::is_constructible<value_type, ValueT &&>::value) ||
              (jstd::is_same_ex<ValueT, init_type>::value ||
               std::is_constructible<init_type, ValueT &&>::value)>::type * = nullptr>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> emplace_impl(ValueT && value) {
        auto find_info = this->find_and_insert(value.first);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            assert(slot_index < this->slot_capacity());
            SlotPolicyTraits::construct(&this->slot_allocator_, slot, std::move(value));
            this->slot_size_++;
        } else {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                static constexpr bool is_rvalue_ref = std::is_rvalue_reference<decltype(value)>::value;
                slot_type * slot = this->slot_at(slot_index);
                if (is_rvalue_ref) {
                    //slot->value.second = std::move(value.second);
                    if (kIsLayoutCompatible)
                        jstd::move_assign_if<is_rvalue_ref>(slot->mutable_value.second, value.second);
                    else
                        jstd::move_assign_if<is_rvalue_ref>(slot->value.second, value.second);
                } else {
                    slot->value.second = value.second;
                }
            }
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    template <bool AlwaysUpdate, typename KeyT, typename MappedT, typename std::enable_if<
              (!std::is_same<KeyT, value_type>::value &&
               !std::is_constructible<value_type, KeyT &&>::value) &&
              (!std::is_same<KeyT, init_type>::value &&
               !std::is_constructible<init_type, KeyT &&>::value) &&
              (!std::is_same<KeyT, std::piecewise_construct_t>::value) &&
               (std::is_same<KeyT, key_type>::value ||
                std::is_constructible<key_type, KeyT &&>::value) &&
               (std::is_same<MappedT, mapped_type>::value ||
                std::is_constructible<mapped_type, MappedT &&>::value)>::type * = nullptr>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> emplace_impl(KeyT && key, MappedT && value) {
        auto find_info = this->find_and_insert(key);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->slot_allocator_, slot,
                                        std::forward<KeyT>(key),
                                        std::forward<MappedT>(value));
            this->slot_size_++;
        } else {
            // The key to be inserted already exists.
            static constexpr bool isMappedType = std::is_same<MappedT, mapped_type>::value;
            if (AlwaysUpdate) {
                slot_type * slot = this->slot_at(slot_index);
                if (isMappedType) {
                    slot->value.second = std::forward<MappedT>(value);
                } else {
                    mapped_type mapped_value(std::forward<MappedT>(value));
                    slot->value.second = std::move(mapped_value);
                }
            }
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    template <bool AlwaysUpdate, typename KeyT, typename std::enable_if<
              (!std::is_same<KeyT, value_type>::value &&
               !std::is_constructible<value_type, KeyT &&>::value) &&
              (!std::is_same<KeyT, init_type>::value &&
               !std::is_constructible<init_type, KeyT &&>::value) &&
              (!std::is_same<KeyT, std::piecewise_construct_t>::value) &&
               (std::is_same<KeyT, key_type>::value ||
                std::is_constructible<key_type, KeyT &&>::value)>::type * = nullptr,
                typename ... Args>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> emplace_impl(KeyT && key, Args && ... args) {
        auto find_info = this->find_and_insert(key);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->slot_allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward_as_tuple(std::forward<KeyT>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            this->slot_size_++;
        } else {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                slot_type * slot = this->slot_at(slot_index);
                mapped_type mapped_value(std::forward<Args>(args)...);
                slot->value.second = std::move(mapped_value);
            }
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    template <bool AlwaysUpdate, typename PieceWise, typename std::enable_if<
              (!std::is_same<PieceWise, value_type>::value &&
               !std::is_constructible<value_type, PieceWise &&>::value) &&
              (!std::is_same<PieceWise, init_type>::value &&
               !std::is_constructible<init_type, PieceWise &&>::value) &&
                std::is_same<PieceWise, std::piecewise_construct_t>::value &&
              (!std::is_same<PieceWise, key_type>::value &&
               !std::is_constructible<key_type, PieceWise &&>::value)>::type * = nullptr,
                typename ... Ts1, typename ... Ts2>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> emplace_impl(PieceWise && hint,
                                           std::tuple<Ts1...> && first,
                                           std::tuple<Ts2...> && second) {
        tuple_wrapper2<key_type> key_wrapper(first);
        auto find_info = this->find_and_insert(key_wrapper.value());
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->slot_allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward<std::tuple<Ts1...>>(first),
                                        std::forward<std::tuple<Ts2...>>(second));
            this->slot_size_++;
        } else {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                tuple_wrapper2<mapped_type> mapped_wrapper(std::move(second));
                slot_type * slot = this->slot_at(slot_index);
                slot->value.second = std::move(mapped_wrapper.value());
            }
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    template <bool AlwaysUpdate, typename First, typename std::enable_if<
              (!std::is_same<First, value_type>::value &&
               !std::is_constructible<value_type, First &&>::value) &&
              (!std::is_same<First, init_type>::value &&
               !std::is_constructible<init_type, First &&>::value) &&
              (!std::is_same<First, std::piecewise_construct_t>::value) &&
              (!std::is_same<First, key_type>::value &&
               !std::is_constructible<key_type, First &&>::value)>::type * = nullptr,
                typename ... Args>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> emplace_impl(First && first, Args && ... args) {
        alignas(slot_type) unsigned char raw[sizeof(slot_type)];
        slot_type * tmp_slot = reinterpret_cast<slot_type *>(&raw);

        SlotPolicyTraits::construct(&this->slot_allocator_, tmp_slot,
                                    std::forward<First>(first),
                                    std::forward<Args>(args)...);

        auto find_info = this->find_and_insert(tmp_slot->value.first);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);

            SlotPolicyTraits::transfer(&this->slot_allocator_, slot, tmp_slot);
            this->slot_size_++;
        } else {
            // The key to be inserted already exists.
            if (AlwaysUpdate) {
                slot_type * slot = this->slot_at(slot_index);
                slot->value.second = std::move(tmp_slot->value.second);
            }
        }
        SlotPolicyTraits::destroy(&this->slot_allocator_, tmp_slot);
        return { this->iterator_at(slot_index), need_insert };
    }

    ////////////////////////////////////////////////////////////////////////////////////////////

    template <typename KeyT, typename ... Args>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> try_emplace_impl(const KeyT & key, Args && ... args) {
        auto find_info = this->find_and_insert(key);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->slot_allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward_as_tuple(key),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            this->slot_size_++;
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    template <typename KeyT, typename ... Args>
    JSTD_FORCED_INLINE
    std::pair<iterator, bool> try_emplace_impl(KeyT && key, Args && ... args) {
        auto find_info = this->find_and_insert(key);
        size_type slot_index = find_info.first;
        bool need_insert = find_info.second;
        if (need_insert) {
            // The key to be inserted is not exists.
            slot_type * slot = this->slot_at(slot_index);
            assert(slot != nullptr);
            SlotPolicyTraits::construct(&this->slot_allocator_, slot,
                                        std::piecewise_construct,
                                        std::forward_as_tuple(std::forward<KeyT>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            this->slot_size_++;
        }
        return { this->iterator_at(slot_index), need_insert };
    }

    JSTD_FORCED_INLINE
    bool ctrl_is_last_bit(size_type slot_index) {
        group_type * group = this->groups() + slot_index / kGroupWidth;
        size_type ctrl_pos = slot_index % kGroupWidth;
        std::uint32_t used_mask = group->match_used();
        std::uint32_t last_bit_pos = BitUtils::bsr32(used_mask);
        return (ctrl_pos == static_cast<size_type>(last_bit_pos));
    }

    JSTD_FORCED_INLINE
    void erase_index(size_type slot_index) {
        assert(slot_index >= 0 && slot_index < this->slot_capacity());
        bool is_last_bit = this->ctrl_is_last_bit(slot_index);
        if (likely(!is_last_bit)) {
            assert(this->slot_threshold_ > 0);
            this->slot_threshold_--;
        }
        assert(this->slot_size_ > 0);
        this->slot_size_--;
        this->destroy_slot_data(slot_index);
    }

    JSTD_FORCED_INLINE
    size_type find_and_erase(const key_type & key) {
        std::size_t hash_code = this->hash_for(key);
        size_type slot_pos = this->index_for_hash(hash_code);
        std::uint8_t ctrl_hash = this->ctrl_for_hash(hash_code);

        size_type slot_index = this->find_index(key, slot_pos, ctrl_hash);
        if (slot_index != this->slot_capacity()) {
            this->erase_index(slot_index);
        }
        return (slot_index != this->slot_capacity()) ? 1 : 0;
    }
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
