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

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

template<bool strict>
GPRReg SpeculativeJIT::fillSpeculateIntInternal(NodeIndex nodeIndex, DataFormat& returnFormat)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (node.isConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            if (isInt32Constant(nodeIndex)) {
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                info.fillInteger(gpr);
                returnFormat = DataFormatInteger;
                return gpr;
            }
            terminateSpeculativeExecution();
            returnFormat = DataFormatInteger;
            return allocate();
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);

            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);

            if (spillFormat == DataFormatJSInteger) {
                // If we know this was spilled as an integer we can fill without checking.
                if (strict) {
                    m_jit.load32(JITCompiler::addressFor(virtualRegister), gpr);
                    info.fillInteger(gpr);
                    returnFormat = DataFormatInteger;
                    return gpr;
                }
                m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
                info.fillJSValue(gpr, DataFormatJSInteger);
                returnFormat = DataFormatJSInteger;
                return gpr;
            }
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
        }

        // Fill as JSValue, and fall through.
        info.fillJSValue(gpr, DataFormatJSInteger);
        m_gprs.unlock(gpr);
    }

    case DataFormatJS: {
        // Check the value is an integer.
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        speculationCheck(m_jit.branchPtr(MacroAssembler::Below, gpr, GPRInfo::tagTypeNumberRegister));
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
    case DataFormatJSDouble:
    case DataFormatJSCell: {
        terminateSpeculativeExecution();
        returnFormat = DataFormatInteger;
        return allocate();
    }
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
}

SpeculationCheck::SpeculationCheck(MacroAssembler::Jump check, SpeculativeJIT* jit, unsigned recoveryIndex)
    : m_check(check)
    , m_nodeIndex(jit->m_compileIndex)
    , m_recoveryIndex(recoveryIndex)
{
    for (gpr_iterator iter = jit->m_gprs.begin(); iter != jit->m_gprs.end(); ++iter) {
        if (iter.name() != InvalidVirtualRegister) {
            GenerationInfo& info =  jit->m_generationInfo[iter.name()];
            m_gprInfo[iter.index()].nodeIndex = info.nodeIndex();
            m_gprInfo[iter.index()].format = info.registerFormat();
            ASSERT(m_gprInfo[iter.index()].format != DataFormatNone);
            m_gprInfo[iter.index()].isSpilled = info.spillFormat() != DataFormatNone;
        } else
            m_gprInfo[iter.index()].nodeIndex = NoNode;
    }
    for (fpr_iterator iter = jit->m_fprs.begin(); iter != jit->m_fprs.end(); ++iter) {
        if (iter.name() != InvalidVirtualRegister) {
            GenerationInfo& info =  jit->m_generationInfo[iter.name()];
            ASSERT(info.registerFormat() == DataFormatDouble);
            m_fprInfo[iter.index()].nodeIndex = info.nodeIndex();
            m_fprInfo[iter.index()].format = DataFormatDouble;
            m_fprInfo[iter.index()].isSpilled = info.spillFormat() != DataFormatNone;
        } else
            m_fprInfo[iter.index()].nodeIndex = NoNode;
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
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {
        GPRReg gpr = allocate();

        if (node.isConstant()) {
            if (isInt32Constant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(static_cast<double>(valueOfInt32Constant(nodeIndex))))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(fpr);
                return fpr;
            }
            if (isDoubleConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfDoubleConstant(nodeIndex)))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(fpr);
                return fpr;
            }
            terminateSpeculativeExecution();
            return fprAllocate();
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(gpr, spillFormat);
            unlock(gpr);
        }
    }

    switch (info.registerFormat()) {
    case DataFormatNone:
        // Should have filled, above.
        ASSERT_NOT_REACHED();
        
    case DataFormatCell:
    case DataFormatJSCell:
    case DataFormatJS: {
        GPRReg jsValueGpr = info.gpr();
        m_gprs.lock(jsValueGpr);
        FPRReg fpr = fprAllocate();
        GPRReg tempGpr = allocate();

        JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);

        speculationCheck(m_jit.branchTestPtr(MacroAssembler::Zero, jsValueGpr, GPRInfo::tagTypeNumberRegister));

        // First, if we get here we have a double encoded as a JSValue
        m_jit.move(jsValueGpr, tempGpr);
        m_jit.addPtr(GPRInfo::tagTypeNumberRegister, tempGpr);
        m_jit.movePtrToDouble(tempGpr, fpr);
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
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (node.isConstant()) {
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            if (jsValue.isCell()) {
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                m_jit.move(MacroAssembler::TrustedImmPtr(jsValue.asCell()), gpr);
                info.fillJSValue(gpr, DataFormatJSCell);
                return gpr;
            }
            terminateSpeculativeExecution();
            return gpr;
        }
        ASSERT(info.spillFormat() & DataFormatJS);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);

        info.fillJSValue(gpr, DataFormatJS);
        if (info.spillFormat() != DataFormatJSCell)
            speculationCheck(m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagMaskRegister));
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
        speculationCheck(m_jit.branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagMaskRegister));
        info.fillJSValue(gpr, DataFormatJSCell);
        return gpr;
    }

    case DataFormatJSInteger:
    case DataFormatInteger:
    case DataFormatJSDouble:
    case DataFormatDouble: {
        terminateSpeculativeExecution();
        return allocate();
    }
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
}

