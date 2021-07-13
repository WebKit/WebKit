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

#ifndef PAS_BIASING_DIRECTORY_INLINES_H
#define PAS_BIASING_DIRECTORY_INLINES_H

#include "pas_biasing_directory.h"
#include "pas_bitfit_biasing_directory.h"
#include "pas_segregated_biasing_directory.h"

PAS_BEGIN_EXTERN_C;

static inline pas_segregated_biasing_directory* pas_unwrap_segregated_biasing_directory(
    pas_biasing_directory* directory)
{
    PAS_ASSERT(pas_is_segregated_biasing_directory(directory));
    return (pas_segregated_biasing_directory*)(
        (uintptr_t)directory - PAS_OFFSETOF(pas_segregated_biasing_directory, biasing_base));
}

static inline pas_bitfit_biasing_directory* pas_unwrap_bitfit_biasing_directory(
    pas_biasing_directory* directory)
{
    PAS_ASSERT(pas_is_bitfit_biasing_directory(directory));
    return (pas_bitfit_biasing_directory*)(
        (uintptr_t)directory - PAS_OFFSETOF(pas_bitfit_biasing_directory, biasing_base));
}

PAS_END_EXTERN_C;

#endif /* PAS_BIASING_DIRECTORY_INLINES_H */

