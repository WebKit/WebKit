/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_THREAD_LOCAL_CACHE_NODE_H
#define PAS_THREAD_LOCAL_CACHE_NODE_H

#include "pas_config.h"
#include "pas_magazine.h"
#include "pas_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* Thread local caches have some data that cannot move when the TLC resizes and that should
   stick around in memory even if the TLC dies. It can be reused if some new TLC is born and
   needs that data. That's what the TLC node is for. */

struct pas_thread_local_cache;
struct pas_thread_local_cache_node;
typedef struct pas_thread_local_cache pas_thread_local_cache;
typedef struct pas_thread_local_cache_node pas_thread_local_cache_node;

struct pas_thread_local_cache_node {
    /* Used to track the free nodes. */
    pas_thread_local_cache_node* next_free;
    
    /* Used to track all nodes, free and allocated. */
    pas_thread_local_cache_node* next;
    
    pas_lock page_lock;
    
    pas_lock log_flush_lock;
    
    pas_thread_local_cache* cache;
};

PAS_API extern pas_thread_local_cache_node* pas_thread_local_cache_node_first_free;
PAS_API extern pas_thread_local_cache_node* pas_thread_local_cache_node_first;

/* These functions assume that the heap lock is held. */
PAS_API pas_thread_local_cache_node* pas_thread_local_cache_node_allocate(void);
PAS_API void pas_thread_local_cache_node_deallocate(pas_thread_local_cache_node* node);

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_NODE_H */

