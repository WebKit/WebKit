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

#include "DFGBlockMapInlines.h"
#include "DFGMayExit.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"

namespace JSC { namespace DFG {

template<typename HeadFunctor, typename TailFunctor>
class OSRAvailabilityAnalysisPhase : public Phase {
    static constexpr bool verbose = false;
public:
    OSRAvailabilityAnalysisPhase(Graph& graph, HeadFunctor& availabilityAtHead, TailFunctor& availabilityAtTail)
        : Phase(graph, "OSR availability analysis")
        , availabilityAtHead(availabilityAtHead)
        , availabilityAtTail(availabilityAtTail)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            availabilityAtHead(block).clear();
            availabilityAtTail(block).clear();
        }
        
        BasicBlock* root = m_graph.block(0);
        availabilityAtHead(root).m_locals.fill(Availability::unavailable());

        for (unsigned argument = 0; argument < m_graph.block(0)->valuesAtHead.numberOfArguments(); ++argument)
            availabilityAtHead(root).m_locals.argument(argument) = Availability(FlushedAt(FlushedJSValue, virtualRegisterForArgumentIncludingThis(argument)));

        // This could be made more efficient by processing blocks in reverse postorder.
        
        auto dumpAvailability = [&] (BasicBlock* block) {
            dataLogLn("Head: ", availabilityAtHead(block));
            dataLogLn("Tail: ", availabilityAtTail(block));
        };

        auto dumpBytecodeLivenessAtHead = [&] (BasicBlock* block) {
            dataLog("Live: ");
            m_graph.forAllLiveInBytecode(
                block->at(0)->origin.forExit,
                [&] (Operand operand) {
                    dataLog(operand, " ");
                });
            dataLogLn("");
        };

        LocalOSRAvailabilityCalculator calculator(m_graph);
        bool changed;
        do {
            changed = false;
            
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                if (verbose) {
                    dataLogLn("Before changing Block #", block->index);
                    dumpAvailability(block);
                }

                calculator.m_availability = availabilityAtHead(block);
                
                for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex)
                    calculator.executeNode(block->at(nodeIndex));
                
                if (calculator.m_availability == availabilityAtTail(block))
                    continue;
                
                availabilityAtTail(block) = calculator.m_availability;
                changed = true;

                if (verbose) {
                    dataLogLn("After changing Block #", block->index);
                    dumpAvailability(block);
                }

                for (unsigned successorIndex = block->numSuccessors(); successorIndex--;) {
                    BasicBlock* successor = block->successor(successorIndex);
                    availabilityAtHead(successor).merge(calculator.m_availability);
                }

                for (unsigned successorIndex = block->numSuccessors(); successorIndex--;) {
                    BasicBlock* successor = block->successor(successorIndex);
                    availabilityAtHead(successor).pruneByLiveness(
                        m_graph, successor->at(0)->origin.forExit);
                    if (verbose) {
                        dataLogLn("After pruning Block #", successor->index);
                        dumpAvailability(successor);
                        dumpBytecodeLivenessAtHead(successor);
                    }
                }
            }
        } while (changed);

        if (validationEnabled()) {

            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                calculator.m_availability = availabilityAtHead(block);
                
                for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                    Node* node = block->at(nodeIndex);
                    // FIXME: The mayExit status of a node doesn't seem like it should mean we don't need to have everything available.
                    if (mayExit(m_graph, node) != DoesNotExit && node->origin.exitOK) {
                        // If we're allowed to exit here, the heap must be in a state
                        // where exiting wouldn't crash. These particular fields are
                        // required for correctness because we use them during OSR exit
                        // to do meaningful things. It would be wrong for any of them
                        // to be dead.

                        CodeOrigin exitOrigin = node->origin.forExit;
                        AvailabilityMap& availabilityMap = calculator.m_availability;
                        availabilityMap.pruneByLiveness(m_graph, exitOrigin);

                        for (auto heapPair : availabilityMap.m_heap) {
                            switch (heapPair.key.kind()) {
                            case ActivationScopePLoc:
                            case ActivationSymbolTablePLoc:
                            case FunctionActivationPLoc:
                            case FunctionExecutablePLoc:
                            case StructurePLoc:
                                if (heapPair.value.isDead()) {
                                    dataLogLn("PromotedHeapLocation is dead, but should not be: ", heapPair.key);
                                    availabilityMap.dump(WTF::dataFile());
                                    CRASH();
                                }
                                break;

                            default:
                                break;
                            }
                        }

                        // FIXME: It seems like we should be able to do at least some validation when OSR entering. https://bugs.webkit.org/show_bug.cgi?id=215511
                        if (m_graph.m_plan.mode() != FTLForOSREntryMode) {
                            for (size_t i = 0; i < availabilityMap.m_locals.size(); ++i) {
                                Operand operand = availabilityMap.m_locals.operandForIndex(i);
                                Availability availability = availabilityMap.m_locals[i];
                                if (availability.isDead() && m_graph.isLiveInBytecode(operand, exitOrigin))
                                    DFG_CRASH(m_graph, node, toCString("Live bytecode local not available: operand = ", operand, ", availabilityMap = ", availabilityMap, ", origin = ", exitOrigin).data());
                            }
                        }
                    }

                    calculator.executeNode(block->at(nodeIndex));
                }
            }
        }
        
        return true;
    }

    HeadFunctor& availabilityAtHead;
    TailFunctor& availabilityAtTail;
};

bool performOSRAvailabilityAnalysis(Graph& graph)
{
    auto availabilityAtHead = [&] (BasicBlock* block) -> AvailabilityMap& { return block->ssa->availabilityAtHead; };
    auto availabilityAtTail = [&] (BasicBlock* block) -> AvailabilityMap& { return block->ssa->availabilityAtTail; };
    return runPhase<OSRAvailabilityAnalysisPhase<decltype(availabilityAtHead), decltype(availabilityAtTail)>>(graph, availabilityAtHead, availabilityAtTail);
}

