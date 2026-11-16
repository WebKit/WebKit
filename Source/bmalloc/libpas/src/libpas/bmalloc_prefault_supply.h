/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#ifndef BMALLOC_PREFAULT_SUPPLY_H
#define BMALLOC_PREFAULT_SUPPLY_H

#include "pas_config.h"

#if PAS_ENABLE_BMALLOC

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* A supply of ordinary bmalloc blocks that a helper thread has already written to, so that the
   thread taking one does not have to fault the memory in.

   This exists for JavaScriptCore's MarkedBlock: a heap that is ramping up asks for thousands of
   fresh blocks and pays a write fault for each one on the mutator thread. The blocks are ordinary
   bmalloc allocations and are freed with the ordinary bmalloc entrypoint, so nothing about how the
   memory is shared with the rest of the process changes; the only difference is which thread takes
   the fault.

   No blocks are kept ready until a client sets the block size and a nonzero target. */

#define BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS 64

/* How many blocks to keep ready, silently capped at BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS, or zero to
   disable the supply. Must be set after the block size and before the first take, and must not
   change afterwards. */
PAS_API extern unsigned bmalloc_prefault_supply_target;

/* How long the supply goes without anyone wanting a block before it hands the memory back and lets
   its thread exit. A later take starts a new one. */
PAS_API extern double bmalloc_prefault_supply_idle_timeout_in_milliseconds;

/* The one size served. Must be set before the target, and must not change afterwards. */
PAS_API void bmalloc_prefault_supply_set_block_size(size_t block_size);

/* A block of the configured size: prefaulted if the supply had one ready, allocated the ordinary
   way if it did not. NULL means the allocation failed, or that no block size has been set. Free it
   with the ordinary bmalloc entrypoint, exactly like a block that never came from here. */
PAS_API void* bmalloc_prefault_supply_try_allocate(void);

/* Frees everything being held. */
PAS_API void bmalloc_prefault_supply_scavenge(void);

/* How many blocks are ready right now. For tests. */
PAS_API unsigned bmalloc_prefault_supply_block_count(void);

/* Makes the filling thread behave as if the heap were exhausted, so that a test can reach the
   standing-down path without running the machine out of memory. */
PAS_API extern bool bmalloc_prefault_supply_allocation_should_fail_for_testing;

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* BMALLOC_PREFAULT_SUPPLY_H */
