/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "DFGLICMPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractInterpreterInlines.h"
#include "DFGAtTailAbstractState.h"
#include "DFGClobberSet.h"
#include "DFGClobberize.h"
#include "DFGControlEquivalenceAnalysis.h"
#include "DFGEdgeDominates.h"
#include "DFGGraph.h"
#include "DFGMayExit.h"
#include "DFGNaturalLoops.h"
#include "DFGPhase.h"
#include "DFGSafeToExecute.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class LICMPhase : public Phase {
    static constexpr bool verbose = false;

    using NaturalLoop = SSANaturalLoop;

    struct LoopData {
        ClobberSet writes;
        BasicBlock* preHeader { nullptr };
    };
    
public:
    LICMPhase(Graph& graph)
        : Phase(graph, "LICM")
        , m_state(graph)
        , m_interpreter(graph, m_state)
    {
    }
    
    bool run()
    {
        DFG_ASSERT(m_graph, nullptr, m_graph.m_form == SSA);
        
        m_graph.ensureSSADominators();
        m_graph.ensureSSANaturalLoops();
        m_graph.ensureControlEquivalenceAnalysis();

        if (verbose) {
            dataLog("Graph before LICM:\n");
            m_graph.dump();
        }
        
        m_data.resize(m_graph.m_ssaNaturalLoops->numLoops());
        
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
            
            const NaturalLoop* loop = m_graph.m_ssaNaturalLoops->innerMostLoopOf(block);
            if (!loop)
                continue;
            LoopData& data = m_data[loop->index()];
            for (auto* node : *block) {
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
        for (unsigned loopIndex = m_graph.m_ssaNaturalLoops->numLoops(); loopIndex--;) {
            const NaturalLoop& loop = m_graph.m_ssaNaturalLoops->loop(loopIndex);
            LoopData& data = m_data[loop.index()];
            
            for (
                const NaturalLoop* outerLoop = m_graph.m_ssaNaturalLoops->innerMostOuterLoop(loop);
                outerLoop;
                outerLoop = m_graph.m_ssaNaturalLoops->innerMostOuterLoop(*outerLoop))
                m_data[outerLoop->index()].writes.addAll(data.writes);
            
            BasicBlock* header = loop.header();
            BasicBlock* preHeader = nullptr;
            unsigned numberOfPreHeaders = 0; // We're cool if this is 1.

            // This is guaranteed because we expect the CFG not to have unreachable code. Therefore, a
            // loop header must have a predecessor. (Also, we don't allow the root block to be a loop,
            // which cuts out the one other way of having a loop header with only one predecessor.)
            DFG_ASSERT(m_graph, header->at(0), header->predecessors.size() > 1, header->predecessors.size());
            
            for (unsigned i = header->predecessors.size(); i--;) {
                BasicBlock* predecessor = header->predecessors[i];
                if (m_graph.m_ssaDominators->dominates(header, predecessor))
                    continue;

                preHeader = predecessor;
                ++numberOfPreHeaders;
            }

            // We need to validate the pre-header. There are a bunch of things that could be wrong
            // about it:
            //
            // - There might be more than one. This means that pre-header creation either did not run,
            //   or some CFG transformation destroyed the pre-headers.
            //
            // - It may not be legal to exit at the pre-header. That would be a real bummer. Currently,
            //   LICM assumes that it can always hoist checks. See
            //   https://bugs.webkit.org/show_bug.cgi?id=148545. Though even with that fixed, we anyway
            //   would need to check if it's OK to exit at the pre-header since if we can't then we
            //   would have to restrict hoisting to non-exiting nodes.

            if (numberOfPreHeaders != 1)
                continue;

            // This is guaranteed because the header has multiple predecessors and critical edges are
            // broken. Therefore the predecessors must all have one successor, which implies that they
            // must end in a Jump.
            DFG_ASSERT(m_graph, preHeader->terminal(), preHeader->terminal()->op() == Jump, preHeader->terminal()->op());

            if (!preHeader->terminal()->origin.exitOK)
                continue;
            
            data.preHeader = preHeader;
        }
        
        m_graph.initializeNodeOwners();
        
        // Walk all basic blocks that belong to loops, looking for hoisting opportunities.
        // We try to hoist to the outer-most loop that permits it. Hoisting is valid if:
        // - The node doesn't write anything.
        // - The node doesn't read anything that the loop writes.
        // - The preHeader is valid (i.e. it passed the validation above).
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
        Vector<const NaturalLoop*> loopStack;
        bool changed = false;

        WeakRandom random { Options::seedForLICMFuzzer() };

        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            if (!block->cfaHasVisited)
                continue;

            const NaturalLoop* loop = m_graph.m_ssaNaturalLoops->innerMostLoopOf(block);
            if (!loop)
                continue;
            
            loopStack.shrink(0);
            for (
                const NaturalLoop* current = loop;
                current;
                current = m_graph.m_ssaNaturalLoops->innerMostOuterLoop(*current))
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
                if (nodeRef->op() == ForceOSRExit)
                    break;
                for (unsigned stackIndex = loopStack.size(); stackIndex--;) {
                    if (UNLIKELY(Options::useLICMFuzzing())) {
                        bool shouldAttemptHoist = random.returnTrueWithProbability(Options::allowHoistingLICMProbability());
                        if (!shouldAttemptHoist)
                            continue;
                    }

                    changed |= attemptHoist(block, nodeRef, loopStack[stackIndex]);
                }
            }
        }

        return changed;
    }

