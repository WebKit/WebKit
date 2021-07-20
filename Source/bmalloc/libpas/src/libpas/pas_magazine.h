/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_MAGAZINE_H
#define PAS_MAGAZINE_H

#include "pas_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_magazine;
struct pas_thread_local_cache_node;
typedef struct pas_magazine pas_magazine;
typedef struct pas_thread_local_cache_node pas_thread_local_cache_node;

struct pas_magazine {
    /* This is the lock that we bias pages to.
     
       For lock ordering, we say that:
    
       - Magazine locks are acquired before page ownership locks.
    
       - Magazine locks are acquired in pointer-as-integer order relative to one another. */
    pas_lock lock;

    /* This gets used as an index in biasing directories. */
    unsigned magazine_index;
};

PAS_API pas_magazine* pas_magazine_create(unsigned magazine_index);

PAS_END_EXTERN_C;

#endif /* PAS_MAGAZINE_H */

