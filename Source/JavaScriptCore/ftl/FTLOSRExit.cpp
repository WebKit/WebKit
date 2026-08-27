/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "FTLOSRExit.h"

#if ENABLE(FTL_JIT)

#include "B3StackmapGenerationParams.h"
#include "FTLJITCode.h"
#include "FTLSlowPathCall.h"
#include "FTLState.h"
#include "JSCJSValueInlines.h"
#include <wtf/LEBDecoder.h>

namespace JSC { namespace FTL {

using namespace B3;
using namespace DFG;

namespace {

constexpr uint8_t maxDeadRunLength = 0x40;
constexpr uint8_t inJSStackAtOwnRegisterTag = 0x40;
constexpr uint8_t inJSStackTag = 0x44;
constexpr uint8_t constantTag = 0x48;
constexpr uint8_t materializationTag = 0x49;
constexpr uint8_t argumentTag = 0x4A;

static_assert(ExitValueInJSStackAsInt32 == ExitValueInJSStack + 1);
static_assert(ExitValueInJSStackAsInt52 == ExitValueInJSStack + 2);
static_assert(ExitValueInJSStackAsDouble == ExitValueInJSStack + 3);
static_assert(inJSStackAtOwnRegisterTag + 4 == inJSStackTag);
static_assert(inJSStackTag + 4 == constantTag);

void appendLEB(Vector<uint8_t, 128>& bytes, uint32_t value)
{
    while (value >= 0x80) {
        bytes.append(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    bytes.append(static_cast<uint8_t>(value));
}

uint32_t readLEB(std::span<const uint8_t> bytes, size_t& offset)
{
    uint32_t result = 0;
    bool success = WTF::LEBDecoder::decodeUInt32(bytes, offset, result);
    RELEASE_ASSERT(success);
    return result;
}

ExitValueKind inJSStackKind(uint8_t tag, uint8_t baseTag)
{
    return static_cast<ExitValueKind>(ExitValueInJSStack + (tag - baseTag));
}

Vector<ExitTimeObjectMaterialization*, 4> materializationTable(const Bag<ExitTimeObjectMaterialization>& materializations)
{
    Vector<ExitTimeObjectMaterialization*, 4> table;
    for (ExitTimeObjectMaterialization* materialization : materializations)
        table.append(materialization);
    return table;
}

} // anonymous namespace

void OSRExitValues::encode(const Operands<ExitValue>& values, const Bag<ExitTimeObjectMaterialization>& materializationBag, JITCode& jitCode)
{
    m_numberOfArguments = values.numberOfArguments();
    m_numberOfLocals = values.numberOfLocals();
    m_numberOfTmps = values.numberOfTmps();

    Vector<ExitTimeObjectMaterialization*, 4> materializations = materializationTable(materializationBag);
    Vector<EncodedJSValue>& constants = jitCode.osrExitConstants;
    Vector<uint8_t, 128> bytes;
    for (unsigned index = 0; index < values.size(); ++index) {
        const ExitValue& value = values[index];
        switch (value.kind()) {
        case ExitValueDead: {
            unsigned runLength = 1;
            while (runLength < maxDeadRunLength && index + runLength < values.size() && values[index + runLength].isDead())
                ++runLength;
            bytes.append(static_cast<uint8_t>(runLength - 1));
            index += runLength - 1;
            break;
        }
        case ExitValueInJSStack:
        case ExitValueInJSStackAsInt32:
        case ExitValueInJSStackAsInt52:
        case ExitValueInJSStackAsDouble: {
            Operand operand = values.operandForIndex(index);
            VirtualRegister reg = value.virtualRegister();
            if (!operand.isTmp() && operand.virtualRegister() == reg) {
                bytes.append(inJSStackAtOwnRegisterTag + (value.kind() - ExitValueInJSStack));
                break;
            }
            bytes.append(inJSStackTag + (value.kind() - ExitValueInJSStack));
            int32_t offset = reg.offset();
            appendLEB(bytes, (static_cast<uint32_t>(offset) << 1) ^ static_cast<uint32_t>(offset >> 31));
            break;
        }
        case ExitValueConstant: {
            bytes.append(constantTag);
            EncodedJSValue constant = JSValue::encode(value.constant());
            size_t constantIndex = constants.find(constant);
            if (constantIndex == notFound) {
                constantIndex = constants.size();
                constants.append(constant);
            }
            appendLEB(bytes, constantIndex);
            break;
        }
        case ExitValueMaterializeNewObject: {
            bytes.append(materializationTag);
            size_t materializationIndex = materializations.find(value.objectMaterialization());
            ASSERT(materializationIndex != notFound);
            appendLEB(bytes, materializationIndex);
            break;
        }
        case ExitValueArgument:
            bytes.append(argumentTag);
            bytes.append(static_cast<uint8_t>(value.exitArgument().format()));
            appendLEB(bytes, value.exitArgument().argument());
            break;
        case InvalidExitValue:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    m_bytes = FixedVector<uint8_t>(bytes);
}

FixedOperands<ExitValue> OSRExitValues::decode(const JITCode& jitCode, const Bag<ExitTimeObjectMaterialization>& materializationBag) const
{
    FixedOperands<ExitValue> values(m_numberOfArguments, m_numberOfLocals, m_numberOfTmps, ExitValue::dead());
    int localsOffset = jitCode.osrExitLocalsOffset();
    Vector<ExitTimeObjectMaterialization*, 4> materializations = materializationTable(materializationBag);

    std::span<const uint8_t> bytes = m_bytes.span();
    size_t offset = 0;
    for (unsigned index = 0; index < values.size(); ++index) {
        uint8_t tag = bytes[offset++];
        if (tag < maxDeadRunLength) {
            index += tag;
            continue;
        }
        if (tag < constantTag) {
            bool atOwnRegister = tag < inJSStackTag;
            VirtualRegister reg;
            if (atOwnRegister)
                reg = values.operandForIndex(index).virtualRegister();
            else {
                uint32_t zigzag = readLEB(bytes, offset);
                reg = VirtualRegister(static_cast<int32_t>(zigzag >> 1) ^ -static_cast<int32_t>(zigzag & 1));
            }
            values[index] = ExitValue::inJSStack(inJSStackKind(tag, atOwnRegister ? inJSStackAtOwnRegisterTag : inJSStackTag), reg).withLocalsOffset(localsOffset);
            continue;
        }
        switch (tag) {
        case constantTag:
            values[index] = ExitValue::constant(JSValue::decode(jitCode.osrExitConstants[readLEB(bytes, offset)]));
            break;
        case materializationTag:
            values[index] = ExitValue::materializeNewObject(materializations[readLEB(bytes, offset)]);
            break;
        case argumentTag: {
            DataFormat format = static_cast<DataFormat>(bytes[offset++]);
            values[index] = ExitValue::exitArgument(ExitArgument(format, readLEB(bytes, offset)));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    RELEASE_ASSERT(offset == bytes.size());
    return values;
}

OSRExitDescriptor::OSRExitDescriptor(DataFormat profileDataFormat, MethodOfGettingAValueProfile valueProfile)
    : m_profileDataFormat(profileDataFormat)
    , m_valueProfile(valueProfile)
{
}

void OSRExitDescriptor::validateReferences(const TrackedReferences& trackedReferences)
{
    for (ExitTimeObjectMaterialization* materialization : m_materializations)
        materialization->validateReferences(trackedReferences);
}

Ref<OSRExitHandle> OSRExitDescriptor::emitOSRExit(
    State& state, ExitKind exitKind, const NodeOrigin& nodeOrigin, CCallHelpers& jit,
    const StackmapGenerationParams& params, uint32_t dfgNodeIndex, unsigned offset)
{
    Ref<OSRExitHandle> handle =
        prepareOSRExitHandle(state, exitKind, nodeOrigin, params, dfgNodeIndex, offset);
    handle->emitExitThunk(state, jit);
    return handle;
}

Ref<OSRExitHandle> OSRExitDescriptor::emitOSRExitLater(
    State& state, ExitKind exitKind, const NodeOrigin& nodeOrigin,
    const StackmapGenerationParams& params, uint32_t dfgNodeIndex, unsigned offset)
{
    RefPtr<OSRExitHandle> handle =
        prepareOSRExitHandle(state, exitKind, nodeOrigin, params, dfgNodeIndex, offset);
    params.addLatePath(
        [handle, &state] (CCallHelpers& jit) {
            handle->emitExitThunk(state, jit);
        });
    return handle.releaseNonNull();
}

Ref<OSRExitHandle> OSRExitDescriptor::prepareOSRExitHandle(
    State& state, ExitKind exitKind, const NodeOrigin& nodeOrigin,
    const StackmapGenerationParams& params, uint32_t dfgNodeIndex, unsigned offset)
{
    FixedVector<B3::ValueRep> valueReps(params.size() - offset);
    for (unsigned i = offset, indexInValueReps = 0; i < params.size(); ++i, ++indexInValueReps)
        valueReps[indexInValueReps] = params[i];
    OSRExit exit(this, exitKind, nodeOrigin.forExit, nodeOrigin.semantic, nodeOrigin.wasHoisted, dfgNodeIndex, WTF::move(valueReps));
    if (exitKind == WillThrowOutOfMemoryError)
        exit.m_exitCallSiteIndex = callSiteIndexForCodeOrigin(state, nodeOrigin.semantic);

    unsigned index = state.jitCode->m_osrExit.size();
    state.jitCode->m_osrExit.append(WTF::move(exit));
    return adoptRef(*new OSRExitHandle(index, state.jitCode.get()));
}

OSRExit::OSRExit(
    OSRExitDescriptor* descriptor, ExitKind exitKind, CodeOrigin codeOrigin,
    CodeOrigin codeOriginForExitProfile, bool wasHoisted, uint32_t dfgNodeIndex, FixedVector<B3::ValueRep>&& valueReps)
    : OSRExitBase(exitKind, codeOrigin, codeOriginForExitProfile, wasHoisted, dfgNodeIndex)
    , m_descriptor(descriptor)
    , m_valueReps(WTF::move(valueReps))
{
}

CodeLocationJump<JSInternalPtrTag> OSRExit::codeLocationForRepatch(CodeBlock* ftlCodeBlock) const
{
    UNUSED_PARAM(ftlCodeBlock);
    return m_patchableJump;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)
