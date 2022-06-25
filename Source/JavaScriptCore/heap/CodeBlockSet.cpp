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
#include <wtf/CommaPrinter.h>

namespace JSC {

CodeBlockSet::CodeBlockSet()
{
}

CodeBlockSet::~CodeBlockSet()
{
}

bool CodeBlockSet::contains(const AbstractLocker&, void* candidateCodeBlock)
{
    RELEASE_ASSERT(m_lock.isLocked());
    CodeBlock* codeBlock = static_cast<CodeBlock*>(candidateCodeBlock);
    if (!HashSet<CodeBlock*>::isValidValue(codeBlock))
        return false;
    return m_codeBlocks.contains(codeBlock);
}

void CodeBlockSet::clearCurrentlyExecuting()
{
    m_currentlyExecuting.clear();
}

bool CodeBlockSet::isCurrentlyExecuting(CodeBlock* codeBlock)
{
    return m_currentlyExecuting.contains(codeBlock);
}

void CodeBlockSet::dump(PrintStream& out) const
{
    CommaPrinter comma;
    out.print("{codeBlocks = [");
    for (CodeBlock* codeBlock : m_codeBlocks)
        out.print(comma, pointerDump(codeBlock));
    out.print("], currentlyExecuting = [");
    comma = CommaPrinter();
    for (CodeBlock* codeBlock : m_currentlyExecuting)
        out.print(comma, pointerDump(codeBlock));
    out.print("]}");
}

void CodeBlockSet::add(CodeBlock* codeBlock)
{
    Locker locker { m_lock };
    auto result = m_codeBlocks.add(codeBlock);
    RELEASE_ASSERT(result);
}

void CodeBlockSet::remove(CodeBlock* codeBlock)
{
    Locker locker { m_lock };
    bool result = m_codeBlocks.remove(codeBlock);
    RELEASE_ASSERT(result);
}

} // namespace JSC

