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

#ifndef PAS_SEGREGATED_VIEW_KIND_H
#define PAS_SEGREGATED_VIEW_KIND_H

#include "pas_segregated_page_role.h"

PAS_BEGIN_EXTERN_C;

enum pas_segregated_view_kind {
    pas_segregated_exclusive_view_kind,
    pas_segregated_ineligible_exclusive_view_kind, /* Really the same type as exclusive_view, but this
                                                      lets us use the kind bits of a view to say if
                                                      eligibility has been noted. */
    pas_segregated_shared_view_kind,
    pas_segregated_shared_handle_kind,
    pas_segregated_partial_view_kind,
    pas_segregated_size_directory_view_kind
};

typedef enum pas_segregated_view_kind pas_segregated_view_kind;

#define PAS_SEGREGATED_VIEW_KIND_MASK ((uintptr_t)7)

static inline char pas_segregated_view_kind_get_character_code(pas_segregated_view_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return 'E';
    case pas_segregated_shared_view_kind:
        return 'S';
    case pas_segregated_shared_handle_kind:
        return 'H';
    case pas_segregated_partial_view_kind:
        return 'P';
    case pas_segregated_size_directory_view_kind:
        return 'S';
    }
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static inline const char* pas_segregated_view_kind_get_string(pas_segregated_view_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_kind:
        return "exclusive_view";
    case pas_segregated_ineligible_exclusive_view_kind:
        return "ineligible_exclusive_view";
    case pas_segregated_shared_view_kind:
        return "shared_view";
    case pas_segregated_shared_handle_kind:
        return "shared_handle";
    case pas_segregated_partial_view_kind:
        return "partial_view";
    case pas_segregated_size_directory_view_kind:
        return "size_directory";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static inline bool pas_segregated_view_kind_is_some_exclusive(pas_segregated_view_kind kind)
{
    return kind <= pas_segregated_ineligible_exclusive_view_kind;
}

static inline pas_segregated_page_role
pas_segregated_view_kind_get_role_for_owner(pas_segregated_view_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_page_exclusive_role;
    case pas_segregated_shared_handle_kind:
        return pas_segregated_page_shared_role;
    default:
        PAS_ASSERT(!"Should not be reached");
        return pas_segregated_page_exclusive_role;
    }
}

static inline pas_segregated_page_role
pas_segregated_view_kind_get_role_for_allocator(pas_segregated_view_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_page_exclusive_role;
    case pas_segregated_partial_view_kind:
        return pas_segregated_page_shared_role;
    default:
        PAS_ASSERT(!"Should not be reached");
        return pas_segregated_page_exclusive_role;
    }
}

static inline bool pas_segregated_view_kind_can_become_empty(pas_segregated_view_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
    case pas_segregated_shared_view_kind:
        return true;
    case pas_segregated_partial_view_kind:
        return false;
    case pas_segregated_shared_handle_kind:
    case pas_segregated_size_directory_view_kind:
        PAS_ASSERT(!"Should not be reached");
        return false;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_VIEW_KIND_H */

