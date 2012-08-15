/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#include "Arguments.h"
#include "DFGSlowPathGenerator.h"

namespace JSC { namespace DFG {

#if USE(JSVALUE64)

GPRReg SpeculativeJIT::fillInteger(NodeIndex nodeIndex, DataFormat& returnFormat)
{
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            if (isInt32Constant(nodeIndex)) {
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                info.fillInteger(*m_stream, gpr);
                returnFormat = DataFormatInteger;
                return gpr;
            }
            if (isNumberConstant(nodeIndex)) {
                JSValue jsValue = jsNumber(valueOfNumberConstant(nodeIndex));
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            } else {
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsValue)), gpr);
            }
        } else if (info.spillFormat() == DataFormatInteger) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
            // Tag it, since fillInteger() is used when we want a boxed integer.
            m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr);
        } else {
            ASSERT(info.spillFormat() == DataFormatJS || info.spillFormat() == DataFormatJSInteger);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
        }

        // Since we statically know that we're filling an integer, and values
        // in the RegisterFile are boxed, this must be DataFormatJSInteger.
        // We will check this with a jitAssert below.
        info.fillJSValue(*m_stream, gpr, DataFormatJSInteger);
        unlock(gpr);
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
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        m_jit.jitAssertIsJSInt32(gpr);
        returnFormat = DataFormatJSInteger;
        return gpr;
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
            GPRReg gpr = allocate();
        
            if (isInt32Constant(nodeIndex)) {
                // FIXME: should not be reachable?
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillInteger(*m_stream, gpr);
                unlock(gpr);
            } else if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfNumberConstant(nodeIndex)))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            } else {
                // FIXME: should not be reachable?
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsValue)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillJSValue(*m_stream, gpr, DataFormatJS);
                unlock(gpr);
            }
        } else {
            DataFormat spillFormat = info.spillFormat();
            switch (spillFormat) {
            case DataFormatDouble: {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            }
                
            case DataFormatInteger: {
                GPRReg gpr = allocate();
                
                m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
                m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
                info.fillInteger(*m_stream, gpr);
                unlock(gpr);
                break;
            }

            default:
                GPRReg gpr = allocate();
        
                ASSERT(spillFormat & DataFormatJS);
                m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
                m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
                info.fillJSValue(*m_stream, gpr, spillFormat);
                unlock(gpr);
                break;
            }
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

    case DataFormatJS: {
        GPRReg jsValueGpr = info.gpr();
        m_gprs.lock(jsValueGpr);
        FPRReg fpr = fprAllocate();
        GPRReg tempGpr = allocate(); // FIXME: can we skip this allocation on the last use of the virtual register?

        JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);

        m_jit.jitAssertIsJSDouble(jsValueGpr);

        // First, if we get here we have a double encoded as a JSValue
        m_jit.move(jsValueGpr, tempGpr);
        unboxDouble(tempGpr, fpr);
        JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

        // Finally, handle integers.
        isInteger.link(&m_jit);
        m_jit.convertInt32ToDouble(jsValueGpr, fpr);
        hasUnboxedDouble.link(&m_jit);

        m_gprs.release(jsValueGpr);
        m_gprs.unlock(jsValueGpr);
        m_gprs.unlock(tempGpr);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(*m_stream, fpr);
        info.killSpilled();
        return fpr;
    }

    case DataFormatJSInteger:
    case DataFormatInteger: {
        FPRReg fpr = fprAllocate();
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        m_jit.convertInt32ToDouble(gpr, fpr);
        m_gprs.unlock(gpr);
        return fpr;
    }

    // Unbox the double
    case DataFormatJSDouble: {
        GPRReg gpr = info.gpr();
        FPRReg fpr = fprAllocate();
        if (m_gprs.isLocked(gpr)) {
            // Make sure we don't trample gpr if it is in use.
            GPRReg temp = allocate();
            m_jit.move(gpr, temp);
            unboxDouble(temp, fpr);
            unlock(temp);
        } else
            unboxDouble(gpr, fpr);

        m_gprs.release(gpr);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);

        info.fillDouble(*m_stream, fpr);
        return fpr;
    }

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

GPRReg SpeculativeJIT::fillJSValue(NodeIndex nodeIndex)
{
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            if (isInt32Constant(nodeIndex)) {
                info.fillJSValue(*m_stream, gpr, DataFormatJSInteger);
                JSValue jsValue = jsNumber(valueOfInt32Constant(nodeIndex));
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            } else if (isNumberConstant(nodeIndex)) {
                info.fillJSValue(*m_stream, gpr, DataFormatJSDouble);
                JSValue jsValue(JSValue::EncodeAsDouble, valueOfNumberConstant(nodeIndex));
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            } else {
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsValue)), gpr);
                info.fillJSValue(*m_stream, gpr, DataFormatJS);
            }

            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
        } else {
            DataFormat spillFormat = info.spillFormat();
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            if (spillFormat == DataFormatInteger) {
                m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
                m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr);
                spillFormat = DataFormatJSInteger;
            } else {
                m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
                if (spillFormat == DataFormatDouble) {
                    // Need to box the double, since we want a JSValue.
                    m_jit.subPtr(GPRInfo::tagTypeNumberRegister, gpr);
                    spillFormat = DataFormatJSDouble;
                } else
                    ASSERT(spillFormat & DataFormatJS);
            }
            info.fillJSValue(*m_stream, gpr, spillFormat);
        }
        return gpr;
    }

    case DataFormatInteger: {
        GPRReg gpr = info.gpr();
        // If the register has already been locked we need to take a copy.
        // If not, we'll zero extend in place, so mark on the info that this is now type DataFormatInteger, not DataFormatJSInteger.
        if (m_gprs.isLocked(gpr)) {
            GPRReg result = allocate();
            m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr, result);
            return result;
        }
        m_gprs.lock(gpr);
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr);
        info.fillJSValue(*m_stream, gpr, DataFormatJSInteger);
        return gpr;
    }

    case DataFormatDouble: {
        FPRReg fpr = info.fpr();
        GPRReg gpr = boxDouble(fpr);

        // Update all info
        info.fillJSValue(*m_stream, gpr, DataFormatJSDouble);
        m_fprs.release(fpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderJS);

        return gpr;
    }

    case DataFormatCell:
        // No retag required on JSVALUE64!
    case DataFormatJS:
    case DataFormatJSInteger:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatJSBoolean: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }
        
    case DataFormatBoolean:
    case DataFormatStorage:
        // this type currently never occurs
        ASSERT_NOT_REACHED();
        
    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

class ValueToNumberSlowPathGenerator
    : public CallSlowPathGenerator<MacroAssembler::Jump, D_DFGOperation_EJ, GPRReg> {
public:
    ValueToNumberSlowPathGenerator(
        MacroAssembler::Jump from, SpeculativeJIT* jit,
        GPRReg resultGPR, GPRReg jsValueGPR)
        : CallSlowPathGenerator<MacroAssembler::Jump, D_DFGOperation_EJ, GPRReg>(
            from, jit, dfgConvertJSValueToNumber, NeedToSpill, resultGPR)
        , m_jsValueGPR(jsValueGPR)
    {
    }

protected:
    virtual void generateInternal(SpeculativeJIT* jit)
    {
        setUp(jit);
        recordCall(jit->callOperation(dfgConvertJSValueToNumber, FPRInfo::returnValueFPR, m_jsValueGPR));
        jit->boxDouble(FPRInfo::returnValueFPR, m_result);
        tearDown(jit);
    }

private:
    GPRReg m_jsValueGPR;
};

void SpeculativeJIT::nonSpeculativeValueToNumber(Node& node)
{
    if (isKnownNumeric(node.child1().index())) {
        JSValueOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        m_jit.move(op1.gpr(), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex);
        return;
    }

    JSValueOperand op1(this, node.child1());
    GPRTemporary result(this);
    
    ASSERT(!isInt32Constant(node.child1().index()));
    ASSERT(!isNumberConstant(node.child1().index()));
    
    GPRReg jsValueGpr = op1.gpr();
    GPRReg gpr = result.gpr();
    op1.use();

    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);
    JITCompiler::Jump nonNumeric = m_jit.branchTestPtr(MacroAssembler::Zero, jsValueGpr, GPRInfo::tagTypeNumberRegister);

    // First, if we get here we have a double encoded as a JSValue
    m_jit.move(jsValueGpr, gpr);
    JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

    // Finally, handle integers.
    isInteger.link(&m_jit);
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, jsValueGpr, gpr);
    hasUnboxedDouble.link(&m_jit);
    
    addSlowPathGenerator(adoptPtr(new ValueToNumberSlowPathGenerator(nonNumeric, this, gpr, jsValueGpr)));
    
    jsValueResult(result.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativeValueToInt32(Node& node)
{
    ASSERT(!isInt32Constant(node.child1().index()));
    
    if (isKnownInteger(node.child1().index())) {
        IntegerOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        m_jit.zeroExtend32ToPtr(op1.gpr(), result.gpr());
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
        
        addSlowPathGenerator(
            slowPathCall(notTruncatedToInteger, this, toInt32, gpr, fpr));

        integerResult(gpr, m_compileIndex, UseChildrenCalledExplicitly);
        return;
    }
    
    JSValueOperand op1(this, node.child1());
    GPRTemporary result(this, op1);
    GPRReg jsValueGpr = op1.gpr();
    GPRReg resultGPR = result.gpr();
    op1.use();

    JITCompiler::Jump isNotInteger = m_jit.branchPtr(MacroAssembler::Below, jsValueGpr, GPRInfo::tagTypeNumberRegister);

    m_jit.zeroExtend32ToPtr(jsValueGpr, resultGPR);
    
    addSlowPathGenerator(
        slowPathCall(isNotInteger, this, dfgConvertJSValueToInt32, resultGPR, jsValueGpr));

    integerResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativeUInt32ToNumber(Node& node)
{
    IntegerOperand op1(this, node.child1());
    FPRTemporary boxer(this);
    GPRTemporary result(this, op1);
    
    JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, op1.gpr(), TrustedImm32(0));
    
    m_jit.convertInt32ToDouble(op1.gpr(), boxer.fpr());
    m_jit.addDouble(JITCompiler::AbsoluteAddress(&AssemblyHelpers::twoToThe32), boxer.fpr());
    
    boxDouble(boxer.fpr(), result.gpr());
    
    JITCompiler::Jump done = m_jit.jump();
    
    positive.link(&m_jit);
    
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, op1.gpr(), result.gpr());
    
    done.link(&m_jit);
    
    jsValueResult(result.gpr(), m_compileIndex);
}

