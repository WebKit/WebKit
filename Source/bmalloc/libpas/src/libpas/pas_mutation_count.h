/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_MUTATION_COUNT_H
#define PAS_MUTATION_COUNT_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_mutation_count;
typedef struct pas_mutation_count pas_mutation_count;

struct pas_mutation_count {
    uintptr_t count;
};

#define PAS_MUTATION_COUNT_INITIALIZER ((pas_mutation_count){ .count = 0 })

#define PAS_MUTATION_COUNT_MUTATING_BIT ((uintptr_t)1)

/* Always call the start_mutating/stop_mutating functions while holding a lock. */

static inline void pas_mutation_count_start_mutating(pas_mutation_count* mutation_count)
{
    pas_compiler_fence();
    PAS_ASSERT(!(mutation_count->count & PAS_MUTATION_COUNT_MUTATING_BIT));
    mutation_count->count++;
    PAS_ASSERT(mutation_count->count & PAS_MUTATION_COUNT_MUTATING_BIT);
    pas_fence();
}

static inline void pas_mutation_count_stop_mutating(pas_mutation_count* mutation_count)
{
    pas_fence();
    PAS_ASSERT(mutation_count->count & PAS_MUTATION_COUNT_MUTATING_BIT);
    mutation_count->count++;
    PAS_ASSERT(!(mutation_count->count & PAS_MUTATION_COUNT_MUTATING_BIT));
    pas_compiler_fence();
}

static inline bool pas_mutation_count_is_mutating(pas_mutation_count saved_mutation_count)
{
    return saved_mutation_count.count & PAS_MUTATION_COUNT_MUTATING_BIT;
}

static inline bool pas_mutation_count_matches_with_dependency(pas_mutation_count* mutation_count,
                                                              pas_mutation_count saved_mutation_count,
                                                              uintptr_t dependency)
{
    pas_compiler_fence();
    return mutation_count[pas_depend(dependency)].count == saved_mutation_count.count;
}

static inline unsigned pas_mutation_count_depend(pas_mutation_count saved_mutation_count)
{
    return pas_depend((unsigned long)saved_mutation_count.count);
}

PAS_END_EXTERN_C;

#endif /* PAS_MUTATION_COUNT_H */

