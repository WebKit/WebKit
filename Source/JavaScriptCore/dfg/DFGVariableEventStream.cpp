/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DFGVariableEventStream.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGValueSource.h"
#include <wtf/DataLog.h>

namespace JSC { namespace DFG {

void VariableEventStream::logEvent(const VariableEvent& event)
{
    dataLogF("seq#%u:", static_cast<unsigned>(size()));
    event.dump(WTF::dataFile());
    dataLogF(" ");
}

struct MinifiedGenerationInfo {
    bool filled; // true -> in gpr/fpr/pair, false -> spilled
    VariableRepresentation u;
    DataFormat format;
    
    MinifiedGenerationInfo()
        : format(DataFormatNone)
    {
    }
    
    void update(const VariableEvent& event)
    {
        switch (event.kind()) {
        case BirthToFill:
        case Fill:
            filled = true;
            break;
        case BirthToSpill:
        case Spill:
            filled = false;
            break;
        case Death:
            format = DataFormatNone;
            return;
        default:
            return;
        }
        
        u = event.variableRepresentation();
        format = event.dataFormat();
    }
};

void VariableEventStream::reconstruct(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, MinifiedGraph& graph,
    unsigned index, Operands<ValueRecovery>& valueRecoveries) const
{
    ASSERT(codeBlock->getJITType() == JITCode::DFGJIT);
    CodeBlock* baselineCodeBlock = codeBlock->baselineVersion();
    
    unsigned numVariables;
    if (codeOrigin.inlineCallFrame)
        numVariables = baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame)->m_numCalleeRegisters + codeOrigin.inlineCallFrame->stackOffset;
    else
        numVariables = baselineCodeBlock->m_numCalleeRegisters;
    
    // Crazy special case: if we're at index == 0 then this must be an argument check
    // failure, in which case all variables are already set up. The recoveries should
    // reflect this.
    if (!index) {
        valueRecoveries = Operands<ValueRecovery>(codeBlock->numParameters(), numVariables);
        for (size_t i = 0; i < valueRecoveries.size(); ++i)
            valueRecoveries[i] = ValueRecovery::alreadyInJSStack();
        return;
    }
    
    // Step 1: Find the last checkpoint, and figure out the number of virtual registers as we go.
    unsigned startIndex = index - 1;
    while (at(startIndex).kind() != Reset)
        startIndex--;
    
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("Computing OSR exit recoveries starting at seq#%u.\n", startIndex);
#endif