private:
    bool attemptHoist(BasicBlock* fromBlock, Node*& nodeRef, const NaturalLoop* loop)
    {
        Node* node = nodeRef;
        LoopData& data = m_data[loop->index()];

        if (!data.preHeader) {
            if (verbose)
                dataLog("    Not hoisting ", node, " because the pre-header is invalid.\n");
            return false;
        }
        
        if (!data.preHeader->cfaDidFinish) {
            if (verbose)
                dataLog("    Not hoisting ", node, " because CFA is invalid.\n");
            return false;
        }
        
        m_state.initializeTo(data.preHeader);
        ASSERT(m_state.isValid());
        NodeOrigin originalOrigin = node->origin;
        bool canSpeculateBlindly = !m_graph.hasGlobalExitSite(originalOrigin.semantic, HoistingFailed);

        // NOTE: We could just use BackwardsDominators here directly, since we already know that the
        // preHeader dominates fromBlock. But we wouldn't get anything from being so clever, since
        // dominance checks are O(1) and only a few integer compares.
        bool isControlEquivalent = m_graph.m_controlEquivalenceAnalysis->dominatesEquivalently(data.preHeader, fromBlock);

        bool addsBlindSpeculation = !isControlEquivalent;
        NodeOrigin terminalOrigin = data.preHeader->terminal()->origin;
        Vector<Node*, 2> hoistedNodes; // This is sorted in the program order they will appear in the basic block we're hoisting to.

        auto insertHoistedNode = [&] (Node* node) {
            data.preHeader->insertBeforeTerminal(node);
            node->owner = data.preHeader;
            node->origin = terminalOrigin.withSemantic(node->origin.semantic);
            node->origin.wasHoisted |= addsBlindSpeculation;
            hoistedNodes.append(node);
        };

        auto updateAbstractState = [&] {
            auto invalidate = [&] (const NaturalLoop* loop) {
                LoopData& data = m_data[loop->index()];
                data.preHeader->cfaDidFinish = false;

                for (unsigned bodyIndex = loop->size(); bodyIndex--;) {
                    BasicBlock* block = loop->at(bodyIndex);
                    if (block != data.preHeader)
                        block->cfaHasVisited = false;
                    block->cfaDidFinish = false;
                }
            };

            // We can trust what AI proves about edge proof statuses when hoisting to the preheader.
            m_state.trustEdgeProofs();
            for (unsigned i = 0; i < hoistedNodes.size(); ++i) {
                if (!m_interpreter.execute(hoistedNodes[i])) {
                    invalidate(loop);
                    return;
                }
            }

            // However, when walking various inner loops below, the proof status of
            // an edge may be trivially true, even if it's not true in the preheader
            // we hoist to. We don't allow the below node executions to change the
            // state of edge proofs. An example of where a proof is trivially true
            // is if we have two loops, L1 and L2, where L2 is nested inside L1. The
            // header for L1 dominates L2. We hoist a Check from L1's header into L1's
            // preheader. However, inside L2's preheader, we can't trust that AI will
            // tell us this edge is proven. It's proven in L2's preheader because L2
            // is dominated by L1's header. However, the edge is not guaranteed to be
            // proven inside L1's preheader.
            m_state.dontTrustEdgeProofs();

            // Modify the states at the end of the preHeader of the loop we hoisted to,
            // and all pre-headers inside the loop. This isn't a stability bottleneck right now
            // because most loops are small and most blocks belong to few loops.
            for (unsigned bodyIndex = loop->size(); bodyIndex--;) {
                BasicBlock* subBlock = loop->at(bodyIndex);
                const NaturalLoop* subLoop = m_graph.m_ssaNaturalLoops->headerOf(subBlock);
                if (!subLoop)
                    continue;
                BasicBlock* subPreHeader = m_data[subLoop->index()].preHeader;
                // We may not have given this loop a pre-header because either it didn't have exitOK
                // or the header had multiple predecessors that it did not dominate. In that case the
                // loop wouldn't be a hoisting candidate anyway, so we don't have to do anything.
                if (!subPreHeader)
                    continue;
                // The pre-header's tail may be unreachable, in which case we have nothing to do.
                if (!subPreHeader->cfaDidFinish)
                    continue;
                // We handled this above.
                if (subPreHeader == data.preHeader)
                    continue;
                m_state.initializeTo(subPreHeader);
                for (unsigned i = 0; i < hoistedNodes.size(); ++i) {
                    if (!m_interpreter.execute(hoistedNodes[i])) {
                        invalidate(subLoop);
                        break;
                    }
                }
            }
        };
        
        auto tryHoistChecks = [&] {
            if (addsBlindSpeculation && !canSpeculateBlindly)
                return false;

            ASSERT(hoistedNodes.isEmpty());

            Vector<Edge, 3> checks;
            m_graph.doToChildren(node, [&] (Edge edge) {
                if (!m_graph.m_ssaDominators->dominates(edge.node()->owner, data.preHeader))
                    return;

                if (!edge.willHaveCheck())
                    return;

                if ((m_state.forNode(edge).m_type & SpecEmpty) && checkMayCrashIfInputIsEmpty(edge.useKind())) {
                    if (!canSpeculateBlindly)
                        return;
                    Node* checkNotEmpty = m_graph.addNode(CheckNotEmpty, originalOrigin, Edge(edge.node(), UntypedUse));
                    insertHoistedNode(checkNotEmpty);
                }

                checks.append(edge);
            });

            if (checks.isEmpty())
                return false;

            AdjacencyList children;
            NodeType checkOp = Check;
            if (checks.size() <= AdjacencyList::Size) {
                children = AdjacencyList(AdjacencyList::Fixed);
                for (unsigned i = 0; i < checks.size(); ++i)
                    children.setChild(i, checks[i]);
            } else {
                checkOp = CheckVarargs;
                unsigned firstChild = m_graph.m_varArgChildren.size();
                for (Edge edge : checks)
                    m_graph.m_varArgChildren.append(edge);
                children = AdjacencyList(AdjacencyList::Variable, firstChild, checks.size());
            }

            Node* check = m_graph.addNode(checkOp, originalOrigin, children);
            insertHoistedNode(check);
            updateAbstractState();

            if (verbose)
                dataLogLn("    Hoisted some checks from ", node, " and created a new Check ", check, ". Hoisted from ", *fromBlock, " to ", *data.preHeader);

            return true;
        };

        if (!edgesDominate(m_graph, node, data.preHeader)) {
            if (verbose) {
                dataLog(
                    "    Not hoisting ", node, " because it isn't loop invariant.\n");
            }
            return tryHoistChecks();
        }

        if (doesWrites(m_graph, node)) {
            if (verbose)
                dataLog("    Not hoisting ", node, " because it writes things.\n");
            return tryHoistChecks();
        }

        // It's not safe to consult the AbstractState inside mayExit until we prove all edges
        // dominate the pre-header we're hoisting to. We are more conservative above when assigning
        // to this variable since we hadn't yet proven all edges dominate the pre-header. Above, we
        // just assume mayExit is true. We refine that here since we can now consult the AbstractState.
        addsBlindSpeculation = mayExit(m_graph, node, m_state) && !isControlEquivalent;

        if (readsOverlap(m_graph, node, data.writes)) {
            if (verbose) {
                dataLog(
                    "    Not hoisting ", node,
                    " because it reads things that the loop writes.\n");
            }
            return tryHoistChecks();
        }
        
        if (addsBlindSpeculation && !canSpeculateBlindly) {
            if (verbose) {
                dataLog(
                    "    Not hoisting ", node, " because it may exit and the pre-header (",
                    *data.preHeader, ") is not control equivalent to the node's original block (",
                    *fromBlock, ") and hoisting had previously failed.\n");
            }
            return tryHoistChecks();
        }
        
        if (!safeToExecute(m_state, m_graph, node)) {
            // See if we can rescue the situation by inserting blind speculations.
            bool ignoreEmptyChildren = true;
            if (canSpeculateBlindly
                && safeToExecute(m_state, m_graph, node, ignoreEmptyChildren)) {
                if (verbose) {
                    dataLog(
                        "    Rescuing hoisting by inserting empty checks.\n");
                }
                m_graph.doToChildren(
                    node,
                    [&] (Edge& edge) {
                        if (!(m_state.forNode(edge).m_type & SpecEmpty))
                            return;
                        
                        Node* check = m_graph.addNode(CheckNotEmpty, originalOrigin, Edge(edge.node(), UntypedUse));
                        insertHoistedNode(check);
                    });
            } else {
                if (verbose) {
                    dataLog(
                        "    Not hoisting ", node, " because it isn't safe to execute.\n");
                }
                return tryHoistChecks();
            }
        }
        
        if (verbose) {
            dataLog(
                "    Hoisting ", node, " from ", *fromBlock, " to ", *data.preHeader,
                "\n");
        }

        insertHoistedNode(node);
        updateAbstractState();

        if (node->flags() & NodeHasVarArgs)
            nodeRef = m_graph.addNode(CheckVarargs, originalOrigin, m_graph.copyVarargChildren(node));
        else
            nodeRef = m_graph.addNode(Check, originalOrigin, node->children);
        
        return true;
    }
    
    AtTailAbstractState m_state;
    AbstractInterpreter<AtTailAbstractState> m_interpreter;
    Vector<LoopData> m_data;
};

bool performLICM(Graph& graph)
{
    return runPhase<LICMPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

