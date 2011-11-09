/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "JSByteArray.h"

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

#if USE(JSVALUE64)

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
        if ((node.hasConstant() && !isInt32Constant(nodeIndex)) || info.spillFormat() == DataFormatDouble) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            returnFormat = DataFormatInteger;
            return allocate();
        }
        
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            ASSERT(isInt32Constant(nodeIndex));
            m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
            info.fillInteger(gpr);
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
                info.fillInteger(gpr);
                returnFormat = DataFormatInteger;
                return gpr;
            }
            if (spillFormat == DataFormatInteger) {
                m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
                m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr);
            } else
                m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(gpr, DataFormatJSInteger);
            returnFormat = DataFormatJSInteger;
            return gpr;
        }
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        // Fill as JSValue, and fall through.
        info.fillJSValue(gpr, DataFormatJSInteger);
        m_gprs.unlock(gpr);
    }

    case DataFormatJS: {
        // Check the value is an integer.
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        speculationCheck(JSValueRegs(gpr), nodeIndex, m_jit.branchPtr(MacroAssembler::Below, gpr, GPRInfo::tagTypeNumberRegister));
        info.fillJSValue(gpr, DataFormatJSInteger);
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
                info.fillInteger(gpr);
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
            GPRReg gpr = allocate();

            if (isInt32Constant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(static_cast<double>(valueOfInt32Constant(nodeIndex))))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(fpr);
                return fpr;
            }
            if (isNumberConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfNumberConstant(nodeIndex)))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(fpr);
                return fpr;
            }
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return fprAllocate();
        }
        
        DataFormat spillFormat = info.spillFormat();
        switch (spillFormat) {
        case DataFormatDouble: {
            FPRReg fpr = fprAllocate();
            m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
            m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
            info.fillDouble(fpr);
            return fpr;
        }
            
        case DataFormatInteger: {
            GPRReg gpr = allocate();
            
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillInteger(gpr);
            unlock(gpr);
            break;
        }

        default:
            GPRReg gpr = allocate();

            ASSERT(spillFormat & DataFormatJS);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(gpr, spillFormat);
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
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        return fprAllocate();

    case DataFormatJSCell:
    case DataFormatJS:
    case DataFormatJSBoolean: {
        GPRReg jsValueGpr = info.gpr();
        m_gprs.lock(jsValueGpr);
        FPRReg fpr = fprAllocate();
        GPRReg tempGpr = allocate();

        JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);

        speculationCheck(JSValueRegs(jsValueGpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::Zero, jsValueGpr, GPRInfo::tagTypeNumberRegister));

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
        info.fillDouble(fpr);
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

        info.fillDouble(fpr);
        return fpr;
    }

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
        if (info.spillFormat() == DataFormatInteger || info.spillFormat() == DataFormatDouble) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return allocate();
        }
        
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isCell()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImmPtr(jsValue.asCell()), gpr);
                info.fillJSValue(gpr, DataFormatJSCell);
                return gpr;
            }
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        info.fillJSValue(gpr, DataFormatJS);
        if (info.spillFormat() != DataFormatJSCell)
            speculationCheck(JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagMaskRegister));
        info.fillJSValue(gpr, DataFormatJSCell);
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
        speculationCheck(JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagMaskRegister));
        info.fillJSValue(gpr, DataFormatJSCell);
        return gpr;
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
        if (info.spillFormat() == DataFormatInteger || info.spillFormat() == DataFormatDouble) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return allocate();
        }
        
        GPRReg gpr = allocate();

        if (node.hasConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isBoolean()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsValue)), gpr);
                info.fillJSValue(gpr, DataFormatJSBoolean);
                return gpr;
            }
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        info.fillJSValue(gpr, DataFormatJS);
        if (info.spillFormat() != DataFormatJSBoolean) {
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
            speculationCheck(JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, TrustedImm32(static_cast<int32_t>(~1))), SpeculationRecovery(BooleanSpeculationCheck, gpr, InvalidGPRReg));
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
        }
        info.fillJSValue(gpr, DataFormatJSBoolean);
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
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
        speculationCheck(JSValueRegs(gpr), nodeIndex, m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, TrustedImm32(static_cast<int32_t>(~1))), SpeculationRecovery(BooleanSpeculationCheck, gpr, InvalidGPRReg));
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), gpr);
        info.fillJSValue(gpr, DataFormatJSBoolean);
        return gpr;
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

