/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "B3Procedure.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BasicBlockUtils.h"
#include "B3BlockWorklist.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

Procedure::Procedure()
    : m_lastPhaseName("initial")
{
}

Procedure::~Procedure()
{
}

BasicBlock* Procedure::addBlock(double frequency)
{
    std::unique_ptr<BasicBlock> block(new BasicBlock(m_blocks.size(), frequency));
    BasicBlock* result = block.get();
    m_blocks.append(WTF::move(block));
    return result;
}

void Procedure::resetReachability()
{
    B3::resetReachability(m_blocks);
}

void Procedure::dump(PrintStream& out) const
{
    for (BasicBlock* block : *this)
        out.print(deepDump(block));
}

Vector<BasicBlock*> Procedure::blocksInPreOrder()
{
    return B3::blocksInPreOrder(at(0));
}

Vector<BasicBlock*> Procedure::blocksInPostOrder()
{
    return B3::blocksInPostOrder(at(0));
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
