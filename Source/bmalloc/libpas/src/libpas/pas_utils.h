/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_UTILS_H
#define PAS_UTILS_H

#include "pas_config.h"

/* These need to be included first. */
#include <pthread.h>
#if defined(__has_include)
#if __has_include(<System/pthread_machdep.h>)
#include <System/pthread_machdep.h>
#define PAS_HAVE_PTHREAD_MACHDEP_H 1
#else
#define PAS_HAVE_PTHREAD_MACHDEP_H 0
#endif
#else
#define PAS_HAVE_PTHREAD_MACHDEP_H 0
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pas_utils_prefix.h"

#define pas_zero_memory bzero

#define PAS_BEGIN_EXTERN_C __PAS_BEGIN_EXTERN_C
#define PAS_END_EXTERN_C __PAS_END_EXTERN_C

PAS_BEGIN_EXTERN_C;

#define PAS_ALWAYS_INLINE_BUT_NOT_INLINE __PAS_ALWAYS_INLINE_BUT_NOT_INLINE
#define PAS_ALWAYS_INLINE __PAS_ALWAYS_INLINE
#define PAS_NEVER_INLINE __PAS_NEVER_INLINE
#define PAS_NO_RETURN __PAS_NO_RETURN
#define PAS_USED __attribute__((used))

#define PAS_COLD /* FIXME: Need some way of triggering cold CC. */

#define PAS_API __PAS_API
#define PAS_BAPI __PAS_BAPI

#define PAS_ALIGNED(amount) __attribute__((aligned(amount)))

#define PAS_UNUSED __attribute__((unused))

#define PAS_OFFSETOF(type, field) __PAS_OFFSETOF(type, field)

#define PAS_ABORT_FUNC_DECL(ID) __PAS_ABORT_FUNC_DECL(ID)

#define PAS_UNUSED_PARAM(variable) __PAS_UNUSED_PARAM(variable)

#define PAS_ARM64 __PAS_ARM64
#define PAS_ARM32 __PAS_ARM32

#define PAS_ARM __PAS_ARM

/* NOTE: panic format string must have \n at the end. */
PAS_API PAS_NO_RETURN void pas_panic(const char* format, ...);

#define pas_set_deallocation_did_fail_callback __pas_set_deallocation_did_fail_callback
#define pas_set_reallocation_did_fail_callback __pas_set_reallocation_did_fail_callback

PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_deallocation_did_fail(const char* reason,
                                                                      uintptr_t begin);
PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_reallocation_did_fail(const char* reason,
                                                                      void* source_heap,
                                                                      void* target_heap,
                                                                      void* old_ptr,
                                                                      size_t old_size,
                                                                      size_t new_count);

PAS_API PAS_NO_RETURN void pas_assertion_failed(const char* filename, int line, const char* function, const char* expression);

#define PAS_LIKELY(x) __PAS_LIKELY(x)
#define PAS_UNLIKELY(x) __PAS_UNLIKELY(x)