void SpeculativeJIT::compileObjectEquality(Node& node, void* vptr, PredictionChecker predictionCheck)
{
    SpeculateCellOperand op1(this, node.child1());
    SpeculateCellOperand op2(this, node.child2());
    GPRTemporary result(this, op1);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();
    
    if (!predictionCheck(m_state.forNode(node.child1()).m_type))
        speculationCheck(JSValueRegs(op1GPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op1GPR), MacroAssembler::TrustedImmPtr(vptr)));
    if (!predictionCheck(m_state.forNode(node.child2()).m_type))
        speculationCheck(JSValueRegs(op2GPR), node.child2(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op2GPR), MacroAssembler::TrustedImmPtr(vptr)));
    
    MacroAssembler::Jump falseCase = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, op2GPR);
    m_jit.move(Imm32(ValueTrue), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();
    falseCase.link(&m_jit);
    m_jit.move(Imm32(ValueFalse), resultGPR);
    done.link(&m_jit);

    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compare(Node& node, MacroAssembler::RelationalCondition condition, MacroAssembler::DoubleCondition doubleCondition, S_DFGOperation_EJJ operation)
{
    if (compilePeepHoleBranch(node, condition, doubleCondition, operation))
        return true;

    if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2()))) {
        SpeculateIntegerOperand op1(this, node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        GPRTemporary result(this, op1, op2);
        
        m_jit.compare32(condition, op1.gpr(), op2.gpr(), result.gpr());
        
        // If we add a DataFormatBool, we should use it here.
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
    } else if (Node::shouldSpeculateNumber(at(node.child1()), at(node.child2()))) {
        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        GPRTemporary result(this);
        
        m_jit.move(TrustedImm32(ValueTrue), result.gpr());
        MacroAssembler::Jump trueCase = m_jit.branchDouble(doubleCondition, op1.fpr(), op2.fpr());
        m_jit.xorPtr(Imm32(true), result.gpr());
        trueCase.link(&m_jit);

        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
    } else if (node.op == CompareEq && Node::shouldSpeculateFinalObject(at(node.child1()), at(node.child2())))
        compileObjectEquality(node, m_jit.globalData()->jsFinalObjectVPtr, isFinalObjectPrediction);
    else if (node.op == CompareEq && Node::shouldSpeculateArray(at(node.child1()), at(node.child2())))
        compileObjectEquality(node, m_jit.globalData()->jsArrayVPtr, isArrayPrediction);
    else
        nonSpeculativeNonPeepholeCompare(node, condition, operation);
    
    return false;
}

void SpeculativeJIT::compileValueAdd(Node& node)
{
    JSValueOperand op1(this, node.child1());
    JSValueOperand op2(this, node.child2());
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    flushRegisters();
    
    GPRResult result(this);
    if (isKnownNotNumber(node.child1()) || isKnownNotNumber(node.child2()))
        callOperation(operationValueAddNotNumber, result.gpr(), op1GPR, op2GPR);
    else
        callOperation(operationValueAdd, result.gpr(), op1GPR, op2GPR);
    
    jsValueResult(result.gpr(), m_compileIndex);
}

void SpeculativeJIT::compileObjectOrOtherLogicalNot(NodeIndex nodeIndex, void *vptr, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeIndex);
    GPRTemporary result(this);
    GPRReg valueGPR = value.gpr();
    GPRReg resultGPR = result.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, valueGPR, GPRInfo::tagMaskRegister);
    if (needSpeculationCheck)
        speculationCheck(JSValueRegs(valueGPR), nodeIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(valueGPR), MacroAssembler::TrustedImmPtr(vptr)));
    m_jit.move(TrustedImm32(static_cast<int32_t>(ValueFalse)), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();
    
    notCell.link(&m_jit);
    
    if (needSpeculationCheck) {
        m_jit.move(valueGPR, resultGPR);
        m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), resultGPR);
        speculationCheck(JSValueRegs(valueGPR), nodeIndex, m_jit.branchPtr(MacroAssembler::NotEqual, resultGPR, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
    }
    m_jit.move(TrustedImm32(static_cast<int32_t>(ValueTrue)), resultGPR);
    
    done.link(&m_jit);
    
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
}

