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

#include "pas_large_free_heap_deferred_commit_log.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_debug_spectrum.h"
#include "pas_page_malloc.h"
#include "pas_physical_memory_transaction.h"
#include "pas_stream.h"
#include "pas_virtual_range.h"

void pas_large_free_heap_deferred_commit_log_construct(
    pas_large_free_heap_deferred_commit_log* log)
{
    pas_large_virtual_range_min_heap_construct(&log->impl);
    log->total = 0;
}

void pas_large_free_heap_deferred_commit_log_destruct(
    pas_large_free_heap_deferred_commit_log* log)
{
    pas_allocation_config allocation_config;
    
    PAS_ASSERT(!log->total);
    PAS_ASSERT(!log->impl.size);
    
    pas_bootstrap_free_heap_allocation_config_construct(&allocation_config, pas_lock_is_held);
    pas_large_virtual_range_min_heap_destruct(&log->impl, &allocation_config);
}

bool pas_large_free_heap_deferred_commit_log_add(
    pas_large_free_heap_deferred_commit_log* log,
    pas_large_virtual_range range,
    pas_physical_memory_transaction* transaction)
{
    pas_allocation_config allocation_config;
    
    if (!log->total
        && &pas_virtual_range_common_lock != transaction->lock_held
        && !pas_lock_try_lock(&pas_virtual_range_common_lock)) {
        pas_physical_memory_transaction_did_fail_to_acquire_lock(
            transaction, &pas_virtual_range_common_lock);
        return false;
    }
    
    log->total += pas_large_virtual_range_size(range);
    
    pas_bootstrap_free_heap_allocation_config_construct(&allocation_config, pas_lock_is_held);
    pas_large_virtual_range_min_heap_add(&log->impl, range, &allocation_config);
    
    return true;
}

static void dump_large_commit(pas_stream* stream, void* arg)
{
    PAS_UNUSED_PARAM(arg);
    pas_stream_printf(stream, "large deferred");
}

static void commit(pas_large_virtual_range range)
{
    static const bool verbose = false;
    
    if (pas_large_virtual_range_is_empty(range))
        return;
    
    if (verbose) {
        printf("Committing %p...%p.\n",
               (void*)range.begin,
               (void*)range.end);
    }
    
    pas_page_malloc_commit((void*)range.begin, pas_large_virtual_range_size(range), range.mmap_capability);

    if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT)
        pas_debug_spectrum_add(dump_large_commit, dump_large_commit, pas_large_virtual_range_size(range));
}

static void commit_all(
    pas_large_free_heap_deferred_commit_log* log,
    pas_physical_memory_transaction* transaction,
    bool for_real)
{
    pas_large_virtual_range current_range;
    
    if (!log->total)
        return;
    
    current_range = pas_large_virtual_range_create_empty();
    
    for (;;) {
        pas_large_virtual_range next_range = pas_large_virtual_range_min_heap_take_min(&log->impl);
        if (pas_large_virtual_range_is_empty(next_range))
            break;
        
        PAS_ASSERT(!pas_large_virtual_range_overlaps(current_range, next_range));
        
        if (next_range.begin == current_range.end) {
            current_range.end = next_range.end;
            continue;
        }
        
        if (for_real)
            commit(current_range);
        current_range = next_range;
    }
    
    if (for_real)
        commit(current_range);
    
    if (&pas_virtual_range_common_lock != transaction->lock_held)
        pas_lock_unlock(&pas_virtual_range_common_lock);
    
    log->total = 0;
}

void pas_large_free_heap_deferred_commit_log_commit_all(
    pas_large_free_heap_deferred_commit_log* log,
    pas_physical_memory_transaction* transaction)
{
    bool for_real = true;
    commit_all(log, transaction, for_real);
}

void pas_large_free_heap_deferred_commit_log_pretend_to_commit_all(
    pas_large_free_heap_deferred_commit_log* log,
    pas_physical_memory_transaction* transaction)
{
    bool for_real = false;
    commit_all(log, transaction, for_real);
}

#endif /* LIBPAS_ENABLED */
