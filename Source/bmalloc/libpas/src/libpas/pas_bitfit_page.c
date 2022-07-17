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

#include "pas_bitfit_page.h"

#include "pas_bitfit_directory.h"
#include "pas_bitfit_view.h"
#include "pas_epoch.h"
#include "pas_log.h"

void pas_bitfit_page_construct(pas_bitfit_page* page,
                               pas_bitfit_view* view,
                               const pas_bitfit_page_config* config_ptr)
{
    static const bool verbose = false;
    
    pas_bitfit_page_config config;

    config = *config_ptr;
    
    PAS_ASSERT(config.base.page_config_kind == pas_page_config_kind_bitfit);

    pas_lock_assert_held(&view->ownership_lock);

    if (verbose) {
        pas_log("Constructing page %p with view %p and config %s\n",
                page, view, pas_bitfit_page_config_kind_get_string(config.kind));
    }

    pas_page_base_construct(&page->base, pas_page_kind_for_bitfit_variant(config.variant));

    page->use_epoch = PAS_EPOCH_INVALID;

    page->did_note_max_free = false;
    page->num_live_bits = 0;
    pas_compact_atomic_bitfit_view_ptr_store(&page->owner, view);

    pas_zero_memory(page->bits, pas_bitfit_page_config_num_alloc_bit_bytes(config));
    
    pas_bitvector64_set_range(
        (uint64_t*)pas_bitfit_page_free_bits(page),
        pas_bitfit_page_offset_to_first_object(config) >> config.base.min_align_shift,
        config.base.page_size >> config.base.min_align_shift,
        true);

    if (PAS_ENABLE_TESTING) {
        size_t begin;
        size_t end;
        size_t offset;
        
        begin = pas_bitfit_page_offset_to_first_object(config);
        end = pas_bitfit_page_offset_to_end_of_last_object(config);
        
        for (offset = begin; offset < end; offset += pas_page_base_config_min_align(config.base)) {
            PAS_TESTING_ASSERT(pas_bitvector_get(pas_bitfit_page_free_bits(page),
                                                 offset >> config.base.min_align_shift));
            PAS_TESTING_ASSERT(!pas_bitvector_get(pas_bitfit_page_object_end_bits(page, config),
                                                  offset >> config.base.min_align_shift));
        }
    }

    if (config.base.page_size != config.base.granule_size) {
        pas_page_granule_use_count* use_counts;
        size_t num_granules;
        uintptr_t start_of_payload;
        uintptr_t end_of_payload;

        use_counts = pas_bitfit_page_get_granule_use_counts(page, config);
        num_granules = config.base.page_size / config.base.granule_size;

        if (verbose) {
            pas_log("Constructed page at %p, use_counts at %p up to %p\n",
                    page, use_counts, use_counts + num_granules);
        }
        
        pas_zero_memory(use_counts, num_granules * sizeof(pas_page_granule_use_count));
        
        /* If there are any bytes in the page not made available for allocation then make sure
           that the use counts know about it. */
        start_of_payload = config.page_object_payload_offset;
        end_of_payload = config.page_object_payload_offset + config.page_object_payload_size;

        pas_page_granule_increment_uses_for_range(
            use_counts, 0, start_of_payload,
            config.base.page_size, config.base.granule_size);
        pas_page_granule_increment_uses_for_range(
            use_counts, end_of_payload, config.base.page_size,
            config.base.page_size, config.base.granule_size);
    }

    if (verbose) {
        pas_log("bits after construction:\n");
        pas_bitfit_page_log_bits(page, 0, 0);
    }
}

const pas_bitfit_page_config* pas_bitfit_page_get_config(pas_bitfit_page* page)
{
    return pas_bitfit_page_config_kind_get_config(
        pas_compact_bitfit_directory_ptr_load_non_null(
            &pas_compact_atomic_bitfit_view_ptr_load_non_null(
                &page->owner)->directory)->config_kind);
}

void pas_bitfit_page_log_bits(
    pas_bitfit_page* page, uintptr_t mark_begin_offset, uintptr_t mark_end_offset)
{
    size_t offset;
    pas_bitfit_page_config config;

    config = *pas_bitfit_page_get_config(page);
    
    pas_log("free bits: ");
    for (offset = 0;
         offset < config.base.page_size;
         offset += pas_page_base_config_min_align(config.base)) {
        pas_log("%d", pas_bitvector_get(pas_bitfit_page_free_bits(page),
                                        offset >> config.base.min_align_shift));
    }
    pas_log("\n");
    
    pas_log(" end bits: ");
    for (offset = 0;
         offset < config.base.page_size;
         offset += pas_page_base_config_min_align(config.base)) {
        pas_log("%d", pas_bitvector_get(pas_bitfit_page_object_end_bits(page, config),
                                        offset >> config.base.min_align_shift));
    }
    pas_log("\n");
    
    if (mark_begin_offset == mark_end_offset)
        return;
    PAS_ASSERT(mark_end_offset > mark_begin_offset);
    pas_log("           ");
    for (offset = 0;
         offset < config.base.page_size;
         offset += pas_page_base_config_min_align(config.base)) {
        if (offset >= mark_begin_offset && offset < mark_end_offset)
            pas_log("^");
        else
            pas_log(" ");
    }
    pas_log("\n");
}

