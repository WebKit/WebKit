/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FastMallocAlignedMemoryAllocator.h"

#include "MarkedBlock.h"
#include "Options.h"
#include "VM.h"
#include <mutex>
#include <wtf/FastMalloc.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(LIBPAS)
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include <bmalloc/js_marked_block_heap.h>
#include <bmalloc/js_marked_block_heap_prefault_supply.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

namespace JSC {

#if !ENABLE(MALLOC_HEAP_BREAKDOWN)

#if USE(LIBPAS)

static_assert(MarkedBlock::blockSize == JS_MARKED_BLOCK_SIZE);

static bool warmUpMarkedBlocksIsEnabled()
{
    // Mini mode trades throughput for footprint, which is the opposite bargain.
    return Options::useWarmUpMarkedBlocks() && Options::warmUpMarkedBlockCount() && !VM::isInMiniMode();
}

static void configureWarmUpSupply()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        js_marked_block_heap_prefault_supply_idle_timeout_in_milliseconds = Options::warmUpMarkedBlockIdleTimeout() * 1000;
        js_marked_block_heap_prefault_supply_target = warmUpMarkedBlocksIsEnabled() ? Options::warmUpMarkedBlockCount() : 0;
    });
}

#endif

bool warmUpMarkedBlocksAreEnabledForTesting()
{
#if USE(LIBPAS)
    configureWarmUpSupply();
    return !!js_marked_block_heap_prefault_supply_target;
#else
    return false;
#endif
}

unsigned warmUpMarkedBlockCountForTesting()
{
#if USE(LIBPAS)
    configureWarmUpSupply();
    return js_marked_block_heap_prefault_supply_block_count();
#else
    return 0;
#endif
}

void setWarmUpMarkedBlockAllocationShouldFailForTesting(bool shouldFail)
{
#if USE(LIBPAS)
    // Read by the libpas filling thread, written here by whichever thread runs the test.
    __atomic_store_n(&js_marked_block_heap_prefault_supply_allocation_should_fail_for_testing, shouldFail, __ATOMIC_RELAXED);
#else
    UNUSED_PARAM(shouldFail);
#endif
}

#else // ENABLE(MALLOC_HEAP_BREAKDOWN)

bool warmUpMarkedBlocksAreEnabledForTesting() { return false; }
unsigned warmUpMarkedBlockCountForTesting() { return 0; }
void setWarmUpMarkedBlockAllocationShouldFailForTesting(bool) { }

#endif

FastMallocAlignedMemoryAllocator::FastMallocAlignedMemoryAllocator()
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    : m_heap("WebKit FastMallocAlignedMemoryAllocator")
#endif
{
}

FastMallocAlignedMemoryAllocator::~FastMallocAlignedMemoryAllocator() = default;

void* FastMallocAlignedMemoryAllocator::tryAllocateAlignedMemory(size_t alignment, size_t size)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.memalign(alignment, size, true);
#elif USE(LIBPAS)
    // freeAlignedMemory has only the pointer to go on, so it cannot tell which heap a block came
    // from. Serving some other shape from somewhere else would make the free path ambiguous.
    RELEASE_ASSERT(alignment == MarkedBlock::blockSize && size == MarkedBlock::blockSize);
    if (warmUpMarkedBlocksIsEnabled()) {
        configureWarmUpSupply();
        return js_marked_block_heap_prefault_supply_try_allocate();
    }
    return js_marked_block_heap_try_allocate();
#else
    return tryFastCompactAlignedMalloc(alignment, size);
#endif
}

void FastMallocAlignedMemoryAllocator::freeAlignedMemory(void* basePtr)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.free(basePtr);
#elif USE(LIBPAS)
    js_marked_block_heap_deallocate(basePtr);
#else
    fastFree(basePtr);
#endif
}

void FastMallocAlignedMemoryAllocator::dump(PrintStream& out) const
{
    out.print("FastMalloc");
}

void* FastMallocAlignedMemoryAllocator::tryAllocateMemory(size_t size)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.malloc(size);
#else
    return FastCompactMalloc::tryMalloc(size);
#endif
}

void FastMallocAlignedMemoryAllocator::freeMemory(void* pointer)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.free(pointer);
#else
    FastCompactMalloc::free(pointer);
#endif
}

void* FastMallocAlignedMemoryAllocator::tryReallocateMemory(void* pointer, size_t size)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.realloc(pointer, size);
#else
    return FastCompactMalloc::tryRealloc(pointer, size);
#endif
}

} // namespace JSC
