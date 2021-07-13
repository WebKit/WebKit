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

#ifndef PAS_BITFIT_BIASING_DIRECTORY_H
#define PAS_BITFIT_BIASING_DIRECTORY_H

#include "pas_biasing_directory.h"
#include "pas_bitfit_directory.h"
#include "pas_compact_bitfit_global_directory_ptr.h"
#include "pas_page_sharing_pool_take_result.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_biasing_directory;
typedef struct pas_bitfit_biasing_directory pas_bitfit_biasing_directory;

struct PAS_ALIGNED(sizeof(uintptr_t) * 2) pas_bitfit_biasing_directory {
    pas_bitfit_directory base;
    pas_biasing_directory biasing_base;
    pas_compact_bitfit_global_directory_ptr parent_global;
};

PAS_API pas_bitfit_biasing_directory* pas_bitfit_biasing_directory_create(
    pas_bitfit_global_directory* parent_global,
    unsigned magazine_index);

PAS_API pas_page_sharing_pool_take_result pas_bitfit_biasing_directory_take_last_unused(
    pas_bitfit_biasing_directory* directory);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_BIASING_DIRECTORY_H */


