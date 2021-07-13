/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#ifndef PAS_LARGE_FREE_HEAP_CONFIG_H
#define PAS_LARGE_FREE_HEAP_CONFIG_H

#include "pas_aligned_allocator.h"
#include "pas_deallocator.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_free_heap_config;
typedef struct pas_large_free_heap_config pas_large_free_heap_config;

struct pas_large_free_heap_config {
    size_t type_size;
    
    /* This is the smallest alignment to which the sizes of objects are
       aligned. This isn't meaningful if type_size is 1. In that case, this
       should be 1 also.
       
       You don't actually have to request alignment that is at least as big
       as this. */
    size_t min_alignment;
    
    pas_aligned_allocator aligned_allocator;
    void* aligned_allocator_arg;
    
    pas_deallocator deallocator;
    void* deallocator_arg;
};

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_FREE_HEAP_CONFIG */

