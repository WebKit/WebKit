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

#ifndef PAS_BITFIT_GLOBAL_SIZE_CLASS_H
#define PAS_BITFIT_GLOBAL_SIZE_CLASS_H

#include "pas_bitfit_size_class.h"
#include "pas_segmented_vector.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_global_directory;
struct pas_bitfit_global_size_class;
struct pas_magazine;
typedef struct pas_bitfit_global_directory pas_bitfit_global_directory;
typedef struct pas_bitfit_global_size_class pas_bitfit_global_size_class;
typedef struct pas_magazine pas_magazine;

PAS_DECLARE_SEGMENTED_VECTOR(pas_bitfit_global_size_class_biasing_vector,
                             pas_compact_atomic_bitfit_size_class_ptr,
                             4);

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_bitfit_global_size_class {
    pas_bitfit_size_class base;
    pas_bitfit_global_size_class_biasing_vector biasing_vector;
};

PAS_API pas_bitfit_global_size_class* pas_bitfit_global_size_class_create(
    unsigned size,
    pas_bitfit_global_directory* directory,
    pas_compact_atomic_bitfit_size_class_ptr* insertion_point);

PAS_API pas_bitfit_size_class*
pas_bitfit_global_size_class_select_for_magazine(pas_bitfit_global_size_class* size_class,
                                                 pas_magazine* magazine);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_GLOBAL_SIZE_CLASS_H */

