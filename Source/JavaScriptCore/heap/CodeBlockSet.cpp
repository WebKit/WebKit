/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include <wtf/CommaPrinter.h>

namespace JSC {

static const bool verbose = false;

CodeBlockSet::CodeBlockSet(BlockAllocator& blockAllocator)
    : m_currentlyExecuting(blockAllocator)
{
}

CodeBlockSet::~CodeBlockSet()
{
    for (CodeBlock* codeBlock : m_oldCodeBlocks)
        codeBlock->deref();

    for (CodeBlock* codeBlock : m_newCodeBlocks)
        codeBlock->deref();
}

void CodeBlockSet::add(PassRefPtr<CodeBlock> codeBlock)
{
    CodeBlock* block = codeBlock.leakRef();
    bool isNewEntry = m_newCodeBlocks.add(block).isNewEntry;
    ASSERT_UNUSED(isNewEntry, isNewEntry);
}

void CodeBlockSet::promoteYoungCodeBlocks()
{
    m_oldCodeBlocks.add(m_newCodeBlocks.begin(), m_newCodeBlocks.end());
    m_newCodeBlocks.clear();
}

void CodeBlockSet::clearMarksForFullCollection()
{
    for (CodeBlock* codeBlock : m_oldCodeBlocks) {
        codeBlock->m_mayBeExecuting = false;
        codeBlock->m_visitAggregateHasBeenCalled = false;
    }

    // We promote after we clear marks on the old generation CodeBlocks because
    // none of the young generations CodeBlocks need to be cleared.
    promoteYoungCodeBlocks();
}

void CodeBlockSet::clearMarksForEdenCollection(const Vector<const JSCell*>& rememberedSet)
{
    // This ensures that we will revisit CodeBlocks in remembered Executables even if they were previously marked.
    for (const JSCell* cell : rememberedSet) {
        ScriptExecutable* executable = const_cast<ScriptExecutable*>(jsDynamicCast<const ScriptExecutable*>(cell));
        if (!executable)
            continue;
        executable->forEachCodeBlock([](CodeBlock* codeBlock) {
            codeBlock->m_mayBeExecuting = false;
            codeBlock->m_visitAggregateHasBeenCalled = false;
        });
    }
}

void CodeBlockSet::deleteUnmarkedAndUnreferenced(HeapOperation collectionType)
{
    HashSet<CodeBlock*>& set = collectionType == EdenCollection ? m_newCodeBlocks : m_oldCodeBlocks;

    // This needs to be a fixpoint because code blocks that are unmarked may
    // refer to each other. For example, a DFG code block that is owned by
    // the GC may refer to an FTL for-entry code block that is also owned by
    // the GC.
    Vector<CodeBlock*, 16> toRemove;
    if (verbose)
        dataLog("Fixpointing over unmarked, set size = ", set.size(), "...\n");
    for (;;) {
        for (CodeBlock* codeBlock : set) {
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
            set.remove(codeBlock);
        toRemove.resize(0);
    }

    // Any remaining young CodeBlocks are live and need to be promoted to the set of old CodeBlocks.
    if (collectionType == EdenCollection)
        promoteYoungCodeBlocks();
}

void CodeBlockSet::remove(CodeBlock* codeBlock)
{
    codeBlock->deref();
    if (m_oldCodeBlocks.contains(codeBlock)) {
        m_oldCodeBlocks.remove(codeBlock);
        return;
    }
    ASSERT(m_newCodeBlocks.contains(codeBlock));
    m_newCodeBlocks.remove(codeBlock);
}

void CodeBlockSet::traceMarked(SlotVisitor& visitor)
{
    if (verbose)
        dataLog("Tracing ", m_currentlyExecuting.size(), " code blocks.\n");
    for (CodeBlock* codeBlock : m_currentlyExecuting) {
        ASSERT(codeBlock->m_mayBeExecuting);
        codeBlock->visitAggregate(visitor);
    }
}

void CodeBlockSet::rememberCurrentlyExecutingCodeBlocks(Heap* heap)
{
#if ENABLE(GGC)
    if (verbose)
        dataLog("Remembering ", m_currentlyExecuting.size(), " code blocks.\n");
    for (CodeBlock* codeBlock : m_currentlyExecuting) {
        heap->addToRememberedSet(codeBlock->ownerExecutable());
        ASSERT(codeBlock->m_mayBeExecuting);
    }
    m_currentlyExecuting.clear();
#else
    UNUSED_PARAM(heap);
#endif // ENABLE(GGC)
}

void CodeBlockSet::dump(PrintStream& out) const
{
    CommaPrinter comma;
    out.print("{old = [");
    for (CodeBlock* codeBlock : m_oldCodeBlocks)
        out.print(comma, pointerDump(codeBlock));
    out.print("], new = [");
    comma = CommaPrinter();
    for (CodeBlock* codeBlock : m_newCodeBlocks)
        out.print(comma, pointerDump(codeBlock));
    out.print("], currentlyExecuting = [");
    comma = CommaPrinter();
    for (CodeBlock* codeBlock : m_currentlyExecuting)
        out.print(comma, pointerDump(codeBlock));
    out.print("]}");
}

} // namespace JSC

