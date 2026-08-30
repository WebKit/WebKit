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
#include <wtf/AutomaticThread.h>
#include <wtf/Box.h>
#include <wtf/FastMalloc.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PageBlock.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace JSC {

#if !ENABLE(MALLOC_HEAP_BREAKDOWN)

namespace {

// Lets a test drive the exhaustion path without running the machine out of memory.
std::atomic<bool> s_allocationFailsForTesting { false };

void* tryAllocateBlock()
{
    if (s_allocationFailsForTesting.load(std::memory_order_relaxed)) [[unlikely]]
        return nullptr;
    return tryFastCompactAlignedMalloc(MarkedBlock::blockSize, MarkedBlock::blockSize);
}

// The first store into a freshly allocated MarkedBlock takes a write fault, and a heap that is
// ramping up pays that fault thousands of times on the mutator thread. WarmUpBlockProvider keeps a
// supply of blocks whose pages a helper thread has already made resident, so the fault lands on the
// helper instead.
//
// The supply has to be deep from the very first request, because ramp demand runs at tens of blocks
// per millisecond and a depth that grows in proportion to observed demand arrives too late to help.
// Warming a page is not free even when it is handed back promptly, so the supply is given up once an
// interval passes with no demand at all.
class WarmUpBlockProvider {
public:
    using Phase = WarmUpMarkedBlockPhase;

    WarmUpBlockProvider()
        : m_lock(Box<Lock>::create())
        , m_condition(AutomaticThreadCondition::create())
        , m_thread(adoptRef(*new WarmUpThread(Locker { *m_lock }, *this)))
    {
    }

    static bool isEnabled()
    {
        // Mini mode trades throughput for footprint, which is the opposite of the bargain here.
        return Options::useWarmUpMarkedBlocks() && Options::warmUpMarkedBlockCount() && !VM::isInMiniMode();
    }

    static WarmUpBlockProvider& singleton()
    {
        static LazyNeverDestroyed<WarmUpBlockProvider> provider;
        static std::once_flag flag;
        std::call_once(flag, [] {
            provider.construct();
        });
        return provider;
    }

    void* tryTake()
    {
        Locker locker { *m_lock };
        void* result = m_blocks.isEmpty() ? nullptr : m_blocks.takeLast();
        m_demandSinceRefill = true;
        m_demandSinceIdleCheck = true;
        // A miss means the mutator is about to take the very fault this exists to avoid, and it is
        // also the only thing that brings the helper back once it has shut itself down. While it is
        // standing down, a notify would only postpone the timeout that lifts the stand-down.
        if ((!result || isRunningLow()) && m_phase != Phase::StandingDown)
            m_condition->notifyOne(locker);
        return result;
    }

    WarmUpMarkedBlockState stateForTesting()
    {
        Locker locker { *m_lock };
        return { m_blocks.size(), m_phase };
    }

private:
    // AutomaticThread has no voluntary temporary stop: PollResult::Stop is permanent, and start()
    // release-asserts that the thread is still running. So giving up after an allocation failure has
    // to be a wait that suppresses notifies, which is what leaves the idle timeout free to lift it.
    class WarmUpThread final : public AutomaticThread {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(WarmUpThread);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WarmUpThread);
    public:
        WarmUpThread(const AbstractLocker& locker, WarmUpBlockProvider& provider)
            : AutomaticThread(locker, provider.m_lock, provider.m_condition.copyRef(), Seconds(Options::warmUpMarkedBlockIdleTimeout()))
            , m_provider(provider)
        {
        }

        ASCIILiteral name() const final { return "JSCWarmUp"_s; }

    protected:
        void threadDidStart() final
        {
            Locker locker { *m_provider.m_lock };
            m_provider.m_phase = Phase::Armed;
        }

