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

#ifndef PAS_HEAP_REF_KIND_H
#define PAS_HEAP_REF_KIND_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_heap_ref_kind {
    /* This means we have a pas_heap_ref. */
    pas_normal_heap_ref_kind,

    /* This means we have a pas_primitive_heap_ref. */
    pas_primitive_heap_ref_kind,

    /* This means that we created a fake pas_heap_ref just to carry the heap and type around. */
    pas_fake_heap_ref_kind
};

typedef enum pas_heap_ref_kind pas_heap_ref_kind;

static inline const char* pas_heap_ref_kind_get_string(pas_heap_ref_kind kind)
{
    switch (kind) {
    case pas_normal_heap_ref_kind:
        return "normal";
    case pas_primitive_heap_ref_kind:
        return "primitive";
    case pas_fake_heap_ref_kind:
        return "fake";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_REF_KIND_H */

