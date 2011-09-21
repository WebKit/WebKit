/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "JettisonedCodeBlocks.h"

#include "CodeBlock.h"
#include "SlotVisitor.h"
#include <wtf/Vector.h>

namespace JSC {

JettisonedCodeBlocks::JettisonedCodeBlocks() { }

JettisonedCodeBlocks::~JettisonedCodeBlocks()
{
    WTF::deleteAllKeys(m_map);
}

void JettisonedCodeBlocks::addCodeBlock(PassOwnPtr<CodeBlock> codeBlock)
{
    ASSERT(m_map.find(codeBlock.get()) == m_map.end());
    
    m_map.add(codeBlock.leakPtr(), false);
}

void JettisonedCodeBlocks::clearMarks()
{
    HashMap<CodeBlock*, bool>::iterator begin = m_map.begin();
    HashMap<CodeBlock*, bool>::iterator end = m_map.end();
    for (HashMap<CodeBlock*, bool>::iterator iter = begin; iter != end; ++iter)
        iter->second = false;
}

void JettisonedCodeBlocks::deleteUnmarkedCodeBlocks()
{
    Vector<CodeBlock*> toRemove;
    
    HashMap<CodeBlock*, bool>::iterator begin = m_map.begin();
    HashMap<CodeBlock*, bool>::iterator end = m_map.end();
    for (HashMap<CodeBlock*, bool>::iterator iter = begin; iter != end; ++iter) {
        if (!iter->second)
            toRemove.append(iter->first);
    }
    
    for (unsigned i = 0; i < toRemove.size(); ++i) {
        m_map.remove(toRemove[i]);
        delete toRemove[i];
    }
}

void JettisonedCodeBlocks::traceCodeBlocks(SlotVisitor& slotVisitor)
{
    HashMap<CodeBlock*, bool>::iterator begin = m_map.begin();
    HashMap<CodeBlock*, bool>::iterator end = m_map.end();
    for (HashMap<CodeBlock*, bool>::iterator iter = begin; iter != end; ++iter)
        iter->first->visitAggregate(slotVisitor);
}

} // namespace JSC

