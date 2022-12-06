/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#if PAS_PLATFORM(PLAYSTATION) && !defined(alignof)
#define PAS_ALIGNOF(type) _Alignof(type)
#elif defined(__cplusplus)
#define PAS_ALIGNOF(type) alignof(type)
#else
#define PAS_ALIGNOF(type) _Alignof(type)
#endif

#define PAS_FORMAT_PRINTF(fmt, args) __attribute__((format(__printf__, fmt, args)))

#define PAS_UNUSED __attribute__((unused))

#define PAS_OFFSETOF(type, field) __PAS_OFFSETOF(type, field)

#define PAS_ABORT_FUNC_DECL(ID) __PAS_ABORT_FUNC_DECL(ID)

#define PAS_UNUSED_PARAM(variable) __PAS_UNUSED_PARAM(variable)

#define PAS_ARM64 __PAS_ARM64
#define PAS_ARM64E __PAS_ARM64E
#define PAS_ARM32 __PAS_ARM32

#define PAS_ARM __PAS_ARM

#define PAS_X86 __PAS_X86
#define PAS_X86_64 __PAS_X86_64

#define PAS_RISCV __PAS_RISCV

#define PAS_PP_THIRD_ARG(a, b, c, ...) c
#define PAS_VA_OPT_SUPPORTED_I(...) PAS_PP_THIRD_ARG(__VA_OPT__(,),true,false,)
#define PAS_VA_OPT_SUPPORTED PAS_VA_OPT_SUPPORTED_I(?)


#if PAS_ARM64 && !PAS_ARM64E && !PAS_OS(MAC) && !defined(__ARM_FEATURE_ATOMICS)
/* Just using LL/SC does not guarantee that the ordering of accesses around the loop. For example,
 *
 *    access(A)
 *  0:
 *    LL (ldaxr)
 *    ...
 *    SC (stlxr)
 *    cond-branch 0
 *    access(B)
 *
 * In the above code case, the ordering A -> LL -> SC -> B is not guaranteed and it can be
 * LL -> A -> B -> SC or LL -> B -> A -> SC: memory access may happen in the middle of RMW atomics.
 * This breaks pas_versioned_field's assumption where they are ordered as A -> LL -> SC -> B.
 *
 * https://stackoverflow.com/questions/35217406/partial-reordering-of-c11-atomics-on-aarch64
 * https://stackoverflow.com/questions/21535058/arm64-ldxr-stxr-vs-ldaxr-stlxr
 * http://lists.infradead.org/pipermail/linux-arm-kernel/2014-February/229588.html
 *
 * Another example is that the following can happen if we use CAS loop without barrier.
 *
 *     == thread A ==
 *     *a = 1;
 *     spin_lock(&lock);
 *     *b = 1;
 *
 *     == thread B ==
 *     b_value = atomic_get(&b);
 *     a_value = atomic_get(&a);
 *     assert(a_value || !b_value); // can fail
 *
 * https://github.com/zephyrproject-rtos/zephyr/issues/32133
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65697
 *
 * To guarantee A -> (atomic) -> B ordering, we insert barrier (dmb ish) just after the loop.
 *
 *    access(A)
 *  0:
 *    LL (ldxr)
 *    ...
 *    SC (stlxr)
 *    cond-branch 0
 *    dmb ish
 *    access(B)
 *
 * `dmb ish` ensures B is done after (atomic) region. And this barrier also ensures that A cannot happen after
 * For this CAS emulation loop, we do not need to have acquire, so we can use ldxr.
 * (atomic) region. SC ensures A does not happen after SC. But still, A and LL can be reordered.
 * If A is storing to the same location X, then it will be detected due to ldxr's exclusiveness.
 *
 *      data = LL(X)
 *      store(X, 42) // Reordered here
 *      => SC will fail.
 *
 * If A is storing to the different location, then we have no way to observe the difference.
 *
 *     data = LL(X)
 *     store(A, 42) // Reordered here. But there is no way to know whether this access happens before or after LL.
 *
 * To achieve that, when we are not building with ARM LSE Atomics, we use inline assembly instead of
 * clang's builtin (e.g. __c11_atomic_compare_exchange_weak).
 */
