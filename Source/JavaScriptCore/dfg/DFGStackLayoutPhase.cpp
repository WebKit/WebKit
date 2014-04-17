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
#include "DFGStackLayoutPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"
#include "DFGValueSource.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class StackLayoutPhase : public Phase {
    static const bool verbose = false;
    
public:
    StackLayoutPhase(Graph& graph)
        : Phase(graph, "stack layout")
    {
    }
    
    bool run()
    {
        SymbolTable* symbolTable = codeBlock()->symbolTable();

        // This enumerates the locals that we actually care about and packs them. So for example
        // if we use local 1, 3, 4, 5, 7, then we remap them: 1->0, 3->1, 4->2, 5->3, 7->4. We
        // treat a variable as being "used" if there exists an access to it (SetLocal, GetLocal,
        // Flush, PhantomLocal).
        
        BitVector usedLocals;
        
        // Collect those variables that are used from IR.
        bool hasGetLocalUnlinked = false;
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                Node* node = block->at(nodeIndex);
                switch (node->op()) {
                case GetLocal:
                case SetLocal:
                case Flush:
                case PhantomLocal: {
                    VariableAccessData* variable = node->variableAccessData();
                    if (variable->local().isArgument())
                        break;
                    usedLocals.set(variable->local().toLocal());
                    break;
                }
                    
                case GetLocalUnlinked: {
                    VirtualRegister operand = node->unlinkedLocal();
                    if (operand.isArgument())
                        break;
                    usedLocals.set(operand.toLocal());
                    hasGetLocalUnlinked = true;
                    break;
                }
                    
                default:
                    break;
                }
            }
        }
        
        // Ensure that captured variables and captured inline arguments are pinned down.
        // They should have been because of flushes, except that the flushes can be optimized
        // away.
        if (symbolTable) {
            for (int i = symbolTable->captureStart(); i > symbolTable->captureEnd(); i--)
                usedLocals.set(VirtualRegister(i).toLocal());
        }
        if (codeBlock()->usesArguments()) {
            usedLocals.set(codeBlock()->argumentsRegister().toLocal());
            usedLocals.set(unmodifiedArgumentsRegister(codeBlock()->argumentsRegister()).toLocal());
        }
        if (codeBlock()->uncheckedActivationRegister().isValid())
            usedLocals.set(codeBlock()->activationRegister().toLocal());
        for (InlineCallFrameSet::iterator iter = m_graph.m_plan.inlineCallFrames->begin(); !!iter; ++iter) {
            InlineCallFrame* inlineCallFrame = *iter;
            if (!inlineCallFrame->executable->usesArguments())
                continue;
            
            VirtualRegister argumentsRegister = m_graph.argumentsRegisterFor(inlineCallFrame);
            usedLocals.set(argumentsRegister.toLocal());
            usedLocals.set(unmodifiedArgumentsRegister(argumentsRegister).toLocal());
            
            for (unsigned argument = inlineCallFrame->arguments.size(); argument-- > 1;) {
                usedLocals.set(VirtualRegister(
                    virtualRegisterForArgument(argument).offset() +
                    inlineCallFrame->stackOffset).toLocal());
            }
        }
        
        Vector<unsigned> allocation(usedLocals.size());
        m_graph.m_nextMachineLocal = 0;
        for (unsigned i = 0; i < usedLocals.size(); ++i) {
            if (!usedLocals.get(i)) {
                allocation[i] = UINT_MAX;
                continue;
            }
            
            allocation[i] = m_graph.m_nextMachineLocal++;
        }
        
        for (unsigned i = m_graph.m_variableAccessData.size(); i--;) {
            VariableAccessData* variable = &m_graph.m_variableAccessData[i];
            if (!variable->isRoot())
                continue;
            
            if (variable->local().isArgument()) {
                variable->machineLocal() = variable->local();
                continue;
            }
            
            size_t local = variable->local().toLocal();
            if (local >= allocation.size())
                continue;
            
            if (allocation[local] == UINT_MAX)
                continue;
            
            variable->machineLocal() = virtualRegisterForLocal(
                allocation[variable->local().toLocal()]);
        }
        
        if (codeBlock()->usesArguments()) {
            VirtualRegister argumentsRegister = virtualRegisterForLocal(
                allocation[codeBlock()->argumentsRegister().toLocal()]);
            RELEASE_ASSERT(
                virtualRegisterForLocal(allocation[
                    unmodifiedArgumentsRegister(
                        codeBlock()->argumentsRegister()).toLocal()])
                == unmodifiedArgumentsRegister(argumentsRegister));
            codeBlock()->setArgumentsRegister(argumentsRegister);
        }
        
        if (codeBlock()->uncheckedActivationRegister().isValid()) {
            codeBlock()->setActivationRegister(
                virtualRegisterForLocal(allocation[codeBlock()->activationRegister().toLocal()]));
        }
        
        for (unsigned i = m_graph.m_inlineVariableData.size(); i--;) {
            InlineVariableData data = m_graph.m_inlineVariableData[i];
            InlineCallFrame* inlineCallFrame = data.inlineCallFrame;
            
            if (inlineCallFrame->executable->usesArguments()) {
                inlineCallFrame->argumentsRegister = virtualRegisterForLocal(
                    allocation[m_graph.argumentsRegisterFor(inlineCallFrame).toLocal()]);

                RELEASE_ASSERT(
                    virtualRegisterForLocal(allocation[unmodifiedArgumentsRegister(
                        m_graph.argumentsRegisterFor(inlineCallFrame)).toLocal()])
                    == unmodifiedArgumentsRegister(inlineCallFrame->argumentsRegister));
            }
            
            for (unsigned argument = inlineCallFrame->arguments.size(); argument-- > 1;) {
                ArgumentPosition& position = m_graph.m_argumentPositions[
                    data.argumentPositionStart + argument];
                VariableAccessData* variable = position.someVariable();
                ValueSource source;
                if (!variable)
                    source = ValueSource(SourceIsDead);
                else {
                    source = ValueSource::forFlushFormat(
                        variable->machineLocal(), variable->flushFormat());
                }
                inlineCallFrame->arguments[argument] = source.valueRecovery();
            }
            
            RELEASE_ASSERT(inlineCallFrame->isClosureCall == !!data.calleeVariable);
            if (inlineCallFrame->isClosureCall) {
                VariableAccessData* variable = data.calleeVariable->find();
                ValueSource source = ValueSource::forFlushFormat(
                    variable->machineLocal(),
                    variable->flushFormat());
                inlineCallFrame->calleeRecovery = source.valueRecovery();
            } else
                RELEASE_ASSERT(inlineCallFrame->calleeRecovery.isConstant());
        }
        
        if (symbolTable) {
            if (symbolTable->captureCount()) {
                unsigned captureStartLocal = allocation[
                    VirtualRegister(codeBlock()->symbolTable()->captureStart()).toLocal()];
                ASSERT(captureStartLocal != UINT_MAX);
                m_graph.m_machineCaptureStart = virtualRegisterForLocal(captureStartLocal).offset();
            } else
                m_graph.m_machineCaptureStart = virtualRegisterForLocal(0).offset();
        
            // This is an abomination. If we had captured an argument then the argument ends
            // up being "slow", meaning that loads of the argument go through an extra lookup
            // table.
            if (const SlowArgument* slowArguments = symbolTable->slowArguments()) {
                auto newSlowArguments = std::make_unique<SlowArgument[]>(
                    symbolTable->parameterCount());
                for (size_t i = symbolTable->parameterCount(); i--;) {
                    newSlowArguments[i] = slowArguments[i];
                    VirtualRegister reg = VirtualRegister(slowArguments[i].index);
                    if (reg.isLocal())
                        newSlowArguments[i].index = virtualRegisterForLocal(allocation[reg.toLocal()]).offset();
                }
            
                m_graph.m_slowArguments = std::move(newSlowArguments);
            }
        }
        
        // Fix GetLocalUnlinked's variable references.
        if (hasGetLocalUnlinked) {
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                    Node* node = block->at(nodeIndex);
                    switch (node->op()) {
                    case GetLocalUnlinked: {
                        VirtualRegister operand = node->unlinkedLocal();
                        if (operand.isLocal())
                            operand = virtualRegisterForLocal(allocation[operand.toLocal()]);
                        node->setUnlinkedMachineLocal(operand);
                        break;
                    }
                        
                    default:
                        break;
                    }
                }
            }
        }
        
        return true;
    }
};

bool performStackLayout(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Stack Layout Phase");
    return runPhase<StackLayoutPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

