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

#ifndef PAS_EPOCH_H
#define PAS_EPOCH_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern bool pas_epoch_is_counter;
PAS_API extern uint64_t pas_current_epoch;

#define PAS_EPOCH_INVALID 0
#define PAS_EPOCH_MIN 1
#define PAS_EPOCH_MAX UINT64_MAX

/* This *may* simply return a new epoch each time you call it. Or it may return some coarser
   notion of epoch. The only requirement is that it proceeds monotonically, and even that
   requirement is a weak one - slight time travel is to be tolerated by callers.

   However: in reality this is monotonic time in nanoseconds. Lots of things are tuned for that
   fact.

   It's just that for testing purposes, we sometimes turn it into a counter and change the tuning
   accordingly. But that won't happen except in the tests that need it. */
PAS_API uint64_t pas_get_epoch(void);

PAS_END_EXTERN_C;

#endif /* PAS_EPOCH_H */

