/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "JSByteArray.h"

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
            ASSERT(info.spillFormat() == DataFormatJS || info.spillFormat() == DataFormatJSInteger);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        }

        info.fillInteger(gpr);
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
        info.fillInteger(payloadGPR);
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
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
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
                info.fillInteger(gpr);
                unlock(gpr);
            } else if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(addressOfDoubleConstant(nodeIndex), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(fpr);
                return fpr;
            } else {
                // FIXME: should not be reachable?
                ASSERT_NOT_REACHED();
            }
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);
            if (spillFormat == DataFormatJSDouble) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
                info.fillDouble(fpr);
                return fpr;
            }

            FPRReg fpr = fprAllocate();
            JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag));
            m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
            JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

            isInteger.link(&m_jit);
            m_jit.convertInt32ToDouble(JITCompiler::payloadFor(virtualRegister), fpr);

            hasUnboxedDouble.link(&m_jit);
            m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
            info.fillDouble(fpr);
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

        if (info.registerFormat() != DataFormatJSInteger)
            hasUnboxedDouble.link(&m_jit);

        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.unlock(tagGPR);
        m_gprs.unlock(payloadGPR);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(fpr);
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
    }

    ASSERT_NOT_REACHED();
    return InvalidFPRReg;
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
            info.fillJSValue(tagGPR, payloadGPR, isInt32Constant(nodeIndex) ? DataFormatJSInteger : DataFormatJS);
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);
            tagGPR = allocate();
            payloadGPR = allocate();
            m_jit.load32(JITCompiler::tagFor(virtualRegister), tagGPR);
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), payloadGPR);
            m_gprs.retain(tagGPR, virtualRegister, SpillOrderSpilled);
            m_gprs.retain(payloadGPR, virtualRegister, SpillOrderSpilled);
            info.fillJSValue(tagGPR, payloadGPR, spillFormat == DataFormatJSDouble ? DataFormatJS : spillFormat);
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
        info.fillJSValue(tagGPR, payloadGPR, fillFormat);
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
        info.fillJSValue(tagGPR, payloadGPR, DataFormatJS);
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
    }

    ASSERT_NOT_REACHED();
    return true;
}

void SpeculativeJIT::nonSpeculativeValueToNumber(Node& node)
{
    if (isKnownNumeric(node.child1())) {
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

    ASSERT(!isInt32Constant(node.child1()));
    ASSERT(!isNumberConstant(node.child1()));

    GPRReg tagGPR = op1.tagGPR();
    GPRReg payloadGPR = op1.payloadGPR();
    GPRReg resultTagGPR = resultTag.gpr();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    op1.use();

    JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, tagGPR, TrustedImm32(JSValue::Int32Tag));
    JITCompiler::Jump nonNumeric = m_jit.branch32(MacroAssembler::AboveOrEqual, tagGPR, TrustedImm32(JSValue::LowestTag));

    // First, if we get here we have a double encoded as a JSValue
    JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

    // Next handle cells (& other JS immediates)
    nonNumeric.link(&m_jit);
    silentSpillAllRegisters(resultTagGPR, resultPayloadGPR);
    callOperation(dfgConvertJSValueToNumber, FPRInfo::returnValueFPR, tagGPR, payloadGPR);
    boxDouble(FPRInfo::returnValueFPR, resultTagGPR, resultPayloadGPR);
    silentFillAllRegisters(resultTagGPR, resultPayloadGPR);
    JITCompiler::Jump hasCalledToNumber = m_jit.jump();
    
    // Finally, handle integers.
    isInteger.link(&m_jit);
    hasUnboxedDouble.link(&m_jit);
    m_jit.move(tagGPR, resultTagGPR);
    m_jit.move(payloadGPR, resultPayloadGPR);
    hasCalledToNumber.link(&m_jit);
    jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativeValueToInt32(Node& node)
{
    ASSERT(!isInt32Constant(node.child1()));

    if (isKnownInteger(node.child1())) {
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
        JITCompiler::Jump truncatedToInteger = m_jit.branchTruncateDoubleToInt32(fpr, gpr, JITCompiler::BranchIfTruncateSuccessful);

        silentSpillAllRegisters(gpr);
        callOperation(toInt32, gpr, fpr);
        silentFillAllRegisters(gpr);

        truncatedToInteger.link(&m_jit);
        integerResult(gpr, m_compileIndex, UseChildrenCalledExplicitly);
        return;
    }

    JSValueOperand op1(this, node.child1());
    GPRTemporary result(this);
    GPRReg tagGPR = op1.tagGPR();
    GPRReg payloadGPR = op1.payloadGPR();
    GPRReg resultGPR = result.gpr();
    op1.use();

    JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, tagGPR, TrustedImm32(JSValue::Int32Tag));

    // First handle non-integers
    silentSpillAllRegisters(resultGPR);
    callOperation(dfgConvertJSValueToInt32, GPRInfo::returnValueGPR, tagGPR, payloadGPR);
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    silentFillAllRegisters(resultGPR);
    JITCompiler::Jump hasCalledToInt32 = m_jit.jump();

    // Then handle integers.
    isInteger.link(&m_jit);
    m_jit.move(payloadGPR, resultGPR);
    hasCalledToInt32.link(&m_jit);
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
    m_jit.move(JITCompiler::TrustedImmPtr(&twoToThe32), resultPayload.gpr()); // reuse resultPayload register here.
    m_jit.addDouble(JITCompiler::Address(resultPayload.gpr(), 0), boxer.fpr());
        
    boxDouble(boxer.fpr(), resultTag.gpr(), resultPayload.gpr());
        
    JITCompiler::Jump done = m_jit.jump();
        
    positive.link(&m_jit);
        
    m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTag.gpr());
    m_jit.move(op1.gpr(), resultPayload.gpr());
        
    done.link(&m_jit);

    jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
}

