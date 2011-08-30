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
#include "DFGNonSpeculativeJIT.h"

#include "DFGSpeculativeJIT.h"

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

const double twoToThe32 = (double)0x100000000ull;

EntryLocation::EntryLocation(MacroAssembler::Label entry, NonSpeculativeJIT* jit)
    : m_entry(entry)
    , m_nodeIndex(jit->m_compileIndex)
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

void NonSpeculativeJIT::valueToNumber(JSValueOperand& operand, GPRReg gpr)
{
    GPRReg jsValueGpr = operand.gpr();
    operand.use();

    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);
    JITCompiler::Jump nonNumeric = m_jit.branchTestPtr(MacroAssembler::Zero, jsValueGpr, GPRInfo::tagTypeNumberRegister);

    // First, if we get here we have a double encoded as a JSValue
    m_jit.move(jsValueGpr, gpr);
    JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

    // Next handle cells (& other JS immediates)
    nonNumeric.link(&m_jit);
    silentSpillAllRegisters(gpr);
    m_jit.move(jsValueGpr, GPRInfo::argumentGPR1);
    m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    appendCallWithExceptionCheck(dfgConvertJSValueToNumber);
    boxDouble(FPRInfo::returnValueFPR, gpr);
    silentFillAllRegisters(gpr);
    JITCompiler::Jump hasCalledToNumber = m_jit.jump();
    
    // Finally, handle integers.
    isInteger.link(&m_jit);
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, jsValueGpr, gpr);
    hasUnboxedDouble.link(&m_jit);
    hasCalledToNumber.link(&m_jit);
}

void NonSpeculativeJIT::valueToInt32(JSValueOperand& operand, GPRReg result)
{
    GPRReg jsValueGpr = operand.gpr();
    operand.use();

    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);

    // First handle non-integers
    silentSpillAllRegisters(result);
    m_jit.move(jsValueGpr, GPRInfo::argumentGPR1);
    m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    appendCallWithExceptionCheck(dfgConvertJSValueToInt32);
    m_jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, result);
    silentFillAllRegisters(result);
    JITCompiler::Jump hasCalledToInt32 = m_jit.jump();

    // Then handle integers.
    isInteger.link(&m_jit);
    m_jit.zeroExtend32ToPtr(jsValueGpr, result);
    hasCalledToInt32.link(&m_jit);
}

void NonSpeculativeJIT::numberToInt32(FPRReg fpr, GPRReg gpr)
{
    JITCompiler::Jump truncatedToInteger = m_jit.branchTruncateDoubleToInt32(fpr, gpr, JITCompiler::BranchIfTruncateSuccessful);

    silentSpillAllRegisters(gpr);

    m_jit.moveDouble(fpr, FPRInfo::argumentFPR0);
    appendCallWithExceptionCheck(toInt32);
    m_jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, gpr);

    silentFillAllRegisters(gpr);

    truncatedToInteger.link(&m_jit);
}

