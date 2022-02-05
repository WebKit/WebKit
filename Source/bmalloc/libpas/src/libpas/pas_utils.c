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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_utils.h"

#include "pas_lock.h"
#include "pas_log.h"
#include "pas_string_stream.h"
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

void pas_panic(const char* format, ...)
{
    static const bool fast_panic = false;
    if (!fast_panic) {
        va_list arg_list;
        pas_log("[%d] pas panic: ", getpid());
        va_start(arg_list, format);
        pas_vlog(format, arg_list);
    }
    __builtin_trap();
}

#if PAS_ENABLE_TESTING
void pas_assertion_failed(const char* filename, int line, const char* function, const char* expression)
{
    pas_panic("%s:%d: %s: assertion %s failed.\n", filename, line, function, expression);
}
#endif /* PAS_ENABLE_TESTING */

void pas_panic_on_out_of_memory_error()
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
