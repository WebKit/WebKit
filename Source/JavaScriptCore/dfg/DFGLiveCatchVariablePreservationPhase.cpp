/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "DFGLiveCatchVariablePreservationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGBlockSet.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "FullBytecodeLiveness.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class LiveCatchVariablePreservationPhase : public Phase {
public:
    LiveCatchVariablePreservationPhase(Graph& graph)
        : Phase(graph, "live catch variable preservation phase")
    {
    }

    bool run()
    {
        DFG_ASSERT(m_graph, nullptr, m_graph.m_form == LoadStore);

        if (!m_graph.m_hasExceptionHandlers)
            return false;

        InsertionSet insertionSet(m_graph);
        if (m_graph.m_hasExceptionHandlers) {
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                handleBlockForTryCatch(block, insertionSet);
                insertionSet.execute(block);
            }
        }

        return true;
    }

    bool isValidFlushLocation(BasicBlock* startingBlock, unsigned index, VirtualRegister operand)
    {
        // This code is not meant to be fast. We just use it for assertions. If we got liveness wrong,
        // this function would return false for a Flush that we insert.
        Vector<BasicBlock*, 4> worklist;
        BlockSet seen;

        auto addPredecessors = [&] (BasicBlock* block) {
            for (BasicBlock* predecessor : block->predecessors) {
                bool isNewEntry = seen.add(predecessor);
                if (isNewEntry)
                    worklist.append(predecessor);
            }
        };

        auto flushIsDefinitelyInvalid = [&] (BasicBlock* block, unsigned index) {
            bool allGood = false;
            for (unsigned i = index; i--; ) {
                if (block->at(i)->accessesStack(m_graph) && block->at(i)->local() == operand) {
                    allGood = true;
                    break;
                }
            }

            if (allGood)
                return false;

            if (block->predecessors.isEmpty()) {
                // This is a root block. We proved we reached here, therefore we can't Flush, as
                // it'll make this local live at the start of a root block, which is invalid IR.
                return true;
            }

            addPredecessors(block);
            return false;
        };

        if (flushIsDefinitelyInvalid(startingBlock, index))
            return false;

        while (!worklist.isEmpty()) {
            BasicBlock* block = worklist.takeLast();
            if (flushIsDefinitelyInvalid(block, block->size()))
                return false;
        }
        return true;
    }


    void handleBlockForTryCatch(BasicBlock* block, InsertionSet& insertionSet)
    {
        HandlerInfo* currentExceptionHandler = nullptr;
        FastBitVector liveAtCatchHead;
        liveAtCatchHead.resize(m_graph.block(0)->variablesAtTail.numberOfLocals());

        HandlerInfo* cachedHandlerResult;
        CodeOrigin cachedCodeOrigin;
        auto catchHandler = [&] (CodeOrigin origin) -> HandlerInfo* {
            ASSERT(origin);
            if (origin == cachedCodeOrigin)
                return cachedHandlerResult;

            unsigned bytecodeIndexToCheck = origin.bytecodeIndex();

            cachedCodeOrigin = origin;

            while (1) {
                InlineCallFrame* inlineCallFrame = origin.inlineCallFrame();
                CodeBlock* codeBlock = m_graph.baselineCodeBlockFor(inlineCallFrame);
                if (HandlerInfo* handler = codeBlock->handlerForBytecodeOffset(bytecodeIndexToCheck)) {
                    liveAtCatchHead.clearAll();

                    unsigned catchBytecodeIndex = handler->target;
                    m_graph.forAllLocalsLiveInBytecode(CodeOrigin(catchBytecodeIndex, inlineCallFrame), [&] (VirtualRegister operand) {
                        liveAtCatchHead[operand.toLocal()] = true;
                    });

                    cachedHandlerResult = handler;
                    break;
                }

                if (!inlineCallFrame) {
                    cachedHandlerResult = nullptr;
                    break;
                }

                bytecodeIndexToCheck = inlineCallFrame->directCaller.bytecodeIndex();
                origin = inlineCallFrame->directCaller;
            }

            return cachedHandlerResult;
        };

        Operands<VariableAccessData*> currentBlockAccessData(block->variablesAtTail.numberOfArguments(), block->variablesAtTail.numberOfLocals(), nullptr);

        auto flushEverything = [&] (NodeOrigin origin, unsigned index) {
            RELEASE_ASSERT(currentExceptionHandler);
            auto flush = [&] (VirtualRegister operand) {
                if ((operand.isLocal() && liveAtCatchHead[operand.toLocal()]) || operand.isArgument()) {

                    ASSERT(isValidFlushLocation(block, index, operand));

                    VariableAccessData* accessData = currentBlockAccessData.operand(operand);
                    if (!accessData)
                        accessData = newVariableAccessData(operand);

                    currentBlockAccessData.operand(operand) = accessData;

                    insertionSet.insertNode(index, SpecNone, 
                        Flush, origin, OpInfo(accessData));
                }
            };

            for (unsigned local = 0; local < block->variablesAtTail.numberOfLocals(); local++)
                flush(virtualRegisterForLocal(local));
            flush(VirtualRegister(CallFrame::thisArgumentOffset()));
        };

        for (unsigned nodeIndex = 0; nodeIndex < block->size(); nodeIndex++) {
            Node* node = block->at(nodeIndex);

            {
                HandlerInfo* newHandler = catchHandler(node->origin.semantic);
                if (newHandler != currentExceptionHandler && currentExceptionHandler)
                    flushEverything(node->origin, nodeIndex);
                currentExceptionHandler = newHandler;
            }

            if (currentExceptionHandler && (node->op() == SetLocal || node->op() == SetArgumentDefinitely || node->op() == SetArgumentMaybe)) {
                VirtualRegister operand = node->local();
                if ((operand.isLocal() && liveAtCatchHead[operand.toLocal()]) || operand.isArgument()) {

                    ASSERT(isValidFlushLocation(block, nodeIndex, operand));

                    VariableAccessData* variableAccessData = currentBlockAccessData.operand(operand);
                    if (!variableAccessData)
                        variableAccessData = newVariableAccessData(operand);

                    insertionSet.insertNode(nodeIndex, SpecNone, 
                        Flush, node->origin, OpInfo(variableAccessData));
                }
            }

            if (node->accessesStack(m_graph))
                currentBlockAccessData.operand(node->local()) = node->variableAccessData();
        }

        if (currentExceptionHandler) {
            NodeOrigin origin = block->at(block->size() - 1)->origin;
            flushEverything(origin, block->size());
        }
    }

    VariableAccessData* newVariableAccessData(VirtualRegister operand)
    {
        ASSERT(!operand.isConstant());
        
        m_graph.m_variableAccessData.append(operand);
        return &m_graph.m_variableAccessData.last();
    }
};

bool performLiveCatchVariablePreservationPhase(Graph& graph)
{
    return runPhase<LiveCatchVariablePreservationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
