/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "pas_heap_summary.h"

#include "pas_stream.h"

void pas_heap_summary_validate(pas_heap_summary* summary)
{
    PAS_ASSERT(summary->free + summary->allocated <= summary->committed + summary->decommitted);
    PAS_ASSERT((summary->allocated +
                summary->meta_ineligible_for_decommit +
                summary->meta_eligible_for_decommit)
               == summary->committed);
    PAS_ASSERT((summary->free_ineligible_for_decommit +
                summary->free_eligible_for_decommit +
                summary->free_decommitted)
               == summary->free);
    PAS_ASSERT(summary->free_ineligible_for_decommit + summary->free_eligible_for_decommit
               <= summary->committed);
    PAS_ASSERT(summary->free_decommitted <= summary->decommitted);
    PAS_ASSERT(summary->cached <= summary->committed);
}

void pas_heap_summary_dump(pas_heap_summary summary, pas_stream* stream)
{
    pas_stream_printf(
        stream,
        "%.0lf%% Alloc: %zu/%zu (CO)/%zu (CT)/%zu (R); Frag: %zu (%.0lf%%)",
        pas_heap_summary_total(summary)
        ? 100. * (summary.allocated + summary.meta) / pas_heap_summary_total(summary)
        : 0.,
        summary.allocated,
        pas_heap_summary_committed_objects(summary),
        summary.committed,
        pas_heap_summary_total(summary),
        pas_heap_summary_fragmentation(summary),
        summary.committed
        ? 100. * pas_heap_summary_fragmentation(summary) / summary.committed
        : 0.);

    if (summary.cached)
        pas_stream_printf(stream, "; Cached: %zu", summary.cached);
}

#endif /* LIBPAS_ENABLED */
