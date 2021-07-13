/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_commit_span.h"

#include "pas_deferred_decommit_log.h"
#include "pas_log.h"
#include "pas_page_base.h"
#include "pas_page_malloc.h"

static const bool verbose = false;

void pas_commit_span_construct(pas_commit_span* span)
{
    if (verbose)
        pas_log("%p: creating commit span.\n");
    span->index_of_start_of_span = UINTPTR_MAX;
    span->did_add_first = false;
    span->total_bytes = 0;
}

void pas_commit_span_add_to_change(pas_commit_span* span, uintptr_t granule_index)
{
    if (span->index_of_start_of_span == UINTPTR_MAX)
        span->index_of_start_of_span = granule_index;
    else
        PAS_ASSERT(span->index_of_start_of_span < granule_index);
}

void pas_commit_span_add_unchanged(pas_commit_span* span,
                                   pas_page_base* page,
                                   uintptr_t granule_index,
                                   pas_page_base_config* config,
                                   void (*commit_or_decommit)(void* base, size_t size, void* arg),
                                   void* arg)
{
    size_t size;
    
    if (span->index_of_start_of_span == UINTPTR_MAX)
        return;
    
    if (verbose)
        pas_log("%p: adding a thing.\n");

    PAS_ASSERT(span->index_of_start_of_span < granule_index);

    size = (granule_index - span->index_of_start_of_span) * config->granule_size;
    
    commit_or_decommit(
        (char*)pas_page_base_boundary(page, *config)
        + span->index_of_start_of_span * config->granule_size,
        size,
        arg);
    span->index_of_start_of_span = UINTPTR_MAX;
    span->did_add_first = true;
    span->total_bytes += size;
}

static void commit(void* base, size_t size, void* arg)
{
    PAS_ASSERT(!arg);
    pas_page_malloc_commit(base, size);
}

void pas_commit_span_add_unchanged_and_commit(pas_commit_span* span,
                                              pas_page_base* page,
                                              uintptr_t granule_index,
                                              pas_page_base_config* config)
{
    pas_commit_span_add_unchanged(span, page, granule_index, config, commit, NULL);
}

typedef struct {
    pas_commit_span* span;
    pas_deferred_decommit_log* log;
    pas_lock* commit_lock;
    pas_lock_hold_mode heap_lock_hold_mode;
} decommit_data;

static void decommit(void* base, size_t size, void* arg)
{
    decommit_data* data;

    data = arg;

    if (verbose)
        pas_log("did_add_first = %d, base = %p, size = %zu\n", data->span->did_add_first, base, size);

    pas_deferred_decommit_log_add_already_locked(
        data->log,
        pas_virtual_range_create(
            (uintptr_t)base,
            (uintptr_t)base + size,
            data->span->did_add_first ? NULL : data->commit_lock),
        data->heap_lock_hold_mode);
}

void pas_commit_span_add_unchanged_and_decommit(pas_commit_span* span,
                                                pas_page_base* page,
                                                uintptr_t granule_index,
                                                pas_deferred_decommit_log* log,
                                                pas_lock* commit_lock,
                                                pas_page_base_config* config,
                                                pas_lock_hold_mode heap_lock_hold_mode)
{
    decommit_data data;
    data.span = span;
    data.log = log;
    data.commit_lock = commit_lock;
    data.heap_lock_hold_mode = heap_lock_hold_mode;
    pas_commit_span_add_unchanged(span, page, granule_index, config, decommit, &data);
}

#endif /* LIBPAS_ENABLED */
