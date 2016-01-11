/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

CodeBlockSet::CodeBlockSet()
{
}

CodeBlockSet::~CodeBlockSet()
{
}

void CodeBlockSet::add(CodeBlock* codeBlock)
{
    LockHolder locker(&m_lock);
    bool isNewEntry = m_newCodeBlocks.add(codeBlock).isNewEntry;
    ASSERT_UNUSED(isNewEntry, isNewEntry);
}

void CodeBlockSet::promoteYoungCodeBlocks(const LockHolder&)
{
    ASSERT(m_lock.isLocked());
    m_oldCodeBlocks.add(m_newCodeBlocks.begin(), m_newCodeBlocks.end());
    m_newCodeBlocks.clear();
}

void CodeBlockSet::clearMarksForFullCollection()
{
    LockHolder locker(&m_lock);
    for (CodeBlock* codeBlock : m_oldCodeBlocks)
        codeBlock->clearVisitWeaklyHasBeenCalled();

    // We promote after we clear marks on the old generation CodeBlocks because
    // none of the young generations CodeBlocks need to be cleared.
    promoteYoungCodeBlocks(locker);
}

void CodeBlockSet::lastChanceToFinalize()
{
    LockHolder locker(&m_lock);
    for (CodeBlock* codeBlock : m_newCodeBlocks)
        codeBlock->classInfo()->methodTable.destroy(codeBlock);

    for (CodeBlock* codeBlock : m_oldCodeBlocks)
        codeBlock->classInfo()->methodTable.destroy(codeBlock);
}

void CodeBlockSet::deleteUnmarkedAndUnreferenced(HeapOperation collectionType)
{
    LockHolder locker(&m_lock);
    HashSet<CodeBlock*>& set = collectionType == EdenCollection ? m_newCodeBlocks : m_oldCodeBlocks;
    Vector<CodeBlock*> unmarked;
    for (CodeBlock* codeBlock : set) {
        if (Heap::isMarked(codeBlock))
            continue;
        unmarked.append(codeBlock);
    }

    for (CodeBlock* codeBlock : unmarked) {
        codeBlock->classInfo()->methodTable.destroy(codeBlock);
        set.remove(codeBlock);
    }

    // Any remaining young CodeBlocks are live and need to be promoted to the set of old CodeBlocks.
    if (collectionType == EdenCollection)
        promoteYoungCodeBlocks(locker);
}

bool CodeBlockSet::contains(const LockHolder&, void* candidateCodeBlock)
{
    RELEASE_ASSERT(m_lock.isLocked());
    CodeBlock* codeBlock = static_cast<CodeBlock*>(candidateCodeBlock);
    if (!HashSet<CodeBlock*>::isValidValue(codeBlock))
        return false;
    return m_oldCodeBlocks.contains(codeBlock) || m_newCodeBlocks.contains(codeBlock) || m_currentlyExecuting.contains(codeBlock);
}

void CodeBlockSet::writeBarrierCurrentlyExecutingCodeBlocks(Heap* heap)
{
    LockHolder locker(&m_lock);
    if (verbose)
        dataLog("Remembering ", m_currentlyExecuting.size(), " code blocks.\n");
    for (CodeBlock* codeBlock : m_currentlyExecuting)
        heap->writeBarrier(codeBlock);

    // It's safe to clear this set because we won't delete the CodeBlocks
    // in it until the next GC, and we'll recompute it at that time.
    m_currentlyExecuting.clear();
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

