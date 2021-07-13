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

#ifndef PAS_ALL_BIASING_DIRECTORIES_H
#define PAS_ALL_BIASING_DIRECTORIES_H

#include "pas_biasing_directory.h"
#include "pas_compact_atomic_biasing_directory_ptr.h"
#include "pas_segmented_vector.h"

PAS_BEGIN_EXTERN_C;

PAS_DECLARE_SEGMENTED_VECTOR(pas_all_biasing_directories_active_bitvector,
                             unsigned,
                             4);
PAS_DECLARE_SEGMENTED_VECTOR(pas_all_biasing_directories_vector,
                             pas_compact_atomic_biasing_directory_ptr,
                             4);

PAS_API extern pas_all_biasing_directories_active_bitvector pas_all_biasing_directories_active_bitvector_instance;
PAS_API extern pas_all_biasing_directories_vector pas_all_biasing_directories_vector_instance;

PAS_API void pas_all_biasing_directories_append(pas_biasing_directory* directory);

PAS_API bool pas_all_biasing_directories_activate(pas_biasing_directory* directory);

PAS_API bool pas_all_biasing_directories_scavenge(void);

PAS_END_EXTERN_C;

#endif /* PAS_ALL_BIASING_DIRECTORIES_H */