void SpeculativeJIT::nonSpeculativeKnownConstantArithOp(NodeType op, NodeIndex regChild, NodeIndex immChild, bool commute)
{
    JSValueOperand regArg(this, regChild);
    regArg.fill();
    
    if (regArg.isDouble()) {
        FPRReg regArgFPR = regArg.fpr();
        FPRTemporary imm(this);
        FPRTemporary result(this, regArg);
        GPRTemporary scratch(this);
        FPRReg immFPR = imm.fpr();
        FPRReg resultFPR = result.fpr();
        GPRReg scratchGPR = scratch.gpr();
        use(regChild);
        use(immChild);

        int32_t imm32 = valueOfInt32Constant(immChild);
        m_jit.move(TrustedImm32(imm32), scratchGPR);
        m_jit.convertInt32ToDouble(scratchGPR, immFPR);

        switch (op) {
        case ValueAdd:
        case ArithAdd:
            m_jit.addDouble(regArgFPR, immFPR, resultFPR);
            break;
        
        case ArithSub:
            m_jit.subDouble(regArgFPR, immFPR, resultFPR);
            break;
            
        default:
            ASSERT_NOT_REACHED();
        }

        doubleResult(resultFPR, m_compileIndex, UseChildrenCalledExplicitly);
        return;
    }

    GPRReg regArgTagGPR = regArg.tagGPR();
    GPRReg regArgPayloadGPR = regArg.payloadGPR();
    GPRTemporary resultTag(this, regArg);
    GPRTemporary resultPayload(this, regArg, false);
    GPRReg resultTagGPR = resultTag.gpr();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    FPRTemporary tmp1(this);
    FPRTemporary tmp2(this);
    FPRReg tmp1FPR = tmp1.fpr();
    FPRReg tmp2FPR = tmp2.fpr();
    use(regChild);
    use(immChild);

    JITCompiler::Jump notInt;
    int32_t imm = valueOfInt32Constant(immChild);

    if (!isKnownNumeric(regChild))
        notInt = m_jit.branch32(MacroAssembler::NotEqual, regArgTagGPR, TrustedImm32(JSValue::Int32Tag));
    
    JITCompiler::Jump overflow;

    switch (op) {
    case ValueAdd:
    case ArithAdd:
        overflow = m_jit.branchAdd32(MacroAssembler::Overflow, regArgPayloadGPR, Imm32(imm), resultPayloadGPR);
        break;
        
    case ArithSub:
        overflow = m_jit.branchSub32(MacroAssembler::Overflow, regArgPayloadGPR, Imm32(imm), resultPayloadGPR);
        break;
        
    default:
        ASSERT_NOT_REACHED();
    }

    m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
    JITCompiler::Jump done = m_jit.jump();
    
    overflow.link(&m_jit);
    // first deal with overflow case
    m_jit.convertInt32ToDouble(regArgPayloadGPR, tmp2FPR);
    m_jit.move(TrustedImm32(imm), resultPayloadGPR);
    m_jit.convertInt32ToDouble(resultPayloadGPR, tmp1FPR);
    switch (op) {
    case ValueAdd:
    case ArithAdd:
        m_jit.addDouble(tmp1FPR, tmp2FPR);
        break;
        
    case ArithSub:
        m_jit.subDouble(tmp1FPR, tmp2FPR);
        break;
            
    default:
        ASSERT_NOT_REACHED();
    }
    
    JITCompiler::Jump doneCaseConvertedToInt;
    
    if (op == ValueAdd) {
        JITCompiler::JumpList failureCases;
        m_jit.branchConvertDoubleToInt32(tmp2FPR, resultPayloadGPR, failureCases, tmp1FPR);
        m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
        doneCaseConvertedToInt = m_jit.jump();
        
        failureCases.link(&m_jit);
    }
    
    boxDouble(tmp2FPR, resultTagGPR, resultPayloadGPR);
        
    if (!isKnownNumeric(regChild)) {
        ASSERT(notInt.isSet());
        ASSERT(op == ValueAdd);
            
        JITCompiler::Jump doneCaseWasNumber = m_jit.jump();
            
        notInt.link(&m_jit);
            
        silentSpillAllRegisters(resultTagGPR, resultPayloadGPR);
        if (commute)
            callOperation(operationValueAddNotNumber, resultTagGPR, resultPayloadGPR, MacroAssembler::Imm32(imm), regArgTagGPR, regArgPayloadGPR);
        else
            callOperation(operationValueAddNotNumber, resultTagGPR, resultPayloadGPR, regArgTagGPR, regArgPayloadGPR, MacroAssembler::Imm32(imm));
        silentFillAllRegisters(resultTagGPR, resultPayloadGPR);
            
        doneCaseWasNumber.link(&m_jit);
    }
    
    done.link(&m_jit);
    if (doneCaseConvertedToInt.isSet())
        doneCaseConvertedToInt.link(&m_jit);

    jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativeBasicArithOp(NodeType op, Node &node)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    arg1.fill();
    arg2.fill();

    if (arg1.isDouble() && arg2.isDouble()) {
        FPRReg arg1FPR = arg1.fpr();
        FPRReg arg2FPR = arg2.fpr();
        FPRTemporary result(this, arg1);
        arg1.use();
        arg2.use();
        switch (op) {
        case ValueAdd:
        case ArithAdd:
            m_jit.addDouble(arg1FPR, arg2FPR, result.fpr());
            break;
            
        case ArithSub:
            m_jit.subDouble(arg1FPR, arg2FPR, result.fpr());
            break;
            
        case ArithMul:
            m_jit.mulDouble(arg1FPR, arg2FPR, result.fpr());
            break;
            
        default:
            ASSERT_NOT_REACHED();
        }

        doubleResult(result.fpr(), m_compileIndex, UseChildrenCalledExplicitly);
        return;
    }

    FPRTemporary tmp1(this);
    FPRTemporary tmp2(this);
    FPRReg tmp1FPR = tmp1.fpr();
    FPRReg tmp2FPR = tmp2.fpr();
    
    GPRTemporary resultTag(this, arg1.isDouble() ? arg2 : arg1);
    GPRTemporary resultPayload(this, arg1.isDouble() ? arg2 : arg1, false);
    GPRReg resultTagGPR = resultTag.gpr();
    GPRReg resultPayloadGPR = resultPayload.gpr();

    GPRReg arg1TagGPR = InvalidGPRReg;
    GPRReg arg1PayloadGPR = InvalidGPRReg;
    GPRReg arg2TagGPR = InvalidGPRReg;
    GPRReg arg2PayloadGPR = InvalidGPRReg;
    GPRTemporary tmpTag(this);
    GPRTemporary tmpPayload(this);

    if (arg1.isDouble()) {
        arg1TagGPR = tmpTag.gpr();
        arg1PayloadGPR = tmpPayload.gpr();
        boxDouble(arg1.fpr(), arg1TagGPR, arg1PayloadGPR);
        arg2TagGPR = arg2.tagGPR();
        arg2PayloadGPR = arg2.payloadGPR();
    } else if (arg2.isDouble()) {
        arg1TagGPR = arg1.tagGPR();
        arg1PayloadGPR = arg1.payloadGPR();
        arg2TagGPR = tmpTag.gpr();
        arg2PayloadGPR = tmpPayload.gpr();
        boxDouble(arg2.fpr(), arg2TagGPR, arg2PayloadGPR);
    } else {
        arg1TagGPR = arg1.tagGPR();
        arg1PayloadGPR = arg1.payloadGPR();
        arg2TagGPR = arg2.tagGPR();
        arg2PayloadGPR = arg2.payloadGPR();
    }

    arg1.use();
    arg2.use();
    
    JITCompiler::Jump child1NotInt;
    JITCompiler::Jump child2NotInt;
    JITCompiler::JumpList overflow;
    
    if (!isKnownInteger(node.child1()))
        child1NotInt = m_jit.branch32(MacroAssembler::NotEqual, arg1TagGPR, TrustedImm32(JSValue::Int32Tag));

    if (!isKnownInteger(node.child2()))
        child2NotInt = m_jit.branch32(MacroAssembler::NotEqual, arg2TagGPR, TrustedImm32(JSValue::Int32Tag));
    
    switch (op) {
    case ValueAdd:
    case ArithAdd: {
        overflow.append(m_jit.branchAdd32(MacroAssembler::Overflow, arg1PayloadGPR, arg2PayloadGPR, resultPayloadGPR));
        break;
    }
        
    case ArithSub: {
        overflow.append(m_jit.branchSub32(MacroAssembler::Overflow, arg1PayloadGPR, arg2PayloadGPR, resultPayloadGPR));
        break;
    }
        
    case ArithMul: {
        overflow.append(m_jit.branchMul32(MacroAssembler::Overflow, arg1PayloadGPR, arg2PayloadGPR, resultPayloadGPR));
        overflow.append(m_jit.branchTest32(MacroAssembler::Zero, resultPayloadGPR));
        break;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
    
    m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
        
    JITCompiler::Jump done = m_jit.jump();
    
    JITCompiler::JumpList haveFPRArguments;

    overflow.link(&m_jit);
        
    // both arguments are integers
    m_jit.convertInt32ToDouble(arg1PayloadGPR, tmp1FPR);
    m_jit.convertInt32ToDouble(arg2PayloadGPR, tmp2FPR);
        
    haveFPRArguments.append(m_jit.jump());
        
    JITCompiler::JumpList notNumbers;
        
    JITCompiler::Jump child2NotInt2;
        
    if (!isKnownInteger(node.child1())) {
        FPRTemporary scratch(this);
        child1NotInt.link(&m_jit);
            
        if (!isKnownNumeric(node.child1())) {
            ASSERT(op == ValueAdd);
            notNumbers.append(m_jit.branch32(MacroAssembler::AboveOrEqual, arg1TagGPR, TrustedImm32(JSValue::LowestTag)));
        }
    
        if (arg1.isDouble())
            m_jit.moveDouble(arg1.fpr(), tmp1FPR);
        else
            unboxDouble(arg1TagGPR, arg1PayloadGPR, tmp1FPR, scratch.fpr());
            
        // child1 is converted to a double; child2 may either be an int or
        // a boxed double
            
        if (!isKnownInteger(node.child2())) {
            if (isKnownNumeric(node.child2()))
                child2NotInt2 = m_jit.branch32(MacroAssembler::NotEqual, arg2TagGPR, TrustedImm32(JSValue::Int32Tag));
            else {
                ASSERT(op == ValueAdd);
                JITCompiler::Jump child2IsInt = m_jit.branch32(MacroAssembler::Equal, arg2TagGPR, TrustedImm32(JSValue::Int32Tag));
                notNumbers.append(m_jit.branch32(MacroAssembler::AboveOrEqual, arg2TagGPR, TrustedImm32(JSValue::LowestTag)));
                child2NotInt2 = m_jit.jump();
                child2IsInt.link(&m_jit);
            }
        }
            
        // child 2 is definitely an integer
        m_jit.convertInt32ToDouble(arg2PayloadGPR, tmp2FPR);
            
        haveFPRArguments.append(m_jit.jump());
    }
        
    if (!isKnownInteger(node.child2())) {
        FPRTemporary scratch(this);
        child2NotInt.link(&m_jit);
            
        if (!isKnownNumeric(node.child2())) {
            ASSERT(op == ValueAdd);
            notNumbers.append(m_jit.branch32(MacroAssembler::AboveOrEqual, arg2TagGPR, TrustedImm32(JSValue::LowestTag)));
        }
            
        // child1 is definitely an integer, and child 2 is definitely not
        m_jit.convertInt32ToDouble(arg1PayloadGPR, tmp1FPR);
            
        if (child2NotInt2.isSet())
            child2NotInt2.link(&m_jit);
            
        if (arg2.isDouble())
            m_jit.moveDouble(arg2.fpr(), tmp2FPR);
        else
            unboxDouble(arg2TagGPR, arg2PayloadGPR, tmp2FPR, scratch.fpr());
    }
        
    haveFPRArguments.link(&m_jit);
        
    switch (op) {
    case ValueAdd:
    case ArithAdd:
        m_jit.addDouble(tmp2FPR, tmp1FPR);
        break;
            
    case ArithSub:
        m_jit.subDouble(tmp2FPR, tmp1FPR);
        break;
            
    case ArithMul:
        m_jit.mulDouble(tmp2FPR, tmp1FPR);
        break;
            
    default:
        ASSERT_NOT_REACHED();
    }
    
    JITCompiler::Jump doneCaseConvertedToInt;
    
    if (op == ValueAdd) {
        JITCompiler::JumpList failureCases;
        m_jit.branchConvertDoubleToInt32(tmp1FPR, resultPayloadGPR, failureCases, tmp2FPR);
        m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
        
        doneCaseConvertedToInt = m_jit.jump();
        
        failureCases.link(&m_jit);
    }
        
    boxDouble(tmp1FPR, resultTagGPR, resultPayloadGPR);
        
    if (!notNumbers.empty()) {
        ASSERT(op == ValueAdd);
            
        JITCompiler::Jump doneCaseWasNumber = m_jit.jump();
            
        notNumbers.link(&m_jit);
            
        silentSpillAllRegisters(resultTagGPR, resultPayloadGPR);
        callOperation(operationValueAddNotNumber, resultTagGPR, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultTagGPR, resultPayloadGPR);

        doneCaseWasNumber.link(&m_jit);
    }
    
    done.link(&m_jit);
    if (doneCaseConvertedToInt.isSet())
        doneCaseConvertedToInt.link(&m_jit);
        
    jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

JITCompiler::Call SpeculativeJIT::cachedGetById(GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, GPRReg scratchGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget)
{
    m_jit.beginUninterruptedSequence();
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::Jump structureCheck = m_jit.branchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(basePayloadGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    m_jit.endUninterruptedSequence();
    
    m_jit.loadPtr(JITCompiler::Address(basePayloadGPR, JSObject::offsetOfPropertyStorage()), resultPayloadGPR);
    JITCompiler::DataLabelCompact tagLoadWithPatch = m_jit.load32WithCompactAddressOffsetPatch(JITCompiler::Address(resultPayloadGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
    JITCompiler::DataLabelCompact payloadLoadWithPatch = m_jit.load32WithCompactAddressOffsetPatch(JITCompiler::Address(resultPayloadGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
    
    JITCompiler::Jump done = m_jit.jump();

    structureCheck.link(&m_jit);
    
    if (slowPathTarget.isSet())
        slowPathTarget.link(&m_jit);
    
    JITCompiler::Label slowCase = m_jit.label();

    silentSpillAllRegisters(resultTagGPR, resultPayloadGPR);
    JITCompiler::Call functionCall;
    if (baseTagGPROrNone == InvalidGPRReg)
        functionCall = callOperation(operationGetByIdOptimize, resultTagGPR, resultPayloadGPR, JSValue::CellTag, basePayloadGPR, identifier(identifierNumber));
    else
        functionCall = callOperation(operationGetByIdOptimize, resultTagGPR, resultPayloadGPR, baseTagGPROrNone, basePayloadGPR, identifier(identifierNumber));
    silentFillAllRegisters(resultTagGPR, resultPayloadGPR);
    
    done.link(&m_jit);
    
    JITCompiler::Label doneLabel = m_jit.label();

    m_jit.addPropertyAccess(PropertyAccessRecord(structureToCompare, functionCall, structureCheck, tagLoadWithPatch, payloadLoadWithPatch, slowCase, doneLabel, safeCast<int8_t>(basePayloadGPR), safeCast<int8_t>(resultTagGPR), safeCast<int8_t>(resultPayloadGPR), safeCast<int8_t>(scratchGPR)));
    
    return functionCall;
}

void SpeculativeJIT::cachedPutById(GPRReg basePayloadGPR, GPRReg valueTagGPR, GPRReg valuePayloadGPR, NodeIndex valueIndex, GPRReg scratchGPR, unsigned identifierNumber, PutKind putKind, JITCompiler::Jump slowPathTarget)
{
    m_jit.beginUninterruptedSequence();
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::Jump structureCheck = m_jit.branchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(basePayloadGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    m_jit.endUninterruptedSequence();

    writeBarrier(basePayloadGPR, valueTagGPR, valueIndex, WriteBarrierForPropertyAccess, scratchGPR);

    m_jit.loadPtr(JITCompiler::Address(basePayloadGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
    JITCompiler::DataLabel32 tagStoreWithPatch = m_jit.store32WithAddressOffsetPatch(valueTagGPR, JITCompiler::Address(scratchGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    JITCompiler::DataLabel32 payloadStoreWithPatch = m_jit.store32WithAddressOffsetPatch(valuePayloadGPR, JITCompiler::Address(scratchGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));

    JITCompiler::Jump done = m_jit.jump();

    structureCheck.link(&m_jit);

    if (slowPathTarget.isSet())
        slowPathTarget.link(&m_jit);

    JITCompiler::Label slowCase = m_jit.label();

    silentSpillAllRegisters(InvalidGPRReg);
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
    JITCompiler::Call functionCall = callOperation(optimizedCall, valueTagGPR, valuePayloadGPR, basePayloadGPR, identifier(identifierNumber));
    silentFillAllRegisters(InvalidGPRReg);

    done.link(&m_jit);
    JITCompiler::Label doneLabel = m_jit.label();

    m_jit.addPropertyAccess(PropertyAccessRecord(structureToCompare, functionCall, structureCheck, JITCompiler::DataLabelCompact(tagStoreWithPatch.label()), JITCompiler::DataLabelCompact(payloadStoreWithPatch.label()), slowCase, doneLabel, safeCast<int8_t>(basePayloadGPR), safeCast<int8_t>(valueTagGPR), safeCast<int8_t>(valuePayloadGPR), safeCast<int8_t>(scratchGPR)));
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompareNull(NodeIndex operand, bool invert)
{
    JSValueOperand arg(this, operand);
    GPRReg argTagGPR = arg.tagGPR();
    GPRReg argPayloadGPR = arg.payloadGPR();

    GPRTemporary resultPayload(this, arg, false);
    GPRReg resultPayloadGPR = resultPayload.gpr();

    JITCompiler::Jump notCell;
    if (!isKnownCell(operand))
        notCell = m_jit.branch32(MacroAssembler::NotEqual, argTagGPR, TrustedImm32(JSValue::CellTag));
    
    m_jit.loadPtr(JITCompiler::Address(argPayloadGPR, JSCell::structureOffset()), resultPayloadGPR);
    m_jit.test8(invert ? JITCompiler::Zero : JITCompiler::NonZero, JITCompiler::Address(resultPayloadGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined), resultPayloadGPR);
    
    if (!isKnownCell(operand)) {
        JITCompiler::Jump done = m_jit.jump();
        
        notCell.link(&m_jit);
        // null or undefined?
        COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
        m_jit.move(argTagGPR, resultPayloadGPR);
        m_jit.or32(TrustedImm32(1), resultPayloadGPR);
        m_jit.compare32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultPayloadGPR, TrustedImm32(JSValue::NullTag), resultPayloadGPR);

        done.link(&m_jit);
    }
    
    booleanResult(resultPayloadGPR, m_compileIndex);
}

void SpeculativeJIT::nonSpeculativePeepholeBranchNull(NodeIndex operand, NodeIndex branchNodeIndex, bool invert)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();
    
    if (taken == (m_block + 1)) {
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
    
    if (!isKnownCell(operand))
        notCell = m_jit.branch32(MacroAssembler::NotEqual, argTagGPR, TrustedImm32(JSValue::CellTag));
    
    m_jit.loadPtr(JITCompiler::Address(argPayloadGPR, JSCell::structureOffset()), resultGPR);
    addBranch(m_jit.branchTest8(invert ? JITCompiler::Zero : JITCompiler::NonZero, JITCompiler::Address(resultGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined)), taken);
    
    if (!isKnownCell(operand)) {
        addBranch(m_jit.jump(), notTaken);
        
        notCell.link(&m_jit);
        // null or undefined?
        COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
        m_jit.move(argTagGPR, resultGPR);
        m_jit.or32(TrustedImm32(1), resultGPR);
        addBranch(m_jit.branch32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImm32(JSValue::NullTag)), taken);
    }
    
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

bool SpeculativeJIT::nonSpeculativeCompareNull(Node& node, NodeIndex operand, bool invert)
{
    NodeIndex branchNodeIndex = detectPeepHoleBranch();
    if (branchNodeIndex != NoNode) {
        ASSERT(node.adjustedRefCount() == 1);
        
        nonSpeculativePeepholeBranchNull(operand, branchNodeIndex, invert);
    
        use(node.child1());
        use(node.child2());
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
    if (taken == (m_block + 1)) {
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
    
    if (isKnownNotInteger(node.child1()) || isKnownNotInteger(node.child2())) {
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);

        addBranch(m_jit.branchTest32(callResultCondition, resultGPR), taken);
    } else {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();

        if (!isKnownInteger(node.child1()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg1TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));
        if (!isKnownInteger(node.child2()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg2TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));
    
        addBranch(m_jit.branch32(cond, arg1PayloadGPR, arg2PayloadGPR), taken);
    
        if (!isKnownInteger(node.child1()) || !isKnownInteger(node.child2())) {
            addBranch(m_jit.jump(), notTaken);
    
            slowPath.link(&m_jit);
    
            silentSpillAllRegisters(resultGPR);
            callOperation(helperFunction, resultGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
            silentFillAllRegisters(resultGPR);
        
            addBranch(m_jit.branchTest32(callResultCondition, resultGPR), taken);
        }
    }

    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompare(Node& node, MacroAssembler::RelationalCondition cond, S_DFGOperation_EJJ helperFunction)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1TagGPR = arg1.tagGPR();
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2TagGPR = arg2.tagGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1()) || isKnownNotInteger(node.child2())) {
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
    
        if (!isKnownInteger(node.child1()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg1TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));
        if (!isKnownInteger(node.child2()))
            slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, arg2TagGPR, JITCompiler::TrustedImm32(JSValue::Int32Tag)));

        m_jit.compare32(cond, arg1PayloadGPR, arg2PayloadGPR, resultPayloadGPR);
    
        if (!isKnownInteger(node.child1()) || !isKnownInteger(node.child2())) {
            JITCompiler::Jump haveResult = m_jit.jump();
    
            slowPath.link(&m_jit);
        
            silentSpillAllRegisters(resultPayloadGPR);
            callOperation(helperFunction, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
            silentFillAllRegisters(resultPayloadGPR);
        
            m_jit.andPtr(TrustedImm32(1), resultPayloadGPR);
        
            haveResult.link(&m_jit);
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
    if (taken == (m_block + 1)) {
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
    
    if (isKnownCell(node.child1()) && isKnownCell(node.child2())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        addBranch(m_jit.branchPtr(JITCompiler::Equal, arg1PayloadGPR, arg2PayloadGPR), invert ? notTaken : taken);
        
        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEqCell, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultPayloadGPR);
        
        addBranch(m_jit.branchTest32(invert ? JITCompiler::NonZero : JITCompiler::Zero, resultPayloadGPR), taken);
    } else {
        // FIXME: Add fast paths for twoCells, number etc.

        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEq, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR, arg2TagGPR, arg2PayloadGPR);
        silentFillAllRegisters(resultPayloadGPR);
        
        addBranch(m_jit.branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultPayloadGPR), taken);
    }
    
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
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
    
    if (isKnownCell(node.child1()) && isKnownCell(node.child2())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
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
    P_DFGOperation_E slowCallFunction;

    if (node.op == Call)
        slowCallFunction = operationLinkCall;
    else {
        ASSERT(node.op == Construct);
        slowCallFunction = operationLinkConstruct;
    }

    // For constructors, the this argument is not passed but we have to make space
    // for it.
    int dummyThisArgument = node.op == Call ? 0 : 1;

    CallLinkInfo::CallType callType = node.op == Call ? CallLinkInfo::Call : CallLinkInfo::Construct;

    NodeIndex calleeNodeIndex = m_jit.graph().m_varArgChildren[node.firstChild()];
    JSValueOperand callee(this, calleeNodeIndex);
    GPRReg calleeTagGPR = callee.tagGPR();
    GPRReg calleePayloadGPR = callee.payloadGPR();
    use(calleeNodeIndex);

    // the call instruction's first child is either the function (normal call) or the
    // receiver (method call). subsequent children are the arguments.
    int numArgs = node.numChildren() - 1;

    // For constructors, the this argument is not passed but we have to make space
    // for it.
    int numPassedArgs = numArgs + dummyThisArgument;

    // amount of stuff (in units of sizeof(Register)) that we need to place at the
    // top of the JS stack.
    int callDataSize = 0;

    // first there are the arguments
    callDataSize += numPassedArgs;

    // and then there is the call frame header
    callDataSize += RegisterFile::CallFrameHeaderSize;

    m_jit.store32(MacroAssembler::TrustedImm32(numPassedArgs), payloadOfCallData(RegisterFile::ArgumentCount));
    m_jit.store32(MacroAssembler::TrustedImm32(JSValue::Int32Tag), tagOfCallData(RegisterFile::ArgumentCount));
    m_jit.storePtr(GPRInfo::callFrameRegister, payloadOfCallData(RegisterFile::CallerFrame));
    m_jit.store32(MacroAssembler::TrustedImm32(JSValue::CellTag), tagOfCallData(RegisterFile::CallerFrame));

    for (int argIdx = 0; argIdx < numArgs; argIdx++) {
        NodeIndex argNodeIndex = m_jit.graph().m_varArgChildren[node.firstChild() + 1 + argIdx];
        JSValueOperand arg(this, argNodeIndex);
        GPRReg argTagGPR = arg.tagGPR();
        GPRReg argPayloadGPR = arg.payloadGPR();
        use(argNodeIndex);

        m_jit.store32(argTagGPR, tagOfCallData(-callDataSize + argIdx + dummyThisArgument));
        m_jit.store32(argPayloadGPR, payloadOfCallData(-callDataSize + argIdx + dummyThisArgument));
    }

    m_jit.store32(calleeTagGPR, tagOfCallData(RegisterFile::Callee));
    m_jit.store32(calleePayloadGPR, payloadOfCallData(RegisterFile::Callee));

    flushRegisters();

    GPRResult resultPayload(this);
    GPRResult2 resultTag(this);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    GPRReg resultTagGPR = resultTag.gpr();

    JITCompiler::DataLabelPtr targetToCheck;
    JITCompiler::JumpList slowPath;

    slowPath.append(m_jit.branchPtrWithPatch(MacroAssembler::NotEqual, calleePayloadGPR, targetToCheck));
    slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, calleeTagGPR, TrustedImm32(JSValue::CellTag)));
    m_jit.loadPtr(MacroAssembler::Address(calleePayloadGPR, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), resultPayloadGPR);
    m_jit.storePtr(resultPayloadGPR, payloadOfCallData(RegisterFile::ScopeChain));
    m_jit.store32(MacroAssembler::TrustedImm32(JSValue::CellTag), tagOfCallData(RegisterFile::ScopeChain));

    m_jit.addPtr(Imm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister);

    JITCompiler::Call fastCall = m_jit.nearCall();
    m_jit.notifyCall(fastCall, at(m_compileIndex).codeOrigin);

    JITCompiler::Jump done = m_jit.jump();

    slowPath.link(&m_jit);

    m_jit.addPtr(Imm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    m_jit.poke(GPRInfo::argumentGPR0);
    JITCompiler::Call slowCall = m_jit.addFastExceptionCheck(m_jit.appendCall(slowCallFunction), at(m_compileIndex).codeOrigin);
    m_jit.addPtr(Imm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister);
    m_jit.notifyCall(m_jit.call(GPRInfo::returnValueGPR), at(m_compileIndex).codeOrigin);

    done.link(&m_jit);

    setupResults(resultPayloadGPR, resultTagGPR);

    jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, DataFormatJS, UseChildrenCalledExplicitly);

    m_jit.addJSCall(fastCall, slowCall, targetToCheck, callType, at(m_compileIndex).codeOrigin);
}

template<bool strict>
GPRReg SpeculativeJIT::fillSpeculateIntInternal(NodeIndex nodeIndex, DataFormat& returnFormat)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "SpecInt@%d   ", nodeIndex);
#endif
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            if (isInt32Constant(nodeIndex)) {
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                info.fillInteger(gpr);
                returnFormat = DataFormatInteger;
                return gpr;
            }
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            returnFormat = DataFormatInteger;
            return allocate();
        }

        DataFormat spillFormat = info.spillFormat();
        ASSERT(spillFormat & DataFormatJS);

        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);

        // If we know this was spilled as an integer we can fill without checking.
        if (spillFormat != DataFormatJSInteger)
            speculationCheck(JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)));

        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        info.fillInteger(gpr);
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
        if (info.registerFormat() != DataFormatJSInteger) 
            speculationCheck(JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::Int32Tag)));
        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderInteger);
        info.fillInteger(payloadGPR);
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
    case DataFormatJSBoolean: {
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        returnFormat = DataFormatInteger;
        return allocate();
    }

    case DataFormatStorage:
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
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
    fprintf(stderr, "SpecDouble@%d   ", nodeIndex);
#endif
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {

        if (node.hasConstant()) {
            if (isInt32Constant(nodeIndex)) {
                GPRReg gpr = allocate();
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillInteger(gpr);
                unlock(gpr);
            } else if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(addressOfDoubleConstant(nodeIndex), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderConstant);
                info.fillDouble(fpr);
                return fpr;
            } else {
                terminateSpeculativeExecution(JSValueRegs(), NoNode);
                return fprAllocate();
            }
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);
            if (spillFormat == DataFormatJSDouble) {
                FPRReg fpr = fprAllocate();
                m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
                m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
                info.fillDouble(fpr);
                return fpr;
            }

            FPRReg fpr = fprAllocate();
            JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag));
            speculationCheck(JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::AboveOrEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::LowestTag)));
            m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
            JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

            isInteger.link(&m_jit);
            m_jit.convertInt32ToDouble(JITCompiler::payloadFor(virtualRegister), fpr);

            hasUnboxedDouble.link(&m_jit);
            m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
            info.fillDouble(fpr);
            return fpr;
        }
    }

    switch (info.registerFormat()) {
    case DataFormatNone:
    case DataFormatBoolean:
    case DataFormatStorage:
        // Should have filled, above.
        ASSERT_NOT_REACHED();

    case DataFormatCell:
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        return fprAllocate();

    case DataFormatJSCell:
    case DataFormatJS:
    case DataFormatJSInteger:
    case DataFormatJSBoolean: {
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        FPRReg fpr = fprAllocate();

        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);

        JITCompiler::Jump hasUnboxedDouble;

        if (info.registerFormat() != DataFormatJSInteger) {
            FPRTemporary scratch(this);
            JITCompiler::Jump isInteger = m_jit.branch32(MacroAssembler::Equal, tagGPR, TrustedImm32(JSValue::Int32Tag));
            speculationCheck(JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::AboveOrEqual, tagGPR, TrustedImm32(JSValue::LowestTag)));
            unboxDouble(tagGPR, payloadGPR, fpr, scratch.fpr());
            hasUnboxedDouble = m_jit.jump();
            isInteger.link(&m_jit);
        }

        m_jit.convertInt32ToDouble(payloadGPR, fpr);

        if (info.registerFormat() != DataFormatJSInteger)
            hasUnboxedDouble.link(&m_jit);

        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.unlock(tagGPR);
        m_gprs.unlock(payloadGPR);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(fpr);
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
    }

    ASSERT_NOT_REACHED();
    return InvalidFPRReg;
}

