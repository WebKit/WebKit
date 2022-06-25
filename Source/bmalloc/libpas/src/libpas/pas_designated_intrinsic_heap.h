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

#ifndef PAS_DESIGNATED_INTRINSIC_HEAP_H
#define PAS_DESIGNATED_INTRINSIC_HEAP_H

#include "pas_allocator_index.h"
#include "pas_heap_config.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_heap;
typedef struct pas_segregated_heap pas_segregated_heap;

enum pas_intrinsic_heap_designation_mode {
    pas_intrinsic_heap_is_not_designated,
    pas_intrinsic_heap_is_designated
};

typedef enum pas_intrinsic_heap_designation_mode pas_intrinsic_heap_designation_mode;

/* This can only be done once ever. Once you do it for an intrinsic heap you cannot do it for
   any other intrinsic heaps. It sets up the ability to use the fast size class lookup function. */
PAS_API void pas_designated_intrinsic_heap_initialize(pas_segregated_heap* heap,
                                                      pas_heap_config* config);

PAS_END_EXTERN_C;

#endif /* PAS_DESIGNATED_INTRINSIC_HEAP_H */

