/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifdef __cplusplus
#define __PAS_BEGIN_EXTERN_C extern "C" { struct __pas_require_semicolon
#define __PAS_END_EXTERN_C } struct __pas_require_semicolon
#else
#define __PAS_BEGIN_EXTERN_C struct __pas_require_semicolon
#define __PAS_END_EXTERN_C struct __pas_require_semicolon
#endif

__PAS_BEGIN_EXTERN_C;

/* Source annotations that the preprocessor may attach to expressions. These
   used to be embedded in multi-line comments, but multi-line comments can't be
   recursive and this caused issues in some narrow cases.

   These aren't intended to be used in a production-ready Secure C. */
#define __TODO__
#define __SUSPICIOUS__
#define __BROKEN__

#ifdef __OPTIMIZE__
#define __PAS_ALWAYS_INLINE_BUT_NOT_INLINE __attribute__((__always_inline__))
#else
#define __PAS_ALWAYS_INLINE_BUT_NOT_INLINE
#endif

#define __PAS_ALWAYS_INLINE inline __PAS_ALWAYS_INLINE_BUT_NOT_INLINE

#define __PAS_NEVER_INLINE __attribute__((__noinline__))
#define __PAS_NO_RETURN __attribute((__noreturn__))

#if defined(PAS_LIBMALLOC) && PAS_LIBMALLOC
#define __PAS_API __attribute__((visibility("hidden")))
#else
#define __PAS_API
#endif

#define __PAS_UNUSED_PARAM(variable) (void)variable

#define __PAS_OFFSETOF(type, field) __builtin_offsetof(type, field)

typedef __SIZE_TYPE__ __pas_size_t;
typedef typeof((char*)0 - (char*)0) __pas_ptrdiff_t;

__PAS_API void __pas_set_deallocation_did_fail_callback(
    void (*callback)(const char* reason, void* begin));
__PAS_API void __pas_set_reallocation_did_fail_callback(
    void (*callback)(const char* reason,
                     void* source_heap,
                     void* target_heap,
                     void* old_ptr,
                     __pas_size_t old_size,
                     __pas_size_t new_count));

#define __PAS_LIKELY(x) __builtin_expect(!!(x), 1)
#define __PAS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define __PAS_ROUND_UP_TO_POWER_OF_2(size, alignment) \
    (((size) + (alignment) - 1) & -(alignment))

static inline __pas_size_t __pas_round_up_to_power_of_2(__pas_size_t size, __pas_size_t alignment)
{
    return __PAS_ROUND_UP_TO_POWER_OF_2(size, alignment);
}

static __PAS_ALWAYS_INLINE void __pas_compiler_fence(void)
{
    asm volatile("" ::: "memory");
}

static __PAS_ALWAYS_INLINE void __pas_fence(void)
{
#if !__PAS_ARM
    if (sizeof(void*) == 8)
        asm volatile("lock; orl $0, (%%rsp)" ::: "memory");
    else
        asm volatile("lock; orl $0, (%%esp)" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

static __PAS_ALWAYS_INLINE unsigned __pas_depend_impl(unsigned long input, _Bool cpu_only)
{
    unsigned output;
#if __PAS_ARM64
    // Create a magical zero value through inline assembly, whose computation
    // isn't visible to the optimizer. This zero is then usable as an offset in
    // further address computations: adding zero does nothing, but the compiler
    // doesn't know it. It's magical because it creates an address dependency
    // from the load of `location` to the uses of the dependency, which triggers
    // the ARM ISA's address dependency rule, a.k.a. the mythical C++ consume
    // ordering. This forces weak memory order CPUs to observe `location` and
    // dependent loads in their store order without the reader using a barrier
    // or an acquire load.
    __PAS_UNUSED_PARAM(cpu_only);
    asm volatile ("eor %w[out], %w[in], %w[in]"
                  : [out] "=r"(output)
                  : [in] "r"(input)
                  : "memory");
#elif __PAS_ARM
    __PAS_UNUSED_PARAM(cpu_only);
    asm volatile ("eor %[out], %[in], %[in]"
                  : [out] "=r"(output)
                  : [in] "r"(input)
                  : "memory");
#else
    __PAS_UNUSED_PARAM(input);
    // No dependency is needed for this architecture.
    if (!cpu_only)
        __pas_compiler_fence();
    output = 0;
#endif
    return output;
}

static __PAS_ALWAYS_INLINE unsigned __pas_depend(unsigned long input)
{
    _Bool cpu_only = 0;
    return __pas_depend_impl(input, cpu_only);
}

static __PAS_ALWAYS_INLINE unsigned __pas_depend_cpu_only(unsigned long input)
{
    _Bool cpu_only = 1;
    return __pas_depend_impl(input, cpu_only);
}

static inline void __pas_memcpy(volatile void* to, const volatile void* from, __pas_size_t size)
{
    __builtin_memcpy((void*)(unsigned long)to, (void*)(unsigned long)from, size);
}

__PAS_END_EXTERN_C;