void NonSpeculativeJIT::knownConstantArithOp(NodeType op, NodeIndex regChild, NodeIndex immChild, bool commute)
{
    JSValueOperand regArg(this, regChild);
    GPRReg regArgGPR = regArg.gpr();
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    FPRTemporary tmp1(this);
    FPRTemporary tmp2(this);
    FPRReg tmp1FPR = tmp1.fpr();
    FPRReg tmp2FPR = tmp2.fpr();
    
    regArg.use();
    use(immChild);

    JITCompiler::Jump notInt;
    
    int32_t imm = valueOfInt32Constant(immChild);
        
    if (!isKnownInteger(regChild))
        notInt = m_jit.branchPtr(MacroAssembler::Below, regArgGPR, GPRInfo::tagTypeNumberRegister);
    
    JITCompiler::Jump overflow;
    
    switch (op) {
    case ValueAdd:
    case ArithAdd:
        overflow = m_jit.branchAdd32(MacroAssembler::Overflow, regArgGPR, Imm32(imm), resultGPR);
        break;
        
    case ArithSub:
        overflow = m_jit.branchSub32(MacroAssembler::Overflow, regArgGPR, Imm32(imm), resultGPR);
        break;
        
    default:
        ASSERT_NOT_REACHED();
    }
    
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
        
    JITCompiler::Jump done = m_jit.jump();
    
    overflow.link(&m_jit);
    
    switch (op) {
    case ValueAdd:
        // overflow and not-int are the same
        if (!isKnownInteger(regChild))
            notInt.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        if (commute) {
            m_jit.move(regArgGPR, GPRInfo::argumentGPR2);
            m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR1);
        } else {
            m_jit.move(regArgGPR, GPRInfo::argumentGPR1);
            m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR2);
        }
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationValueAdd);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        break;

    case ArithAdd:
    case ArithSub:
        // first deal with overflow case
        m_jit.convertInt32ToDouble(regArgGPR, tmp2FPR);
        
        // now deal with not-int case, if applicable
        if (!isKnownInteger(regChild)) {
            JITCompiler::Jump haveValue = m_jit.jump();
            
            notInt.link(&m_jit);
            
            m_jit.move(regArgGPR, resultGPR);
            m_jit.addPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
            m_jit.movePtrToDouble(resultGPR, tmp2FPR);
            
            haveValue.link(&m_jit);
        }
        
        m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfDoubleConstant(immChild)))), resultGPR);
        m_jit.movePtrToDouble(resultGPR, tmp1FPR);
        if (op == ArithAdd)
            m_jit.addDouble(tmp1FPR, tmp2FPR);
        else
            m_jit.subDouble(tmp1FPR, tmp2FPR);
        m_jit.moveDoubleToPtr(tmp2FPR, resultGPR);
        m_jit.subPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
        break;
        
    default:
        ASSERT_NOT_REACHED();
    }
    
    done.link(&m_jit);
        
    jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void NonSpeculativeJIT::basicArithOp(NodeType op, Node &node)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    
    FPRTemporary tmp1(this);
    FPRTemporary tmp2(this);
    FPRReg tmp1FPR = tmp1.fpr();
    FPRReg tmp2FPR = tmp2.fpr();
    
    GPRTemporary result(this);

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    JITCompiler::Jump child1NotInt;
    JITCompiler::Jump child2NotInt;
    JITCompiler::JumpList overflow;
    
    if (!isKnownInteger(node.child1()))
        child1NotInt = m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister);
    if (!isKnownInteger(node.child2()))
        child2NotInt = m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister);
    
    switch (op) {
    case ValueAdd:
    case ArithAdd: {
        overflow.append(m_jit.branchAdd32(MacroAssembler::Overflow, arg1GPR, arg2GPR, resultGPR));
        break;
    }
        
    case ArithSub: {
        overflow.append(m_jit.branchSub32(MacroAssembler::Overflow, arg1GPR, arg2GPR, resultGPR));
        break;
    }
        
    case ArithMul: {
        overflow.append(m_jit.branchMul32(MacroAssembler::Overflow, arg1GPR, arg2GPR, resultGPR));
        overflow.append(m_jit.branchTest32(MacroAssembler::Zero, resultGPR));
        break;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
    
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
        
    JITCompiler::Jump done = m_jit.jump();
    
    if (op == ValueAdd) {
        if (child1NotInt.isSet())
            child1NotInt.link(&m_jit);
        if (child2NotInt.isSet())
            child2NotInt.link(&m_jit);
        overflow.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        setupStubArguments(arg1GPR, arg2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationValueAdd);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
    } else {
        JITCompiler::JumpList haveFPRArguments;

        overflow.link(&m_jit);
        
        // both arguments are integers
        m_jit.convertInt32ToDouble(arg1GPR, tmp1FPR);
        m_jit.convertInt32ToDouble(arg2GPR, tmp2FPR);
        
        haveFPRArguments.append(m_jit.jump());
        
        JITCompiler::Jump child2NotInt2;
        
        if (!isKnownInteger(node.child1())) {
            child1NotInt.link(&m_jit);
            
            m_jit.move(arg1GPR, resultGPR);
            unboxDouble(resultGPR, tmp1FPR);
            
            // child1 is converted to a double; child2 may either be an int or
            // a boxed double
            
            if (!isKnownInteger(node.child2()))
                child2NotInt2 = m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister);
            
            // child 2 is definitely an integer
            m_jit.convertInt32ToDouble(arg2GPR, tmp2FPR);
            
            haveFPRArguments.append(m_jit.jump());
        }
        
        if (!isKnownInteger(node.child2())) {
            child2NotInt.link(&m_jit);
            // child1 is definitely an integer, and child 2 is definitely not
            
            m_jit.convertInt32ToDouble(arg1GPR, tmp1FPR);
            
            if (child2NotInt2.isSet())
                child2NotInt2.link(&m_jit);
            
            m_jit.move(arg2GPR, resultGPR);
            unboxDouble(resultGPR, tmp2FPR);
        }
        
        haveFPRArguments.link(&m_jit);
        
        switch (op) {
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
        
        boxDouble(tmp1FPR, resultGPR);
    }
    
    done.link(&m_jit);
        
    jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

void NonSpeculativeJIT::compile(SpeculationCheckIndexIterator& checkIterator, Node& node)
{
    // Check for speculation checks from the corresponding instruction in the
    // speculative path. Do not check for NodeIndex 0, since this is checked
    // in the outermost compile layer, at the head of the non-speculative path
    // (for index 0 we may need to check regardless of whether or not the node
    // will be generated, since argument type speculation checks will appear
    // as speculation checks at this index).
    if (m_compileIndex && checkIterator.hasCheckAtIndex(m_compileIndex))
        trackEntry(m_jit.label());

    NodeType op = node.op;

    switch (op) {
    case ConvertThis: {
        JSValueOperand thisValue(this, node.child1());
        GPRReg thisGPR = thisValue.gpr();
        flushRegisters();

        GPRResult result(this);
        callOperation(operationConvertThis, result.gpr(), thisGPR);
        cellResult(result.gpr(), m_compileIndex);
        break;
    }

    case JSConstant:
        initConstantInfo(m_compileIndex);
        break;

    case GetLocal: {
        GPRTemporary result(this);
        m_jit.loadPtr(JITCompiler::addressFor(node.local()), result.gpr());

        // Like jsValueResult, but don't useChildren - our children are phi nodes,
        // and don't represent values within this dataflow with virtual registers.
        VirtualRegister virtualRegister = node.virtualRegister();
        m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);
        m_generationInfo[virtualRegister].initJSValue(m_compileIndex, node.refCount(), result.gpr(), DataFormatJS);
        break;
    }

    case SetLocal: {
        JSValueOperand value(this, node.child1());
        m_jit.storePtr(value.gpr(), JITCompiler::addressFor(node.local()));
        noResult(m_compileIndex);
        break;
    }

    case BitAnd:
    case BitOr:
    case BitXor:
        if (isInt32Constant(node.child1())) {
            IntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op2);

            bitOp(op, valueOfInt32Constant(node.child1()), op2.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else if (isInt32Constant(node.child2())) {
            IntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);

            bitOp(op, valueOfInt32Constant(node.child2()), op1.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else {
            IntegerOperand op1(this, node.child1());
            IntegerOperand op2(this, node.child2());
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
            IntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);

            int shiftAmount = valueOfInt32Constant(node.child2()) & 0x1f;
            // Shifts by zero should have been optimized out of the graph!
            ASSERT(shiftAmount);
            shiftOp(op, op1.gpr(), shiftAmount, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else {
            // Do not allow shift amount to be used as the result, MacroAssembler does not permit this.
            IntegerOperand op1(this, node.child1());
            IntegerOperand op2(this, node.child2());
            GPRTemporary result(this, op1);

            GPRReg reg1 = op1.gpr();
            GPRReg reg2 = op2.gpr();
            shiftOp(op, reg1, reg2, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        }
        break;

    case UInt32ToNumber: {
        IntegerOperand op1(this, node.child1());
        FPRTemporary boxer(this);
        GPRTemporary result(this, op1);
        
        JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, op1.gpr(), TrustedImm32(0));
        
        m_jit.convertInt32ToDouble(op1.gpr(), boxer.fpr());
        m_jit.addDouble(JITCompiler::AbsoluteAddress(&twoToThe32), boxer.fpr());
        
        m_jit.moveDoubleToPtr(boxer.fpr(), result.gpr());
        m_jit.subPtr(GPRInfo::tagTypeNumberRegister, result.gpr());
        
        JITCompiler::Jump done = m_jit.jump();
        
        positive.link(&m_jit);
        
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, op1.gpr(), result.gpr());
        
        done.link(&m_jit);

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case ValueToInt32: {
        ASSERT(!isInt32Constant(node.child1()));

        if (isKnownInteger(node.child1())) {
            IntegerOperand op1(this, node.child1());
            GPRTemporary result(this, op1);
            m_jit.move(op1.gpr(), result.gpr());
            integerResult(result.gpr(), m_compileIndex);
            break;
        }

        GenerationInfo& childInfo = m_generationInfo[m_jit.graph()[node.child1()].virtualRegister()];
        if ((childInfo.registerFormat() | DataFormatJS) == DataFormatJSDouble) {
            DoubleOperand op1(this, node.child1());
            GPRTemporary result(this);
            FPRReg fpr = op1.fpr();
            GPRReg gpr = result.gpr();
            op1.use();
            numberToInt32(fpr, gpr);
            integerResult(gpr, m_compileIndex, UseChildrenCalledExplicitly);
            break;
        }

        JSValueOperand op1(this, node.child1());
        GPRTemporary result(this, op1);
        valueToInt32(op1, result.gpr());
        integerResult(result.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case ValueToNumber: {
        ASSERT(!isInt32Constant(node.child1()));
        ASSERT(!isDoubleConstant(node.child1()));

        if (isKnownNumeric(node.child1())) {
            JSValueOperand op1(this, node.child1());
            GPRTemporary result(this, op1);
            m_jit.move(op1.gpr(), result.gpr());
            jsValueResult(result.gpr(), m_compileIndex);
            break;
        }

        JSValueOperand op1(this, node.child1());
        GPRTemporary result(this);
        valueToNumber(op1, result.gpr());
        jsValueResult(result.gpr(), m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case ValueAdd:
    case ArithAdd: {
        if (isInt32Constant(node.child1())) {
            knownConstantArithOp(op, node.child2(), node.child1(), true);
            break;
        }
        
        if (isInt32Constant(node.child2())) {
            knownConstantArithOp(op, node.child1(), node.child2(), false);
            break;
        }
        
        basicArithOp(op, node);
        break;
    }
        
    case ArithSub: {
        if (isInt32Constant(node.child2())) {
            knownConstantArithOp(ArithSub, node.child1(), node.child2(), false);
            break;
        }
        
        basicArithOp(ArithSub, node);
        break;
    }

    case ArithMul: {
        basicArithOp(ArithMul, node);
        break;
    }

    case ArithDiv: {
        DoubleOperand op1(this, node.child1());
        DoubleOperand op2(this, node.child2());
        FPRTemporary result(this, op1);
        FPRReg op1FPR = op1.fpr();
        FPRReg op2FPR = op2.fpr();
        FPRReg resultFPR = result.fpr();
        
        m_jit.divDouble(op1FPR, op2FPR, resultFPR);
        
        doubleResult(resultFPR, m_compileIndex);
        break;
    }

    case ArithMod: {
        JSValueOperand op1(this, node.child1());
        JSValueOperand op2(this, node.child2());
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);

        FPRTemporary op1Double(this);
        FPRTemporary op2Double(this);
    
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
    
        FPRReg op1FPR = op1Double.fpr();
        FPRReg op2FPR = op2Double.fpr();
        
        op1.use();
        op2.use();
    
        GPRReg temp2 = InvalidGPRReg;
        GPRReg unboxGPR;
        if (op2GPR == X86Registers::eax || op2GPR == X86Registers::edx) {
            temp2 = allocate();
            m_jit.move(op2GPR, temp2);
            op2GPR = temp2;
            unboxGPR = temp2;
        } else if (op1GPR == X86Registers::eax)
            unboxGPR = X86Registers::edx;
        else
            unboxGPR = X86Registers::eax;
        ASSERT(unboxGPR != op1.gpr());
        ASSERT(unboxGPR != op2.gpr());

        JITCompiler::Jump firstOpNotInt;
        JITCompiler::Jump secondOpNotInt;
        JITCompiler::JumpList done;
        JITCompiler::Jump modByZero;
    
        if (!isKnownInteger(node.child1()))
            firstOpNotInt = m_jit.branchPtr(MacroAssembler::Below, op1GPR, GPRInfo::tagTypeNumberRegister);
        if (!isKnownInteger(node.child2()))
            secondOpNotInt = m_jit.branchPtr(MacroAssembler::Below, op2GPR, GPRInfo::tagTypeNumberRegister);
    
        modByZero = m_jit.branchTest32(MacroAssembler::Zero, op2GPR);
    
        m_jit.move(op1GPR, eax.gpr());
        m_jit.assembler().cdq();
        m_jit.assembler().idivl_r(op2GPR);
    
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, X86Registers::edx);
    
        done.append(m_jit.jump());
        
        JITCompiler::Jump gotDoubleArgs;
    
        modByZero.link(&m_jit);
        
        m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsNumber(std::numeric_limits<double>::quiet_NaN()))), X86Registers::edx);
        done.append(m_jit.jump());
    
        if (!isKnownInteger(node.child1())) {
            firstOpNotInt.link(&m_jit);
        
            JITCompiler::Jump secondOpNotInt2;
        
            if (!isKnownInteger(node.child2()))
                secondOpNotInt2 = m_jit.branchPtr(MacroAssembler::Below, op2GPR, GPRInfo::tagTypeNumberRegister);
            
            // first op is a double, second op is an int.
            m_jit.convertInt32ToDouble(op2GPR, op2FPR);

            if (!isKnownInteger(node.child2())) {
                JITCompiler::Jump gotSecondOp = m_jit.jump();
            
                secondOpNotInt2.link(&m_jit);
            
                // first op is a double, second op is a double.
                m_jit.move(op2GPR, unboxGPR);
                unboxDouble(unboxGPR, op2FPR);
            
                gotSecondOp.link(&m_jit);
            }
        
            m_jit.move(op1GPR, unboxGPR);
            unboxDouble(unboxGPR, op1FPR);
        
            gotDoubleArgs = m_jit.jump();
        }
    
        if (!isKnownInteger(node.child2())) {
            secondOpNotInt.link(&m_jit);
        
            // we know that the first op is an int, and the second is a double
            m_jit.convertInt32ToDouble(op1GPR, op1FPR);
            m_jit.move(op2GPR, unboxGPR);
            unboxDouble(unboxGPR, op2FPR);
        }
    
        if (!isKnownInteger(node.child1()))
            gotDoubleArgs.link(&m_jit);
    
        if (!isKnownInteger(node.child1()) || !isKnownInteger(node.child2())) {
            silentSpillAllRegisters(X86Registers::edx);
            setupTwoStubArgs<FPRInfo::argumentFPR0, FPRInfo::argumentFPR1>(op1FPR, op2FPR);
            m_jit.appendCall(fmod);
            boxDouble(FPRInfo::returnValueFPR, X86Registers::edx);
            silentFillAllRegisters(X86Registers::edx);
        }
        
        done.link(&m_jit);
    
        if (temp2 != InvalidGPRReg)
            unlock(temp2);
    
        jsValueResult(X86Registers::edx, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case LogicalNot: {
        JSValueOperand arg1(this, node.child1());
        GPRTemporary result(this);
        
        GPRReg arg1GPR = arg1.gpr();
        GPRReg resultGPR = result.gpr();
        
        arg1.use();

        m_jit.move(arg1GPR, resultGPR);
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), resultGPR);
        JITCompiler::Jump fastCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, TrustedImm32(static_cast<int32_t>(~1)));
        
        silentSpillAllRegisters(resultGPR);
        m_jit.move(arg1GPR, GPRInfo::argumentGPR1);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(dfgConvertJSValueToBoolean);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        fastCase.link(&m_jit);

        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueTrue)), resultGPR);
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case CompareLess:
        if (nonSpeculativeCompare(node, MacroAssembler::LessThan, operationCompareLess))
            return;
        break;
        
    case CompareLessEq:
        if (nonSpeculativeCompare(node, MacroAssembler::LessThanOrEqual, operationCompareLessEq))
            return;
        break;
        
    case CompareGreater:
        if (nonSpeculativeCompare(node, MacroAssembler::GreaterThan, operationCompareGreater))
            return;
        break;
        
    case CompareGreaterEq:
        if (nonSpeculativeCompare(node, MacroAssembler::GreaterThanOrEqual, operationCompareGreaterEq))
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
        if (nonSpeculativeCompare(node, MacroAssembler::Equal, operationCompareEq))
            return;
        break;

    case CompareStrictEq:
        if (nonSpeculativeStrictEq(node))
            return;
        break;

    case GetByVal: {
        if (node.child3() != NoNode)
            use(node.child3());
        
        JSValueOperand base(this, node.child1());
        JSValueOperand property(this, node.child2());

        GPRTemporary storage(this);
        GPRTemporary cleanIndex(this);

        GPRReg baseGPR = base.gpr();
        GPRReg propertyGPR = property.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg cleanIndexGPR = cleanIndex.gpr();
        
        base.use();
        property.use();

        JITCompiler::Jump baseNotCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        JITCompiler::Jump propertyNotInt = m_jit.branchPtr(MacroAssembler::Below, propertyGPR, GPRInfo::tagTypeNumberRegister);

        // Get the array storage. We haven't yet checked this is a JSArray, so this is only safe if
        // an access with offset JSArray::storageOffset() is valid for all JSCells!
        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);

        JITCompiler::Jump baseNotArray = m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr));

        m_jit.zeroExtend32ToPtr(propertyGPR, cleanIndexGPR);

        JITCompiler::Jump outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, cleanIndexGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));

        m_jit.loadPtr(MacroAssembler::BaseIndex(storageGPR, cleanIndexGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), storageGPR);

        JITCompiler::Jump loadFailed = m_jit.branchTestPtr(MacroAssembler::Zero, storageGPR);

        JITCompiler::Jump done = m_jit.jump();

        baseNotCell.link(&m_jit);
        propertyNotInt.link(&m_jit);
        baseNotArray.link(&m_jit);
        outOfBounds.link(&m_jit);
        loadFailed.link(&m_jit);

        silentSpillAllRegisters(storageGPR);
        setupStubArguments(baseGPR, propertyGPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationGetByVal);
        m_jit.move(GPRInfo::returnValueGPR, storageGPR);
        silentFillAllRegisters(storageGPR);

        done.link(&m_jit);

        jsValueResult(storageGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByVal:
    case PutByValAlias: {
        JSValueOperand base(this, node.child1());
        JSValueOperand property(this, node.child2());
        JSValueOperand value(this, node.child3());
        GPRTemporary storage(this);
        GPRTemporary cleanIndex(this);
        GPRReg baseGPR = base.gpr();
        GPRReg propertyGPR = property.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg cleanIndexGPR = cleanIndex.gpr();
        
        base.use();
        property.use();
        value.use();
        
        writeBarrier(m_jit, baseGPR, storageGPR);
        
        JITCompiler::Jump baseNotCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        JITCompiler::Jump propertyNotInt = m_jit.branchPtr(MacroAssembler::Below, propertyGPR, GPRInfo::tagTypeNumberRegister);

        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), storageGPR);

        JITCompiler::Jump baseNotArray = m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr));

        m_jit.zeroExtend32ToPtr(propertyGPR, cleanIndexGPR);

        JITCompiler::Jump outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, cleanIndexGPR, MacroAssembler::Address(baseGPR, JSArray::vectorLengthOffset()));
        
        JITCompiler::Jump notHoleValue = m_jit.branchTestPtr(MacroAssembler::NonZero, MacroAssembler::BaseIndex(storageGPR, cleanIndexGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        
        JITCompiler::Jump lengthDoesNotNeedUpdate = m_jit.branch32(MacroAssembler::Below, cleanIndexGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        
        m_jit.add32(TrustedImm32(1), cleanIndexGPR);
        m_jit.store32(cleanIndexGPR, MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)));
        m_jit.zeroExtend32ToPtr(propertyGPR, cleanIndexGPR);
        
        lengthDoesNotNeedUpdate.link(&m_jit);
        notHoleValue.link(&m_jit);
        
        m_jit.storePtr(valueGPR, MacroAssembler::BaseIndex(storageGPR, cleanIndexGPR, MacroAssembler::ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
        
        JITCompiler::Jump done = m_jit.jump();
        
        baseNotCell.link(&m_jit);
        propertyNotInt.link(&m_jit);
        baseNotArray.link(&m_jit);
        outOfBounds.link(&m_jit);
        
        silentSpillAllRegisters(InvalidGPRReg);
        setupStubArguments(baseGPR, propertyGPR, valueGPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        JITCompiler::Call functionCall = appendCallWithExceptionCheck(m_jit.codeBlock()->isStrictMode() ? operationPutByValStrict : operationPutByValNonStrict);
        silentFillAllRegisters(InvalidGPRReg);
        
        done.link(&m_jit);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetById: {
        JSValueOperand base(this, node.child1());
        GPRReg baseGPR = base.gpr();
        GPRTemporary result(this, base);
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR;
        
        if (resultGPR == baseGPR)
            scratchGPR = tryAllocate();
        else
            scratchGPR = resultGPR;
        
        base.use();

        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedGetById(baseGPR, resultGPR, scratchGPR, node.identifierNumber(), notCell);

        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case GetMethod: {
        JSValueOperand base(this, node.child1());
        GPRReg baseGPR = base.gpr();
        GPRTemporary result(this, base);
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR;
        if (resultGPR == baseGPR)
            scratchGPR = tryAllocate();
        else
            scratchGPR = resultGPR;
        
        base.use();

        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedGetMethod(baseGPR, resultGPR, scratchGPR, node.identifierNumber(), notCell);

        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutById: {
        JSValueOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        GPRReg valueGPR = value.gpr();
        GPRReg baseGPR = base.gpr();
        GPRReg scratchGPR = scratch.gpr();
        
        base.use();
        value.use();
        
        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedPutById(baseGPR, valueGPR, scratchGPR, node.identifierNumber(), NotDirect, notCell);

        noResult(m_compileIndex, UseChildrenCalledExplicitly);
        break;
    }

    case PutByIdDirect: {
        JSValueOperand base(this, node.child1());
        JSValueOperand value(this, node.child2());
        GPRTemporary scratch(this);
        GPRReg valueGPR = value.gpr();
        GPRReg baseGPR = base.gpr();
        
        base.use();
        value.use();
        
        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedPutById(baseGPR, valueGPR, scratch.gpr(), node.identifierNumber(), Direct, notCell);

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
        static SamplingCounter counter("NonSpeculativeJIT");
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

    case CheckHasInstance: {
        JSValueOperand base(this, node.child1());
        GPRTemporary structure(this);

        GPRReg baseReg = base.gpr();
        GPRReg structureReg = structure.gpr();

        // Check that base is a cell.
        MacroAssembler::Jump baseNotCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseReg, GPRInfo::tagMaskRegister);

        // Check that base 'ImplementsHasInstance'.
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSCell::structureOffset()), structureReg);
        MacroAssembler::Jump implementsHasInstance = m_jit.branchTest8(MacroAssembler::NonZero, MacroAssembler::Address(structureReg, Structure::typeInfoFlagsOffset()), MacroAssembler::TrustedImm32(ImplementsHasInstance));

        // At this point we always throw, so no need to preserve registers.
        baseNotCell.link(&m_jit);
        m_jit.move(baseReg, GPRInfo::argumentGPR1);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        // At some point we could optimize this to plant a direct jump, rather then checking
        // for an exception (operationThrowHasInstanceError always throws). Probably not worth
        // adding the extra interface to do this now, but we may also want this for op_throw.
        appendCallWithExceptionCheck(operationThrowHasInstanceError);

        implementsHasInstance.link(&m_jit);
        noResult(m_compileIndex);
        break;
    }

    case InstanceOf: {
        JSValueOperand value(this, node.child1());
        JSValueOperand base(this, node.child2());
        JSValueOperand prototype(this, node.child3());
        GPRTemporary scratch(this, base);

        GPRReg valueReg = value.gpr();
        GPRReg baseReg = base.gpr();
        GPRReg prototypeReg = prototype.gpr();
        GPRReg scratchReg = scratch.gpr();
        
        value.use();
        base.use();
        prototype.use();

        // Check that operands are cells (base is checked by CheckHasInstance, so we can just assert).
        MacroAssembler::Jump valueNotCell = m_jit.branchTestPtr(MacroAssembler::NonZero, valueReg, GPRInfo::tagMaskRegister);
        m_jit.jitAssertIsCell(baseReg);
        MacroAssembler::Jump prototypeNotCell = m_jit.branchTestPtr(MacroAssembler::NonZero, prototypeReg, GPRInfo::tagMaskRegister);

        // Check that baseVal 'ImplementsDefaultHasInstance'.
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSCell::structureOffset()), scratchReg);
        MacroAssembler::Jump notDefaultHasInstance = m_jit.branchTest8(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, Structure::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance));

        // Check that prototype is an object
        m_jit.loadPtr(MacroAssembler::Address(prototypeReg, JSCell::structureOffset()), scratchReg);
        MacroAssembler::Jump protoNotObject = m_jit.branch8(MacroAssembler::NotEqual, MacroAssembler::Address(scratchReg, Structure::typeInfoTypeOffset()), MacroAssembler::TrustedImm32(ObjectType));

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
        MacroAssembler::Jump wasNotInstance = m_jit.jump();

        // Link to here if any checks fail that require us to try calling out to an operation to help,
        // e.g. for an API overridden HasInstance.
        valueNotCell.link(&m_jit);
        prototypeNotCell.link(&m_jit);
        notDefaultHasInstance.link(&m_jit);
        protoNotObject.link(&m_jit);

        silentSpillAllRegisters(scratchReg);
        setupStubArguments(valueReg, baseReg, prototypeReg);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationInstanceOf);
        m_jit.move(GPRInfo::returnValueGPR, scratchReg);
        silentFillAllRegisters(scratchReg);

        MacroAssembler::Jump wasNotDefaultHasInstance = m_jit.jump();

        isInstance.link(&m_jit);
        m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(true))), scratchReg);

        wasNotInstance.link(&m_jit);
        wasNotDefaultHasInstance.link(&m_jit);
        jsValueResult(scratchReg, m_compileIndex, UseChildrenCalledExplicitly);
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

void NonSpeculativeJIT::compile(SpeculationCheckIndexIterator& checkIterator, BasicBlock& block)
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
        fprintf(stderr, "NonSpeculativeJIT generating Node @%d at code offset 0x%x\n", (int)m_compileIndex, m_jit.debugOffset());
#endif
#if DFG_JIT_BREAK_ON_EVERY_NODE
        m_jit.breakpoint();
#endif

        checkConsistency();
        compile(checkIterator, node);
        checkConsistency();
    }
}

void NonSpeculativeJIT::compile(SpeculationCheckIndexIterator& checkIterator)
{
    // Check for speculation checks added at function entry (checking argument types).
    if (checkIterator.hasCheckAtIndex(m_compileIndex))
        trackEntry(m_jit.label());

    ASSERT(!m_compileIndex);
    for (m_block = 0; m_block < m_jit.graph().m_blocks.size(); ++m_block)
        compile(checkIterator, *m_jit.graph().m_blocks[m_block]);
    linkBranches();
}

} } // namespace JSC::DFG

#endif
