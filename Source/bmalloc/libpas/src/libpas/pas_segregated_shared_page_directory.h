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

#ifndef PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_H
#define PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_H

#include "pas_segregated_directory.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_shared_page_directory;
typedef struct pas_segregated_shared_page_directory pas_segregated_shared_page_directory;

struct pas_segregated_shared_page_directory {
    pas_segregated_directory base;
    pas_segregated_shared_page_directory* next;
};

#define PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_INITIALIZER(page_config, sharing_mode) \
    ((pas_segregated_shared_page_directory){ \
         .base = PAS_SEGREGATED_DIRECTORY_INITIALIZER( \
                     (page_config).kind, (sharing_mode), pas_segregated_shared_page_directory_kind), \
         .next = NULL \
     })

/* NOTE: It doesn't matter which kind of first_eligible we use. We just pick one. */
#define PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_FIRST_ELIGIBLE_KIND \
    pas_segregated_directory_first_eligible_but_not_tabled_kind

extern PAS_API unsigned pas_segregated_shared_page_directory_probability_of_ineligibility;

PAS_API pas_segregated_shared_view* pas_segregated_shared_page_directory_find_first_eligible(
    pas_segregated_shared_page_directory* directory,
    unsigned size,
    unsigned alignment,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_page_sharing_pool_take_result
pas_segregated_shared_page_directory_take_last_empty(
    pas_segregated_shared_page_directory* directory,
    pas_deferred_decommit_log* log,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_segregated_shared_page_directory_dump_reference(
    pas_segregated_shared_page_directory* directory,
    pas_stream* stream);

PAS_API void pas_segregated_shared_page_directory_dump_for_spectrum(
    pas_stream* stream, void* directory);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_H */

