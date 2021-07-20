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

#ifndef PAS_PAGE_SHARING_PARTICIPANT_KIND_H
#define PAS_PAGE_SHARING_PARTICIPANT_KIND_H

#include "pas_segregated_directory_kind.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_page_sharing_participant_kind {
    pas_page_sharing_participant_null,
    pas_page_sharing_participant_segregated_shared_page_directory,
    pas_page_sharing_participant_segregated_size_directory,
    pas_page_sharing_participant_bitfit_directory,
    pas_page_sharing_participant_biasing_directory,
    pas_page_sharing_participant_large_sharing_pool
};

typedef enum pas_page_sharing_participant_kind pas_page_sharing_participant_kind;

#define PAS_PAGE_SHARING_PARTICIPANT_KIND_MASK ((uintptr_t)7)

static inline pas_page_sharing_participant_kind
pas_page_sharing_participant_kind_select_for_segregated_directory(
    pas_segregated_directory_kind directory_kind)
{
    switch (directory_kind) {
    case pas_segregated_global_size_directory_kind:
        return pas_page_sharing_participant_segregated_size_directory;
    case pas_segregated_shared_page_directory_kind:
        return pas_page_sharing_participant_segregated_shared_page_directory;
    case pas_segregated_biasing_directory_kind:
        PAS_ASSERT(!"Should not use segregated biasing directories as sharing participants except "
                   "via the biasing directory base");
        return pas_page_sharing_participant_null;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_page_sharing_participant_null;
}

static inline const char*
pas_page_sharing_participant_kind_get_string(pas_page_sharing_participant_kind kind)
{
    switch (kind) {
    case pas_page_sharing_participant_null:
        return "null";
    case pas_page_sharing_participant_segregated_shared_page_directory:
        return "segregated_shared_page_directory";
    case pas_page_sharing_participant_segregated_size_directory:
        return "segregated_size_directory";
    case pas_page_sharing_participant_bitfit_directory:
        return "bitfit_directory";
    case pas_page_sharing_participant_biasing_directory:
        return "biasing_directory";
    case pas_page_sharing_participant_large_sharing_pool:
        return "large_sharing_pool";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_SHARING_PARTICIPANT_KIND_H */

