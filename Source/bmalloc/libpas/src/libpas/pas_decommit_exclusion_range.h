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

#ifndef PAS_DECOMMIT_EXCLUSION_RANGE_H
#define PAS_DECOMMIT_EXCLUSION_RANGE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_decommit_exclusion_range;
typedef struct pas_decommit_exclusion_range pas_decommit_exclusion_range;

struct pas_decommit_exclusion_range {
    uintptr_t start_of_possible_decommit;
    uintptr_t end_of_possible_decommit;
};

static inline bool pas_decommit_exclusion_range_is_empty(pas_decommit_exclusion_range range)
{
    return range.start_of_possible_decommit == range.end_of_possible_decommit;
}

static inline bool pas_decommit_exclusion_range_is_contiguous(pas_decommit_exclusion_range range)
{
    return range.start_of_possible_decommit < range.end_of_possible_decommit;
}

/* This means that the set of things that could be decommitted is anything before end_of_possible_decommit
   (exclusive) and after start_of_possible_decommit (inclusive). */
static inline bool pas_decommit_exclusion_range_is_inverted(pas_decommit_exclusion_range range)
{
    return range.start_of_possible_decommit > range.end_of_possible_decommit;
}

static inline pas_decommit_exclusion_range
pas_decommit_exclusion_range_create_inverted(pas_decommit_exclusion_range range)
{
    pas_decommit_exclusion_range result;
    result.start_of_possible_decommit = range.end_of_possible_decommit;
    result.end_of_possible_decommit = range.start_of_possible_decommit;
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_DECOMMIT_EXCLUSION_RANGE_H */

