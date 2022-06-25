/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(B3_JIT)

#include "AirBasicBlock.h"
#include "AirCode.h"
#include <wtf/IndexMap.h>
#include <wtf/IndexSet.h>

namespace JSC { namespace B3 { namespace Air {

class CFG {
    WTF_MAKE_NONCOPYABLE(CFG);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef BasicBlock* Node;
    typedef IndexSet<BasicBlock*> Set;
    template<typename T> using Map = IndexMap<BasicBlock*, T>;
    typedef Vector<BasicBlock*, 4> List;

    CFG(Code& code)
        : m_code(code)
    {
    }

    Node root() { return m_code[0]; }

    template<typename T>
    Map<T> newMap() { return IndexMap<JSC::B3::Air::BasicBlock*, T>(m_code.size()); }

    SuccessorCollection<BasicBlock, BasicBlock::SuccessorList> successors(Node node) { return node->successorBlocks(); }
    BasicBlock::PredecessorList& predecessors(Node node) { return node->predecessors(); }

    unsigned index(Node node) const { return node->index(); }
    Node node(unsigned index) const { return m_code[index]; }
    unsigned numNodes() const { return m_code.size(); }

    PointerDump<BasicBlock> dump(Node node) const { return pointerDump(node); }

    void dump(PrintStream& out) const
    {
        m_code.dump(out);
    }

private:
    Code& m_code;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