#define PAS_COMPILER_ARM64_ATOMICS_LL_SC 1
#endif

#ifdef __cplusplus
#define PAS_TYPEOF(a) decltype (a)
#else
#define PAS_TYPEOF(a) typeof (a)
#endif

static PAS_ALWAYS_INLINE void pas_zero_memory(void* memory, size_t size)
{
    memset(memory, 0, size);
}

/* NOTE: panic format string must have \n at the end. */
PAS_API PAS_NO_RETURN void pas_panic(const char* format, ...) PAS_FORMAT_PRINTF(1, 2);

PAS_API PAS_NEVER_INLINE PAS_NO_RETURN void pas_panic_on_out_of_memory_error(void);

#define pas_set_deallocation_did_fail_callback __pas_set_deallocation_did_fail_callback
#define pas_set_reallocation_did_fail_callback __pas_set_reallocation_did_fail_callback

PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_deallocation_did_fail(const char* reason,
                                                                      uintptr_t begin);
PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_reallocation_did_fail(const char* reason,
                                                                      void* source_heap,
                                                                      void* target_heap,
                                                                      void* old_ptr,
                                                                      size_t old_size,
                                                                      size_t new_size);

#if PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED

#define PAS_VA_COUNT6(x, ...) + 1
#define PAS_VA_COUNT5(x, ...) __VA_OPT__(PAS_VA_COUNT6(__VA_ARGS__)) + 1
#define PAS_VA_COUNT4(x, ...) __VA_OPT__(PAS_VA_COUNT5(__VA_ARGS__)) + 1
#define PAS_VA_COUNT3(x, ...) __VA_OPT__(PAS_VA_COUNT4(__VA_ARGS__)) + 1
#define PAS_VA_COUNT2(x, ...) __VA_OPT__(PAS_VA_COUNT3(__VA_ARGS__)) + 1
#define PAS_VA_COUNT(x, ...) __VA_OPT__(PAS_VA_COUNT2(__VA_ARGS__)) + 1
#define PAS_VA_NUM_ARGS(...) (__VA_OPT__(PAS_VA_COUNT(__VA_ARGS__)) + 0)

