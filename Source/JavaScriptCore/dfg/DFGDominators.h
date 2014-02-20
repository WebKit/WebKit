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

#ifndef DFGDominators_h
#define DFGDominators_h

#if ENABLE(DFG_JIT)

#include "DFGAnalysis.h"
#include "DFGBasicBlock.h"
#include "DFGCommon.h"
#include <wtf/FastBitVector.h>

namespace JSC { namespace DFG {

class Graph;

class Dominators : public Analysis<Dominators> {
public:
    Dominators();
    ~Dominators();
    
    void compute(Graph& graph);
    
    bool dominates(BlockIndex from, BlockIndex to) const
    {
        ASSERT(isValid());
        return m_results[to].get(from);
    }
    
    bool dominates(BasicBlock* from, BasicBlock* to) const
    {
        return dominates(from->index, to->index);
    }
    
    void dump(Graph& graph, PrintStream&) const;
    
private:
    bool pruneDominators(Graph&, BlockIndex);
    
    Vector<FastBitVector> m_results; // For each block, the bitvector of blocks that dominate it.
    FastBitVector m_scratch; // A temporary bitvector with bit for each block. We recycle this to save new/deletes.
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDominators_h
