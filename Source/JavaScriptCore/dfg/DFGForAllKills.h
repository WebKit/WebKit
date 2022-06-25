/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "DFGCombinedLiveness.h"
#include "DFGGraph.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "FullBytecodeLiveness.h"

namespace JSC { namespace DFG {

namespace ForAllKillsInternal {
constexpr bool verbose = false;
}

// Utilities for finding the last points where a node is live in DFG SSA. This accounts for liveness due
// to OSR exit. This is usually used for enumerating over all of the program points where a node is live,
// by exploring all blocks where the node is live at tail and then exploring all program points where the
// node is killed. A prerequisite to using these utilities is having liveness and OSR availability
// computed.

// This tells you those things that die on the boundary between nodeBefore and nodeAfter. It is
// conservative in the sense that it might resort to telling you some things that are still live at
// nodeAfter.
template<typename Functor>
void forAllKilledOperands(Graph& graph, Node* nodeBefore, Node* nodeAfter, const Functor& functor)
{
    CodeOrigin before = nodeBefore->origin.forExit;

    if (!nodeAfter) {
        graph.forAllLiveInBytecode(before, functor);
        return;
    }
    
    CodeOrigin after = nodeAfter->origin.forExit;
    
    Operand alreadyNoted;
    // If we MovHint something that is live at the time, then we kill the old value.
    if (nodeAfter->containsMovHint()) {
        Operand operand = nodeAfter->unlinkedOperand();
        if (graph.isLiveInBytecode(operand, after)) {
            functor(operand);
            alreadyNoted = operand;
        }
    }
    
    if (before == after)
        return;
    
    // It's easier to do this if the inline call frames are the same. This is way faster than the
    // other loop, below.
    auto* beforeInlineCallFrame = before.inlineCallFrame();
    if (beforeInlineCallFrame == after.inlineCallFrame()) {
        CodeBlock* codeBlock = graph.baselineCodeBlockFor(beforeInlineCallFrame);
        if (after.bytecodeIndex().checkpoint()) {
            ASSERT(before.bytecodeIndex().checkpoint() != after.bytecodeIndex().checkpoint());
            ASSERT_WITH_MESSAGE(before.bytecodeIndex().offset() == after.bytecodeIndex().offset() || nodeAfter->op() == ExitOK || nodeAfter->op() == InvalidationPoint, "When the DFG does code motion it should change the forExit origin to match the surrounding bytecodes.");

            auto liveBefore = tmpLivenessForCheckpoint(*codeBlock, before.bytecodeIndex());
            auto liveAfter = tmpLivenessForCheckpoint(*codeBlock, after.bytecodeIndex());
            liveAfter.invert();
            liveBefore.filter(liveAfter);

            liveBefore.forEachSetBit([&] (size_t tmp) {
                functor(remapOperand(beforeInlineCallFrame, Operand::tmp(tmp)));
            });
        } else if (before.bytecodeIndex().checkpoint()) {
            // We are moving on to another bytecode, all temps should be dead now.
            auto liveBefore = tmpLivenessForCheckpoint(*codeBlock, before.bytecodeIndex());

            liveBefore.forEachSetBit([&] (size_t tmp) {
                functor(remapOperand(beforeInlineCallFrame, Operand::tmp(tmp)));
            });
        }

        FullBytecodeLiveness& fullLiveness = graph.livenessFor(codeBlock);
        const FastBitVector& liveBefore = fullLiveness.getLiveness(before.bytecodeIndex(), LivenessCalculationPoint::BeforeUse);
        const FastBitVector& liveAfter = fullLiveness.getLiveness(after.bytecodeIndex(), LivenessCalculationPoint::BeforeUse);
        
        (liveBefore & ~liveAfter).forEachSetBit(
            [&] (size_t relativeLocal) {
                functor(remapOperand(beforeInlineCallFrame, virtualRegisterForLocal(relativeLocal)));
            });
        return;
    }

    // Detect kills the super conservative way: it is killed if it was live before and dead after.
    BitVector liveAfter = graph.localsAndTmpsLiveInBytecode(after);
    unsigned numLocals = graph.block(0)->variablesAtHead.numberOfLocals();
    graph.forAllLocalsAndTmpsLiveInBytecode(
        before,
        [&] (Operand operand) {
            if (operand == alreadyNoted)
                return;
            unsigned offset = operand.isTmp() ? numLocals + operand.value() : operand.toLocal();
            if (liveAfter.get(offset))
                return;
            functor(operand);
        });
}
    
// Tells you all of the nodes that would no longer be live across the node at this nodeIndex.
template<typename Functor>
void forAllKilledNodesAtNodeIndex(
    Graph& graph, AvailabilityMap& availabilityMap, BasicBlock* block, unsigned nodeIndex,
    const Functor& functor)
{
    static constexpr unsigned seenInClosureFlag = 1;
    static constexpr unsigned calledFunctorFlag = 2;
    HashMap<Node*, unsigned> flags;

    ASSERT(nodeIndex);
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

    Node* before = block->at(nodeIndex - 1);

    forAllKilledOperands(
        graph, before, node,
        [&] (Operand operand) {
            availabilityMap.closeStartingWithLocal(
                operand,
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
void forAllKillsInBlock(
    Graph& graph, const CombinedLiveness& combinedLiveness, BasicBlock* block,
    const Functor& functor)
{
    for (Node* node : combinedLiveness.liveAtTail[block])
        functor(block->size(), node);
    
    LocalOSRAvailabilityCalculator localAvailability(graph);
    localAvailability.beginBlock(block);
    // Start at the second node, because the functor is expected to only inspect nodes from the start of
    // the block up to nodeIndex (exclusive), so if nodeIndex is zero then the functor has nothing to do.
    for (unsigned nodeIndex = 1; nodeIndex < block->size(); ++nodeIndex) {
        dataLogLnIf(ForAllKillsInternal::verbose, "local availability at index: ", nodeIndex, " ", localAvailability.m_availability);
        forAllKilledNodesAtNodeIndex(
            graph, localAvailability.m_availability, block, nodeIndex,
            [&] (Node* node) {
                functor(nodeIndex, node);
            });
        localAvailability.executeNode(block->at(nodeIndex));
    }
}

} } // namespace JSC::DFG
