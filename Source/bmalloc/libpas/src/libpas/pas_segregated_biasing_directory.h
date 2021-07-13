/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_BIASING_DIRECTORY_H
#define PAS_SEGREGATED_BIASING_DIRECTORY_H

#include "pas_biasing_directory.h"
#include "pas_compact_segregated_global_size_directory_ptr.h"
#include "pas_segregated_size_directory.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_biasing_directory;
typedef struct pas_segregated_biasing_directory pas_segregated_biasing_directory;

struct PAS_ALIGNED(sizeof(uintptr_t) * 2) pas_segregated_biasing_directory {
    pas_segregated_directory base;
    pas_biasing_directory biasing_base;
    pas_compact_segregated_global_size_directory_ptr parent_global;
};

static inline unsigned
pas_segregated_biasing_directory_magazine_index(pas_segregated_biasing_directory* directory)
{
    return pas_biasing_directory_magazine_index(&directory->biasing_base);
}

PAS_API pas_segregated_biasing_directory* pas_segregated_biasing_directory_create(
    pas_segregated_global_size_directory* parent_global, unsigned magazine_index);

PAS_API pas_segregated_view pas_segregated_biasing_directory_take_first_eligible(
    pas_segregated_biasing_directory* directory,
    pas_segregated_global_size_directory* parent_global);

PAS_API pas_page_sharing_pool_take_result
pas_segregated_biasing_directory_take_last_unused(
    pas_segregated_biasing_directory* directory);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_BIASING_DIRECTORY_H */

