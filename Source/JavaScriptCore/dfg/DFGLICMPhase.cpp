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

#if ENABLE(DFG_JIT)

#include "DFGLICMPhase.h"

#include "DFGAbstractInterpreterInlines.h"
#include "DFGAtTailAbstractState.h"
#include "DFGBasicBlockInlines.h"
#include "DFGClobberSet.h"
#include "DFGClobberize.h"
#include "DFGEdgeDominates.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGSafeToExecute.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

namespace {

struct LoopData {
    LoopData()
        : preHeader(0)
    {
    }
    
    ClobberSet writes;
    BasicBlock* preHeader;
};

} // anonymous namespace

class LICMPhase : public Phase {
    static const bool verbose = false;
    
public:
    LICMPhase(Graph& graph)
        : Phase(graph, "LICM")
        , m_interpreter(graph, m_state)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        m_graph.m_dominators.computeIfNecessary(m_graph);
        m_graph.m_naturalLoops.computeIfNecessary(m_graph);
        
        m_data.resize(m_graph.m_naturalLoops.numLoops());
        
        // Figure out the set of things each loop writes to, not including blocks that
        // belong to inner loops. We fix this later.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            // Skip blocks that are proved to not execute.
            // FIXME: This shouldn't be needed.
            // https://bugs.webkit.org/show_bug.cgi?id=128584
            if (!block->cfaHasVisited)
                continue;
            
            const NaturalLoop* loop = m_graph.m_naturalLoops.innerMostLoopOf(block);
            if (!loop)
                continue;
            LoopData& data = m_data[loop->index()];
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                // Don't look beyond parts of the code that definitely always exit.
                // FIXME: This shouldn't be needed.
                // https://bugs.webkit.org/show_bug.cgi?id=128584
                if (node->op() == ForceOSRExit)
                    break;

                addWrites(m_graph, node, data.writes);
            }
        }
        
        // For each loop:
        // - Identify its pre-header.
        // - Make sure its outer loops know what it clobbers.
        for (unsigned loopIndex = m_graph.m_naturalLoops.numLoops(); loopIndex--;) {
            const NaturalLoop& loop = m_graph.m_naturalLoops.loop(loopIndex);
            LoopData& data = m_data[loop.index()];
            for (
                const NaturalLoop* outerLoop = m_graph.m_naturalLoops.innerMostOuterLoop(loop);
                outerLoop;
                outerLoop = m_graph.m_naturalLoops.innerMostOuterLoop(*outerLoop))
                m_data[outerLoop->index()].writes.addAll(data.writes);
            
            BasicBlock* header = loop.header();
            BasicBlock* preHeader = 0;
            for (unsigned i = header->predecessors.size(); i--;) {
                BasicBlock* predecessor = header->predecessors[i];
                if (m_graph.m_dominators.dominates(header, predecessor))
                    continue;
                RELEASE_ASSERT(!preHeader || preHeader == predecessor);
                preHeader = predecessor;
            }
            
            RELEASE_ASSERT(preHeader->last()->op() == Jump);
            
            data.preHeader = preHeader;
        }
        
        m_graph.initializeNodeOwners();
        
        // Walk all basic blocks that belong to loops, looking for hoisting opportunities.
        // We try to hoist to the outer-most loop that permits it. Hoisting is valid if:
        // - The node doesn't write anything.
        // - The node doesn't read anything that the loop writes.
        // - The preHeader's state at tail makes the node safe to execute.
        // - The loop's children all belong to nodes that strictly dominate the loop header.
        // - The preHeader's state at tail is still valid. This is mostly to save compile
        //   time and preserve some kind of sanity, if we hoist something that must exit.
        //
        // Also, we need to remember to:
        // - Update the state-at-tail with the node we hoisted, so future hoist candidates
        //   know about any type checks we hoisted.
        //
        // For maximum profit, we walk blocks in DFS order to ensure that we generally
        // tend to hoist dominators before dominatees.
        Vector<BasicBlock*> depthFirst;
        m_graph.getBlocksInDepthFirstOrder(depthFirst);
        Vector<const NaturalLoop*> loopStack;
        bool changed = false;
        for (
            unsigned depthFirstIndex = 0;
            depthFirstIndex < depthFirst.size();
            ++depthFirstIndex) {
            
            BasicBlock* block = depthFirst[depthFirstIndex];
            const NaturalLoop* loop = m_graph.m_naturalLoops.innerMostLoopOf(block);
            if (!loop)
                continue;
            
            loopStack.resize(0);
            for (
                const NaturalLoop* current = loop;
                current;
                current = m_graph.m_naturalLoops.innerMostOuterLoop(*current))
                loopStack.append(current);
            
            // Remember: the loop stack has the inner-most loop at index 0, so if we want
            // to bias hoisting to outer loops then we need to use a reverse loop.
            
            if (verbose) {
                dataLog(
                    "Attempting to hoist out of block ", *block, " in loops:\n");
                for (unsigned stackIndex = loopStack.size(); stackIndex--;) {
                    dataLog(
                        "        ", *loopStack[stackIndex], ", which writes ",
                        m_data[loopStack[stackIndex]->index()].writes, "\n");
                }
            }
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node*& nodeRef = block->at(nodeIndex);
                if (doesWrites(m_graph, nodeRef)) {
                    if (verbose)
                        dataLog("    Not hoisting ", nodeRef, " because it writes things.\n");
                    continue;
                }

                for (unsigned stackIndex = loopStack.size(); stackIndex--;)
                    changed |= attemptHoist(block, nodeRef, loopStack[stackIndex]);
            }
        }
        
        return changed;
    }

