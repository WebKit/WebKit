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

#ifndef PAS_INTRINSIC_HEAP_SUPPORT_H
#define PAS_INTRINSIC_HEAP_SUPPORT_H

#include "pas_allocator_index.h"
#include "pas_compact_atomic_segregated_size_directory_ptr.h"
#include "pas_internal_config.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_intrinsic_heap_support;
typedef struct pas_intrinsic_heap_support pas_intrinsic_heap_support;

struct pas_intrinsic_heap_support {
    pas_compact_atomic_segregated_size_directory_ptr index_to_size_directory[
        PAS_NUM_INTRINSIC_SIZE_CLASSES];
    pas_allocator_index index_to_allocator_index[PAS_NUM_INTRINSIC_SIZE_CLASSES];
#ifdef __cplusplus
    constexpr pas_intrinsic_heap_support(cpp_initialization_t)
        : index_to_size_directory { }
        , index_to_allocator_index { }
    {
        for (unsigned i = 0; i < PAS_NUM_INTRINSIC_SIZE_CLASSES; ++i) {
            index_to_size_directory[i] = PAS_COMPACT_ATOMIC_PTR_INITIALIZER;
            index_to_allocator_index[i] = 0;
        }
    }
#endif
};

#ifdef __cplusplus
#define PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER { cpp_initialization }
#else
#define PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER { \
        .index_to_size_directory = {[0 ... PAS_NUM_INTRINSIC_SIZE_CLASSES - 1] = \
                                        PAS_COMPACT_ATOMIC_PTR_INITIALIZER}, \
        .index_to_allocator_index = {[0 ... PAS_NUM_INTRINSIC_SIZE_CLASSES - 1] = 0}, \
    }
#endif

PAS_END_EXTERN_C;

#endif /* PAS_INTRINSIC_HEAP_SUPPORT_H */

