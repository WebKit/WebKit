/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "DFGOSRAvailabilityAnalysisPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class OSRAvailabilityAnalysisPhase : public Phase {
public:
    OSRAvailabilityAnalysisPhase(Graph& graph)
        : Phase(graph, "OSR availability analysis")
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            block->ssa->availabilityAtHead.clear();
            block->ssa->availabilityAtTail.clear();
        }
        
        BasicBlock* root = m_graph.block(0);
        root->ssa->availabilityAtHead.m_locals.fill(Availability::unavailable());
        for (unsigned argument = m_graph.m_argumentFormats.size(); argument--;) {
            FlushedAt flushedAt = FlushedAt(
                m_graph.m_argumentFormats[argument],
                virtualRegisterForArgument(argument));
            root->ssa->availabilityAtHead.m_locals.argument(argument) = Availability(flushedAt);
        }

        // This could be made more efficient by processing blocks in reverse postorder.
        
        LocalOSRAvailabilityCalculator calculator;
        bool changed;
        do {
            changed = false;
            
            for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                
                calculator.beginBlock(block);
                
                for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex)
                    calculator.executeNode(block->at(nodeIndex));
                
                if (calculator.m_availability == block->ssa->availabilityAtTail)
                    continue;
                
                block->ssa->availabilityAtTail = calculator.m_availability;
                changed = true;
                
                for (unsigned successorIndex = block->numSuccessors(); successorIndex--;) {
                    BasicBlock* successor = block->successor(successorIndex);
                    successor->ssa->availabilityAtHead.merge(calculator.m_availability);
                    successor->ssa->availabilityAtHead.pruneByLiveness(
                        m_graph, successor->at(0)->origin.forExit);
                }
            }
        } while (changed);
        
        return true;
    }
};

bool performOSRAvailabilityAnalysis(Graph& graph)
{
    return runPhase<OSRAvailabilityAnalysisPhase>(graph);
}

LocalOSRAvailabilityCalculator::LocalOSRAvailabilityCalculator()
{
}

LocalOSRAvailabilityCalculator::~LocalOSRAvailabilityCalculator()
{
}

void LocalOSRAvailabilityCalculator::beginBlock(BasicBlock* block)
{
    m_availability = block->ssa->availabilityAtHead;
}

void LocalOSRAvailabilityCalculator::endBlock(BasicBlock* block)
{
    m_availability = block->ssa->availabilityAtTail;
}

void LocalOSRAvailabilityCalculator::executeNode(Node* node)
{
    switch (node->op()) {
    case PutStack: {
        StackAccessData* data = node->stackAccessData();
        m_availability.m_locals.operand(data->local).setFlush(data->flushedAt());
        break;
    }
        
    case KillStack: {
        m_availability.m_locals.operand(node->unlinkedLocal()).setFlush(FlushedAt(ConflictingFlush));
        break;
    }

    case GetStack: {
        StackAccessData* data = node->stackAccessData();
        m_availability.m_locals.operand(data->local) = Availability(node, data->flushedAt());
        break;
    }

    case MovHint: {
        m_availability.m_locals.operand(node->unlinkedLocal()).setNode(node->child1().node());
        break;
    }

    case ZombieHint: {
        m_availability.m_locals.operand(node->unlinkedLocal()).setNodeUnavailable();
        break;
    }
        
    case LoadVarargs:
    case ForwardVarargs: {
        LoadVarargsData* data = node->loadVarargsData();
        m_availability.m_locals.operand(data->count) =
            Availability(FlushedAt(FlushedInt32, data->machineCount));
        for (unsigned i = data->limit; i--;) {
            m_availability.m_locals.operand(VirtualRegister(data->start.offset() + i)) =
                Availability(FlushedAt(FlushedJSValue, VirtualRegister(data->machineStart.offset() + i)));
        }
        break;
    }
        
    case PhantomDirectArguments:
    case PhantomClonedArguments: {
        InlineCallFrame* inlineCallFrame = node->origin.semantic.inlineCallFrame;
        if (!inlineCallFrame) {
            // We don't need to record anything about how the arguments are to be recovered. It's just a
            // given that we can read them from the stack.
            break;
        }
        
        if (inlineCallFrame->isVarargs()) {
            // Record how to read each argument and the argument count.
            Availability argumentCount =
                m_availability.m_locals.operand(inlineCallFrame->stackOffset + CallFrameSlot::argumentCount);
            
            m_availability.m_heap.set(PromotedHeapLocation(ArgumentCountPLoc, node), argumentCount);
        }
        
        if (inlineCallFrame->isClosureCall) {
            Availability callee = m_availability.m_locals.operand(
                inlineCallFrame->stackOffset + CallFrameSlot::callee);
            m_availability.m_heap.set(PromotedHeapLocation(ArgumentsCalleePLoc, node), callee);
        }
        
        for (unsigned i = 0; i < inlineCallFrame->arguments.size() - 1; ++i) {
            Availability argument = m_availability.m_locals.operand(
                inlineCallFrame->stackOffset + CallFrame::argumentOffset(i));
            
            m_availability.m_heap.set(PromotedHeapLocation(ArgumentPLoc, node, i), argument);
        }
        break;
    }
        
    case PutHint: {
        m_availability.m_heap.set(
            PromotedHeapLocation(node->child1().node(), node->promotedLocationDescriptor()),
            Availability(node->child2().node()));
        break;
    }
        
    default:
        break;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

