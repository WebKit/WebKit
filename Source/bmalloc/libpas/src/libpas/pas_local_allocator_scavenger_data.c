/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "pas_local_allocator_scavenger_data.h"

#include "pas_local_allocator.h"
#include "pas_local_view_cache.h"

uint8_t pas_local_allocator_should_stop_count_for_suspend = 5;

bool pas_local_allocator_scavenger_data_is_active(pas_local_allocator_scavenger_data* data)
{
    switch (data->kind) {
    case pas_local_allocator_allocator_kind:
        return pas_local_allocator_is_active((pas_local_allocator*)data);
    case pas_local_allocator_view_cache_kind:
        return !pas_local_view_cache_is_empty((pas_local_view_cache*)data);
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

bool pas_local_allocator_scavenger_data_stop(
    pas_local_allocator_scavenger_data* data,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    switch (data->kind) {
    case pas_local_allocator_allocator_kind:
        return pas_local_allocator_stop((pas_local_allocator*)data, page_lock_mode, heap_lock_hold_mode);
    case pas_local_allocator_view_cache_kind:
        return pas_local_view_cache_stop((pas_local_view_cache*)data, page_lock_mode);
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

#endif /* LIBPAS_ENABLED */