void validateOSRExitAvailability(Graph& graph)
{
    BlockMap<AvailabilityMap> availabilityMapAtHead(graph);
    BlockMap<AvailabilityMap> availabilityMapAtTail(graph);

    for (BasicBlock* block : graph.blocksInNaturalOrder()) {
        availabilityMapAtHead[block] = AvailabilityMap(block->ssa->availabilityAtHead);
        availabilityMapAtTail[block] = AvailabilityMap(block->ssa->availabilityAtTail);
    }

    auto availabilityAtHead = [&] (BasicBlock* block) -> AvailabilityMap& { return availabilityMapAtHead[block]; };
    auto availabilityAtTail = [&] (BasicBlock* block) -> AvailabilityMap& { return availabilityMapAtTail[block]; };
    OSRAvailabilityAnalysisPhase phase(graph, availabilityAtHead, availabilityAtTail);
    phase.run();
}

LocalOSRAvailabilityCalculator::LocalOSRAvailabilityCalculator(Graph& graph)
    : m_graph(graph)
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

    // It's somewhat subtle why we cannot use the node for the GetStack itself in the Availability's node field. The reason is that if we did we would need to make any phase that converts nodes to GetStack availability aware. For instance a place where this could come up is if you had a graph like:

    // BB#1:
    // @1: NewObject()
    // @2: MovHint(@1, inline-arg1)
    // @3: Jump(#2, #3)

    // BB#2:
    // @4: PutStack(@1, inline-arg1)
    // @5: GetMyArgumentByVal(inline-arg1)
    // @6: Jump(#3)

    // BB#3:
    // @7: InvalidationPoint()

    // If constant folding converts @5 to a GetStack then at @7 inline-arg1 won't be available since at the end of BB#1 our availability is (@1, DeadFlush) and (@5, FlushedAt(inline-arg1)). When that gets merged at BB#3 then the availability will be (nullptr, ConflictingFlush).
    case GetStack:
    case PutStack: {
        StackAccessData* data = node->stackAccessData();
        m_availability.m_locals.operand(data->operand).setFlush(data->flushedAt());
        break;
    }
        
    case KillStack: {
        m_availability.m_locals.operand(node->unlinkedOperand()).setFlush(FlushedAt(ConflictingFlush));
        break;
    }

    case MovHint: {
        m_availability.m_locals.operand(node->unlinkedOperand()).setNode(node->child1().node());
        break;
    }

    case InitializeEntrypointArguments: {
        unsigned entrypointIndex = node->entrypointIndex();
        const Vector<FlushFormat>& argumentFormats = m_graph.m_argumentFormats[entrypointIndex];
        for (unsigned argument = argumentFormats.size(); argument--; ) {
            FlushedAt flushedAt = FlushedAt(argumentFormats[argument], virtualRegisterForArgumentIncludingThis(argument));
            m_availability.m_locals.argument(argument) = Availability(flushedAt);
        }
        break;
    }

    case VarargsLength: {
        break;
    }

    case LoadVarargs:
    case ForwardVarargs: {
        LoadVarargsData* data = node->loadVarargsData();
        m_availability.m_locals.operand(data->count) = Availability(FlushedAt(FlushedInt32, data->machineCount));
        for (unsigned i = data->limit; i--;) {
            m_availability.m_locals.operand(data->start + i) =
                Availability(FlushedAt(FlushedJSValue, data->machineStart + i));
        }
        break;
    }
    
    case PhantomCreateRest:
    case PhantomDirectArguments:
    case PhantomClonedArguments: {
        InlineCallFrame* inlineCallFrame = node->origin.semantic.inlineCallFrame();
        if (!inlineCallFrame) {
            // We don't need to record anything about how the arguments are to be recovered. It's just a
            // given that we can read them from the stack.
            break;
        }

        unsigned numberOfArgumentsToSkip = 0;
        if (node->op() == PhantomCreateRest)
            numberOfArgumentsToSkip = node->numberOfArgumentsToSkip();
        
        if (inlineCallFrame->isVarargs()) {
            // Record how to read each argument and the argument count.
            Availability argumentCount =
                m_availability.m_locals.operand(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis));
            
            m_availability.m_heap.set(PromotedHeapLocation(ArgumentCountPLoc, node), argumentCount);
        }
        
        if (inlineCallFrame->isClosureCall) {
            Availability callee = m_availability.m_locals.operand(
                VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::callee));
            m_availability.m_heap.set(PromotedHeapLocation(ArgumentsCalleePLoc, node), callee);
        }
        
        for (unsigned i = numberOfArgumentsToSkip; i < static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1); ++i) {
            Availability argument = m_availability.m_locals.operand(
                VirtualRegister(inlineCallFrame->stackOffset + CallFrame::argumentOffset(i)));
            
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

    case PhantomSpread:
        m_availability.m_heap.set(PromotedHeapLocation(SpreadPLoc, node), Availability(node->child1().node()));
        break;

    case PhantomNewArrayWithSpread:
        for (unsigned i = 0; i < node->numChildren(); i++) {
            Node* child = m_graph.varArgChild(node, i).node();
            m_availability.m_heap.set(PromotedHeapLocation(NewArrayWithSpreadArgumentPLoc, node, i), Availability(child));
        }
        break;

    case PhantomNewArrayBuffer:
        m_availability.m_heap.set(PromotedHeapLocation(NewArrayBufferPLoc, node), Availability(node->child1().node()));
        break;
        
    default:
        break;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

