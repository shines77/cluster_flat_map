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

#ifndef JSTD_HASHMAP_FLAT_MAP_ITERATOR_HPP
#define JSTD_HASHMAP_FLAT_MAP_ITERATOR_HPP

#pragma once

#include <xutility>

namespace jstd {

template <typename HashMap, typename T, bool IsIndirectKV /* = false */>
class flat_map_iterator {
public:
    using iterator_category = std::forward_iterator_tag;

    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using hashmap_type = HashMap;
    using ctrl_type = typename HashMap::ctrl_type;
    using slot_type = typename HashMap::slot_type;

    using mutable_value_type = typename std::remove_const<value_type>::type;
    using const_value_type = typename std::add_const<mutable_value_type>::type;

    using opp_value_type = typename std::conditional<std::is_const<value_type>::value,
                                                     mutable_value_type,
                                                     const_value_type>::type;
    using opp_flat_map_iterator = flat_map_iterator<HashMap, opp_value_type, IsIndirectKV>;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    const hashmap_type * owner_;
    size_type            index_;

public:
    flat_map_iterator() noexcept : owner_(nullptr), index_(0) {
    }
    flat_map_iterator(hashmap_type * owner, size_type index) noexcept
        : owner_(const_cast<const hashmap_type *>(owner)), index_(index) {
    }
    flat_map_iterator(const hashmap_type * owner, size_type index) noexcept
        : owner_(owner), index_(index) {
    }
    flat_map_iterator(slot_type * slot) noexcept
        : owner_(nullptr), index_(0) {
    }
    flat_map_iterator(const slot_type * slot) noexcept
        : owner_(nullptr), index_(0) {
    }
    flat_map_iterator(const flat_map_iterator & src) noexcept
        : owner_(src.owner_), index_(src.index_) {
    }
    flat_map_iterator(const opp_flat_map_iterator & src) noexcept
        : owner_(src.owner()), index_(src.index()) {
    }

    flat_map_iterator & operator = (const flat_map_iterator & rhs) noexcept {
        this->owner_ = rhs.owner_;
        this->index_ = rhs.index_;
        return *this;
    }

    flat_map_iterator & operator = (const opp_flat_map_iterator & rhs) noexcept {
        this->owner_ = rhs.owner();
        this->index_ = rhs.index();
        return *this;
    }

    friend bool operator == (const flat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.index_ == rhs.index_) && (lhs.owner_ == rhs.owner_);
    }

    friend bool operator != (const flat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.index_ != rhs.index_) || (lhs.owner_ != rhs.owner_);
    }

    friend bool operator == (const flat_map_iterator & lhs, const opp_flat_map_iterator & rhs) noexcept {
        return (lhs.index() == rhs.index()) && (lhs.owner() == rhs.owner());
    }

    friend bool operator != (const flat_map_iterator & lhs, const opp_flat_map_iterator & rhs) noexcept {
        return (lhs.index() != rhs.index()) || (lhs.owner() != rhs.owner());
    }

    friend bool operator == (const opp_flat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.index() == rhs.index()) && (lhs.owner() == rhs.owner());
    }

    friend bool operator != (const opp_flat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.index() != rhs.index()) || (lhs.owner() != rhs.owner());
    }

    flat_map_iterator & operator ++ () {
        const ctrl_type * ctrl = this->owner_->ctrl_at(this->index_);
        size_type max_index = this->owner_->slot_capacity();
        do {
            ++(this->index_);
            ++ctrl;
        } while (ctrl->is_empty_slot() && (this->index_ < max_index));
        return *this;
    }

    flat_map_iterator operator ++ (int) {
        flat_map_iterator copy(*this);
        ++*this;
        return copy;
    }

    flat_map_iterator & operator -- () {
        const ctrl_type * ctrl = this->owner_->ctrl_at(this->index_);
        while (this->index_ != 0) {
            --(this->index_);
            --ctrl;
            if (!ctrl->is_empty_slot())
                break;
        }
        return *this;
    }

    flat_map_iterator operator -- (int) {
        flat_map_iterator copy(*this);
        --*this;
        return copy;
    }

    reference operator * () {
        const slot_type * _slot = this->slot();
        return const_cast<slot_type *>(_slot)->value;
    }

    const_reference operator * () const {
        const slot_type * _slot = this->slot();
        return const_cast<slot_type *>(_slot)->value;
    }

    pointer operator -> () {
        const slot_type * _slot = this->slot();
        return std::addressof(const_cast<slot_type *>(_slot)->value);
    }

    const_pointer operator -> () const {
        const slot_type * _slot = this->slot();
        return std::addressof(const_cast<slot_type *>(_slot)->value);
    }
