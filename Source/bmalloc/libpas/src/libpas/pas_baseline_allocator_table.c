/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#include "pas_baseline_allocator_table.h"

#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_internal_config.h"
#include "pas_random.h"
#include <pthread.h>

pas_baseline_allocator* pas_baseline_allocator_table;
uint64_t pas_num_baseline_allocator_evictions = 0;
unsigned pas_baseline_allocator_table_bound = PAS_NUM_BASELINE_ALLOCATORS;

static void initialize(void)
{
    pas_baseline_allocator* table;
    size_t index;
    pas_heap_lock_lock();
    table = pas_immortal_heap_allocate(
        PAS_NUM_BASELINE_ALLOCATORS * sizeof(pas_baseline_allocator),
        "pas_baseline_allocator_table",
        pas_object_allocation);
    for (index = PAS_NUM_BASELINE_ALLOCATORS; index--;)
        table[index] = PAS_BASELINE_ALLOCATOR_INITIALIZER;
    pas_store_store_fence();
    pas_baseline_allocator_table = table;
    pas_heap_lock_unlock();
}

void pas_baseline_allocator_table_initialize_if_necessary(void)
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    pthread_once(&once_control, initialize);
}

unsigned pas_baseline_allocator_table_get_random_index(void)
{
    return pas_get_random(pas_cheesy_random, PAS_MIN(PAS_NUM_BASELINE_ALLOCATORS,
                                                     pas_baseline_allocator_table_bound));
}

bool pas_baseline_allocator_table_for_all(pas_allocator_scavenge_action action)
{
    size_t index;
    bool result;

    if (!pas_baseline_allocator_table)
        return false;

    result = false;

    for (index = PAS_NUM_BASELINE_ALLOCATORS; index--;) {
        pas_baseline_allocator* allocator;

        allocator = pas_baseline_allocator_table + index;

        pas_lock_lock(&allocator->lock);
        result |= pas_local_allocator_scavenge(&allocator->u.allocator, action);
        pas_lock_unlock(&allocator->lock);
    }

    return result;
}

#endif /* LIBPAS_ENABLED */
