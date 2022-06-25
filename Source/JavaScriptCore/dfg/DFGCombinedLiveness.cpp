/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "DFGCombinedLiveness.h"

#if ENABLE(DFG_JIT)

#include "DFGAvailabilityMap.h"
#include "DFGBlockMapInlines.h"
#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

static void addBytecodeLiveness(Graph& graph, AvailabilityMap& availabilityMap, NodeSet& seen, Node* node)
{
    graph.forAllLiveInBytecode(
        node->origin.forExit,
        [&] (Operand reg) {
            availabilityMap.closeStartingWithLocal(
                reg,
                [&] (Node* node) -> bool {
                    return seen.contains(node);
                },
                [&] (Node* node) -> bool {
                    return seen.add(node).isNewEntry;
                });
        });
}

NodeSet liveNodesAtHead(Graph& graph, BasicBlock* block)
{
    NodeSet seen;
    for (NodeFlowProjection node : block->ssa->liveAtHead) {
        if (node.kind() == NodeFlowProjection::Primary)
            seen.addVoid(node.node());
    }

    addBytecodeLiveness(graph, block->ssa->availabilityAtHead, seen, block->at(0));
    return seen;
}

CombinedLiveness::CombinedLiveness(Graph& graph)
    : liveAtHead(graph)
    , liveAtTail(graph)
{
    // First compute 
    // - The liveAtHead for each block.
    // - The liveAtTail for blocks that won't properly propagate
    //   the information based on their empty successor list.
    for (BasicBlock* block : graph.blocksInNaturalOrder()) {
        liveAtHead[block] = liveNodesAtHead(graph, block);

        // If we don't have successors, we can't rely on the propagation below. This doesn't usually
        // do anything for terminal blocks, since the last node is usually a return, so nothing is live
        // after it. However, we may also have the end of the basic block be:
        //
        // ForceOSRExit
        // Unreachable
        //
        // And things may definitely be live in bytecode at that point in the program.
        if (!block->numSuccessors()) {
            NodeSet seen;
            addBytecodeLiveness(graph, block->ssa->availabilityAtTail, seen, block->last());
            liveAtTail[block] = seen;
        }
    }
    
    // Now compute the liveAtTail by unifying the liveAtHead of the successors.
    for (BasicBlock* block : graph.blocksInNaturalOrder()) {
        for (BasicBlock* successor : block->successors()) {
            for (Node* node : liveAtHead[successor])
                liveAtTail[block].addVoid(node);
        }
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