        PollResult poll(const AbstractLocker&) final
        {
            assertIsHeld(*m_provider.m_lock);
            if (m_provider.m_phase != Phase::Armed)
                return PollResult::Wait;
            // Topping the supply back up while demand is still arriving keeps it near its full depth
            // on a ramp, rather than letting it drain to the watermark first.
            if (m_provider.m_demandSinceRefill || m_provider.isRunningLow())
                return PollResult::Work;
            return PollResult::Wait;
        }

        WorkResult work() final
        {
            m_provider.refill();
            return WorkResult::Continue;
        }

        bool shouldSleep(const AbstractLocker&) final
        {
            assertIsHeld(*m_provider.m_lock);
            if (!std::exchange(m_provider.m_demandSinceIdleCheck, false))
                return true;
            m_provider.m_phase = Phase::Armed;
            return false;
        }

        void threadIsStopping(const AbstractLocker&) final
        {
            assertIsHeld(*m_provider.m_lock);
            // fastFree does not reach back into this lock, so it is safe to call with it held.
            m_provider.m_phase = Phase::Stopped;
            for (void* block : std::exchange(m_provider.m_blocks, { }))
                fastFree(block);
        }

    private:
        WarmUpBlockProvider& m_provider;
    };

    size_t targetDepth() WTF_REQUIRES_LOCK(*m_lock) { return m_phase == Phase::Armed ? Options::warmUpMarkedBlockCount() : 0; }

    bool isRunningLow() WTF_REQUIRES_LOCK(*m_lock) { return m_blocks.size() * 4 < targetDepth(); }

    static void makeResident(void* block)
    {
        // One store per page is what takes the fault. Striding by the real page size rather than by
        // the smallest a supported system could have avoids repeating the store within a page.
        size_t pageSize = WTF::pageSize();
        ASSERT(!(MarkedBlock::blockSize % pageSize));
        auto bytes = unsafeMakeSpan(static_cast<volatile char*>(block), MarkedBlock::blockSize);
        for (size_t offset = 0; offset < bytes.size(); offset += pageSize)
            bytes[offset] = 0;
    }

    void refill()
    {
        size_t want;
        {
            Locker locker { *m_lock };
            m_demandSinceRefill = false;
            size_t target = targetDepth();
            want = target > m_blocks.size() ? target - m_blocks.size() : 0;
        }

        Vector<void*, 32> staging;
        bool exhausted = false;
        for (size_t i = 0; i < want; ++i) {
            void* block = tryAllocateBlock();
            if (!block) {
                exhausted = true;
                break;
            }
            makeResident(block);
            staging.append(block);
        }

        Locker locker { *m_lock };
        m_blocks.appendVector(WTF::move(staging));
        if (exhausted)
            m_phase = Phase::StandingDown;
    }

    const Box<Lock> m_lock;
    const Ref<AutomaticThreadCondition> m_condition;
    const Ref<WarmUpThread> m_thread;
    Vector<void*, 32> m_blocks WTF_GUARDED_BY_LOCK(*m_lock);
    Phase m_phase WTF_GUARDED_BY_LOCK(*m_lock) { Phase::Stopped };
    bool m_demandSinceRefill WTF_GUARDED_BY_LOCK(*m_lock) { false };
    bool m_demandSinceIdleCheck WTF_GUARDED_BY_LOCK(*m_lock) { false };
};

} // anonymous namespace

WarmUpMarkedBlockState warmUpMarkedBlockStateForTesting()
{
    return WarmUpBlockProvider::singleton().stateForTesting();
}

void setWarmUpMarkedBlockAllocationShouldFailForTesting(bool shouldFail)
{
    s_allocationFailsForTesting.store(shouldFail, std::memory_order_relaxed);
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
    // MarkedBlock::tryCreate is the only caller today and always asks for a block-shaped region.
    // The guard keeps a future caller of some other size from being handed a block.
    if (alignment == MarkedBlock::blockSize && size == MarkedBlock::blockSize && WarmUpBlockProvider::isEnabled()) {
        if (void* block = WarmUpBlockProvider::singleton().tryTake())
            return block;
    }
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

