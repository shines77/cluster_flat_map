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

#ifndef JSTD_HASHMAP_FLAT_MAP_CLUSTER_HPP
#define JSTD_HASHMAP_FLAT_MAP_CLUSTER_HPP

#pragma once

#include <cstdint>

// For SSE2, SSE3, SSSE3, SSE 4.1, AVX, AVX2
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif //_MSC_VER

#include <assert.h>

#if defined(_MSC_VER)
#include <xatomic.h>
#define __COMPILER_BARRIER() \
    _Compiler_barrier()
#else
#define __COMPILER_BARRIER() \
    __asm volatile ("" : : : "memory")
#endif

namespace jstd {

static inline
__m128i _mm_setones_si128()
{
    __m128i ones {};
    ones = _mm_cmpeq_epi16(ones, ones);
    return ones;
}

static inline
__m256i _mm256_setones_si256()
{
    __m256i ones {};
    ones = _mm256_cmpeq_epi16(ones, ones);
    return ones;
}

class cluster_meta_ctrl
{
public:
    static constexpr std::uint8_t kHashMask     = 0b01111111;
    static constexpr std::uint8_t kEmptySlot    = 0b00000000 & kHashMask;
    static constexpr std::uint8_t kOverflowMask = 0b10000000;

    typedef std::uint8_t value_type;
    typedef std::uint8_t hash_type;

    cluster_meta_ctrl(hash_type value = kEmptySlot) : value_(value) {}
    ~cluster_meta_ctrl() {}

    static inline value_type hash_bits(std::size_t hash) {
        return static_cast<value_type>(hash & (std::size_t)kHashMask);
    }

    static inline value_type overflow_bits(std::size_t hash) {
        return static_cast<value_type>(hash & (std::size_t)kOverflowMask);
    }

    inline bool is_empty() const {
        value_type hash = hash_bits(this->value);
        return (hash == kEmptySlot);
    }

    inline bool is_used(std::size_t pos) const {
        value_type hash = hash_bits(this->value);
        return (hash != kEmptySlot);
    }

    inline bool is_overflow(std::size_t pos) const {
        value_type overflow = overflow_bits(this->value);
        return (overflow != 0);
    }

    inline bool is_overflow_strict(std::size_t pos) const {
        value_type overflow = overflow_bits(this->value);
        value_type hash = hash_bits(this->value);
        return ((overflow != 0) && (hash != kEmptySlot));
    }

    bool is_equals(hash_type hash) {
        return (hash == this->value);
    }

    bool is_equals64(std::size_t hash) {
        value_type hash8 = hash_bits(hash);
        return (hash8 == this->value);
    }

    inline void set_empty() {
        this->value = kEmptySlot;
    }

    inline void set_hash(hash_type hash) {
        this->value = hash;
    }

    inline void set_hash64(std::size_t hash) {
        this->value = hash_bits(hash);
    }

    inline void set_overflow() {
        assert(this->value != kEmptySlot);
        this->value &= kOverflowMask;
    }

private:
    value_type value;
};

template <typename T>
class flat_map_cluster16
{
public:
    typedef T                       ctrl_type;
    typedef typename T::value_type  value_type;
    typedef typename T::hash_type   hash_type;
    typedef ctrl_type *             pointer;
    typedef const ctrl_type *       const_pointer;
    typedef ctrl_type &             reference;
    typedef const ctrl_type &       const_reference;

    static constexpr std::uint8_t kHashMask     = ctrl_type::kHashMask;
    static constexpr std::uint8_t kEmptySlot    = ctrl_type::kEmptySlot;
    static constexpr std::uint8_t kOverflowMask = ctrl_type::kOverflowMask;

    static constexpr std::size_t kSlotCount = 16;

    flat_map_cluster16() {}
    ~flat_map_cluster16() {}

    void init() {
        if (kEmptySlot == 0b00000000) {
            __m128i zeros = _mm_setzero_si128();
            _mm_store_si128(reinterpret_cast<__m128i *>(slot), zeros);
        }
        else if (kEmptySlot == 0b11111111) {
            __m128i ones = _mm_setones_si128();
            _mm_store_si128(reinterpret_cast<__m128i *>(slot), ones);
        }
        else {
            __m128i empty_bits = _mm_set1_epi8(kEmptySlot);
            _mm_store_si128(reinterpret_cast<__m128i *>(slot), empty_bits);
        }
    }

    inline __m128i _load_data() const {
        return _mm_load_si128(reinterpret_cast<const __m128i *>(slot));
    }

    inline bool is_empty(std::size_t pos) const {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        return ctrl->is_empty();
    }

    inline bool is_used(std::size_t pos) const {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        return ctrl->is_used();
    }

    inline bool is_overflow(std::size_t pos) const {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        return ctrl->is_overflow();
    }

    inline bool is_overflow_strict(std::size_t pos) const {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        return ctrl->is_overflow_strict();
    }

    inline void set_empty(std::size_t pos) {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        ctrl->set_empty();
    }

    inline void set_hash(std::size_t pos, hash_type hash) {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        ctrl->set_hash(hash);
    }

    inline void set_hash64(std::size_t pos, std::size_t hash) {
        assert(pos < kSlotCount);
        ctrl_type * ctrl = &slot[pos];
        ctrl->set_hash64(hash);
    }

    inline void set_overflow(std::size_t pos) {
        ctrl_type * ctrl = &slot[pos];
        ctrl->set_overflow();
    }

    inline int match_empty_mask() const {
        // Latency = 6
        __m128i ctrl_bits = _load_data();
        __COMPILER_BARRIER();
        __m128i mask_bits = _mm_set1_epi8(kHashMask);
        __COMPILER_BARRIER();

        __m128i empty_bits;
        if (kEmptySlot == 0b00000000)
            empty_bits = _mm_setzero_si128();
        else if (kEmptySlot == 0b11111111)
            empty_bits = _mm_setones_si128();
        else
            empty_bits = _mm_set1_epi8(kEmptySlot);
        
        __m128i match_mask = _mm_cmpeq_epi8(_mm_and_si128(ctrl_bits, mask_bits), empty_bits);
        int mask = _mm_movemask_epi8(match_mask);
        return mask;
    }

    inline int match_empty() const {
        int mask = match_empty_mask();
        return (mask & 0x7FFF);
    }

    inline int match_hash_mask(hash_type hash) const {
        // Latency = 6
        __m128i ctrl_bits  = _load_data();
        __COMPILER_BARRIER();
        __m128i mask_bits  = _mm_set1_epi8(kHashMask);
        __COMPILER_BARRIER();
        __m128i hash_bits  = _mm_set1_epi8(hash);
        __m128i match_mask = _mm_cmpeq_epi8(_mm_and_si128(ctrl_bits, mask_bits), hash_bits);
        int mask = _mm_movemask_epi8(match_mask);
        return mask;
    }

    inline int match_hash() const {
        int mask = match_hash_mask();
        return (mask & 0x7FFF);
    }

private:
    alignas(16) ctrl_type slot[kSlotCount];
};

} // namespace jstd

#endif // JSTD_HASHMAP_FLAT_MAP_CLUSTER_HPP
