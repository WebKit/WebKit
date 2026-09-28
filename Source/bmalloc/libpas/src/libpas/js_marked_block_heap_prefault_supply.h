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

#ifndef JS_MARKED_BLOCK_HEAP_PREFAULT_SUPPLY_H
#define JS_MARKED_BLOCK_HEAP_PREFAULT_SUPPLY_H

#include "pas_config.h"

#if PAS_ENABLE_JS_MARKED_BLOCK && PAS_ENABLE_BMALLOC

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* A supply of blocks that a helper thread has already written to, so that the thread taking one does
   not have to fault the memory in.

   This exists for JavaScriptCore's MarkedBlock: a heap that is ramping up asks for thousands of fresh
   blocks and pays a write fault for each one on the mutator thread. A block from here is an ordinary
   block of whichever heap the supply was pointed at, and is freed the ordinary way; the only
   difference is which thread takes the fault.

   No blocks are kept ready until a client sets a nonzero target. */

#define JS_MARKED_BLOCK_HEAP_PREFAULT_SUPPLY_MAX_BLOCKS 64

/* How many blocks to keep ready, silently capped at JS_MARKED_BLOCK_HEAP_PREFAULT_SUPPLY_MAX_BLOCKS,
   or zero to disable the supply. Must be set before the first take, and must not change afterwards. */
PAS_API extern unsigned js_marked_block_heap_prefault_supply_target;

/* How long the supply goes without anyone wanting a block before it hands the memory back and lets
   its thread exit. A later take starts a new one. */
PAS_API extern double js_marked_block_heap_prefault_supply_idle_timeout_in_milliseconds;

/* A block of JS_MARKED_BLOCK_SIZE bytes, aligned to that size: prefaulted if the supply had one
   ready, allocated the ordinary way if it did not. NULL means the allocation failed. Free it exactly
   like a block that never came from here. */
PAS_API void* js_marked_block_heap_prefault_supply_try_allocate(void);

/* Frees everything being held. */
PAS_API void js_marked_block_heap_prefault_supply_scavenge(void);

/* How many blocks are ready right now. For tests. */
PAS_API unsigned js_marked_block_heap_prefault_supply_block_count(void);

/* Makes the filling thread behave as if the heap were exhausted, so that a test can reach the
   standing-down path without running the machine out of memory. */
PAS_API extern bool js_marked_block_heap_prefault_supply_allocation_should_fail_for_testing;

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JS_MARKED_BLOCK && PAS_ENABLE_BMALLOC */

#endif /* JS_MARKED_BLOCK_HEAP_PREFAULT_SUPPLY_H */
