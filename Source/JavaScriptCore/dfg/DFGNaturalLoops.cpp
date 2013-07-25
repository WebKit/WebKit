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
#include "DFGNaturalLoops.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include <wtf/CommaPrinter.h>

namespace JSC { namespace DFG {

void NaturalLoop::dump(PrintStream& out) const
{
    out.print("[Header: ", *header(), ", Body:");
    for (unsigned i = 0; i < m_body.size(); ++i)
        out.print(" ", *m_body[i]);
    out.print("]");
}

NaturalLoops::NaturalLoops() { }
NaturalLoops::~NaturalLoops() { }

void NaturalLoops::compute(Graph& graph)
{
    // Implement the classic dominator-based natural loop finder. The first
    // step is to find all control flow edges A -> B where B dominates A.
    // Then B is a loop header and A is a backward branching block. We will
    // then accumulate, for each loop header, multiple backward branching
    // blocks. Then we backwards graph search from the backward branching
    // blocks to their loop headers, which gives us all of the blocks in the
    // loop body.
    
    static const bool verbose = false;
    
    graph.m_dominators.computeIfNecessary(graph);
    
    if (verbose) {
        dataLog("Dominators:\n");
        graph.m_dominators.dump(graph, WTF::dataFile());
    }
    
    m_loops.resize(0);
    
    for (BlockIndex blockIndex = graph.numBlocks(); blockIndex--;) {
        BasicBlock* block = graph.block(blockIndex);
        if (!block)
            continue;
        
        for (unsigned i = block->numSuccessors(); i--;) {
            BasicBlock* successor = block->successor(i);
            if (!graph.m_dominators.dominates(successor, block))
                continue;
            bool found = false;
            for (unsigned j = m_loops.size(); j--;) {
                if (m_loops[j].header() == successor) {
                    m_loops[j].addBlock(block);
                    found = true;
                    break;
                }
            }
            if (found)
                continue;
            NaturalLoop loop(successor);
            loop.addBlock(block);
            m_loops.append(loop);
        }
    }
    
    if (verbose)
        dataLog("After bootstrap: ", *this, "\n");
    
    FastBitVector seenBlocks;
    Vector<BasicBlock*, 4> blockWorklist;
    seenBlocks.resize(graph.numBlocks());
    
    for (unsigned i = m_loops.size(); i--;) {
        NaturalLoop& loop = m_loops[i];
        
        seenBlocks.clearAll();
        ASSERT(blockWorklist.isEmpty());
        
        if (verbose)
            dataLog("Dealing with loop ", loop, "\n");
        
        for (unsigned j = loop.size(); j--;) {
            seenBlocks.set(loop[j]->index);
            blockWorklist.append(loop[j]);
        }
        
        while (!blockWorklist.isEmpty()) {
            BasicBlock* block = blockWorklist.takeLast();
            
            if (verbose)
                dataLog("    Dealing with ", *block, "\n");
            
            if (block == loop.header())
                continue;
            
            for (unsigned j = block->predecessors.size(); j--;) {
                BasicBlock* predecessor = block->predecessors[j];
                if (seenBlocks.get(predecessor->index))
                    continue;
                
                loop.addBlock(predecessor);
                blockWorklist.append(predecessor);
                seenBlocks.set(predecessor->index);
            }
        }
    }
    
    if (verbose)
        dataLog("Results: ", *this, "\n");
}

Vector<const NaturalLoop*> NaturalLoops::loopsOf(BasicBlock* block) const
{
    Vector<const NaturalLoop*> result;
    
    for (unsigned i = m_loops.size(); i--;) {
        if (m_loops[i].contains(block))
            result.append(&m_loops[i]);
    }
    
    return result;
}

void NaturalLoops::dump(PrintStream& out) const
{
    out.print("NaturalLoops:{");
    CommaPrinter comma;
    for (unsigned i = 0; i < m_loops.size(); ++i)
        out.print(comma, m_loops[i]);
    out.print("}");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

