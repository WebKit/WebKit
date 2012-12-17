/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Intel Corporation. All rights reserved.
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
#include "DFGSpeculativeJIT.h"

#if ENABLE(DFG_JIT)

#include "ArrayPrototype.h"
#include "DFGCallArrayAllocatorSlowPathGenerator.h"
#include "DFGSlowPathGenerator.h"
#include "JSActivation.h"
#include "ObjectPrototype.h"

namespace JSC { namespace DFG {

#if USE(JSVALUE32_64)

GPRReg SpeculativeJIT::fillInteger(NodeIndex nodeIndex, DataFormat& returnFormat)
{
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            if (isInt32Constant(nodeIndex))
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
            else if (isNumberConstant(nodeIndex))
                ASSERT_NOT_REACHED();
            else {
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::Imm32(jsValue.payload()), gpr);
            }
        } else {
            ASSERT(info.spillFormat() == DataFormatJS || info.spillFormat() == DataFormatJSInteger || info.spillFormat() == DataFormatInteger);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        }

        info.fillInteger(*m_stream, gpr);
        returnFormat = DataFormatInteger;
        return gpr;
    }

    switch (info.registerFormat()) {
    case DataFormatNone:
        // Should have filled, above.
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJS:
    case DataFormatCell:
    case DataFormatJSCell:
    case DataFormatBoolean:
    case DataFormatJSBoolean:
    case DataFormatStorage:
        // Should only be calling this function if we know this operand to be integer.
        ASSERT_NOT_REACHED();

    case DataFormatJSInteger: {
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_jit.jitAssertIsJSInt32(tagGPR);
        m_gprs.unlock(tagGPR);
        m_gprs.lock(payloadGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderInteger);
        info.fillInteger(*m_stream, payloadGPR);
        returnFormat = DataFormatInteger;
        return payloadGPR;
    }

    case DataFormatInteger: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        m_jit.jitAssertIsInt32(gpr);
        returnFormat = DataFormatInteger;
        return gpr;
    }

    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

FPRReg SpeculativeJIT::fillDouble(NodeIndex nodeIndex)
{
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {

        if (node.hasConstant()) {
            if (isInt32Constant(nodeIndex)) {
                // FIXME: should not be reachable?
                GPRReg gpr = allocate();
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillInteger(*m_stream, gpr);
                unlock(gpr);
            } else if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(addressOfDoubleConstant(nodeIndex), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            } else {
                // FIXME: should not be reachable?
                ASSERT_NOT_REACHED();
            }
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT((spillFormat & DataFormatJS) || spillFormat == DataFormatInteger);
            if (spillFormat == DataFormatJSDouble) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            }

            FPRReg fpr = fprAllocate();
            JITCompiler::Jump hasUnboxedDouble;

            if (spillFormat != DataFormatJSInteger && spillFormat != DataFormatInteger) {
                JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag));
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                hasUnboxedDouble = m_jit.jump();
                isInteger.link(&m_jit);
            }

            m_jit.convertInt32ToDouble(JITCompiler::payloadFor(virtualRegister), fpr);

            if (hasUnboxedDouble.isSet())
                hasUnboxedDouble.link(&m_jit);

            m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
            info.fillDouble(*m_stream, fpr);
            return fpr;
        }
    }

    switch (info.registerFormat()) {
    case DataFormatNone:
        // Should have filled, above.
    case DataFormatCell:
    case DataFormatJSCell:
    case DataFormatBoolean:
    case DataFormatJSBoolean:
    case DataFormatStorage:
        // Should only be calling this function if we know this operand to be numeric.
        ASSERT_NOT_REACHED();

    case DataFormatJSInteger:
    case DataFormatJS: {
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        FPRReg fpr = fprAllocate();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);

        JITCompiler::Jump hasUnboxedDouble;

        if (info.registerFormat() != DataFormatJSInteger) {
            FPRTemporary scratch(this);
            JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, tagGPR, TrustedImm32(JSValue::Int32Tag));
            m_jit.jitAssertIsJSDouble(tagGPR);
            unboxDouble(tagGPR, payloadGPR, fpr, scratch.fpr());
            hasUnboxedDouble = m_jit.jump();
            isInteger.link(&m_jit);
        }

        m_jit.convertInt32ToDouble(payloadGPR, fpr);

        if (hasUnboxedDouble.isSet())
            hasUnboxedDouble.link(&m_jit);

        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.unlock(tagGPR);
        m_gprs.unlock(payloadGPR);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(*m_stream, fpr);
        info.killSpilled();
        return fpr;
    }

    case DataFormatInteger: {
        FPRReg fpr = fprAllocate();
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        m_jit.convertInt32ToDouble(gpr, fpr);
        m_gprs.unlock(gpr);
        return fpr;
    }

    case DataFormatJSDouble: 
    case DataFormatDouble: {
        FPRReg fpr = info.fpr();
        m_fprs.lock(fpr);
        return fpr;
    }

    default:
        ASSERT_NOT_REACHED();
        return InvalidFPRReg;
    }
}

bool SpeculativeJIT::fillJSValue(NodeIndex nodeIndex, GPRReg& tagGPR, GPRReg& payloadGPR, FPRReg& fpr)
{
    // FIXME: For double we could fill with a FPR.
    UNUSED_PARAM(fpr);

    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {

        if (node.hasConstant()) {
            tagGPR = allocate();
            payloadGPR = allocate();
            m_jit.move(Imm32(valueOfJSConstant(nodeIndex).tag()), tagGPR);
            m_jit.move(Imm32(valueOfJSConstant(nodeIndex).payload()), payloadGPR);
            m_gprs.retain(tagGPR, virtualRegister, SpillOrderConstant);
            m_gprs.retain(payloadGPR, virtualRegister, SpillOrderConstant);
            info.fillJSValue(*m_stream, tagGPR, payloadGPR, isInt32Constant(nodeIndex) ? DataFormatJSInteger : DataFormatJS);
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat != DataFormatNone && spillFormat != DataFormatStorage);
            tagGPR = allocate();
            payloadGPR = allocate();
            switch (spillFormat) {
            case DataFormatInteger:
                m_jit.move(TrustedImm32(JSValue::Int32Tag), tagGPR);
                spillFormat = DataFormatJSInteger; // This will be used as the new register format.
                break;
            case DataFormatCell:
                m_jit.move(TrustedImm32(JSValue::CellTag), tagGPR);
                spillFormat = DataFormatJSCell; // This will be used as the new register format.
                break;
            case DataFormatBoolean:
                m_jit.move(TrustedImm32(JSValue::BooleanTag), tagGPR);
                spillFormat = DataFormatJSBoolean; // This will be used as the new register format.
                break;
            default:
                m_jit.load32(JITCompiler::tagFor(virtualRegister), tagGPR);
                break;
            }
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), payloadGPR);
            m_gprs.retain(tagGPR, virtualRegister, SpillOrderSpilled);
            m_gprs.retain(payloadGPR, virtualRegister, SpillOrderSpilled);
            info.fillJSValue(*m_stream, tagGPR, payloadGPR, spillFormat == DataFormatJSDouble ? DataFormatJS : spillFormat);
        }

        return true;
    }

    case DataFormatInteger:
    case DataFormatCell:
    case DataFormatBoolean: {
        GPRReg gpr = info.gpr();
        // If the register has already been locked we need to take a copy.
        if (m_gprs.isLocked(gpr)) {
            payloadGPR = allocate();
            m_jit.move(gpr, payloadGPR);
        } else {
            payloadGPR = gpr;
            m_gprs.lock(gpr);
        }
        tagGPR = allocate();
        uint32_t tag = JSValue::EmptyValueTag;
        DataFormat fillFormat = DataFormatJS;
        switch (info.registerFormat()) {
        case DataFormatInteger:
            tag = JSValue::Int32Tag;
            fillFormat = DataFormatJSInteger;
            break;
        case DataFormatCell:
            tag = JSValue::CellTag;
            fillFormat = DataFormatJSCell;
            break;
        case DataFormatBoolean:
            tag = JSValue::BooleanTag;
            fillFormat = DataFormatJSBoolean;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        m_jit.move(TrustedImm32(tag), tagGPR);
        m_gprs.release(gpr);
        m_gprs.retain(tagGPR, virtualRegister, SpillOrderJS);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderJS);
        info.fillJSValue(*m_stream, tagGPR, payloadGPR, fillFormat);
        return true;
    }

    case DataFormatJSDouble:
    case DataFormatDouble: {
        FPRReg oldFPR = info.fpr();
        m_fprs.lock(oldFPR);
        tagGPR = allocate();
        payloadGPR = allocate();
        boxDouble(oldFPR, tagGPR, payloadGPR);
        m_fprs.unlock(oldFPR);
        m_fprs.release(oldFPR);
        m_gprs.retain(tagGPR, virtualRegister, SpillOrderJS);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderJS);
        info.fillJSValue(*m_stream, tagGPR, payloadGPR, DataFormatJS);
        return true;
    }

    case DataFormatJS:
    case DataFormatJSInteger:
    case DataFormatJSCell:
    case DataFormatJSBoolean: {
        tagGPR = info.tagGPR();
        payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);
        return true;
    }
        
    case DataFormatStorage:
        // this type currently never occurs
        ASSERT_NOT_REACHED();

    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

class ValueToNumberSlowPathGenerator
    : public CallSlowPathGenerator<MacroAssembler::Jump, D_DFGOperation_EJ, JSValueRegs> {
public:
    ValueToNumberSlowPathGenerator(
        MacroAssembler::Jump from, SpeculativeJIT* jit,
        GPRReg resultTagGPR, GPRReg resultPayloadGPR, GPRReg jsValueTagGPR, GPRReg jsValuePayloadGPR)
        : CallSlowPathGenerator<MacroAssembler::Jump, D_DFGOperation_EJ, JSValueRegs>(
            from, jit, dfgConvertJSValueToNumber, NeedToSpill, JSValueRegs(resultTagGPR, resultPayloadGPR))
        , m_jsValueTagGPR(jsValueTagGPR)
        , m_jsValuePayloadGPR(jsValuePayloadGPR)
    {
    }

protected:
    virtual void generateInternal(SpeculativeJIT* jit)
    {
        setUp(jit);
        recordCall(jit->callOperation(dfgConvertJSValueToNumber, FPRInfo::returnValueFPR, m_jsValueTagGPR, m_jsValuePayloadGPR));
        jit->boxDouble(FPRInfo::returnValueFPR, m_result.tagGPR(), m_result.payloadGPR());
        tearDown(jit);
    }

private:
    GPRReg m_jsValueTagGPR;
    GPRReg m_jsValuePayloadGPR;
};

void SpeculativeJIT::nonSpeculativeValueToNumber(Node& node)
{
    if (isKnownNumeric(node.child1().index())) {
        JSValueOperand op1(this, node.child1());
        op1.fill();
        if (op1.isDouble()) {
            FPRTemporary result(this, op1);
            m_jit.moveDouble(op1.fpr(), result.fpr());
            doubleResult(result.fpr(), m_compileIndex);
        } else {
            GPRTemporary resultTag(this, op1);
            GPRTemporary resultPayload(this, op1, false);
            m_jit.move(op1.tagGPR(), resultTag.gpr());
            m_jit.move(op1.payloadGPR(), resultPayload.gpr());
            jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        }
        return;
    }

    JSValueOperand op1(this, node.child1());
    GPRTemporary resultTag(this, op1);
    GPRTemporary resultPayload(this, op1, false);

    ASSERT(!isInt32Constant(node.child1().index()));
    ASSERT(!isNumberConstant(node.child1().index()));

    GPRReg tagGPR = op1.tagGPR();
    GPRReg payloadGPR = op1.payloadGPR();
    GPRReg resultTagGPR = resultTag.gpr();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    op1.use();

    JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, tagGPR, TrustedImm32(JSValue::Int32Tag));
    JITCompiler::Jump nonNumeric = m_jit.branch32(MacroAssembler::AboveOrEqual, tagGPR, TrustedImm32(JSValue::LowestTag));

    isInteger.link(&m_jit);
    m_jit.move(tagGPR, resultTagGPR);
    m_jit.move(payloadGPR, resultPayloadGPR);

    addSlowPathGenerator(adoptPtr(new ValueToNumberSlowPathGenerator(nonNumeric, this, resultTagGPR, resultPayloadGPR, tagGPR, payloadGPR)));

    jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativeValueToInt32(Node& node)
{
    ASSERT(!isInt32Constant(node.child1().index()));

    if (isKnownInteger(node.child1().index())) {
        IntegerOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        m_jit.move(op1.gpr(), result.gpr());
        integerResult(result.gpr(), m_compileIndex);
        return;
    }

    GenerationInfo& childInfo = m_generationInfo[at(node.child1()).virtualRegister()];
    if (childInfo.isJSDouble()) {
        DoubleOperand op1(this, node.child1());
        GPRTemporary result(this);
        FPRReg fpr = op1.fpr();
        GPRReg gpr = result.gpr();
        op1.use();
        JITCompiler::Jump notTruncatedToInteger = m_jit.branchTruncateDoubleToInt32(fpr, gpr, JITCompiler::BranchIfTruncateFailed);

        addSlowPathGenerator(slowPathCall(notTruncatedToInteger, this, toInt32, gpr, fpr));

        integerResult(gpr, m_compileIndex, UseChildrenCalledExplicitly);
        return;
    }

    JSValueOperand op1(this, node.child1());
    GPRTemporary result(this);
    GPRReg tagGPR = op1.tagGPR();
    GPRReg payloadGPR = op1.payloadGPR();
    GPRReg resultGPR = result.gpr();
    op1.use();

    JITCompiler::Jump isNotInteger = m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::Int32Tag));

    m_jit.move(payloadGPR, resultGPR);
    
    addSlowPathGenerator(slowPathCall(isNotInteger, this, dfgConvertJSValueToInt32, resultGPR, tagGPR, payloadGPR));
    
    integerResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativeUInt32ToNumber(Node& node)
{
    IntegerOperand op1(this, node.child1());
    FPRTemporary boxer(this);
    GPRTemporary resultTag(this, op1);
    GPRTemporary resultPayload(this);
        
    JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, op1.gpr(), TrustedImm32(0));
        
    m_jit.convertInt32ToDouble(op1.gpr(), boxer.fpr());
    m_jit.move(JITCompiler::TrustedImmPtr(&AssemblyHelpers::twoToThe32), resultPayload.gpr()); // reuse resultPayload register here.
    m_jit.addDouble(JITCompiler::Address(resultPayload.gpr(), 0), boxer.fpr());
        
    boxDouble(boxer.fpr(), resultTag.gpr(), resultPayload.gpr());
        
    JITCompiler::Jump done = m_jit.jump();
        
    positive.link(&m_jit);
        
    m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTag.gpr());
    m_jit.move(op1.gpr(), resultPayload.gpr());
        
    done.link(&m_jit);

    jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
}

