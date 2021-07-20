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

#include "pas_segregated_size_directory.h"

#include "pas_deferred_decommit_log.h"
#include "pas_exclusive_view_template_memo_table.h"
#include "pas_monotonic_time.h"
#include "pas_page_malloc.h"
#include "pas_segregated_directory_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_thread_local_cache_layout.h"

#ifdef PAS_LIBMALLOC
bool pas_segregated_size_directory_use_tabling = true;
#else
bool pas_segregated_size_directory_use_tabling = false;
#endif

void pas_segregated_size_directory_construct(
    pas_segregated_directory* directory,
    pas_segregated_page_config_kind page_config_kind,
    pas_page_sharing_mode page_sharing_mode,
    pas_segregated_directory_kind kind)
{
    pas_segregated_directory_construct(directory, page_config_kind, page_sharing_mode, kind);
}

typedef struct {
    pas_segregated_directory* directory;
    pas_segregated_size_directory_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_object_callback(
    pas_segregated_view view,
    pas_range range,
    void* arg)
{
    for_each_live_object_data* data;

    data = arg;

    PAS_ASSERT(pas_segregated_size_directory_get_global(data->directory)->object_size
               == pas_range_size(range));

    return data->callback(data->directory, view, range.begin, data->arg);
}

bool pas_segregated_size_directory_for_each_live_object(
    pas_segregated_directory* directory,
    pas_segregated_size_directory_for_each_live_object_callback callback,
    void* arg)
{
    for_each_live_object_data data;
    size_t index;

    data.directory = directory;
    data.callback = callback;
    data.arg = arg;

    /* The cool thing about this loop is that in case of races it'll tend to go check out
       more view. */
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        pas_segregated_view view;

        view = pas_segregated_directory_get(directory, index);
        if (!pas_segregated_view_for_each_live_object(
                view, for_each_live_object_object_callback, &data, pas_lock_is_not_held))
            return false;
    }

    return true;
}

pas_segregated_directory* pas_segregated_size_directory_for_object(
    uintptr_t begin,
    pas_heap_config* config)
{
    pas_segregated_view view;
    view = pas_segregated_view_for_object(begin, config);
    if (view)
        return pas_segregated_view_get_size_directory(view);
    return NULL;
}

#endif /* LIBPAS_ENABLED */