void SpeculativeJIT::compilePeepHoleIntegerBranch(Node& node, NodeIndex branchNodeIndex, JITCompiler::RelationalCondition condition)
{
    Node& branchNode = m_jit.graph()[branchNodeIndex];
    BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.takenBytecodeOffset());
    BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.notTakenBytecodeOffset());

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == (m_block + 1)) {
        condition = JITCompiler::invert(condition);
        BlockIndex tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    if (isInt32Constant(node.child1())) {
        int32_t imm = valueOfInt32Constant(node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        addBranch(m_jit.branch32(condition, JITCompiler::Imm32(imm), op2.gpr()), taken);
    } else if (isInt32Constant(node.child2())) {
        SpeculateIntegerOperand op1(this, node.child1());
        int32_t imm = valueOfInt32Constant(node.child2());
        addBranch(m_jit.branch32(condition, op1.gpr(), JITCompiler::Imm32(imm)), taken);
    } else {
        SpeculateIntegerOperand op1(this, node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        addBranch(m_jit.branch32(condition, op1.gpr(), op2.gpr()), taken);
    }

    // Check for fall through, otherwise we need to jump.
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

JITCompiler::Jump SpeculativeJIT::convertToDouble(GPRReg value, FPRReg result, GPRReg tmp)
{
    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, value, GPRInfo::tagTypeNumberRegister);
    
    JITCompiler::Jump notNumber = m_jit.branchTestPtr(MacroAssembler::Zero, value, GPRInfo::tagTypeNumberRegister);
    
    m_jit.move(value, tmp);
    m_jit.addPtr(GPRInfo::tagTypeNumberRegister, tmp);
    m_jit.movePtrToDouble(tmp, result);
    
    JITCompiler::Jump done = m_jit.jump();
    
    isInteger.link(&m_jit);
    
    m_jit.convertInt32ToDouble(value, result);
    
    done.link(&m_jit);

    return notNumber;
}

void SpeculativeJIT::compilePeepHoleDoubleBranch(Node& node, NodeIndex branchNodeIndex, JITCompiler::DoubleCondition condition, Z_DFGOperation_EJJ operation)
{
    Node& branchNode = m_jit.graph()[branchNodeIndex];
    BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.takenBytecodeOffset());
    BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.notTakenBytecodeOffset());
    
    bool op1Numeric = isKnownNumeric(node.child1());
    bool op2Numeric = isKnownNumeric(node.child2());
    
    if (op1Numeric && op2Numeric) {
        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        
        addBranch(m_jit.branchDouble(condition, op1.fpr(), op2.fpr()), taken);
    } else if (op1Numeric) {
        SpeculateDoubleOperand op1(this, node.child1());
        JSValueOperand op2(this, node.child2());
        
        FPRTemporary fprTmp(this);
        GPRTemporary gprTmp(this);
        
        FPRReg op1FPR = op1.fpr();
        GPRReg op2GPR = op2.gpr();
        FPRReg op2FPR = fprTmp.fpr();
        GPRReg gpr = gprTmp.gpr();
        
        JITCompiler::Jump slowPath = convertToDouble(op2GPR, op2FPR, gpr);
        
        addBranch(m_jit.branchDouble(condition, op1FPR, op2FPR), taken);
        addBranch(m_jit.jump(), notTaken);
        
        slowPath.link(&m_jit);
        
        boxDouble(op1FPR, gpr);
        
        silentSpillAllRegisters(gpr);
        setupStubArguments(gpr, op2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operation);
        m_jit.move(GPRInfo::returnValueGPR, gpr);
        silentFillAllRegisters(gpr);
        
        addBranch(m_jit.branchTest8(JITCompiler::NonZero, gpr), taken);
    } else {
        JSValueOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        
        FPRTemporary fprTmp(this);
        GPRTemporary gprTmp(this);
        
        FPRReg op2FPR = op2.fpr();
        GPRReg op1GPR = op1.gpr();
        FPRReg op1FPR = fprTmp.fpr();
        GPRReg gpr = gprTmp.gpr();
        
        JITCompiler::Jump slowPath = convertToDouble(op1GPR, op1FPR, gpr);
        
        addBranch(m_jit.branchDouble(condition, op1FPR, op2FPR), taken);
        addBranch(m_jit.jump(), notTaken);
        
        slowPath.link(&m_jit);
        
        boxDouble(op2FPR, gpr);
        
        silentSpillAllRegisters(gpr);
        setupStubArguments(op1GPR, gpr);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operation);
        m_jit.move(GPRInfo::returnValueGPR, gpr);
        silentFillAllRegisters(gpr);
        
        addBranch(m_jit.branchTest8(JITCompiler::NonZero, gpr), taken);
    }
    
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compare(Node& node, MacroAssembler::RelationalCondition condition, MacroAssembler::DoubleCondition doubleCondition, Z_DFGOperation_EJJ operation)
{
    // Fused compare & branch.
    NodeIndex branchNodeIndex = detectPeepHoleBranch();
    if (branchNodeIndex != NoNode) {
        // detectPeepHoleBranch currently only permits the branch to be the very next node,
        // so can be no intervening nodes to also reference the compare. 
        ASSERT(node.adjustedRefCount() == 1);

        if (shouldSpeculateInteger(node.child1(), node.child2())) {
            compilePeepHoleIntegerBranch(node, branchNodeIndex, condition);
            use(node.child1());
            use(node.child2());
        } else if (isKnownNumeric(node.child1()) || isKnownNumeric(node.child2())) {
            compilePeepHoleDoubleBranch(node, branchNodeIndex, doubleCondition, operation);
            use(node.child1());
            use(node.child2());
        } else
            nonSpeculativePeepholeBranch(node, branchNodeIndex, condition, operation);

        m_compileIndex = branchNodeIndex;
        return true;
    }

    if (isKnownNotInteger(node.child1()) || isKnownNotInteger(node.child2()))
        nonSpeculativeNonPeepholeCompare(node, condition, operation);
    else {
        // Normal case, not fused to branch.
        SpeculateIntegerOperand op1(this, node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        GPRTemporary result(this, op1, op2);
        
        m_jit.compare32(condition, op1.gpr(), op2.gpr(), result.gpr());
        
        // If we add a DataFormatBool, we should use it here.
        m_jit.or32(TrustedImm32(ValueFalse), result.gpr());
        jsValueResult(result.gpr(), m_compileIndex);
    }
    
    return false;
}

void SpeculativeJIT::compile(Node& node)
{
    NodeType op = node.op;

    switch (op) {
    case JSConstant:
        initConstantInfo(m_compileIndex);
        break;

    case GetLocal: {
        GPRTemporary result(this);
        PredictedType prediction = m_jit.graph().getPrediction(node.local());
        if (isInt32Prediction(prediction)) {
            m_jit.load32(JITCompiler::payloadFor(node.local()), result.gpr());

            // Like integerResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node.virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderInteger);
            m_generationInfo[virtualRegister].initInteger(m_compileIndex, node.refCount(), result.gpr());
        } else {
            m_jit.loadPtr(JITCompiler::addressFor(node.local()), result.gpr());

            // Like jsValueResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node.virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);
            m_generationInfo[virtualRegister].initJSValue(m_compileIndex, node.refCount(), result.gpr(), isArrayPrediction(prediction) ? DataFormatJSCell : DataFormatJS);
        }
        break;
    }

    case SetLocal: {
        PredictedType predictedType = m_jit.graph().getPrediction(node.local());
        if (isInt32Prediction(predictedType)) {
            SpeculateIntegerOperand value(this, node.child1());
            m_jit.store32(value.gpr(), JITCompiler::payloadFor(node.local()));
            noResult(m_compileIndex);
        } else if (isArrayPrediction(predictedType)) {
            SpeculateCellOperand cell(this, node.child1());
            GPRReg cellGPR = cell.gpr();
            speculationCheck(m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(cellGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
            m_jit.storePtr(cellGPR, JITCompiler::addressFor(node.local()));
            noResult(m_compileIndex);
        } else {
            JSValueOperand value(this, node.child1());
            m_jit.storePtr(value.gpr(), JITCompiler::addressFor(node.local()));
            noResult(m_compileIndex);
        }
        break;
    }

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
        IntegerOperand op1(this, node.child1());
        GPRTemporary result(this, op1);

        // Test the operand is positive.
        speculationCheck(m_jit.branch32(MacroAssembler::LessThan, op1.gpr(), TrustedImm32(0)));

        m_jit.move(op1.gpr(), result.gpr());
        integerResult(result.gpr(), m_compileIndex, op1.format());
        break;
    }

    case ValueToInt32: {
        SpeculateIntegerOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        m_jit.move(op1.gpr(), result.gpr());
        integerResult(result.gpr(), m_compileIndex, op1.format());
        break;
    }

    case ValueToNumber: {
        if (shouldSpeculateInteger(node.child1())) {
            SpeculateIntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);
            m_jit.move(op1.gpr(), result.gpr());
            integerResult(result.gpr(), m_compileIndex, op1.format());
            break;
        }
        SpeculateDoubleOperand op1(this, node.child1());
        FPRTemporary result(this, op1);
        m_jit.moveDouble(op1.fpr(), result.fpr());
        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case ValueAdd:
    case ArithAdd: {
        if (shouldSpeculateInteger(node.child1(), node.child2())) {
            if (isInt32Constant(node.child1())) {
                int32_t imm1 = valueOfInt32Constant(node.child1());
                SpeculateIntegerOperand op2(this, node.child2());
                GPRTemporary result(this);

                speculationCheck(m_jit.branchAdd32(MacroAssembler::Overflow, op2.gpr(), Imm32(imm1), result.gpr()));

                integerResult(result.gpr(), m_compileIndex);
                break;
            }
                
            if (isInt32Constant(node.child2())) {
                SpeculateIntegerOperand op1(this, node.child1());
                int32_t imm2 = valueOfInt32Constant(node.child2());
                GPRTemporary result(this);
                
                speculationCheck(m_jit.branchAdd32(MacroAssembler::Overflow, op1.gpr(), Imm32(imm2), result.gpr()));

                integerResult(result.gpr(), m_compileIndex);
                break;
            }
                
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op1, op2);

            GPRReg gpr1 = op1.gpr();
            GPRReg gpr2 = op2.gpr();
            GPRReg gprResult = result.gpr();
            MacroAssembler::Jump check = m_jit.branchAdd32(MacroAssembler::Overflow, gpr1, gpr2, gprResult);

            if (gpr1 == gprResult)
                speculationCheck(check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr2));
            else if (gpr2 == gprResult)
                speculationCheck(check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr1));
            else
                speculationCheck(check);

            integerResult(gprResult, m_compileIndex);
            break;
        }

        SpeculateDoubleOperand op1(this, node.child1());
        SpeculateDoubleOperand op2(this, node.child2());
        FPRTemporary result(this, op1, op2);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        m_jit.addDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), m_compileIndex);
        break;
    }

    case ArithSub: {
        if (shouldSpeculateInteger(node.child1(), node.child2())) {
            if (isInt32Constant(node.child2())) {
                SpeculateIntegerOperand op1(this, node.child1());
                int32_t imm2 = valueOfInt32Constant(node.child2());
                GPRTemporary result(this);

                speculationCheck(m_jit.branchSub32(MacroAssembler::Overflow, op1.gpr(), Imm32(imm2), result.gpr()));

                integerResult(result.gpr(), m_compileIndex);
                break;
            }
                
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this);

            speculationCheck(m_jit.branchSub32(MacroAssembler::Overflow, op1.gpr(), op2.gpr(), result.gpr()));

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
        if (shouldSpeculateInteger(node.child1(), node.child2())) {
            SpeculateIntegerOperand op1(this, node.child1());
            SpeculateIntegerOperand op2(this, node.child2());
            GPRTemporary result(this);

            GPRReg reg1 = op1.gpr();
            GPRReg reg2 = op2.gpr();
            speculationCheck(m_jit.branchMul32(MacroAssembler::Overflow, reg1, reg2, result.gpr()));

            MacroAssembler::Jump resultNonZero = m_jit.branchTest32(MacroAssembler::NonZero, result.gpr());
            speculationCheck(m_jit.branch32(MacroAssembler::LessThan, reg1, TrustedImm32(0)));
            speculationCheck(m_jit.branch32(MacroAssembler::LessThan, reg2, TrustedImm32(0)));
            resultNonZero.link(&m_jit);

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
        SpeculateIntegerOperand op1(this, node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);
        GPRReg op1Gpr = op1.gpr();
        GPRReg op2Gpr = op2.gpr();

        speculationCheck(m_jit.branchTest32(JITCompiler::Zero, op2Gpr));

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

    case LogicalNot: {
        JSValueOperand value(this, node.child1());
        GPRTemporary result(this); // FIXME: We could reuse, but on speculation fail would need recovery to restore tag (akin to add).

        m_jit.move(value.gpr(), result.gpr());
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), result.gpr());
        speculationCheck(m_jit.branchTestPtr(JITCompiler::NonZero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueTrue)), result.gpr());

        // If we add a DataFormatBool, we should use it here.
        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

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

    case GetByVal: {
        NodeIndex alias = node.child3();
        if (alias != NoNode) {
            // FIXME: result should be able to reuse child1, child2. Should have an 'UnusedOperand' type.
            JSValueOperand aliasedValue(this, node.child3());
            GPRTemporary result(this, aliasedValue);
            m_jit.move(aliasedValue.gpr(), result.gpr());
            jsValueResult(result.gpr(), m_compileIndex);
            break;
        }

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
        Node& baseNode = m_jit.graph()[node.child1()];
        if (baseNode.op != GetLocal || !isArrayPrediction(m_jit.graph().getPrediction(baseNode.local())))
            speculationCheck(m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        speculationCheck(m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset())));

        // FIXME: In cases where there are subsequent by_val accesses to the same base it might help to cache
        // the storage pointer - especially if there happens to be another register free right now. If we do so,
        // then we'll need to allocate a new temporary for result.
        GPRTemporary& result = storage;
        m_jit.loadPtr(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), result.gpr());
        speculationCheck(m_jit.branchTestPtr(MacroAssembler::Zero, result.gpr()));

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case PutByVal: {
        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        JSValueOperand value(this, node.child3());
        GPRTemporary scratch(this);

        // Map base, property & value into registers, allocate a scratch register.
        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg valueReg = value.gpr();
        GPRReg scratchReg = scratch.gpr();
        
        if (!m_compileOkay)
            return;
        
        writeBarrier(m_jit, baseReg, scratchReg);

        // Check that base is an array, and that property is contained within m_vector (< m_vectorLength).
        // If we have predicted the base to be type array, we can skip the check.
        Node& baseNode = m_jit.graph()[node.child1()];
        if (baseNode.op != GetLocal || !isArrayPrediction(m_jit.graph().getPrediction(baseNode.local())))
            speculationCheck(m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));

        base.use();
        property.use();
        value.use();
        
        MacroAssembler::Jump withinArrayBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(baseReg, JSArray::vectorLengthOffset()));

        // Code to handle put beyond array bounds.
        silentSpillAllRegisters(scratchReg);
        setupStubArguments(baseReg, propertyReg, valueReg);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        JITCompiler::Call functionCall = appendCallWithExceptionCheck(operationPutByValBeyondArrayBounds);
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
        SpeculateCellOperand base(this, node.child1());
        SpeculateStrictInt32Operand property(this, node.child2());
        JSValueOperand value(this, node.child3());
        GPRTemporary scratch(this);
        
        GPRReg baseReg = base.gpr();
        GPRReg scratchReg = scratch.gpr();

        writeBarrier(m_jit, baseReg, scratchReg);

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

    case DFG::Jump: {
        BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(node.takenBytecodeOffset());
        if (taken != (m_block + 1))
            addBranch(m_jit.jump(), taken);
        noResult(m_compileIndex);
        break;
    }

    case Branch:
        emitBranch(node);
        break;

    case Return: {
        ASSERT(GPRInfo::callFrameRegister != GPRInfo::regT1);
        ASSERT(GPRInfo::regT1 != GPRInfo::returnValueGPR);
        ASSERT(GPRInfo::returnValueGPR != GPRInfo::callFrameRegister);

#if DFG_SUCCESS_STATS
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

    case ConvertThis: {
        SpeculateCellOperand thisValue(this, node.child1());

        speculationCheck(m_jit.branchPtr(JITCompiler::Equal, JITCompiler::Address(thisValue.gpr()), JITCompiler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));

        cellResult(thisValue.gpr(), m_compileIndex);
        break;
    }

    case GetById: {
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

    case PutById: {
        SpeculateCellOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        base.use();
        value.use();

        cachedPutById(baseGPR, valueGPR, scratchGPR, node.identifierNumber(), NotDirect);
        
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

        cachedPutById(baseGPR, valueGPR, scratchGPR, node.identifierNumber(), Direct);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetGlobalVar: {
        GPRTemporary result(this);

        JSVariableObject* globalObject = m_jit.codeBlock()->globalObject();
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

        m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.codeBlock()->globalObject()), globalObjectReg);

        writeBarrier(m_jit, globalObjectReg, scratchReg);

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
        speculationCheck(m_jit.branchTest8(MacroAssembler::Zero, MacroAssembler::Address(structure.gpr(), Structure::typeInfoFlagsOffset()), MacroAssembler::TrustedImm32(ImplementsDefaultHasInstance)));

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
        speculationCheck(m_jit.branch8(MacroAssembler::NotEqual, MacroAssembler::Address(scratchReg, Structure::typeInfoTypeOffset()), MacroAssembler::TrustedImm32(ObjectType)));

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
        jsValueResult(scratchReg, m_compileIndex);
        break;
    }

    case Phi:
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
    }

    if (node.hasResult() && node.mustGenerate())
        use(m_compileIndex);
}

