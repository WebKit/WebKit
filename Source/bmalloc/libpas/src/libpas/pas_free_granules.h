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

#ifndef PAS_FREE_GRANULES_H
#define PAS_FREE_GRANULES_H

#include "pas_bitvector.h"
#include "pas_config.h"
#include "pas_lock.h"
#include "pas_page_granule_use_count.h"

PAS_BEGIN_EXTERN_C;

struct pas_deferred_decommit_log;
struct pas_free_granules;
struct pas_page_base;
struct pas_page_base_config;
typedef struct pas_deferred_decommit_log pas_deferred_decommit_log;
typedef struct pas_free_granules pas_free_granules;
typedef struct pas_page_base pas_page_base;
typedef struct pas_page_base_config pas_page_base_config;

struct pas_free_granules {
    unsigned free_granules[PAS_BITVECTOR_NUM_WORDS(PAS_MAX_GRANULES)];
    size_t num_free_granules;
    size_t num_already_decommitted_granules;
};

/* This must be done under the page's lock (ownership lock for most pages or the page's biased
   lock for segregated exclusive). */
PAS_API void pas_free_granules_compute_and_mark_decommitted(pas_free_granules* free_granules,
                                                            pas_page_granule_use_count* use_counts,
                                                            size_t num_granules);

PAS_API void pas_free_granules_unmark_decommitted(pas_free_granules* free_granules,
                                                  pas_page_granule_use_count* use_count,
                                                  size_t num_granules);

static inline bool pas_free_granules_is_free(pas_free_granules* free_granules,
                                             size_t index)
{
    PAS_ASSERT(index < PAS_MAX_GRANULES);
    return pas_bitvector_get(free_granules->free_granules, index);
}

/* It's the caller's responsibility to tell the log about what locks to acquire. */
PAS_API void pas_free_granules_decommit_after_locking_range(pas_free_granules* free_granules,
                                                            pas_page_base* page,
                                                            pas_deferred_decommit_log* log,
                                                            pas_lock* commit_lock,
                                                            const pas_page_base_config* page_config,
                                                            pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_FREE_GRANULES_H */