void SpeculativeJIT::cachedGetById(CodeOrigin codeOrigin, GPRReg baseGPR, GPRReg resultGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget, SpillRegistersMode spillMode)
{
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::PatchableJump structureCheck = m_jit.patchableBranchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(baseGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    
    JITCompiler::ConvertibleLoadLabel propertyStorageLoad =
        m_jit.convertibleLoadPtr(JITCompiler::Address(baseGPR, JSObject::offsetOfOutOfLineStorage()), resultGPR);
    JITCompiler::DataLabelCompact loadWithPatch = m_jit.loadPtrWithCompactAddressOffsetPatch(JITCompiler::Address(resultGPR, 0), resultGPR);
    
    JITCompiler::Label doneLabel = m_jit.label();

    OwnPtr<SlowPathGenerator> slowPath;
    if (!slowPathTarget.isSet()) {
        slowPath = slowPathCall(
            structureCheck.m_jump, this, operationGetByIdOptimize, resultGPR, baseGPR,
            identifier(identifierNumber), spillMode);
    } else {
        JITCompiler::JumpList slowCases;
        slowCases.append(structureCheck.m_jump);
        slowCases.append(slowPathTarget);
        slowPath = slowPathCall(
            slowCases, this, operationGetByIdOptimize, resultGPR, baseGPR,
            identifier(identifierNumber), spillMode);
    }
    m_jit.addPropertyAccess(
        PropertyAccessRecord(
            codeOrigin, structureToCompare, structureCheck, propertyStorageLoad, loadWithPatch,
            slowPath.get(), doneLabel, safeCast<int8_t>(baseGPR), safeCast<int8_t>(resultGPR),
            usedRegisters(),
            spillMode == NeedToSpill ? PropertyAccessRecord::RegistersInUse : PropertyAccessRecord::RegistersFlushed));
    addSlowPathGenerator(slowPath.release());
}

void SpeculativeJIT::cachedPutById(CodeOrigin codeOrigin, GPRReg baseGPR, GPRReg valueGPR, Edge valueUse, GPRReg scratchGPR, unsigned identifierNumber, PutKind putKind, JITCompiler::Jump slowPathTarget)
{
    
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::PatchableJump structureCheck = m_jit.patchableBranchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(baseGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));

    writeBarrier(baseGPR, valueGPR, valueUse, WriteBarrierForPropertyAccess, scratchGPR);

    JITCompiler::ConvertibleLoadLabel propertyStorageLoad =
        m_jit.convertibleLoadPtr(JITCompiler::Address(baseGPR, JSObject::offsetOfOutOfLineStorage()), scratchGPR);
    JITCompiler::DataLabel32 storeWithPatch = m_jit.storePtrWithAddressOffsetPatch(valueGPR, JITCompiler::Address(scratchGPR, 0));

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
            structureCheck.m_jump, this, optimizedCall, NoResult, valueGPR, baseGPR,
            identifier(identifierNumber));
    } else {
        JITCompiler::JumpList slowCases;
        slowCases.append(structureCheck.m_jump);
        slowCases.append(slowPathTarget);
        slowPath = slowPathCall(
            slowCases, this, optimizedCall, NoResult, valueGPR, baseGPR,
            identifier(identifierNumber));
    }
    RegisterSet currentlyUsedRegisters = usedRegisters();
    currentlyUsedRegisters.clear(scratchGPR);
    ASSERT(currentlyUsedRegisters.get(baseGPR));
    ASSERT(currentlyUsedRegisters.get(valueGPR));
    m_jit.addPropertyAccess(
        PropertyAccessRecord(
            codeOrigin, structureToCompare, structureCheck, propertyStorageLoad,
            JITCompiler::DataLabelCompact(storeWithPatch.label()), slowPath.get(), doneLabel,
            safeCast<int8_t>(baseGPR), safeCast<int8_t>(valueGPR), currentlyUsedRegisters));
    addSlowPathGenerator(slowPath.release());
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompareNull(Edge operand, bool invert)
{
    JSValueOperand arg(this, operand);
    GPRReg argGPR = arg.gpr();
    
    GPRTemporary result(this, arg);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::Jump notCell;
    
    if (!isKnownCell(operand.index()))
        notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, argGPR, GPRInfo::tagMaskRegister);
    
    m_jit.loadPtr(JITCompiler::Address(argGPR, JSCell::structureOffset()), resultGPR);
    m_jit.test8(invert ? JITCompiler::Zero : JITCompiler::NonZero, JITCompiler::Address(resultGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined), resultGPR);
    
    if (!isKnownCell(operand.index())) {
        JITCompiler::Jump done = m_jit.jump();
        
        notCell.link(&m_jit);
        
        m_jit.move(argGPR, resultGPR);
        m_jit.andPtr(JITCompiler::TrustedImm32(~TagBitUndefined), resultGPR);
        m_jit.comparePtr(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImm32(ValueNull), resultGPR);
        
        done.link(&m_jit);
    }
    
    m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
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
    GPRReg argGPR = arg.gpr();
    
    GPRTemporary result(this, arg);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::Jump notCell;
    
    if (!isKnownCell(operand.index()))
        notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, argGPR, GPRInfo::tagMaskRegister);
    
    m_jit.loadPtr(JITCompiler::Address(argGPR, JSCell::structureOffset()), resultGPR);
    branchTest8(invert ? JITCompiler::Zero : JITCompiler::NonZero, JITCompiler::Address(resultGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined), taken);
    
    if (!isKnownCell(operand.index())) {
        jump(notTaken, ForceJump);
        
        notCell.link(&m_jit);
        
        m_jit.move(argGPR, resultGPR);
        m_jit.andPtr(JITCompiler::TrustedImm32(~TagBitUndefined), resultGPR);
        branchPtr(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull)), taken);
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
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1().index()) || isKnownNotInteger(node.child2().index())) {
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();
    
        flushRegisters();
        callOperation(helperFunction, resultGPR, arg1GPR, arg2GPR);

        branchTest32(callResultCondition, resultGPR, taken);
    } else {
        GPRTemporary result(this, arg2);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();
    
        if (!isKnownInteger(node.child1().index()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister));
        if (!isKnownInteger(node.child2().index()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister));
    
        branch32(cond, arg1GPR, arg2GPR, taken);
    
        if (!isKnownInteger(node.child1().index()) || !isKnownInteger(node.child2().index())) {
            jump(notTaken, ForceJump);
    
            slowPath.link(&m_jit);
    
            silentSpillAllRegisters(resultGPR);
            callOperation(helperFunction, resultGPR, arg1GPR, arg2GPR);
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
        S_DFGOperation_EJJ function, GPRReg result, GPRReg arg1, GPRReg arg2)
        : CallSlowPathGenerator<JumpType, S_DFGOperation_EJJ, GPRReg>(
            from, jit, function, NeedToSpill, result)
        , m_arg1(arg1)
        , m_arg2(arg2)
    {
    }
    
protected:
    virtual void generateInternal(SpeculativeJIT* jit)
    {
        this->setUp(jit);
        this->recordCall(jit->callOperation(this->m_function, this->m_result, m_arg1, m_arg2));
        jit->m_jit.and32(JITCompiler::TrustedImm32(1), this->m_result);
        jit->m_jit.or32(JITCompiler::TrustedImm32(ValueFalse), this->m_result);
        this->tearDown(jit);
    }
   
private:
    GPRReg m_arg1;
    GPRReg m_arg2;
};

void SpeculativeJIT::nonSpeculativeNonPeepholeCompare(Node& node, MacroAssembler::RelationalCondition cond, S_DFGOperation_EJJ helperFunction)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1().index()) || isKnownNotInteger(node.child2().index())) {
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();
    
        flushRegisters();
        callOperation(helperFunction, resultGPR, arg1GPR, arg2GPR);
        
        m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
        jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean, UseChildrenCalledExplicitly);
    } else {
        GPRTemporary result(this, arg2);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();
    
        if (!isKnownInteger(node.child1().index()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister));
        if (!isKnownInteger(node.child2().index()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister));
    
        m_jit.compare32(cond, arg1GPR, arg2GPR, resultGPR);
        m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
        
        if (!isKnownInteger(node.child1().index()) || !isKnownInteger(node.child2().index())) {
            addSlowPathGenerator(adoptPtr(
                new CompareAndBoxBooleanSlowPathGenerator<JITCompiler::JumpList>(
                    slowPath, this, helperFunction, resultGPR, arg1GPR, arg2GPR)));
        }

        jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean, UseChildrenCalledExplicitly);
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
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node.child1().index()) && isKnownCell(node.child2().index())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        branchPtr(JITCompiler::Equal, arg1GPR, arg2GPR, invert ? notTaken : taken);
        
        silentSpillAllRegisters(resultGPR);
        callOperation(operationCompareStrictEqCell, resultGPR, arg1GPR, arg2GPR);
        silentFillAllRegisters(resultGPR);
        
        branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultGPR, taken);
    } else {
        m_jit.orPtr(arg1GPR, arg2GPR, resultGPR);
        
        JITCompiler::Jump twoCellsCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, GPRInfo::tagMaskRegister);
        
        JITCompiler::Jump leftOK = m_jit.branchPtr(JITCompiler::AboveOrEqual, arg1GPR, GPRInfo::tagTypeNumberRegister);
        JITCompiler::Jump leftDouble = m_jit.branchTestPtr(JITCompiler::NonZero, arg1GPR, GPRInfo::tagTypeNumberRegister);
        leftOK.link(&m_jit);
        JITCompiler::Jump rightOK = m_jit.branchPtr(JITCompiler::AboveOrEqual, arg2GPR, GPRInfo::tagTypeNumberRegister);
        JITCompiler::Jump rightDouble = m_jit.branchTestPtr(JITCompiler::NonZero, arg2GPR, GPRInfo::tagTypeNumberRegister);
        rightOK.link(&m_jit);
        
        branchPtr(invert ? JITCompiler::NotEqual : JITCompiler::Equal, arg1GPR, arg2GPR, taken);
        jump(notTaken, ForceJump);
        
        twoCellsCase.link(&m_jit);
        branchPtr(JITCompiler::Equal, arg1GPR, arg2GPR, invert ? notTaken : taken);
        
        leftDouble.link(&m_jit);
        rightDouble.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        callOperation(operationCompareStrictEq, resultGPR, arg1GPR, arg2GPR);
        silentFillAllRegisters(resultGPR);
        
        branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultGPR, taken);
    }
    
    jump(notTaken);
}

void SpeculativeJIT::nonSpeculativeNonPeepholeStrictEq(Node& node, bool invert)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node.child1().index()) && isKnownCell(node.child2().index())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        // FIXME: this should flush registers instead of silent spill/fill.
        JITCompiler::Jump notEqualCase = m_jit.branchPtr(JITCompiler::NotEqual, arg1GPR, arg2GPR);
        
        m_jit.move(JITCompiler::TrustedImmPtr(JSValue::encode(jsBoolean(!invert))), resultGPR);
        
        JITCompiler::Jump done = m_jit.jump();

        notEqualCase.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        callOperation(operationCompareStrictEqCell, resultGPR, arg1GPR, arg2GPR);
        silentFillAllRegisters(resultGPR);
        
        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultGPR);
        m_jit.or32(JITCompiler::TrustedImm32(ValueFalse), resultGPR);
        
        done.link(&m_jit);
    } else {
        m_jit.orPtr(arg1GPR, arg2GPR, resultGPR);
        
        JITCompiler::JumpList slowPathCases;
        
        JITCompiler::Jump twoCellsCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, GPRInfo::tagMaskRegister);
        
        JITCompiler::Jump leftOK = m_jit.branchPtr(JITCompiler::AboveOrEqual, arg1GPR, GPRInfo::tagTypeNumberRegister);
        slowPathCases.append(m_jit.branchTestPtr(JITCompiler::NonZero, arg1GPR, GPRInfo::tagTypeNumberRegister));
        leftOK.link(&m_jit);
        JITCompiler::Jump rightOK = m_jit.branchPtr(JITCompiler::AboveOrEqual, arg2GPR, GPRInfo::tagTypeNumberRegister);
        slowPathCases.append(m_jit.branchTestPtr(JITCompiler::NonZero, arg2GPR, GPRInfo::tagTypeNumberRegister));
        rightOK.link(&m_jit);
        
        m_jit.comparePtr(invert ? JITCompiler::NotEqual : JITCompiler::Equal, arg1GPR, arg2GPR, resultGPR);
        m_jit.or32(JITCompiler::TrustedImm32(ValueFalse), resultGPR);
        
        JITCompiler::Jump done = m_jit.jump();
        
        twoCellsCase.link(&m_jit);
        slowPathCases.append(m_jit.branchPtr(JITCompiler::NotEqual, arg1GPR, arg2GPR));
        
        m_jit.move(JITCompiler::TrustedImmPtr(JSValue::encode(jsBoolean(!invert))), resultGPR);
        
        addSlowPathGenerator(
            adoptPtr(
                new CompareAndBoxBooleanSlowPathGenerator<MacroAssembler::JumpList>(
                    slowPathCases, this, operationCompareStrictEq, resultGPR, arg1GPR,
                    arg2GPR)));
        
        done.link(&m_jit);
    }
    
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean, UseChildrenCalledExplicitly);
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
    GPRReg calleeGPR = callee.gpr();
    use(calleeEdge);
    
    // The call instruction's first child is the function; the subsequent children are the
    // arguments.
    int numPassedArgs = node.numChildren() - 1;
    
    m_jit.store32(MacroAssembler::TrustedImm32(numPassedArgs + dummyThisArgument), callFramePayloadSlot(RegisterFile::ArgumentCount));
    m_jit.storePtr(GPRInfo::callFrameRegister, callFrameSlot(RegisterFile::CallerFrame));
    m_jit.storePtr(calleeGPR, callFrameSlot(RegisterFile::Callee));
    
    for (int i = 0; i < numPassedArgs; i++) {
        Edge argEdge = m_jit.graph().m_varArgChildren[node.firstChild() + 1 + i];
        JSValueOperand arg(this, argEdge);
        GPRReg argGPR = arg.gpr();
        use(argEdge);
        
        m_jit.storePtr(argGPR, argumentSlot(i + dummyThisArgument));
    }

    flushRegisters();

    GPRResult result(this);
    GPRReg resultGPR = result.gpr();

    JITCompiler::DataLabelPtr targetToCheck;
    JITCompiler::Jump slowPath;

    CallBeginToken token;
    m_jit.beginCall(node.codeOrigin, token);
    
    m_jit.addPtr(TrustedImm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister);
    
    slowPath = m_jit.branchPtrWithPatch(MacroAssembler::NotEqual, calleeGPR, targetToCheck, MacroAssembler::TrustedImmPtr(JSValue::encode(JSValue())));
    m_jit.loadPtr(MacroAssembler::Address(calleeGPR, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), resultGPR);
    m_jit.storePtr(resultGPR, MacroAssembler::Address(GPRInfo::callFrameRegister, static_cast<ptrdiff_t>(sizeof(Register)) * RegisterFile::ScopeChain));

    CodeOrigin codeOrigin = at(m_compileIndex).codeOrigin;
    JITCompiler::Call fastCall = m_jit.nearCall();
    m_jit.notifyCall(fastCall, codeOrigin, token);
    
    JITCompiler::Jump done = m_jit.jump();
    
    slowPath.link(&m_jit);
    
    m_jit.move(calleeGPR, GPRInfo::nonArgGPR0);
    m_jit.prepareForExceptionCheck();
    JITCompiler::Call slowCall = m_jit.nearCall();
    m_jit.notifyCall(slowCall, codeOrigin, token);
    
    done.link(&m_jit);
    
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    
    jsValueResult(resultGPR, m_compileIndex, DataFormatJS, UseChildrenCalledExplicitly);
    
    m_jit.addJSCall(fastCall, slowCall, targetToCheck, callType, at(m_compileIndex).codeOrigin);
}

