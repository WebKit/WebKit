/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include "DFGJITCode.h"
#include "DFGValueSource.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>

namespace JSC { namespace DFG {

void VariableEventStream::logEvent(const VariableEvent& event)
{
    dataLogF("seq#%u:", static_cast<unsigned>(size()));
    event.dump(WTF::dataFile());
    dataLogF(" ");
}

namespace {

struct MinifiedGenerationInfo {
    bool filled; // true -> in gpr/fpr/pair, false -> spilled
    bool alive;
    VariableRepresentation u;
    DataFormat format;
    
    MinifiedGenerationInfo()
        : filled(false)
        , alive(false)
        , format(DataFormatNone)
    {
    }
    
    void update(const VariableEvent& event)
    {
        switch (event.kind()) {
        case BirthToFill:
        case Fill:
            filled = true;
            alive = true;
            break;
        case BirthToSpill:
        case Spill:
            filled = false;
            alive = true;
            break;
        case Birth:
            alive = true;
            return;
        case Death:
            format = DataFormatNone;
            alive = false;
            return;
        default:
            return;
        }
        
        u = event.variableRepresentation();
        format = event.dataFormat();
    }
};

} // namespace

static bool tryToSetConstantRecovery(ValueRecovery& recovery, MinifiedNode* node)
{
    if (!node)
        return false;
    
    if (node->hasConstant()) {
        recovery = ValueRecovery::constant(node->constant());
        return true;
    }
    
    if (node->isPhantomDirectArguments()) {
        recovery = ValueRecovery::directArgumentsThatWereNotCreated(node->id());
        return true;
    }
    
    if (node->isPhantomClonedArguments()) {
        recovery = ValueRecovery::clonedArgumentsThatWereNotCreated(node->id());
        return true;
    }
    
    return false;
}

template<VariableEventStream::ReconstructionStyle style>
unsigned VariableEventStream::reconstruct(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, MinifiedGraph& graph,
    unsigned index, Operands<ValueRecovery>& valueRecoveries, Vector<UndefinedOperandSpan>* undefinedOperandSpans) const
{
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    CodeBlock* baselineCodeBlock = codeBlock->baselineVersion();

    unsigned numVariables;
    static const unsigned invalidIndex = std::numeric_limits<unsigned>::max();
    unsigned firstUndefined = invalidIndex;
    bool firstUndefinedIsArgument = false;

    auto flushUndefinedOperandSpan = [&] (unsigned i) {
        if (firstUndefined == invalidIndex)
            return;
        int firstOffset = valueRecoveries.virtualRegisterForIndex(firstUndefined).offset();
        int lastOffset = valueRecoveries.virtualRegisterForIndex(i - 1).offset();
        int minOffset = std::min(firstOffset, lastOffset);
        undefinedOperandSpans->append({ firstUndefined, minOffset, i - firstUndefined });
        firstUndefined = invalidIndex;
    };
    auto recordUndefinedOperand = [&] (unsigned i) {
        // We want to separate the span of arguments from the span of locals even if they have adjacent operands indexes.
        if (firstUndefined != invalidIndex && firstUndefinedIsArgument != valueRecoveries.isArgument(i))
            flushUndefinedOperandSpan(i);

        if (firstUndefined == invalidIndex) {
            firstUndefined = i;
            firstUndefinedIsArgument = valueRecoveries.isArgument(i);
        }
    };

    auto* inlineCallFrame = codeOrigin.inlineCallFrame();
    if (inlineCallFrame)
        numVariables = baselineCodeBlockForInlineCallFrame(inlineCallFrame)->numCalleeLocals() + VirtualRegister(inlineCallFrame->stackOffset).toLocal() + 1;
    else
        numVariables = baselineCodeBlock->numCalleeLocals();
    
    // Crazy special case: if we're at index == 0 then this must be an argument check
    // failure, in which case all variables are already set up. The recoveries should
    // reflect this.
    if (!index) {
        valueRecoveries = Operands<ValueRecovery>(codeBlock->numParameters(), numVariables);
        for (size_t i = 0; i < valueRecoveries.size(); ++i) {
            valueRecoveries[i] = ValueRecovery::displacedInJSStack(
                VirtualRegister(valueRecoveries.operandForIndex(i)), DataFormatJS);
        }
        return numVariables;
    }
    
    // Step 1: Find the last checkpoint, and figure out the number of virtual registers as we go.
    unsigned startIndex = index - 1;
    while (at(startIndex).kind() != Reset)
        startIndex--;
    
    // Step 2: Create a mock-up of the DFG's state and execute the events.
    Operands<ValueSource> operandSources(codeBlock->numParameters(), numVariables);
    for (unsigned i = operandSources.size(); i--;)
        operandSources[i] = ValueSource(SourceIsDead);
    HashMap<MinifiedID, MinifiedGenerationInfo> generationInfos;
    for (unsigned i = startIndex; i < index; ++i) {
        const VariableEvent& event = at(i);
        switch (event.kind()) {
        case Reset:
            // nothing to do.
            break;
        case BirthToFill:
        case BirthToSpill:
        case Birth: {
            MinifiedGenerationInfo info;
            info.update(event);
            generationInfos.add(event.id(), info);
            break;
        }
        case Fill:
        case Spill:
        case Death: {
            HashMap<MinifiedID, MinifiedGenerationInfo>::iterator iter = generationInfos.find(event.id());
            ASSERT(iter != generationInfos.end());
            iter->value.update(event);
            break;
        }
        case MovHintEvent:
            if (operandSources.hasOperand(event.bytecodeRegister()))
                operandSources.setOperand(event.bytecodeRegister(), ValueSource(event.id()));
            break;
        case SetLocalEvent:
            if (operandSources.hasOperand(event.bytecodeRegister()))
                operandSources.setOperand(event.bytecodeRegister(), ValueSource::forDataFormat(event.machineRegister(), event.dataFormat()));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    // Step 3: Compute value recoveries!
    valueRecoveries = Operands<ValueRecovery>(codeBlock->numParameters(), numVariables);
    for (unsigned i = 0; i < operandSources.size(); ++i) {
        ValueSource& source = operandSources[i];
        if (source.isTriviallyRecoverable()) {
            valueRecoveries[i] = source.valueRecovery();
            if (style == ReconstructionStyle::Separated) {
                if (valueRecoveries[i].isConstant() && valueRecoveries[i].constant() == jsUndefined())
                    recordUndefinedOperand(i);
                else
                    flushUndefinedOperandSpan(i);
            }
            continue;
        }
        
        ASSERT(source.kind() == HaveNode);
        MinifiedNode* node = graph.at(source.id());
        MinifiedGenerationInfo info = generationInfos.get(source.id());
        if (!info.alive) {
            valueRecoveries[i] = ValueRecovery::constant(jsUndefined());
            if (style == ReconstructionStyle::Separated)
                recordUndefinedOperand(i);
            continue;
        }

        if (tryToSetConstantRecovery(valueRecoveries[i], node)) {
            if (style == ReconstructionStyle::Separated) {
                if (node->hasConstant() && node->constant() == jsUndefined())
                    recordUndefinedOperand(i);
                else
                    flushUndefinedOperandSpan(i);
            }
            continue;
        }
        
        ASSERT(info.format != DataFormatNone);
        if (style == ReconstructionStyle::Separated)
            flushUndefinedOperandSpan(i);

        if (info.filled) {
            if (info.format == DataFormatDouble) {
                valueRecoveries[i] = ValueRecovery::inFPR(info.u.fpr, DataFormatDouble);
                continue;
            }
#if USE(JSVALUE32_64)
            if (info.format & DataFormatJS) {
                valueRecoveries[i] = ValueRecovery::inPair(info.u.pair.tagGPR, info.u.pair.payloadGPR);
                continue;
            }
#endif
            valueRecoveries[i] = ValueRecovery::inGPR(info.u.gpr, info.format);
            continue;
        }
        
        valueRecoveries[i] =
            ValueRecovery::displacedInJSStack(static_cast<VirtualRegister>(info.u.virtualReg), info.format);
    }
    if (style == ReconstructionStyle::Separated)
        flushUndefinedOperandSpan(operandSources.size());

    return numVariables;
}

unsigned VariableEventStream::reconstruct(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, MinifiedGraph& graph,
    unsigned index, Operands<ValueRecovery>& valueRecoveries) const
{
    return reconstruct<ReconstructionStyle::Combined>(codeBlock, codeOrigin, graph, index, valueRecoveries, nullptr);
}

unsigned VariableEventStream::reconstruct(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, MinifiedGraph& graph,
    unsigned index, Operands<ValueRecovery>& valueRecoveries, Vector<UndefinedOperandSpan>* undefinedOperandSpans) const
{
    return reconstruct<ReconstructionStyle::Separated>(codeBlock, codeOrigin, graph, index, valueRecoveries, undefinedOperandSpans);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

