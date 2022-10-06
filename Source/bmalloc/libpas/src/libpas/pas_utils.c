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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_utils.h"

#include "pas_lock.h"
#include "pas_log.h"
#include "pas_string_stream.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#if PAS_X86_64

#define CRASH_INST "int3"

#define CRASH_GPR0 "r11"
#define CRASH_GPR1 "r10"
#define CRASH_GPR2 "r9"
#define CRASH_GPR3 "r8"
#define CRASH_GPR4 "r15"
#define CRASH_GPR5 "r14"
#define CRASH_GPR6 "r13"

#elif PAS_ARM64

#define CRASH_INST "brk #0xc471"

#define CRASH_GPR0 "x16"
#define CRASH_GPR1 "x17"
#define CRASH_GPR2 "x18"
#define CRASH_GPR3 "x19"
#define CRASH_GPR4 "x20"
#define CRASH_GPR5 "x21"
#define CRASH_GPR6 "x22"

#endif

#if PAS_X86_64 || PAS_ARM64

#if PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl1(uint64_t reason, uint64_t misc1)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR));
    __builtin_unreachable();
}

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl2(uint64_t reason, uint64_t misc1, uint64_t misc2)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR));
    __builtin_unreachable();
}

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl3(uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR));
    __builtin_unreachable();
}

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl4(uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR));
    __builtin_unreachable();
}

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl5(uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR));
    __builtin_unreachable();
}

PAS_NEVER_INLINE PAS_NO_RETURN void pas_crash_with_info_impl6(uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5, uint64_t misc6)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    register uint64_t misc6GPR asm(CRASH_GPR6) = misc6;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR), "r"(misc6GPR));
    __builtin_unreachable();
}

#endif /* PAS_OS(DARWIN) && PAS_VA_OPT_SUPPORTED */

PAS_NEVER_INLINE PAS_NO_RETURN static void pas_crash_with_info_impl(uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5, uint64_t misc6)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    register uint64_t misc6GPR asm(CRASH_GPR6) = misc6;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR), "r"(misc6GPR));
    __builtin_trap();
}

#else

PAS_IGNORE_WARNINGS_BEGIN("unused-parameter")
PAS_NEVER_INLINE PAS_NO_RETURN static void pas_crash_with_info_impl(uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5, uint64_t misc6) { __builtin_trap(); }
PAS_IGNORE_WARNINGS_END

#endif

void pas_panic(const char* format, ...)
{
    static const bool fast_panic = false;
    if (!fast_panic) {
        va_list arg_list;
        pas_log("[%d] pas panic: ", getpid());
        va_start(arg_list, format);
        pas_vlog(format, arg_list);
        pas_crash_with_info_impl((uint64_t)format, 0, 0, 0, 0, 0, 0);
    }
    __builtin_trap();
}

#if PAS_ENABLE_TESTING
PAS_NEVER_INLINE void pas_report_assertion_failed(
    const char* filename, int line, const char* function, const char* expression)
{
    pas_log("[%d] pas panic: ", getpid());
    pas_log("%s:%d: %s: assertion %s failed.\n", filename, line, function, expression);
}
#endif /* PAS_ENABLE_TESTING */

void pas_assertion_failed_no_inline(const char* filename, int line, const char* function, const char* expression)
{
    pas_log("[%d] pas assertion failed: ", getpid());
    pas_log("%s:%d: %s: assertion %s failed.\n", filename, line, function, expression);
    pas_crash_with_info_impl((uint64_t)filename, (uint64_t)line, (uint64_t)function, (uint64_t)expression, 0xbeefbff0, 42, 1337);
}

void pas_assertion_failed_no_inline_with_extra_detail(const char* filename, int line, const char* function, const char* expression, uint64_t extra)
{
    pas_log("[%d] pas assertion failed (with extra detail): ", getpid());
    pas_log("%s:%d: %s: assertion %s failed. Extra data: %" PRIu64 ".\n", filename, line, function, expression, extra);
    pas_crash_with_info_impl((uint64_t)filename, (uint64_t)line, (uint64_t)function, (uint64_t)expression, extra, 1337, 0xbeef0bff);
}

void pas_panic_on_out_of_memory_error(void)
{
    __builtin_trap();
}

static void (*deallocation_did_fail_callback)(const char* reason, void* begin);

PAS_NO_RETURN PAS_NEVER_INLINE void pas_deallocation_did_fail(const char *reason, uintptr_t begin)
{
    if (deallocation_did_fail_callback)
        deallocation_did_fail_callback(reason, (void*)begin);
    pas_panic("deallocation did fail at %p: %s\n", (void*)begin, reason);
}

void pas_set_deallocation_did_fail_callback(void (*callback)(const char* reason, void* begin))
{
    deallocation_did_fail_callback = callback;
}

static void (*reallocation_did_fail_callback)(const char* reason,
                                              void* source_heap,
                                              void* target_heap,
                                              void* old_ptr,
                                              size_t old_size,
                                              size_t new_size);

PAS_NO_RETURN PAS_NEVER_INLINE void pas_reallocation_did_fail(const char *reason,
                                                              void* source_heap,
                                                              void* target_heap,
                                                              void* old_ptr,
                                                              size_t old_size,
                                                              size_t new_size)
{
    if (reallocation_did_fail_callback) {
        reallocation_did_fail_callback(
            reason, source_heap, target_heap, old_ptr, old_size, new_size);
    }
    pas_panic("reallocation did fail with source_heap = %p, target_heap = %p, "
              "old_ptr = %p, old_size = %zu, new_size = %zu: %s\n",
              source_heap, target_heap, old_ptr, old_size, new_size,
              reason);
}

void pas_set_reallocation_did_fail_callback(void (*callback)(const char* reason,
                                                             void* source_heap,
                                                             void* target_heap,
                                                             void* old_ptr,
                                                             size_t old_size,
                                                             size_t new_count))
{
    reallocation_did_fail_callback = callback;
}

#endif /* LIBPAS_ENABLED */
