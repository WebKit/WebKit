/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_EXCLUSIVE_VIEW_OWNERSHIP_KIND_H
#define PAS_SEGREGATED_EXCLUSIVE_VIEW_OWNERSHIP_KIND_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* This is really two flags -- owned and biased.

   Ownership means that the page pointer points to a committed and fully constructed
   page. The ownership lock is taken when this state changes. So if you grab the ownership
   lock and observe the ownership bit to be set, then you know that you can dereference
   the page pointer and access the page knowing that it's fully initialized. But that
   requires continuing to hold the ownership lock.

   Biasing means that there is a biasing_view that has taken over the exclusive view and
   so the exclusive view isn't really acting as a member of its directory (it should be
   ineligible in that directory and ignored). */
enum __attribute__((__packed__)) pas_segregated_exclusive_view_ownership_kind {
    pas_segregated_exclusive_view_not_owned,
    pas_segregated_exclusive_view_owned,
    pas_segregated_exclusive_view_not_owned_and_biased,
    pas_segregated_exclusive_view_owned_and_biased,
};

typedef enum pas_segregated_exclusive_view_ownership_kind pas_segregated_exclusive_view_ownership_kind;

static inline bool pas_segregated_exclusive_view_ownership_kind_is_owned(
    pas_segregated_exclusive_view_ownership_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_not_owned:
    case pas_segregated_exclusive_view_not_owned_and_biased:
        return false;
    case pas_segregated_exclusive_view_owned:
    case pas_segregated_exclusive_view_owned_and_biased:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline bool pas_segregated_exclusive_view_ownership_kind_is_biased(
    pas_segregated_exclusive_view_ownership_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_not_owned:
    case pas_segregated_exclusive_view_owned:
        return false;
    case pas_segregated_exclusive_view_not_owned_and_biased:
    case pas_segregated_exclusive_view_owned_and_biased:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline pas_segregated_exclusive_view_ownership_kind
pas_segregated_exclusive_view_ownership_kind_with_owned(
    pas_segregated_exclusive_view_ownership_kind kind,
    bool is_owned)
{
    switch (kind) {
    case pas_segregated_exclusive_view_not_owned:
    case pas_segregated_exclusive_view_owned:
        return is_owned
            ? pas_segregated_exclusive_view_owned
            : pas_segregated_exclusive_view_not_owned;
    case pas_segregated_exclusive_view_not_owned_and_biased:
    case pas_segregated_exclusive_view_owned_and_biased:
        return is_owned
            ? pas_segregated_exclusive_view_owned_and_biased
            : pas_segregated_exclusive_view_not_owned_and_biased;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_segregated_exclusive_view_not_owned;
}

static inline pas_segregated_exclusive_view_ownership_kind
pas_segregated_exclusive_view_ownership_kind_with_biased(
    pas_segregated_exclusive_view_ownership_kind kind,
    bool is_biased)
{
    switch (kind) {
    case pas_segregated_exclusive_view_not_owned:
    case pas_segregated_exclusive_view_not_owned_and_biased:
        return is_biased
            ? pas_segregated_exclusive_view_not_owned_and_biased
            : pas_segregated_exclusive_view_not_owned;
    case pas_segregated_exclusive_view_owned:
    case pas_segregated_exclusive_view_owned_and_biased:
        return is_biased
            ? pas_segregated_exclusive_view_owned_and_biased
            : pas_segregated_exclusive_view_owned;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_segregated_exclusive_view_not_owned;
}

static inline const char* pas_segregated_exclusive_view_ownership_kind_get_string(
    pas_segregated_exclusive_view_ownership_kind kind)
{
    switch (kind) {
    case pas_segregated_exclusive_view_not_owned:
        return "not_owned";
    case pas_segregated_exclusive_view_owned:
        return "owned";
    case pas_segregated_exclusive_view_not_owned_and_biased:
        return "not_owned_and_biased";
    case pas_segregated_exclusive_view_owned_and_biased:
        return "owned_and_biased";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_EXCLUSIVE_VIEW_OWNERSHIP_KIND_H */


