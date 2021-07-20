/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_all_magazines.h"

#include "pas_heap_lock.h"
#include "pas_magazine.h"
#include "pas_monotonic_time.h"
#include "pas_thread_local_cache_node.h"

#if defined(__has_include) && __has_include(<os/tsd.h>)
#include <os/tsd.h>
#define PAS_HAVE_OS_TSD 1
#else
#define PAS_HAVE_OS_TSD 0
#endif

pas_all_magazines_vector pas_all_magazines_vector_instance = PAS_IMMUTABLE_VECTOR_INITIALIZER;
unsigned pas_all_magazines_forced_cpu_number = UINT_MAX;

pas_magazine* pas_all_magazines_get_current(void)
{
    static const bool verbose = false;
    
    unsigned cpu;
    unsigned size;

    cpu = pas_all_magazines_forced_cpu_number;
    if (cpu == UINT_MAX) {
#if PAS_HAVE_OS_TSD
        cpu = _os_cpu_number();
#else
        cpu = 0; /* FIXME: We should be able to do better than this! */
#endif
    }

    if (verbose)
        pas_log("cpu = %u\n", cpu);

    size = pas_all_magazines_vector_instance.size;

    if (PAS_LIKELY(cpu < size))
        return pas_all_magazines_vector_get(&pas_all_magazines_vector_instance + pas_depend(size), cpu);

    pas_heap_lock_lock();

    while (cpu >= pas_all_magazines_vector_instance.size) {
        pas_magazine* magazine;

        magazine = pas_magazine_create(pas_all_magazines_vector_instance.size);
        
        pas_all_magazines_vector_append(&pas_all_magazines_vector_instance,
                                        magazine, pas_lock_is_held);
    }

    pas_heap_lock_unlock();

    PAS_ASSERT(cpu < pas_all_magazines_vector_instance.size);
    return pas_all_magazines_vector_get(&pas_all_magazines_vector_instance, cpu);
}

#endif /* LIBPAS_ENABLED */
