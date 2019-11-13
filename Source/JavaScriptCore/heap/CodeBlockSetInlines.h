/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CodeBlock.h"
#include "CodeBlockSet.h"

namespace JSC {

inline void CodeBlockSet::mark(const AbstractLocker&, CodeBlock* codeBlock)
{
    if (!codeBlock)
        return;

    // Conservative root scanning in Eden collection can only find PreciseAllocation that is allocated in this Eden cycle.
    // Since CodeBlockSet::m_currentlyExecuting is strongly assuming that this catches all the currently executing CodeBlock,
    // we now have a restriction that all CodeBlock needs to be a non-precise-allocation.
    ASSERT(!codeBlock->isPreciseAllocation());
    m_currentlyExecuting.add(codeBlock);
}

template<typename Functor>
void CodeBlockSet::iterate(const Functor& functor)
{
    auto locker = holdLock(m_lock);
    iterate(locker, functor);
}

template<typename Functor>
void CodeBlockSet::iterate(const AbstractLocker&, const Functor& functor)
{
    for (CodeBlock* codeBlock : m_codeBlocks)
        functor(codeBlock);
}

template<typename Functor>
void CodeBlockSet::iterateViaSubspaces(VM& vm, const Functor& functor)
{
    vm.forEachCodeBlockSpace(
        [&] (auto& spaceAndSet) {
            spaceAndSet.space.forEachLiveCell(
                [&] (HeapCell* cell, HeapCell::Kind) {
                    functor(jsCast<CodeBlock*>(static_cast<JSCell*>(cell)));
                });
        });
}

template<typename Functor>
void CodeBlockSet::iterateCurrentlyExecuting(const Functor& functor)
{
    LockHolder locker(&m_lock);
    for (CodeBlock* codeBlock : m_currentlyExecuting)
        functor(codeBlock);
}

} // namespace JSC

