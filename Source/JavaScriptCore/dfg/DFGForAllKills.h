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

#ifndef DFGForAllKills_h
#define DFGForAllKills_h

#include "BytecodeKills.h"
#include "DFGGraph.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "FullBytecodeLiveness.h"

namespace JSC { namespace DFG {

// Utilities for finding the last points where a node is live in DFG SSA. This accounts for liveness due
// to OSR exit. This is usually used for enumerating over all of the program points where a node is live,
// by exploring all blocks where the node is live at tail and then exploring all program points where the
// node is killed. A prerequisite to using these utilities is having liveness and OSR availability
// computed.

template<typename Functor>
void forAllLiveNodesAtTail(Graph& graph, BasicBlock* block, const Functor& functor)
{
    HashSet<Node*> seen;
    for (Node* node : block->ssa->liveAtTail) {
        if (seen.add(node).isNewEntry)
            functor(node);
    }
    
    DFG_ASSERT(graph, block->terminal(), block->terminal()->origin.forExit.isSet());
    
    AvailabilityMap& availabilityMap = block->ssa->availabilityAtTail;
    for (unsigned i = availabilityMap.m_locals.size(); i--;) {
        VirtualRegister reg = availabilityMap.m_locals.virtualRegisterForIndex(i);
        
        if (!graph.isLiveInBytecode(reg, block->terminal()->origin.forExit))
            continue;
        
        availabilityMap.closeStartingWithLocal(
            reg,
            [&] (Node* node) -> bool {
                return seen.contains(node);
            },
            [&] (Node* node) -> bool {
                if (!seen.add(node).isNewEntry)
                    return false;
                functor(node);
                return true;
            });
    }
}

template<typename Functor>
void forAllKilledOperands(Graph& graph, CodeOrigin before, Node* nodeAfter, const Functor& functor)
{
    CodeOrigin after = nodeAfter->origin.forExit;
    
    if (!before) {
        if (!after)
            return;
        // The true before-origin is the origin at predecessors that jump to us. But there can be
        // many such predecessors and they will likely all have a different origin. So, it's better
        // to do the conservative thing.
        for (unsigned i = graph.block(0)->variablesAtHead.numberOfLocals(); i--;) {
            VirtualRegister reg = virtualRegisterForLocal(i);
            if (graph.isLiveInBytecode(reg, after))
                functor(reg);
        }
        return;
    }
    
    // If we MovHint something that is live at the time, then we kill the old value.
    VirtualRegister alreadyNoted;
    if (nodeAfter->containsMovHint()) {
        VirtualRegister reg = nodeAfter->unlinkedLocal();
        if (graph.isLiveInBytecode(reg, after)) {
            functor(reg);
            alreadyNoted = reg;
        }
    }
    
    if (before == after)
        return;
    
    // before could be unset even if after is, but the opposite cannot happen.
    ASSERT(!!after);
    
    // Detect kills the super conservative way: it is killed if it was live before and dead after.
    for (unsigned i = graph.block(0)->variablesAtHead.numberOfLocals(); i--;) {
        VirtualRegister reg = virtualRegisterForLocal(i);
        if (reg == alreadyNoted)
            continue;
        if (graph.isLiveInBytecode(reg, before) && !graph.isLiveInBytecode(reg, after))
            functor(reg);
    }
}
    
// Tells you all of the nodes that would no longer be live across the node at this nodeIndex.
template<typename Functor>
void forAllKilledNodesAtNodeIndex(
    Graph& graph, AvailabilityMap& availabilityMap, BasicBlock* block, unsigned nodeIndex,
    const Functor& functor)
{
    static const unsigned seenInClosureFlag = 1;
    static const unsigned calledFunctorFlag = 2;
    HashMap<Node*, unsigned> flags;
    
    Node* node = block->at(nodeIndex);
    
    graph.doToChildren(
        node,
        [&] (Edge edge) {
            if (edge.doesKill()) {
                auto& result = flags.add(edge.node(), 0).iterator->value;
                if (!(result & calledFunctorFlag)) {
                    functor(edge.node());
                    result |= calledFunctorFlag;
                }
            }
        });

    CodeOrigin before;
    if (nodeIndex)
        before = block->at(nodeIndex - 1)->origin.forExit;

    forAllKilledOperands(
        graph, before, node,
        [&] (VirtualRegister reg) {
            availabilityMap.closeStartingWithLocal(
                reg,
                [&] (Node* node) -> bool {
                    return flags.get(node) & seenInClosureFlag;
                },
                [&] (Node* node) -> bool {
                    auto& resultFlags = flags.add(node, 0).iterator->value;
                    bool result = resultFlags & seenInClosureFlag;
                    if (!(resultFlags & calledFunctorFlag))
                        functor(node);
                    resultFlags |= seenInClosureFlag | calledFunctorFlag;
                    return result;
                });
        });
}

// Tells you all of the places to start searching from in a basic block. Gives you the node index at which
// the value is either no longer live. This pretends that nodes are dead at the end of the block, so that
// you can use this to do per-basic-block analyses.
template<typename Functor>
void forAllKillsInBlock(Graph& graph, BasicBlock* block, const Functor& functor)
{
    forAllLiveNodesAtTail(
        graph, block,
        [&] (Node* node) {
            functor(block->size(), node);
        });
    
    LocalOSRAvailabilityCalculator localAvailability;
    localAvailability.beginBlock(block);
    // Start at the second node, because the functor is expected to only inspect nodes from the start of
    // the block up to nodeIndex (exclusive), so if nodeIndex is zero then the functor has nothing to do.
    for (unsigned nodeIndex = 1; nodeIndex < block->size(); ++nodeIndex) {
        forAllKilledNodesAtNodeIndex(
            graph, localAvailability.m_availability, block, nodeIndex,
            [&] (Node* node) {
                functor(nodeIndex, node);
            });
        localAvailability.executeNode(block->at(nodeIndex));
    }
}

} } // namespace JSC::DFG

#endif // DFGForAllKills_h