void SpeculativeJIT::compileLogicalNot(Node& node)
{
    if (isKnownBoolean(node.child1())) {
        SpeculateBooleanOperand value(this, node.child1());
        GPRTemporary result(this, value);
        
        m_jit.move(value.gpr(), result.gpr());
        m_jit.xorPtr(TrustedImm32(true), result.gpr());
        
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
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
        m_jit.xor32(Imm32(true), result.gpr());
        nonZero.link(&m_jit);
        jsValueResult(result.gpr(), m_compileIndex, DataFormatJSBoolean);
        return;
    }
    
    PredictedType prediction = m_jit.getPrediction(node.child1());
    if (isBooleanPrediction(prediction) || !prediction) {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this); // FIXME: We could reuse, but on speculation fail would need recovery to restore tag (akin to add).
        
        m_jit.move(value.gpr(), result.gpr());
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), result.gpr());
        speculationCheck(JSValueRegs(value.gpr()), node.child1(), m_jit.branchTestPtr(JITCompiler::NonZero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
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
    JITCompiler::Jump fastCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, TrustedImm32(static_cast<int32_t>(~1)));
    
    silentSpillAllRegisters(resultGPR);
    callOperation(dfgConvertJSValueToBoolean, resultGPR, arg1GPR);
    silentFillAllRegisters(resultGPR);
    
    fastCase.link(&m_jit);
    
    m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueTrue)), resultGPR);
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitObjectOrOtherBranch(NodeIndex nodeIndex, BlockIndex taken, BlockIndex notTaken, void *vptr, bool needSpeculationCheck)
{
    JSValueOperand value(this, nodeIndex);
    GPRTemporary scratch(this);
    GPRReg valueGPR = value.gpr();
    GPRReg scratchGPR = scratch.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, valueGPR, GPRInfo::tagMaskRegister);
    if (needSpeculationCheck)
        speculationCheck(JSValueRegs(valueGPR), nodeIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(valueGPR), MacroAssembler::TrustedImmPtr(vptr)));
    addBranch(m_jit.jump(), taken);
    
    notCell.link(&m_jit);
    
    if (needSpeculationCheck) {
        m_jit.move(valueGPR, scratchGPR);
        m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), scratchGPR);
        speculationCheck(JSValueRegs(valueGPR), nodeIndex, m_jit.branchPtr(MacroAssembler::NotEqual, scratchGPR, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
    }
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
    
    noResult(m_compileIndex);
}

