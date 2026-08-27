/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "InlineCallFrame.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/LEBDecoder.h>
#include <wtf/LEBEncoder.h>

namespace JSC { namespace DFG {

void VariableEventStreamBuilder::logEvent(const VariableEvent& event)
{
    dataLog("offset#", static_cast<unsigned>(m_bytes.size()), ":");
    event.dump(WTF::dataFile());
    dataLogLn(" ");
}

namespace {

constexpr unsigned operandKindShift = 4;
constexpr uint8_t eventKindMask = (1 << operandKindShift) - 1;

static_assert(InvalidEventKind <= eventKindMask);
static_assert(static_cast<unsigned>(lastOperandKind) < (1 << (CHAR_BIT - operandKindShift)));
static_assert(sizeof(MacroAssembler::RegisterID) == 1);
static_assert(sizeof(MacroAssembler::FPRegisterID) == 1);
static_assert(sizeof(DataFormat) == 1);

} // anonymous namespace

void VariableEventStream::encode(Vector<uint8_t>& bytes, const VariableEvent& event)
{
    VariableEventKind kind = event.kind();
    uint8_t tag = kind;
    if (kind == MovHintEvent || kind == SetLocalEvent)
        tag |= static_cast<uint8_t>(event.operand().kind()) << operandKindShift;
    bytes.append(tag);
    switch (kind) {
    case Reset:
        return;
    case Birth:
    case Death:
        WTF::LEBEncoder::encodeUInt32(bytes, event.id().bits());
        return;
    case BirthToFill:
    case Fill:
        WTF::LEBEncoder::encodeUInt32(bytes, event.id().bits());
        bytes.append(event.dataFormat());
        if (event.dataFormat() == DataFormatDouble)
            bytes.append(event.fpr());
        else
            bytes.append(event.gpr());
        return;
    case BirthToSpill:
    case Spill:
        WTF::LEBEncoder::encodeUInt32(bytes, event.id().bits());
        bytes.append(event.dataFormat());
        WTF::LEBEncoder::encodeInt32(bytes, event.spillRegister().offset());
        return;
    case MovHintEvent:
        WTF::LEBEncoder::encodeUInt32(bytes, event.id().bits());
        WTF::LEBEncoder::encodeInt32(bytes, event.operand().value());
        return;
    case SetLocalEvent:
        WTF::LEBEncoder::encodeInt32(bytes, event.machineRegister().offset());
        bytes.append(event.dataFormat());
        WTF::LEBEncoder::encodeInt32(bytes, event.operand().value());
        return;
    case InvalidEventKind:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

VariableEvent VariableEventStream::decode(std::span<const uint8_t> bytes, size_t& offset)
{
    uint8_t tag = bytes[offset++];
    VariableEventKind kind = static_cast<VariableEventKind>(tag & eventKindMask);
    OperandKind operandKind = static_cast<OperandKind>(tag >> operandKindShift);
    switch (kind) {
    case Reset:
        return VariableEvent::reset();
    case Birth:
        return VariableEvent::birth(MinifiedID::fromBits(WTF::LEBDecoder::decodeUInt32OrCrash(bytes, offset)));
    case Death:
        return VariableEvent::death(MinifiedID::fromBits(WTF::LEBDecoder::decodeUInt32OrCrash(bytes, offset)));
    case BirthToFill:
    case Fill: {
        MinifiedID id = MinifiedID::fromBits(WTF::LEBDecoder::decodeUInt32OrCrash(bytes, offset));
        DataFormat format = static_cast<DataFormat>(bytes[offset++]);
        if (format == DataFormatDouble)
            return VariableEvent::fillFPR(kind, id, static_cast<MacroAssembler::FPRegisterID>(bytes[offset++]));
        return VariableEvent::fillGPR(kind, id, static_cast<MacroAssembler::RegisterID>(bytes[offset++]), format);
    }
    case BirthToSpill:
    case Spill: {
        MinifiedID id = MinifiedID::fromBits(WTF::LEBDecoder::decodeUInt32OrCrash(bytes, offset));
        DataFormat format = static_cast<DataFormat>(bytes[offset++]);
        return VariableEvent::spill(kind, id, VirtualRegister(WTF::LEBDecoder::decodeInt32OrCrash(bytes, offset)), format);
    }
    case MovHintEvent: {
        MinifiedID id = MinifiedID::fromBits(WTF::LEBDecoder::decodeUInt32OrCrash(bytes, offset));
        return VariableEvent::movHint(id, Operand(operandKind, WTF::LEBDecoder::decodeInt32OrCrash(bytes, offset)));
    }
    case SetLocalEvent: {
        VirtualRegister machineRegister(WTF::LEBDecoder::decodeInt32OrCrash(bytes, offset));
        DataFormat format = static_cast<DataFormat>(bytes[offset++]);
        return VariableEvent::setLocal(Operand(operandKind, WTF::LEBDecoder::decodeInt32OrCrash(bytes, offset)), machineRegister, format);
    }
    case InvalidEventKind:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

namespace {

struct MinifiedGenerationInfo {
    bool filled; // true -> in gpr/fpr, false -> spilled
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
    constexpr bool verbose = false;
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);
    CodeBlock* baselineCodeBlock = codeBlock->baselineVersion();

    unsigned numVariables;
    unsigned numTmps;
    static constexpr unsigned invalidIndex = std::numeric_limits<unsigned>::max();
    unsigned firstUndefined = invalidIndex;
    bool firstUndefinedIsArgument = false;

    auto flushUndefinedOperandSpan = [&] (unsigned i) {
        if (firstUndefined == invalidIndex)
            return;
        int firstOffset = valueRecoveries.operandForIndex(firstUndefined).virtualRegister().offset();
        int lastOffset = valueRecoveries.operandForIndex(i - 1).virtualRegister().offset();
        int minOffset = std::min(firstOffset, lastOffset);
        undefinedOperandSpans->append({ firstUndefined, minOffset, i - firstUndefined });
        firstUndefined = invalidIndex;
    };
    auto recordUndefinedOperand = [&] (unsigned i) {
        // We want to separate the span of arguments from the span of locals even if they have adjacent operands indexes.
        if (firstUndefined != invalidIndex && firstUndefinedIsArgument != valueRecoveries.operandForIndex(i).isArgument())
            flushUndefinedOperandSpan(i);

        if (firstUndefined == invalidIndex) {
            firstUndefined = i;
            firstUndefinedIsArgument = valueRecoveries.operandForIndex(i).isArgument();
        }
    };

    auto* inlineCallFrame = codeOrigin.inlineCallFrame();
    if (inlineCallFrame) {
        CodeBlock* codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
        numVariables = codeBlock->numCalleeLocals() + VirtualRegister(inlineCallFrame->stackOffset).toLocal() + 1;
        numTmps = codeBlock->numTmps() + inlineCallFrame->tmpOffset;
    } else {
        numVariables = baselineCodeBlock->numCalleeLocals();
        numTmps = baselineCodeBlock->numTmps();
    }
    
    // Crazy special case: if we're at index == 0 then this must be an argument check
    // failure, in which case all variables are already set up. The recoveries should
    // reflect this.
    if (!index) {
        // We don't include tmps here because they can't be used yet.
        valueRecoveries = Operands<ValueRecovery>(codeBlock->numParameters(), numVariables, 0);
        for (size_t i = 0; i < valueRecoveries.size(); ++i) {
            valueRecoveries[i] = ValueRecovery::displacedInJSStack(
                valueRecoveries.operandForIndex(i).virtualRegister(), DataFormatJS);
        }
        return numVariables;
    }
    
    // Step 1: Find the last checkpoint.
    auto nextReset = std::lower_bound(m_resetOffsets.begin(), m_resetOffsets.end(), index);
    RELEASE_ASSERT(nextReset != m_resetOffsets.begin());
    size_t offset = *std::prev(nextReset);

    // Step 2: Create a mock-up of the DFG's state and execute the events.
    Operands<ValueSource> operandSources(codeBlock->numParameters(), numVariables, numTmps);
    for (unsigned i = operandSources.size(); i--;)
        operandSources[i] = ValueSource(SourceIsDead);
    UncheckedKeyHashMap<MinifiedID, MinifiedGenerationInfo> generationInfos;
    std::span<const uint8_t> bytes = m_bytes.span();
    while (offset < index) {
        VariableEvent event = decode(bytes, offset);
        dataLogLnIf(verbose, "Processing event ", event);
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
            UncheckedKeyHashMap<MinifiedID, MinifiedGenerationInfo>::iterator iter = generationInfos.find(event.id());
            ASSERT(iter != generationInfos.end());
            iter->value.update(event);
            break;
        }
        case MovHintEvent:
            if (operandSources.hasOperand(event.operand()))
                operandSources.setOperand(event.operand(), ValueSource(event.id()));
            break;
        case SetLocalEvent:
            if (operandSources.hasOperand(event.operand()))
                operandSources.setOperand(event.operand(), ValueSource::forDataFormat(event.machineRegister(), event.dataFormat()));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    RELEASE_ASSERT(offset == index);

    dataLogLnIf(verbose, "Operand sources: ", operandSources);
    
    // Step 3: Compute value recoveries!
    valueRecoveries = Operands<ValueRecovery>(OperandsLike, operandSources);
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
            dataLogLnIf(verbose, "Operand ", valueRecoveries.operandForIndex(i), " is dead.");
            if (Options::poisonDeadOSRExitVariables()) [[unlikely]] {
                valueRecoveries[i] = ValueRecovery::constant(JSValue::decode(poisonedDeadOSRExitValue));
                continue;
            }

            valueRecoveries[i] = ValueRecovery::constant(jsUndefined());
            if (style == ReconstructionStyle::Separated)
                recordUndefinedOperand(i);
            continue;
        }

        if (tryToSetConstantRecovery(valueRecoveries[i], node)) {
            dataLogLnIf(verbose, "Operand ", valueRecoveries.operandForIndex(i), " is constant.");
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
            valueRecoveries[i] = ValueRecovery::inGPR(info.u.gpr, info.format);
            continue;
        }
        
        valueRecoveries[i] =
            ValueRecovery::displacedInJSStack(info.u.operand.virtualRegister(), info.format);
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

