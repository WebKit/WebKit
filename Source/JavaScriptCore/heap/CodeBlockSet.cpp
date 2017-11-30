/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "SuperSampler.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

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

void CodeBlockSet::promoteYoungCodeBlocks(const AbstractLocker&)
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
}

void CodeBlockSet::lastChanceToFinalize(VM& vm)
{
    LockHolder locker(&m_lock);
    for (CodeBlock* codeBlock : m_newCodeBlocks)
        codeBlock->structure(vm)->classInfo()->methodTable.destroy(codeBlock);

    for (CodeBlock* codeBlock : m_oldCodeBlocks)
        codeBlock->structure(vm)->classInfo()->methodTable.destroy(codeBlock);
}

void CodeBlockSet::deleteUnmarkedAndUnreferenced(VM& vm, CollectionScope scope)
{
    LockHolder locker(&m_lock);
    
    // Destroying a CodeBlock takes about 1us on average in Speedometer. Full collections in Speedometer
    // usually have ~2000 CodeBlocks to process. The time it takes to process the whole list varies a
    // lot. In one extreme case I saw 18ms (on my fast MBP).
    //
    // FIXME: use Subspace instead of HashSet and adopt Subspace-based constraint solving. This may
    // remove the need to eagerly destruct CodeBlocks.
    // https://bugs.webkit.org/show_bug.cgi?id=180089
    //
    // FIXME: make CodeBlock::~CodeBlock a lot faster. It seems insane for that to take 1us or more.
    // https://bugs.webkit.org/show_bug.cgi?id=180109
    
    auto consider = [&] (HashSet<CodeBlock*>& set) {
        set.removeIf(
            [&] (CodeBlock* codeBlock) -> bool {
                if (Heap::isMarked(codeBlock))
                    return false;
                codeBlock->structure(vm)->classInfo()->methodTable.destroy(codeBlock);
                return true;
            });
    };

    switch (scope) {
    case CollectionScope::Eden:
        consider(m_newCodeBlocks);
        break;
    case CollectionScope::Full:
        consider(m_oldCodeBlocks);
        consider(m_newCodeBlocks);
        break;
    }

    // Any remaining young CodeBlocks are live and need to be promoted to the set of old CodeBlocks.
    promoteYoungCodeBlocks(locker);
}

bool CodeBlockSet::contains(const AbstractLocker&, void* candidateCodeBlock)
{
    RELEASE_ASSERT(m_lock.isLocked());
    CodeBlock* codeBlock = static_cast<CodeBlock*>(candidateCodeBlock);
    if (!HashSet<CodeBlock*>::isValidValue(codeBlock))
        return false;
    return m_oldCodeBlocks.contains(codeBlock) || m_newCodeBlocks.contains(codeBlock) || m_currentlyExecuting.contains(codeBlock);
}

void CodeBlockSet::clearCurrentlyExecuting()
{
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

