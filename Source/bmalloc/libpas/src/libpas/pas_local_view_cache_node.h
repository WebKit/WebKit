/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCAL_VIEW_CACHE_NODE_H
#define PAS_LOCAL_VIEW_CACHE_NODE_H

#include "pas_compact_segregated_size_directory_ptr.h"
#include "pas_compact_atomic_thread_local_cache_layout_node.h"
#include "pas_allocator_index.h"

PAS_BEGIN_EXTERN_C;

struct pas_local_view_cache_node;
typedef struct pas_local_view_cache_node pas_local_view_cache_node;

struct pas_local_view_cache_node {
    pas_compact_atomic_thread_local_cache_layout_node next;
    pas_compact_segregated_size_directory_ptr directory;
};

PAS_API pas_local_view_cache_node*
pas_local_view_cache_node_create(pas_segregated_size_directory* directory);

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_VIEW_CACHE_NODE_H */