#if 0
    operator flat_map_iterator<HashMap, const mutable_value_type, IsIndirectKV>() const noexcept {
        return { this->owner_, this->index_ };
    }
#endif
    hashmap_type * owner() {
        return this->owner_;
    }

    const hashmap_type * owner() const {
        return this->owner_;
    }

    size_type index() {
        return this->index_;
    }

    size_type index() const {
        return this->index_;
    }

    ctrl_type * ctrl() {
        ctrl_type * _ctrl = const_cast<hashmap_type *>(this->owner_)->ctrl_at(this->index);
        return _ctrl;
    }

    const ctrl_type * ctrl() const {
        const ctrl_type * _ctrl = this->owner_->ctrl_at(this->index);
        return _ctrl;
    }

    slot_type * slot() {
        const slot_type * _slot = this->owner_->slot_at(this->index_);
        return const_cast<hashmap_type *>(_slot);
    }

    const slot_type * slot() const {
        const slot_type * _slot = this->owner_->slot_at(this->index_);
        return _slot;
    }
};

template <typename HashMap, typename T>
class flat_map_iterator<HashMap, T, true> {
public:
    using iterator_category = std::forward_iterator_tag;

    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using hashmap_type = HashMap;
    using ctrl_type = typename HashMap::ctrl_type;
    using slot_type = typename HashMap::slot_type;

    using mutable_value_type = typename std::remove_const<value_type>::type;
    using const_value_type = typename std::add_const<mutable_value_type>::type;

    using opp_value_type = typename std::conditional<std::is_const<value_type>::value,
                                                     mutable_value_type,
                                                     const_value_type>::type;
    using opp_flat_map_iterator = flat_map_iterator<HashMap, opp_value_type, true>;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    const slot_type * slot_;

public:
    flat_map_iterator() noexcept : slot_(nullptr) {
    }
    flat_map_iterator(slot_type * slot) noexcept
        : slot_(const_cast<const slot_type *>(slot)) {
    }
    flat_map_iterator(const slot_type * slot) noexcept
        : slot_(slot) {
    }
    flat_map_iterator(hashmap_type * owner, size_type index) noexcept
        : slot_(owner->slot_at(index)) {
    }
    flat_map_iterator(const hashmap_type * owner, size_type index) noexcept
        : slot_(owner->slot_at(index)) {
    }
    flat_map_iterator(const flat_map_iterator & src) noexcept
        : slot_(src.slot_) {
    }
    flat_map_iterator(const opp_flat_map_iterator & src) noexcept
        : slot_(src.slot()) {
    }

    flat_map_iterator & operator = (const flat_map_iterator & rhs) noexcept {
        this->slot_ = rhs.slot_;
        return *this;
    }

    flat_map_iterator & operator = (const opp_flat_map_iterator & rhs) noexcept {
        this->slot_ = rhs.slot();
        return *this;
    }

    friend bool operator == (const flat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.slot_ == rhs.slot_);
    }

    friend bool operator != (const flat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.slot_ != rhs.slot_);
    }

    friend bool operator == (const flat_map_iterator & lhs, const opp_flat_map_iterator & rhs) noexcept {
        return (lhs.slot_ == rhs.slot());
    }

    friend bool operator != (const flat_map_iterator & lhs, const opp_flat_map_iterator & rhs) noexcept {
        return (lhs.slot_ != rhs.slot());
    }

    friend bool operator == (const opp_fflat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.slot_ == rhs.slot());
    }

    friend bool operator != (const opp_fflat_map_iterator & lhs, const flat_map_iterator & rhs) noexcept {
        return (lhs.slot_ != rhs.slot());
    }

    flat_map_iterator & operator ++ () {
        ++(this->slot_);
        return *this;
    }

    flat_map_iterator operator ++ (int) {
        flat_map_iterator copy(*this);
        ++*this;
        return copy;
    }

    flat_map_iterator & operator -- () {
        --(this->slot_);
        return *this;
    }

    flat_map_iterator operator -- (int) {
        flat_map_iterator copy(*this);
        --*this;
        return copy;
    }

    reference operator * () {
        return const_cast<slot_type *>(this->slot_)->value;
    }

    const_reference operator * () const {
        return const_cast<slot_type *>(this->slot_)->value;
    }

    pointer operator -> () {
        return std::addressof(const_cast<slot_type *>(this->slot_)->value);
    }

    const_pointer operator -> () const {
        return std::addressof(const_cast<slot_type *>(this->slot_)->value);
    }

    size_type index() const {
        return 0;
    }

    slot_type * slot() {
        return const_cast<slot_type *>(this->slot_);
    }

    const slot_type * slot() const {
        return this->slot_;
    }
};

} // namespace jstd

#endif // JSTD_HASHMAP_FLAT_MAP_ITERATOR_HPP