#define PAS_ASSERT(exp) \
    do { \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

#define PAS_TESTING_ASSERT(exp) \
    do { \
        if (!PAS_ENABLE_TESTING) \
            break; \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

static inline bool pas_is_power_of_2(uintptr_t value)
{
    return value && !(value & (value - 1));
}

#define PAS_ROUND_DOWN_TO_POWER_OF_2(size, alignment) ((size) & -(alignment))

static inline uintptr_t pas_round_down_to_power_of_2(uintptr_t size, uintptr_t alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    return PAS_ROUND_DOWN_TO_POWER_OF_2(size, alignment);
}

static inline uintptr_t pas_round_down(uintptr_t size, uintptr_t alignment)
{
    return size / alignment * alignment;
}

#define PAS_ROUND_UP_TO_POWER_OF_2(size, alignment) \
    __PAS_ROUND_UP_TO_POWER_OF_2((size), (alignment))

static inline uintptr_t pas_round_up_to_power_of_2(uintptr_t size, uintptr_t alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    return __pas_round_up_to_power_of_2(size, alignment);
}

static inline uintptr_t pas_round_up(uintptr_t size, uintptr_t alignment)
{
    return (size + alignment - 1) / alignment * alignment;
}

static inline uintptr_t pas_modulo_power_of_2(uintptr_t value, uintptr_t alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    return value & (alignment - 1);
}

static inline bool pas_is_aligned(uintptr_t value, uintptr_t alignment)
{
    return !pas_modulo_power_of_2(value, alignment);
}

static inline unsigned pas_reverse(unsigned value)
{
#if PAS_ARM64
    unsigned result;
    asm ("rbit %w0, %w1"
         : "=r"(result)
         : "r"(value));
    return result;
#else
    value = ((value & 0xaaaaaaaa) >> 1) | ((value & 0x55555555) << 1);
    value = ((value & 0xcccccccc) >> 2) | ((value & 0x33333333) << 2);
    value = ((value & 0xf0f0f0f0) >> 4) | ((value & 0x0f0f0f0f) << 4);
    value = ((value & 0xff00ff00) >> 8) | ((value & 0x00ff00ff) << 8);
    return (value >> 16) | (value << 16);
#endif
}

static inline uint64_t pas_reverse64(uint64_t value)
{
#if PAS_ARM64
    uint64_t result;
    asm ("rbit %0, %1"
         : "=r"(result)
         : "r"(value));
    return result;
#else
    return ((uint64_t)pas_reverse((unsigned)value) << (uint64_t)32)
        | (uint64_t)pas_reverse((unsigned)(value >> (uint64_t)32));
#endif
}

static inline uint64_t pas_make_mask64(uint64_t num_bits)
{
    PAS_TESTING_ASSERT(num_bits <= 64);
    if (num_bits == 64)
        return (uint64_t)(int64_t)-1;
    return ((uint64_t)1 << num_bits) - 1;
}

static inline bool pas_compare_and_swap_uintptr_weak(uintptr_t* ptr, uintptr_t old_value, uintptr_t new_value)
{
    return __c11_atomic_compare_exchange_weak((_Atomic uintptr_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline uintptr_t pas_compare_and_swap_uintptr_strong(uintptr_t* ptr, uintptr_t old_value, uintptr_t new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic uintptr_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
}

static inline bool pas_compare_and_swap_bool_weak(bool* ptr, bool old_value, bool new_value)
{
    return __c11_atomic_compare_exchange_weak((_Atomic bool*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline bool pas_compare_and_swap_bool_strong(bool* ptr, bool old_value, bool new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic bool*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
}

static inline bool pas_compare_and_swap_uint16_weak(uint16_t* ptr, uint16_t old_value, uint16_t new_value)
{
    return __c11_atomic_compare_exchange_weak((_Atomic uint16_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline bool pas_compare_and_swap_uint32_weak(uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
    return __c11_atomic_compare_exchange_weak((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline uint32_t pas_compare_and_swap_uint32_strong(uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
}

static inline bool pas_compare_and_swap_uint64_weak(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
    return __c11_atomic_compare_exchange_weak((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline uint64_t pas_compare_and_swap_uint64_strong(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
}

static inline bool pas_compare_and_swap_ptr_weak(void* ptr, const void* old_value, const void* new_value)
{
    return __c11_atomic_compare_exchange_weak((const void* _Atomic *)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline void* pas_compare_and_swap_ptr_strong(void* ptr, const void* old_value, const void* new_value)
{
    __c11_atomic_compare_exchange_strong((const void* _Atomic *)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return (void*)(uintptr_t)old_value;
}

#define pas_compiler_fence __pas_compiler_fence
#define pas_fence __pas_fence
#define pas_depend __pas_depend
#define pas_depend_cpu_only __pas_depend_cpu_only

/* Block loads or stores after this from getting hoisted above some prior load. */
static PAS_ALWAYS_INLINE void pas_fence_after_load(void)
{
    if (PAS_ARM)
        pas_fence();
    else
        pas_compiler_fence();
}

static PAS_ALWAYS_INLINE void pas_store_store_fence(void)
{
    if (PAS_ARM)
        asm volatile ("dmb ishst" : : : "memory");
    else
        pas_compiler_fence();
}

static PAS_ALWAYS_INLINE uintptr_t pas_opaque(uintptr_t value)
{
    asm volatile ("" : "+r"(value) : : "memory");
    return value;
}

struct pas_pair;
typedef struct pas_pair pas_pair;

struct PAS_ALIGNED(sizeof(uintptr_t) * 2) pas_pair {
    uintptr_t low;
    uintptr_t high;
};

static inline pas_pair pas_pair_create(uintptr_t low, uintptr_t high)
{
    pas_pair result;
    result.low = low;
    result.high = high;
    return result;
}

static inline bool pas_compare_and_swap_pair_weak(void* raw_ptr,
                                                  pas_pair old_value, pas_pair new_value)
{
    return __c11_atomic_compare_exchange_weak((_Atomic pas_pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline pas_pair pas_compare_and_swap_pair_strong(void* raw_ptr,
                                                        pas_pair old_value, pas_pair new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic pas_pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
}

static inline pas_pair pas_atomic_load_pair(void* raw_ptr)
{
    return __c11_atomic_load((_Atomic pas_pair*)raw_ptr, __ATOMIC_RELAXED);
}

static inline void pas_atomic_store_pair(void* raw_ptr, pas_pair value)
{
    __c11_atomic_store((_Atomic pas_pair*)raw_ptr, value, __ATOMIC_SEQ_CST);
}

#define PAS_MIN(a, b) ({ \
        typeof (a) _tmp_a = (a); \
        typeof (b) _tmp_b = (b); \
        _tmp_a < _tmp_b ? _tmp_a : _tmp_b; \
    })
#define PAS_MAX(a, b) ({ \
        typeof (a) _tmp_a = (a); \
        typeof (b) _tmp_b = (b); \
        _tmp_a > _tmp_b ? _tmp_a : _tmp_b; \
    })
#define PAS_CLIP(x, min, max) PAS_MIN(PAS_MAX(x, min), max)

#define PAS_MIN_CONST(a, b) ((a) > (b) ? (b) : (a))
#define PAS_MAX_CONST(a, b) ((a) > (b) ? (a) : (b))

static inline unsigned pas_hash32(unsigned a)
{
    /* Source: http://burtleburtle.net/bob/hash/integer.html */
    a = a ^ (a >> 4);
    a = (a ^ 0xdeadbeef) + (a << 5);
    a = a ^ (a >> 11);
    return a;
}

static inline unsigned pas_hash64(uint64_t value)
{
    /* Just mix the high and low 32-bit hashes.  I think that this works well for pointers in
       practice. */
    return pas_hash32((unsigned)value) ^ pas_hash32((unsigned)(value >> 32));
}

static inline unsigned pas_hash_intptr(uintptr_t value)
{
    if (sizeof(uintptr_t) == 8)
        return pas_hash64((uint64_t)value);
    return pas_hash32((unsigned)value);
}

static inline unsigned pas_hash_ptr(const void* ptr)
{
    return pas_hash_intptr((uintptr_t)ptr);
}

/* Undefined for value == 0. */
static inline unsigned pas_log2(uintptr_t value)
{
    return (sizeof(uintptr_t) * 8 - 1) - __builtin_clzl(value);
}

/* Undefined for value <= 1. */
static inline unsigned pas_log2_rounded_up(uintptr_t value)
{
    return (sizeof(uintptr_t) * 8 - 1) - (__builtin_clzl(value - 1) - 1);
}

static inline unsigned pas_log2_rounded_up_safe(uintptr_t value)
{
    if (value <= 1)
        return 0;
    return pas_log2_rounded_up(value);
}

#define PAS_SWAP(left, right) do { \
        typeof (left) _swap_tmp = left; \
        left = right; \
        right = _swap_tmp; \
    } while (0)

static inline bool pas_non_empty_ranges_overlap(uintptr_t left_min, uintptr_t left_max, 
                                                uintptr_t right_min, uintptr_t right_max)
{
    PAS_ASSERT(left_min < left_max);
    PAS_ASSERT(right_min < right_max);

    return left_max > right_min && right_max > left_min;
}

/* Pass ranges with the min being inclusive and the max being exclusive. For example, this should
 * return false:
 *
 *     pas_ranges_overlap(0, 8, 8, 16) */
static inline bool pas_ranges_overlap(uintptr_t left_min, uintptr_t left_max,
                                      uintptr_t right_min, uintptr_t right_max)
{
    PAS_ASSERT(left_min <= left_max);
    PAS_ASSERT(right_min <= right_max);
    
    // Empty ranges interfere with nothing.
    if (left_min == left_max)
        return false;
    if (right_min == right_max)
        return false;

    return pas_non_empty_ranges_overlap(left_min, left_max, right_min, right_max);
}

static inline uint32_t pas_xorshift32(uint32_t state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

#define PAS_NUM_PTR_BITS (sizeof(void*) * 8)

static inline unsigned pas_large_object_hash(uintptr_t key)
{
    return pas_hash_intptr(key);
}

#define PAS_INFINITY (1. / 0.)

#define PAS_SIZE_MAX ((size_t)(intptr_t)-1)

#define PAS_IS_DIVISIBLE_BY_MAGIC_CONSTANT(divisor) \
    (1 + UINT64_C(0xffffffffffffffff) / (unsigned)(divisor))

static inline bool pas_is_divisible_by(unsigned value, uint64_t magic_constant)
{
    return value * magic_constant <= magic_constant - 1;
}

#define PAS_CONCAT(a, b) a ## b

PAS_END_EXTERN_C;

#endif /* PAS_UTILS_H */