    // Step 2: Create a mock-up of the DFG's state and execute the events.
    Operands<ValueSource> operandSources(codeBlock->numParameters(), numVariables);
    Vector<MinifiedGenerationInfo, 32> generationInfos(graph.originalGraphSize());
    for (unsigned i = startIndex; i < index; ++i) {
        const VariableEvent& event = at(i);
        switch (event.kind()) {
        case Reset:
            // nothing to do.
            break;
        case BirthToFill:
        case BirthToSpill:
        case Fill:
        case Spill:
        case Death:
            generationInfos[event.nodeIndex()].update(event);
            break;
        case MovHint:
            if (operandSources.hasOperand(event.operand()))
                operandSources.setOperand(event.operand(), ValueSource(event.nodeIndex()));
            break;
        case SetLocalEvent:
            if (operandSources.hasOperand(event.operand()))
                operandSources.setOperand(event.operand(), ValueSource::forDataFormat(event.dataFormat()));
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    // Step 3: Record the things that are live, so we can get to them more quickly.
    Vector<unsigned, 16> indicesOfLiveThings;
    for (unsigned i = 0; i < generationInfos.size(); ++i) {
        if (generationInfos[i].format != DataFormatNone)
            indicesOfLiveThings.append(i);
    }
    
    // Step 4: Compute value recoveries!
    valueRecoveries = Operands<ValueRecovery>(codeBlock->numParameters(), numVariables);
    for (unsigned i = 0; i < operandSources.size(); ++i) {
        ValueSource& source = operandSources[i];
        if (source.isTriviallyRecoverable()) {
            valueRecoveries[i] = source.valueRecovery();
            continue;
        }
        
        ASSERT(source.kind() == HaveNode);
        MinifiedNode* node = graph.at(source.nodeIndex());
        if (node) {
            if (node->hasConstantNumber()) {
                valueRecoveries[i] = ValueRecovery::constant(
                    codeBlock->constantRegister(
                        FirstConstantRegisterIndex + node->constantNumber()).get());
                continue;
            }
            if (node->hasWeakConstant()) {
                valueRecoveries[i] = ValueRecovery::constant(node->weakConstant());
                continue;
            }
            if (node->op() == PhantomArguments) {
                valueRecoveries[i] = ValueRecovery::argumentsThatWereNotCreated();
                continue;
            }
        }
        
        MinifiedGenerationInfo* info = &generationInfos[source.nodeIndex()];
        if (info->format == DataFormatNone) {
            // Try to see if there is an alternate node that would contain the value we want.
            // There are four possibilities:
            //
            // Int32ToDouble: We can use this in place of the original node, but
            //    we'd rather not; so we use it only if it is the only remaining
            //    live version.
            //
            // ValueToInt32: If the only remaining live version of the value is
            //    ValueToInt32, then we can use it.
            //
            // UInt32ToNumber: If the only live version of the value is a UInt32ToNumber
            //    then the only remaining uses are ones that want a properly formed number
            //    rather than a UInt32 intermediate.
            //
            // DoubleAsInt32: Same as UInt32ToNumber.
            //
            // The reverse of the above: This node could be a UInt32ToNumber, but its
            //    alternative is still alive. This means that the only remaining uses of
            //    the number would be fine with a UInt32 intermediate.
            
            bool found = false;
            
            if (node && node->op() == UInt32ToNumber) {
                NodeIndex nodeIndex = node->child1();
                node = graph.at(nodeIndex);
                info = &generationInfos[nodeIndex];
                if (info->format != DataFormatNone)
                    found = true;
            }
            
            if (!found) {
                NodeIndex int32ToDoubleIndex = NoNode;
                NodeIndex valueToInt32Index = NoNode;
                NodeIndex uint32ToNumberIndex = NoNode;
                NodeIndex doubleAsInt32Index = NoNode;
                
                for (unsigned i = 0; i < indicesOfLiveThings.size(); ++i) {
                    NodeIndex nodeIndex = indicesOfLiveThings[i];
                    node = graph.at(nodeIndex);
                    if (!node)
                        continue;
                    if (!node->hasChild1())
                        continue;
                    if (node->child1() != source.nodeIndex())
                        continue;
                    ASSERT(generationInfos[nodeIndex].format != DataFormatNone);
                    switch (node->op()) {
                    case Int32ToDouble:
                        int32ToDoubleIndex = nodeIndex;
                        break;
                    case ValueToInt32:
                        valueToInt32Index = nodeIndex;
                        break;
                    case UInt32ToNumber:
                        uint32ToNumberIndex = nodeIndex;
                        break;
                    case DoubleAsInt32:
                        doubleAsInt32Index = nodeIndex;
                        break;
                    default:
                        break;
                    }
                }
                
                NodeIndex nodeIndexToUse;
                if (doubleAsInt32Index != NoNode)
                    nodeIndexToUse = doubleAsInt32Index;
                else if (int32ToDoubleIndex != NoNode)
                    nodeIndexToUse = int32ToDoubleIndex;
                else if (valueToInt32Index != NoNode)
                    nodeIndexToUse = valueToInt32Index;
                else if (uint32ToNumberIndex != NoNode)
                    nodeIndexToUse = uint32ToNumberIndex;
                else
                    nodeIndexToUse = NoNode;
                
                if (nodeIndexToUse != NoNode) {
                    info = &generationInfos[nodeIndexToUse];
                    ASSERT(info->format != DataFormatNone);
                    found = true;
                }
            }
            
            if (!found) {
                valueRecoveries[i] = ValueRecovery::constant(jsUndefined());
                continue;
            }
        }
        
        ASSERT(info->format != DataFormatNone);
        
        if (info->filled) {
            if (info->format == DataFormatDouble) {
                valueRecoveries[i] = ValueRecovery::inFPR(info->u.fpr);
                continue;
            }
#if USE(JSVALUE32_64)
            if (info->format & DataFormatJS) {
                valueRecoveries[i] = ValueRecovery::inPair(info->u.pair.tagGPR, info->u.pair.payloadGPR);
                continue;
            }
#endif
            valueRecoveries[i] = ValueRecovery::inGPR(info->u.gpr, info->format);
            continue;
        }
        
        valueRecoveries[i] =
            ValueRecovery::displacedInJSStack(static_cast<VirtualRegister>(info->u.virtualReg), info->format);
    }
    
    // Step 5: Make sure that for locals that coincide with true call frame headers, the exit compiler knows
    // that those values don't have to be recovered. Signal this by using ValueRecovery::alreadyInJSStack()
    for (InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->caller.inlineCallFrame) {
        for (unsigned i = JSStack::CallFrameHeaderSize; i--;)
            valueRecoveries.setLocal(inlineCallFrame->stackOffset - i - 1, ValueRecovery::alreadyInJSStack());
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

