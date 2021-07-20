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

#ifndef PAS_HEAP_KIND_H
#define PAS_HEAP_KIND_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* This is used for heap iteration and callbacks. It only includes heaps that are currently
   part of that. */
enum pas_heap_kind {
    pas_bootstrap_free_heap_kind,
    pas_compact_bootstrap_free_heap_kind,
    pas_large_utility_free_heap_kind,
    pas_immortal_heap_kind,
    pas_utility_heap_kind
};

typedef enum pas_heap_kind pas_heap_kind;

#define PAS_NUM_HEAP_KINDS 5

static inline const char* pas_heap_kind_get_string(pas_heap_kind kind)
{
    switch (kind) {
    case pas_bootstrap_free_heap_kind:
        return "bootstrap_free_heap";
    case pas_compact_bootstrap_free_heap_kind:
        return "compact_bootstrap_free_heap";
    case pas_large_utility_free_heap_kind:
        return "large_utility_free_heap";
    case pas_immortal_heap_kind:
        return "immortal_heap";
    case pas_utility_heap_kind:
        return "utility_heap";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_KIND_H */