void SpeculativeJIT::cachedGetById(CodeOrigin codeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget, SpillRegistersMode spillMode)
{
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::PatchableJump structureCheck = m_jit.patchableBranchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(basePayloadGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    
    JITCompiler::ConvertibleLoadLabel propertyStorageLoad = m_jit.convertibleLoadPtr(JITCompiler::Address(basePayloadGPR, JSObject::butterflyOffset()), resultPayloadGPR);
    JITCompiler::DataLabelCompact tagLoadWithPatch = m_jit.load32WithCompactAddressOffsetPatch(JITCompiler::Address(resultPayloadGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
    JITCompiler::DataLabelCompact payloadLoadWithPatch = m_jit.load32WithCompactAddressOffsetPatch(JITCompiler::Address(resultPayloadGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
    
    JITCompiler::Label doneLabel = m_jit.label();

    OwnPtr<SlowPathGenerator> slowPath;
    if (baseTagGPROrNone == InvalidGPRReg) {
        if (!slowPathTarget.isSet()) {
            slowPath = slowPathCall(
                structureCheck.m_jump, this, operationGetByIdOptimize,
                JSValueRegs(resultTagGPR, resultPayloadGPR),
                static_cast<int32_t>(JSValue::CellTag), basePayloadGPR,
                identifier(identifierNumber));
        } else {
            JITCompiler::JumpList slowCases;
            slowCases.append(structureCheck.m_jump);
            slowCases.append(slowPathTarget);
            slowPath = slowPathCall(
                slowCases, this, operationGetByIdOptimize,
                JSValueRegs(resultTagGPR, resultPayloadGPR),
                static_cast<int32_t>(JSValue::CellTag), basePayloadGPR,
                identifier(identifierNumber));
        }
    } else {
        if (!slowPathTarget.isSet()) {
            slowPath = slowPathCall(
                structureCheck.m_jump, this, operationGetByIdOptimize,
                JSValueRegs(resultTagGPR, resultPayloadGPR), baseTagGPROrNone, basePayloadGPR,
                identifier(identifierNumber));
        } else {
            JITCompiler::JumpList slowCases;
            slowCases.append(structureCheck.m_jump);
            slowCases.append(slowPathTarget);
            slowPath = slowPathCall(
                slowCases, this, operationGetByIdOptimize,
                JSValueRegs(resultTagGPR, resultPayloadGPR), baseTagGPROrNone, basePayloadGPR,
                identifier(identifierNumber));
        }
    }
    m_jit.addPropertyAccess(
        PropertyAccessRecord(
            codeOrigin, structureToCompare, structureCheck, propertyStorageLoad,
            tagLoadWithPatch, payloadLoadWithPatch, slowPath.get(), doneLabel,
            safeCast<int8_t>(basePayloadGPR), safeCast<int8_t>(resultTagGPR),
            safeCast<int8_t>(resultPayloadGPR), usedRegisters(),
            spillMode == NeedToSpill ? PropertyAccessRecord::RegistersInUse : PropertyAccessRecord::RegistersFlushed));
    addSlowPathGenerator(slowPath.release());
}

void SpeculativeJIT::cachedPutById(CodeOrigin codeOrigin, GPRReg basePayloadGPR, GPRReg valueTagGPR, GPRReg valuePayloadGPR, Edge valueUse, GPRReg scratchGPR, unsigned identifierNumber, PutKind putKind, JITCompiler::Jump slowPathTarget)
{
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::PatchableJump structureCheck = m_jit.patchableBranchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(basePayloadGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));

    writeBarrier(basePayloadGPR, valueTagGPR, valueUse, WriteBarrierForPropertyAccess, scratchGPR);

    JITCompiler::ConvertibleLoadLabel propertyStorageLoad = m_jit.convertibleLoadPtr(JITCompiler::Address(basePayloadGPR, JSObject::butterflyOffset()), scratchGPR);
    JITCompiler::DataLabel32 tagStoreWithPatch = m_jit.store32WithAddressOffsetPatch(valueTagGPR, JITCompiler::Address(scratchGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    JITCompiler::DataLabel32 payloadStoreWithPatch = m_jit.store32WithAddressOffsetPatch(valuePayloadGPR, JITCompiler::Address(scratchGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));

    JITCompiler::Label doneLabel = m_jit.label();
    V_DFGOperation_EJCI optimizedCall;
    if (m_jit.strictModeFor(at(m_compileIndex).codeOrigin)) {
        if (putKind == Direct)
            optimizedCall = operationPutByIdDirectStrictOptimize;
        else
            optimizedCall = operationPutByIdStrictOptimize;
    } else {
        if (putKind == Direct)
            optimizedCall = operationPutByIdDirectNonStrictOptimize;
        else
            optimizedCall = operationPutByIdNonStrictOptimize;
    }
    OwnPtr<SlowPathGenerator> slowPath;
    if (!slowPathTarget.isSet()) {
        slowPath = slowPathCall(
            structureCheck.m_jump, this, optimizedCall, NoResult, valueTagGPR, valuePayloadGPR,
            basePayloadGPR, identifier(identifierNumber));
    } else {
        JITCompiler::JumpList slowCases;
        slowCases.append(structureCheck.m_jump);
        slowCases.append(slowPathTarget);
        slowPath = slowPathCall(
            slowCases, this, optimizedCall, NoResult, valueTagGPR, valuePayloadGPR,
            basePayloadGPR, identifier(identifierNumber));
    }
    RegisterSet currentlyUsedRegisters = usedRegisters();
    currentlyUsedRegisters.clear(scratchGPR);
    ASSERT(currentlyUsedRegisters.get(basePayloadGPR));
    ASSERT(currentlyUsedRegisters.get(valueTagGPR));
    ASSERT(currentlyUsedRegisters.get(valuePayloadGPR));
    m_jit.addPropertyAccess(
        PropertyAccessRecord(
            codeOrigin, structureToCompare, structureCheck, propertyStorageLoad,
            JITCompiler::DataLabelCompact(tagStoreWithPatch.label()),
            JITCompiler::DataLabelCompact(payloadStoreWithPatch.label()),
            slowPath.get(), doneLabel, safeCast<int8_t>(basePayloadGPR),
            safeCast<int8_t>(valueTagGPR), safeCast<int8_t>(valuePayloadGPR),
            usedRegisters()));
    addSlowPathGenerator(slowPath.release());
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompareNull(Edge operand, bool invert)
{
    JSValueOperand arg(this, operand);
    GPRReg argTagGPR = arg.tagGPR();
    GPRReg argPayloadGPR = arg.payloadGPR();

    GPRTemporary resultPayload(this, arg, false);
    GPRReg resultPayloadGPR = resultPayload.gpr();

    JITCompiler::Jump notCell;
    if (!isKnownCell(operand.index()))
        notCell = m_jit.branch32(MacroAssembler::NotEqual, argTagGPR, TrustedImm32(JSValue::CellTag));

    JITCompiler::Jump notMasqueradesAsUndefined;   
    if (m_jit.graph().globalObjectFor(m_jit.graph()[operand].codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(m_jit.graph()[operand].codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
        m_jit.move(invert ? TrustedImm32(1) : TrustedImm32(0), resultPayloadGPR);
        notMasqueradesAsUndefined = m_jit.jump();
    } else {
        m_jit.loadPtr(JITCompiler::Address(argPayloadGPR, JSCell::structureOffset()), resultPayloadGPR);
        JITCompiler::Jump isMasqueradesAsUndefined = m_jit.branchTest8(JITCompiler::NonZero, JITCompiler::Address(resultPayloadGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined));
        
        m_jit.move(invert ? TrustedImm32(1) : TrustedImm32(0), resultPayloadGPR);
        notMasqueradesAsUndefined = m_jit.jump();

        isMasqueradesAsUndefined.link(&m_jit);
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);
        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();
        m_jit.move(JITCompiler::TrustedImmPtr(m_jit.graph().globalObjectFor(m_jit.graph()[operand].codeOrigin)), localGlobalObjectGPR);
        m_jit.loadPtr(JITCompiler::Address(resultPayloadGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        m_jit.compare32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, resultPayloadGPR);
    }
 
    if (!isKnownCell(operand.index())) {
        JITCompiler::Jump done = m_jit.jump();
        
        notCell.link(&m_jit);
        // null or undefined?
        COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
        m_jit.move(argTagGPR, resultPayloadGPR);
        m_jit.or32(TrustedImm32(1), resultPayloadGPR);
        m_jit.compare32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultPayloadGPR, TrustedImm32(JSValue::NullTag), resultPayloadGPR);

        done.link(&m_jit);
    }
    
    notMasqueradesAsUndefined.link(&m_jit);
 
    booleanResult(resultPayloadGPR, m_compileIndex);
}

void SpeculativeJIT::nonSpeculativePeepholeBranchNull(Edge operand, NodeIndex branchNodeIndex, bool invert)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();
    
    if (taken == nextBlock()) {
        invert = !invert;
        BlockIndex tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    JSValueOperand arg(this, operand);
    GPRReg argTagGPR = arg.tagGPR();
    GPRReg argPayloadGPR = arg.payloadGPR();
    
    GPRTemporary result(this, arg);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::Jump notCell;
    
    if (!isKnownCell(operand.index()))
        notCell = m_jit.branch32(MacroAssembler::NotEqual, argTagGPR, TrustedImm32(JSValue::CellTag));
    
    if (m_jit.graph().globalObjectFor(m_jit.graph()[operand].codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(m_jit.graph()[operand].codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
        jump(invert ? taken : notTaken, ForceJump);
    } else {
        m_jit.loadPtr(JITCompiler::Address(argPayloadGPR, JSCell::structureOffset()), resultGPR);
        branchTest8(JITCompiler::Zero, JITCompiler::Address(resultGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined), invert ? taken : notTaken);
   
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);
        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();
        m_jit.move(TrustedImmPtr(m_jit.graph().globalObjectFor(m_jit.graph()[operand].codeOrigin)), localGlobalObjectGPR);
        m_jit.loadPtr(JITCompiler::Address(resultGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        branchPtr(JITCompiler::Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, invert ? notTaken : taken);
    }
 
    if (!isKnownCell(operand.index())) {
        jump(notTaken, ForceJump);
        
        notCell.link(&m_jit);
        // null or undefined?
        COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
        m_jit.move(argTagGPR, resultGPR);
        m_jit.or32(TrustedImm32(1), resultGPR);
        branch32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImm32(JSValue::NullTag), taken);
    }
    
    jump(notTaken);
}

bool SpeculativeJIT::nonSpeculativeCompareNull(Node& node, Edge operand, bool invert)
{
    unsigned branchIndexInBlock = detectPeepHoleBranch();
    if (branchIndexInBlock != UINT_MAX) {
        NodeIndex branchNodeIndex = m_jit.graph().m_blocks[m_block]->at(branchIndexInBlock);

        ASSERT(node.adjustedRefCount() == 1);
        
        nonSpeculativePeepholeBranchNull(operand, branchNodeIndex, invert);
    
        use(node.child1());
        use(node.child2());
        m_indexInBlock = branchIndexInBlock;
        m_compileIndex = branchNodeIndex;
        
        return true;
    }
    
    nonSpeculativeNonPeepholeCompareNull(operand, invert);
    
    return false;
}

void SpeculativeJIT::nonSpeculativePeepholeBranch(Node& node, NodeIndex branchNodeIndex, MacroAssembler::RelationalCondition cond, S_DFGOperation_EJJ helperFunction)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();

    JITCompiler::ResultCondition callResultCondition = JITCompiler::NonZero;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        cond = JITCompiler::invert(cond);
        callResultCondition = JITCompiler::Zero;
        BlockIndex tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1TagGPR = arg1.tagGPR();
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2TagGPR = arg2.tagGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1().index()) || isKnownNotInteger(node.child2().index())) {
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);

        branchTest32(callResultCondition, resultGPR, taken);
    } else {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();

        if (!isKnownInteger(node.child1().index()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg1TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));
        if (!isKnownInteger(node.child2().index()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg2TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));
    
        branch32(cond, arg1PayloadGPR, arg2PayloadGPR, taken);
    
        if (!isKnownInteger(node.child1().index()) || !isKnownInteger(node.child2().index())) {
            jump(notTaken, ForceJump);
    
            slowPath.link(&m_jit);
    
            silentSpillAllRegisters(resultGPR);
            callOperation(helperFunction, resultGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
            silentFillAllRegisters(resultGPR);
        
            branchTest32(callResultCondition, resultGPR, taken);
        }
    }

    jump(notTaken);
    
    m_indexInBlock = m_jit.graph().m_blocks[m_block]->size() - 1;
    m_compileIndex = branchNodeIndex;
}

template<typename JumpType>
class CompareAndBoxBooleanSlowPathGenerator
    : public CallSlowPathGenerator<JumpType, S_DFGOperation_EJJ, GPRReg> {
public:
    CompareAndBoxBooleanSlowPathGenerator(
        JumpType from, SpeculativeJIT* jit,
        S_DFGOperation_EJJ function, GPRReg result, GPRReg arg1Tag, GPRReg arg1Payload,
        GPRReg arg2Tag, GPRReg arg2Payload)
        : CallSlowPathGenerator<JumpType, S_DFGOperation_EJJ, GPRReg>(
            from, jit, function, NeedToSpill, result)
        , m_arg1Tag(arg1Tag)
        , m_arg1Payload(arg1Payload)
        , m_arg2Tag(arg2Tag)
        , m_arg2Payload(arg2Payload)
    {
    }
    
protected:
    virtual void generateInternal(SpeculativeJIT* jit)
    {
        this->setUp(jit);
        this->recordCall(
            jit->callOperation(
                this->m_function, this->m_result, m_arg1Tag, m_arg1Payload, m_arg2Tag,
                m_arg2Payload));
        jit->m_jit.and32(JITCompiler::TrustedImm32(1), this->m_result);
        this->tearDown(jit);
    }
   
private:
    GPRReg m_arg1Tag;
    GPRReg m_arg1Payload;
    GPRReg m_arg2Tag;
    GPRReg m_arg2Payload;
};

void SpeculativeJIT::nonSpeculativeNonPeepholeCompare(Node& node, MacroAssembler::RelationalCondition cond, S_DFGOperation_EJJ helperFunction)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1TagGPR = arg1.tagGPR();
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2TagGPR = arg2.tagGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1().index()) || isKnownNotInteger(node.child2().index())) {
        GPRResult result(this);
        GPRReg resultPayloadGPR = result.gpr();
    
        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        
        booleanResult(resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
    } else {
        GPRTemporary resultPayload(this, arg1, false);
        GPRReg resultPayloadGPR = resultPayload.gpr();

        arg1.use();
        arg2.use();
    
        if (!isKnownInteger(node.child1().index()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg1TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));
        if (!isKnownInteger(node.child2().index()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg2TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));

        m_jit.compare32(cond, arg1PayloadGPR, arg2PayloadGPR, resultPayloadGPR);
    
        if (!isKnownInteger(node.child1().index()) || !isKnownInteger(node.child2().index())) {
            addSlowPathGenerator(adoptPtr(
                new CompareAndBoxBooleanSlowPathGenerator<JITCompiler::JumpList>(
                    slowPath, this, helperFunction, resultPayloadGPR, arg1TagGPR,
                    arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR)));
        }
        
        booleanResult(resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
    }
}

void SpeculativeJIT::nonSpeculativePeepholeStrictEq(Node& node, NodeIndex branchNodeIndex, bool invert)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        invert = !invert;
        BlockIndex tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }
    
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1TagGPR = arg1.tagGPR();
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2TagGPR = arg2.tagGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    
    GPRTemporary resultPayload(this, arg1, false);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node.child1().index()) && isKnownCell(node.child2().index())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        branchPtr(JITCompiler::Equal, arg1PayloadGPR, arg2PayloadGPR, invert ? notTaken : taken);
        
        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEqCell, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultPayloadGPR);
        
        branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultPayloadGPR, taken);
    } else {
        // FIXME: Add fast paths for twoCells, number etc.

        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEq, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultPayloadGPR);
        
        branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultPayloadGPR, taken);
    }
    
    jump(notTaken);
}

void SpeculativeJIT::nonSpeculativeNonPeepholeStrictEq(Node& node, bool invert)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1TagGPR = arg1.tagGPR();
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2TagGPR = arg2.tagGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    
    GPRTemporary resultPayload(this, arg1, false);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node.child1().index()) && isKnownCell(node.child2().index())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        // FIXME: this should flush registers instead of silent spill/fill.
        JITCompiler::Jump notEqualCase = m_jit.branchPtr(JITCompiler::NotEqual, arg1PayloadGPR, arg2PayloadGPR);
        
        m_jit.move(JITCompiler::TrustedImm32(!invert), resultPayloadGPR);
        JITCompiler::Jump done = m_jit.jump();

        notEqualCase.link(&m_jit);
        
        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEqCell, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultPayloadGPR);
        
        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultPayloadGPR);
        
        done.link(&m_jit);
    } else {
        // FIXME: Add fast paths.

        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEq, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultPayloadGPR);
        
        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultPayloadGPR);
    }

    booleanResult(resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitCall(Node& node)
{
    if (node.op() != Call)
        ASSERT(node.op() == Construct);

    // For constructors, the this argument is not passed but we have to make space
    // for it.
    int dummyThisArgument = node.op() == Call ? 0 : 1;

    CallLinkInfo::CallType callType = node.op() == Call ? CallLinkInfo::Call : CallLinkInfo::Construct;

    Edge calleeEdge = m_jit.graph().m_varArgChildren[node.firstChild()];
    JSValueOperand callee(this, calleeEdge);
    GPRReg calleeTagGPR = callee.tagGPR();
    GPRReg calleePayloadGPR = callee.payloadGPR();
    use(calleeEdge);

    // The call instruction's first child is either the function (normal call) or the
    // receiver (method call). subsequent children are the arguments.
    int numPassedArgs = node.numChildren() - 1;

    m_jit.store32(MacroAssembler::TrustedImm32(numPassedArgs + dummyThisArgument), callFramePayloadSlot(JSStack::ArgumentCount));
    m_jit.storePtr(GPRInfo::callFrameRegister, callFramePayloadSlot(JSStack::CallerFrame));
    m_jit.store32(calleePayloadGPR, callFramePayloadSlot(JSStack::Callee));
    m_jit.store32(calleeTagGPR, callFrameTagSlot(JSStack::Callee));

    for (int i = 0; i < numPassedArgs; i++) {
        Edge argEdge = m_jit.graph().m_varArgChildren[node.firstChild() + 1 + i];
        JSValueOperand arg(this, argEdge);
        GPRReg argTagGPR = arg.tagGPR();
        GPRReg argPayloadGPR = arg.payloadGPR();
        use(argEdge);

        m_jit.store32(argTagGPR, argumentTagSlot(i + dummyThisArgument));
        m_jit.store32(argPayloadGPR, argumentPayloadSlot(i + dummyThisArgument));
    }

    flushRegisters();

    GPRResult resultPayload(this);
    GPRResult2 resultTag(this);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    GPRReg resultTagGPR = resultTag.gpr();

    JITCompiler::DataLabelPtr targetToCheck;
    JITCompiler::JumpList slowPath;

    CallBeginToken token;
    m_jit.beginCall(node.codeOrigin, token);
    
    m_jit.addPtr(TrustedImm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister);
    
    slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, calleeTagGPR, TrustedImm32(JSValue::CellTag)));
    slowPath.append(m_jit.branchPtrWithPatch(MacroAssembler::NotEqual, calleePayloadGPR, targetToCheck));
    m_jit.loadPtr(MacroAssembler::Address(calleePayloadGPR, OBJECT_OFFSETOF(JSFunction, m_scope)), resultPayloadGPR);
    m_jit.storePtr(resultPayloadGPR, MacroAssembler::Address(GPRInfo::callFrameRegister, static_cast<ptrdiff_t>(sizeof(Register)) * JSStack::ScopeChain + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
    m_jit.store32(MacroAssembler::TrustedImm32(JSValue::CellTag), MacroAssembler::Address(GPRInfo::callFrameRegister, static_cast<ptrdiff_t>(sizeof(Register)) * JSStack::ScopeChain + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));

    CodeOrigin codeOrigin = at(m_compileIndex).codeOrigin;
    JITCompiler::Call fastCall = m_jit.nearCall();
    m_jit.notifyCall(fastCall, codeOrigin, token);

    JITCompiler::Jump done = m_jit.jump();

    slowPath.link(&m_jit);

    if (calleeTagGPR == GPRInfo::nonArgGPR0) {
        if (calleePayloadGPR == GPRInfo::nonArgGPR1)
            m_jit.swap(GPRInfo::nonArgGPR1, GPRInfo::nonArgGPR0);
        else {
            m_jit.move(calleeTagGPR, GPRInfo::nonArgGPR1);
            m_jit.move(calleePayloadGPR, GPRInfo::nonArgGPR0);
        }
    } else {
        m_jit.move(calleePayloadGPR, GPRInfo::nonArgGPR0);
        m_jit.move(calleeTagGPR, GPRInfo::nonArgGPR1);
    }
    m_jit.prepareForExceptionCheck();
    JITCompiler::Call slowCall = m_jit.nearCall();
    m_jit.notifyCall(slowCall, codeOrigin, token);

    done.link(&m_jit);

    m_jit.setupResults(resultPayloadGPR, resultTagGPR);

    jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, DataFormatJS, UseChildrenCalledExplicitly);

    m_jit.addJSCall(fastCall, slowCall, targetToCheck, callType, calleePayloadGPR, at(m_compileIndex).codeOrigin);
}

template<bool strict>
GPRReg SpeculativeJIT::fillSpeculateIntInternal(NodeIndex nodeIndex, DataFormat& returnFormat, SpeculationDirection direction)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("SpecInt@%d   ", nodeIndex);
#endif
    if (isKnownNotInteger(nodeIndex)) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode, direction);
        returnFormat = DataFormatInteger;
        return allocate();
    }

    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {

        if (node.hasConstant()) {
            ASSERT(isInt32Constant(nodeIndex));
            GPRReg gpr = allocate();
            m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            info.fillInteger(*m_stream, gpr);
            returnFormat = DataFormatInteger;
            return gpr;
        }

        DataFormat spillFormat = info.spillFormat();
        ASSERT_UNUSED(spillFormat, (spillFormat & DataFormatJS) || spillFormat == DataFormatInteger);

        // If we know this was spilled as an integer we can fill without checking.
        if (!isInt32Speculation(type))
            speculationCheck(BadType, JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)), direction);

        GPRReg gpr = allocate();
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillInteger(*m_stream, gpr);
        returnFormat = DataFormatInteger;
        return gpr;
    }

    case DataFormatJSInteger:
    case DataFormatJS: {
        // Check the value is an integer.
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);
        if (!isInt32Speculation(type))
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::Int32Tag)), direction);
        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderInteger);
        info.fillInteger(*m_stream, payloadGPR);
        // If !strict we're done, return.
        returnFormat = DataFormatInteger;
        return payloadGPR;
    }

    case DataFormatInteger: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        returnFormat = DataFormatInteger;
        return gpr;
    }

    case DataFormatDouble:
    case DataFormatCell:
    case DataFormatBoolean:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatJSBoolean:
    case DataFormatStorage:
        ASSERT_NOT_REACHED();

    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateInt(NodeIndex nodeIndex, DataFormat& returnFormat, SpeculationDirection direction)
{
    return fillSpeculateIntInternal<false>(nodeIndex, returnFormat, direction);
}

GPRReg SpeculativeJIT::fillSpeculateIntStrict(NodeIndex nodeIndex)
{
    DataFormat mustBeDataFormatInteger;
    GPRReg result = fillSpeculateIntInternal<true>(nodeIndex, mustBeDataFormatInteger, BackwardSpeculation);
    ASSERT(mustBeDataFormatInteger == DataFormatInteger);
    return result;
}

FPRReg SpeculativeJIT::fillSpeculateDouble(NodeIndex nodeIndex, SpeculationDirection direction)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("SpecDouble@%d   ", nodeIndex);
#endif
    if (isKnownNotNumber(nodeIndex)) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode, direction);
        return fprAllocate();
    }

    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {

        if (node.hasConstant()) {
            if (isInt32Constant(nodeIndex)) {
                GPRReg gpr = allocate();
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillInteger(*m_stream, gpr);
                unlock(gpr);
            } else if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(addressOfDoubleConstant(nodeIndex), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderConstant);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            } else
                ASSERT_NOT_REACHED();
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT((spillFormat & DataFormatJS) || spillFormat == DataFormatInteger);
            if (spillFormat == DataFormatJSDouble || spillFormat == DataFormatDouble) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            }

            FPRReg fpr = fprAllocate();
            JITCompiler::Jump hasUnboxedDouble;

            if (spillFormat != DataFormatJSInteger && spillFormat != DataFormatInteger) {
                JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag));
                if (!isNumberSpeculation(type))
                    speculationCheck(BadType, JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::AboveOrEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::LowestTag)), direction);
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                hasUnboxedDouble = m_jit.jump();

                isInteger.link(&m_jit);
            }

            m_jit.convertInt32ToDouble(JITCompiler::payloadFor(virtualRegister), fpr);

            if (hasUnboxedDouble.isSet())
                hasUnboxedDouble.link(&m_jit);

            m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
            info.fillDouble(*m_stream, fpr);
            info.killSpilled();
            return fpr;
        }
    }

    switch (info.registerFormat()) {
    case DataFormatJS:
    case DataFormatJSInteger: {
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        FPRReg fpr = fprAllocate();

        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);

        JITCompiler::Jump hasUnboxedDouble;

        if (info.registerFormat() != DataFormatJSInteger) {
            FPRTemporary scratch(this);
            JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, tagGPR, TrustedImm32(JSValue::Int32Tag));
            if (!isNumberSpeculation(type))
                speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::AboveOrEqual, tagGPR, TrustedImm32(JSValue::LowestTag)), direction);
            unboxDouble(tagGPR, payloadGPR, fpr, scratch.fpr());
            hasUnboxedDouble = m_jit.jump();
            isInteger.link(&m_jit);
        }

        m_jit.convertInt32ToDouble(payloadGPR, fpr);

        if (hasUnboxedDouble.isSet())
            hasUnboxedDouble.link(&m_jit);

        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.unlock(tagGPR);
        m_gprs.unlock(payloadGPR);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(*m_stream, fpr);
        info.killSpilled();
        return fpr;
    }

    case DataFormatInteger: {
        FPRReg fpr = fprAllocate();
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        m_jit.convertInt32ToDouble(gpr, fpr);
        m_gprs.unlock(gpr);
        return fpr;
    }

    case DataFormatJSDouble:
    case DataFormatDouble: {
        FPRReg fpr = info.fpr();
        m_fprs.lock(fpr);
        return fpr;
    }

    case DataFormatNone:
    case DataFormatStorage:
    case DataFormatCell:
    case DataFormatJSCell:
    case DataFormatBoolean:
    case DataFormatJSBoolean:
        ASSERT_NOT_REACHED();

    default:
        ASSERT_NOT_REACHED();
        return InvalidFPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateCell(NodeIndex nodeIndex, SpeculationDirection direction)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("SpecCell@%d   ", nodeIndex);