template<bool strict>
GPRReg SpeculativeJIT::fillSpeculateIntInternal(NodeIndex nodeIndex, DataFormat& returnFormat)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("SpecInt@%d   ", nodeIndex);
#endif
    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        if ((node.hasConstant() && !isInt32Constant(nodeIndex)) || info.spillFormat() == DataFormatDouble) {
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
            returnFormat = DataFormatInteger;
            return allocate();
        }
        
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            ASSERT(isInt32Constant(nodeIndex));
            m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
            info.fillInteger(*m_stream, gpr);
            returnFormat = DataFormatInteger;
            return gpr;
        }
        
        DataFormat spillFormat = info.spillFormat();
        
        ASSERT((spillFormat & DataFormatJS) || spillFormat == DataFormatInteger);
        
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        
        if (spillFormat == DataFormatJSInteger || spillFormat == DataFormatInteger) {
            // If we know this was spilled as an integer we can fill without checking.
            if (strict) {
                m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
                info.fillInteger(*m_stream, gpr);
                returnFormat = DataFormatInteger;
                return gpr;
            }
            if (spillFormat == DataFormatInteger) {
                m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
                m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr);
            } else
                m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(*m_stream, gpr, DataFormatJSInteger);
            returnFormat = DataFormatJSInteger;
            return gpr;
        }
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        // Fill as JSValue, and fall through.
        info.fillJSValue(*m_stream, gpr, DataFormatJSInteger);
        m_gprs.unlock(gpr);
    }

    case DataFormatJS: {
        // Check the value is an integer.
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (!isInt32Speculation(type))
            speculationCheck(BadType, JSValueRegs(gpr), nodeIndex, m_jit.branchPtr(MacroAssembler::Below, gpr, GPRInfo::tagTypeNumberRegister));
        info.fillJSValue(*m_stream, gpr, DataFormatJSInteger);
        // If !strict we're done, return.
        if (!strict) {
            returnFormat = DataFormatJSInteger;
            return gpr;
        }
        // else fall through & handle as DataFormatJSInteger.
        m_gprs.unlock(gpr);
    }

    case DataFormatJSInteger: {
        // In a strict fill we need to strip off the value tag.
        if (strict) {
            GPRReg gpr = info.gpr();
            GPRReg result;
            // If the register has already been locked we need to take a copy.
            // If not, we'll zero extend in place, so mark on the info that this is now type DataFormatInteger, not DataFormatJSInteger.
            if (m_gprs.isLocked(gpr))
                result = allocate();
            else {
                m_gprs.lock(gpr);
                info.fillInteger(*m_stream, gpr);
                result = gpr;
            }
            m_jit.zeroExtend32ToPtr(gpr, result);
            returnFormat = DataFormatInteger;
            return result;
        }

        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        returnFormat = DataFormatJSInteger;
        return gpr;
    }

    case DataFormatInteger: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        returnFormat = DataFormatInteger;
        return gpr;
    }

    case DataFormatDouble:
    case DataFormatJSDouble: {
        if (node.hasConstant() && isInt32Constant(nodeIndex)) {
            GPRReg gpr = allocate();
            ASSERT(isInt32Constant(nodeIndex));
            m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
            returnFormat = DataFormatInteger;
            return gpr;
        }
    }
    case DataFormatCell:
    case DataFormatBoolean:
    case DataFormatJSCell:
    case DataFormatJSBoolean: {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
        returnFormat = DataFormatInteger;
        return allocate();
    }

    case DataFormatStorage:
        ASSERT_NOT_REACHED();
        
    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateInt(NodeIndex nodeIndex, DataFormat& returnFormat)
{
    return fillSpeculateIntInternal<false>(nodeIndex, returnFormat);
}

GPRReg SpeculativeJIT::fillSpeculateIntStrict(NodeIndex nodeIndex)
{
    DataFormat mustBeDataFormatInteger;
    GPRReg result = fillSpeculateIntInternal<true>(nodeIndex, mustBeDataFormatInteger);
    ASSERT(mustBeDataFormatInteger == DataFormatInteger);
    return result;
}

FPRReg SpeculativeJIT::fillSpeculateDouble(NodeIndex nodeIndex)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("SpecDouble@%d   ", nodeIndex);
#endif
    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {
        if (node.hasConstant()) {
            GPRReg gpr = allocate();

            if (isInt32Constant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(static_cast<double>(valueOfInt32Constant(nodeIndex))))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            }
            if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfNumberConstant(nodeIndex)))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(*m_stream, fpr);
                return fpr;
            }
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
            return fprAllocate();
        }
        
        DataFormat spillFormat = info.spillFormat();
        switch (spillFormat) {
        case DataFormatDouble: {
            FPRReg fpr = fprAllocate();
            m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
            m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
            info.fillDouble(*m_stream, fpr);
            return fpr;
        }
            
        case DataFormatInteger: {
            GPRReg gpr = allocate();
            
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillInteger(*m_stream, gpr);
            unlock(gpr);
            break;
        }

        default:
            GPRReg gpr = allocate();

            ASSERT(spillFormat & DataFormatJS);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(*m_stream, gpr, spillFormat);
            unlock(gpr);
            break;
        }
    }

    switch (info.registerFormat()) {
    case DataFormatNone: // Should have filled, above.
    case DataFormatBoolean: // This type never occurs.
    case DataFormatStorage:
        ASSERT_NOT_REACHED();

    case DataFormatCell:
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
        return fprAllocate();

    case DataFormatJSCell:
    case DataFormatJS:
    case DataFormatJSBoolean: {
        GPRReg jsValueGpr = info.gpr();
        m_gprs.lock(jsValueGpr);
        FPRReg fpr = fprAllocate();
        GPRReg tempGpr = allocate();

        JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);

        if (!isNumberSpeculation(type))
            speculationCheck(BadType, JSValueRegs(jsValueGpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::Zero, jsValueGpr, GPRInfo::tagTypeNumberRegister));

        // First, if we get here we have a double encoded as a JSValue
        m_jit.move(jsValueGpr, tempGpr);
        unboxDouble(tempGpr, fpr);
        JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

        // Finally, handle integers.
        isInteger.link(&m_jit);
        m_jit.convertInt32ToDouble(jsValueGpr, fpr);
        hasUnboxedDouble.link(&m_jit);

        m_gprs.release(jsValueGpr);
        m_gprs.unlock(jsValueGpr);
        m_gprs.unlock(tempGpr);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(*m_stream, fpr);
        info.killSpilled();
        return fpr;
    }

    case DataFormatJSInteger:
    case DataFormatInteger: {
        FPRReg fpr = fprAllocate();
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        m_jit.convertInt32ToDouble(gpr, fpr);
        m_gprs.unlock(gpr);
        return fpr;
    }

    // Unbox the double
    case DataFormatJSDouble: {
        GPRReg gpr = info.gpr();
        FPRReg fpr = fprAllocate();
        if (m_gprs.isLocked(gpr)) {
            // Make sure we don't trample gpr if it is in use.
            GPRReg temp = allocate();
            m_jit.move(gpr, temp);
            unboxDouble(temp, fpr);
            unlock(temp);
        } else
            unboxDouble(gpr, fpr);

        m_gprs.release(gpr);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);

        info.fillDouble(*m_stream, fpr);
        return fpr;
    }

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

GPRReg SpeculativeJIT::fillSpeculateCell(NodeIndex nodeIndex, bool isForwardSpeculation)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("SpecCell@%d   ", nodeIndex);
#endif
    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (info.spillFormat() == DataFormatInteger || info.spillFormat() == DataFormatDouble) {
            terminateSpeculativeExecutionWithConditionalDirection(Uncountable, JSValueRegs(), NoNode, isForwardSpeculation);
            return allocate();
        }
        
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isCell()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImmPtr(jsValue.asCell()), gpr);
                info.fillJSValue(*m_stream, gpr, DataFormatJSCell);
                return gpr;
            }
            terminateSpeculativeExecutionWithConditionalDirection(Uncountable, JSValueRegs(), NoNode, isForwardSpeculation);
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        info.fillJSValue(*m_stream, gpr, DataFormatJS);
        if (!isCellSpeculation(type))
            speculationCheckWithConditionalDirection(BadType, JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagMaskRegister), isForwardSpeculation);
        info.fillJSValue(*m_stream, gpr, DataFormatJSCell);
        return gpr;
    }

    case DataFormatCell:
    case DataFormatJSCell: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }

    case DataFormatJS: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (!isCellSpeculation(type))
            speculationCheckWithConditionalDirection(BadType, JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagMaskRegister), isForwardSpeculation);
        info.fillJSValue(*m_stream, gpr, DataFormatJSCell);
        return gpr;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJSBoolean:
    case DataFormatBoolean: {
        terminateSpeculativeExecutionWithConditionalDirection(Uncountable, JSValueRegs(), NoNode, isForwardSpeculation);
        return allocate();
    }

    case DataFormatStorage:
        ASSERT_NOT_REACHED();
        
    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateBoolean(NodeIndex nodeIndex)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("SpecBool@%d   ", nodeIndex);
#endif
    SpeculatedType type = m_state.forNode(nodeIndex).m_type;
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (info.spillFormat() == DataFormatInteger || info.spillFormat() == DataFormatDouble) {
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
            return allocate();
        }
        
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isBoolean()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsValue)), gpr);
                info.fillJSValue(*m_stream, gpr, DataFormatJSBoolean);
                return gpr;
            }
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        info.fillJSValue(*m_stream, gpr, DataFormatJS);
        if (!isBooleanSpeculation(type)) {
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
            speculationCheck(BadType, JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, TrustedImm32(static_cast<int32_t>(~1))), SpeculationRecovery(BooleanSpeculationCheck, gpr, InvalidGPRReg));
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
        }
        info.fillJSValue(*m_stream, gpr, DataFormatJSBoolean);
        return gpr;
    }

    case DataFormatBoolean:
    case DataFormatJSBoolean: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }

    case DataFormatJS: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (!isBooleanSpeculation(type)) {
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
            speculationCheck(BadType, JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, TrustedImm32(static_cast<int32_t>(~1))), SpeculationRecovery(BooleanSpeculationCheck, gpr, InvalidGPRReg));
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
        }
        info.fillJSValue(*m_stream, gpr, DataFormatJSBoolean);
        return gpr;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJSCell:
    case DataFormatCell: {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), NoNode);
        return allocate();
    }
        
    case DataFormatStorage:
        ASSERT_NOT_REACHED();
        
    default:
        ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

JITCompiler::Jump SpeculativeJIT::convertToDouble(GPRReg value, FPRReg result, GPRReg tmp)
{
    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, value, GPRInfo::tagTypeNumberRegister);
    
    JITCompiler::Jump notNumber = m_jit.branchTestPtr(MacroAssembler::Zero, value, GPRInfo::tagTypeNumberRegister);
    
    m_jit.move(value, tmp);
    unboxDouble(tmp, result);
    
    JITCompiler::Jump done = m_jit.jump();
    
    isInteger.link(&m_jit);
    
    m_jit.convertInt32ToDouble(value, result);
    
    done.link(&m_jit);

    return notNumber;
}

void SpeculativeJIT::compileObjectEquality(Node& node, const ClassInfo* classInfo, SpeculatedTypeChecker speculatedTypeChecker)
{
    SpeculateCellOperand op1(this, node.child1());
    SpeculateCellOperand op2(this, node.child2());
    GPRTemporary result(this, op1);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();
    
    if (!speculatedTypeChecker(m_state.forNode(node.child1()).m_type))
        speculationCheck(BadType, JSValueRegs(op1GPR), node.child1().index(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op1GPR, JSCell::classInfoOffset()), MacroAssembler::TrustedImmPtr(classInfo)));
    if (!speculatedTypeChecker(m_state.forNode(node.child2()).m_type))
        speculationCheck(BadType, JSValueRegs(op2GPR), node.child2().index(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op2GPR, JSCell::classInfoOffset()), MacroAssembler::TrustedImmPtr(classInfo)));
    
    MacroAssembler::Jump falseCase = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, op2GPR);
    m_jit.move(TrustedImm32(ValueTrue), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();
    falseCase.link(&m_jit);
    m_jit.move(TrustedImm32(ValueFalse), resultGPR);
    done.link(&m_jit);

    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
}