GPRReg SpeculativeJIT::fillSpeculateCell(NodeIndex nodeIndex)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "SpecCell@%d   ", nodeIndex);
#endif
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {

        GPRReg gpr = allocate();
        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isCell()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImmPtr(jsValue.asCell()), gpr);
                info.fillCell(gpr);
                return gpr;
            }
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        if (info.spillFormat() != DataFormatJSCell)
            speculationCheck(JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::CellTag)));
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillCell(gpr);
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
        if (info.spillFormat() != DataFormatJSCell)
            speculationCheck(JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::CellTag)));
        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderCell);
        info.fillCell(payloadGPR);
        return payloadGPR;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJSBoolean:
    case DataFormatBoolean: {
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        return allocate();
    }

    case DataFormatStorage:
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
}

GPRReg SpeculativeJIT::fillSpeculateBoolean(NodeIndex nodeIndex)
{
#if DFG_ENABLE(DEBUG_VERBOSE)
     fprintf(stderr, "SpecBool@%d   ", nodeIndex);
#endif
    Node& node = at(nodeIndex);
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isBoolean()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImm32(jsValue.asBoolean()), gpr);
                info.fillBoolean(gpr);
                return gpr;
            }
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);

        if (info.spillFormat() != DataFormatJSBoolean)
            speculationCheck(JSValueSource(JITCompiler::addressFor(virtualRegister)), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)));

        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        info.fillBoolean(gpr);
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
        if (info.registerFormat() != DataFormatJSBoolean)
            speculationCheck(JSValueRegs(tagGPR, payloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, tagGPR, TrustedImm32(JSValue::BooleanTag)));

        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderBoolean);
        info.fillBoolean(payloadGPR);
        return payloadGPR;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble:
    case DataFormatJSCell:
    case DataFormatCell: {
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        return allocate();
    }
        
    case DataFormatStorage:
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
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

