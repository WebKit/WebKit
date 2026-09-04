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
#include <bmalloc/bmalloc_prefault_supply.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

namespace JSC {

#if !ENABLE(MALLOC_HEAP_BREAKDOWN)

#if USE(LIBPAS)

static bool warmUpMarkedBlocksIsEnabled()
{
    // Mini mode trades throughput for footprint, which is the opposite bargain.
    return Options::useWarmUpMarkedBlocks() && Options::warmUpMarkedBlockCount() && !VM::isInMiniMode();
}

static void configureWarmUpSupply()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        bmalloc_prefault_supply_set_block_size(MarkedBlock::blockSize);
        bmalloc_prefault_supply_idle_timeout_in_milliseconds = Options::warmUpMarkedBlockIdleTimeout() * 1000;
        bmalloc_prefault_supply_target = warmUpMarkedBlocksIsEnabled() ? Options::warmUpMarkedBlockCount() : 0;
    });
}

#endif

WarmUpMarkedBlockState warmUpMarkedBlockStateForTesting()
{
    WarmUpMarkedBlockState state;
#if USE(LIBPAS)
    configureWarmUpSupply();
    state.blockCount = bmalloc_prefault_supply_block_count();
    state.phase = bmalloc_prefault_supply_target ? WarmUpMarkedBlockPhase::Armed : WarmUpMarkedBlockPhase::Stopped;
#endif
    return state;
}

void setWarmUpMarkedBlockAllocationShouldFailForTesting(bool shouldFail)
{
#if USE(LIBPAS)
    // Read by the libpas filling thread, written here by whichever thread runs the test.
    __atomic_store_n(&bmalloc_prefault_supply_allocation_should_fail_for_testing, shouldFail, __ATOMIC_RELAXED);
#else
    UNUSED_PARAM(shouldFail);
#endif
}

#else // ENABLE(MALLOC_HEAP_BREAKDOWN)

WarmUpMarkedBlockState warmUpMarkedBlockStateForTesting() { return { }; }
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
#else

#if USE(LIBPAS)
    // MarkedBlock::tryCreate is the only caller today and always asks for a block-shaped region.
    // The guard keeps a future caller of some other size from being handed a block.
    if (alignment == MarkedBlock::blockSize && size == MarkedBlock::blockSize && warmUpMarkedBlocksIsEnabled()) {
        configureWarmUpSupply();
        return bmalloc_prefault_supply_try_allocate();
    }
#endif

    return tryFastCompactAlignedMalloc(alignment, size);
#endif
}

void FastMallocAlignedMemoryAllocator::freeAlignedMemory(void* basePtr)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.free(basePtr);
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