void SpeculativeJIT::compileObjectToObjectOrOtherEquality(
    Edge leftChild, Edge rightChild,
    const ClassInfo* classInfo, SpeculatedTypeChecker speculatedTypeChecker)
{
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();
    
    if (!speculatedTypeChecker(m_state.forNode(leftChild).m_type)) {
        speculationCheck(
            BadType, JSValueRegs(op1GPR), leftChild.index(),
            m_jit.branchPtr(
                MacroAssembler::NotEqual,
                MacroAssembler::Address(op1GPR, JSCell::classInfoOffset()),
                MacroAssembler::TrustedImmPtr(classInfo)));
    }
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    MacroAssembler::Jump rightNotCell =
        m_jit.branchTestPtr(MacroAssembler::NonZero, op2GPR, GPRInfo::tagMaskRegister);
    
    // We know that within this branch, rightChild must be a cell. If the CFA can tell us that the
    // proof, when filtered on cell, demonstrates that we have an object of the desired type
    // (speculationCheck() will test for FinalObject or Array, currently), then we can skip the
    // speculation.
    if (!speculatedTypeChecker(m_state.forNode(rightChild).m_type & SpecCell)) {
        speculationCheck(
            BadType, JSValueRegs(op2GPR), rightChild.index(),
            m_jit.branchPtr(
                MacroAssembler::NotEqual,
                MacroAssembler::Address(op2GPR, JSCell::classInfoOffset()),
                MacroAssembler::TrustedImmPtr(classInfo)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    MacroAssembler::Jump falseCase = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, op2GPR);
    MacroAssembler::Jump trueCase = m_jit.jump();
    
    rightNotCell.link(&m_jit);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (!isOtherOrEmptySpeculation(m_state.forNode(rightChild).m_type & ~SpecCell)) {
        m_jit.move(op2GPR, resultGPR);
        m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), resultGPR);
        
        speculationCheck(
            BadType, JSValueRegs(op2GPR), rightChild.index(),
            m_jit.branchPtr(
                MacroAssembler::NotEqual, resultGPR,
                MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
    }
    
    falseCase.link(&m_jit);
    m_jit.move(TrustedImm32(ValueFalse), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();
    trueCase.link(&m_jit);
    m_jit.move(TrustedImm32(ValueTrue), resultGPR);
    done.link(&m_jit);
    
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
}

void SpeculativeJIT::compilePeepHoleObjectToObjectOrOtherEquality(
    Edge leftChild, Edge rightChild, NodeIndex branchNodeIndex,
    const ClassInfo* classInfo, SpeculatedTypeChecker speculatedTypeChecker)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();
    
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();
    
    if (!speculatedTypeChecker(m_state.forNode(leftChild).m_type)) {
        speculationCheck(
            BadType, JSValueRegs(op1GPR), leftChild.index(),
            m_jit.branchPtr(
                MacroAssembler::NotEqual,
                MacroAssembler::Address(op1GPR, JSCell::classInfoOffset()),
                MacroAssembler::TrustedImmPtr(classInfo)));
    }
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    MacroAssembler::Jump rightNotCell =
        m_jit.branchTestPtr(MacroAssembler::NonZero, op2GPR, GPRInfo::tagMaskRegister);
    
    // We know that within this branch, rightChild must be a cell. If the CFA can tell us that the
    // proof, when filtered on cell, demonstrates that we have an object of the desired type
    // (speculationCheck() will test for FinalObject or Array, currently), then we can skip the
    // speculation.
    if (!speculatedTypeChecker(m_state.forNode(rightChild).m_type & SpecCell)) {
        speculationCheck(
            BadType, JSValueRegs(op2GPR), rightChild.index(),
            m_jit.branchPtr(
                MacroAssembler::NotEqual,
                MacroAssembler::Address(op2GPR, JSCell::classInfoOffset()),
                MacroAssembler::TrustedImmPtr(classInfo)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    branchPtr(MacroAssembler::Equal, op1GPR, op2GPR, taken);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (isOtherOrEmptySpeculation(m_state.forNode(rightChild).m_type & ~SpecCell))
        rightNotCell.link(&m_jit);
    else {
        jump(notTaken, ForceJump);
        
        rightNotCell.link(&m_jit);
        m_jit.move(op2GPR, resultGPR);
        m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), resultGPR);
        
        speculationCheck(
            BadType, JSValueRegs(op2GPR), rightChild.index(),
            m_jit.branchPtr(
                MacroAssembler::NotEqual, resultGPR,
                MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
    }
    
    jump(notTaken);
}

void SpeculativeJIT::compileIntegerCompare(Node& node, MacroAssembler::RelationalCondition condition)
{
    SpeculateIntegerOperand op1(this, node.child1());
    SpeculateIntegerOperand op2(this, node.child2());
    GPRTemporary result(this, op1, op2);
    
    m_jit.compare32(condition, op1.gpr(), op2.gpr(), result.gpr());
    
    // If we add a DataFormatBool, we should use it here.
    m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
    jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
}

void SpeculativeJIT::compileDoubleCompare(Node& node, MacroAssembler::DoubleCondition condition)
{
    SpeculateDoubleOperand op1(this, node.child1());
    SpeculateDoubleOperand op2(this, node.child2());
    GPRTemporary result(this);
    
    m_jit.move(TrustedImm32(ValueTrue), result.gpr());
    MacroAssembler::Jump trueCase = m_jit.branchDouble(condition, op1.fpr(), op2.fpr());
    m_jit.xorPtr(TrustedImm32(true), result.gpr());
    trueCase.link(&m_jit);
    
    jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
}

void SpeculativeJIT::compileValueAdd(Node& node)
{
    JSValueOperand op1(this, node.child1());
    JSValueOperand op2(this, node.child2());
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    flushRegisters();
    
    GPRResult result(this);
    if (isKnownNotNumber(node.child1().index()) || isKnownNotNumber(node.child2().index()))
        callOperation(operationValueAddNotNumber, result.gpr(), op1GPR, op2GPR);
    else
        callOperation(operationValueAdd, result.gpr(), op1GPR, op2GPR);
    
    jsValueResult(result.gpr(), m_compileIndex);
}

void SpeculativeJIT::compileObjectOrOtherLogicalNot(Edge nodeUse, const ClassInfo* classInfo, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeUse);
    GPRTemporary result(this);
    GPRReg valueGPR = value.gpr();
    GPRReg resultGPR = result.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, valueGPR, GPRInfo::tagMaskRegister);
    if (needSpeculationCheck)
        speculationCheck(BadType, JSValueRegs(valueGPR), nodeUse, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(valueGPR, JSCell::classInfoOffset()), MacroAssembler::TrustedImmPtr(classInfo)));
    m_jit.move(TrustedImm32(static_cast<int32_t>(ValueFalse)), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();
    
    notCell.link(&m_jit);
    
    if (needSpeculationCheck) {
        m_jit.move(valueGPR, resultGPR);
        m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), resultGPR);
        speculationCheck(BadType, JSValueRegs(valueGPR), nodeUse, m_jit.branchPtr(MacroAssembler::NotEqual, resultGPR, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
    }
    m_jit.move(TrustedImm32(static_cast<int32_t>(ValueTrue)), resultGPR);
    
    done.link(&m_jit);
    
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
}

void SpeculativeJIT::compileLogicalNot(Node& node)
{
    if (at(node.child1()).shouldSpeculateFinalObjectOrOther()) {
        compileObjectOrOtherLogicalNot(node.child1(), &JSFinalObject::s_info, !isFinalObjectOrOtherSpeculation(m_state.forNode(node.child1()).m_type));
        return;
    }
    if (at(node.child1()).shouldSpeculateArrayOrOther()) {
        compileObjectOrOtherLogicalNot(node.child1(), &JSArray::s_info, !isArrayOrOtherSpeculation(m_state.forNode(node.child1()).m_type));
        return;
    }
    if (at(node.child1()).shouldSpeculateInteger()) {
        SpeculateIntegerOperand value(this, node.child1());
        GPRTemporary result(this, value);
        m_jit.compare32(MacroAssembler::Equal, value.gpr(), MacroAssembler::TrustedImm32(0), result.gpr());
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        return;
    }
    if (at(node.child1()).shouldSpeculateNumber()) {
        SpeculateDoubleOperand value(this, node.child1());
        FPRTemporary scratch(this);
        GPRTemporary result(this);
        m_jit.move(TrustedImm32(ValueFalse), result.gpr());
        MacroAssembler::Jump nonZero = m_jit.branchDoubleNonZero(value.fpr(), scratch.fpr());
        m_jit.xor32(TrustedImm32(true), result.gpr());
        nonZero.link(&m_jit);
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        return;
    }
    
    SpeculatedType prediction = m_jit.getSpeculation(node.child1());
    if (isBooleanSpeculation(prediction)) {
        if (isBooleanSpeculation(m_state.forNode(node.child1()).m_type)) {
            SpeculateBooleanOperand value(this, node.child1());
            GPRTemporary result(this, value);
            
            m_jit.move(value.gpr(), result.gpr());
            m_jit.xorPtr(TrustedImm32(true), result.gpr());
            
            jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
            return;
        }
        
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this); // FIXME: We could reuse, but on speculation fail would need recovery to restore tag (akin to add).
        
        m_jit.move(value.gpr(), result.gpr());
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), result.gpr());
        speculationCheck(BadType, JSValueRegs(value.gpr()), node.child1(), m_jit.branchTestPtr(JITCompiler::NonZero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueTrue)), result.gpr());
        
        // If we add a DataFormatBool, we should use it here.
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        return;
    }
    
    JSValueOperand arg1(this, node.child1());
    GPRTemporary result(this);
    
    GPRReg arg1GPR = arg1.gpr();
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    
    m_jit.move(arg1GPR, resultGPR);
    m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), resultGPR);
    JITCompiler::Jump slowCase = m_jit.branchTestPtr(JITCompiler::NonZero, resultGPR, TrustedImm32(static_cast<int32_t>(~1)));
    
    addSlowPathGenerator(
        slowPathCall(slowCase, this, dfgConvertJSValueToBoolean, resultGPR, arg1GPR));
    
    m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueTrue)), resultGPR);
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitObjectOrOtherBranch(Edge nodeUse, BlockIndex taken, BlockIndex notTaken, const ClassInfo* classInfo, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeUse);
    GPRTemporary scratch(this);
    GPRReg valueGPR = value.gpr();
    GPRReg scratchGPR = scratch.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, valueGPR, GPRInfo::tagMaskRegister);
    if (needSpeculationCheck)
        speculationCheck(BadType, JSValueRegs(valueGPR), nodeUse.index(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(valueGPR, JSCell::classInfoOffset()), MacroAssembler::TrustedImmPtr(classInfo)));
    jump(taken, ForceJump);
    
    notCell.link(&m_jit);
    
    if (needSpeculationCheck) {
        m_jit.move(valueGPR, scratchGPR);
        m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), scratchGPR);
        speculationCheck(BadType, JSValueRegs(valueGPR), nodeUse.index(), m_jit.branchPtr(MacroAssembler::NotEqual, scratchGPR, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
    }
    jump(notTaken);
    
    noResult(m_compileIndex);
}

void SpeculativeJIT::emitBranch(Node& node)
{
    BlockIndex taken = node.takenBlockIndex();
    BlockIndex notTaken = node.notTakenBlockIndex();
    
    if (at(node.child1()).shouldSpeculateFinalObjectOrOther()) {
        emitObjectOrOtherBranch(node.child1(), taken, notTaken, &JSFinalObject::s_info, !isFinalObjectOrOtherSpeculation(m_state.forNode(node.child1()).m_type));
    } else if (at(node.child1()).shouldSpeculateArrayOrOther()) {
        emitObjectOrOtherBranch(node.child1(), taken, notTaken, &JSArray::s_info, !isArrayOrOtherSpeculation(m_state.forNode(node.child1()).m_type));
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
        GPRReg valueGPR = value.gpr();
        
        bool predictBoolean = isBooleanSpeculation(m_jit.getSpeculation(node.child1()));
    
        if (predictBoolean) {
            if (isBooleanSpeculation(m_state.forNode(node.child1()).m_type)) {
                MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;
                
                if (taken == nextBlock()) {
                    condition = MacroAssembler::Zero;
                    BlockIndex tmp = taken;
                    taken = notTaken;
                    notTaken = tmp;
                }
                
                branchTest32(condition, valueGPR, TrustedImm32(true), taken);
                jump(notTaken);
            } else {
                branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(false))), notTaken);
                branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(true))), taken);
                
                speculationCheck(BadType, JSValueRegs(valueGPR), node.child1(), m_jit.jump());
            }
            value.use();
        } else {
            GPRTemporary result(this);
            GPRReg resultGPR = result.gpr();
        
            branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::TrustedImmPtr(JSValue::encode(jsNumber(0))), notTaken);
            branchPtr(MacroAssembler::AboveOrEqual, valueGPR, GPRInfo::tagTypeNumberRegister, taken);
    
            if (!predictBoolean) {
                branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(false))), notTaken);
                branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(true))), taken);
            }
    
            value.use();
    
            silentSpillAllRegisters(resultGPR);
            callOperation(dfgConvertJSValueToBoolean, resultGPR, valueGPR);
            silentFillAllRegisters(resultGPR);
    
            branchTest32(MacroAssembler::NonZero, resultGPR, taken);
            jump(notTaken);
        }
        
        noResult(m_compileIndex, UseChildrenCalledExplicitly);
    }
}

