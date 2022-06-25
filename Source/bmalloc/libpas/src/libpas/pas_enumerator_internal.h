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

#ifndef PAS_ENUMERATOR_INTERNAL_H
#define PAS_ENUMERATOR_INTERNAL_H

#include "pas_enumerator.h"
#include "pas_heap.h"
#include "pas_page_granule_use_count.h"

PAS_BEGIN_EXTERN_C;

PAS_API void pas_enumerator_record_page_payload_and_meta(pas_enumerator* enumerator,
                                                         uintptr_t page_boundary,
                                                         uintptr_t page_size,
                                                         uintptr_t granule_size,
                                                         pas_page_granule_use_count* use_counts,
                                                         uintptr_t payload_begin,
                                                         uintptr_t payload_end);

/* This goes over all heaps, and gives you a local copy of the heap struct. */
typedef bool (*pas_enumerator_heap_callback)(pas_enumerator* enumerator,
                                             pas_heap* heap,
                                             void* arg);

PAS_API bool pas_enumerator_for_each_heap(pas_enumerator* enumerator,
                                          pas_enumerator_heap_callback callback,
                                          void* arg);

PAS_END_EXTERN_C;

#endif /* PAS_ENUMERATOR_INTERNAL_H */