void SpeculativeJIT::compileObjectEquality(Node& node, void* vptr, PredictionChecker predictionCheck)
{
    SpeculateCellOperand op1(this, node.child1());
    SpeculateCellOperand op2(this, node.child2());
    GPRTemporary resultPayload(this, op2);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    if (!predictionCheck(m_state.forNode(node.child1()).m_type))
        speculationCheck(JSValueSource::unboxedCell(op1GPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op1GPR), MacroAssembler::TrustedImmPtr(vptr)));
    if (!predictionCheck(m_state.forNode(node.child2()).m_type))
        speculationCheck(JSValueSource::unboxedCell(op2GPR), node.child2(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op2GPR), MacroAssembler::TrustedImmPtr(vptr)));
    
    MacroAssembler::Jump falseCase = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, op2GPR);
    m_jit.move(Imm32(1), resultPayloadGPR);
    MacroAssembler::Jump done = m_jit.jump();
    falseCase.link(&m_jit);
    m_jit.move(Imm32(0), resultPayloadGPR);
    done.link(&m_jit);

    booleanResult(resultPayloadGPR, m_compileIndex);
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
    
    m_jit.move(Imm32(1), resultPayload.gpr());
    MacroAssembler::Jump trueCase = m_jit.branchDouble(condition, op1.fpr(), op2.fpr());
    m_jit.move(Imm32(0), resultPayload.gpr());
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
    if (isKnownNotNumber(node.child1()) || isKnownNotNumber(node.child2()))
        callOperation(operationValueAddNotNumber, resultTag.gpr(), resultPayload.gpr(), op1TagGPR, op1PayloadGPR, op2TagGPR, op2PayloadGPR);
    else
        callOperation(operationValueAdd, resultTag.gpr(), resultPayload.gpr(), op1TagGPR, op1PayloadGPR, op2TagGPR, op2PayloadGPR);
    
    jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
}

void SpeculativeJIT::compileObjectOrOtherLogicalNot(NodeIndex nodeIndex, void* vptr, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeIndex);
    GPRTemporary resultPayload(this);
    GPRReg valueTagGPR = value.tagGPR();
    GPRReg valuePayloadGPR = value.payloadGPR();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branch32(MacroAssembler::NotEqual, valueTagGPR, TrustedImm32(JSValue::CellTag));
    if (needSpeculationCheck)
        speculationCheck(JSValueRegs(valueTagGPR, valuePayloadGPR), nodeIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(valuePayloadGPR), MacroAssembler::TrustedImmPtr(vptr)));
    m_jit.move(TrustedImm32(0), resultPayloadGPR);
    MacroAssembler::Jump done = m_jit.jump();
    
    notCell.link(&m_jit);
 
    COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
    if (needSpeculationCheck) {
        m_jit.move(valueTagGPR, resultPayloadGPR);
        m_jit.or32(TrustedImm32(1), resultPayloadGPR);
        speculationCheck(JSValueRegs(valueTagGPR, valuePayloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, resultPayloadGPR, TrustedImm32(JSValue::NullTag)));
    }
    m_jit.move(TrustedImm32(1), resultPayloadGPR);
    
    done.link(&m_jit);
    
    booleanResult(resultPayloadGPR, m_compileIndex);
}