void SpeculativeJIT::emitBranch(Node& node)
{
    JSValueOperand value(this, node.child1());
    GPRReg valueGPR = value.gpr();
    
    BlockIndex taken = node.takenBlockIndex();
    BlockIndex notTaken = node.notTakenBlockIndex();
    
    if (isKnownBoolean(node.child1())) {
        MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;
        
        if (taken == (m_block + 1)) {
            condition = MacroAssembler::Zero;
            BlockIndex tmp = taken;
            taken = notTaken;
            notTaken = tmp;
        }
        
        addBranch(m_jit.branchTest32(condition, valueGPR, TrustedImm32(true)), taken);
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
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
        
        bool predictBoolean = isBooleanPrediction(m_jit.getPrediction(node.child1()));
    
        if (predictBoolean) {
            addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsBoolean(false)))), notTaken);
            addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsBoolean(true)))), taken);

            speculationCheck(JSValueRegs(valueGPR), node.child1(), m_jit.jump());
            value.use();
        } else {
            addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsNumber(0)))), notTaken);
            addBranch(m_jit.branchPtr(MacroAssembler::AboveOrEqual, valueGPR, GPRInfo::tagTypeNumberRegister), taken);
    
            if (!predictBoolean) {
                addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsBoolean(false)))), notTaken);
                addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsBoolean(true)))), taken);
            }
    
            value.use();
    
            silentSpillAllRegisters(resultGPR);
            callOperation(dfgConvertJSValueToBoolean, resultGPR, valueGPR);
            silentFillAllRegisters(resultGPR);
    
            addBranch(m_jit.branchTest32(MacroAssembler::NonZero, resultGPR), taken);
            if (notTaken != (m_block + 1))
                addBranch(m_jit.jump(), notTaken);
        }
        
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

    case GetLocal: {
        PredictedType prediction = node.variableAccessData()->prediction();
        AbstractValue& value = block()->valuesAtHead.operand(node.local());

        // If we have no prediction for this local, then don't attempt to compile.
        if (prediction == PredictNone || value.isClear()) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            break;
        }
        
        GPRTemporary result(this);
        if (isInt32Prediction(value.m_type)) {
            m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

            // Like integerResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node.virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderInteger);
            m_generationInfo[virtualRegister].initInteger(m_compileIndex, node.refCount(), result.gpr());
            break;
        }

        m_jit.loadPtr(JITCompiler::addressFor(node.local()), result.gpr());

        // Like jsValueResult, but don't useChildren - our children are phi nodes,
        // and don't represent values within this dataflow with virtual registers.
        VirtualRegister virtualRegister = node.virtualRegister();
        m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);

        DataFormat format;
        if (isCellPrediction(value.m_type))
            format = DataFormatJSCell;
        else if (isBooleanPrediction(value.m_type))
            format = DataFormatJSBoolean;
        else
            format = DataFormatJS;

        m_generationInfo[virtualRegister].initJSValue(m_compileIndex, node.refCount(), result.gpr(), format);
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
        m_codeOriginForOSR = nextNode.codeOrigin;
        
        PredictedType predictedType = node.variableAccessData()->prediction();
        if (isInt32Prediction(predictedType)) {
            SpeculateIntegerOperand value(this, node.child1());
            m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
            noResult(m_compileIndex);
        } else if (isArrayPrediction(predictedType)) {
            SpeculateCellOperand cell(this, node.child1());
            GPRReg cellGPR = cell.gpr();
            if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
                speculationCheck(JSValueRegs(cellGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(cellGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
            m_jit.storePtr(cellGPR, JITCompiler::addressFor(node.local()));
            noResult(m_compileIndex);
        } else if (isByteArrayPrediction(predictedType)) {
            SpeculateCellOperand cell(this, node.child1());
            GPRReg cellGPR = cell.gpr();
            if (!isByteArrayPrediction(m_state.forNode(node.child1()).m_type))
                speculationCheck(JSValueRegs(cellGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(cellGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
            m_jit.storePtr(cellGPR, JITCompiler::addressFor(node.local()));
            noResult(m_compileIndex);
        } else if (isBooleanPrediction(predictedType)) {
            SpeculateBooleanOperand boolean(this, node.child1());
            m_jit.storePtr(boolean.gpr(), JITCompiler::addressFor(node.local()));
            noResult(m_compileIndex);
        } else {
            JSValueOperand value(this, node.child1());
            m_jit.storePtr(value.gpr(), JITCompiler::addressFor(node.local()));
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
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this);

            GPRReg reg1 = op1.gpr();
            GPRReg reg2 = op2.gpr();

            // What is unfortunate is that we cannot take advantage of nodeCanTruncateInteger()
            // here. A multiply on integers performed in the double domain and then truncated to
            // an integer will give a different result than a multiply performed in the integer
            // domain and then truncated, if the integer domain result would have resulted in
            // something bigger than what a 32-bit integer can hold. JavaScript mandates that
            // the semantics are always as if the multiply had been performed in the double
            // domain.
            
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchMul32(MacroAssembler::Overflow, reg1, reg2, result.gpr()));
            
            // Check for negative zero, if the users of this node care about such things.
            if (!nodeCanIgnoreNegativeZero(node.arithNodeFlags())) {
                MacroAssembler::Jump resultNonZero = m_jit.branchTest32(MacroAssembler::NonZero, result.gpr());
                speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, reg1, TrustedImm32(0)));
                speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, reg2, TrustedImm32(0)));
                resultNonZero.link(&m_jit);
            }

            integerResult(result.gpr(), m_compileIndex);
            break;
        }

        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        FPRTemporary result(this, op1, op2);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        
        m_jit.mulDouble(reg1, reg2, result.fpr());
        
        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case ArithDiv: {
        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
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
        if (at(node.child1()).shouldNotSpeculateInteger() || at(node.child2()).shouldNotSpeculateInteger()
            || !node.canSpeculateInteger()) {
            SpeculateDoubleOperand op1(this, node.child1());
            SpeculateDoubleOperand op2(this, node.child2());
            
            FPRReg op1FPR = op1.fpr();
            FPRReg op2FPR = op2.fpr();
            
            flushRegisters();
            
            FPRResult result(this);

            callOperation(fmod, result.fpr(), op1FPR, op2FPR);
            
            doubleResult(result.fpr(), m_compileIndex);
            break;
        }
        
        SpeculateIntegerOperand op1(this, node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);
        GPRReg op1Gpr = op1.gpr();
        GPRReg op2Gpr = op2.gpr();

        speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(JITCompiler::Zero, op2Gpr));

        GPRReg temp2 = InvalidGPRReg;
        if (op2Gpr == X86Registers::eax || op2Gpr == X86Registers::edx) {
            temp2 = allocate();
            m_jit.move(op2Gpr, temp2);
            op2Gpr = temp2;
        }

        m_jit.move(op1Gpr, eax.gpr());
        m_jit.assembler().cdq();
        m_jit.assembler().idivl_r(op2Gpr);

        if (temp2 != InvalidGPRReg)
            unlock(temp2);

        integerResult(edx.gpr(), m_compileIndex);
        break;
    }

    case ArithAbs: {
        if (at(node.child1()).shouldSpeculateInteger() && node.canSpeculateInteger()) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);
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
        FPRTemporary result(this, op1);
        
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
        if (nonSpeculativeStrictEq(node))
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

        ASSERT(node.child3() == NoNode);
        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        GPRTemporary storage(this);

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
            speculationCheck(JSValueRegs(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset())));

        // FIXME: In cases where there are subsequent by_val accesses to the same base it might help to cache
        // the storage pointer - especially if there happens to be another register free right now. If we do so,
        // then we'll need to allocate a new temporary for result.
        GPRTemporary& result = storage;
        m_jit.loadPtr(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), result.gpr());
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::Zero, result.gpr()));

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case PutByVal: {
        PredictedType basePrediction = at(node.child2()).prediction();
        if (!(basePrediction & PredictInt32) && basePrediction) {
            JSValueOperand arg1(this, node.child1());
            JSValueOperand arg2(this, node.child2());
            JSValueOperand arg3(this, node.child3());
            GPRReg arg1GPR = arg1.gpr();
            GPRReg arg2GPR = arg2.gpr();
            GPRReg arg3GPR = arg3.gpr();
            flushRegisters();
            
            callOperation(m_jit.codeBlock()->isStrictMode() ? operationPutByValStrict : operationPutByValNonStrict, arg1GPR, arg2GPR, arg3GPR);
            
            noResult(m_compileIndex);
            break;
        }

        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        if (at(node.child1()).shouldSpeculateByteArray()) {
            compilePutByValForByteArray(base.gpr(), property.gpr(), node);
            break;
        }

        JSValueOperand value(this, node.child3());
        GPRTemporary scratch(this);

        // Map base, property & value into registers, allocate a scratch register.
        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg valueReg = value.gpr();
        GPRReg scratchReg = scratch.gpr();
        
        if (!m_compileOkay)
            return;
        
        writeBarrier(baseReg, value.gpr(), node.child3(), WriteBarrierForPropertyAccess, scratchReg);

        // Check that base is an array, and that property is contained within m_vector (< m_vectorLength).
        // If we have predicted the base to be type array, we can skip the check.
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueRegs(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));

        base.use();
        property.use();
        value.use();
        
        MacroAssembler::Jump withinArrayBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset()));

        // Code to handle put beyond array bounds.
        silentSpillAllRegisters(scratchReg);
        JITCompiler::Call functionCall = callOperation(operationPutByValBeyondArrayBounds, baseReg, propertyReg, valueReg);
        silentFillAllRegisters(scratchReg);
        JITCompiler::Jump wasBeyondArrayBounds = m_jit.jump();

        withinArrayBounds.link(&m_jit);

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

        wasBeyondArrayBounds.link(&m_jit);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByValAlias: {
        PredictedType basePrediction = at(node.child2()).prediction();
        ASSERT_UNUSED(basePrediction, (basePrediction & PredictInt32) || !basePrediction);
            
        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        if (at(node.child1()).shouldSpeculateByteArray()) {
            compilePutByValForByteArray(base.gpr(), property.gpr(), node);
            break;
        }

        JSValueOperand value(this, node.child3());
        GPRTemporary scratch(this);
        
        GPRReg baseReg = base.gpr();
        GPRReg scratchReg = scratch.gpr();

        writeBarrier(base.gpr(), value.gpr(), node.child3(), WriteBarrierForPropertyAccess, scratchReg);

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

        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueRegs(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);
        m_jit.load32(MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), storageLengthGPR);
        
        // Refuse to handle bizarre lengths.
        speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::Above, storageLengthGPR, TrustedImm32(0x7ffffffe)));
        
        MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        m_jit.storePtr(valueGPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        
        m_jit.add32(Imm32(1), storageLengthGPR);
        m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.add32(Imm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, storageLengthGPR);
        
        MacroAssembler::Jump done = m_jit.jump();
        
        slowPath.link(&m_jit);
        
        silentSpillAllRegisters(storageLengthGPR);
        callOperation(operationArrayPush, storageLengthGPR, valueGPR, baseGPR);
        silentFillAllRegisters(storageLengthGPR);
        
        done.link(&m_jit);
        
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
        
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueRegs(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);
        m_jit.load32(MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), storageLengthGPR);
        
        MacroAssembler::Jump emptyArrayCase = m_jit.branchTest32(MacroAssembler::Zero, storageLengthGPR);
        
        m_jit.sub32(Imm32(1), storageLengthGPR);
        
        MacroAssembler::Jump slowCase = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        m_jit.loadPtr(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), valueGPR);
        
        m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));

        MacroAssembler::Jump holeCase = m_jit.branchTestPtr(MacroAssembler::Zero, valueGPR);
        
        m_jit.storePtr(MacroAssembler::TrustedImmPtr(0), MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        m_jit.sub32(MacroAssembler::Imm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
        MacroAssembler::JumpList done;
        
        done.append(m_jit.jump());
        
        holeCase.link(&m_jit);
        emptyArrayCase.link(&m_jit);
        m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsUndefined())), valueGPR);
        done.append(m_jit.jump());
        
        slowCase.link(&m_jit);
        
        silentSpillAllRegisters(valueGPR);
        callOperation(operationArrayPop, valueGPR, baseGPR);
        silentFillAllRegisters(valueGPR);
        
        done.link(&m_jit);
        
        jsValueResult(valueGPR, m_compileIndex);
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
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
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
        
        bool wasPrimitive = isKnownNumeric(node.child1()) || isKnownBoolean(node.child1());
        
        JSValueOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        
        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();
        
        op1.use();
        
        if (wasPrimitive)
            m_jit.move(op1GPR, resultGPR);
        else {
            MacroAssembler::JumpList alreadyPrimitive;
            
            alreadyPrimitive.append(m_jit.branchTestPtr(MacroAssembler::NonZero, op1GPR, GPRInfo::tagMaskRegister));
            alreadyPrimitive.append(m_jit.branchPtr(MacroAssembler::Equal, MacroAssembler::Address(op1GPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));
            
            silentSpillAllRegisters(resultGPR);
            callOperation(operationToPrimitive, resultGPR, op1GPR);
            silentFillAllRegisters(resultGPR);
            
            MacroAssembler::Jump done = m_jit.jump();
            
            alreadyPrimitive.link(&m_jit);
            m_jit.move(op1GPR, resultGPR);
            
            done.link(&m_jit);
        }
        
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
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
            GPRReg opGPR = operand.gpr();
            operand.use();
            
            m_jit.storePtr(opGPR, buffer + operandIdx);
        }
        
        flushRegisters();
        
        GPRResult result(this);
        
        callOperation(op == StrCat ? operationStrCat : operationNewArray, result.gpr(), buffer, node.numChildren());
        
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
        if (isObjectPrediction(m_state.forNode(node.child1()).m_type)) {
            SpeculateCellOperand thisValue(this, node.child1());
            GPRTemporary result(this, thisValue);
            m_jit.move(thisValue.gpr(), result.gpr());
            cellResult(result.gpr(), m_compileIndex);
            break;
        }
        
        if (isOtherPrediction(at(node.child1()).prediction())) {
            JSValueOperand thisValue(this, node.child1());
            GPRTemporary scratch(this, thisValue);
            GPRReg thisValueGPR = thisValue.gpr();
            GPRReg scratchGPR = scratch.gpr();
            
            if (!isOtherPrediction(m_state.forNode(node.child1()).m_type)) {
                m_jit.move(thisValueGPR, scratchGPR);
                m_jit.andPtr(MacroAssembler::TrustedImm32(~TagBitUndefined), scratchGPR);
                speculationCheck(JSValueRegs(thisValueGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, scratchGPR, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))));
            }
            
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
                speculationCheck(JSValueRegs(thisValueGPR), node.child1(), m_jit.branchPtr(JITCompiler::Equal, JITCompiler::Address(thisValueGPR), JITCompiler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));
            
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
                speculationCheck(JSValueRegs(protoGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(protoGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsFinalObjectVPtr)));
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
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            break;
        }
        
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this, base);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR;
        
        if (resultGPR == baseGPR)
            scratchGPR = tryAllocate();
        else
            scratchGPR = resultGPR;
        
        base.use();

        cachedGetById(baseGPR, resultGPR, scratchGPR, node.identifierNumber());

        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetArrayLength: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueRegs(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        
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
            speculationCheck(JSValueRegs(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));
        
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
            speculationCheck(JSValueRegs(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
        
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSByteArray::offsetOfStorage()), resultGPR);
        m_jit.load32(MacroAssembler::Address(baseGPR, ByteArray::offsetOfSize()), resultGPR);

        integerResult(resultGPR, m_compileIndex);
        break;
    }

    case CheckFunction: {
        SpeculateCellOperand function(this, node.child1());
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(JITCompiler::NotEqual, function.gpr(), JITCompiler::TrustedImmPtr(node.function())));
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
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(JITCompiler::NotEqual, JITCompiler::Address(base.gpr(), JSCell::structureOffset()), JITCompiler::TrustedImmPtr(node.structureSet()[0])));
        else {
            GPRTemporary structure(this);
            
            m_jit.loadPtr(JITCompiler::Address(base.gpr(), JSCell::structureOffset()), structure.gpr());
            
            JITCompiler::JumpList done;
            
            for (size_t i = 0; i < node.structureSet().size() - 1; ++i)
                done.append(m_jit.branchPtr(JITCompiler::Equal, structure.gpr(), JITCompiler::TrustedImmPtr(node.structureSet()[i])));
            
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(JITCompiler::NotEqual, structure.gpr(), JITCompiler::TrustedImmPtr(node.structureSet().last())));
            
            done.link(&m_jit);
        }
        
        noResult(m_compileIndex);
        break;
    }
        
    case PutStructure: {
        SpeculateCellOperand base(this, node.child1());
        GPRReg baseGPR = base.gpr();
        
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
        SpeculateCellOperand base(this, node.child1());
#endif
        StorageOperand storage(this, node.child2());
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
        
    case GetMethod: {
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary result(this, base);

        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR;
        
        if (resultGPR == baseGPR)
            scratchGPR = tryAllocate();
        else
            scratchGPR = resultGPR;
        
        base.use();

        cachedGetMethod(baseGPR, resultGPR, scratchGPR, node.identifierNumber());

        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case CheckMethod: {
        MethodCheckData& methodCheckData = m_jit.graph().m_methodCheckData[node.methodCheckDataIndex()];
        
        SpeculateCellOperand base(this, node.child1());
        GPRTemporary scratch(this); // this needs to be a separate register, unfortunately.
        GPRReg baseGPR = base.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        if (!m_state.forNode(node.child1()).m_structure.doesNotContainAnyOtherThan(methodCheckData.structure))
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(JITCompiler::NotEqual, JITCompiler::Address(baseGPR, JSCell::structureOffset()), JITCompiler::TrustedImmPtr(methodCheckData.structure)));
        if (methodCheckData.prototype != m_jit.globalObjectFor(node.codeOrigin)->methodCallDummy()) {
            m_jit.move(JITCompiler::TrustedImmPtr(methodCheckData.prototype->structureAddress()), scratchGPR);
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(JITCompiler::NotEqual, JITCompiler::Address(scratchGPR), JITCompiler::TrustedImmPtr(methodCheckData.prototypeStructure)));
        }
        
        useChildren(node);
        initConstantInfo(m_compileIndex);
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

        cachedPutById(baseGPR, valueGPR, node.child2(), scratchGPR, node.identifierNumber(), NotDirect);
        
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

        cachedPutById(baseGPR, valueGPR, node.child2(), scratchGPR, node.identifierNumber(), Direct);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetGlobalVar: {
        GPRTemporary result(this);

        JSVariableObject* globalObject = m_jit.globalObjectFor(node.codeOrigin);
        m_jit.loadPtr(globalObject->addressOfRegisters(), result.gpr());
        m_jit.loadPtr(JITCompiler::addressForGlobalVar(result.gpr(), node.varNumber()), result.gpr());

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case PutGlobalVar: {
        JSValueOperand value(this, node.child1());
        GPRTemporary globalObject(this);
        GPRTemporary scratch(this);
        
        GPRReg globalObjectReg = globalObject.gpr();
        GPRReg scratchReg = scratch.gpr();

        m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.globalObjectFor(node.codeOrigin)), globalObjectReg);

        writeBarrier(m_jit.globalObjectFor(node.codeOrigin), value.gpr(), node.child1(), WriteBarrierForVariableAccess, scratchReg);

        m_jit.loadPtr(MacroAssembler::Address(globalObjectReg, JSVariableObject::offsetOfRegisters()), scratchReg);
        m_jit.storePtr(value.gpr(), JITCompiler::addressForGlobalVar(scratchReg, node.varNumber()));

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
        SpeculateCellOperand value(this, node.child1());
        // Base unused since we speculate default InstanceOf behaviour in CheckHasInstance.
        SpeculateCellOperand prototype(this, node.child3());

        GPRTemporary scratch(this);

        GPRReg valueReg = value.gpr();
        GPRReg prototypeReg = prototype.gpr();
        GPRReg scratchReg = scratch.gpr();

        // Check that prototype is an object.
        m_jit.loadPtr(MacroAssembler::Address(prototypeReg, JSCell::structureOffset()), scratchReg);
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchIfNotObject(scratchReg));

        // Initialize scratchReg with the value being checked.
        m_jit.move(valueReg, scratchReg);

        // Walk up the prototype chain of the value (in scratchReg), comparing to prototypeReg.
        MacroAssembler::Label loop(&m_jit);
        m_jit.loadPtr(MacroAssembler::Address(scratchReg, JSCell::structureOffset()), scratchReg);
        m_jit.loadPtr(MacroAssembler::Address(scratchReg, Structure::prototypeOffset()), scratchReg);
        MacroAssembler::Jump isInstance = m_jit.branchPtr(MacroAssembler::Equal, scratchReg, prototypeReg);
        m_jit.branchTestPtr(MacroAssembler::Zero, scratchReg, GPRInfo::tagMaskRegister).linkTo(loop, &m_jit);

        // No match - result is false.
        m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(false))), scratchReg);
        MacroAssembler::Jump putResult = m_jit.jump();

        isInstance.link(&m_jit);
        m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(true))), scratchReg);

        putResult.link(&m_jit);
        jsValueResult(scratchReg, m_compileIndex, DataFormatJSBoolean);
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
        JITCompiler::Jump structuresMatch = m_jit.branchPtr(JITCompiler::Equal, resultGPR, JITCompiler::Address(globalObjectGPR, JSCell::structureOffset()));

        silentSpillAllRegisters(resultGPR);
        JITCompiler::Call functionCall = callOperation(operationResolveGlobal, resultGPR, resolveInfoGPR, &m_jit.codeBlock()->identifier(data.identifierNumber));
        silentFillAllRegisters(resultGPR);

        JITCompiler::Jump wasSlow = m_jit.jump();

        // Fast case
        structuresMatch.link(&m_jit);
        m_jit.loadPtr(JITCompiler::Address(globalObjectGPR, JSObject::offsetOfPropertyStorage()), resultGPR);
        m_jit.load32(JITCompiler::Address(resolveInfoGPR, OBJECT_OFFSETOF(GlobalResolveInfo, offset)), resolveInfoGPR);
        m_jit.loadPtr(JITCompiler::BaseIndex(resultGPR, resolveInfoGPR, JITCompiler::ScalePtr), resultGPR);

        wasSlow.link(&m_jit);

        jsValueResult(resultGPR, m_compileIndex);
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
