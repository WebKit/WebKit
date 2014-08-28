/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "DFGBlockWorklist.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlock.h"

namespace JSC { namespace DFG {

BlockWorklist::BlockWorklist()
{
}

BlockWorklist::~BlockWorklist()
{
}

bool BlockWorklist::push(BasicBlock* block)
{
    if (!m_seen.add(block))
        return false;
    
    m_stack.append(block);
    return true;
}

BasicBlock* BlockWorklist::pop()
{
    if (m_stack.isEmpty())
        return nullptr;
    
    return m_stack.takeLast();
}

PostOrderBlockWorklist::PostOrderBlockWorklist()
{
}

PostOrderBlockWorklist::~PostOrderBlockWorklist()
{
}

bool PostOrderBlockWorklist::pushPre(BasicBlock* block)
{
    return m_worklist.push(block, PreOrder);
}

void PostOrderBlockWorklist::pushPost(BasicBlock* block)
{
    m_worklist.forcePush(block, PostOrder);
}

BlockWithOrder PostOrderBlockWorklist::pop()
{
    BlockWith<VisitOrder> result = m_worklist.pop();
    return BlockWithOrder(result.block, result.data);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
