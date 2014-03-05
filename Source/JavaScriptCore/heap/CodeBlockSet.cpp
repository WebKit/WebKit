/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CodeBlockSet.h"

#include "CodeBlock.h"
#include "JSCInlines.h"
#include "SlotVisitor.h"

namespace JSC {

static const bool verbose = false;

CodeBlockSet::CodeBlockSet(BlockAllocator& blockAllocator)
    : m_currentlyExecuting(blockAllocator)
{
}

CodeBlockSet::~CodeBlockSet()
{
    for (CodeBlock* codeBlock : m_set)
        codeBlock->deref();
}

void CodeBlockSet::add(PassRefPtr<CodeBlock> codeBlock)
{
    CodeBlock* block = codeBlock.leakRef();
    bool isNewEntry = m_set.add(block).isNewEntry;
    ASSERT_UNUSED(isNewEntry, isNewEntry);
}

void CodeBlockSet::clearMarks()
{
    for (CodeBlock* codeBlock : m_set) {
        codeBlock->m_mayBeExecuting = false;
        codeBlock->m_visitAggregateHasBeenCalled = false;
    }
}

void CodeBlockSet::deleteUnmarkedAndUnreferenced()
{
    // This needs to be a fixpoint because code blocks that are unmarked may
    // refer to each other. For example, a DFG code block that is owned by
    // the GC may refer to an FTL for-entry code block that is also owned by
    // the GC.
    Vector<CodeBlock*, 16> toRemove;
    if (verbose)
        dataLog("Fixpointing over unmarked, set size = ", m_set.size(), "...\n");
    for (;;) {
        for (CodeBlock* codeBlock : m_set) {
            if (!codeBlock->hasOneRef())
                continue;
            if (codeBlock->m_mayBeExecuting)
                continue;
            codeBlock->deref();
            toRemove.append(codeBlock);
        }
        if (verbose)
            dataLog("    Removing ", toRemove.size(), " blocks.\n");
        if (toRemove.isEmpty())
            break;
        for (CodeBlock* codeBlock : toRemove)
            m_set.remove(codeBlock);
        toRemove.resize(0);
    }
}

void CodeBlockSet::remove(CodeBlock* codeBlock)
{
    codeBlock->deref();
    m_set.remove(codeBlock);
}

void CodeBlockSet::traceMarked(SlotVisitor& visitor)
{
    if (verbose)
        dataLog("Tracing ", m_set.size(), " code blocks.\n");
    for (CodeBlock* codeBlock : m_set) {
        if (!codeBlock->m_mayBeExecuting)
            continue;
        codeBlock->visitAggregate(visitor);
    }
}

void CodeBlockSet::rememberCurrentlyExecutingCodeBlocks(Heap* heap)
{
#if ENABLE(GGC)
    for (CodeBlock* codeBlock : m_currentlyExecuting)
        heap->addToRememberedSet(codeBlock->ownerExecutable());
    m_currentlyExecuting.clear();
#else
    UNUSED_PARAM(heap);
#endif // ENABLE(GGC)
}

} // namespace JSC