void SpeculativeJIT::compileLogicalNot(Node& node)
{
    if (isKnownBoolean(node.child1()) || isBooleanPrediction(m_jit.getPrediction(node.child1()))) {
        SpeculateBooleanOperand value(this, node.child1());
        GPRTemporary result(this, value);
        m_jit.xor32(TrustedImm32(1), value.gpr(), result.gpr());
        booleanResult(result.gpr(), m_compileIndex);
        return;
    }
    if (at(node.child1()).shouldSpeculateFinalObjectOrOther()) {
        compileObjectOrOtherLogicalNot(node.child1(), m_jit.globalData()->jsFinalObjectVPtr, !isFinalObjectOrOtherPrediction(m_state.forNode(node.child1()).m_type));
        return;
    }
    if (at(node.child1()).shouldSpeculateArrayOrOther()) {
        compileObjectOrOtherLogicalNot(node.child1(), m_jit.globalData()->jsArrayVPtr, !isArrayOrOtherPrediction(m_state.forNode(node.child1()).m_type));
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

    JITCompiler::Jump fastCase = m_jit.branch32(JITCompiler::Equal, arg1TagGPR, TrustedImm32(JSValue::BooleanTag));
        
    silentSpillAllRegisters(resultPayloadGPR);
    callOperation(dfgConvertJSValueToBoolean, resultPayloadGPR, arg1TagGPR, arg1PayloadGPR);
    silentFillAllRegisters(resultPayloadGPR);
    JITCompiler::Jump doNot = m_jit.jump();
        
    fastCase.link(&m_jit);
    m_jit.move(arg1PayloadGPR, resultPayloadGPR);

    doNot.link(&m_jit);
    m_jit.xor32(TrustedImm32(1), resultPayloadGPR);
    booleanResult(resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitObjectOrOtherBranch(NodeIndex nodeIndex, BlockIndex taken, BlockIndex notTaken, void *vptr, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeIndex);
    GPRTemporary scratch(this);
    GPRReg valueTagGPR = value.tagGPR();
    GPRReg valuePayloadGPR = value.payloadGPR();
    GPRReg scratchGPR = scratch.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branch32(MacroAssembler::NotEqual, valueTagGPR, TrustedImm32(JSValue::CellTag));
    if (needSpeculationCheck)
        speculationCheck(JSValueRegs(valueTagGPR, valuePayloadGPR), nodeIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(valuePayloadGPR), MacroAssembler::TrustedImmPtr(vptr)));
    addBranch(m_jit.jump(), taken);
    
    notCell.link(&m_jit);
    
    COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
    if (needSpeculationCheck) {
        m_jit.move(valueTagGPR, scratchGPR);
        m_jit.or32(TrustedImm32(1), scratchGPR);
        speculationCheck(JSValueRegs(valueTagGPR, valuePayloadGPR), nodeIndex, m_jit.branch32(MacroAssembler::NotEqual, scratchGPR, TrustedImm32(JSValue::NullTag)));
    }

    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
    
    noResult(m_compileIndex);
}

void SpeculativeJIT::emitBranch(Node& node)
{
    BlockIndex taken = node.takenBlockIndex();
    BlockIndex notTaken = node.notTakenBlockIndex();

    if (isKnownBoolean(node.child1())) {
        SpeculateBooleanOperand value(this, node.child1());
        MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;

        if (taken == (m_block + 1)) {
            condition = MacroAssembler::Zero;
            BlockIndex tmp = taken;
            taken = notTaken;
            notTaken = tmp;
        }

        addBranch(m_jit.branchTest32(condition, value.gpr(), TrustedImm32(1)), taken);
        if (notTaken != (m_block + 1))
            addBranch(m_jit.jump(), notTaken);

        noResult(m_compileIndex);
    } else if (at(node.child1()).shouldSpeculateFinalObjectOrOther()) {
        emitObjectOrOtherBranch(node.child1(), taken, notTaken, m_jit.globalData()->jsFinalObjectVPtr, !isFinalObjectOrOtherPrediction(m_state.forNode(node.child1()).m_type));
    } else if (at(node.child1()).shouldSpeculateArrayOrOther()) {
        emitObjectOrOtherBranch(node.child1(), taken, notTaken, m_jit.globalData()->jsArrayVPtr, !isArrayOrOtherPrediction(m_state.forNode(node.child1()).m_type));
    } else if (at(node.child1()).shouldSpeculateNumber()) {
        if (at(node.child1()).shouldSpeculateInteger()) {
            bool invert = false;
            
            if (taken == (m_block + 1)) {
                invert = true;
                BlockIndex tmp = taken;
                taken = notTaken;
                notTaken = tmp;
            }

            SpeculateIntegerOperand value(this, node.child1());
            addBranch(m_jit.branchTest32(invert ? MacroAssembler::Zero : MacroAssembler::NonZero, value.gpr()), taken);
        } else {
            SpeculateDoubleOperand value(this, node.child1());
            FPRTemporary scratch(this);
            addBranch(m_jit.branchDoubleNonZero(value.fpr(), scratch.fpr()), taken);
        }
        
        if (notTaken != (m_block + 1))
            addBranch(m_jit.jump(), notTaken);
        
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
        addBranch(m_jit.branchTest32(JITCompiler::Zero, valuePayloadGPR), notTaken);
        addBranch(m_jit.jump(), taken);

        slowPath.link(&m_jit);
        silentSpillAllRegisters(resultGPR);
        callOperation(dfgConvertJSValueToBoolean, resultGPR, valueTagGPR, valuePayloadGPR);
        silentFillAllRegisters(resultGPR);
    
        addBranch(m_jit.branchTest32(JITCompiler::NonZero, resultGPR), taken);
        if (notTaken != (m_block + 1))
            addBranch(m_jit.jump(), notTaken);
    
        noResult(m_compileIndex, UseChildrenCalledExplicitly);
    }
}

void SpeculativeJIT::compile(Node& node)
{
    NodeType op = node.op;

    switch (op) {
    case JSConstant:
        initConstantInfo(m_compileIndex);
        break;

    case WeakJSConstant:
        m_jit.addWeakReference(node.weakConstant());
        initConstantInfo(m_compileIndex);
        break;

    case GetLocal: {
        PredictedType prediction = node.variableAccessData()->prediction();
        AbstractValue& value = block()->valuesAtHead.operand(node.local());

        // If we have no prediction for this local, then don't attempt to compile.
        if (prediction == PredictNone) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            break;
        }
        
        GPRTemporary result(this);
        if (isInt32Prediction(prediction)) {
            m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

            // Like integerResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node.virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderInteger);
            m_generationInfo[virtualRegister].initInteger(m_compileIndex, node.refCount(), result.gpr());
            break;
        }

        if (isArrayPrediction(prediction) || isByteArrayPrediction(prediction)) {
            m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

            // Like cellResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node.virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderCell);
            m_generationInfo[virtualRegister].initCell(m_compileIndex, node.refCount(), result.gpr());
            break;
        }

        if (isBooleanPrediction(prediction)) {
            m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

            // Like booleanResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node.virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderBoolean);
            m_generationInfo[virtualRegister].initBoolean(m_compileIndex, node.refCount(), result.gpr());
            break;
        }

        GPRTemporary tag(this);
        m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());
        m_jit.load32(JITCompiler::tagFor(node.local()), tag.gpr());

        // Like jsValueResult, but don't useChildren - our children are phi nodes,
        // and don't represent values within this dataflow with virtual registers.
        VirtualRegister virtualRegister = node.virtualRegister();
        m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);
        m_gprs.retain(tag.gpr(), virtualRegister, SpillOrderJS);

        DataFormat format = isCellPrediction(value.m_type) ? DataFormatJSCell : DataFormatJS;
        m_generationInfo[virtualRegister].initJSValue(m_compileIndex, node.refCount(), tag.gpr(), result.gpr(), format);
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
        Node& nextNode = at(m_compileIndex + 1);
        
        m_codeOriginForOSR = nextNode.codeOrigin;
        
        PredictedType predictedType = node.variableAccessData()->prediction();
        if (m_generationInfo[at(node.child1()).virtualRegister()].registerFormat() == DataFormatDouble) {
            DoubleOperand value(this, node.child1());
            m_jit.storeDouble(value.fpr(), JITCompiler::addressFor(node.local()));
            noResult(m_compileIndex);
        } else if (isInt32Prediction(predictedType)) {
            SpeculateIntegerOperand value(this, node.child1());
            m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
            noResult(m_compileIndex);
        } else if (isArrayPrediction(predictedType)) {
            SpeculateCellOperand cell(this, node.child1());
            GPRReg cellGPR = cell.gpr();
            if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
                speculationCheck(JSValueSource::unboxedCell(cellGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(cellGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
            m_jit.storePtr(cellGPR, JITCompiler::payloadFor(node.local()));
            noResult(m_compileIndex);
        } else if (isByteArrayPrediction(predictedType)) {
            SpeculateCellOperand cell(this, node.child1());
            GPRReg cellGPR = cell.gpr();
            if (!isByteArrayPrediction(m_state.forNode(node.child1()).m_type))
                speculationCheck(JSValueSource::unboxedCell(cellGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(cellGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
            m_jit.storePtr(cellGPR, JITCompiler::payloadFor(node.local()));
            noResult(m_compileIndex);
        } else if (isBooleanPrediction(predictedType)) {
            SpeculateBooleanOperand value(this, node.child1());
            m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
            noResult(m_compileIndex);
        } else {
            JSValueOperand value(this, node.child1());
            m_jit.store32(value.payloadGPR(), JITCompiler::payloadFor(node.local()));
            m_jit.store32(value.tagGPR(), JITCompiler::tagFor(node.local()));
            noResult(m_compileIndex);
        }

        // Indicate that it's no longer necessary to retrieve the value of
        // this bytecode variable from registers or other locations in the register file.
        valueSourceReferenceForOperand(node.local()) = ValueSource::forPrediction(predictedType);
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
        if (isInt32Constant(node.child1())) {
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op2);

            bitOp(op, valueOfInt32Constant(node.child1()), op2.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else if (isInt32Constant(node.child2())) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);

            bitOp(op, valueOfInt32Constant(node.child2()), op1.gpr(), result.gpr());

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
        if (isInt32Constant(node.child2())) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);

            shiftOp(op, op1.gpr(), valueOfInt32Constant(node.child2()) & 0x1f, result.gpr());

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
        if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
            // We know that this sometimes produces doubles. So produce a double every
            // time. This at least allows subsequent code to not have weird conditionals.
            
            IntegerOperand op1(this, node.child1());
            FPRTemporary result(this);
            
            GPRReg inputGPR = op1.gpr();
            FPRReg outputFPR = result.fpr();
            
            m_jit.convertInt32ToDouble(inputGPR, outputFPR);
            
            JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, inputGPR, TrustedImm32(0));
            m_jit.addDouble(JITCompiler::AbsoluteAddress(&twoToThe32), outputFPR);
            positive.link(&m_jit);
            
            doubleResult(outputFPR, m_compileIndex);
            break;
        }

        IntegerOperand op1(this, node.child1());
        GPRTemporary result(this, op1);

        // Test the operand is positive.
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, op1.gpr(), TrustedImm32(0)));

        m_jit.move(op1.gpr(), result.gpr());
        integerResult(result.gpr(), m_compileIndex, op1.format());
        break;
    }

    case ValueToInt32: {
        compileValueToInt32(node);
        break;
    }

    case ValueToNumber: {
        if (at(node.child1()).shouldNotSpeculateInteger()) {
            SpeculateDoubleOperand op1(this, node.child1());
            FPRTemporary result(this, op1);
            m_jit.moveDouble(op1.fpr(), result.fpr());
            doubleResult(result.fpr(), m_compileIndex);
            break;
        }
        
        SpeculateIntegerOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        m_jit.move(op1.gpr(), result.gpr());
        integerResult(result.gpr(), m_compileIndex, op1.format());
        break;
    }

    case ValueToDouble: {
        SpeculateDoubleOperand op1(this, node.child1());
        FPRTemporary result(this, op1);
        m_jit.moveDouble(op1.fpr(), result.fpr());
        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case ValueAdd:
    case ArithAdd: {
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
            if (isInt32Constant(node.child1())) {
                int32_t imm1 = valueOfInt32Constant(node.child1());
                SpeculateIntegerOperand op2(this, node.child2());
                GPRTemporary result(this);

                if (nodeCanTruncateInteger(node.arithNodeFlags())) {
                    m_jit.move(op2.gpr(), result.gpr());
                    m_jit.add32(Imm32(imm1), result.gpr());
                } else
                    speculationCheck(JSValueRegs(), NoNode, m_jit.branchAdd32(MacroAssembler::Overflow, op2.gpr(), Imm32(imm1), result.gpr()));

                integerResult(result.gpr(), m_compileIndex);
                break;
            }
                
            if (isInt32Constant(node.child2())) {
                SpeculateIntegerOperand op1(this, node.child1());
                int32_t imm2 = valueOfInt32Constant(node.child2());
                GPRTemporary result(this);
                
                if (nodeCanTruncateInteger(node.arithNodeFlags())) {
                    m_jit.move(op1.gpr(), result.gpr());
                    m_jit.add32(Imm32(imm2), result.gpr());
                } else
                    speculationCheck(JSValueRegs(), NoNode, m_jit.branchAdd32(MacroAssembler::Overflow, op1.gpr(), Imm32(imm2), result.gpr()));

                integerResult(result.gpr(), m_compileIndex);
                break;
            }
                
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op1, op2);

            GPRReg gpr1 = op1.gpr();
            GPRReg gpr2 = op2.gpr();
            GPRReg gprResult = result.gpr();

            if (nodeCanTruncateInteger(node.arithNodeFlags())) {
                if (gpr1 == gprResult)
                    m_jit.add32(gpr2, gprResult);
                else {
                    m_jit.move(gpr2, gprResult);
                    m_jit.add32(gpr1, gprResult);
                }
            } else {
                MacroAssembler::Jump check = m_jit.branchAdd32(MacroAssembler::Overflow, gpr1, gpr2, gprResult);
                
                if (gpr1 == gprResult)
                    speculationCheck(JSValueRegs(), NoNode, check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr2));
                else if (gpr2 == gprResult)
                    speculationCheck(JSValueRegs(), NoNode, check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr1));
                else
                    speculationCheck(JSValueRegs(), NoNode, check);
            }

            integerResult(gprResult, m_compileIndex);
            break;
        }
        
        if (Node::shouldSpeculateNumber(at(node.child1()), at(node.child2()))) {
            SpeculateDoubleOperand op1(this, node.child1());
            SpeculateDoubleOperand op2(this, node.child2());
            FPRTemporary result(this, op1, op2);

            FPRReg reg1 = op1.fpr();
            FPRReg reg2 = op2.fpr();
            m_jit.addDouble(reg1, reg2, result.fpr());

            doubleResult(result.fpr(), m_compileIndex);
            break;
        }

        ASSERT(op == ValueAdd);
        compileValueAdd(node);
        break;
    }

    case ArithSub: {
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
            if (isInt32Constant(node.child2())) {
                SpeculateIntegerOperand op1(this, node.child1());
                int32_t imm2 = valueOfInt32Constant(node.child2());
                GPRTemporary result(this);

                if (nodeCanTruncateInteger(node.arithNodeFlags())) {
                    m_jit.move(op1.gpr(), result.gpr());
                    m_jit.sub32(Imm32(imm2), result.gpr());
                } else
                    speculationCheck(JSValueRegs(), NoNode, m_jit.branchSub32(MacroAssembler::Overflow, op1.gpr(), Imm32(imm2), result.gpr()));

                integerResult(result.gpr(), m_compileIndex);
                break;
            }
                
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this);

            if (nodeCanTruncateInteger(node.arithNodeFlags())) {
                m_jit.move(op1.gpr(), result.gpr());
                m_jit.sub32(op2.gpr(), result.gpr());
            } else
                speculationCheck(JSValueRegs(), NoNode, m_jit.branchSub32(MacroAssembler::Overflow, op1.gpr(), op2.gpr(), result.gpr()));

            integerResult(result.gpr(), m_compileIndex);
            break;
        }
        
        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        FPRTemporary result(this, op1);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        m_jit.subDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case ArithMul: {
        compileArithMul(node);
        break;
    }

    case ArithDiv: {
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
#if CPU(X86)
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary eax(this, X86Registers::eax);
            GPRTemporary edx(this, X86Registers::edx);
            GPRReg op1GPR = op1.gpr();
            GPRReg op2GPR = op2.gpr();
            
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(JITCompiler::Zero, op2GPR));
            
            // If the user cares about negative zero, then speculate that we're not about
            // to produce negative zero.
            if (!nodeCanIgnoreNegativeZero(node.arithNodeFlags())) {
                MacroAssembler::Jump numeratorNonZero = m_jit.branchTest32(MacroAssembler::NonZero, op1GPR);
                speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, op2GPR, TrustedImm32(0)));
                numeratorNonZero.link(&m_jit);
            }
            
            GPRReg temp2 = InvalidGPRReg;
            if (op2GPR == X86Registers::eax || op2GPR == X86Registers::edx) {
                temp2 = allocate();
                m_jit.move(op2GPR, temp2);
                op2GPR = temp2;
            }
            
            m_jit.move(op1GPR, eax.gpr());
            m_jit.assembler().cdq();
            m_jit.assembler().idivl_r(op2GPR);
            
            if (temp2 != InvalidGPRReg)
                unlock(temp2);

            // Check that there was no remainder. If there had been, then we'd be obligated to
            // produce a double result instead.
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(JITCompiler::NonZero, edx.gpr()));
            
            integerResult(eax.gpr(), m_compileIndex);