private:
    bool attemptHoist(BasicBlock* fromBlock, Node*& nodeRef, const NaturalLoop* loop)
    {
        Node* node = nodeRef;
        LoopData& data = m_data[loop->index()];
        
        if (!data.preHeader->cfaDidFinish) {
            if (verbose)
                dataLog("    Not hoisting ", node, " because CFA is invalid.\n");
            return false;
        }
        
        if (!edgesDominate(m_graph, node, data.preHeader)) {
            if (verbose) {
                dataLog(
                    "    Not hoisting ", node, " because it isn't loop invariant.\n");
            }
            return false;
        }
        
        if (readsOverlap(m_graph, node, data.writes)) {
            if (verbose) {
                dataLog(
                    "    Not hoisting ", node,
                    " because it reads things that the loop writes.\n");
            }
            return false;
        }
        
        m_state.initializeTo(data.preHeader);
        if (!safeToExecute(m_state, m_graph, node)) {
            if (verbose) {
                dataLog(
                    "    Not hoisting ", node, " because it isn't safe to execute.\n");
            }
            return false;
        }
        
        if (verbose) {
            dataLog(
                "    Hoisting ", node, " from ", *fromBlock, " to ", *data.preHeader,
                "\n");
        }
        
        data.preHeader->insertBeforeLast(node);
        node->misc.owner = data.preHeader;
        node->codeOriginForExitTarget = data.preHeader->last()->codeOriginForExitTarget;
        
        // Modify the states at the end of the preHeader of the loop we hoisted to,
        // and all pre-headers inside the loop.
        // FIXME: This could become a scalability bottleneck. Fortunately, most loops
        // are small and anyway we rapidly skip over basic blocks here.
        for (unsigned bodyIndex = loop->size(); bodyIndex--;) {
            BasicBlock* subBlock = loop->at(bodyIndex);
            const NaturalLoop* subLoop = m_graph.m_naturalLoops.headerOf(subBlock);
            if (!subLoop)
                continue;
            BasicBlock* subPreHeader = m_data[subLoop->index()].preHeader;
            if (!subPreHeader->cfaDidFinish)
                continue;
            m_state.initializeTo(subPreHeader);
            m_interpreter.execute(node);
        }
        
        // It just so happens that all of the nodes we currently know how to hoist
        // don't have var-arg children. That may change and then we can fix this
        // code. But for now we just assert that's the case.
        RELEASE_ASSERT(!(node->flags() & NodeHasVarArgs));
        
        nodeRef = m_graph.addNode(SpecNone, Phantom, node->codeOrigin, node->children);
        
        return true;
    }
    
    AtTailAbstractState m_state;
    AbstractInterpreter<AtTailAbstractState> m_interpreter;
    Vector<LoopData> m_data;
};

bool performLICM(Graph& graph)
{
    SamplingRegion samplingRegion("DFG LICM Phase");
    return runPhase<LICMPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