static PAS_ALWAYS_INLINE PAS_NO_RETURN void pas_assertion_failed(
    const char* filename, int line, const char* function, const char* expression)
{
    asm volatile("" : "=r"(filename), "=r"(line), "=r"(function), "=r"(expression));
    __builtin_unreachable();
}

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl1(uint64_t reason, uint64_t);
PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl2(uint64_t reason, uint64_t, uint64_t);
PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl3(uint64_t reason, uint64_t, uint64_t, uint64_t);
PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl4(uint64_t reason, uint64_t, uint64_t, uint64_t, uint64_t);
PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl5(uint64_t reason, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl6(uint64_t reason, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#else /* PAS_OS(DARWIN) */

#if PAS_ENABLE_TESTING
PAS_API PAS_NO_RETURN void pas_assertion_failed(const char* filename, int line, const char* function, const char* expression)
{
    pas_panic("%s:%d: %s: assertion %s failed.\n", filename, line, function, expression);
}
#else /* PAS_ENABLE_TESTING -> so !PAS_ENABLE_TESTING */
static PAS_ALWAYS_INLINE PAS_NO_RETURN void pas_assertion_failed(
    const char* filename, int line, const char* function, const char* expression)
{
#if PAS_COMPILER(GCC) || PAS_COMPILER(CLANG)
    // Force each assertion crash site to be unique.
    asm volatile("" : "=r"(filename), "=r"(line), "=r"(function), "=r"(expression));
#endif
    __builtin_trap();
}
#endif /* PAS_ENABLE_TESTING -> so end of !PAS_ENABLE_TESTING */
#endif /* PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED */

PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_assertion_failed_no_inline(const char* filename, int line, const char* function, const char* expression);
PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_assertion_failed_no_inline_with_extra_detail(const char* filename, int line, const char* function, const char* expression, uint64_t extra);

PAS_IGNORE_WARNINGS_BEGIN("missing-noreturn")

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer(
    const char* filename, int line, const char* function, const char* expression)
{
    pas_assertion_failed(filename, line, function, expression);
}

#if PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED

/* FIXME: Consider whether it makes sense to capture the filename, function, and expression
   in crash data or not. Need to measure if there's a performance impact.
   FIXME: Also consider converting PAS_ASSERT_WITH_DETAIL and PAS_ASSERT_WITH_EXTRA_DETAIL
   to just use the variadic PAS_ASSERT. We currently leave these and the PAS_ASSERT with
   no extra args unchanged to make sure we don't perturb performance (until we can measure
   and confirm that using the variadic form won't impact performance).
*/

#define PAS_UNUSED_ASSERTION_FAILED_ARGS(filename, line, function, expression) do { \
        PAS_UNUSED_PARAM(filename); \
        PAS_UNUSED_PARAM(line); \
        PAS_UNUSED_PARAM(function); \
        PAS_UNUSED_PARAM(expression); \
    } while (0)

PAS_NEVER_INLINE void pas_report_assertion_failed(
    const char* filename, int line, const char* function, const char* expression);

#if PAS_ENABLE_TESTING
#define PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression) \
    pas_report_assertion_failed(filename, line, function, expression)
#else
#define PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression) \
    PAS_UNUSED_ASSERTION_FAILED_ARGS(filename, line, function, expression)
#endif

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer1(
    const char* filename, int line, const char* function, const char* expression, uint64_t misc1)
{
    PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression);
    pas_crash_with_info_impl1((uint64_t)line, misc1);
}

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer2(
    const char* filename, int line, const char* function, const char* expression, uint64_t misc1, uint64_t misc2)
{
    PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression);
    pas_crash_with_info_impl2((uint64_t)line, misc1, misc2);
}

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer3(
    const char* filename, int line, const char* function, const char* expression, uint64_t misc1, uint64_t misc2, uint64_t misc3)
{
    PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression);
    pas_crash_with_info_impl3((uint64_t)line, misc1, misc2, misc3);
}

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer4(
    const char* filename, int line, const char* function, const char* expression, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4)
{
    PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression);
    pas_crash_with_info_impl4((uint64_t)line, misc1, misc2, misc3, misc4);
}

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer5(
    const char* filename, int line, const char* function, const char* expression, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5)
{
    PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression);
    pas_crash_with_info_impl5((uint64_t)line, misc1, misc2, misc3, misc4, misc5);
}

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer6(
    const char* filename, int line, const char* function, const char* expression, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5, uint64_t misc6)
{
    PAS_REPORT_ASSERTION_FAILED(filename, line, function, expression);
    pas_crash_with_info_impl6((uint64_t)line, misc1, misc2, misc3, misc4, misc5, misc6);
}

PAS_IGNORE_WARNINGS_END

/* The count argument will always be computed with PAS_VA_NUM_ARGS in the client.
   Hence, it is always a constant, and the following cascade of if statements will
   reduce to a single statement for the appropriate number of __VA_ARGS__.
*/
#define PAS_ASSERT_FAIL(count, file, line, function, exp, ...) do { \
        if (!count) \
            pas_assertion_failed_noreturn_silencer(file, line, function, exp); \
        __VA_OPT__( \
        else \
            PAS_ASSERT_FAIL1(count, file, line, function, exp, __VA_ARGS__); \
        ) \
    } while (0)

#define PAS_ASSERT_FAIL1(count, file, line, function, exp, misc1, ...) do { \
        if (count == 1) \
            pas_assertion_failed_noreturn_silencer1(file, line, function, exp, (uint64_t)misc1); \
        __VA_OPT__( \
        else \
            PAS_ASSERT_FAIL2(count, file, line, function, exp, misc1, __VA_ARGS__); \
        ) \
    } while (0)