void SpeculativeJIT::compile(Node& node)
{
    NodeType op = node.op();

    switch (op) {
    case JSConstant:
        initConstantInfo(m_compileIndex);
        break;

    case PhantomArguments:
        // This should never be must-generate.
        ASSERT_NOT_REACHED();
        // But as a release-mode fall-back make it the empty value.
        initConstantInfo(m_compileIndex);
        break;

    case WeakJSConstant:
        m_jit.addWeakReference(node.weakConstant());
        initConstantInfo(m_compileIndex);
        break;

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
        }

        GPRTemporary result(this);
        m_jit.loadPtr(JITCompiler::addressFor(node.local()), result.gpr());

        // Like jsValueResult, but don't useChildren - our children are phi nodes,
        // and don't represent values within this dataflow with virtual registers.
        VirtualRegister virtualRegister = node.virtualRegister();
        m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);

        DataFormat format;
        if (node.variableAccessData()->isCaptured())
            format = DataFormatJS;
        else if (isCellSpeculation(value.m_type))
            format = DataFormatJSCell;
        else if (isBooleanSpeculation(value.m_type))
            format = DataFormatJSBoolean;
        else
            format = DataFormatJS;

        m_generationInfo[virtualRegister].initJSValue(m_compileIndex, node.refCount(), result.gpr(), format);
        break;
    }

    case GetLocalUnlinked: {
        GPRTemporary result(this);
        
        m_jit.loadPtr(JITCompiler::addressFor(node.unlinkedLocal()), result.gpr());
        
        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case SetLocal: {
        // SetLocal doubles as a hint as to where a node will be stored and
        // as a speculation point. So before we speculate make sure that we
        // know where the child of this node needs to go in the virtual
        // register file.
        compileMovHint(node);
        
        // As far as OSR is concerned, we're on the bytecode index corresponding
        // to the *next* instruction, since we've already "executed" the
        // SetLocal and whatever other DFG Nodes are associated with the same
        // bytecode index as the SetLocal.
        ASSERT(m_codeOriginForOSR == node.codeOrigin);
        Node* nextNode = &at(block()->at(m_indexInBlock + 1));

        // But even more oddly, we need to be super careful about the following
        // sequence:
        //
        // a: Foo()
        // b: SetLocal(@a)
        // c: Flush(@b)
        //
        // This next piece of crazy takes care of this.
        if (nextNode->op() == Flush && nextNode->child1() == m_compileIndex)
            nextNode = &at(block()->at(m_indexInBlock + 2));
        
        // Oddly, it's possible for the bytecode index for the next node to be
        // equal to ours. This will happen for op_post_inc. And, even more oddly,
        // this is just fine. Ordinarily, this wouldn't be fine, since if the
        // next node failed OSR then we'd be OSR-ing with this SetLocal's local
        // variable already set even though from the standpoint of the old JIT,
        // this SetLocal should not have executed. But for op_post_inc, it's just
        // fine, because this SetLocal's local (i.e. the LHS in a x = y++
        // statement) would be dead anyway - so the fact that DFG would have
        // already made the assignment, and baked it into the register file during
        // OSR exit, would not be visible to the old JIT in any way.
        m_codeOriginForOSR = nextNode->codeOrigin;
        
        if (!node.variableAccessData()->isCaptured() && !m_jit.graph().isCreatedThisArgument(node.local())) {
            if (node.variableAccessData()->shouldUseDoubleFormat()) {
                SpeculateDoubleOperand value(this, node.child1());
                m_jit.storeDouble(value.fpr(), JITCompiler::addressFor(node.local()));
                noResult(m_compileIndex);
                // Indicate that it's no longer necessary to retrieve the value of
                // this bytecode variable from registers or other locations in the register file,
                // but that it is stored as a double.
                recordSetLocal(node.local(), ValueSource(DoubleInRegisterFile));
                break;
            }
        
            SpeculatedType predictedType = node.variableAccessData()->argumentAwarePrediction();
            if (isInt32Speculation(predictedType)) {
                SpeculateIntegerOperand value(this, node.child1());
                m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(Int32InRegisterFile));
                break;
            }
            if (isCellSpeculation(predictedType)) {
                SpeculateCellOperand cell(this, node.child1());
                GPRReg cellGPR = cell.gpr();
                m_jit.storePtr(cellGPR, JITCompiler::addressFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(CellInRegisterFile));
                break;
            }
            if (isBooleanSpeculation(predictedType)) {
                SpeculateBooleanOperand boolean(this, node.child1());
                m_jit.storePtr(boolean.gpr(), JITCompiler::addressFor(node.local()));
                noResult(m_compileIndex);
                recordSetLocal(node.local(), ValueSource(BooleanInRegisterFile));
                break;
            }
        }
        
        JSValueOperand value(this, node.child1());
        m_jit.storePtr(value.gpr(), JITCompiler::addressFor(node.local()));
        noResult(m_compileIndex);

        recordSetLocal(node.local(), ValueSource(ValueInRegisterFile));
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
            JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, op1.gpr(), GPRInfo::tagTypeNumberRegister);
            speculationCheck(
                BadType, JSValueRegs(op1.gpr()), node.child1().index(),
                m_jit.branchTestPtr(MacroAssembler::Zero, op1.gpr(), GPRInfo::tagTypeNumberRegister));
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
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
            compileIntegerArithDivForX86(node);
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
        if (at(node.child1()).shouldSpeculateInteger() && node.canSpeculateInteger()) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this);
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
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
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

    case GetByVal: {
        if (!node.prediction() || !at(node.child1()).prediction() || !at(node.child2()).prediction()) {
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        }
        
        if (!at(node.child2()).shouldSpeculateInteger() || (!node.child3() && !at(node.child1()).shouldSpeculateArguments())) {
            JSValueOperand base(this, node.child1());
            JSValueOperand property(this, node.child2());
            GPRReg baseGPR = base.gpr();
            GPRReg propertyGPR = property.gpr();
            
            flushRegisters();
            GPRResult result(this);
            callOperation(operationGetByVal, result.gpr(), baseGPR, propertyGPR);
            
            jsValueResult(result.gpr(), m_compileIndex);
            break;
        }
        
        if (at(node.child1()).shouldSpeculateArguments()) {
            compileGetByValOnArguments(node);
            if (!m_compileOkay)
                return;
            break;
        }
        
        if (at(node.child1()).prediction() == SpecString) {
            compileGetByValOnString(node);
            if (!m_compileOkay)
                return;
            break;
        }

        if (at(node.child1()).shouldSpeculateInt8Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), node, sizeof(int8_t), isInt8ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt16Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), node, sizeof(int16_t), isInt16ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt32Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), node, sizeof(int32_t), isInt32ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }

        if (at(node.child1()).shouldSpeculateUint8Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), node, sizeof(uint8_t), isUint8ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }

        if (at(node.child1()).shouldSpeculateUint8ClampedArray()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint8ClampedArrayDescriptor(), node, sizeof(uint8_t), isUint8ClampedArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;
        }

        if (at(node.child1()).shouldSpeculateUint16Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), node, sizeof(uint16_t), isUint16ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint32Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), node, sizeof(uint32_t), isUint32ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat32Array()) {
            compileGetByValOnFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), node, sizeof(float), isFloat32ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat64Array()) {
            compileGetByValOnFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), node, sizeof(double), isFloat64ArraySpeculation(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
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

        // We will have already speculated that the base is some kind of array,
        // at this point.
        
        speculationCheck(Uncountable, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset())));

        GPRTemporary result(this);
        m_jit.loadPtr(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), result.gpr());
        speculationCheck(Uncountable, JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::Zero, result.gpr()));

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case PutByVal:
    case PutByValSafe: {
        Edge child1 = m_jit.graph().varArgChild(node, 0);
        Edge child2 = m_jit.graph().varArgChild(node, 1);
        Edge child3 = m_jit.graph().varArgChild(node, 2);
        
        if (!at(child1).prediction() || !at(child2).prediction()) {
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        }
        
        if (!at(child2).shouldSpeculateInteger()) {
            JSValueOperand arg1(this, child1);
            JSValueOperand arg2(this, child2);
            JSValueOperand arg3(this, child3);
            GPRReg arg1GPR = arg1.gpr();
            GPRReg arg2GPR = arg2.gpr();
            GPRReg arg3GPR = arg3.gpr();
            flushRegisters();
            
            callOperation(m_jit.strictModeFor(node.codeOrigin) ? operationPutByValStrict : operationPutByValNonStrict, arg1GPR, arg2GPR, arg3GPR);
            
            noResult(m_compileIndex);
            break;
        }

        SpeculateCellOperand base(this, child1);
        SpeculateStrictInt32Operand property(this, child2);
        if (at(child1).shouldSpeculateArguments()) {
            dataLog(" in here ");
            JSValueOperand value(this, child3);
            GPRTemporary scratch(this);
            GPRTemporary scratch2(this);
            
            GPRReg baseReg = base.gpr();
            GPRReg propertyReg = property.gpr();
            GPRReg valueReg = value.gpr();
            GPRReg scratchReg = scratch.gpr();
            GPRReg scratch2Reg = scratch2.gpr();
            
            if (!m_compileOkay)
                return;

            if (!isArgumentsSpeculation(m_state.forNode(child1).m_type)) {
                speculationCheck(
                    BadType, JSValueSource::unboxedCell(baseReg), child1,
                    m_jit.branchPtr(
                        MacroAssembler::NotEqual,
                        MacroAssembler::Address(baseReg, JSCell::classInfoOffset()),
                        MacroAssembler::TrustedImmPtr(&Arguments::s_info)));
            }
    
            m_jit.loadPtr(
                MacroAssembler::Address(baseReg, Arguments::offsetOfData()),
                scratchReg);

            // Two really lame checks.
            speculationCheck(
                Uncountable, JSValueSource(), NoNode,
                m_jit.branchPtr(
                    MacroAssembler::AboveOrEqual, propertyReg,
                    MacroAssembler::Address(scratchReg, OBJECT_OFFSETOF(ArgumentsData, numArguments))));
            speculationCheck(
                Uncountable, JSValueSource(), NoNode,
                m_jit.branchTestPtr(
                    MacroAssembler::NonZero,
                    MacroAssembler::Address(
                        scratchReg, OBJECT_OFFSETOF(ArgumentsData, deletedArguments))));
    
            m_jit.move(propertyReg, scratch2Reg);
            m_jit.neg32(scratch2Reg);
            m_jit.signExtend32ToPtr(scratch2Reg, scratch2Reg);
            m_jit.loadPtr(
                MacroAssembler::Address(scratchReg, OBJECT_OFFSETOF(ArgumentsData, registers)),
                scratchReg);
            
            m_jit.storePtr(
                valueReg,
                MacroAssembler::BaseIndex(
                    scratchReg, scratch2Reg, MacroAssembler::TimesEight,
                    CallFrame::thisArgumentOffset() * sizeof(Register) - sizeof(Register)));
            
            noResult(m_compileIndex);
            break;
        }
        
        if (at(child1).shouldSpeculateInt8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int8_t), isInt8ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateInt16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int16_t), isInt16ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }

        if (at(child1).shouldSpeculateInt32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int32_t), isInt32ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateUint8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), isUint8ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }

        if (at(child1).shouldSpeculateUint8ClampedArray()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ClampedArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), isUint8ClampedArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray, ClampRounding);
            break;
        }

        if (at(child1).shouldSpeculateUint16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint16_t), isUint16ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateUint32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint32_t), isUint32ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateFloat32Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(float), isFloat32ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateFloat64Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(double), isFloat64ArraySpeculation(m_state.forNode(child1).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }
            
        JSValueOperand value(this, child3);
        GPRTemporary scratch(this);

        // Map base, property & value into registers, allocate a scratch register.
        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg valueReg = value.gpr();
        GPRReg scratchReg = scratch.gpr();
        
        if (!m_compileOkay)
            return;
        
        writeBarrier(baseReg, value.gpr(), child3, WriteBarrierForPropertyAccess, scratchReg);

        speculateArray(child1, baseReg);

        MacroAssembler::Jump beyondArrayBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset()));
        if (node.op() == PutByVal)
            speculationCheck(OutOfBounds, JSValueRegs(), NoNode, beyondArrayBounds);

        base.use();
        property.use();
        value.use();
        
        // Get the array storage.
        GPRReg storageReg = scratchReg;
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSArray::storageOffset()), storageReg);

        // Check if we're writing to a hole; if so increment m_numValuesInVector.
        MacroAssembler::Jump notHoleValue = m_jit.branchTestPtr(MacroAssembler::NonZero, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        m_jit.add32(TrustedImm32(1), MacroAssembler::Address(storageReg, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));

        // If we're writing to a hole we might be growing the array; 
        MacroAssembler::Jump lengthDoesNotNeedUpdate = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(storageReg, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.add32(TrustedImm32(1), propertyReg);
        m_jit.store32(propertyReg, MacroAssembler::Address(storageReg, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.sub32(TrustedImm32(1), propertyReg);

        lengthDoesNotNeedUpdate.link(&m_jit);
        notHoleValue.link(&m_jit);

        // Store the value to the array.
        m_jit.storePtr(valueReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));

        if (node.op() == PutByValSafe) {
            addSlowPathGenerator(
                slowPathCall(
                    beyondArrayBounds, this,
                    m_jit.codeBlock()->isStrictMode() ? operationPutByValBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsNonStrict,
                    NoResult, baseReg, propertyReg, valueReg));
        }

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByValAlias: {
        Edge child1 = m_jit.graph().varArgChild(node, 0);
        Edge child2 = m_jit.graph().varArgChild(node, 1);
        Edge child3 = m_jit.graph().varArgChild(node, 2);
        
        if (!at(child1).prediction() || !at(child2).prediction()) {
            terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
            break;
        }
        
        ASSERT(isActionableMutableArraySpeculation(at(child1).prediction()));
        ASSERT(at(child2).shouldSpeculateInteger());

        SpeculateCellOperand base(this, child1);
        SpeculateStrictInt32Operand property(this, child2);
        if (at(child1).shouldSpeculateInt8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int8_t), NoTypedArraySpecCheck, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateInt16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int16_t), NoTypedArraySpecCheck, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateInt32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int32_t), NoTypedArraySpecCheck, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateUint8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), NoTypedArraySpecCheck, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }

        if (at(child1).shouldSpeculateUint8ClampedArray()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ClampedArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), NoTypedArraySpecCheck, UnsignedTypedArray, ClampRounding);
            if (!m_compileOkay)
                return;
            break;
        }

        if (at(child1).shouldSpeculateUint16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint16_t), NoTypedArraySpecCheck, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateUint32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint32_t), NoTypedArraySpecCheck, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateFloat32Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(float), NoTypedArraySpecCheck);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(child1).shouldSpeculateFloat64Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(double), NoTypedArraySpecCheck);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        ASSERT(at(child1).shouldSpeculateArray());

        JSValueOperand value(this, child3);
        GPRTemporary scratch(this);
        
        GPRReg baseReg = base.gpr();
        GPRReg scratchReg = scratch.gpr();

        writeBarrier(base.gpr(), value.gpr(), child3, WriteBarrierForPropertyAccess, scratchReg);

        // Get the array storage.
        GPRReg storageReg = scratchReg;
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSArray::storageOffset()), storageReg);

        // Store the value to the array.
        GPRReg propertyReg = property.gpr();
        GPRReg valueReg = value.gpr();
        m_jit.storePtr(valueReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));

        noResult(m_compileIndex);
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
            jsValueResult(result.gpr(), m_compileIndex);
            break;
        }

        SpeculateCellOperand base(this, node.child1());
        SpeculateCellOperand argument(this, node.child2());
        GPRReg baseGPR = base.gpr();
        GPRReg argumentGPR = argument.gpr();
        
        flushRegisters();
        GPRResult result(this);
        callOperation(operationRegExpExec, result.gpr(), baseGPR, argumentGPR);
        
        jsValueResult(result.gpr(), m_compileIndex);
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
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }
        
    case ArrayPush: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary storage(this);
        GPRTemporary storageLength(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg storageLengthGPR = storageLength.gpr();
        
        writeBarrier(baseGPR, valueGPR, node.child2(), WriteBarrierForPropertyAccess, storageGPR, storageLengthGPR);

        speculateArray(node.child1(), baseGPR);

        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);
        m_jit.load32(MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), storageLengthGPR);
        
        // Refuse to handle bizarre lengths.
        speculationCheck(Uncountable, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Above, storageLengthGPR, TrustedImm32(0x7ffffffe)));
        
        MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        m_jit.storePtr(valueGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        
        m_jit.add32(TrustedImm32(1), storageLengthGPR);
        m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.add32(TrustedImm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, storageLengthGPR);
        
        addSlowPathGenerator(
            slowPathCall(
                slowPath, this, operationArrayPush, NoResult, storageLengthGPR,
                valueGPR, baseGPR));
        
        jsValueResult(storageLengthGPR, m_compileIndex);
        break;
    }
        
    case ArrayPop: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary value(this);
        GPRTemporary storage(this);
        GPRTemporary storageLength(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg storageLengthGPR = storageLength.gpr();
        
        speculateArray(node.child1(), baseGPR);

        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);
        m_jit.load32(MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), storageLengthGPR);
        
        JITCompiler::JumpList setUndefinedCases;
        setUndefinedCases.append(m_jit.branchTest32(MacroAssembler::Zero, storageLengthGPR));
        
        m_jit.sub32(TrustedImm32(1), storageLengthGPR);
        
        MacroAssembler::Jump slowCase = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        m_jit.loadPtr(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), valueGPR);
        
        m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));

        setUndefinedCases.append(m_jit.branchTestPtr(MacroAssembler::Zero, valueGPR));
        
        m_jit.storePtr(MacroAssembler::TrustedImmPtr(0), MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        m_jit.sub32(MacroAssembler::TrustedImm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
        addSlowPathGenerator(
            slowPathMove(
                setUndefinedCases, this,
                MacroAssembler::TrustedImmPtr(JSValue::encode(jsUndefined())), valueGPR));
        
        addSlowPathGenerator(
            slowPathCall(
                slowCase, this, operationArrayPop, valueGPR, baseGPR));

        jsValueResult(valueGPR, m_compileIndex);
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
        ASSERT(GPRInfo::callFrameRegister != GPRInfo::regT1);
        ASSERT(GPRInfo::regT1 != GPRInfo::returnValueGPR);
        ASSERT(GPRInfo::returnValueGPR != GPRInfo::callFrameRegister);

#if DFG_ENABLE(SUCCESS_STATS)
        static SamplingCounter counter("SpeculativeJIT");
        m_jit.emitCount(counter);
#endif

        // Return the result in returnValueGPR.
        JSValueOperand op1(this, node.child1());
        m_jit.move(op1.gpr(), GPRInfo::returnValueGPR);

        // Grab the return address.
        m_jit.emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, GPRInfo::regT1);
        // Restore our caller's "r".
        m_jit.emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, GPRInfo::callFrameRegister);
        // Return.
        m_jit.restoreReturnAddressBeforeReturn(GPRInfo::regT1);
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
            
            m_jit.move(op1.gpr(), result.gpr());
            if (op1.format() == DataFormatInteger)
                m_jit.orPtr(GPRInfo::tagTypeNumberRegister, result.gpr());
            
            jsValueResult(result.gpr(), m_compileIndex);
            break;
        }
        
        // FIXME: Add string speculation here.
        
        JSValueOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        
        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();
        
        op1.use();
        
        if (!(m_state.forNode(node.child1()).m_type & ~(SpecNumber | SpecBoolean)))
            m_jit.move(op1GPR, resultGPR);
        else {
            MacroAssembler::Jump alreadyPrimitive = m_jit.branchTestPtr(MacroAssembler::NonZero, op1GPR, GPRInfo::tagMaskRegister);
            MacroAssembler::Jump notPrimitive = m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op1GPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get()));
            
            alreadyPrimitive.link(&m_jit);
            m_jit.move(op1GPR, resultGPR);
            
            addSlowPathGenerator(
                slowPathCall(notPrimitive, this, operationToPrimitive, resultGPR, op1GPR));
        }
        
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }
        
    case NewArray: {
        if (!node.numChildren()) {
            flushRegisters();
            GPRResult result(this);
            callOperation(
                operationNewEmptyArray, result.gpr(),
                m_jit.graph().globalObjectFor(node.codeOrigin)->arrayStructure());
            cellResult(result.gpr(), m_compileIndex);
            break;
        }
        
        size_t scratchSize = sizeof(EncodedJSValue) * node.numChildren();
        ScratchBuffer* scratchBuffer = m_jit.globalData()->scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;
        
        for (unsigned operandIdx = 0; operandIdx < node.numChildren(); ++operandIdx) {
            JSValueOperand operand(this, m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx]);
            GPRReg opGPR = operand.gpr();
            operand.use();
            
            m_jit.storePtr(opGPR, buffer + operandIdx);
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
            operationNewArray, result.gpr(),
            m_jit.graph().globalObjectFor(node.codeOrigin)->arrayStructure(),
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
        SpeculateStrictInt32Operand size(this, node.child1());
        GPRReg sizeGPR = size.gpr();
        flushRegisters();
        GPRResult result(this);
        callOperation(operationNewArrayWithSize, result.gpr(), m_jit.graph().globalObjectFor(node.codeOrigin)->arrayStructure(), sizeGPR);
        cellResult(result.gpr(), m_compileIndex);
        break;
    }
        
    case StrCat: {
        size_t scratchSize = sizeof(EncodedJSValue) * node.numChildren();
        ScratchBuffer* scratchBuffer = m_jit.globalData()->scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;
        
        for (unsigned operandIdx = 0; operandIdx < node.numChildren(); ++operandIdx) {
            JSValueOperand operand(this, m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx]);
            GPRReg opGPR = operand.gpr();
            operand.use();
            
            m_jit.storePtr(opGPR, buffer + operandIdx);
        }
        
        flushRegisters();

        if (scratchSize) {
            GPRTemporary scratch(this);

            // Tell GC mark phase how much of the scratch buffer is active during call.
            m_jit.move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratch.gpr());
            m_jit.storePtr(TrustedImmPtr(scratchSize), scratch.gpr());
        }

        GPRResult result(this);
        
        callOperation(operationStrCat, result.gpr(), static_cast<void *>(buffer), node.numChildren());

        if (scratchSize) {
            GPRTemporary scratch(this);

            m_jit.move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratch.gpr());
            m_jit.storePtr(TrustedImmPtr(0), scratch.gpr());
        }

        cellResult(result.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }
        
    case NewArrayBuffer: {
        flushRegisters();
        GPRResult result(this);
        
        callOperation(operationNewArrayBuffer, result.gpr(), node.startConstant(), node.numConstants());
        
        cellResult(result.gpr(), m_compileIndex);
        break;
    }
        
    case NewRegexp: {
        flushRegisters();
        GPRResult result(this);
        
        callOperation(operationNewRegexp, result.gpr(), m_jit.codeBlock()->regexp(node.regexpIndex()));
        
        cellResult(result.gpr(), m_compileIndex);
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
            GPRTemporary scratch(this, thisValue);
            GPRReg thisValueGPR = thisValue.gpr();
            GPRReg scratchGPR = scratch.gpr();
            
            if (!isOtherSpeculation(m_state.forNode(node.child1()).m_type)) {
                m_jit.move(thisValueGPR, scratchGPR);
                m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), scratchGPR);
                speculationCheck(BadType, JSValueRegs(thisValueGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, scratchGPR, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
            }
            
            m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.globalThisObjectFor(node.codeOrigin)), scratchGPR);
            cellResult(scratchGPR, m_compileIndex);
            break;
        }
        
        if (isObjectSpeculation(at(node.child1()).prediction())) {
            SpeculateCellOperand thisValue(this, node.child1());
            GPRTemporary result(this, thisValue);
            GPRReg thisValueGPR = thisValue.gpr();
            GPRReg resultGPR = result.gpr();
            
            if (!isObjectSpeculation(m_state.forNode(node.child1()).m_type))
                speculationCheck(BadType, JSValueRegs(thisValueGPR), node.child1(), m_jit.branchPtr(JITCompiler::Equal, JITCompiler::Address(thisValueGPR, JSCell::structureOffset()), JITCompiler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
            
            m_jit.move(thisValueGPR, resultGPR);
            
            cellResult(resultGPR, m_compileIndex);
            break;
        }
        
        JSValueOperand thisValue(this, node.child1());
        GPRReg thisValueGPR = thisValue.gpr();
        
        flushRegisters();
        
        GPRResult result(this);
        callOperation(operationConvertThis, result.gpr(), thisValueGPR);
        
        cellResult(result.gpr(), m_compileIndex);
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
        GPRTemporary scratch(this);
        
        GPRReg calleeGPR = callee.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        // Load the inheritorID. If the inheritorID is not set, go to slow path.
        m_jit.loadPtr(MacroAssembler::Address(calleeGPR, JSFunction::offsetOfCachedInheritorID()), scratchGPR);
        MacroAssembler::JumpList slowPath;
        slowPath.append(m_jit.branchTestPtr(MacroAssembler::Zero, scratchGPR));
        
        emitAllocateJSFinalObject(scratchGPR, resultGPR, scratchGPR, slowPath);
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationCreateThis, resultGPR, calleeGPR));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }

    case NewObject: {
        GPRTemporary result(this);
        GPRTemporary scratch(this);
        
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        MacroAssembler::JumpList slowPath;
        
        emitAllocateJSFinalObject(MacroAssembler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)->emptyObjectStructure()), resultGPR, scratchGPR, slowPath);
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewObject, resultGPR));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }

    case GetCallee: {
        GPRTemporary result(this);
        m_jit.loadPtr(JITCompiler::addressFor(static_cast<VirtualRegister>(RegisterFile::Callee)), result.gpr());
        cellResult(result.gpr(), m_compileIndex);
        break;
    }

    case GetScopeChain: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        m_jit.loadPtr(JITCompiler::addressFor(static_cast<VirtualRegister>(RegisterFile::ScopeChain)), resultGPR);
        bool checkTopLevel = m_jit.codeBlock()->codeType() == FunctionCode && m_jit.codeBlock()->needsFullScopeChain();
        int skip = node.scopeChainDepth();
        ASSERT(skip || !checkTopLevel);
        if (checkTopLevel && skip--) {
            JITCompiler::Jump activationNotCreated;
            if (checkTopLevel)
                activationNotCreated = m_jit.branchTestPtr(JITCompiler::Zero, JITCompiler::addressFor(static_cast<VirtualRegister>(m_jit.codeBlock()->activationRegister())));
            m_jit.loadPtr(JITCompiler::Address(resultGPR, OBJECT_OFFSETOF(ScopeChainNode, next)), resultGPR);
            activationNotCreated.link(&m_jit);
        }
        while (skip--)
            m_jit.loadPtr(JITCompiler::Address(resultGPR, OBJECT_OFFSETOF(ScopeChainNode, next)), resultGPR);
        
        m_jit.loadPtr(JITCompiler::Address(resultGPR, OBJECT_OFFSETOF(ScopeChainNode, object)), resultGPR);

        cellResult(resultGPR, m_compileIndex);
        break;
    }
    case GetScopedVar: {
        SpeculateCellOperand scopeChain(this, node.child1());
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
        m_jit.loadPtr(JITCompiler::Address(scopeChain.gpr(), JSVariableObject::offsetOfRegisters()), resultGPR);
        m_jit.loadPtr(JITCompiler::Address(resultGPR, node.varNumber() * sizeof(Register)), resultGPR);
        jsValueResult(resultGPR, m_compileIndex);
        break;
    }
    case PutScopedVar: {
        SpeculateCellOperand scopeChain(this, node.child1());
        GPRTemporary scratchRegister(this);
        GPRReg scratchGPR = scratchRegister.gpr();
        m_jit.loadPtr(JITCompiler::Address(scopeChain.gpr(), JSVariableObject::offsetOfRegisters()), scratchGPR);
        JSValueOperand value(this, node.child2());
        m_jit.storePtr(value.gpr(), JITCompiler::Address(scratchGPR, node.varNumber() * sizeof(Register)));
        writeBarrier(scopeChain.gpr(), value.gpr(), node.child2(), WriteBarrierForVariableAccess, scratchGPR);
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
            GPRTemporary result(this, base);
            
            GPRReg baseGPR = base.gpr();
            GPRReg resultGPR = result.gpr();
            
            base.use();
            
            cachedGetById(node.codeOrigin, baseGPR, resultGPR, node.identifierNumber());
            
            jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
            break;
        }
        
        JSValueOperand base(this, node.child1());
        GPRTemporary result(this, base);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        base.use();
        
        JITCompiler::Jump notCell = m_jit.branchTestPtr(JITCompiler::NonZero, baseGPR, GPRInfo::tagMaskRegister);
        
        cachedGetById(node.codeOrigin, baseGPR, resultGPR, node.identifierNumber(), notCell);
        
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        
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

            GPRResult result(this);
            
            GPRReg resultGPR = result.gpr();
            
            base.use();
            
            flushRegisters();
            
            cachedGetById(node.codeOrigin, baseGPR, resultGPR, node.identifierNumber(), JITCompiler::Jump(), DontSpill);
            
            jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
            break;
        }
        
        JSValueOperand base(this, node.child1());
        GPRReg baseGPR = base.gpr();

        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
        
        base.use();
        flushRegisters();
        
        JITCompiler::Jump notCell = m_jit.branchTestPtr(JITCompiler::NonZero, baseGPR, GPRInfo::tagMaskRegister);
        
        cachedGetById(node.codeOrigin, baseGPR, resultGPR, node.identifierNumber(), notCell, DontSpill);
        
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        
        break;
    }

    case GetArrayLength: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        speculateArray(node.child1(), baseGPR);

        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), resultGPR);
        m_jit.load32(MacroAssembler::Address(resultGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), resultGPR);
        
        speculationCheck(Uncountable, JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, resultGPR, MacroAssembler::TrustedImm32(0)));
        
        integerResult(resultGPR, m_compileIndex);
        break;
    }
        
    case GetArgumentsLength: {
        compileGetArgumentsLength(node);
        break;
    }

    case GetStringLength: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        if (!isStringSpeculation(m_state.forNode(node.child1()).m_type))
            speculationCheck(BadType, JSValueRegs(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->stringStructure.get())));
        
        m_jit.load32(MacroAssembler::Address(baseGPR, JSString::offsetOfLength()), resultGPR);

        integerResult(resultGPR, m_compileIndex);
        break;
    }

    case GetInt8ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->int8ArrayDescriptor(), node, !isInt8ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetInt16ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->int16ArrayDescriptor(), node, !isInt16ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetInt32ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->int32ArrayDescriptor(), node, !isInt32ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint8ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint8ArrayDescriptor(), node, !isUint8ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint8ClampedArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint8ClampedArrayDescriptor(), node, !isUint8ClampedArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint16ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint16ArrayDescriptor(), node, !isUint16ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint32ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint32ArrayDescriptor(), node, !isUint32ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetFloat32ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->float32ArrayDescriptor(), node, !isFloat32ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetFloat64ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->float64ArrayDescriptor(), node, !isFloat64ArraySpeculation(m_state.forNode(node.child1()).m_type));
        break;
    }
    case CheckFunction: {
        SpeculateCellOperand function(this, node.child1());
        speculationCheck(BadCache, JSValueRegs(), NoNode, m_jit.branchWeakPtr(JITCompiler::NotEqual, function.gpr(), node.function()));
        noResult(m_compileIndex);
        break;
    }
    case CheckStructure:
    case ForwardCheckStructure: {
        AbstractValue& value = m_state.forNode(node.child1());
        if (value.m_structure.isSubsetOf(node.structureSet())
            && isCellSpeculation(value.m_type)) {
            noResult(m_compileIndex);
            break;
        }
        
        SpeculateCellOperand base(this, node.child1(), node.op() == ForwardCheckStructure);
        
        ASSERT(node.structureSet().size());
        
        if (node.structureSet().size() == 1) {
            speculationCheckWithConditionalDirection(
                BadCache, JSValueRegs(), NoNode,
                m_jit.branchWeakPtr(
                    JITCompiler::NotEqual,
                    JITCompiler::Address(base.gpr(), JSCell::structureOffset()),
                    node.structureSet()[0]),
                node.op() == ForwardCheckStructure);
        } else {
            GPRTemporary structure(this);
            
            m_jit.loadPtr(JITCompiler::Address(base.gpr(), JSCell::structureOffset()), structure.gpr());
            
            JITCompiler::JumpList done;
            
            for (size_t i = 0; i < node.structureSet().size() - 1; ++i)
                done.append(m_jit.branchWeakPtr(JITCompiler::Equal, structure.gpr(), node.structureSet()[i]));
            
            speculationCheckWithConditionalDirection(
                BadCache, JSValueRegs(), NoNode,
                m_jit.branchWeakPtr(
                    JITCompiler::NotEqual, structure.gpr(), node.structureSet().last()),
                node.op() == ForwardCheckStructure);
            
            done.link(&m_jit);
        }
        
        noResult(m_compileIndex);
        break;
    }
        
    case StructureTransitionWatchpoint: {
        m_jit.addWeakReference(node.structure());
        node.structure()->addTransitionWatchpoint(speculationWatchpoint(BadCache));

#if !ASSERT_DISABLED
        SpeculateCellOperand op1(this, node.child1());
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
        
    case GetPropertyStorage: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this, base);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.loadPtr(JITCompiler::Address(baseGPR, JSObject::offsetOfOutOfLineStorage()), resultGPR);
        
        storageResult(resultGPR, m_compileIndex);
        break;
    }

    case GetIndexedPropertyStorage: {
        compileGetIndexedPropertyStorage(node);
        break;
    }
        
    case GetByOffset: {
        StorageOperand storage(this, node.child1());
        GPRTemporary result(this, storage);
        
        GPRReg storageGPR = storage.gpr();
        GPRReg resultGPR = result.gpr();
        
        StorageAccessData& storageAccessData = m_jit.graph().m_storageAccessData[node.storageAccessDataIndex()];
        
        m_jit.loadPtr(JITCompiler::Address(storageGPR, storageAccessData.offset * sizeof(EncodedJSValue)), resultGPR);
        
        jsValueResult(resultGPR, m_compileIndex);
        break;
    }
        
    case PutByOffset: {
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
        SpeculateCellOperand base(this, node.child2());
#endif
        StorageOperand storage(this, node.child1());
        JSValueOperand value(this, node.child3());

        GPRReg storageGPR = storage.gpr();
        GPRReg valueGPR = value.gpr();
        
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
        writeBarrier(base.gpr(), value.gpr(), node.child3(), WriteBarrierForPropertyAccess);
#endif

        StorageAccessData& storageAccessData = m_jit.graph().m_storageAccessData[node.storageAccessDataIndex()];
        
        m_jit.storePtr(valueGPR, JITCompiler::Address(storageGPR, storageAccessData.offset * sizeof(EncodedJSValue)));
        
        noResult(m_compileIndex);
        break;
    }
        
    case PutById: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        base.use();
        value.use();

        cachedPutById(node.codeOrigin, baseGPR, valueGPR, node.child2(), scratchGPR, node.identifierNumber(), NotDirect);
        
        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByIdDirect: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        base.use();
        value.use();

        cachedPutById(node.codeOrigin, baseGPR, valueGPR, node.child2(), scratchGPR, node.identifierNumber(), Direct);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetGlobalVar: {
        GPRTemporary result(this);

        m_jit.loadPtr(node.registerPointer(), result.gpr());

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case PutGlobalVar: {
        JSValueOperand value(this, node.child1());
        
        if (Heap::isWriteBarrierEnabled()) {
            GPRTemporary scratch(this);
            GPRReg scratchReg = scratch.gpr();
            
            writeBarrier(m_jit.globalObjectFor(node.codeOrigin), value.gpr(), node.child1(), WriteBarrierForVariableAccess, scratchReg);
        }
        
        m_jit.storePtr(value.gpr(), node.registerPointer());

        noResult(m_compileIndex);
        break;
    }

    case PutGlobalVarCheck: {
        JSValueOperand value(this, node.child1());
        
        WatchpointSet* watchpointSet =
            m_jit.globalObjectFor(node.codeOrigin)->symbolTable().get(
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
            
            writeBarrier(m_jit.globalObjectFor(node.codeOrigin), value.gpr(), node.child1(), WriteBarrierForVariableAccess, scratchReg);
        }
        
        m_jit.storePtr(value.gpr(), node.registerPointer());

        noResult(m_compileIndex);
        break;
    }
        
    case GlobalVarWatchpoint: {
        m_jit.globalObjectFor(node.codeOrigin)->symbolTable().get(
            identifier(node.identifierNumberForCheck())->impl()).addWatchpoint(
                speculationWatchpoint());
        
#if DFG_ENABLE(JIT_ASSERT)
        GPRTemporary scratch(this);
        GPRReg scratchGPR = scratch.gpr();
        m_jit.loadPtr(node.registerPointer(), scratchGPR);
        JITCompiler::Jump ok = m_jit.branchPtr(
            JITCompiler::Equal, scratchGPR,
            TrustedImmPtr(bitwise_cast<void*>(JSValue::encode(node.registerPointer()->get()))));
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
        
        JITCompiler::Jump isCell = m_jit.branchTestPtr(JITCompiler::Zero, value.gpr(), GPRInfo::tagMaskRegister);
        
        m_jit.comparePtr(JITCompiler::Equal, value.gpr(), TrustedImm32(ValueUndefined), result.gpr());
        JITCompiler::Jump done = m_jit.jump();
        
        isCell.link(&m_jit);
        m_jit.loadPtr(JITCompiler::Address(value.gpr(), JSCell::structureOffset()), result.gpr());
        m_jit.test8(JITCompiler::NonZero, JITCompiler::Address(result.gpr(), Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined), result.gpr());
        
        done.link(&m_jit);
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }
        
    case IsBoolean: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        m_jit.move(value.gpr(), result.gpr());
        m_jit.xorPtr(JITCompiler::TrustedImm32(ValueFalse), result.gpr());
        m_jit.testPtr(JITCompiler::Zero, result.gpr(), JITCompiler::TrustedImm32(static_cast<int32_t>(~1)), result.gpr());
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }
        
    case IsNumber: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        m_jit.testPtr(JITCompiler::NonZero, value.gpr(), GPRInfo::tagTypeNumberRegister, result.gpr());
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }
        
    case IsString: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        JITCompiler::Jump isNotCell = m_jit.branchTestPtr(JITCompiler::NonZero, value.gpr(), GPRInfo::tagMaskRegister);
        
        m_jit.loadPtr(JITCompiler::Address(value.gpr(), JSCell::structureOffset()), result.gpr());
        m_jit.compare8(JITCompiler::Equal, JITCompiler::Address(result.gpr(), Structure::typeInfoTypeOffset()), TrustedImm32(StringType), result.gpr());
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        JITCompiler::Jump done = m_jit.jump();
        
        isNotCell.link(&m_jit);
        m_jit.move(TrustedImm32(ValueFalse), result.gpr());
        
        done.link(&m_jit);
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }
        
    case IsObject: {
        JSValueOperand value(this, node.child1());
        GPRReg valueGPR = value.gpr();
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
        flushRegisters();
        callOperation(operationIsObject, resultGPR, valueGPR);
        m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }

    case IsFunction: {
        JSValueOperand value(this, node.child1());
        GPRReg valueGPR = value.gpr();
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
        flushRegisters();
        callOperation(operationIsFunction, resultGPR, valueGPR);
        m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        break;
    }

    case Flush:
    case Phi:
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
        GPRResult result(this);
        callOperation(operationResolve, result.gpr(), identifier(node.identifierNumber()));
        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case ResolveBase: {
        flushRegisters();
        GPRResult result(this);
        callOperation(operationResolveBase, result.gpr(), identifier(node.identifierNumber()));
        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case ResolveBaseStrictPut: {
        flushRegisters();
        GPRResult result(this);
        callOperation(operationResolveBaseStrictPut, result.gpr(), identifier(node.identifierNumber()));
        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case ResolveGlobal: {
        GPRTemporary globalObject(this);
        GPRTemporary resolveInfo(this);
        GPRTemporary result(this);

        GPRReg globalObjectGPR = globalObject.gpr();
        GPRReg resolveInfoGPR = resolveInfo.gpr();
        GPRReg resultGPR = result.gpr();

        ResolveGlobalData& data = m_jit.graph().m_resolveGlobalData[node.resolveGlobalDataIndex()];
        GlobalResolveInfo* resolveInfoAddress = &(m_jit.codeBlock()->globalResolveInfo(data.resolveInfoIndex));

        // Check Structure of global object
        m_jit.move(JITCompiler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), globalObjectGPR);
        m_jit.move(JITCompiler::TrustedImmPtr(resolveInfoAddress), resolveInfoGPR);
        m_jit.loadPtr(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(GlobalResolveInfo, structure)), resultGPR);
        JITCompiler::Jump structuresDontMatch = m_jit.branchPtr(JITCompiler::NotEqual, resultGPR, JITCompiler::Address(globalObjectGPR, JSCell::structureOffset()));

        // Fast case
        m_jit.load32(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(GlobalResolveInfo, offset)), resolveInfoGPR);
