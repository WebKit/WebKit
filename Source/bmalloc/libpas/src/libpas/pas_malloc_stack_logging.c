/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include "pas_darwin_spi.h"
#include "pas_get_allocation_size.h"
#include "pas_malloc_stack_logging.h"
#include <stdlib.h>

PAS_BEGIN_EXTERN_C;

pas_msl_is_enabled_flag pas_msl_is_enabled_flag_value = pas_msl_is_enabled_flag_indeterminate;
static void compute_msl_status(void)
{
    pas_msl_is_enabled_flag_value = getenv("MallocStackLogging") ? pas_msl_is_enabled_flag_enabled : pas_msl_is_enabled_flag_disabled;
}

bool pas_compute_msl_is_enabled(void)
{
    static pthread_once_t key = PTHREAD_ONCE_INIT;
    pthread_once(&key, compute_msl_status);
    return pas_msl_is_enabled_flag_value == pas_msl_is_enabled_flag_enabled;
}

#if PAS_ENABLE_MALLOC_STACK_LOGGER

#if !PAS_OS(DARWIN)
/* On Darwin the system libmalloc owns this global; elsewhere libpas provides it
   so an injected tool (e.g. via LD_PRELOAD) can install a logger callback. */
PAS_API malloc_logger_t* malloc_logger = NULL;
#endif

/* The zone argument of a malloc_logger callout is a malloc_zone_t that only supports size(). It
   lets the hook query the size of the underlying block (as opposed to the requested size, which
   the hook already receives). The wrapper carries the context needed to do that lookup safely
   from inside the callout: the heap config the pointer belongs to, and whether the callout runs
   with the heap lock held.

   Every other zone entry is NULL: the zone is never registered with libmalloc and must not be
   used for anything but size(). It lives on the stack of the callout, so hooks must not retain
   it past the call. */
typedef struct {
    malloc_zone_t zone; /* Must be first, size() casts back to the wrapper. */
    const pas_heap_config* config;
    pas_lock_hold_mode heap_lock_hold_mode;
} pas_msl_zone;

static size_t pas_msl_zone_size(malloc_zone_t* zone, const void* ptr)
{
    pas_msl_zone* pas_zone = (pas_msl_zone*)zone;
    if (pas_zone->heap_lock_hold_mode == pas_lock_is_held)
        pas_heap_lock_assert_held(); /* Prevents race conditions due to missing locks. */
    /* else: Trying to retake an already-held lock will reliably fail. */

    return pas_get_allocation_size_with_heap_lock_hold_mode(
        (void*)(uintptr_t)ptr, *pas_zone->config, pas_zone->heap_lock_hold_mode);
}

static pas_msl_zone init_zone(const pas_heap_config* config, pas_lock_hold_mode heap_lock_hold_mode)
{
    return (pas_msl_zone)
    {
        .zone = { .size = pas_msl_zone_size },
        .config = config,
        .heap_lock_hold_mode = heap_lock_hold_mode,
    };
}

PAS_NEVER_INLINE pas_allocation_result pas_msl_malloc_logging_slow(size_t size, pas_allocation_result result, const pas_heap_config* config, pas_lock_hold_mode heap_lock_hold_mode)
{
    PAS_TESTING_ASSERT(malloc_logger);
    if (result.did_succeed && pas_msl_is_enabled()) {
        pas_msl_zone zone = init_zone(config, heap_lock_hold_mode);
        uint32_t type = (pas_stack_logging_flag_zone | pas_stack_logging_type_alloc);
        malloc_logger(type, (uintptr_t)&zone, (uintptr_t)size, 0, (uintptr_t)result.begin, 0);
    }
    return result;
}

PAS_NEVER_INLINE void pas_msl_free_logging_slow(void* ptr, const pas_heap_config* config, pas_lock_hold_mode heap_lock_hold_mode)
{
    PAS_TESTING_ASSERT(malloc_logger);
    if (ptr && pas_msl_is_enabled()) {
        pas_msl_zone zone = init_zone(config, heap_lock_hold_mode);
        uint32_t type = (pas_stack_logging_flag_zone | pas_stack_logging_type_dealloc);
        malloc_logger(type, (uintptr_t)&zone, (uintptr_t)ptr, 0, 0, 0);
    }
}

#endif

PAS_END_EXTERN_C;

#endif /* LIBPAS_ENABLED */