#endif
    if (isKnownNotCell(nodeIndex)) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode, direction);
        return allocate();
    }

    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            ASSERT(jsValue.isCell());
            GPRReg gpr = allocate();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            m_jit.move(MacroAssembler::TrustedImmPtr(jsValue.asCell()), gpr);
            info.fillCell(*m_stream, gpr);
            return gpr;
        }

        ASSERT((info.spillFormat() & DataFormatJS) || info.spillFormat() == DataFormatCell);
        if (!isCellSpeculation(type))
            speculationCheck(BadType, JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::CellTag)), direction);
        GPRReg gpr = allocate();
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillCell(*m_stream, gpr);
        return gpr;
    }

    case DataFormatCell: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }

    case DataFormatJSCell:
    case DataFormatJS: {
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);
        if (!isCellSpeculation(type))
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::CellTag)), direction);
        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderCell);
        info.fillCell(*m_stream, payloadGPR);
        return payloadGPR;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJSBoolean:
    case DataFormatBoolean:
    case DataFormatStorage:
        ASSERT_NOT_REACHED();

    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateBoolean(NodeIndex nodeIndex, SpeculationDirection direction)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("SpecBool@%d   ", nodeIndex);
#endif
    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    if ((node.hasConstant() && !valueOfJSConstant(nodeIndex).isBoolean())
        || !(info.isJSBoolean() || info.isUnknownJS())) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode, direction);
        return allocate();
    }

    switch (info.registerFormat()) {
    case DataFormatNone: {

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            ASSERT(jsValue.isBoolean());
            GPRReg gpr = allocate();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            m_jit.move(MacroAssembler::TrustedImm32(jsValue.asBoolean()), gpr);
            info.fillBoolean(*m_stream, gpr);
            return gpr;
        }

        ASSERT((info.spillFormat() & DataFormatJS) || info.spillFormat() == DataFormatBoolean);

        if (!isBooleanSpeculation(type))
            speculationCheck(BadType, JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)), direction);

        GPRReg gpr = allocate();
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillBoolean(*m_stream, gpr);
        return gpr;
    }

    case DataFormatBoolean: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }

    case DataFormatJSBoolean:
    case DataFormatJS: {
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);
        if (!isBooleanSpeculation(type))
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::BooleanTag)), direction);

        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderBoolean);
        info.fillBoolean(*m_stream, payloadGPR);
        return payloadGPR;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJSCell:
    case DataFormatCell:
    case DataFormatStorage:
        ASSERT_NOT_REACHED();

    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

JITCompiler::Jump SpeculativeJIT::convertToDouble(JSValueOperand& op, FPRReg result)
{
    FPRTemporary scratch(this);

    JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, op.tagGPR(), TrustedImm32(JSValue::Int32Tag));
    JITCompiler::Jump notNumber = m_jit.branch32(MacroAssembler::AboveOrEqual, op.payloadGPR(), TrustedImm32(JSValue::LowestTag));

    unboxDouble(op.tagGPR(), op.payloadGPR(), result, scratch.fpr());
    JITCompiler::Jump done = m_jit.jump();

    isInteger.link(&m_jit);
    m_jit.convertInt32ToDouble(op.payloadGPR(), result);

    done.link(&m_jit);

    return notNumber;
}

