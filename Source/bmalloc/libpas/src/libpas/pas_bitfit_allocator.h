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

#ifndef PAS_BITFIT_ALLOCATOR_H
#define PAS_BITFIT_ALLOCATOR_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_allocator;
struct pas_bitfit_size_class;
struct pas_bitfit_page;
struct pas_bitfit_size_class;
struct pas_bitfit_view;
typedef struct pas_bitfit_allocator pas_bitfit_allocator;
typedef struct pas_bitfit_size_class pas_bitfit_size_class;
typedef struct pas_bitfit_page pas_bitfit_page;
typedef struct pas_bitfit_size_class pas_bitfit_size_class;
typedef struct pas_bitfit_view pas_bitfit_view;

struct pas_bitfit_allocator {
    pas_bitfit_size_class* size_class;
    pas_bitfit_view* view;
};

PAS_API void pas_bitfit_allocator_construct(pas_bitfit_allocator* allocator,
                                            pas_bitfit_size_class* size_class);

PAS_API void pas_bitfit_allocator_stop(pas_bitfit_allocator* allocator);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_ALLOCATOR_H */

