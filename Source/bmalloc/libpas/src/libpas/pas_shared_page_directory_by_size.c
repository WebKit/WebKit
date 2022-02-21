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

#include "pas_shared_page_directory_by_size.h"

#include <math.h>
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_segregated_page_config.h"
#include "pas_stream.h"

pas_segregated_shared_page_directory* pas_shared_page_directory_by_size_get(
    pas_shared_page_directory_by_size* by_size,
    unsigned size,
    pas_segregated_page_config* page_config)
{
    pas_shared_page_directory_by_size_data* data;
    unsigned index;

    data = by_size->data;
    if (PAS_UNLIKELY(!data)) {
        unsigned log_shift;
        unsigned min_size;
        unsigned max_size;
        unsigned num_directories;
        unsigned max_index;

        log_shift = by_size->log_shift;
        
        min_size = (unsigned)pas_segregated_page_config_min_align(*page_config);
        max_size = (unsigned)page_config->base.max_object_size;

        PAS_ASSERT(size >= min_size);
        PAS_ASSERT(size <= max_size);

        max_index = pas_log2_rounded_up_safe(
            max_size >> page_config->base.min_align_shift) >> log_shift;
        
        PAS_ASSERT(max_index <= max_size - min_size);

        num_directories = (unsigned)max_index + 1;

        pas_heap_lock_lock();

        data = by_size->data;
        if (data) {
            PAS_ASSERT(data->log_shift == log_shift);
            PAS_ASSERT(data->num_directories == num_directories);
        } else {
            unsigned index;
            
            data = pas_immortal_heap_allocate(
                PAS_OFFSETOF(pas_shared_page_directory_by_size_data, directories)
                + sizeof(pas_segregated_shared_page_directory) * num_directories,
                "pas_shared_page_directory_by_size_data",
                pas_object_allocation);

            data->log_shift = log_shift;
            data->num_directories = num_directories;

            for (index = num_directories; index--;) {
                data->directories[index] = PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_INITIALIZER(
                    *page_config, by_size->sharing_mode,
                    (void*)(((size_t)1 << ((size_t)index << log_shift)) << page_config->base.min_align_shift));
            }

            pas_fence();

            by_size->data = data;
        }
        
        pas_heap_lock_unlock();
    }

    index = pas_log2_rounded_up_safe(size >> page_config->base.min_align_shift) >> data->log_shift;

    PAS_ASSERT(index < data->num_directories);

    return data->directories + index;
}

bool pas_shared_page_directory_by_size_for_each(
    pas_shared_page_directory_by_size* by_size,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg)
{
    pas_shared_page_directory_by_size_data* data;
    unsigned index;

    data = by_size->data;
    if (!data)
        return true;

    for (index = data->num_directories; index--;) {
        if (!callback(data->directories + index, arg))
            return false;
    }

    return true;
}

bool pas_shared_page_directory_by_size_for_each_remote(
    pas_shared_page_directory_by_size* by_size,
    pas_enumerator* enumerator,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg)
{
    pas_shared_page_directory_by_size_data* data;
    unsigned index;

    data = pas_enumerator_read_compact(enumerator, by_size->data);
    if (!data)
        return true;

    for (index = data->num_directories; index--;) {
        if (!callback(enumerator, data->directories + index, arg))
            return false;
    }

    return true;
}

void pas_shared_page_directory_by_size_dump_directory_arg(
    pas_stream* stream,
    pas_segregated_shared_page_directory* directory)
{
    pas_stream_printf(stream, "Size = %zu", (size_t)directory->dump_arg);
}

#endif /* LIBPAS_ENABLED */