void SpeculativeJIT::compileObjectEquality(Node& node)
{
    SpeculateCellOperand op1(this, node.child1());
    SpeculateCellOperand op2(this, node.child2());
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    if (m_jit.graph().globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint()); 
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node.child1(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(op1GPR, JSCell::structureOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node.child2(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(op2GPR, JSCell::structureOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
    } else {
        GPRTemporary structure(this);
        GPRReg structureGPR = structure.gpr();

        m_jit.loadPtr(MacroAssembler::Address(op1GPR, JSCell::structureOffset()), structureGPR);
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node.child1(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                structureGPR, 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node.child1(), 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));

        m_jit.loadPtr(MacroAssembler::Address(op2GPR, JSCell::structureOffset()), structureGPR);
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node.child2(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                structureGPR, 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node.child2(), 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    GPRTemporary resultPayload(this, op2);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    MacroAssembler::Jump falseCase = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, op2GPR);
    m_jit.move(TrustedImm32(1), resultPayloadGPR);
    MacroAssembler::Jump done = m_jit.jump();
    falseCase.link(&m_jit);
    m_jit.move(TrustedImm32(0), resultPayloadGPR);
    done.link(&m_jit);

    booleanResult(resultPayloadGPR, m_compileIndex);
}

void SpeculativeJIT::compileObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild)
{
    Node& leftNode = m_jit.graph()[leftChild.index()];
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2TagGPR = op2.tagGPR();
    GPRReg op2PayloadGPR = op2.payloadGPR();
    GPRReg resultGPR = result.gpr();
    
    if (m_jit.graph().globalObjectFor(leftNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) { 
        m_jit.graph().globalObjectFor(leftNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(op1GPR, JSCell::structureOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
    } else {
        GPRTemporary structure(this);
        GPRReg structureGPR = structure.gpr();

        m_jit.loadPtr(MacroAssembler::Address(op1GPR, JSCell::structureOffset()), structureGPR);
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal,
                structureGPR,
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild.index(), 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    MacroAssembler::Jump rightNotCell =
        m_jit.branch32(MacroAssembler::NotEqual, op2TagGPR, TrustedImm32(JSValue::CellTag));
    
    // We know that within this branch, rightChild must be a cell.
    if (m_jit.graph().globalObjectFor(leftNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) { 
        m_jit.graph().globalObjectFor(leftNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(op2PayloadGPR, JSCell::structureOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
    } else {
        GPRTemporary structure(this);
        GPRReg structureGPR = structure.gpr();

        m_jit.loadPtr(MacroAssembler::Address(op2PayloadGPR, JSCell::structureOffset()), structureGPR);
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal,
                structureGPR,
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(), 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    MacroAssembler::Jump falseCase = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, op2PayloadGPR);
    MacroAssembler::Jump trueCase = m_jit.jump();
    
    rightNotCell.link(&m_jit);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (!isOtherOrEmptySpeculation(m_state.forNode(rightChild).m_type & ~SpecCell)) {
        m_jit.move(op2TagGPR, resultGPR);
        m_jit.or32(TrustedImm32(1), resultGPR);
        
        speculationCheck(
            BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(),
            m_jit.branch32(
                MacroAssembler::NotEqual, resultGPR,
                MacroAssembler::TrustedImm32(JSValue::NullTag)));
    }
    
    falseCase.link(&m_jit);
    m_jit.move(TrustedImm32(0), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();
    trueCase.link(&m_jit);
    m_jit.move(TrustedImm32(1), resultGPR);
    done.link(&m_jit);
    
    booleanResult(resultGPR, m_compileIndex);
}

void SpeculativeJIT::compilePeepHoleObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild, NodeIndex branchNodeIndex)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();
    
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2TagGPR = op2.tagGPR();
    GPRReg op2PayloadGPR = op2.payloadGPR();
    GPRReg resultGPR = result.gpr();
    
    if (m_jit.graph().globalObjectFor(branchNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(branchNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(op1GPR, JSCell::structureOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
    } else {
        GPRTemporary structure(this);
        GPRReg structureGPR = structure.gpr();

        m_jit.loadPtr(MacroAssembler::Address(op1GPR, JSCell::structureOffset()), structureGPR);
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                structureGPR, 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild.index(), 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    MacroAssembler::Jump rightNotCell =
        m_jit.branch32(MacroAssembler::NotEqual, op2TagGPR, TrustedImm32(JSValue::CellTag));
    
    // We know that within this branch, rightChild must be a cell.
    if (m_jit.graph().globalObjectFor(branchNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(branchNode.codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(op2PayloadGPR, JSCell::structureOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
    } else {
        GPRTemporary structure(this);
        GPRReg structureGPR = structure.gpr();

        m_jit.loadPtr(MacroAssembler::Address(op2PayloadGPR, JSCell::structureOffset()), structureGPR);
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                structureGPR, 
                MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(), 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    branch32(MacroAssembler::Equal, op1GPR, op2PayloadGPR, taken);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (isOtherOrEmptySpeculation(m_state.forNode(rightChild).m_type & ~SpecCell))
        rightNotCell.link(&m_jit);
    else {
        jump(notTaken, ForceJump);
        
        rightNotCell.link(&m_jit);
        m_jit.move(op2TagGPR, resultGPR);
        m_jit.or32(TrustedImm32(1), resultGPR);
        
        speculationCheck(
            BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild.index(),
            m_jit.branch32(
                MacroAssembler::NotEqual, resultGPR,
                MacroAssembler::TrustedImm32(JSValue::NullTag)));
    }
    
    jump(notTaken);
}

void SpeculativeJIT::compileIntegerCompare(Node& node, MacroAssembler::RelationalCondition condition)
{
    SpeculateIntegerOperand op1(this, node.child1());
    SpeculateIntegerOperand op2(this, node.child2());
    GPRTemporary resultPayload(this);
    
    m_jit.compare32(condition, op1.gpr(), op2.gpr(), resultPayload.gpr());
    
    // If we add a DataFormatBool, we should use it here.
    booleanResult(resultPayload.gpr(), m_compileIndex);
}

void SpeculativeJIT::compileDoubleCompare(Node& node, MacroAssembler::DoubleCondition condition)
{
    SpeculateDoubleOperand op1(this, node.child1());
    SpeculateDoubleOperand op2(this, node.child2());
    GPRTemporary resultPayload(this);
    
    m_jit.move(TrustedImm32(1), resultPayload.gpr());
    MacroAssembler::Jump trueCase = m_jit.branchDouble(condition, op1.fpr(), op2.fpr());
    m_jit.move(TrustedImm32(0), resultPayload.gpr());
    trueCase.link(&m_jit);
    
    booleanResult(resultPayload.gpr(), m_compileIndex);
}

void SpeculativeJIT::compileValueAdd(Node& node)
{
    JSValueOperand op1(this, node.child1());
    JSValueOperand op2(this, node.child2());

    GPRReg op1TagGPR = op1.tagGPR();
    GPRReg op1PayloadGPR = op1.payloadGPR();
    GPRReg op2TagGPR = op2.tagGPR();
    GPRReg op2PayloadGPR = op2.payloadGPR();

    flushRegisters();
    
    GPRResult2 resultTag(this);
    GPRResult resultPayload(this);
    if (isKnownNotNumber(node.child1().index()) || isKnownNotNumber(node.child2().index()))
        callOperation(operationValueAddNotNumber, resultTag.gpr(), resultPayload.gpr(), op1TagGPR, op1PayloadGPR, op2TagGPR, op2PayloadGPR);
    else
        callOperation(operationValueAdd, resultTag.gpr(), resultPayload.gpr(), op1TagGPR, op1PayloadGPR, op2TagGPR, op2PayloadGPR);
    
    jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
}

void SpeculativeJIT::compileNonStringCellOrOtherLogicalNot(Edge nodeUse, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeUse);
    GPRTemporary resultPayload(this);
    GPRReg valueTagGPR = value.tagGPR();
    GPRReg valuePayloadGPR = value.payloadGPR();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branch32(MacroAssembler::NotEqual, valueTagGPR, TrustedImm32(JSValue::CellTag));
    if (m_jit.graph().globalObjectFor(m_jit.graph()[nodeUse.index()].codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(m_jit.graph()[nodeUse.index()].codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());

        if (needSpeculationCheck) {
            speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse,
                m_jit.branchPtr(
                    MacroAssembler::Equal,
                    MacroAssembler::Address(valuePayloadGPR, JSCell::structureOffset()),
                    MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        }
    } else {
        GPRTemporary structure(this);
        GPRReg structureGPR = structure.gpr();

        m_jit.loadPtr(MacroAssembler::Address(valuePayloadGPR, JSCell::structureOffset()), structureGPR);

        if (needSpeculationCheck) {
            speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse,
                m_jit.branchPtr(
                    MacroAssembler::Equal,
                    structureGPR,
                    MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        }

        MacroAssembler::Jump isNotMasqueradesAsUndefined = 
            m_jit.branchTest8(
                MacroAssembler::Zero, 
                MacroAssembler::Address(structureGPR, Structure::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined));

        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(structureGPR, Structure::globalObjectOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.graph().globalObjectFor(m_jit.graph()[nodeUse.index()].codeOrigin))));

        isNotMasqueradesAsUndefined.link(&m_jit);
    }
    m_jit.move(TrustedImm32(0), resultPayloadGPR);
    MacroAssembler::Jump done = m_jit.jump();
    
    notCell.link(&m_jit);
 
    COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
    if (needSpeculationCheck) {
        m_jit.move(valueTagGPR, resultPayloadGPR);
        m_jit.or32(TrustedImm32(1), resultPayloadGPR);
        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, 
            m_jit.branch32(
                MacroAssembler::NotEqual, 
                resultPayloadGPR, 
                TrustedImm32(JSValue::NullTag)));
    }
    m_jit.move(TrustedImm32(1), resultPayloadGPR);
    
    done.link(&m_jit);
    
    booleanResult(resultPayloadGPR, m_compileIndex);
}

void SpeculativeJIT::compileLogicalNot(Node& node)
{
    if (at(node.child1()).shouldSpeculateBoolean()) {
        SpeculateBooleanOperand value(this, node.child1());
        GPRTemporary result(this, value);
        m_jit.xor32(TrustedImm32(1), value.gpr(), result.gpr());
        booleanResult(result.gpr(), m_compileIndex);
        return;
    }
    if (at(node.child1()).shouldSpeculateNonStringCellOrOther()) {
        compileNonStringCellOrOtherLogicalNot(node.child1(), 
            !isNonStringCellOrOtherSpeculation(m_state.forNode(node.child1()).m_type));
        return;
    }
    if (at(node.child1()).shouldSpeculateInteger()) {
        SpeculateIntegerOperand value(this, node.child1());
        GPRTemporary resultPayload(this, value);
        m_jit.compare32(MacroAssembler::Equal, value.gpr(), MacroAssembler::TrustedImm32(0), resultPayload.gpr());
        booleanResult(resultPayload.gpr(), m_compileIndex);
        return;
    }
    if (at(node.child1()).shouldSpeculateNumber()) {
        SpeculateDoubleOperand value(this, node.child1());
        FPRTemporary scratch(this);
        GPRTemporary resultPayload(this);
        m_jit.move(TrustedImm32(0), resultPayload.gpr());
        MacroAssembler::Jump nonZero = m_jit.branchDoubleNonZero(value.fpr(), scratch.fpr());
        m_jit.move(TrustedImm32(1), resultPayload.gpr());
        nonZero.link(&m_jit);
        booleanResult(resultPayload.gpr(), m_compileIndex);
        return;
    }

    JSValueOperand arg1(this, node.child1());
    GPRTemporary resultPayload(this, arg1, false);
    GPRReg arg1TagGPR = arg1.tagGPR();
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg resultPayloadGPR = resultPayload.gpr();
        
    arg1.use();

    JITCompiler::Jump slowCase = m_jit.branch32(JITCompiler::NotEqual, arg1TagGPR, TrustedImm32(JSValue::BooleanTag));
    
    m_jit.move(arg1PayloadGPR, resultPayloadGPR);

    addSlowPathGenerator(
        slowPathCall(
            slowCase, this, dfgConvertJSValueToBoolean, resultPayloadGPR, arg1TagGPR,
            arg1PayloadGPR));
    
    m_jit.xor32(TrustedImm32(1), resultPayloadGPR);
    booleanResult(resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitNonStringCellOrOtherBranch(Edge nodeUse, BlockIndex taken, BlockIndex notTaken, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeUse);
    GPRTemporary scratch(this);
    GPRReg valueTagGPR = value.tagGPR();
    GPRReg valuePayloadGPR = value.payloadGPR();
    GPRReg scratchGPR = scratch.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branch32(MacroAssembler::NotEqual, valueTagGPR, TrustedImm32(JSValue::CellTag));
    if (m_jit.graph().globalObjectFor(m_jit.graph()[nodeUse.index()].codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
        m_jit.graph().globalObjectFor(m_jit.graph()[nodeUse.index()].codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());

        if (needSpeculationCheck) {
            speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, 
                m_jit.branchPtr(
                    MacroAssembler::Equal, 
                    MacroAssembler::Address(valuePayloadGPR, JSCell::structureOffset()), 
                    MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        }
    } else {
        m_jit.loadPtr(MacroAssembler::Address(valuePayloadGPR, JSCell::structureOffset()), scratchGPR);

        if (needSpeculationCheck) {
            speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse.index(), 
                m_jit.branchPtr(
                    MacroAssembler::Equal, 
                    scratchGPR,
                    MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        }

        JITCompiler::Jump isNotMasqueradesAsUndefined = m_jit.branchTest8(JITCompiler::Zero, MacroAssembler::Address(scratchGPR, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));

        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse.index(), 
            m_jit.branchPtr(
                MacroAssembler::Equal, 
                MacroAssembler::Address(scratchGPR, Structure::globalObjectOffset()), 
                MacroAssembler::TrustedImmPtr(m_jit.graph().globalObjectFor(m_jit.graph()[nodeUse.index()].codeOrigin))));

        isNotMasqueradesAsUndefined.link(&m_jit);
    }
    jump(taken, ForceJump);
    
    notCell.link(&m_jit);
    
    COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
    if (needSpeculationCheck) {
        m_jit.move(valueTagGPR, scratchGPR);
        m_jit.or32(TrustedImm32(1), scratchGPR);
        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, m_jit.branch32(MacroAssembler::NotEqual, scratchGPR, TrustedImm32(JSValue::NullTag)));
    }

    jump(notTaken);
    
    noResult(m_compileIndex);
}

void SpeculativeJIT::emitBranch(Node& node)
{
    BlockIndex taken = node.takenBlockIndex();
    BlockIndex notTaken = node.notTakenBlockIndex();

    if (at(node.child1()).shouldSpeculateBoolean()) {
        SpeculateBooleanOperand value(this, node.child1());
        MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;

        if (taken == nextBlock()) {
            condition = MacroAssembler::Zero;
            BlockIndex tmp = taken;
            taken = notTaken;
            notTaken = tmp;
        }

        branchTest32(condition, value.gpr(), TrustedImm32(1), taken);
        jump(notTaken);

        noResult(m_compileIndex);
    } else if (at(node.child1()).shouldSpeculateNonStringCellOrOther()) {
        emitNonStringCellOrOtherBranch(node.child1(), taken, notTaken, 
            !isNonStringCellOrOtherSpeculation(m_state.forNode(node.child1()).m_type));
    } else if (at(node.child1()).shouldSpeculateNumber()) {
        if (at(node.child1()).shouldSpeculateInteger()) {
            bool invert = false;
            
            if (taken == nextBlock()) {
                invert = true;
                BlockIndex tmp = taken;
                taken = notTaken;
                notTaken = tmp;
            }

            SpeculateIntegerOperand value(this, node.child1());
            branchTest32(invert ? MacroAssembler::Zero : MacroAssembler::NonZero, value.gpr(), taken);
        } else {
            SpeculateDoubleOperand value(this, node.child1());
            FPRTemporary scratch(this);
            branchDoubleNonZero(value.fpr(), scratch.fpr(), taken);
        }
        
        jump(notTaken);
        
        noResult(m_compileIndex);
    } else {
        JSValueOperand value(this, node.child1());
        value.fill();
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();

        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
    
        use(node.child1());
    
        JITCompiler::Jump fastPath = m_jit.branch32(JITCompiler::Equal, valueTagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag));
        JITCompiler::Jump slowPath = m_jit.branch32(JITCompiler::NotEqual, valueTagGPR, JITCompiler::TrustedImm32(JSValue::BooleanTag));

        fastPath.link(&m_jit);
        branchTest32(JITCompiler::Zero, valuePayloadGPR, notTaken);
        jump(taken, ForceJump);

        slowPath.link(&m_jit);
        silentSpillAllRegisters(resultGPR);
        callOperation(dfgConvertJSValueToBoolean, resultGPR, valueTagGPR, valuePayloadGPR);
        silentFillAllRegisters(resultGPR);
    
        branchTest32(JITCompiler::NonZero, resultGPR, taken);
        jump(notTaken);
    
        noResult(m_compileIndex, UseChildrenCalledExplicitly);
    }
}

template<typename BaseOperandType, typename PropertyOperandType, typename ValueOperandType, typename TagType>
void SpeculativeJIT::compileContiguousPutByVal(Node& node, BaseOperandType& base, PropertyOperandType& property, ValueOperandType& value, GPRReg valuePayloadReg, TagType valueTag)
{
    Edge child4 = m_jit.graph().varArgChild(node, 3);

    ArrayMode arrayMode = node.arrayMode();
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    
    StorageOperand storage(this, child4);
    GPRReg storageReg = storage.gpr();

    if (node.op() == PutByValAlias) {
        // Store the value to the array.
        GPRReg propertyReg = property.gpr();
        m_jit.store32(valueTag, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
        m_jit.store32(valuePayloadReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
        
        noResult(m_compileIndex);
        return;
    }
    
    MacroAssembler::Jump slowCase;

    if (arrayMode.isInBounds()) {
        speculationCheck(
            StoreToHoleOrOutOfBounds, JSValueRegs(), NoNode,
            m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));
    } else {
        MacroAssembler::Jump inBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength()));
        
        slowCase = m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfVectorLength()));
        
        if (!arrayMode.isOutOfBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), NoNode, slowCase);
        
        m_jit.add32(TrustedImm32(1), propertyReg);
        m_jit.store32(propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength()));
        m_jit.sub32(TrustedImm32(1), propertyReg);
        
        inBounds.link(&m_jit);
    }
    
    m_jit.store32(valueTag, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
    m_jit.store32(valuePayloadReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
    
    base.use();
    property.use();
    value.use();
    storage.use();
    
    if (arrayMode.isOutOfBounds()) {
        addSlowPathGenerator(
            slowPathCall(
                slowCase, this,
                m_jit.codeBlock()->isStrictMode() ? operationPutByValBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsNonStrict,
                NoResult, baseReg, propertyReg, valueTag, valuePayloadReg));
    }

    noResult(m_compileIndex, UseChildrenCalledExplicitly);    
}

void SpeculativeJIT::compile(Node& node)
{
    NodeType op = node.op();

    switch (op) {
    case JSConstant:
        initConstantInfo(m_compileIndex);
        break;

    case PhantomArguments:
        initConstantInfo(m_compileIndex);
        break;

    case WeakJSConstant:
        m_jit.addWeakReference(node.weakConstant());
        initConstantInfo(m_compileIndex);
        break;

    case Identity: {
        // This could be done a lot better. We take the cheap way out because Identity
        // is only going to stick around after CSE if we had prediction weirdness.
        JSValueOperand operand(this, node.child1());
        GPRTemporary resultTag(this);
        GPRTemporary resultPayload(this);
        m_jit.move(operand.tagGPR(), resultTag.gpr());
        m_jit.move(operand.payloadGPR(), resultPayload.gpr());
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case GetLocal: {
        SpeculatedType prediction = node.variableAccessData()->prediction();
        AbstractValue& value = block()->valuesAtHead.operand(node.local());

        // If we have no prediction for this local, then don't attempt to compile.
        if (prediction == SpecNone) {
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        }
        
        if (!node.variableAccessData()->isCaptured()) {
            // If the CFA is tracking this variable and it found that the variable
            // cannot have been assigned, then don't attempt to proceed.
            if (value.isClear()) {
                terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
                break;
            }
            
            if (node.variableAccessData()->shouldUseDoubleFormat()) {
                FPRTemporary result(this);
                m_jit.loadDouble(JITCompiler::addressFor(node.local()), result.fpr());
                VirtualRegister virtualRegister = node.virtualRegister();
                m_fprs.retain(result.fpr(), virtualRegister, SpillOrderDouble);
                m_generationInfo[virtualRegister].initDouble(m_compileIndex, node.refCount(), result.fpr());
                break;
            }
        
            if (isInt32Speculation(value.m_type)) {
                GPRTemporary result(this);
                m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

                // Like integerResult, but don't useChildren - our children are phi nodes,
                // and don't represent values within this dataflow with virtual registers.
                VirtualRegister virtualRegister = node.virtualRegister();
                m_gprs.retain(result.gpr(), virtualRegister, SpillOrderInteger);
                m_generationInfo[virtualRegister].initInteger(m_compileIndex, node.refCount(), result.gpr());
                break;
            }

            if (isCellSpeculation(value.m_type)) {
                GPRTemporary result(this);
                m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

                // Like cellResult, but don't useChildren - our children are phi nodes,
                // and don't represent values within this dataflow with virtual registers.
                VirtualRegister virtualRegister = node.virtualRegister();
                m_gprs.retain(result.gpr(), virtualRegister, SpillOrderCell);
                m_generationInfo[virtualRegister].initCell(m_compileIndex, node.refCount(), result.gpr());
                break;
            }

            if (isBooleanSpeculation(value.m_type)) {
                GPRTemporary result(this);
                m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

                // Like booleanResult, but don't useChildren - our children are phi nodes,
                // and don't represent values within this dataflow with virtual registers.
                VirtualRegister virtualRegister = node.virtualRegister();
                m_gprs.retain(result.gpr(), virtualRegister, SpillOrderBoolean);
                m_generationInfo[virtualRegister].initBoolean(m_compileIndex, node.refCount(), result.gpr());
                break;
            }
        }

        GPRTemporary result(this);
        GPRTemporary tag(this);
        m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());
        m_jit.load32(JITCompiler::tagFor(node.local()), tag.gpr());

        // Like jsValueResult, but don't useChildren - our children are phi nodes,
        // and don't represent values within this dataflow with virtual registers.
        VirtualRegister virtualRegister = node.virtualRegister();
        m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);
        m_gprs.retain(tag.gpr(), virtualRegister, SpillOrderJS);

        DataFormat format;
        if (isCellSpeculation(value.m_type)
            && !node.variableAccessData()->isCaptured())
            format = DataFormatJSCell;
        else
            format = DataFormatJS;
        m_generationInfo[virtualRegister].initJSValue(m_compileIndex, node.refCount(), tag.gpr(), result.gpr(), format);
        break;
    }
        
    case GetLocalUnlinked: {
        GPRTemporary payload(this);
        GPRTemporary tag(this);
        m_jit.load32(JITCompiler::payloadFor(node.unlinkedLocal()), payload.gpr());
        m_jit.load32(JITCompiler::tagFor(node.unlinkedLocal()), tag.gpr());
        jsValueResult(tag.gpr(), payload.gpr(), m_compileIndex);
        break;
    }

    case SetLocal: {
        // SetLocal doubles as a hint as to where a node will be stored and
        // as a speculation point. So before we speculate make sure that we
        // know where the child of this node needs to go in the virtual
        // stack.
        compileMovHint(node);
        
        if (!node.variableAccessData()->isCaptured() && !m_jit.graph().isCreatedThisArgument(node.local())) {
            if (node.variableAccessData()->shouldUseDoubleFormat()) {
                SpeculateDoubleOperand value(this, node.child1(), ForwardSpeculation);
                m_jit.storeDouble(value.fpr(), JITCompiler::addressFor(node.local()));
                noResult(m_compileIndex);
                // Indicate that it's no longer necessary to retrieve the value of
                // this bytecode variable from registers or other locations in the stack,
                // but that it is stored as a double.
                recordSetLocal(node.local(), ValueSource(DoubleInJSStack));
                break;
            }
            SpeculatedType predictedType = node.variableAccessData()->argumentAwarePrediction();
            if (m_generationInfo[at(node.child1()).virtualRegister()].registerFormat() == DataFormatDouble) {
                DoubleOperand value(this, node.child1());
                m_jit.storeDouble(value.fpr(), JITCompiler::addressFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(DoubleInJSStack));
                break;
            }
            if (isInt32Speculation(predictedType)) {
                SpeculateIntegerOperand value(this, node.child1(), ForwardSpeculation);
                m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(Int32InJSStack));
                break;
            }
            if (isCellSpeculation(predictedType)) {
                SpeculateCellOperand cell(this, node.child1(), ForwardSpeculation);
                GPRReg cellGPR = cell.gpr();
                m_jit.storePtr(cellGPR, JITCompiler::payloadFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(CellInJSStack));
                break;
            }
            if (isBooleanSpeculation(predictedType)) {
                SpeculateBooleanOperand value(this, node.child1(), ForwardSpeculation);
                m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(BooleanInJSStack));
                break;
            }
        }
        JSValueOperand value(this, node.child1());
        m_jit.store32(value.payloadGPR(), JITCompiler::payloadFor(node.local()));
        m_jit.store32(value.tagGPR(), JITCompiler::tagFor(node.local()));
        noResult(m_compileIndex);
        recordSetLocal(node.local(), ValueSource(ValueInJSStack));

        // If we're storing an arguments object that has been optimized away,
        // our variable event stream for OSR exit now reflects the optimized
        // value (JSValue()). On the slow path, we want an arguments object
        // instead. We add an additional move hint to show OSR exit that it
        // needs to reconstruct the arguments object.
        if (at(node.child1()).op() == PhantomArguments)
            compileMovHint(node);

        break;
    }

    case SetArgument:
        // This is a no-op; it just marks the fact that the argument is being used.
        // But it may be profitable to use this as a hook to run speculation checks
        // on arguments, thereby allowing us to trivially eliminate such checks if
        // the argument is not used.
        break;

    case BitAnd:
    case BitOr:
    case BitXor:
        if (isInt32Constant(node.child1().index())) {
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op2);

            bitOp(op, valueOfInt32Constant(node.child1().index()), op2.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else if (isInt32Constant(node.child2().index())) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);

            bitOp(op, valueOfInt32Constant(node.child2().index()), op1.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else {
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op1, op2);

            GPRReg reg1 = op1.gpr();
            GPRReg reg2 = op2.gpr();
            bitOp(op, reg1, reg2, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        }
        break;

    case BitRShift:
    case BitLShift:
    case BitURShift:
        if (isInt32Constant(node.child2().index())) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);

            shiftOp(op, op1.gpr(), valueOfInt32Constant(node.child2().index()) & 0x1f, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else {
            // Do not allow shift amount to be used as the result, MacroAssembler does not permit this.
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op1);

            GPRReg reg1 = op1.gpr();
            GPRReg reg2 = op2.gpr();
            shiftOp(op, reg1, reg2, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        }
        break;

    case UInt32ToNumber: {
        compileUInt32ToNumber(node);
        break;
    }
        
    case DoubleAsInt32: {
        compileDoubleAsInt32(node);
        break;
    }

    case ValueToInt32: {
        compileValueToInt32(node);
        break;
    }
        
    case Int32ToDouble: {
        compileInt32ToDouble(node);
        break;
    }
        
    case CheckNumber: {
        if (!isNumberSpeculation(m_state.forNode(node.child1()).m_type)) {
            JSValueOperand op1(this, node.child1());
            JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, op1.tagGPR(), TrustedImm32(JSValue::Int32Tag));
            speculationCheck(
                BadType, JSValueRegs(op1.tagGPR(), op1.payloadGPR()), node.child1().index(),
                m_jit.branch32(MacroAssembler::AboveOrEqual, op1.tagGPR(), TrustedImm32(JSValue::LowestTag)));
            isInteger.link(&m_jit);
        }
        noResult(m_compileIndex);
        break;
    }

    case ValueAdd:
    case ArithAdd:
        compileAdd(node);
        break;

    case ArithSub:
        compileArithSub(node);
        break;

    case ArithNegate:
        compileArithNegate(node);
        break;

    case ArithMul:
        compileArithMul(node);
        break;

    case ArithDiv: {
        if (Node::shouldSpeculateIntegerForArithmetic(at(node.child1()), at(node.child2()))
            && node.canSpeculateInteger()) {
#if CPU(X86)
            compileIntegerArithDivForX86(node);
#else // CPU(X86) -> so non-X86 code follows
            ASSERT_NOT_REACHED(); // should have been coverted into a double divide.
#endif // CPU(X86)
            break;
        }
        
        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        FPRTemporary result(this, op1);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        m_jit.divDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case ArithMod: {
        compileArithMod(node);
        break;
    }

    case ArithAbs: {
        if (at(node.child1()).shouldSpeculateIntegerForArithmetic()
            && node.canSpeculateInteger()) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);
            GPRTemporary scratch(this);
            
            m_jit.zeroExtend32ToPtr(op1.gpr(), result.gpr());
            m_jit.rshift32(result.gpr(), MacroAssembler::TrustedImm32(31), scratch.gpr());
            m_jit.add32(scratch.gpr(), result.gpr());
            m_jit.xor32(scratch.gpr(), result.gpr());
            speculationCheck(Overflow, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Equal, result.gpr(), MacroAssembler::TrustedImm32(1 << 31)));
            integerResult(result.gpr(), m_compileIndex);
            break;
        }
        
        SpeculateDoubleOperand op1(this, node.child1());
        FPRTemporary result(this);
        
        m_jit.absDouble(op1.fpr(), result.fpr());
        doubleResult(result.fpr(), m_compileIndex);
        break;
    }
        
    case ArithMin:
    case ArithMax: {
        if (Node::shouldSpeculateIntegerForArithmetic(at(node.child1()), at(node.child2()))
            && node.canSpeculateInteger()) {
            SpeculateStrictInt32Operand op1(this, node.child1());
            SpeculateStrictInt32Operand op2(this, node.child2());
            GPRTemporary result(this, op1);
            
            MacroAssembler::Jump op1Less = m_jit.branch32(op == ArithMin ? MacroAssembler::LessThan : MacroAssembler::GreaterThan, op1.gpr(), op2.gpr());
            m_jit.move(op2.gpr(), result.gpr());
            if (op1.gpr() != result.gpr()) {
                MacroAssembler::Jump done = m_jit.jump();
                op1Less.link(&m_jit);
                m_jit.move(op1.gpr(), result.gpr());
                done.link(&m_jit);
            } else
                op1Less.link(&m_jit);
            
            integerResult(result.gpr(), m_compileIndex);
            break;
        }
        
        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        FPRTemporary result(this, op1);
        
        MacroAssembler::JumpList done;
        
        MacroAssembler::Jump op1Less = m_jit.branchDouble(op == ArithMin ? MacroAssembler::DoubleLessThan : MacroAssembler::DoubleGreaterThan, op1.fpr(), op2.fpr());
        
        // op2 is eather the lesser one or one of then is NaN
        MacroAssembler::Jump op2Less = m_jit.branchDouble(op == ArithMin ? MacroAssembler::DoubleGreaterThanOrEqual : MacroAssembler::DoubleLessThanOrEqual, op1.fpr(), op2.fpr());
        
        // Unordered case. We don't know which of op1, op2 is NaN. Manufacture NaN by adding 
        // op1 + op2 and putting it into result.
        m_jit.addDouble(op1.fpr(), op2.fpr(), result.fpr());
        done.append(m_jit.jump());
        
        op2Less.link(&m_jit);
        m_jit.moveDouble(op2.fpr(), result.fpr());
        
        if (op1.fpr() != result.fpr()) {
            done.append(m_jit.jump());
            
            op1Less.link(&m_jit);
            m_jit.moveDouble(op1.fpr(), result.fpr());
        } else
            op1Less.link(&m_jit);
        
        done.link(&m_jit);
        
        doubleResult(result.fpr(), m_compileIndex);
        break;
    }
        
    case ArithSqrt: {
        SpeculateDoubleOperand op1(this, node.child1());
        FPRTemporary result(this, op1);
        
        m_jit.sqrtDouble(op1.fpr(), result.fpr());
        
        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case LogicalNot:
        compileLogicalNot(node);
        break;

    case CompareLess:
        if (compare(node, JITCompiler::LessThan, JITCompiler::DoubleLessThan, operationCompareLess))
            return;
        break;

    case CompareLessEq:
        if (compare(node, JITCompiler::LessThanOrEqual, JITCompiler::DoubleLessThanOrEqual, operationCompareLessEq))
            return;
        break;

    case CompareGreater:
        if (compare(node, JITCompiler::GreaterThan, JITCompiler::DoubleGreaterThan, operationCompareGreater))
            return;
        break;

    case CompareGreaterEq:
        if (compare(node, JITCompiler::GreaterThanOrEqual, JITCompiler::DoubleGreaterThanOrEqual, operationCompareGreaterEq))
            return;
        break;

    case CompareEq:
        if (isNullConstant(node.child1().index())) {
            if (nonSpeculativeCompareNull(node, node.child2()))
                return;
            break;
        }
        if (isNullConstant(node.child2().index())) {
            if (nonSpeculativeCompareNull(node, node.child1()))
                return;
            break;
        }
        if (compare(node, JITCompiler::Equal, JITCompiler::DoubleEqual, operationCompareEq))
            return;
        break;

    case CompareStrictEq:
        if (compileStrictEq(node))
            return;
        break;

    case StringCharCodeAt: {
        compileGetCharCodeAt(node);
        break;
    }

    case StringCharAt: {
        // Relies on StringCharAt node having same basic layout as GetByVal
        compileGetByValOnString(node);
        break;
    }
        
    case CheckArray: {
        checkArray(node);
        break;
    }
        
    case Arrayify:
    case ArrayifyToStructure: {
        arrayify(node);
        break;
    }

    case GetByVal: {
        switch (node.arrayMode().type()) {
        case Array::SelectUsingPredictions:
        case Array::ForceExit:
            ASSERT_NOT_REACHED();
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        case Array::Generic: {
            SpeculateCellOperand base(this, node.child1()); // Save a register, speculate cell. We'll probably be right.
            JSValueOperand property(this, node.child2());
            GPRReg baseGPR = base.gpr();
            GPRReg propertyTagGPR = property.tagGPR();
            GPRReg propertyPayloadGPR = property.payloadGPR();
            
            flushRegisters();
            GPRResult2 resultTag(this);
            GPRResult resultPayload(this);
            callOperation(operationGetByValCell, resultTag.gpr(), resultPayload.gpr(), baseGPR, propertyTagGPR, propertyPayloadGPR);
            
            jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
            break;
        }
        case Array::Int32:
        case Array::Contiguous: {
            if (node.arrayMode().isInBounds()) {
                SpeculateStrictInt32Operand property(this, node.child2());
                StorageOperand storage(this, node.child3());
            
                GPRReg propertyReg = property.gpr();
                GPRReg storageReg = storage.gpr();
            
                if (!m_compileOkay)
                    return;
            
                speculationCheck(OutOfBounds, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));
            
                GPRTemporary resultPayload(this);
                if (node.arrayMode().type() == Array::Int32) {
                    speculationCheck(
                        OutOfBounds, JSValueRegs(), NoNode,
                        m_jit.branch32(
                            MacroAssembler::Equal,
                            MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)),
                            TrustedImm32(JSValue::EmptyValueTag)));
                    m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayload.gpr());
                    integerResult(resultPayload.gpr(), m_compileIndex);
                    break;
                }
                
                GPRTemporary resultTag(this);
                m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTag.gpr());
                speculationCheck(LoadFromHole, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Equal, resultTag.gpr(), TrustedImm32(JSValue::EmptyValueTag)));
                m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayload.gpr());
                jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
                break;
            }

            SpeculateCellOperand base(this, node.child1());
            SpeculateStrictInt32Operand property(this, node.child2());
            StorageOperand storage(this, node.child3());
            
            GPRReg baseReg = base.gpr();
            GPRReg propertyReg = property.gpr();
            GPRReg storageReg = storage.gpr();
            
            if (!m_compileOkay)
                return;
            
            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            GPRReg resultTagReg = resultTag.gpr();
            GPRReg resultPayloadReg = resultPayload.gpr();
            
            MacroAssembler::JumpList slowCases;

            slowCases.append(m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));
            
            m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTagReg);
            m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayloadReg);
            slowCases.append(m_jit.branch32(MacroAssembler::Equal, resultTagReg, TrustedImm32(JSValue::EmptyValueTag)));
            
            addSlowPathGenerator(
                slowPathCall(
                    slowCases, this, operationGetByValArrayInt,
                    JSValueRegs(resultTagReg, resultPayloadReg), baseReg, propertyReg));
            
            jsValueResult(resultTagReg, resultPayloadReg, m_compileIndex);
            break;
        }
        case Array::Double: {
            if (node.arrayMode().isInBounds()) {
                if (node.arrayMode().isSaneChain()) {
                    JSGlobalObject* globalObject = m_jit.globalObjectFor(node.codeOrigin);
                    ASSERT(globalObject->arrayPrototypeChainIsSane());
                    globalObject->arrayPrototype()->structure()->addTransitionWatchpoint(speculationWatchpoint());
                    globalObject->objectPrototype()->structure()->addTransitionWatchpoint(speculationWatchpoint());
                }
                
                SpeculateStrictInt32Operand property(this, node.child2());
                StorageOperand storage(this, node.child3());
            
                GPRReg propertyReg = property.gpr();
                GPRReg storageReg = storage.gpr();
            
                if (!m_compileOkay)
                    return;
            
                speculationCheck(OutOfBounds, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));
            
                FPRTemporary result(this);
                m_jit.loadDouble(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), result.fpr());
                if (!node.arrayMode().isSaneChain())
                    speculationCheck(LoadFromHole, JSValueRegs(), NoNode, m_jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, result.fpr(), result.fpr()));
                doubleResult(result.fpr(), m_compileIndex);
                break;
            }

            SpeculateCellOperand base(this, node.child1());
            SpeculateStrictInt32Operand property(this, node.child2());
            StorageOperand storage(this, node.child3());
            
            GPRReg baseReg = base.gpr();
            GPRReg propertyReg = property.gpr();
            GPRReg storageReg = storage.gpr();
            
            if (!m_compileOkay)
                return;
            
            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            FPRTemporary temp(this);
            GPRReg resultTagReg = resultTag.gpr();
            GPRReg resultPayloadReg = resultPayload.gpr();
            FPRReg tempReg = temp.fpr();
            
            MacroAssembler::JumpList slowCases;
            
            slowCases.append(m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));
            
            m_jit.loadDouble(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), tempReg);
            slowCases.append(m_jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, tempReg, tempReg));
            boxDouble(tempReg, resultTagReg, resultPayloadReg);

            addSlowPathGenerator(
                slowPathCall(
                    slowCases, this, operationGetByValArrayInt,
                    JSValueRegs(resultTagReg, resultPayloadReg), baseReg, propertyReg));
            
            jsValueResult(resultTagReg, resultPayloadReg, m_compileIndex);
            break;
        }
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage: {
            if (node.arrayMode().isInBounds()) {
                SpeculateStrictInt32Operand property(this, node.child2());
                StorageOperand storage(this, node.child3());
                GPRReg propertyReg = property.gpr();
                GPRReg storageReg = storage.gpr();
        
                if (!m_compileOkay)
                    return;

                speculationCheck(OutOfBounds, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, ArrayStorage::vectorLengthOffset())));

                GPRTemporary resultTag(this);
                GPRTemporary resultPayload(this);

                m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTag.gpr());
                speculationCheck(LoadFromHole, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Equal, resultTag.gpr(), TrustedImm32(JSValue::EmptyValueTag)));
                m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayload.gpr());
            
                jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
                break;
            }

            SpeculateCellOperand base(this, node.child1());
            SpeculateStrictInt32Operand property(this, node.child2());
            StorageOperand storage(this, node.child3());
            GPRReg propertyReg = property.gpr();
            GPRReg storageReg = storage.gpr();
            GPRReg baseReg = base.gpr();

            if (!m_compileOkay)
                return;

            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            GPRReg resultTagReg = resultTag.gpr();
            GPRReg resultPayloadReg = resultPayload.gpr();

            JITCompiler::Jump outOfBounds = m_jit.branch32(
                MacroAssembler::AboveOrEqual, propertyReg,
                MacroAssembler::Address(storageReg, ArrayStorage::vectorLengthOffset()));

            m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTagReg);
            JITCompiler::Jump hole = m_jit.branch32(
                MacroAssembler::Equal, resultTag.gpr(), TrustedImm32(JSValue::EmptyValueTag));
            m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayloadReg);
            
            JITCompiler::JumpList slowCases;
            slowCases.append(outOfBounds);
            slowCases.append(hole);
            addSlowPathGenerator(
                slowPathCall(
                    slowCases, this, operationGetByValArrayInt,
                    JSValueRegs(resultTagReg, resultPayloadReg),
                    baseReg, propertyReg));

            jsValueResult(resultTagReg, resultPayloadReg, m_compileIndex);
            break;
        }
        case Array::String:
            compileGetByValOnString(node);
            break;
        case Array::Arguments:
            compileGetByValOnArguments(node);
            break;
        case Array::Int8Array:
            compileGetByValOnIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), node, sizeof(int8_t), SignedTypedArray);
            break;
        case Array::Int16Array:
            compileGetByValOnIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), node, sizeof(int16_t), SignedTypedArray);
            break;
        case Array::Int32Array:
            compileGetByValOnIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), node, sizeof(int32_t), SignedTypedArray);
            break;
        case Array::Uint8Array:
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), node, sizeof(uint8_t), UnsignedTypedArray);
            break;
        case Array::Uint8ClampedArray:
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint8ClampedArrayDescriptor(), node, sizeof(uint8_t), UnsignedTypedArray);
            break;
        case Array::Uint16Array:
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), node, sizeof(uint16_t), UnsignedTypedArray);
            break;
        case Array::Uint32Array:
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), node, sizeof(uint32_t), UnsignedTypedArray);
            break;
        case Array::Float32Array:
            compileGetByValOnFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), node, sizeof(float));
            break;
        case Array::Float64Array:
            compileGetByValOnFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), node, sizeof(double));
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case PutByVal:
    case PutByValAlias: {
        Edge child1 = m_jit.graph().varArgChild(node, 0);
        Edge child2 = m_jit.graph().varArgChild(node, 1);
        Edge child3 = m_jit.graph().varArgChild(node, 2);
        Edge child4 = m_jit.graph().varArgChild(node, 3);
        
        ArrayMode arrayMode = node.arrayMode().modeForPut();
        bool alreadyHandled = false;
        
        switch (arrayMode.type()) {
        case Array::SelectUsingPredictions:
        case Array::ForceExit:
            ASSERT_NOT_REACHED();
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            alreadyHandled = true;
            break;
        case Array::Generic: {
            ASSERT(node.op() == PutByVal);
            
            SpeculateCellOperand base(this, child1); // Save a register, speculate cell. We'll probably be right.
            JSValueOperand property(this, child2);
            JSValueOperand value(this, child3);
            GPRReg baseGPR = base.gpr();
            GPRReg propertyTagGPR = property.tagGPR();
            GPRReg propertyPayloadGPR = property.payloadGPR();
            GPRReg valueTagGPR = value.tagGPR();
            GPRReg valuePayloadGPR = value.payloadGPR();
            
            flushRegisters();
            callOperation(m_jit.codeBlock()->isStrictMode() ? operationPutByValCellStrict : operationPutByValCellNonStrict, baseGPR, propertyTagGPR, propertyPayloadGPR, valueTagGPR, valuePayloadGPR);
            
            noResult(m_compileIndex);
            alreadyHandled = true;
            break;
        }
        default:
            break;
        }
        
        if (alreadyHandled)
            break;
        
        SpeculateCellOperand base(this, child1);
        SpeculateStrictInt32Operand property(this, child2);
        
        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();

        switch (arrayMode.type()) {
        case Array::Int32: {
            SpeculateIntegerOperand value(this, child3);

            GPRReg valuePayloadReg = value.gpr();
        
            if (!m_compileOkay)
                return;
            
            compileContiguousPutByVal(node, base, property, value, valuePayloadReg, TrustedImm32(JSValue::Int32Tag));
            break;
        }
        case Array::Contiguous: {
            JSValueOperand value(this, child3);

            GPRReg valueTagReg = value.tagGPR();
            GPRReg valuePayloadReg = value.payloadGPR();
        
            if (!m_compileOkay)
                return;
        
            if (Heap::isWriteBarrierEnabled()) {
                GPRTemporary scratch(this);
                writeBarrier(baseReg, valueTagReg, child3, WriteBarrierForPropertyAccess, scratch.gpr());
            }
            
            compileContiguousPutByVal(node, base, property, value, valuePayloadReg, valueTagReg);
            break;
        }
        case Array::Double: {
            compileDoublePutByVal(node, base, property);
            break;
        }
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage: {
            JSValueOperand value(this, child3);

            GPRReg valueTagReg = value.tagGPR();
            GPRReg valuePayloadReg = value.payloadGPR();
            
            if (!m_compileOkay)
                return;
            
            {
                GPRTemporary scratch(this);
                GPRReg scratchReg = scratch.gpr();
                writeBarrier(baseReg, valueTagReg, child3, WriteBarrierForPropertyAccess, scratchReg);
            }
            
            StorageOperand storage(this, child4);
            GPRReg storageReg = storage.gpr();

            if (node.op() == PutByValAlias) {
                // Store the value to the array.
                GPRReg propertyReg = property.gpr();
                m_jit.store32(value.tagGPR(), MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
                m_jit.store32(value.payloadGPR(), MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
                
                noResult(m_compileIndex);
                break;
            }

            MacroAssembler::JumpList slowCases;

            MacroAssembler::Jump beyondArrayBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, ArrayStorage::vectorLengthOffset()));
            if (!arrayMode.isOutOfBounds())
                speculationCheck(OutOfBounds, JSValueRegs(), NoNode, beyondArrayBounds);
            else
                slowCases.append(beyondArrayBounds);

            // Check if we're writing to a hole; if so increment m_numValuesInVector.
            if (arrayMode.isInBounds()) {
                speculationCheck(
                    StoreToHole, JSValueRegs(), NoNode,
                    m_jit.branch32(MacroAssembler::Equal, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag)));
            } else {
                MacroAssembler::Jump notHoleValue = m_jit.branch32(MacroAssembler::NotEqual, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));
                if (arrayMode.isSlowPut()) {
                    // This is sort of strange. If we wanted to optimize this code path, we would invert
                    // the above branch. But it's simply not worth it since this only happens if we're
                    // already having a bad time.
                    slowCases.append(m_jit.jump());
                } else {
                    m_jit.add32(TrustedImm32(1), MacroAssembler::Address(storageReg, ArrayStorage::numValuesInVectorOffset()));
                
                    // If we're writing to a hole we might be growing the array; 
                    MacroAssembler::Jump lengthDoesNotNeedUpdate = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(storageReg, ArrayStorage::lengthOffset()));
                    m_jit.add32(TrustedImm32(1), propertyReg);
                    m_jit.store32(propertyReg, MacroAssembler::Address(storageReg, ArrayStorage::lengthOffset()));
                    m_jit.sub32(TrustedImm32(1), propertyReg);
                
                    lengthDoesNotNeedUpdate.link(&m_jit);
                }
                notHoleValue.link(&m_jit);
            }
    
            // Store the value to the array.
            m_jit.store32(valueTagReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.store32(valuePayloadReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));

            base.use();
            property.use();
            value.use();
            storage.use();
            
            if (!slowCases.empty()) {
                addSlowPathGenerator(
                    slowPathCall(
                        slowCases, this,
                        m_jit.codeBlock()->isStrictMode() ? operationPutByValBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsNonStrict,
                        NoResult, baseReg, propertyReg, valueTagReg, valuePayloadReg));
            }

            noResult(m_compileIndex, UseChildrenCalledExplicitly);
            break;
        }
            
        case Array::Arguments:
            // FIXME: we could at some point make this work. Right now we're assuming that the register
            // pressure would be too great.
            ASSERT_NOT_REACHED();
            break;
            
        case Array::Int8Array:
            compilePutByValForIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int8_t), SignedTypedArray);
            break;
            
        case Array::Int16Array:
            compilePutByValForIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int16_t), SignedTypedArray);
            break;
            
        case Array::Int32Array:
            compilePutByValForIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int32_t), SignedTypedArray);
            break;
            
        case Array::Uint8Array:
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), UnsignedTypedArray);
            break;
            
        case Array::Uint8ClampedArray:
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ClampedArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), UnsignedTypedArray, ClampRounding);
            break;
            
        case Array::Uint16Array:
            compilePutByValForIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint16_t), UnsignedTypedArray);
            break;
            
        case Array::Uint32Array:
            compilePutByValForIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint32_t), UnsignedTypedArray);
            break;
            
        case Array::Float32Array:
            compilePutByValForFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(float));
            break;
            
        case Array::Float64Array:
            compilePutByValForFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(double));
            break;
            
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case RegExpExec: {
        if (compileRegExpExec(node))
            return;

        if (!node.adjustedRefCount()) {
            SpeculateCellOperand base(this, node.child1());
            SpeculateCellOperand argument(this, node.child2());
            GPRReg baseGPR = base.gpr();
            GPRReg argumentGPR = argument.gpr();
            
            flushRegisters();
            GPRResult result(this);
            callOperation(operationRegExpTest, result.gpr(), baseGPR, argumentGPR);
            
            // Must use jsValueResult because otherwise we screw up register
            // allocation, which thinks that this node has a result.
            booleanResult(result.gpr(), m_compileIndex);
            break;
        }

        SpeculateCellOperand base(this, node.child1());
        SpeculateCellOperand argument(this, node.child2());
        GPRReg baseGPR = base.gpr();
        GPRReg argumentGPR = argument.gpr();
        
        flushRegisters();
        GPRResult2 resultTag(this);
        GPRResult resultPayload(this);
        callOperation(operationRegExpExec, resultTag.gpr(), resultPayload.gpr(), baseGPR, argumentGPR);
        
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }
        
    case RegExpTest: {
        SpeculateCellOperand base(this, node.child1());
        SpeculateCellOperand argument(this, node.child2());
        GPRReg baseGPR = base.gpr();
        GPRReg argumentGPR = argument.gpr();
        
        flushRegisters();
        GPRResult result(this);
        callOperation(operationRegExpTest, result.gpr(), baseGPR, argumentGPR);
        
        // If we add a DataFormatBool, we should use it here.
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }
        
    case ArrayPush: {
        ASSERT(node.arrayMode().isJSArray());
        
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary storageLength(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg storageLengthGPR = storageLength.gpr();
        
        StorageOperand storage(this, node.child3());
        GPRReg storageGPR = storage.gpr();
        
        switch (node.arrayMode().type()) {
        case Array::Int32: {
            SpeculateIntegerOperand value(this, node.child2());
            GPRReg valuePayloadGPR = value.gpr();
            
            m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
            m_jit.store32(TrustedImm32(JSValue::Int32Tag), MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.store32(valuePayloadGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.move(TrustedImm32(JSValue::Int32Tag), storageGPR);
            
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationArrayPush,
                    JSValueRegs(storageGPR, storageLengthGPR),
                    TrustedImm32(JSValue::Int32Tag), valuePayloadGPR, baseGPR));
        
            jsValueResult(storageGPR, storageLengthGPR, m_compileIndex);
            break;
        }
            
        case Array::Contiguous: {
            JSValueOperand value(this, node.child2());
            GPRReg valueTagGPR = value.tagGPR();
            GPRReg valuePayloadGPR = value.payloadGPR();

            if (Heap::isWriteBarrierEnabled()) {
                GPRTemporary scratch(this);
                writeBarrier(baseGPR, valueTagGPR, node.child2(), WriteBarrierForPropertyAccess, scratch.gpr(), storageLengthGPR);
            }

            m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
            m_jit.store32(valueTagGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.store32(valuePayloadGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.move(TrustedImm32(JSValue::Int32Tag), storageGPR);
            
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationArrayPush,
                    JSValueRegs(storageGPR, storageLengthGPR),
                    valueTagGPR, valuePayloadGPR, baseGPR));
        
            jsValueResult(storageGPR, storageLengthGPR, m_compileIndex);
            break;
        }
            
        case Array::Double: {
            SpeculateDoubleOperand value(this, node.child2());
            FPRReg valueFPR = value.fpr();

            if (!isRealNumberSpeculation(m_state.forNode(node.child2()).m_type)) {
                // FIXME: We need a way of profiling these, and we need to hoist them into
                // SpeculateDoubleOperand.
                speculationCheck(
                    BadType, JSValueRegs(), NoNode,
                    m_jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, valueFPR, valueFPR));
            }
            
            m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
            m_jit.storeDouble(valueFPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight));
            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.move(TrustedImm32(JSValue::Int32Tag), storageGPR);
            
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationArrayPushDouble,
                    JSValueRegs(storageGPR, storageLengthGPR),
                    valueFPR, baseGPR));
        
            jsValueResult(storageGPR, storageLengthGPR, m_compileIndex);
            break;
        }
            
        case Array::ArrayStorage: {
            JSValueOperand value(this, node.child2());
            GPRReg valueTagGPR = value.tagGPR();
            GPRReg valuePayloadGPR = value.payloadGPR();

            if (Heap::isWriteBarrierEnabled()) {
                GPRTemporary scratch(this);
                writeBarrier(baseGPR, valueTagGPR, node.child2(), WriteBarrierForPropertyAccess, scratch.gpr(), storageLengthGPR);
            }

            m_jit.load32(MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);
        
            // Refuse to handle bizarre lengths.
            speculationCheck(Uncountable, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Above, storageLengthGPR, TrustedImm32(0x7ffffffe)));
        
            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::vectorLengthOffset()));
        
            m_jit.store32(valueTagGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.store32(valuePayloadGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
        
            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()));
            m_jit.add32(TrustedImm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
            m_jit.move(TrustedImm32(JSValue::Int32Tag), storageGPR);
        
            addSlowPathGenerator(slowPathCall(slowPath, this, operationArrayPush, JSValueRegs(storageGPR, storageLengthGPR), valueTagGPR, valuePayloadGPR, baseGPR));
        
            jsValueResult(storageGPR, storageLengthGPR, m_compileIndex);
            break;
        }
            
        default:
            CRASH();
            break;
        }
        break;
    }
        
    case ArrayPop: {
        ASSERT(node.arrayMode().isJSArray());
        
        SpeculateCellOperand base(this, node.child1());
        StorageOperand storage(this, node.child2());
        GPRTemporary valueTag(this);
        GPRTemporary valuePayload(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueTagGPR = valueTag.gpr();
        GPRReg valuePayloadGPR = valuePayload.gpr();
        GPRReg storageGPR = storage.gpr();
        
        switch (node.arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous: {
            m_jit.load32(
                MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), valuePayloadGPR);
            MacroAssembler::Jump undefinedCase =
                m_jit.branchTest32(MacroAssembler::Zero, valuePayloadGPR);
            m_jit.sub32(TrustedImm32(1), valuePayloadGPR);
            m_jit.store32(
                valuePayloadGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.load32(
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)),
                valueTagGPR);
            MacroAssembler::Jump slowCase = m_jit.branch32(MacroAssembler::Equal, valueTagGPR, TrustedImm32(JSValue::EmptyValueTag));
            m_jit.store32(
                MacroAssembler::TrustedImm32(JSValue::EmptyValueTag),
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.load32(
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)),
                valuePayloadGPR);

            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    MacroAssembler::TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    MacroAssembler::TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPopAndRecoverLength,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), baseGPR));
            
            jsValueResult(valueTagGPR, valuePayloadGPR, m_compileIndex);
            break;
        }
            
        case Array::Double: {
            FPRTemporary temp(this);
            FPRReg tempFPR = temp.fpr();
            
            m_jit.load32(
                MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), valuePayloadGPR);
            MacroAssembler::Jump undefinedCase =
                m_jit.branchTest32(MacroAssembler::Zero, valuePayloadGPR);
            m_jit.sub32(TrustedImm32(1), valuePayloadGPR);
            m_jit.store32(
                valuePayloadGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.loadDouble(
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight),
                tempFPR);
            MacroAssembler::Jump slowCase = m_jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, tempFPR, tempFPR);
            JSValue nan = JSValue(JSValue::EncodeAsDouble, QNaN);
            m_jit.store32(
                MacroAssembler::TrustedImm32(nan.u.asBits.tag),
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.store32(
                MacroAssembler::TrustedImm32(nan.u.asBits.payload),
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
            boxDouble(tempFPR, valueTagGPR, valuePayloadGPR);

            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    MacroAssembler::TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    MacroAssembler::TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPopAndRecoverLength,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), baseGPR));
            
            jsValueResult(valueTagGPR, valuePayloadGPR, m_compileIndex);
            break;
        }

        case Array::ArrayStorage: {
            GPRTemporary storageLength(this);
            GPRReg storageLengthGPR = storageLength.gpr();

            m_jit.load32(MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);
        
            JITCompiler::JumpList setUndefinedCases;
            setUndefinedCases.append(m_jit.branchTest32(MacroAssembler::Zero, storageLengthGPR));
        
            m_jit.sub32(TrustedImm32(1), storageLengthGPR);
        
            MacroAssembler::Jump slowCase = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::vectorLengthOffset()));
        
            m_jit.load32(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), valueTagGPR);
            m_jit.load32(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), valuePayloadGPR);
        
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()));

            setUndefinedCases.append(m_jit.branch32(MacroAssembler::Equal, TrustedImm32(JSValue::EmptyValueTag), valueTagGPR));
        
            m_jit.store32(TrustedImm32(JSValue::EmptyValueTag), MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

            m_jit.sub32(TrustedImm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
            addSlowPathGenerator(
                slowPathMove(
                    setUndefinedCases, this,
                    MacroAssembler::TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    MacroAssembler::TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
        
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPop,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), baseGPR));

            jsValueResult(valueTagGPR, valuePayloadGPR, m_compileIndex);
            break;
        }
            
        default:
            CRASH();
            break;
        }
        break;
    }

    case DFG::Jump: {
        BlockIndex taken = node.takenBlockIndex();
        jump(taken);
        noResult(m_compileIndex);
        break;
    }

    case Branch:
        if (at(node.child1()).shouldSpeculateInteger()) {
            SpeculateIntegerOperand op(this, node.child1());
            
            BlockIndex taken = node.takenBlockIndex();
            BlockIndex notTaken = node.notTakenBlockIndex();
            
            MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;
            
            if (taken == nextBlock()) {
                condition = MacroAssembler::Zero;
                BlockIndex tmp = taken;
                taken = notTaken;
                notTaken = tmp;
            }
            
            branchTest32(condition, op.gpr(), taken);
            jump(notTaken);
            
            noResult(m_compileIndex);
            break;
        }
        emitBranch(node);
        break;

    case Return: {
        ASSERT(GPRInfo::callFrameRegister != GPRInfo::regT2);
        ASSERT(GPRInfo::regT1 != GPRInfo::returnValueGPR);
        ASSERT(GPRInfo::returnValueGPR != GPRInfo::callFrameRegister);

#if DFG_ENABLE(SUCCESS_STATS)
        static SamplingCounter counter("SpeculativeJIT");
        m_jit.emitCount(counter);
#endif

        // Return the result in returnValueGPR.
        JSValueOperand op1(this, node.child1());
        op1.fill();
        if (op1.isDouble())
            boxDouble(op1.fpr(), GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
        else {
            if (op1.payloadGPR() == GPRInfo::returnValueGPR2 && op1.tagGPR() == GPRInfo::returnValueGPR)
                m_jit.swap(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
            else if (op1.payloadGPR() == GPRInfo::returnValueGPR2) {
                m_jit.move(op1.payloadGPR(), GPRInfo::returnValueGPR);
                m_jit.move(op1.tagGPR(), GPRInfo::returnValueGPR2);
            } else {
                m_jit.move(op1.tagGPR(), GPRInfo::returnValueGPR2);
                m_jit.move(op1.payloadGPR(), GPRInfo::returnValueGPR);
            }
        }

        // Grab the return address.
        m_jit.emitGetFromCallFrameHeaderPtr(JSStack::ReturnPC, GPRInfo::regT2);
        // Restore our caller's "r".
        m_jit.emitGetFromCallFrameHeaderPtr(JSStack::CallerFrame, GPRInfo::callFrameRegister);
        // Return.
        m_jit.restoreReturnAddressBeforeReturn(GPRInfo::regT2);
        m_jit.ret();
        
        noResult(m_compileIndex);
        break;
    }
        
    case Throw:
    case ThrowReferenceError: {
        // We expect that throw statements are rare and are intended to exit the code block
        // anyway, so we just OSR back to the old JIT for now.
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
        break;
    }
        
    case ToPrimitive: {
        if (at(node.child1()).shouldSpeculateInteger()) {
            // It's really profitable to speculate integer, since it's really cheap,
            // it means we don't have to do any real work, and we emit a lot less code.
            
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);
            
            ASSERT(op1.format() == DataFormatInteger);
            m_jit.move(op1.gpr(), result.gpr());
            
            integerResult(result.gpr(), m_compileIndex);
            break;
        }
        
        // FIXME: Add string speculation here.
        
        JSValueOperand op1(this, node.child1());
        GPRTemporary resultTag(this, op1);
        GPRTemporary resultPayload(this, op1, false);
        
        GPRReg op1TagGPR = op1.tagGPR();
        GPRReg op1PayloadGPR = op1.payloadGPR();
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        
        op1.use();
        
        if (!(m_state.forNode(node.child1()).m_type & ~(SpecNumber | SpecBoolean))) {
            m_jit.move(op1TagGPR, resultTagGPR);
            m_jit.move(op1PayloadGPR, resultPayloadGPR);
        } else {
            MacroAssembler::Jump alreadyPrimitive = m_jit.branch32(MacroAssembler::NotEqual, op1TagGPR, TrustedImm32(JSValue::CellTag));
            MacroAssembler::Jump notPrimitive = m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op1PayloadGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get()));
            
            alreadyPrimitive.link(&m_jit);
            m_jit.move(op1TagGPR, resultTagGPR);
            m_jit.move(op1PayloadGPR, resultPayloadGPR);
            
            addSlowPathGenerator(
                slowPathCall(
                    notPrimitive, this, operationToPrimitive,
                    JSValueRegs(resultTagGPR, resultPayloadGPR), op1TagGPR, op1PayloadGPR));
        }
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }
        
    case StrCat: {
        size_t scratchSize = sizeof(EncodedJSValue) * node.numChildren();
        ScratchBuffer* scratchBuffer = m_jit.globalData()->scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;
        
        for (unsigned operandIdx = 0; operandIdx < node.numChildren(); ++operandIdx) {
            JSValueOperand operand(this, m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx]);
            GPRReg opTagGPR = operand.tagGPR();
            GPRReg opPayloadGPR = operand.payloadGPR();
            operand.use();
            
            m_jit.store32(opTagGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
            m_jit.store32(opPayloadGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
        }
        
        flushRegisters();

        if (scratchSize) {
            GPRTemporary scratch(this);

            // Tell GC mark phase how much of the scratch buffer is active during call.
            m_jit.move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratch.gpr());
            m_jit.storePtr(TrustedImmPtr(scratchSize), scratch.gpr());
        }

        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        
        callOperation(operationStrCat, resultTag.gpr(), resultPayload.gpr(), static_cast<void *>(buffer), node.numChildren());

        if (scratchSize) {
            GPRTemporary scratch(this);

            m_jit.move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratch.gpr());
            m_jit.storePtr(TrustedImmPtr(0), scratch.gpr());
        }

        // FIXME: make the callOperation above explicitly return a cell result, or jitAssert the tag is a cell tag.
        cellResult(resultPayload.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case NewArray: {
        JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node.codeOrigin);
        if (!globalObject->isHavingABadTime() && !hasArrayStorage(node.indexingType())) {
            globalObject->havingABadTimeWatchpoint()->add(speculationWatchpoint());
            
            Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType());
            ASSERT(structure->indexingType() == node.indexingType());
            ASSERT(
                hasUndecided(structure->indexingType())
                || hasInt32(structure->indexingType())
                || hasDouble(structure->indexingType())
                || hasContiguous(structure->indexingType()));

            unsigned numElements = node.numChildren();
            
            GPRTemporary result(this);
            GPRTemporary storage(this);
            
            GPRReg resultGPR = result.gpr();
            GPRReg storageGPR = storage.gpr();

            emitAllocateJSArray(structure, resultGPR, storageGPR, numElements);
            
            // At this point, one way or another, resultGPR and storageGPR have pointers to
            // the JSArray and the Butterfly, respectively.
            
            ASSERT(!hasUndecided(structure->indexingType()) || !node.numChildren());
            
            for (unsigned operandIdx = 0; operandIdx < node.numChildren(); ++operandIdx) {
                Edge use = m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx];
                switch (node.indexingType()) {
                case ALL_BLANK_INDEXING_TYPES:
                case ALL_UNDECIDED_INDEXING_TYPES:
                    CRASH();
                    break;
                case ALL_DOUBLE_INDEXING_TYPES: {
                    SpeculateDoubleOperand operand(this, use);
                    FPRReg opFPR = operand.fpr();
                    if (!isRealNumberSpeculation(m_state.forNode(use).m_type)) {
                        // FIXME: We need a way of profiling these, and we need to hoist them into
                        // SpeculateDoubleOperand.
                        speculationCheck(
                            BadType, JSValueRegs(), NoNode,
                            m_jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, opFPR, opFPR));
                    }
        
                    m_jit.storeDouble(opFPR, MacroAssembler::Address(storageGPR, sizeof(double) * operandIdx));
                    break;
                }
                case ALL_INT32_INDEXING_TYPES: {
                    SpeculateIntegerOperand operand(this, use);
                    m_jit.store32(TrustedImm32(JSValue::Int32Tag), MacroAssembler::Address(storageGPR, sizeof(JSValue) * operandIdx + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
                    m_jit.store32(operand.gpr(), MacroAssembler::Address(storageGPR, sizeof(JSValue) * operandIdx + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
                    break;
                }
                case ALL_CONTIGUOUS_INDEXING_TYPES: {
                    JSValueOperand operand(this, m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx]);
                    GPRReg opTagGPR = operand.tagGPR();
                    GPRReg opPayloadGPR = operand.payloadGPR();
                    m_jit.store32(opTagGPR, MacroAssembler::Address(storageGPR, sizeof(JSValue) * operandIdx + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
                    m_jit.store32(opPayloadGPR, MacroAssembler::Address(storageGPR, sizeof(JSValue) * operandIdx + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
                    break;
                }
                default:
                    CRASH();
                    break;
                }
            }
            
            // Yuck, we should *really* have a way of also returning the storageGPR. But
            // that's the least of what's wrong with this code. We really shouldn't be
            // allocating the array after having computed - and probably spilled to the
            // stack - all of the things that will go into the array. The solution to that
            // bigger problem will also likely fix the redundancy in reloading the storage
            // pointer that we currently have.
            
            cellResult(resultGPR, m_compileIndex);
            break;
        }
        
        if (!node.numChildren()) {
            flushRegisters();
            GPRResult result(this);
            callOperation(
                operationNewEmptyArray, result.gpr(), globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType()));
            cellResult(result.gpr(), m_compileIndex);
            break;
        }
        
        size_t scratchSize = sizeof(EncodedJSValue) * node.numChildren();
        ScratchBuffer* scratchBuffer = m_jit.globalData()->scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;
        
        for (unsigned operandIdx = 0; operandIdx < node.numChildren(); ++operandIdx) {
            // Need to perform the speculations that this node promises to perform. If we're
            // emitting code here and the indexing type is not array storage then there is
            // probably something hilarious going on and we're already failing at all the
            // things, but at least we're going to be sound.
            Edge use = m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx];
            switch (node.indexingType()) {
            case ALL_BLANK_INDEXING_TYPES:
            case ALL_UNDECIDED_INDEXING_TYPES:
                CRASH();
                break;
            case ALL_DOUBLE_INDEXING_TYPES: {
                SpeculateDoubleOperand operand(this, use);
                FPRReg opFPR = operand.fpr();
                if (!isRealNumberSpeculation(m_state.forNode(use).m_type)) {
                    // FIXME: We need a way of profiling these, and we need to hoist them into
                    // SpeculateDoubleOperand.
                    speculationCheck(
                        BadType, JSValueRegs(), NoNode,
                        m_jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, opFPR, opFPR));
                }
                
                m_jit.storeDouble(opFPR, reinterpret_cast<char*>(buffer + operandIdx));
                break;
            }
            case ALL_INT32_INDEXING_TYPES: {
                SpeculateIntegerOperand operand(this, use);
                GPRReg opGPR = operand.gpr();
                m_jit.store32(TrustedImm32(JSValue::Int32Tag), reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
                m_jit.store32(opGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
                break;
            }
            case ALL_CONTIGUOUS_INDEXING_TYPES:
            case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
                JSValueOperand operand(this, m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx]);
                GPRReg opTagGPR = operand.tagGPR();
                GPRReg opPayloadGPR = operand.payloadGPR();
                
                m_jit.store32(opTagGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
                m_jit.store32(opPayloadGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
                operand.use();
                break;
            }
            default:
                CRASH();
                break;
            }
        }
        
        switch (node.indexingType()) {
        case ALL_DOUBLE_INDEXING_TYPES:
        case ALL_INT32_INDEXING_TYPES:
            useChildren(node);
            break;
        default:
            break;
        }
        
        flushRegisters();

        if (scratchSize) {
            GPRTemporary scratch(this);

            // Tell GC mark phase how much of the scratch buffer is active during call.
            m_jit.move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratch.gpr());
            m_jit.storePtr(TrustedImmPtr(scratchSize), scratch.gpr());
        }

        GPRResult result(this);
        
        callOperation(
            operationNewArray, result.gpr(), globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType()),
            static_cast<void*>(buffer), node.numChildren());

        if (scratchSize) {
            GPRTemporary scratch(this);

            m_jit.move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratch.gpr());
            m_jit.storePtr(TrustedImmPtr(0), scratch.gpr());
        }

        cellResult(result.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case NewArrayWithSize: {
        JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node.codeOrigin);
        if (!globalObject->isHavingABadTime()) {
            globalObject->havingABadTimeWatchpoint()->add(speculationWatchpoint());
            
            SpeculateStrictInt32Operand size(this, node.child1());
            GPRTemporary result(this);
            GPRTemporary storage(this);
            GPRTemporary scratch(this);
            
            GPRReg sizeGPR = size.gpr();
            GPRReg resultGPR = result.gpr();
            GPRReg storageGPR = storage.gpr();
            GPRReg scratchGPR = scratch.gpr();
            
            MacroAssembler::JumpList slowCases;
            slowCases.append(m_jit.branch32(MacroAssembler::AboveOrEqual, sizeGPR, TrustedImm32(MIN_SPARSE_ARRAY_INDEX)));
            
            ASSERT((1 << 3) == sizeof(JSValue));
            m_jit.move(sizeGPR, scratchGPR);
            m_jit.lshift32(TrustedImm32(3), scratchGPR);
            m_jit.add32(TrustedImm32(sizeof(IndexingHeader)), scratchGPR, resultGPR);
            slowCases.append(
                emitAllocateBasicStorage(resultGPR, storageGPR));
            m_jit.subPtr(scratchGPR, storageGPR);
            emitAllocateBasicJSObject<JSArray, MarkedBlock::None>(
                TrustedImmPtr(globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType())), resultGPR, scratchGPR,
                storageGPR, sizeof(JSArray), slowCases);
            
            m_jit.store32(sizeGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.store32(sizeGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
            
            if (hasDouble(node.indexingType())) {
                JSValue nan = JSValue(JSValue::EncodeAsDouble, QNaN);
                
                m_jit.move(sizeGPR, scratchGPR);
                MacroAssembler::Jump done = m_jit.branchTest32(MacroAssembler::Zero, scratchGPR);
                MacroAssembler::Label loop = m_jit.label();
                m_jit.sub32(TrustedImm32(1), scratchGPR);
                m_jit.store32(TrustedImm32(nan.u.asBits.tag), MacroAssembler::BaseIndex(storageGPR, scratchGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
                m_jit.store32(TrustedImm32(nan.u.asBits.payload), MacroAssembler::BaseIndex(storageGPR, scratchGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
                m_jit.branchTest32(MacroAssembler::NonZero, scratchGPR).linkTo(loop, &m_jit);
                done.link(&m_jit);
            }
            
            addSlowPathGenerator(adoptPtr(
                new CallArrayAllocatorWithVariableSizeSlowPathGenerator(
                    slowCases, this, operationNewArrayWithSize, resultGPR,
                    globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType()),
                    globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage),
                    sizeGPR)));
            
            cellResult(resultGPR, m_compileIndex);
            break;
        }
        
        SpeculateStrictInt32Operand size(this, node.child1());
        GPRReg sizeGPR = size.gpr();
        flushRegisters();
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
        GPRReg structureGPR = selectScratchGPR(sizeGPR);
        MacroAssembler::Jump bigLength = m_jit.branch32(MacroAssembler::AboveOrEqual, sizeGPR, TrustedImm32(MIN_SPARSE_ARRAY_INDEX));
        m_jit.move(TrustedImmPtr(globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType())), structureGPR);
        MacroAssembler::Jump done = m_jit.jump();
        bigLength.link(&m_jit);
        m_jit.move(TrustedImmPtr(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage)), structureGPR);
        done.link(&m_jit);
        callOperation(
            operationNewArrayWithSize, resultGPR, structureGPR, sizeGPR);
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case NewArrayBuffer: {
        JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node.codeOrigin);
        IndexingType indexingType = node.indexingType();
        if (!globalObject->isHavingABadTime() && !hasArrayStorage(indexingType)) {
            globalObject->havingABadTimeWatchpoint()->add(speculationWatchpoint());
            
            unsigned numElements = node.numConstants();
            
            GPRTemporary result(this);
            GPRTemporary storage(this);
            
            GPRReg resultGPR = result.gpr();
            GPRReg storageGPR = storage.gpr();

            emitAllocateJSArray(globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType), resultGPR, storageGPR, numElements);
            
            if (node.indexingType() == ArrayWithDouble) {
                JSValue* data = m_jit.codeBlock()->constantBuffer(node.startConstant());
                for (unsigned index = 0; index < node.numConstants(); ++index) {
                    union {
                        int32_t halves[2];
                        double value;
                    } u;
                    u.value = data[index].asNumber();
                    m_jit.store32(Imm32(u.halves[0]), MacroAssembler::Address(storageGPR, sizeof(double) * index));
                    m_jit.store32(Imm32(u.halves[1]), MacroAssembler::Address(storageGPR, sizeof(double) * index + sizeof(int32_t)));
                }
            } else {
                int32_t* data = bitwise_cast<int32_t*>(m_jit.codeBlock()->constantBuffer(node.startConstant()));
                for (unsigned index = 0; index < node.numConstants() * 2; ++index) {
                    m_jit.store32(
                        Imm32(data[index]), MacroAssembler::Address(storageGPR, sizeof(int32_t) * index));
                }
            }
            
            cellResult(resultGPR, m_compileIndex);
            break;
        }
        
        flushRegisters();
        GPRResult result(this);
        
        callOperation(operationNewArrayBuffer, result.gpr(), globalObject->arrayStructureForIndexingTypeDuringAllocation(node.indexingType()), node.startConstant(), node.numConstants());
        
        cellResult(result.gpr(), m_compileIndex);
        break;
    }
        
    case NewRegexp: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        
        callOperation(operationNewRegexp, resultTag.gpr(), resultPayload.gpr(), m_jit.codeBlock()->regexp(node.regexpIndex()));
        
        // FIXME: make the callOperation above explicitly return a cell result, or jitAssert the tag is a cell tag.
        cellResult(resultPayload.gpr(), m_compileIndex);
        break;
    }
        
    case ConvertThis: {
        if (isObjectSpeculation(m_state.forNode(node.child1()).m_type)) {
            SpeculateCellOperand thisValue(this, node.child1());
            GPRTemporary result(this, thisValue);
            m_jit.move(thisValue.gpr(), result.gpr());
            cellResult(result.gpr(), m_compileIndex);
            break;
        }
        
        if (isOtherSpeculation(at(node.child1()).prediction())) {
            JSValueOperand thisValue(this, node.child1());
            GPRTemporary scratch(this);
            
            GPRReg thisValueTagGPR = thisValue.tagGPR();
            GPRReg scratchGPR = scratch.gpr();
            
            COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
            m_jit.move(thisValueTagGPR, scratchGPR);
            m_jit.or32(TrustedImm32(1), scratchGPR);
            // This is hard. It would be better to save the value, but we can't quite do it,
            // since this operation does not otherwise get the payload.
            speculationCheck(BadType, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, scratchGPR, TrustedImm32(JSValue::NullTag)));
            
            m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.globalThisObjectFor(node.codeOrigin)), scratchGPR);
            cellResult(scratchGPR, m_compileIndex);
            break;
        }
        
        if (isObjectSpeculation(at(node.child1()).prediction())) {
            SpeculateCellOperand thisValue(this, node.child1());
            GPRReg thisValueGPR = thisValue.gpr();
            
            if (!isObjectSpeculation(m_state.forNode(node.child1()).m_type))
                speculationCheck(BadType, JSValueSource::unboxedCell(thisValueGPR), node.child1(), m_jit.branchPtr(JITCompiler::Equal, JITCompiler::Address(thisValueGPR, JSCell::structureOffset()), JITCompiler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
            
            GPRTemporary result(this, thisValue);
            GPRReg resultGPR = result.gpr();
            m_jit.move(thisValueGPR, resultGPR);
            cellResult(resultGPR, m_compileIndex);
            break;
        }
        
        JSValueOperand thisValue(this, node.child1());
        GPRReg thisValueTagGPR = thisValue.tagGPR();
        GPRReg thisValuePayloadGPR = thisValue.payloadGPR();
        
        flushRegisters();
        
        GPRResult2 resultTag(this);
        GPRResult resultPayload(this);
        callOperation(operationConvertThis, resultTag.gpr(), resultPayload.gpr(), thisValueTagGPR, thisValuePayloadGPR);
        
        cellResult(resultPayload.gpr(), m_compileIndex);
        break;
    }

    case CreateThis: {
        // Note that there is not so much profit to speculate here. The only things we
        // speculate on are (1) that it's a cell, since that eliminates cell checks
        // later if the proto is reused, and (2) if we have a FinalObject prediction
        // then we speculate because we want to get recompiled if it isn't (since
        // otherwise we'd start taking slow path a lot).
        
        SpeculateCellOperand callee(this, node.child1());
        GPRTemporary result(this);
        GPRTemporary structure(this);
        GPRTemporary scratch(this);
        
        GPRReg calleeGPR = callee.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg structureGPR = structure.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        // Load the inheritorID. If the inheritorID is not set, go to slow path.
        m_jit.loadPtr(MacroAssembler::Address(calleeGPR, JSFunction::offsetOfCachedInheritorID()), structureGPR);
        MacroAssembler::JumpList slowPath;
        slowPath.append(m_jit.branchTestPtr(MacroAssembler::Zero, structureGPR));
        
        emitAllocateJSFinalObject(structureGPR, resultGPR, scratchGPR, slowPath);
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationCreateThis, resultGPR, calleeGPR));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }

    case InheritorIDWatchpoint: {
        jsCast<JSFunction*>(node.function())->addInheritorIDWatchpoint(speculationWatchpoint());
        noResult(m_compileIndex);
        break;
    }

    case NewObject: {
        GPRTemporary result(this);
        GPRTemporary scratch(this);
        
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        MacroAssembler::JumpList slowPath;
        
        emitAllocateJSFinalObject(MacroAssembler::TrustedImmPtr(node.structure()), resultGPR, scratchGPR, slowPath);
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewObject, resultGPR, node.structure()));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }

    case GetCallee: {
        GPRTemporary result(this);
        m_jit.loadPtr(JITCompiler::payloadFor(static_cast<VirtualRegister>(JSStack::Callee)), result.gpr());
        cellResult(result.gpr(), m_compileIndex);
        break;
    }
        
    case GetMyScope: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        m_jit.loadPtr(JITCompiler::payloadFor(static_cast<VirtualRegister>(JSStack::ScopeChain)), resultGPR);
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case SkipTopScope: {
        SpeculateCellOperand scope(this, node.child1());
        GPRTemporary result(this, scope);
        GPRReg resultGPR = result.gpr();
        m_jit.move(scope.gpr(), resultGPR);
        JITCompiler::Jump activationNotCreated =
            m_jit.branchTestPtr(
                JITCompiler::Zero,
                JITCompiler::payloadFor(
                    static_cast<VirtualRegister>(m_jit.codeBlock()->activationRegister())));
        m_jit.loadPtr(JITCompiler::Address(resultGPR, JSScope::offsetOfNext()), resultGPR);
        activationNotCreated.link(&m_jit);
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case SkipScope: {
        SpeculateCellOperand scope(this, node.child1());
        GPRTemporary result(this, scope);
        m_jit.loadPtr(JITCompiler::Address(scope.gpr(), JSScope::offsetOfNext()), result.gpr());
        cellResult(result.gpr(), m_compileIndex);
        break;
    }
        
    case GetScopeRegisters: {
        SpeculateCellOperand scope(this, node.child1());
        GPRTemporary result(this);
        GPRReg scopeGPR = scope.gpr();
        GPRReg resultGPR = result.gpr();

        m_jit.loadPtr(JITCompiler::Address(scopeGPR, JSVariableObject::offsetOfRegisters()), resultGPR);
        storageResult(resultGPR, m_compileIndex);
        break;
    }
    case GetScopedVar: {
        StorageOperand registers(this, node.child1());
        GPRTemporary resultTag(this);
        GPRTemporary resultPayload(this);
        GPRReg registersGPR = registers.gpr();
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        m_jit.load32(JITCompiler::Address(registersGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
        m_jit.load32(JITCompiler::Address(registersGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }
    case PutScopedVar: {
        SpeculateCellOperand scope(this, node.child1());
        StorageOperand registers(this, node.child2());
        JSValueOperand value(this, node.child3());
        GPRTemporary scratchRegister(this);
        GPRReg scopeGPR = scope.gpr();
        GPRReg registersGPR = registers.gpr();
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg scratchGPR = scratchRegister.gpr();

        m_jit.store32(valueTagGPR, JITCompiler::Address(registersGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
        m_jit.store32(valuePayloadGPR, JITCompiler::Address(registersGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        writeBarrier(scopeGPR, valueTagGPR, node.child2(), WriteBarrierForVariableAccess, scratchGPR);
        noResult(m_compileIndex);
        break;
    }
        
    case GetById: {
        if (!node.prediction()) {
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        }
        
        if (isCellSpeculation(at(node.child1()).prediction())) {
            SpeculateCellOperand base(this, node.child1());
            GPRTemporary resultTag(this, base);
            GPRTemporary resultPayload(this);
            
            GPRReg baseGPR = base.gpr();
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();

            base.use();
            
            cachedGetById(node.codeOrigin, InvalidGPRReg, baseGPR, resultTagGPR, resultPayloadGPR, node.identifierNumber());
            
            jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
            break;
        }
        
        JSValueOperand base(this, node.child1());
        GPRTemporary resultTag(this, base);
        GPRTemporary resultPayload(this);
        
        GPRReg baseTagGPR = base.tagGPR();
        GPRReg basePayloadGPR = base.payloadGPR();
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        
        base.use();
        
        JITCompiler::Jump notCell = m_jit.branch32(JITCompiler::NotEqual, baseTagGPR, TrustedImm32(JSValue::CellTag));
        
        cachedGetById(node.codeOrigin, baseTagGPR, basePayloadGPR, resultTagGPR, resultPayloadGPR, node.identifierNumber(), notCell);
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetByIdFlush: {
        if (!node.prediction()) {
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        }
        
        if (isCellSpeculation(at(node.child1()).prediction())) {
            SpeculateCellOperand base(this, node.child1());
            
            GPRReg baseGPR = base.gpr();

            GPRResult resultTag(this);
            GPRResult2 resultPayload(this);
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();

            base.use();
            
            flushRegisters();
            
            cachedGetById(node.codeOrigin, InvalidGPRReg, baseGPR, resultTagGPR, resultPayloadGPR, node.identifierNumber(), JITCompiler::Jump(), DontSpill);
            
            jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
            break;
        }
        
        JSValueOperand base(this, node.child1());
        GPRReg baseTagGPR = base.tagGPR();
        GPRReg basePayloadGPR = base.payloadGPR();

        GPRResult resultTag(this);
        GPRResult2 resultPayload(this);
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();

        base.use();
        
        flushRegisters();
        
        JITCompiler::Jump notCell = m_jit.branch32(JITCompiler::NotEqual, baseTagGPR, TrustedImm32(JSValue::CellTag));
        
        cachedGetById(node.codeOrigin, baseTagGPR, basePayloadGPR, resultTagGPR, resultPayloadGPR, node.identifierNumber(), notCell, DontSpill);
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetArrayLength:
        compileGetArrayLength(node);
        break;
        
    case CheckFunction: {
        SpeculateCellOperand function(this, node.child1());
        speculationCheck(BadCache, JSValueSource::unboxedCell(function.gpr()), node.child1(), m_jit.branchWeakPtr(JITCompiler::NotEqual, function.gpr(), node.function()));
        noResult(m_compileIndex);
        break;
    }

    case CheckStructure:
    case ForwardCheckStructure: {
        AbstractValue& value = m_state.forNode(node.child1());
        if (value.m_currentKnownStructure.isSubsetOf(node.structureSet())
            && isCellSpeculation(value.m_type)) {
            noResult(m_compileIndex);
            break;
        }
        
        SpeculationDirection direction = node.op() == ForwardCheckStructure ? ForwardSpeculation : BackwardSpeculation;
        SpeculateCellOperand base(this, node.child1(), direction);
        
        ASSERT(node.structureSet().size());
        
        if (node.structureSet().size() == 1) {
            speculationCheck(
                BadCache, JSValueSource::unboxedCell(base.gpr()), NoNode,
                m_jit.branchWeakPtr(
                    JITCompiler::NotEqual,
                    JITCompiler::Address(base.gpr(), JSCell::structureOffset()),
                    node.structureSet()[0]),
                direction);
        } else {
            GPRTemporary structure(this);
            
            m_jit.loadPtr(JITCompiler::Address(base.gpr(), JSCell::structureOffset()), structure.gpr());
            
            JITCompiler::JumpList done;
            
            for (size_t i = 0; i < node.structureSet().size() - 1; ++i)
                done.append(m_jit.branchWeakPtr(JITCompiler::Equal, structure.gpr(), node.structureSet()[i]));
            
            speculationCheck(
                BadCache, JSValueSource::unboxedCell(base.gpr()), NoNode,
                m_jit.branchWeakPtr(
                    JITCompiler::NotEqual, structure.gpr(), node.structureSet().last()),
                direction);
            
            done.link(&m_jit);
        }
        
        noResult(m_compileIndex);
        break;
    }
        
    case StructureTransitionWatchpoint:
    case ForwardStructureTransitionWatchpoint: {
        // There is a fascinating question here of what to do about array profiling.
        // We *could* try to tell the OSR exit about where the base of the access is.
        // The DFG will have kept it alive, though it may not be in a register, and
        // we shouldn't really load it since that could be a waste. For now though,
        // we'll just rely on the fact that when a watchpoint fires then that's
        // quite a hint already.
        
        SpeculationDirection direction = node.op() == ForwardStructureTransitionWatchpoint ? ForwardSpeculation : BackwardSpeculation;

        m_jit.addWeakReference(node.structure());
        node.structure()->addTransitionWatchpoint(
            speculationWatchpoint(
                m_jit.graph()[node.child1()].op() == WeakJSConstant ? BadWeakConstantCache : BadCache,
                direction));
        
#if !ASSERT_DISABLED
        SpeculateCellOperand op1(this, node.child1(), direction);
        JITCompiler::Jump isOK = m_jit.branchPtr(JITCompiler::Equal, JITCompiler::Address(op1.gpr(), JSCell::structureOffset()), TrustedImmPtr(node.structure()));
        m_jit.breakpoint();
        isOK.link(&m_jit);
#endif

        noResult(m_compileIndex);
        break;
    }
        
    case PhantomPutStructure: {
        ASSERT(node.structureTransitionData().previousStructure->transitionWatchpointSetHasBeenInvalidated());
        m_jit.addWeakReferenceTransition(
            node.codeOrigin.codeOriginOwner(),
            node.structureTransitionData().previousStructure,
            node.structureTransitionData().newStructure);
        noResult(m_compileIndex);
        break;
    }

    case PutStructure: {
        ASSERT(node.structureTransitionData().previousStructure->transitionWatchpointSetHasBeenInvalidated());

        SpeculateCellOperand base(this, node.child1());
        GPRReg baseGPR = base.gpr();
        
        m_jit.addWeakReferenceTransition(
            node.codeOrigin.codeOriginOwner(),
            node.structureTransitionData().previousStructure,
            node.structureTransitionData().newStructure);
        
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
        // Must always emit this write barrier as the structure transition itself requires it
        writeBarrier(baseGPR, node.structureTransitionData().newStructure, WriteBarrierForGenericAccess);
#endif
        
        m_jit.storePtr(MacroAssembler::TrustedImmPtr(node.structureTransitionData().newStructure), MacroAssembler::Address(baseGPR, JSCell::structureOffset()));
        
        noResult(m_compileIndex);
        break;
    }
        
    case AllocatePropertyStorage:
        compileAllocatePropertyStorage(node);
        break;
        
    case ReallocatePropertyStorage:
        compileReallocatePropertyStorage(node);
        break;
        
    case GetButterfly: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this, base);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.loadPtr(JITCompiler::Address(baseGPR, JSObject::butterflyOffset()), resultGPR);
        
        storageResult(resultGPR, m_compileIndex);
        break;
    }

    case GetIndexedPropertyStorage: {
        compileGetIndexedPropertyStorage(node);
        break;
    }

    case GetByOffset: {
        StorageOperand storage(this, node.child1());
        GPRTemporary resultTag(this, storage);
        GPRTemporary resultPayload(this);
        
        GPRReg storageGPR = storage.gpr();
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        
        StorageAccessData& storageAccessData = m_jit.graph().m_storageAccessData[node.storageAccessDataIndex()];
        
        m_jit.load32(JITCompiler::Address(storageGPR, storageAccessData.offset * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
        m_jit.load32(JITCompiler::Address(storageGPR, storageAccessData.offset * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }
        
    case PutByOffset: {
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
        SpeculateCellOperand base(this, node.child2());
#endif
        StorageOperand storage(this, node.child1());
        JSValueOperand value(this, node.child3());

        GPRReg storageGPR = storage.gpr();
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
        writeBarrier(base.gpr(), valueTagGPR, node.child3(), WriteBarrierForPropertyAccess);
#endif

        StorageAccessData& storageAccessData = m_jit.graph().m_storageAccessData[node.storageAccessDataIndex()];
        
        m_jit.storePtr(valueTagGPR, JITCompiler::Address(storageGPR, storageAccessData.offset * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
        m_jit.storePtr(valuePayloadGPR, JITCompiler::Address(storageGPR, storageAccessData.offset * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        
        noResult(m_compileIndex);
        break;
    }
        
    case PutById: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg scratchGPR = scratch.gpr();
        
        base.use();
        value.use();

        cachedPutById(node.codeOrigin, baseGPR, valueTagGPR, valuePayloadGPR, node.child2(), scratchGPR, node.identifierNumber(), NotDirect);
        
        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByIdDirect: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg scratchGPR = scratch.gpr();
        
        base.use();
        value.use();

        cachedPutById(node.codeOrigin, baseGPR, valueTagGPR, valuePayloadGPR, node.child2(), scratchGPR, node.identifierNumber(), Direct);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetGlobalVar: {
        GPRTemporary resultPayload(this);
        GPRTemporary resultTag(this);

        m_jit.move(TrustedImmPtr(node.registerPointer()), resultPayload.gpr());
        m_jit.load32(JITCompiler::Address(resultPayload.gpr(), OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTag.gpr());
        m_jit.load32(JITCompiler::Address(resultPayload.gpr(), OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayload.gpr());

        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case PutGlobalVar: {
        JSValueOperand value(this, node.child1());
        if (Heap::isWriteBarrierEnabled()) {
            GPRTemporary scratch(this);
            GPRReg scratchReg = scratch.gpr();
            
            writeBarrier(m_jit.globalObjectFor(node.codeOrigin), value.tagGPR(), node.child1(), WriteBarrierForVariableAccess, scratchReg);
        }

        // FIXME: if we happen to have a spare register - and _ONLY_ if we happen to have
        // a spare register - a good optimization would be to put the register pointer into
        // a register and then do a zero offset store followed by a four-offset store (or
        // vice-versa depending on endianness).
        m_jit.store32(value.tagGPR(), node.registerPointer()->tagPointer());
        m_jit.store32(value.payloadGPR(), node.registerPointer()->payloadPointer());

        noResult(m_compileIndex);
        break;
    }

    case PutGlobalVarCheck: {
        JSValueOperand value(this, node.child1());
        
        WatchpointSet* watchpointSet =
            m_jit.globalObjectFor(node.codeOrigin)->symbolTable()->get(
                identifier(node.identifierNumberForCheck())->impl()).watchpointSet();
        addSlowPathGenerator(
            slowPathCall(
                m_jit.branchTest8(
                    JITCompiler::NonZero,
                    JITCompiler::AbsoluteAddress(watchpointSet->addressOfIsWatched())),
                this, operationNotifyGlobalVarWrite, NoResult, watchpointSet));
        
        if (Heap::isWriteBarrierEnabled()) {
            GPRTemporary scratch(this);
            GPRReg scratchReg = scratch.gpr();
            
            writeBarrier(m_jit.globalObjectFor(node.codeOrigin), value.tagGPR(), node.child1(), WriteBarrierForVariableAccess, scratchReg);
        }

        // FIXME: if we happen to have a spare register - and _ONLY_ if we happen to have
        // a spare register - a good optimization would be to put the register pointer into
        // a register and then do a zero offset store followed by a four-offset store (or
        // vice-versa depending on endianness).
        m_jit.store32(value.tagGPR(), node.registerPointer()->tagPointer());
        m_jit.store32(value.payloadGPR(), node.registerPointer()->payloadPointer());

        noResult(m_compileIndex);
        break;
    }
        
    case GlobalVarWatchpoint: {
        m_jit.globalObjectFor(node.codeOrigin)->symbolTable()->get(
            identifier(node.identifierNumberForCheck())->impl()).addWatchpoint(
                speculationWatchpoint());
        
#if DFG_ENABLE(JIT_ASSERT)
        GPRTemporary scratch(this);
        GPRReg scratchGPR = scratch.gpr();
        m_jit.load32(node.registerPointer()->tagPointer(), scratchGPR);
        JITCompiler::Jump notOK = m_jit.branch32(
            JITCompiler::NotEqual, scratchGPR,
            TrustedImm32(node.registerPointer()->get().tag()));
        m_jit.load32(node.registerPointer()->payloadPointer(), scratchGPR);
        JITCompiler::Jump ok = m_jit.branch32(
            JITCompiler::Equal, scratchGPR,
            TrustedImm32(node.registerPointer()->get().payload()));
        notOK.link(&m_jit);
        m_jit.breakpoint();
        ok.link(&m_jit);
#endif
        
        noResult(m_compileIndex);
        break;
    }

    case CheckHasInstance: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary structure(this);

        // Speculate that base 'ImplementsDefaultHasInstance'.
        m_jit.loadPtr(MacroAssembler::Address(base.gpr(), JSCell::structureOffset()), structure.gpr());
        speculationCheck(Uncountable, JSValueRegs(), NoNode, m_jit.branchTest8(MacroAssembler::Zero, MacroAssembler::Address(structure.gpr(), Structure::typeInfoFlagsOffset()), MacroAssembler::TrustedImm32(ImplementsDefaultHasInstance)));

        noResult(m_compileIndex);
        break;
    }

    case InstanceOf: {
        compileInstanceOf(node);
        break;
    }

    case IsUndefined: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this);
        
        JITCompiler::Jump isCell = m_jit.branch32(JITCompiler::Equal, value.tagGPR(), JITCompiler::TrustedImm32(JSValue::CellTag));
        
        m_jit.compare32(JITCompiler::Equal, value.tagGPR(), TrustedImm32(JSValue::UndefinedTag), result.gpr());
        JITCompiler::Jump done = m_jit.jump();
        
        isCell.link(&m_jit);
        JITCompiler::Jump notMasqueradesAsUndefined;
        if (m_jit.graph().globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
            m_jit.graph().globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->add(speculationWatchpoint());
            m_jit.move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = m_jit.jump();
        } else {
            m_jit.loadPtr(JITCompiler::Address(value.payloadGPR(), JSCell::structureOffset()), result.gpr());
            JITCompiler::Jump isMasqueradesAsUndefined = m_jit.branchTest8(JITCompiler::NonZero, JITCompiler::Address(result.gpr(), Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
            m_jit.move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = m_jit.jump();
            
            isMasqueradesAsUndefined.link(&m_jit);
            GPRTemporary localGlobalObject(this);
            GPRTemporary remoteGlobalObject(this);
            GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
            GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();
            m_jit.move(TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), localGlobalObjectGPR);
            m_jit.loadPtr(JITCompiler::Address(result.gpr(), Structure::globalObjectOffset()), remoteGlobalObjectGPR); 
            m_jit.compare32(JITCompiler::Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, result.gpr());
        }

        notMasqueradesAsUndefined.link(&m_jit);
        done.link(&m_jit);
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }

    case IsBoolean: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        m_jit.compare32(JITCompiler::Equal, value.tagGPR(), JITCompiler::TrustedImm32(JSValue::BooleanTag), result.gpr());
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }

    case IsNumber: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        m_jit.add32(TrustedImm32(1), value.tagGPR(), result.gpr());
        m_jit.compare32(JITCompiler::Below, result.gpr(), JITCompiler::TrustedImm32(JSValue::LowestTag + 1), result.gpr());
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }

    case IsString: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        JITCompiler::Jump isNotCell = m_jit.branch32(JITCompiler::NotEqual, value.tagGPR(), JITCompiler::TrustedImm32(JSValue::CellTag));
        
        m_jit.loadPtr(JITCompiler::Address(value.payloadGPR(), JSCell::structureOffset()), result.gpr());
        m_jit.compare8(JITCompiler::Equal, JITCompiler::Address(result.gpr(), Structure::typeInfoTypeOffset()), TrustedImm32(StringType), result.gpr());
        JITCompiler::Jump done = m_jit.jump();
        
        isNotCell.link(&m_jit);
        m_jit.move(TrustedImm32(0), result.gpr());
        
        done.link(&m_jit);
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }

    case IsObject: {
        JSValueOperand value(this, node.child1());
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
        flushRegisters();
        callOperation(operationIsObject, resultGPR, valueTagGPR, valuePayloadGPR);
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }

    case IsFunction: {
        JSValueOperand value(this, node.child1());
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
        flushRegisters();
        callOperation(operationIsFunction, resultGPR, valueTagGPR, valuePayloadGPR);
        booleanResult(result.gpr(), m_compileIndex);
        break;
    }

    case Phi:
    case Flush:
        break;

    case Breakpoint:
#if ENABLE(DEBUG_WITH_BREAKPOINT)
        m_jit.breakpoint();
#else
        ASSERT_NOT_REACHED();
#endif
        break;
        
    case Call:
    case Construct:
        emitCall(node);
        break;

    case Resolve: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        ResolveOperationData& data = m_jit.graph().m_resolveOperationsData[node.resolveOperationsDataIndex()];
        callOperation(operationResolve, resultTag.gpr(), resultPayload.gpr(), identifier(data.identifierNumber), resolveOperations(data.resolveOperationsIndex));
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case ResolveBase: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        ResolveOperationData& data = m_jit.graph().m_resolveOperationsData[node.resolveOperationsDataIndex()];
        callOperation(operationResolveBase, resultTag.gpr(), resultPayload.gpr(), identifier(data.identifierNumber), resolveOperations(data.resolveOperationsIndex), putToBaseOperation(data.putToBaseOperationIndex));
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case ResolveBaseStrictPut: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        ResolveOperationData& data = m_jit.graph().m_resolveOperationsData[node.resolveOperationsDataIndex()];
        callOperation(operationResolveBaseStrictPut, resultTag.gpr(), resultPayload.gpr(), identifier(data.identifierNumber), resolveOperations(data.resolveOperationsIndex), putToBaseOperation(data.putToBaseOperationIndex));
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case ResolveGlobal: {
        GPRTemporary globalObject(this);
        GPRTemporary resolveInfo(this);
        GPRTemporary resultTag(this);
        GPRTemporary resultPayload(this);

        GPRReg globalObjectGPR = globalObject.gpr();
        GPRReg resolveInfoGPR = resolveInfo.gpr();
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();

        ResolveGlobalData& data = m_jit.graph().m_resolveGlobalData[node.resolveGlobalDataIndex()];
        ResolveOperation* resolveOperationAddress = &(m_jit.codeBlock()->resolveOperations(data.resolveOperationsIndex)->data()[data.resolvePropertyIndex]);

        // Check Structure of global object
        m_jit.move(JITCompiler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), globalObjectGPR);
        m_jit.move(JITCompiler::TrustedImmPtr(resolveOperationAddress), resolveInfoGPR);
        m_jit.loadPtr(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(ResolveOperation, m_structure)), resultPayloadGPR);

        JITCompiler::Jump structuresNotMatch = m_jit.branchPtr(JITCompiler::NotEqual, resultPayloadGPR, JITCompiler::Address(globalObjectGPR, JSCell::structureOffset()));

        // Fast case
        m_jit.loadPtr(JITCompiler::Address(globalObjectGPR, JSObject::butterflyOffset()), resultPayloadGPR);
        m_jit.load32(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(ResolveOperation, m_offset)), resolveInfoGPR);
#if DFG_ENABLE(JIT_ASSERT)
        JITCompiler::Jump isOutOfLine = m_jit.branch32(JITCompiler::GreaterThanOrEqual, resolveInfoGPR, TrustedImm32(firstOutOfLineOffset));
        m_jit.breakpoint();
        isOutOfLine.link(&m_jit);
#endif
        m_jit.neg32(resolveInfoGPR);
        m_jit.signExtend32ToPtr(resolveInfoGPR, resolveInfoGPR);
        m_jit.load32(JITCompiler::BaseIndex(resultPayloadGPR, resolveInfoGPR, JITCompiler::TimesEight, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag) + (firstOutOfLineOffset - 2) * static_cast<ptrdiff_t>(sizeof(JSValue))), resultTagGPR);
        m_jit.load32(JITCompiler::BaseIndex(resultPayloadGPR, resolveInfoGPR, JITCompiler::TimesEight, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload) + (firstOutOfLineOffset - 2) * static_cast<ptrdiff_t>(sizeof(JSValue))), resultPayloadGPR);

        addSlowPathGenerator(
            slowPathCall(
                structuresNotMatch, this, operationResolveGlobal,
                JSValueRegs(resultTagGPR, resultPayloadGPR), resolveInfoGPR, globalObjectGPR,
                &m_jit.codeBlock()->identifier(data.identifierNumber)));

        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }

    case CreateActivation: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value, false);
        
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(valuePayloadGPR, resultGPR);
        
        JITCompiler::Jump notCreated = m_jit.branch32(JITCompiler::Equal, valueTagGPR, TrustedImm32(JSValue::EmptyValueTag));
        
        addSlowPathGenerator(
            slowPathCall(notCreated, this, operationCreateActivation, resultGPR));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case CreateArguments: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value, false);
        
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(valuePayloadGPR, resultGPR);
        
        JITCompiler::Jump notCreated = m_jit.branch32(JITCompiler::Equal, valueTagGPR, TrustedImm32(JSValue::EmptyValueTag));
        
        if (node.codeOrigin.inlineCallFrame) {
            addSlowPathGenerator(
                slowPathCall(
                    notCreated, this, operationCreateInlinedArguments, resultGPR,
                    node.codeOrigin.inlineCallFrame));
        } else {
            addSlowPathGenerator(
                slowPathCall(notCreated, this, operationCreateArguments, resultGPR));
        }
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case TearOffActivation: {
        JSValueOperand activationValue(this, node.child1());
        GPRTemporary scratch(this);
        
        GPRReg activationValueTagGPR = activationValue.tagGPR();
        GPRReg activationValuePayloadGPR = activationValue.payloadGPR();
        GPRReg scratchGPR = scratch.gpr();

        JITCompiler::Jump notCreated = m_jit.branch32(JITCompiler::Equal, activationValueTagGPR, TrustedImm32(JSValue::EmptyValueTag));

        SharedSymbolTable* symbolTable = m_jit.symbolTableFor(node.codeOrigin);
        int registersOffset = JSActivation::registersOffset(symbolTable);

        int captureEnd = symbolTable->captureEnd();
        for (int i = symbolTable->captureStart(); i < captureEnd; ++i) {
            m_jit.loadPtr(
                JITCompiler::Address(
                    GPRInfo::callFrameRegister, i * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)),
                scratchGPR);
            m_jit.storePtr(
                scratchGPR, JITCompiler::Address(
                    activationValuePayloadGPR, registersOffset + i * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
            m_jit.loadPtr(
                JITCompiler::Address(
                    GPRInfo::callFrameRegister, i * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
                scratchGPR);
            m_jit.storePtr(
                scratchGPR, JITCompiler::Address(
                    activationValuePayloadGPR, registersOffset + i * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        }
        m_jit.addPtr(TrustedImm32(registersOffset), activationValuePayloadGPR, scratchGPR);
        m_jit.storePtr(scratchGPR, JITCompiler::Address(activationValuePayloadGPR, JSActivation::offsetOfRegisters()));
        
        notCreated.link(&m_jit);
        noResult(m_compileIndex);
        break;
    }
        
    case TearOffArguments: {
        JSValueOperand unmodifiedArgumentsValue(this, node.child1());
        JSValueOperand activationValue(this, node.child2());
        GPRReg unmodifiedArgumentsValuePayloadGPR = unmodifiedArgumentsValue.payloadGPR();
        GPRReg activationValuePayloadGPR = activationValue.payloadGPR();
        
        JITCompiler::Jump created = m_jit.branchTest32(
            JITCompiler::NonZero, unmodifiedArgumentsValuePayloadGPR);
        
        if (node.codeOrigin.inlineCallFrame) {
            addSlowPathGenerator(
                slowPathCall(
                    created, this, operationTearOffInlinedArguments, NoResult,
                    unmodifiedArgumentsValuePayloadGPR, activationValuePayloadGPR, node.codeOrigin.inlineCallFrame));
        } else {
            addSlowPathGenerator(
                slowPathCall(
                    created, this, operationTearOffArguments, NoResult,
                    unmodifiedArgumentsValuePayloadGPR, activationValuePayloadGPR));
        }
        
        noResult(m_compileIndex);
        break;
    }
        
    case CheckArgumentsNotCreated: {
        ASSERT(!isEmptySpeculation(
            m_state.variables().operand(
                m_jit.graph().argumentsRegisterFor(node.codeOrigin)).m_type));
        speculationCheck(
            Uncountable, JSValueRegs(), NoNode,
            m_jit.branch32(
                JITCompiler::NotEqual,
                JITCompiler::tagFor(m_jit.argumentsRegisterFor(node.codeOrigin)),
                TrustedImm32(JSValue::EmptyValueTag)));
        noResult(m_compileIndex);
        break;
    }
        
    case GetMyArgumentsLength: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
        
        if (!isEmptySpeculation(
                m_state.variables().operand(
                    m_jit.graph().argumentsRegisterFor(node.codeOrigin)).m_type)) {
            speculationCheck(
                ArgumentsEscaped, JSValueRegs(), NoNode,
                m_jit.branch32(
                    JITCompiler::NotEqual,
                    JITCompiler::tagFor(m_jit.argumentsRegisterFor(node.codeOrigin)),
                    TrustedImm32(JSValue::EmptyValueTag)));
        }
        
        ASSERT(!node.codeOrigin.inlineCallFrame);
        m_jit.load32(JITCompiler::payloadFor(JSStack::ArgumentCount), resultGPR);
        m_jit.sub32(TrustedImm32(1), resultGPR);
        integerResult(resultGPR, m_compileIndex);
        break;
    }
        
    case GetMyArgumentsLengthSafe: {
        GPRTemporary resultPayload(this);
        GPRTemporary resultTag(this);
        GPRReg resultPayloadGPR = resultPayload.gpr();
        GPRReg resultTagGPR = resultTag.gpr();
        
        JITCompiler::Jump created = m_jit.branch32(
            JITCompiler::NotEqual,
            JITCompiler::tagFor(m_jit.argumentsRegisterFor(node.codeOrigin)),
            TrustedImm32(JSValue::EmptyValueTag));
        
        if (node.codeOrigin.inlineCallFrame) {
            m_jit.move(
                Imm32(node.codeOrigin.inlineCallFrame->arguments.size() - 1),
                resultPayloadGPR);
        } else {
            m_jit.load32(JITCompiler::payloadFor(JSStack::ArgumentCount), resultPayloadGPR);
            m_jit.sub32(TrustedImm32(1), resultPayloadGPR);
        }
        m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
        
        // FIXME: the slow path generator should perform a forward speculation that the
        // result is an integer. For now we postpone the speculation by having this return
        // a JSValue.
        
        addSlowPathGenerator(
            slowPathCall(
                created, this, operationGetArgumentsLength,
                JSValueRegs(resultTagGPR, resultPayloadGPR),
                m_jit.argumentsRegisterFor(node.codeOrigin)));
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }
        
    case GetMyArgumentByVal: {
        SpeculateStrictInt32Operand index(this, node.child1());
        GPRTemporary resultPayload(this);
        GPRTemporary resultTag(this);
        GPRReg indexGPR = index.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        GPRReg resultTagGPR = resultTag.gpr();
        
        if (!isEmptySpeculation(
                m_state.variables().operand(
                    m_jit.graph().argumentsRegisterFor(node.codeOrigin)).m_type)) {
            speculationCheck(
                ArgumentsEscaped, JSValueRegs(), NoNode,
                m_jit.branch32(
                    JITCompiler::NotEqual,
                    JITCompiler::tagFor(m_jit.argumentsRegisterFor(node.codeOrigin)),
                    TrustedImm32(JSValue::EmptyValueTag)));
        }
            
        m_jit.add32(TrustedImm32(1), indexGPR, resultPayloadGPR);
            
        if (node.codeOrigin.inlineCallFrame) {
            speculationCheck(
                Uncountable, JSValueRegs(), NoNode,
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultPayloadGPR,
                    Imm32(node.codeOrigin.inlineCallFrame->arguments.size())));
        } else {
            speculationCheck(
                Uncountable, JSValueRegs(), NoNode,
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultPayloadGPR,
                    JITCompiler::payloadFor(JSStack::ArgumentCount)));
        }
        
        JITCompiler::JumpList slowArgument;
        JITCompiler::JumpList slowArgumentOutOfBounds;
        if (const SlowArgument* slowArguments = m_jit.symbolTableFor(node.codeOrigin)->slowArguments()) {
            slowArgumentOutOfBounds.append(
                m_jit.branch32(
                    JITCompiler::AboveOrEqual, indexGPR,
                    Imm32(m_jit.symbolTableFor(node.codeOrigin)->parameterCount())));

            COMPILE_ASSERT(sizeof(SlowArgument) == 8, SlowArgument_size_is_eight_bytes);
            m_jit.move(ImmPtr(slowArguments), resultPayloadGPR);
            m_jit.load32(
                JITCompiler::BaseIndex(
                    resultPayloadGPR, indexGPR, JITCompiler::TimesEight, 
                    OBJECT_OFFSETOF(SlowArgument, index)), 
                resultPayloadGPR);

            m_jit.load32(
                JITCompiler::BaseIndex(
                    GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                    m_jit.offsetOfLocals(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)),
                resultTagGPR);
            m_jit.load32(
                JITCompiler::BaseIndex(
                    GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                    m_jit.offsetOfLocals(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
                resultPayloadGPR);
            slowArgument.append(m_jit.jump());
        }
        slowArgumentOutOfBounds.link(&m_jit);

        m_jit.neg32(resultPayloadGPR);
        
        m_jit.load32(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                m_jit.offsetOfArgumentsIncludingThis(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)),
            resultTagGPR);
        m_jit.load32(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                m_jit.offsetOfArgumentsIncludingThis(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
            resultPayloadGPR);
            
        slowArgument.link(&m_jit);
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }
    case GetMyArgumentByValSafe: {
        SpeculateStrictInt32Operand index(this, node.child1());
        GPRTemporary resultPayload(this);
        GPRTemporary resultTag(this);
        GPRReg indexGPR = index.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        GPRReg resultTagGPR = resultTag.gpr();
        
        JITCompiler::JumpList slowPath;
        slowPath.append(
            m_jit.branch32(
                JITCompiler::NotEqual,
                JITCompiler::tagFor(m_jit.argumentsRegisterFor(node.codeOrigin)),
                TrustedImm32(JSValue::EmptyValueTag)));
        
        m_jit.add32(TrustedImm32(1), indexGPR, resultPayloadGPR);
        if (node.codeOrigin.inlineCallFrame) {
            slowPath.append(
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultPayloadGPR,
                    Imm32(node.codeOrigin.inlineCallFrame->arguments.size())));
        } else {
            slowPath.append(
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultPayloadGPR,
                    JITCompiler::payloadFor(JSStack::ArgumentCount)));
        }
        
        JITCompiler::JumpList slowArgument;
        JITCompiler::JumpList slowArgumentOutOfBounds;
        if (const SlowArgument* slowArguments = m_jit.symbolTableFor(node.codeOrigin)->slowArguments()) {
            slowArgumentOutOfBounds.append(
                m_jit.branch32(
                    JITCompiler::AboveOrEqual, indexGPR,
                    Imm32(m_jit.symbolTableFor(node.codeOrigin)->parameterCount())));

            COMPILE_ASSERT(sizeof(SlowArgument) == 8, SlowArgument_size_is_eight_bytes);
            m_jit.move(ImmPtr(slowArguments), resultPayloadGPR);
            m_jit.load32(
                JITCompiler::BaseIndex(
                    resultPayloadGPR, indexGPR, JITCompiler::TimesEight, 
                    OBJECT_OFFSETOF(SlowArgument, index)), 
                resultPayloadGPR);
            m_jit.load32(
                JITCompiler::BaseIndex(
                    GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                    m_jit.offsetOfLocals(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)),
                resultTagGPR);
            m_jit.load32(
                JITCompiler::BaseIndex(
                    GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                    m_jit.offsetOfLocals(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
                resultPayloadGPR);
            slowArgument.append(m_jit.jump());
        }
        slowArgumentOutOfBounds.link(&m_jit);

        m_jit.neg32(resultPayloadGPR);
        
        m_jit.load32(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                m_jit.offsetOfArgumentsIncludingThis(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)),
            resultTagGPR);
        m_jit.load32(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, resultPayloadGPR, JITCompiler::TimesEight,
                m_jit.offsetOfArgumentsIncludingThis(node.codeOrigin) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
            resultPayloadGPR);
        
        if (node.codeOrigin.inlineCallFrame) {
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationGetInlinedArgumentByVal,
                    JSValueRegs(resultTagGPR, resultPayloadGPR),
                    m_jit.argumentsRegisterFor(node.codeOrigin),
                    node.codeOrigin.inlineCallFrame, indexGPR));
        } else {
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationGetArgumentByVal,
                    JSValueRegs(resultTagGPR, resultPayloadGPR),
                    m_jit.argumentsRegisterFor(node.codeOrigin), indexGPR));
        }
        
        slowArgument.link(&m_jit);
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }
        
    case NewFunctionNoCheck:
        compileNewFunctionNoCheck(node);
        break;
        
    case NewFunction: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value, false);
        
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(valuePayloadGPR, resultGPR);
        
        JITCompiler::Jump notCreated = m_jit.branch32(JITCompiler::Equal, valueTagGPR, TrustedImm32(JSValue::EmptyValueTag));
        
        addSlowPathGenerator(
            slowPathCall(
                notCreated, this, operationNewFunction, resultGPR,
                m_jit.codeBlock()->functionDecl(node.functionDeclIndex())));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case NewFunctionExpression:
        compileNewFunctionExpression(node);
        break;

    case GarbageValue:
        // We should never get to the point of code emission for a GarbageValue
        CRASH();
        break;

    case ForceOSRExit: {
        terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
        break;
    }

    case CountExecution:
        m_jit.add64(TrustedImm32(1), MacroAssembler::AbsoluteAddress(node.executionCounter()->address()));
        break;

    case Phantom:
        // This is a no-op.
        noResult(m_compileIndex);
        break;

    case InlineStart:
    case Nop:
    case LastNodeType:
        ASSERT_NOT_REACHED();
        break;
    }
    
    if (!m_compileOkay)
        return;
    
    if (node.hasResult() && node.mustGenerate())
        use(m_compileIndex);
}

#endif

} } // namespace JSC::DFG

#endif