#else // CPU(X86) -> so non-X86 code follows
            SpeculateDoubleOperand op1(this, node.child1());
            SpeculateDoubleOperand op2(this, node.child2());
            FPRTemporary result(this);
            FPRTemporary scratch(this);
            GPRTemporary intResult(this);
            
            FPRReg op1FPR = op1.fpr();
            FPRReg op2FPR = op2.fpr();
            FPRReg resultFPR = result.fpr();
            FPRReg scratchFPR = scratch.fpr();
            GPRReg resultGPR = intResult.gpr();
            
            m_jit.divDouble(op1FPR, op2FPR, resultFPR);
            
            JITCompiler::JumpList failureCases;
            m_jit.branchConvertDoubleToInt32(resultFPR, resultGPR, failureCases, scratchFPR);
            speculationCheck(JSValueRegs(), NoNode, failureCases);

            integerResult(resultGPR, m_compileIndex);
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
        if (at(node.child1()).shouldSpeculateInteger() && node.canSpeculateInteger()) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this);
            GPRTemporary scratch(this);
            
            m_jit.zeroExtend32ToPtr(op1.gpr(), result.gpr());
            m_jit.rshift32(result.gpr(), MacroAssembler::TrustedImm32(31), scratch.gpr());
            m_jit.add32(scratch.gpr(), result.gpr());
            m_jit.xor32(scratch.gpr(), result.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Equal, result.gpr(), MacroAssembler::TrustedImm32(1 << 31)));
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
        if (isNullConstant(node.child1())) {
            if (nonSpeculativeCompareNull(node, node.child2()))
                return;
            break;
        }
        if (isNullConstant(node.child2())) {
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
        PredictedType basePrediction = at(node.child2()).prediction();
        if (!(basePrediction & PredictInt32) && basePrediction) {
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

        if (at(node.child1()).prediction() == PredictString) {
            compileGetByValOnString(node);
            if (!m_compileOkay)
                return;
            break;
        }
        
        if (at(node.child1()).shouldSpeculateByteArray()) {
            compileGetByValOnByteArray(node);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt8Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), node, sizeof(int8_t), isInt8ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt16Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), node, sizeof(int16_t), isInt16ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt32Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), node, sizeof(int32_t), isInt32ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint8Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), node, sizeof(uint8_t), isUint8ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint16Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), node, sizeof(uint16_t), isUint16ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint32Array()) {
            compileGetByValOnIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), node, sizeof(uint32_t), isUint32ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat32Array()) {
            compileGetByValOnFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), node, sizeof(float), isFloat32ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat64Array()) {
            compileGetByValOnFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), node, sizeof(double), isFloat64ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }

        ASSERT(node.child3() == NoNode);
        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        GPRTemporary storage(this);
        GPRTemporary resultTag(this);

        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg storageReg = storage.gpr();
        
        if (!m_compileOkay)
            return;

        // Get the array storage. We haven't yet checked this is a JSArray, so this is only safe if
        // an access with offset JSArray::storageOffset() is valid for all JSCells!
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSArray::storageOffset()), storageReg);

        // Check that base is an array, and that property is contained within m_vector (< m_vectorLength).
        // If we have predicted the base to be type array, we can skip the check.
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset())));

        // FIXME: In cases where there are subsequent by_val accesses to the same base it might help to cache
        // the storage pointer - especially if there happens to be another register free right now. If we do so,
        // then we'll need to allocate a new temporary for result.
        m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTag.gpr());
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Equal, resultTag.gpr(), TrustedImm32(JSValue::EmptyValueTag)));
        m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), storageReg);

        jsValueResult(resultTag.gpr(), storageReg, m_compileIndex);
        break;
    }

    case PutByVal: {
        PredictedType basePrediction = at(node.child2()).prediction();
        if (!(basePrediction & PredictInt32) && basePrediction) {
            SpeculateCellOperand base(this, node.child1()); // Save a register, speculate cell. We'll probably be right.
            JSValueOperand property(this, node.child2());
            JSValueOperand value(this, node.child3());
            GPRReg baseGPR = base.gpr();
            GPRReg propertyTagGPR = property.tagGPR();
            GPRReg propertyPayloadGPR = property.payloadGPR();
            GPRReg valueTagGPR = value.tagGPR();
            GPRReg valuePayloadGPR = value.payloadGPR();
            
            flushRegisters();
            callOperation(m_jit.codeBlock()->isStrictMode() ? operationPutByValCellStrict : operationPutByValCellNonStrict, baseGPR, propertyTagGPR, propertyPayloadGPR, valueTagGPR, valuePayloadGPR);
            
            noResult(m_compileIndex);
            break;
        }

        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        if (at(node.child1()).shouldSpeculateByteArray()) {
            compilePutByValForByteArray(base.gpr(), property.gpr(), node);
            break;
        }
        if (at(node.child1()).shouldSpeculateByteArray()) {
            compilePutByValForByteArray(base.gpr(), property.gpr(), node);
            break;
        }
        
        if (at(node.child1()).shouldSpeculateInt8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int8_t), isInt8ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int16_t), isInt16ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int32_t), isInt32ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), isUint8ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint16_t), isUint16ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint32_t), isUint32ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat32Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(float), isFloat32ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat64Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(double), isFloat64ArrayPrediction(m_state.forNode(node.child1()).m_type) ? NoTypedArrayTypeSpecCheck : AllTypedArraySpecChecks);
            if (!m_compileOkay)
                return;
            break;            
        }

        JSValueOperand value(this, node.child3());
        GPRTemporary scratch(this);

        // Map base, property & value into registers, allocate a scratch register.
        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg valueTagReg = value.tagGPR();
        GPRReg valuePayloadReg = value.payloadGPR();
        GPRReg scratchReg = scratch.gpr();
        
        if (!m_compileOkay)
            return;
        
        writeBarrier(baseReg, valueTagReg, node.child3(), WriteBarrierForPropertyAccess, scratchReg);

        // Check that base is an array, and that property is contained within m_vector (< m_vectorLength).
        // If we have predicted the base to be type array, we can skip the check.
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));

        base.use();
        property.use();
        value.use();
        
        MacroAssembler::Jump withinArrayBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset()));

        // Code to handle put beyond array bounds.
        silentSpillAllRegisters(scratchReg);
        callOperation(operationPutByValBeyondArrayBounds, baseReg, propertyReg, valueTagReg, valuePayloadReg);
        silentFillAllRegisters(scratchReg);
        JITCompiler::Jump wasBeyondArrayBounds = m_jit.jump();

        withinArrayBounds.link(&m_jit);

        // Get the array storage.
        GPRReg storageReg = scratchReg;
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSArray::storageOffset()), storageReg);

        // Check if we're writing to a hole; if so increment m_numValuesInVector.
        MacroAssembler::Jump notHoleValue = m_jit.branch32(MacroAssembler::NotEqual, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));
        m_jit.add32(TrustedImm32(1), MacroAssembler::Address(storageReg, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));

        // If we're writing to a hole we might be growing the array; 
        MacroAssembler::Jump lengthDoesNotNeedUpdate = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(storageReg, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.add32(TrustedImm32(1), propertyReg);
        m_jit.store32(propertyReg, MacroAssembler::Address(storageReg, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.sub32(TrustedImm32(1), propertyReg);

        lengthDoesNotNeedUpdate.link(&m_jit);
        notHoleValue.link(&m_jit);

        // Store the value to the array.
        m_jit.store32(valueTagReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
        m_jit.store32(valuePayloadReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));

        wasBeyondArrayBounds.link(&m_jit);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByValAlias: {
        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());

        if (at(node.child1()).shouldSpeculateByteArray()) {
            compilePutByValForByteArray(base.gpr(), property.gpr(), node);
            break;
        }
        
        if (at(node.child1()).shouldSpeculateInt8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int8_t), NoTypedArraySpecCheck, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int16_t), NoTypedArraySpecCheck, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateInt32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->int32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(int32_t), NoTypedArraySpecCheck, SignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint8Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint8ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint8_t), NoTypedArraySpecCheck, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint16Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint16ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint16_t), NoTypedArraySpecCheck, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateUint32Array()) {
            compilePutByValForIntTypedArray(m_jit.globalData()->uint32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(uint32_t), NoTypedArraySpecCheck, UnsignedTypedArray);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat32Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float32ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(float), NoTypedArraySpecCheck);
            if (!m_compileOkay)
                return;
            break;            
        }
        
        if (at(node.child1()).shouldSpeculateFloat64Array()) {
            compilePutByValForFloatTypedArray(m_jit.globalData()->float64ArrayDescriptor(), base.gpr(), property.gpr(), node, sizeof(double), NoTypedArraySpecCheck);
            if (!m_compileOkay)
                return;
            break;            
        }

        JSValueOperand value(this, node.child3());
        GPRTemporary scratch(this, base);
        
        GPRReg baseReg = base.gpr();
        GPRReg scratchReg = scratch.gpr();

        writeBarrier(baseReg, value.tagGPR(), node.child3(), WriteBarrierForPropertyAccess, scratchReg);

        // Get the array storage.
        GPRReg storageReg = scratchReg;
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSArray::storageOffset()), storageReg);

        // Store the value to the array.
        GPRReg propertyReg = property.gpr();
        m_jit.store32(value.tagGPR(), MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
        m_jit.store32(value.payloadGPR(), MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));

        noResult(m_compileIndex);
        break;
    }

    case ArrayPush: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary storage(this);
        GPRTemporary storageLength(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueTagGPR = value.tagGPR();
        GPRReg valuePayloadGPR = value.payloadGPR();
        GPRReg storageGPR = storage.gpr();
        GPRReg storageLengthGPR = storageLength.gpr();
        
        writeBarrier(baseGPR, valueTagGPR, node.child2(), WriteBarrierForPropertyAccess, storageGPR, storageLengthGPR);

        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);
        m_jit.load32(MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), storageLengthGPR);
        
        // Refuse to handle bizarre lengths.
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Above, storageLengthGPR, TrustedImm32(0x7ffffffe)));
        
        MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        m_jit.store32(valueTagGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
        m_jit.store32(valuePayloadGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
        
        m_jit.add32(Imm32(1), storageLengthGPR);
        m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.add32(Imm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        m_jit.move(Imm32(JSValue::Int32Tag), storageGPR);
        
        MacroAssembler::Jump done = m_jit.jump();
        
        slowPath.link(&m_jit);
        
        silentSpillAllRegisters(storageGPR, storageLengthGPR);
        callOperation(operationArrayPush, storageGPR, storageLengthGPR, valueTagGPR, valuePayloadGPR, baseGPR);
        silentFillAllRegisters(storageGPR, storageLengthGPR);
        
        done.link(&m_jit);
        
        jsValueResult(storageGPR, storageLengthGPR, m_compileIndex);
        break;
    }
        
    case ArrayPop: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary valueTag(this);
        GPRTemporary valuePayload(this);
        GPRTemporary storage(this);
        GPRTemporary storageLength(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueTagGPR = valueTag.gpr();
        GPRReg valuePayloadGPR = valuePayload.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg storageLengthGPR = storageLength.gpr();
        
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);
        m_jit.load32(MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), storageLengthGPR);
        
        MacroAssembler::Jump emptyArrayCase = m_jit.branchTest32(MacroAssembler::Zero, storageLengthGPR);
        
        m_jit.sub32(Imm32(1), storageLengthGPR);
        
        MacroAssembler::Jump slowCase = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        m_jit.load32(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), valueTagGPR);
        m_jit.load32(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), valuePayloadGPR);
        
        m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));

        MacroAssembler::Jump holeCase = m_jit.branch32(MacroAssembler::Equal, Imm32(JSValue::EmptyValueTag), valueTagGPR);
        
        m_jit.store32(TrustedImm32(JSValue::EmptyValueTag), MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

        m_jit.sub32(MacroAssembler::Imm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
        MacroAssembler::JumpList done;
        
        done.append(m_jit.jump());
        
        holeCase.link(&m_jit);
        emptyArrayCase.link(&m_jit);
        m_jit.move(MacroAssembler::Imm32(jsUndefined().tag()), valueTagGPR);
        m_jit.move(MacroAssembler::Imm32(jsUndefined().payload()), valuePayloadGPR);
        done.append(m_jit.jump());
        
        slowCase.link(&m_jit);
        
        silentSpillAllRegisters(valueTagGPR, valuePayloadGPR);
        callOperation(operationArrayPop, valueTagGPR, valuePayloadGPR, baseGPR);
        silentFillAllRegisters(valueTagGPR, valuePayloadGPR);
        
        done.link(&m_jit);
        
        jsValueResult(valueTagGPR, valuePayloadGPR, m_compileIndex);
        break;
    }

    case DFG::Jump: {
        BlockIndex taken = node.takenBlockIndex();
        if (taken != (m_block + 1))
            addBranch(m_jit.jump(), taken);
        noResult(m_compileIndex);
        break;
    }

    case Branch:
        if (isStrictInt32(node.child1()) || at(node.child1()).shouldSpeculateInteger()) {
            SpeculateIntegerOperand op(this, node.child1());
            
            BlockIndex taken = node.takenBlockIndex();
            BlockIndex notTaken = node.notTakenBlockIndex();
            
            MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;
            
            if (taken == (m_block + 1)) {
                condition = MacroAssembler::Zero;
                BlockIndex tmp = taken;
                taken = notTaken;
                notTaken = tmp;
            }
            
            addBranch(m_jit.branchTest32(condition, op.gpr()), taken);
            if (notTaken != (m_block + 1))
                addBranch(m_jit.jump(), notTaken);
            
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
        m_jit.emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, GPRInfo::regT2);
        // Restore our caller's "r".
        m_jit.emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, GPRInfo::callFrameRegister);
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
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
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
        
        bool wasPrimitive = isKnownNumeric(node.child1()) || isKnownBoolean(node.child1());
        
        JSValueOperand op1(this, node.child1());
        GPRTemporary resultTag(this, op1);
        GPRTemporary resultPayload(this, op1, false);
        
        GPRReg op1TagGPR = op1.tagGPR();
        GPRReg op1PayloadGPR = op1.payloadGPR();
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        
        op1.use();
        
        if (wasPrimitive) {
            m_jit.move(op1TagGPR, resultTagGPR);
            m_jit.move(op1PayloadGPR, resultPayloadGPR);
        } else {
            MacroAssembler::JumpList alreadyPrimitive;
            
            alreadyPrimitive.append(m_jit.branch32(MacroAssembler::NotEqual, op1TagGPR, TrustedImm32(JSValue::CellTag)));
            alreadyPrimitive.append(m_jit.branchPtr(MacroAssembler::Equal, MacroAssembler::Address(op1PayloadGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));
            
            silentSpillAllRegisters(resultTagGPR, resultPayloadGPR);
            callOperation(operationToPrimitive, resultTagGPR, resultPayloadGPR, op1TagGPR, op1PayloadGPR);
            silentFillAllRegisters(resultTagGPR, resultPayloadGPR);
            
            MacroAssembler::Jump done = m_jit.jump();
            
            alreadyPrimitive.link(&m_jit);
            m_jit.move(op1TagGPR, resultTagGPR);
            m_jit.move(op1PayloadGPR, resultPayloadGPR);
            
            done.link(&m_jit);
        }
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }
        
    case StrCat:
    case NewArray: {
        // We really don't want to grow the register file just to do a StrCat or NewArray.
        // Say we have 50 functions on the stack that all have a StrCat in them that has
        // upwards of 10 operands. In the DFG this would mean that each one gets
        // some random virtual register, and then to do the StrCat we'd need a second
        // span of 10 operands just to have somewhere to copy the 10 operands to, where
        // they'd be contiguous and we could easily tell the C code how to find them.
        // Ugly! So instead we use the scratchBuffer infrastructure in JSGlobalData. That
        // way, those 50 functions will share the same scratchBuffer for offloading their
        // StrCat operands. It's about as good as we can do, unless we start doing
        // virtual register coalescing to ensure that operands to StrCat get spilled
        // in exactly the place where StrCat wants them, or else have the StrCat
        // refer to those operands' SetLocal instructions to force them to spill in
        // the right place. Basically, any way you cut it, the current approach
        // probably has the best balance of performance and sensibility in the sense
        // that it does not increase the complexity of the DFG JIT just to make StrCat
        // fast and pretty.
        
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(m_jit.globalData()->scratchBufferForSize(sizeof(EncodedJSValue) * node.numChildren()));
        
        for (unsigned operandIdx = 0; operandIdx < node.numChildren(); ++operandIdx) {
            JSValueOperand operand(this, m_jit.graph().m_varArgChildren[node.firstChild() + operandIdx]);
            GPRReg opTagGPR = operand.tagGPR();
            GPRReg opPayloadGPR = operand.payloadGPR();
            operand.use();
            
            m_jit.store32(opTagGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
            m_jit.store32(opPayloadGPR, reinterpret_cast<char*>(buffer + operandIdx) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
        }
        
        flushRegisters();
        
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        
        callOperation(op == StrCat ? operationStrCat : operationNewArray, resultTag.gpr(), resultPayload.gpr(), buffer, node.numChildren());

        // FIXME: make the callOperation above explicitly return a cell result, or jitAssert the tag is a cell tag.
        cellResult(resultPayload.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case NewArrayBuffer: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        
        callOperation(operationNewArrayBuffer, resultTag.gpr(), resultPayload.gpr(), node.startConstant(), node.numConstants());
        
        // FIXME: make the callOperation above explicitly return a cell result, or jitAssert the tag is a cell tag.
        cellResult(resultPayload.gpr(), m_compileIndex);
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
        if (isObjectPrediction(m_state.forNode(node.child1()).m_type)) {
            SpeculateCellOperand thisValue(this, node.child1());
            GPRTemporary result(this, thisValue);
            m_jit.move(thisValue.gpr(), result.gpr());
            cellResult(result.gpr(), m_compileIndex);
            break;
        }
        
        if (isOtherPrediction(at(node.child1()).prediction())) {
            JSValueOperand thisValue(this, node.child1());
            GPRTemporary scratch(this);
            
            GPRReg thisValueTagGPR = thisValue.tagGPR();
            GPRReg scratchGPR = scratch.gpr();
            
            COMPILE_ASSERT((JSValue::UndefinedTag | 1) == JSValue::NullTag, UndefinedTag_OR_1_EQUALS_NullTag);
            m_jit.move(thisValueTagGPR, scratchGPR);
            m_jit.or32(TrustedImm32(1), scratchGPR);
            // This is hard. It would be better to save the value, but we can't quite do it,
            // since this operation does not otherwise get the payload.
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, scratchGPR, TrustedImm32(JSValue::NullTag)));
            
            m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), scratchGPR);
            cellResult(scratchGPR, m_compileIndex);
            break;
        }
        
        if (isObjectPrediction(at(node.child1()).prediction())) {
            SpeculateCellOperand thisValue(this, node.child1());
            GPRTemporary result(this, thisValue);
            GPRReg thisValueGPR = thisValue.gpr();
            GPRReg resultGPR = result.gpr();
            
            if (!isObjectPrediction(m_state.forNode(node.child1()).m_type))
                speculationCheck(JSValueSource::unboxedCell(thisValueGPR), node.child1(), m_jit.branchPtr(JITCompiler::Equal, JITCompiler::Address(thisValueGPR), JITCompiler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));
            
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
        
        SpeculateCellOperand proto(this, node.child1());
        GPRTemporary result(this);
        GPRTemporary scratch(this);
        
        GPRReg protoGPR = proto.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        proto.use();
        
        MacroAssembler::JumpList slowPath;
        
        // Need to verify that the prototype is an object. If we have reason to believe
        // that it's a FinalObject then we speculate on that directly. Otherwise we
        // do the slow (structure-based) check.
        if (at(node.child1()).shouldSpeculateFinalObject()) {
            if (!isFinalObjectPrediction(m_state.forNode(node.child1()).m_type))
                speculationCheck(JSValueSource::unboxedCell(protoGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(protoGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsFinalObjectVPtr)));
        } else {
            m_jit.loadPtr(MacroAssembler::Address(protoGPR, JSCell::structureOffset()), scratchGPR);
            slowPath.append(m_jit.branch8(MacroAssembler::Below, MacroAssembler::Address(scratchGPR, Structure::typeInfoTypeOffset()), MacroAssembler::TrustedImm32(ObjectType)));
        }
        
        // Load the inheritorID (the Structure that objects who have protoGPR as the prototype
        // use to refer to that prototype). If the inheritorID is not set, go to slow path.
        m_jit.loadPtr(MacroAssembler::Address(protoGPR, JSObject::offsetOfInheritorID()), scratchGPR);
        slowPath.append(m_jit.branchTestPtr(MacroAssembler::Zero, scratchGPR));
        
        emitAllocateJSFinalObject(scratchGPR, resultGPR, scratchGPR, slowPath);
        
        MacroAssembler::Jump done = m_jit.jump();
        
        slowPath.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        if (node.codeOrigin.inlineCallFrame)
            callOperation(operationCreateThisInlined, resultGPR, protoGPR, node.codeOrigin.inlineCallFrame->callee.get());
        else
            callOperation(operationCreateThis, resultGPR, protoGPR);
        silentFillAllRegisters(resultGPR);
        
        done.link(&m_jit);
        
        cellResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case NewObject: {
        GPRTemporary result(this);
        GPRTemporary scratch(this);
        
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        MacroAssembler::JumpList slowPath;
        
        emitAllocateJSFinalObject(MacroAssembler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)->emptyObjectStructure()), resultGPR, scratchGPR, slowPath);
        
        MacroAssembler::Jump done = m_jit.jump();
        
        slowPath.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        callOperation(operationNewObject, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        done.link(&m_jit);
        
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
        GPRTemporary resultTag(this);
        GPRTemporary resultPayload(this);
        GPRReg resultTagGPR = resultTag.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        m_jit.loadPtr(JITCompiler::Address(scopeChain.gpr(), JSVariableObject::offsetOfRegisters()), resultPayloadGPR);
        m_jit.load32(JITCompiler::Address(resultPayloadGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
        m_jit.load32(JITCompiler::Address(resultPayloadGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }
    case PutScopedVar: {
        SpeculateCellOperand scopeChain(this, node.child1());
        GPRTemporary scratchRegister(this);
        GPRReg scratchGPR = scratchRegister.gpr();
        m_jit.loadPtr(JITCompiler::Address(scopeChain.gpr(), JSVariableObject::offsetOfRegisters()), scratchGPR);
        JSValueOperand value(this, node.child2());
        m_jit.store32(value.tagGPR(), JITCompiler::Address(scratchGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
        m_jit.store32(value.payloadGPR(), JITCompiler::Address(scratchGPR, node.varNumber() * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        writeBarrier(scopeChain.gpr(), value.tagGPR(), node.child2(), WriteBarrierForVariableAccess, scratchGPR);
        noResult(m_compileIndex);
        break;
    }
        
    case GetById: {
        if (!node.prediction()) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            break;
        }
        
        if (isCellPrediction(at(node.child1()).prediction())) {
            SpeculateCellOperand base(this, node.child1());
            GPRTemporary resultTag(this, base);
            GPRTemporary resultPayload(this);
            
            GPRReg baseGPR = base.gpr();
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();
            GPRReg scratchGPR;
            
            if (resultTagGPR == baseGPR)
                scratchGPR = resultPayloadGPR;
            else
                scratchGPR = resultTagGPR;
            
            base.use();
            
            cachedGetById(InvalidGPRReg, baseGPR, resultTagGPR, resultPayloadGPR, scratchGPR, node.identifierNumber());
            
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
        GPRReg scratchGPR;

        if (resultTagGPR == basePayloadGPR)
            scratchGPR = resultPayloadGPR;
        else
            scratchGPR = resultTagGPR;
        
        base.use();
        
        JITCompiler::Jump notCell = m_jit.branch32(JITCompiler::NotEqual, baseTagGPR, TrustedImm32(JSValue::CellTag));
        
        cachedGetById(baseTagGPR, basePayloadGPR, resultTagGPR, resultPayloadGPR, scratchGPR, node.identifierNumber(), notCell);
        
        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetArrayLength: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), resultGPR);
        m_jit.load32(MacroAssembler::Address(resultGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), resultGPR);
        
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, resultGPR, MacroAssembler::TrustedImm32(0)));
        
        integerResult(resultGPR, m_compileIndex);
        break;
    }

    case GetStringLength: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        if (!isStringPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));
        
        m_jit.load32(MacroAssembler::Address(baseGPR, JSString::offsetOfLength()), resultGPR);

        integerResult(resultGPR, m_compileIndex);
        break;
    }

    case GetByteArrayLength: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        if (!isByteArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSByteArray::offsetOfStorage()), resultGPR);
        m_jit.load32(MacroAssembler::Address(baseGPR, ByteArray::offsetOfSize()), resultGPR);
        
        integerResult(resultGPR, m_compileIndex);
        break;
    }
    case GetInt8ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->int8ArrayDescriptor(), node, !isInt8ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetInt16ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->int16ArrayDescriptor(), node, !isInt16ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetInt32ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->int32ArrayDescriptor(), node, !isInt32ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint8ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint8ArrayDescriptor(), node, !isUint8ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint16ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint16ArrayDescriptor(), node, !isUint16ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetUint32ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->uint32ArrayDescriptor(), node, !isUint32ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetFloat32ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->float32ArrayDescriptor(), node, !isFloat32ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }
    case GetFloat64ArrayLength: {
        compileGetTypedArrayLength(m_jit.globalData()->float64ArrayDescriptor(), node, !isFloat64ArrayPrediction(m_state.forNode(node.child1()).m_type));
        break;
    }

    case CheckFunction: {
        SpeculateCellOperand function(this, node.child1());
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchWeakPtr(JITCompiler::NotEqual, function.gpr(), node.function()));
        noResult(m_compileIndex);
        break;
    }

    case CheckStructure: {
        if (m_state.forNode(node.child1()).m_structure.isSubsetOf(node.structureSet())) {
            noResult(m_compileIndex);
            break;
        }
        
        SpeculateCellOperand base(this, node.child1());
        
        ASSERT(node.structureSet().size());
        
        if (node.structureSet().size() == 1)
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchWeakPtr(JITCompiler::NotEqual, JITCompiler::Address(base.gpr(), JSCell::structureOffset()), node.structureSet()[0]));
        else {
            GPRTemporary structure(this);
            
            m_jit.loadPtr(JITCompiler::Address(base.gpr(), JSCell::structureOffset()), structure.gpr());
            
            JITCompiler::JumpList done;
            
            for (size_t i = 0; i < node.structureSet().size() - 1; ++i)
                done.append(m_jit.branchWeakPtr(JITCompiler::Equal, structure.gpr(), node.structureSet()[i]));
            
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchWeakPtr(JITCompiler::NotEqual, structure.gpr(), node.structureSet().last()));
            
            done.link(&m_jit);
        }
        
        noResult(m_compileIndex);
        break;
    }
        
    case PutStructure: {
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
        
    case GetPropertyStorage: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this, base);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.loadPtr(JITCompiler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), resultGPR);
        
        storageResult(resultGPR, m_compileIndex);
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
        SpeculateCellOperand base(this, node.child1());