void SpeculativeJIT::compile(BasicBlock& block)
{
    ASSERT(m_compileIndex == block.begin);
    m_blockHeads[m_block] = m_jit.label();
#if DFG_JIT_BREAK_ON_EVERY_BLOCK
    m_jit.breakpoint();
#endif

    for (; m_compileIndex < block.end; ++m_compileIndex) {
        Node& node = m_jit.graph()[m_compileIndex];
        if (!node.shouldGenerate())
            continue;

#if DFG_DEBUG_VERBOSE
        fprintf(stderr, "SpeculativeJIT generating Node @%d at JIT offset 0x%x\n", (int)m_compileIndex, m_jit.debugOffset());
#endif
#if DFG_JIT_BREAK_ON_EVERY_NODE
        m_jit.breakpoint();
#endif
        checkConsistency();
        compile(node);
        if (!m_compileOkay)
            return;
        checkConsistency();
    }
}

// If we are making type predictions about our arguments then
// we need to check that they are correct on function entry.
void SpeculativeJIT::checkArgumentTypes()
{
    ASSERT(!m_compileIndex);
    for (int i = 0; i < m_jit.codeBlock()->m_numParameters; ++i) {
        VirtualRegister virtualRegister = (VirtualRegister)(m_jit.codeBlock()->thisRegister() + i);
        PredictedType predictedType = m_jit.graph().getPrediction(virtualRegister);
        if (isInt32Prediction(predictedType))
            speculationCheck(m_jit.branchPtr(MacroAssembler::Below, JITCompiler::addressFor(virtualRegister), GPRInfo::tagTypeNumberRegister));
        else if (isArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        }
    }
}

// For any vars that we will be treating as numeric, write 0 to
// the var on entry. Throughout the block we will only read/write
// to the payload, by writing the tag now we prevent the GC from
// misinterpreting values as pointers.
void SpeculativeJIT::initializeVariableTypes()
{
    ASSERT(!m_compileIndex);
    for (int var = 0; var < m_jit.codeBlock()->m_numVars; ++var) {
        if (isInt32Prediction(m_jit.graph().getPrediction(var)))
            m_jit.storePtr(GPRInfo::tagTypeNumberRegister, JITCompiler::addressFor((VirtualRegister)var));
    }
}

bool SpeculativeJIT::compile()
{
    checkArgumentTypes();
    initializeVariableTypes();

    ASSERT(!m_compileIndex);
    for (m_block = 0; m_block < m_jit.graph().m_blocks.size(); ++m_block) {
        compile(*m_jit.graph().m_blocks[m_block]);
        if (!m_compileOkay)
            return false;
    }
    linkBranches();
    return true;
}

} } // namespace JSC::DFG

#endif
