/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_HEAP_PAGE_SHARING_MODE_H
#define PAS_SEGREGATED_HEAP_PAGE_SHARING_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_segregated_heap_page_sharing_mode {
    pas_segregated_heap_invalid_sharing_mode,
    pas_segregated_heap_do_not_share_pages,
    pas_segregated_heap_share_pages
};

typedef enum pas_segregated_heap_page_sharing_mode pas_segregated_heap_page_sharing_mode;

static inline bool
pas_segregated_heap_page_sharing_mode_does_any_sharing(
    pas_segregated_heap_page_sharing_mode mode)
{
    switch (mode) {
    case pas_segregated_heap_invalid_sharing_mode:
        PAS_ASSERT(!"Should not be reached");
        return false;
    case pas_segregated_heap_do_not_share_pages:
        return false;
    case pas_segregated_heap_share_pages_physically:
    case pas_segregated_heap_share_pages_virtually:
    case pas_segregated_heap_share_pages_virtually_and_physically:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline bool
pas_segregated_heap_page_sharing_mode_does_virtual_sharing(
    pas_segregated_heap_page_sharing_mode mode)
{
    switch (mode) {
    case pas_segregated_heap_invalid_sharing_mode:
        PAS_ASSERT(!"Should not be reached");
        return false;
    case pas_segregated_heap_do_not_share_pages:
    case pas_segregated_heap_share_pages_physically:
        return false;
    case pas_segregated_heap_share_pages_virtually:
    case pas_segregated_heap_share_pages_virtually_and_physically:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline bool
pas_segregated_heap_page_sharing_mode_does_physical_sharing(
    pas_segregated_heap_page_sharing_mode mode)
{
    switch (mode) {
    case pas_segregated_heap_invalid_sharing_mode:
        PAS_ASSERT(!"Should not be reached");
        return false;
    case pas_segregated_heap_do_not_share_pages:
    case pas_segregated_heap_share_pages_virtually:
        return false;
    case pas_segregated_heap_share_pages_physically:
    case pas_segregated_heap_share_pages_virtually_and_physically:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline const char*
pas_segregated_heap_page_sharing_mode_get_string(pas_segregated_heap_page_sharing_mode mode)
{
    switch (mode) {
    case pas_segregated_heap_invalid_sharing_mode:
        return "invalid_sharing_mode";
    case pas_segregated_heap_do_not_share_pages:
        return "do_not_share_pages";
    case pas_segregated_heap_share_pages_virtually:
        return "share_pages_virtually";
    case pas_segregated_heap_share_pages_physically:
        return "share_pages_physically";
    case pas_segregated_heap_share_pages_virtually_and_physically:
        return "share_pages_virtually_and_physically";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_HEAP_PAGE_SHARING_MODE_H */

