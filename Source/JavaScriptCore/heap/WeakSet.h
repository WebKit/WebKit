/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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

    bool isEmpty() const;
    bool isTriviallyDestructible() const;

    void reap();
    void sweep();
    JS_EXPORT_PRIVATE void shrink();

    static constexpr ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(WeakSet, m_vm); }

    WeakBlock* head() { return m_blocks.head(); }

    template<typename Functor>
    void forEachBlock(const Functor& functor)
    {
        for (WeakBlock* block = m_blocks.head(); block; block = block->next())
            functor(*block);
    }

private:
    JS_EXPORT_PRIVATE WeakBlock* findAllocator(CellContainer);
    WeakBlock* NODELETE tryFindAllocator();
    WeakBlock* addAllocator(CellContainer);

    JS_EXPORT_PRIVATE void didBecomeEmpty(WeakBlock*);

    void tryReleaseBlock(WeakBlock*);

    void detachAllocator();
    void resetAllocator();

    WeakBlock* m_currentBlock { nullptr };
    WeakBlock* m_nextAllocator { nullptr };
    DoublyLinkedList<WeakBlock> m_blocks;
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

inline bool WeakSet::isEmpty() const
{
    for (WeakBlock* block = m_blocks.head(); block; block = block->next()) {
        if (!block->isEmpty())
            return false;
    }

    return true;
}

inline bool WeakSet::isTriviallyDestructible() const
{
    if (!m_blocks.isEmpty())
        return false;
    if (isOnList())
        return false;
    return true;
}

ALWAYS_INLINE void WeakSet::deallocate(WeakImpl* weakImpl)
{
    weakImpl->clear();
}

inline void WeakSet::detachAllocator()
{
    m_currentBlock = nullptr;
    m_nextAllocator = nullptr;
}

inline void WeakSet::resetAllocator()
{
    m_currentBlock = nullptr;
    m_nextAllocator = m_blocks.head();
}

} // namespace JSC