#define PAS_ASSERT_FAIL2(count, file, line, function, exp, misc1, misc2, ...) do { \
        if (count == 2) \
            pas_assertion_failed_noreturn_silencer2(file, line, function, exp, (uint64_t)misc1, (uint64_t)misc2); \
        __VA_OPT__( \
        else \
            PAS_ASSERT_FAIL3(count, file, line, function, exp, misc1, misc2, __VA_ARGS__); \
        ) \
    } while (0)

#define PAS_ASSERT_FAIL3(count, file, line, function, exp, misc1, misc2, misc3, ...) do { \
        if (count == 3) \
            pas_assertion_failed_noreturn_silencer3(file, line, function, exp, (uint64_t)misc1, (uint64_t)misc2, (uint64_t)misc3); \
        __VA_OPT__( \
        else \
            PAS_ASSERT_FAIL4(count, file, line, function, exp, misc1, misc2, misc3, __VA_ARGS__); \
        ) \
    } while (0)

#define PAS_ASSERT_FAIL4(count, file, line, function, exp, misc1, misc2, misc3, misc4, ...) do { \
        if (count == 4) \
            pas_assertion_failed_noreturn_silencer4(file, line, function, exp, (uint64_t)misc1, (uint64_t)misc2, (uint64_t)misc3, (uint64_t)misc4); \
        __VA_OPT__( \
        else \
            PAS_ASSERT_FAIL5(count, file, line, function, exp, misc1, misc2, misc3, misc4, __VA_ARGS__); \
        ) \
    } while (0)

#define PAS_ASSERT_FAIL5(count, file, line, function, exp, misc1, misc2, misc3, misc4, misc5, ...) do { \
        if (count == 5) \
            pas_assertion_failed_noreturn_silencer5(file, line, function, exp, (uint64_t)misc1, (uint64_t)misc2, (uint64_t)misc3, (uint64_t)misc4, (uint64_t)misc5); \
        __VA_OPT__( \
        else \
            PAS_ASSERT_FAIL6(count, file, line, function, exp, misc1, misc2, misc3, misc4, misc5, __VA_ARGS__); \
        ) \
    } while (0)

#define PAS_ASSERT_FAIL6(count, file, line, function, exp, misc1, misc2, misc3, misc4, misc5, misc6, ...) \
        pas_assertion_failed_noreturn_silencer6(file, line, function, exp, (uint64_t)misc1, (uint64_t)misc2, (uint64_t)misc3, (uint64_t)misc4, (uint64_t)misc5, (uint64_t)misc6)

#endif /* PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED */

#define PAS_LIKELY(x) __PAS_LIKELY(x)
#define PAS_UNLIKELY(x) __PAS_UNLIKELY(x)

#if PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED

#define PAS_ASSERT_IF(cond, exp, ...) \
    do { \
        if (!cond) \
            break; \
        if (PAS_UNLIKELY(!(exp))) \
            PAS_ASSERT_FAIL(PAS_VA_NUM_ARGS(__VA_ARGS__), __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp __VA_OPT__(,) __VA_ARGS__); \
    } while (0)

#define PAS_ASSERT(exp, ...) \
    PAS_ASSERT_IF(PAS_ENABLE_ASSERT, exp __VA_OPT__(,) __VA_ARGS__)

#define PAS_TESTING_ASSERT(exp, ...) \
    PAS_ASSERT_IF(PAS_ENABLE_TESTING, exp __VA_OPT__(,) __VA_ARGS__)

#else /* not PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED */

#define PAS_ASSERT(exp, ...) \
    do { \
        if (!PAS_ENABLE_ASSERT) \
            break; \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_noreturn_silencer(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

#define PAS_TESTING_ASSERT(exp, ...) \
    do { \
        if (!PAS_ENABLE_TESTING) \
            break; \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_noreturn_silencer(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

#endif /* PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED */

#define PAS_ASSERT_WITH_DETAIL(exp) \
    do { \
        if (!PAS_ENABLE_ASSERT) \
            break; \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_no_inline(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

#define PAS_ASSERT_WITH_EXTRA_DETAIL(exp, extra) \
    do { \
        if (!PAS_ENABLE_ASSERT) \
            break; \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_no_inline_with_extra_detail(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp, extra); \
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

static inline void pas_atomic_store_uint8(uint8_t* ptr, uint8_t value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    asm volatile (
        "stlrb %w[value], [%x[ptr]]\t\n"
        /* outputs */  :
        /* inputs  */  : [value]"r"(value), [ptr]"r"(ptr)
        /* clobbers */ : "memory"
    );
#elif PAS_COMPILER(CLANG)
PAS_IGNORE_WARNINGS_BEGIN("atomic-alignment")
    __c11_atomic_store((_Atomic uint8_t*)ptr, value, __ATOMIC_SEQ_CST);
PAS_IGNORE_WARNINGS_END
#else
    __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
#endif
}

static inline bool pas_compare_and_swap_uint8_weak(uint8_t* ptr, uint8_t old_value, uint8_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxrb %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxtb\t\n"
        "b.ne 1f\t\n"
        "stlxrb %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint8_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint8_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline uint8_t pas_compare_and_swap_uint8_strong(uint8_t* ptr, uint8_t old_value, uint8_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxrb %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxtb\t\n"
        "b.ne 1f\t\n"
        "stlxrb %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic uint8_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((uint8_t*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline bool pas_compare_and_swap_uint16_weak(uint16_t* ptr, uint16_t old_value, uint16_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxrh %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxth\t\n"
        "b.ne 1f\t\n"
        "stlxrh %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint16_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint16_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline bool pas_compare_and_swap_uint32_weak(uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxr %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint32_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline uint32_t pas_compare_and_swap_uint32_strong(uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxr %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((uint32_t*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline bool pas_compare_and_swap_uint64_weak(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint64_t value = 0;
    uint64_t cond = 0;
    asm volatile (
        "ldxr %x[value], [%x[ptr]]\t\n"
        "cmp %x[value], %x[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %x[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %x[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %x[cond], xzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint64_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline uint64_t pas_compare_and_swap_uint64_strong(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint64_t value = 0;
    uint64_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxr %x[value], [%x[ptr]]\t\n"
        "cmp %x[value], %x[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %x[new_value], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((uint64_t*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline void pas_atomic_store_bool(bool* ptr, bool value)
{
    pas_atomic_store_uint8((uint8_t*)ptr, value ? 1 : 0);
}

static inline bool pas_compare_and_swap_bool_weak(bool* ptr, bool old_value, bool new_value)
{
    return pas_compare_and_swap_uint8_weak((uint8_t*)ptr, old_value ? 1 : 0, new_value ? 1 : 0);
}

static inline bool pas_compare_and_swap_bool_strong(bool* ptr, bool old_value, bool new_value)
{
    return pas_compare_and_swap_uint8_strong((uint8_t*)ptr, old_value ? 1 : 0, new_value ? 1 : 0);
}

static inline bool pas_compare_and_swap_uintptr_weak(uintptr_t* ptr, uintptr_t old_value, uintptr_t new_value)
{
    return pas_compare_and_swap_uint64_weak((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

static inline uintptr_t pas_compare_and_swap_uintptr_strong(uintptr_t* ptr, uintptr_t old_value, uintptr_t new_value)
{
    return (uintptr_t)pas_compare_and_swap_uint64_strong((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

static inline bool pas_compare_and_swap_ptr_weak(void* ptr, const void* old_value, const void* new_value)
{
    return pas_compare_and_swap_uint64_weak((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

static inline void* pas_compare_and_swap_ptr_strong(void* ptr, const void* old_value, const void* new_value)
{
    return (void*)pas_compare_and_swap_uint64_strong((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
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

#if PAS_COMPILER(CLANG)

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

static inline uintptr_t pas_pair_low(pas_pair pair)
{
    return pair.low;
}

static inline uintptr_t pas_pair_high(pas_pair pair)
{
    return pair.high;
}

#else

typedef __uint128_t pas_pair;

static inline pas_pair pas_pair_create(uintptr_t low, uintptr_t high)
{
    return ((pas_pair)low) | ((pas_pair)(high) << 64);
}

static inline uintptr_t pas_pair_low(pas_pair pair)
{
    return pair;
}

static inline uintptr_t pas_pair_high(pas_pair pair)
{
    return pair >> 64;
}

#endif

static inline bool pas_compare_and_swap_pair_weak(void* raw_ptr,
                                                  pas_pair old_value, pas_pair new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uintptr_t low = 0;
    uintptr_t high = 0;
    uintptr_t old_low = pas_pair_low(old_value);
    uintptr_t old_high = pas_pair_high(old_value);
    uintptr_t new_low = pas_pair_low(new_value);
    uintptr_t new_high = pas_pair_high(new_value);
    uintptr_t cond = 0;
    uintptr_t temp = 0;
    asm volatile (
        "ldxp %x[low], %x[high], [%x[ptr]]\t\n"
        "eor %x[cond], %x[high], %x[old_high]\t\n"
        "eor %x[temp], %x[low], %x[old_low]\t\n"
        "orr %x[cond], %x[temp], %x[cond]\t\n"
        "cbnz %x[cond], 1f\t\n"
        "stlxp %w[cond], %x[new_low], %x[new_high], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %x[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %x[cond], xzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [low]"=&r"(low), [high]"=&r"(high), [cond]"=&r"(cond), [temp]"=&r"(temp)
        /* inputs  */  : [old_low]"r"(old_low), [old_high]"r"(old_high), [new_low]"r"(new_low), [new_high]"r"(new_high), [ptr]"r"(raw_ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
PAS_IGNORE_WARNINGS_BEGIN("atomic-alignment")
    return __c11_atomic_compare_exchange_weak((_Atomic pas_pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
PAS_IGNORE_WARNINGS_END
#else
    return __atomic_compare_exchange_n((pas_pair*)raw_ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline pas_pair pas_compare_and_swap_pair_strong(void* raw_ptr,
                                                        pas_pair old_value, pas_pair new_value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uintptr_t low = 0;
    uintptr_t high = 0;
    uintptr_t old_low = pas_pair_low(old_value);
    uintptr_t old_high = pas_pair_high(old_value);
    uintptr_t new_low = pas_pair_low(new_value);
    uintptr_t new_high = pas_pair_high(new_value);
    uintptr_t cond = 0;
    uintptr_t temp = 0;
    asm volatile (
    "0:\t\n"
        "ldxp %x[low], %x[high], [%x[ptr]]\t\n"
        "eor %x[cond], %x[high], %x[old_high]\t\n"
        "eor %x[temp], %x[low], %x[old_low]\t\n"
        "orr %x[cond], %x[temp], %x[cond]\t\n"
        "cbnz %x[cond], 1f\t\n"
        "stlxp %w[cond], %x[new_low], %x[new_high], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [low]"=&r"(low), [high]"=&r"(high), [cond]"=&r"(cond), [temp]"=&r"(temp)
        /* inputs  */  : [old_low]"r"(old_low), [old_high]"r"(old_high), [new_low]"r"(new_low), [new_high]"r"(new_high), [ptr]"r"(raw_ptr)
        /* clobbers */ : "cc", "memory"
    );
    return pas_pair_create(low, high);
#elif PAS_COMPILER(CLANG)
PAS_IGNORE_WARNINGS_BEGIN("atomic-alignment")
    __c11_atomic_compare_exchange_strong((_Atomic pas_pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
PAS_IGNORE_WARNINGS_END
    return old_value;
#else
    __atomic_compare_exchange_n((pas_pair*)raw_ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline pas_pair pas_atomic_load_pair_relaxed(void* raw_ptr)
{
#if PAS_COMPILER(CLANG)
    /* Since it is __ATOMIC_RELAXED, we do not need to care about memory barrier even when the implementation uses LL/SC. */
PAS_IGNORE_WARNINGS_BEGIN("atomic-alignment")
    return __c11_atomic_load((_Atomic pas_pair*)raw_ptr, __ATOMIC_RELAXED);
PAS_IGNORE_WARNINGS_END
#else
    return __atomic_load_n((pas_pair*)raw_ptr, __ATOMIC_RELAXED);
#endif
}

static inline void pas_atomic_store_pair(void* raw_ptr, pas_pair value)
{
#if PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uintptr_t low = pas_pair_low(value);
    uintptr_t high = pas_pair_high(value);
    uintptr_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxp xzr, %x[cond], [%x[ptr]]\t\n"
        "stlxp %w[cond], %x[low], %x[high], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "dmb ish\t\n"
        /* outputs */  : [cond]"=&r"(cond)
        /* inputs  */  : [low]"r"(low), [high]"r"(high), [ptr]"r"(raw_ptr)
        /* clobbers */ : "cc", "memory"
    );
#elif PAS_COMPILER(CLANG)
PAS_IGNORE_WARNINGS_BEGIN("atomic-alignment")
    __c11_atomic_store((_Atomic pas_pair*)raw_ptr, value, __ATOMIC_SEQ_CST);
PAS_IGNORE_WARNINGS_END
#else
    __atomic_store_n((pas_pair*)raw_ptr, value, __ATOMIC_SEQ_CST);
#endif
}

static inline void pas_atomic_store_pair_relaxed(void* raw_ptr, pas_pair value)
{
    /* Since it is __ATOMIC_RELAXED, we do not need to care about memory barrier even when the implementation uses LL/SC. */
#if PAS_COMPILER(CLANG)
PAS_IGNORE_WARNINGS_BEGIN("atomic-alignment")
    __c11_atomic_store((_Atomic pas_pair*)raw_ptr, value, __ATOMIC_RELAXED);
PAS_IGNORE_WARNINGS_END
#else
    __atomic_store_n((pas_pair*)raw_ptr, value, __ATOMIC_RELAXED);
#endif
}

static PAS_ALWAYS_INLINE bool pas_compare_ptr_opaque(uintptr_t a, uintptr_t b)
{
#if PAS_COMPILER(CLANG)
#if PAS_ARM64
    uint32_t cond = 0;
    asm volatile (
        "cmp %x[a], %x[b]\t\n"
        "cset %w[cond], eq\t\n"
        /* outputs */  : [cond]"=&r"(cond)
        /* inputs  */  : [a]"r"(a), [b]"r"(b)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#else
    return a == b;
#endif
#else
    return a == b;
#endif
}

#define PAS_MIN(a, b) ({ \
        PAS_TYPEOF(a) _tmp_a = (a); \
        PAS_TYPEOF(b) _tmp_b = (b); \
        _tmp_a < _tmp_b ? _tmp_a : _tmp_b; \
    })
#define PAS_MAX(a, b) ({ \
        PAS_TYPEOF(a) _tmp_a = (a); \
        PAS_TYPEOF(b) _tmp_b = (b); \
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
    return (sizeof(uintptr_t) * 8 - 1) - (unsigned)__builtin_clzl(value);
}

/* Undefined for value <= 1. */
static inline unsigned pas_log2_rounded_up(uintptr_t value)
{
    return (sizeof(uintptr_t) * 8 - 1) - ((unsigned)__builtin_clzl(value - 1) - 1);
}

static inline unsigned pas_log2_rounded_up_safe(uintptr_t value)
{
    if (value <= 1)
        return 0;
    return pas_log2_rounded_up(value);
}

static inline uintptr_t pas_round_up_to_next_power_of_2(uintptr_t value)
{
    return (uintptr_t)1 << pas_log2_rounded_up_safe(value);
}

#define PAS_SWAP(left, right) do { \
        PAS_TYPEOF(left) _swap_tmp = left; \
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

#ifdef __cplusplus
enum cpp_initialization_t { cpp_initialization };
#endif

#define PAS_SYSCALL(x) do { \
    while ((x) == -1 && errno == EAGAIN) { } \
} while (0)

PAS_END_EXTERN_C;

#endif /* PAS_UTILS_H */