#endif
        StorageOperand storage(this, node.child2());
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

        cachedPutById(baseGPR, valueTagGPR, valuePayloadGPR, node.child2(), scratchGPR, node.identifierNumber(), NotDirect);
        
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

        cachedPutById(baseGPR, valueTagGPR, valuePayloadGPR, node.child2(), scratchGPR, node.identifierNumber(), Direct);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetGlobalVar: {
        GPRTemporary result(this);
        GPRTemporary scratch(this);

        JSVariableObject* globalObject = m_jit.globalObjectFor(node.codeOrigin);
        m_jit.loadPtr(const_cast<WriteBarrier<Unknown>**>(globalObject->addressOfRegisters()), result.gpr());
        m_jit.load32(JITCompiler::tagForGlobalVar(result.gpr(), node.varNumber()), scratch.gpr());
        m_jit.load32(JITCompiler::payloadForGlobalVar(result.gpr(), node.varNumber()), result.gpr());

        jsValueResult(scratch.gpr(), result.gpr(), m_compileIndex);
        break;
    }

    case PutGlobalVar: {
        JSValueOperand value(this, node.child1());
        GPRTemporary globalObject(this);
        GPRTemporary scratch(this);
        
        GPRReg globalObjectReg = globalObject.gpr();
        GPRReg scratchReg = scratch.gpr();

        m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), globalObjectReg);

        writeBarrier(m_jit.globalObjectFor(node.codeOrigin), value.tagGPR(), node.child1(), WriteBarrierForVariableAccess, scratchReg);

        m_jit.loadPtr(MacroAssembler::Address(globalObjectReg, JSVariableObject::offsetOfRegisters()), scratchReg);
        m_jit.store32(value.tagGPR(), JITCompiler::tagForGlobalVar(scratchReg, node.varNumber()));
        m_jit.store32(value.payloadGPR(), JITCompiler::payloadForGlobalVar(scratchReg, node.varNumber()));

        noResult(m_compileIndex);
        break;
    }

    case CheckHasInstance: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary structure(this);

        // Speculate that base 'ImplementsDefaultHasInstance'.
        m_jit.loadPtr(MacroAssembler::Address(base.gpr(), JSCell::structureOffset()), structure.gpr());
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest8(MacroAssembler::Zero, MacroAssembler::Address(structure.gpr(), Structure::typeInfoFlagsOffset()), MacroAssembler::TrustedImm32(ImplementsDefaultHasInstance)));

        noResult(m_compileIndex);
        break;
    }

    case InstanceOf: {
        compileInstanceOf(node);
        break;
    }

    case Phi:
    case Flush:
        ASSERT_NOT_REACHED();

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
        callOperation(operationResolve, resultTag.gpr(), resultPayload.gpr(), identifier(node.identifierNumber()));
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case ResolveBase: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        callOperation(operationResolveBase, resultTag.gpr(), resultPayload.gpr(), identifier(node.identifierNumber()));
        jsValueResult(resultTag.gpr(), resultPayload.gpr(), m_compileIndex);
        break;
    }

    case ResolveBaseStrictPut: {
        flushRegisters();
        GPRResult resultPayload(this);
        GPRResult2 resultTag(this);
        callOperation(operationResolveBaseStrictPut, resultTag.gpr(), resultPayload.gpr(), identifier(node.identifierNumber()));
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
        GlobalResolveInfo* resolveInfoAddress = &(m_jit.codeBlock()->globalResolveInfo(data.resolveInfoIndex));

        // Check Structure of global object
        m_jit.move(JITCompiler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), globalObjectGPR);
        m_jit.move(JITCompiler::TrustedImmPtr(resolveInfoAddress), resolveInfoGPR);
        m_jit.loadPtr(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(GlobalResolveInfo, structure)), resultPayloadGPR);

        JITCompiler::Jump structuresNotMatch = m_jit.branchPtr(JITCompiler::NotEqual, resultPayloadGPR, JITCompiler::Address(globalObjectGPR, JSCell::structureOffset()));

        // Fast case
        m_jit.loadPtr(JITCompiler::Address(globalObjectGPR, JSObject::offsetOfPropertyStorage()), resultPayloadGPR);
        m_jit.load32(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(GlobalResolveInfo, offset)), resolveInfoGPR);
        m_jit.load32(JITCompiler::BaseIndex(resultPayloadGPR, resolveInfoGPR, JITCompiler::TimesEight, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
        m_jit.load32(JITCompiler::BaseIndex(resultPayloadGPR, resolveInfoGPR, JITCompiler::TimesEight, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);

        JITCompiler::Jump wasFast = m_jit.jump();

        structuresNotMatch.link(&m_jit);
        silentSpillAllRegisters(resultTagGPR, resultPayloadGPR);
        callOperation(operationResolveGlobal, resultTagGPR, resultPayloadGPR, resolveInfoGPR, &m_jit.codeBlock()->identifier(data.identifierNumber));
        silentFillAllRegisters(resultTagGPR, resultPayloadGPR);

        wasFast.link(&m_jit);

        jsValueResult(resultTagGPR, resultPayloadGPR, m_compileIndex);
        break;
    }

    case ForceOSRExit: {
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        break;
    }

    case Phantom:
        // This is a no-op.
        noResult(m_compileIndex);
        break;

    case InlineStart:
        ASSERT_NOT_REACHED();
        break;
    }

    if (node.hasResult() && node.mustGenerate())
        use(m_compileIndex);
}

#endif

} } // namespace JSC::DFG

#endif