PAS_NO_RETURN void pas_bitfit_page_deallocation_did_fail(
    pas_bitfit_page* page,
    pas_bitfit_page_config_kind config_kind,
    uintptr_t begin,
    uintptr_t offset,
    const char* reason)
{
    pas_start_crash_logging();
    pas_log("Thread %p encountered bitfit alloaction error.\n", (void*)pthread_self());
    pas_log("Bits for page %p (%s):\n",
            page, pas_bitfit_page_config_kind_get_string(config_kind));
    pas_bitfit_page_log_bits(page, offset, offset + 1);
    pas_deallocation_did_fail(reason, begin);
}

bool pas_bitfit_page_for_each_live_object(
    pas_bitfit_page* page,
    pas_bitfit_page_for_each_live_object_callback callback,
    void* arg)
{
    const pas_bitfit_page_config* config_ptr;
    pas_bitfit_page_config config;
    void* boundary;
    pas_bitfit_view* view;
    size_t begin;
    size_t end;
    size_t offset;

    view = pas_compact_atomic_bitfit_view_ptr_load(&page->owner);
    config_ptr = pas_bitfit_page_config_kind_get_config(
        pas_compact_bitfit_directory_ptr_load_non_null(
            &view->directory)->config_kind);
    config = *config_ptr;

    boundary = pas_bitfit_page_boundary(page, config);

    begin = pas_bitfit_page_offset_to_first_object(config);
    end = pas_bitfit_page_offset_to_end_of_last_object(config);

    for (offset = begin; offset < end; offset += pas_page_base_config_min_align(config.base)) {
        size_t end_offset;
        
        if (pas_bitvector_get(pas_bitfit_page_free_bits(page),
                              offset >> config.base.min_align_shift))
            continue;

        for (end_offset = offset;
             end_offset < end && !pas_bitvector_get(pas_bitfit_page_object_end_bits(page, config),
                                                    end_offset >> config.base.min_align_shift);
             end_offset += pas_page_base_config_min_align(config.base));

        PAS_ASSERT(end_offset >= offset);
        PAS_ASSERT(end_offset < end);

        if (!callback((uintptr_t)boundary + offset,
                      end_offset - offset + pas_page_base_config_min_align(config.base),
                      arg))
            return false;

        offset = end_offset;
    }

    return true;
}

typedef struct {
    const pas_bitfit_page_config* config;
    uintptr_t boundary;
    pas_page_granule_use_count my_use_counts[PAS_MAX_GRANULES];
} verify_for_each_object_data;

static bool verify_for_each_object_callback(uintptr_t begin,
                                            size_t size,
                                            void* arg)
{
    verify_for_each_object_data* data;

    data = arg;

    pas_page_granule_increment_uses_for_range(
        data->my_use_counts,
        begin - data->boundary,
        begin - data->boundary + size,
        data->config->base.page_size,
        data->config->base.granule_size);

    return true;
}

void pas_bitfit_page_verify(pas_bitfit_page* page)
{
    size_t begin;
    size_t end;
    size_t offset;
    const pas_bitfit_page_config* config_ptr;
    pas_bitfit_page_config config;
    size_t num_live_bits;
    verify_for_each_object_data data;
    bool for_each_result;
    size_t index;
    pas_page_granule_use_count* use_counts;
    uintptr_t start_of_payload;
    uintptr_t end_of_payload;
    
    config_ptr = pas_bitfit_page_get_config(page);
    config = *config_ptr;

    begin = pas_bitfit_page_offset_to_first_object(config);
    end = pas_bitfit_page_offset_to_end_of_last_object(config);

    num_live_bits = 0;
    
    for (offset = begin; offset < end; offset += pas_page_base_config_min_align(config.base)) {
        if (!pas_bitvector_get(pas_bitfit_page_free_bits(page),
                               offset >> config.base.min_align_shift))
            num_live_bits++;
    }

    PAS_ASSERT(num_live_bits == page->num_live_bits);

    if (config.base.page_size == config.base.granule_size)
        return;

    PAS_ASSERT(config.base.page_size / config.base.granule_size <= PAS_MAX_GRANULES);
    
    data.config = config_ptr;
    data.boundary = (uintptr_t)pas_bitfit_page_boundary(page, config);
    pas_zero_memory(data.my_use_counts, PAS_MAX_GRANULES);
    
    start_of_payload = config.page_object_payload_offset;
    end_of_payload = config.page_object_payload_offset + config.page_object_payload_size;
    
    pas_page_granule_increment_uses_for_range(
        data.my_use_counts, 0, start_of_payload,
        config.base.page_size, config.base.granule_size);
    pas_page_granule_increment_uses_for_range(
        data.my_use_counts, end_of_payload, config.base.page_size,
        config.base.page_size, config.base.granule_size);
    
    for_each_result = pas_bitfit_page_for_each_live_object(
        page, verify_for_each_object_callback, &data);
    PAS_ASSERT(for_each_result);

    use_counts = pas_bitfit_page_get_granule_use_counts(page, config);
    for (index = config.base.page_size / config.base.granule_size; index--;) {
        if (use_counts[index] == PAS_PAGE_GRANULE_DECOMMITTED)
            PAS_ASSERT(!data.my_use_counts[index]);
        else
            PAS_ASSERT(use_counts[index] == data.my_use_counts[index]);
    }
}

#endif /* LIBPAS_ENABLED */
