/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_segregated_page_config.h"

#include "pas_config.h"
#include "pas_page_malloc.h"
#include "pas_segregated_page.h"

bool pas_segregated_page_config_do_validate = false;

bool pas_small_segregated_page_config_variant_is_enabled_override =
    PAS_USE_SMALL_SEGREGATED_OVERRIDE;
bool pas_medium_segregated_page_config_variant_is_enabled_override =
    PAS_USE_MEDIUM_SEGREGATED_OVERRIDE;

void pas_segregated_page_config_validate(const pas_segregated_page_config* config)
{
    if (!pas_segregated_page_config_do_validate)
        return;

    PAS_ASSERT(config->exclusive_payload_size <= config->base.page_size);
    PAS_ASSERT(config->shared_payload_size <= config->base.page_size);
    PAS_ASSERT(pas_segregated_page_config_min_align(*config) < config->base.max_object_size);
    PAS_ASSERT(config->exclusive_payload_offset < config->base.page_size);
    PAS_ASSERT(config->shared_payload_offset < config->base.page_size);
    PAS_ASSERT(config->base.max_object_size <= config->exclusive_payload_size);
    PAS_ASSERT(config->base.max_object_size <= config->shared_payload_size);
    PAS_ASSERT(config->num_alloc_bits >=
               (pas_segregated_page_config_payload_end_offset_for_role(
                   *config, pas_segregated_page_shared_role) >> config->base.min_align_shift));
    PAS_ASSERT(pas_segregated_page_config_payload_end_offset_for_role(
                   *config, pas_segregated_page_exclusive_role) <= config->base.page_size);
    PAS_ASSERT(!(config->base.page_size % config->base.granule_size));
    PAS_ASSERT(config->base.page_size >= config->base.granule_size);
    PAS_ASSERT(!(config->base.granule_size % pas_page_malloc_alignment()));
    PAS_ASSERT(config->base.granule_size >= pas_page_malloc_alignment());
    if (config->base.page_size > config->base.granule_size)
        PAS_ASSERT(((config->base.granule_size >> config->base.min_align_shift) + 1)
                   < PAS_PAGE_GRANULE_DECOMMITTED);
}

#endif /* LIBPAS_ENABLED */