#if DFG_ENABLE(JIT_ASSERT)
        JITCompiler::Jump isOutOfLine = m_jit.branch32(JITCompiler::GreaterThanOrEqual, resolveInfoGPR, TrustedImm32(inlineStorageCapacity));
        m_jit.breakpoint();
        isOutOfLine.link(&m_jit);
#endif
        m_jit.neg32(resolveInfoGPR);
        m_jit.signExtend32ToPtr(resolveInfoGPR, resolveInfoGPR);
        m_jit.loadPtr(JITCompiler::Address(globalObjectGPR, JSObject::offsetOfOutOfLineStorage()), resultGPR);
        m_jit.loadPtr(JITCompiler::BaseIndex(resultGPR, resolveInfoGPR, JITCompiler::ScalePtr, (inlineStorageCapacity - 2) * static_cast<ptrdiff_t>(sizeof(JSValue))), resultGPR);
        
        addSlowPathGenerator(
            slowPathCall(
                structuresDontMatch, this, operationResolveGlobal,
                resultGPR, resolveInfoGPR, globalObjectGPR,
                &m_jit.codeBlock()->identifier(data.identifierNumber)));

        jsValueResult(resultGPR, m_compileIndex);
        break;
    }
        
    case CreateActivation: {
        ASSERT(!node.codeOrigin.inlineCallFrame);
        
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        GPRReg valueGPR = value.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(valueGPR, resultGPR);
        
        JITCompiler::Jump notCreated = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR);
        
        addSlowPathGenerator(
            slowPathCall(notCreated, this, operationCreateActivation, resultGPR));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case CreateArguments: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        GPRReg valueGPR = value.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(valueGPR, resultGPR);
        
        JITCompiler::Jump notCreated = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR);
        
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
        ASSERT(!node.codeOrigin.inlineCallFrame);

        JSValueOperand activationValue(this, node.child1());
        JSValueOperand argumentsValue(this, node.child2());
        GPRReg activationValueGPR = activationValue.gpr();
        GPRReg argumentsValueGPR = argumentsValue.gpr();
        
        JITCompiler::JumpList created;
        created.append(m_jit.branchTestPtr(JITCompiler::NonZero, activationValueGPR));
        created.append(m_jit.branchTestPtr(JITCompiler::NonZero, argumentsValueGPR));
        
        addSlowPathGenerator(
            slowPathCall(
                created, this, operationTearOffActivation, NoResult, activationValueGPR,
                static_cast<int32_t>(node.unmodifiedArgumentsRegister())));
        
        noResult(m_compileIndex);
        break;
    }
        
    case TearOffArguments: {
        JSValueOperand argumentsValue(this, node.child1());
        GPRReg argumentsValueGPR = argumentsValue.gpr();
        
        JITCompiler::Jump created = m_jit.branchTestPtr(JITCompiler::NonZero, argumentsValueGPR);
        
        if (node.codeOrigin.inlineCallFrame) {
            addSlowPathGenerator(
                slowPathCall(
                    created, this, operationTearOffInlinedArguments, NoResult,
                    argumentsValueGPR, node.codeOrigin.inlineCallFrame));
        } else {
            addSlowPathGenerator(
                slowPathCall(
                    created, this, operationTearOffArguments, NoResult, argumentsValueGPR));
        }
        
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
                m_jit.branchTestPtr(
                    JITCompiler::NonZero,
                    JITCompiler::addressFor(
                        m_jit.argumentsRegisterFor(node.codeOrigin))));
        }
        
        ASSERT(!node.codeOrigin.inlineCallFrame);
        m_jit.load32(JITCompiler::payloadFor(RegisterFile::ArgumentCount), resultGPR);
        m_jit.sub32(TrustedImm32(1), resultGPR);
        integerResult(resultGPR, m_compileIndex);
        break;
    }
        
    case GetMyArgumentsLengthSafe: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
        
        JITCompiler::Jump created = m_jit.branchTestPtr(
            JITCompiler::NonZero,
            JITCompiler::addressFor(
                m_jit.argumentsRegisterFor(node.codeOrigin)));
        
        if (node.codeOrigin.inlineCallFrame) {
            m_jit.move(
                ImmPtr(
                    bitwise_cast<void*>(
                        JSValue::encode(
                            jsNumber(node.codeOrigin.inlineCallFrame->arguments.size() - 1)))),
                resultGPR);
        } else {
            m_jit.load32(JITCompiler::payloadFor(RegisterFile::ArgumentCount), resultGPR);
            m_jit.sub32(TrustedImm32(1), resultGPR);
            m_jit.orPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
        }
        
        // FIXME: the slow path generator should perform a forward speculation that the
        // result is an integer. For now we postpone the speculation by having this return
        // a JSValue.
        
        addSlowPathGenerator(
            slowPathCall(
                created, this, operationGetArgumentsLength, resultGPR,
                m_jit.argumentsRegisterFor(node.codeOrigin)));
        
        jsValueResult(resultGPR, m_compileIndex);
        break;
    }
        
    case GetMyArgumentByVal: {
        SpeculateStrictInt32Operand index(this, node.child1());
        GPRTemporary result(this);
        GPRReg indexGPR = index.gpr();
        GPRReg resultGPR = result.gpr();
        
        if (!isEmptySpeculation(
                m_state.variables().operand(
                    m_jit.graph().argumentsRegisterFor(node.codeOrigin)).m_type)) {
            speculationCheck(
                ArgumentsEscaped, JSValueRegs(), NoNode,
                m_jit.branchTestPtr(
                    JITCompiler::NonZero,
                    JITCompiler::addressFor(
                        m_jit.argumentsRegisterFor(node.codeOrigin))));
        }

        m_jit.add32(TrustedImm32(1), indexGPR, resultGPR);
        if (node.codeOrigin.inlineCallFrame) {
            speculationCheck(
                Uncountable, JSValueRegs(), NoNode,
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultGPR,
                    Imm32(node.codeOrigin.inlineCallFrame->arguments.size())));
        } else {
            speculationCheck(
                Uncountable, JSValueRegs(), NoNode,
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultGPR,
                    JITCompiler::payloadFor(RegisterFile::ArgumentCount)));
        }
            
        m_jit.neg32(resultGPR);
        m_jit.signExtend32ToPtr(resultGPR, resultGPR);
            
        m_jit.loadPtr(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, resultGPR, JITCompiler::TimesEight,
                ((node.codeOrigin.inlineCallFrame
                  ? node.codeOrigin.inlineCallFrame->stackOffset
                  : 0) + CallFrame::argumentOffsetIncludingThis(0)) * sizeof(Register)),
            resultGPR);

        jsValueResult(resultGPR, m_compileIndex);
        break;
    }
        
    case GetMyArgumentByValSafe: {
        SpeculateStrictInt32Operand index(this, node.child1());
        GPRTemporary result(this);
        GPRReg indexGPR = index.gpr();
        GPRReg resultGPR = result.gpr();
        
        JITCompiler::JumpList slowPath;
        slowPath.append(
            m_jit.branchTestPtr(
                JITCompiler::NonZero,
                JITCompiler::addressFor(
                    m_jit.argumentsRegisterFor(node.codeOrigin))));
        
        m_jit.add32(TrustedImm32(1), indexGPR, resultGPR);
        if (node.codeOrigin.inlineCallFrame) {
            slowPath.append(
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultGPR,
                    Imm32(node.codeOrigin.inlineCallFrame->arguments.size())));
        } else {
            slowPath.append(
                m_jit.branch32(
                    JITCompiler::AboveOrEqual,
                    resultGPR,
                    JITCompiler::payloadFor(RegisterFile::ArgumentCount)));
        }
        
        m_jit.neg32(resultGPR);
        m_jit.signExtend32ToPtr(resultGPR, resultGPR);
        
        m_jit.loadPtr(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, resultGPR, JITCompiler::TimesEight,
                ((node.codeOrigin.inlineCallFrame
                  ? node.codeOrigin.inlineCallFrame->stackOffset
                  : 0) + CallFrame::argumentOffsetIncludingThis(0)) * sizeof(Register)),
            resultGPR);
        
        if (node.codeOrigin.inlineCallFrame) {
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationGetInlinedArgumentByVal, resultGPR, 
                    m_jit.argumentsRegisterFor(node.codeOrigin),
                    node.codeOrigin.inlineCallFrame,
                    indexGPR));
        } else {
            addSlowPathGenerator(
                slowPathCall(
                    slowPath, this, operationGetArgumentByVal, resultGPR, 
                    m_jit.argumentsRegisterFor(node.codeOrigin),
                    indexGPR));
        }
        
        jsValueResult(resultGPR, m_compileIndex);
        break;
    }
        
    case CheckArgumentsNotCreated: {
        ASSERT(!isEmptySpeculation(
            m_state.variables().operand(
                m_jit.graph().argumentsRegisterFor(node.codeOrigin)).m_type));
        speculationCheck(
            ArgumentsEscaped, JSValueRegs(), NoNode,
            m_jit.branchTestPtr(
                JITCompiler::NonZero,
                JITCompiler::addressFor(
                    m_jit.argumentsRegisterFor(node.codeOrigin))));
        noResult(m_compileIndex);
        break;
    }
        
    case NewFunctionNoCheck:
        compileNewFunctionNoCheck(node);
        break;
        
    case NewFunction: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        GPRReg valueGPR = value.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(valueGPR, resultGPR);
        
        JITCompiler::Jump notCreated = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR);
        
        addSlowPathGenerator(
            slowPathCall(
                notCreated, this, operationNewFunction,
                resultGPR, m_jit.codeBlock()->functionDecl(node.functionDeclIndex())));
        
        cellResult(resultGPR, m_compileIndex);
        break;
    }
        
    case NewFunctionExpression:
        compileNewFunctionExpression(node);
        break;

    case ForceOSRExit: {
        terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), NoNode);
        break;
    }

    case Phantom:
        // This is a no-op.
        noResult(m_compileIndex);
        break;
        
    case InlineStart:
    case Nop:
        ASSERT_NOT_REACHED();
        break;
        
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
