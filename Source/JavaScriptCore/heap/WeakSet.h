/*
 * Copyright (C) 2012-2022, 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/CellContainer.h>
#include <JavaScriptCore/WeakBlock.h>
#include <wtf/SentinelLinkedList.h>

namespace JSC {

class Heap;
class WeakImpl;

namespace Integrity {
class Analyzer;
}

class WTF_CAPABILITY("mutator read or weak block lock") MutatorReadOrWeakBlockLock { };

class WeakSet : public BasicRawSentinelNode<WeakSet> {
    friend class LLIntOffsetsExtractor;
    friend class Integrity::Analyzer;
    friend class WeakBlock;

public:
    static WeakImpl* allocate(JSValue, WeakHandleOwner* = nullptr, void* context = nullptr);
    static void deallocate(WeakImpl*);

    WeakSet(VM&);
    ~WeakSet();
    void lastChanceToFinalize();

    JSC::Heap* heap() const;
    VM& vm() const;
    Lock& weakBlockLock() const WTF_RETURNS_CAPABILITY(m_weakBlockLock);

    NODELETE JS_EXPORT_PRIVATE bool isEmpty() const;
    bool isTriviallyDestructible() const;

    JS_EXPORT_PRIVATE void reap();
    JS_EXPORT_PRIVATE void sweep();
    JS_EXPORT_PRIVATE void shrink();

    // finalizeDeadHandles() may run off the mutator (one sweeper at a time); releaseBlocks() must run on the mutator.
    JS_EXPORT_PRIVATE void finalizeDeadHandles();
    JS_EXPORT_PRIVATE void releaseBlocks();

    static constexpr ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(WeakSet, m_vm); }

    void assertIsHoldingWeakBlockLock() const WTF_ASSERTS_ACQUIRED_CAPABILITY(m_weakBlockLock) WTF_ASSERTS_ACQUIRED_CAPABILITY(m_mutatorReadOrWeakBlockLock);
    // Every thread that changes m_blocks is a GC aware thread, so a stopped world excludes them all.
    void assertWorldIsStopped() const WTF_ASSERTS_ACQUIRED_CAPABILITY(m_weakBlockLock);

    WeakBlock* head() WTF_REQUIRES_CAPABILITY(m_weakBlockLock) { return m_blocks.head(); }

    template<typename Functor>
    void forEachBlock(NOESCAPE const Functor& functor) WTF_REQUIRES_CAPABILITY(m_weakBlockLock)
    {
        for (WeakBlock* block = m_blocks.head(); block; block = block->next())
            functor(*block);
    }

private:
    JS_EXPORT_PRIVATE WeakBlock* findAllocator(CellContainer);
    NODELETE WeakBlock* tryFindAllocator();
    WeakBlock* addAllocator(CellContainer);

    void tryReleaseBlock(Locker<Lock>&, WeakBlock*, IsConcurrent) WTF_REQUIRES_CAPABILITY(m_weakBlockLock, m_mutatorReadOrWeakBlockLock);

    void resetAllocator() WTF_REQUIRES_CAPABILITY(m_weakBlockLock);

    void assertIsExclusiveAccessHolder() const WTF_ASSERTS_ACQUIRED_SHARED_CAPABILITY(m_mutatorReadOrWeakBlockLock);

    // Integrity audits cells while marking, when the weak-set pass may hold weakBlockLock() on
    // another thread. A racy read only weakens its sanity check.
    WeakBlock* currentBlockForAudit() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { return m_currentBlock; }
    WeakBlock* nextAllocatorForAudit() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { return m_nextAllocator; }

    NO_UNIQUE_ADDRESS WeakBlockLock m_weakBlockLock;
    // Only the thread with heap access, or the conductor while the world is stopped, writes
    // m_currentBlock, so it may read it without the lock.
    NO_UNIQUE_ADDRESS MutatorReadOrWeakBlockLock m_mutatorReadOrWeakBlockLock;
    WeakBlock* m_currentBlock WTF_GUARDED_BY_CAPABILITY(m_mutatorReadOrWeakBlockLock) { nullptr };
    WeakBlock* m_nextAllocator WTF_GUARDED_BY_CAPABILITY(m_weakBlockLock) { nullptr };
    DoublyLinkedList<WeakBlock> m_blocks WTF_GUARDED_BY_CAPABILITY(m_weakBlockLock);
    // m_vm must be a pointer (instead of a reference) because the JSCLLIntOffsetsExtractor
    // cannot handle it being a reference. LowLevelInterpreter.asm reaches it by name, through
    // PreciseAllocationVMOffset.
    VM* const m_vm;
};

// PreciseAllocation embeds a WeakSet by value and rounds headerSize() up from its own size, so this
// size is part of the precise-allocation cell layout.
static_assert(sizeof(WeakSet) == 7 * sizeof(void*));

inline WeakSet::WeakSet(VM& vm)
    : m_vm(&vm)
{
}

inline VM& WeakSet::vm() const
{
    return *m_vm;
}

ALWAYS_INLINE void WeakSet::deallocate(WeakImpl* weakImpl)
{
    weakImpl->clear();
}

inline void WeakSet::resetAllocator()
{
    m_nextAllocator = m_blocks.head();
}

} // namespace JSC
