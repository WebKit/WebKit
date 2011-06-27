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
        } else
            m_gprInfo[iter.index()].nodeIndex = NoNode;
    }
    for (fpr_iterator iter = jit->m_fprs.begin(); iter != jit->m_fprs.end(); ++iter) {
        if (iter.name() != InvalidVirtualRegister) {
            GenerationInfo& info =  jit->m_generationInfo[iter.name()];
            ASSERT(info.registerFormat() == DataFormatDouble);
            m_fprInfo[iter.index()] = info.nodeIndex();
        } else
            m_fprInfo[iter.index()] = NoNode;
    }
}

void NonSpeculativeJIT::valueToNumber(JSValueOperand& operand, GPRReg gpr)
{
    GPRReg jsValueGpr = operand.gpr();

    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);
    JITCompiler::Jump nonNumeric = m_jit.branchTestPtr(MacroAssembler::Zero, jsValueGpr, GPRInfo::tagTypeNumberRegister);

    // First, if we get here we have a double encoded as a JSValue
    m_jit.move(jsValueGpr, gpr);
    JITCompiler::Jump hasUnboxedDouble = m_jit.jump();

    // Next handle cells (& other JS immediates)
    nonNumeric.link(&m_jit);
    silentSpillAllRegisters(gpr, jsValueGpr);
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

    JITCompiler::Jump isInteger = m_jit.branchPtr(MacroAssembler::AboveOrEqual, jsValueGpr, GPRInfo::tagTypeNumberRegister);

    // First handle non-integers
    silentSpillAllRegisters(result, jsValueGpr);
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

bool NonSpeculativeJIT::isKnownInteger(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex))
        return true;

    GenerationInfo& info = m_generationInfo[m_jit.graph()[nodeIndex].virtualRegister()];

    DataFormat registerFormat = info.registerFormat();
    if (registerFormat != DataFormatNone)
        return (registerFormat | DataFormatJS) == DataFormatJSInteger;

    DataFormat spillFormat = info.spillFormat();
    if (spillFormat != DataFormatNone)
        return (spillFormat | DataFormatJS) == DataFormatJSInteger;

    ASSERT(isConstant(nodeIndex));
    return false;
}

bool NonSpeculativeJIT::isKnownNumeric(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex) || isDoubleConstant(nodeIndex))
        return true;

    GenerationInfo& info = m_generationInfo[m_jit.graph()[nodeIndex].virtualRegister()];

    DataFormat registerFormat = info.registerFormat();
    if (registerFormat != DataFormatNone)
        return (registerFormat | DataFormatJS) == DataFormatJSInteger
            || (registerFormat | DataFormatJS) == DataFormatJSDouble;

    DataFormat spillFormat = info.spillFormat();
    if (spillFormat != DataFormatNone)
        return (spillFormat | DataFormatJS) == DataFormatJSInteger
            || (spillFormat | DataFormatJS) == DataFormatJSDouble;

    ASSERT(isConstant(nodeIndex));
    return false;
}

void NonSpeculativeJIT::knownConstantArithOp(NodeType op, NodeIndex regChild, NodeIndex immChild, bool commute)
{
    JSValueOperand regArg(this, regChild);
    GPRReg regArgGPR = regArg.gpr();
    GPRTemporary result(this, regArg);
    GPRReg resultGPR = result.gpr();

    JITCompiler::Jump notInt;
    
    int32_t imm = valueOfInt32Constant(immChild);
        
    if (!isKnownInteger(regChild))
        notInt = m_jit.branchPtr(MacroAssembler::Below, regArgGPR, GPRInfo::tagTypeNumberRegister);
        
    m_jit.zeroExtend32ToPtr(regArgGPR, resultGPR); 
        
    JITCompiler::Jump overflow;
    
    switch (op) {
    case ValueAdd:
    case ArithAdd: {
        overflow = m_jit.branchAdd32(MacroAssembler::Overflow, Imm32(imm), resultGPR);
        break;
    }
        
    case ArithSub: {
        overflow = m_jit.branchSub32(MacroAssembler::Overflow, Imm32(imm), resultGPR);
        break;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
    
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
        
    JITCompiler::Jump done = m_jit.jump();
    
    overflow.link(&m_jit);
    
    if (regArgGPR == resultGPR) {
        switch (op) {
        case ValueAdd:
        case ArithAdd: {
            m_jit.sub32(Imm32(imm), regArgGPR);
            break;
        }
            
        case ArithSub: {
            m_jit.add32(Imm32(imm), regArgGPR);
            break;
        }
            
        default:
            ASSERT_NOT_REACHED();
        }
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, regArgGPR);
    }
    
    if (!isKnownInteger(regChild))
        notInt.link(&m_jit);
    
    silentSpillAllRegisters(resultGPR, regArgGPR);
    switch (op) {
    case ValueAdd:
        if (commute) {
            m_jit.move(regArgGPR, GPRInfo::argumentGPR2);
            m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR1);
        } else {
            m_jit.move(regArgGPR, GPRInfo::argumentGPR1);
            m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR2);
        }
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationValueAdd);
        break;
        
    case ArithAdd:
        if (commute) {
            m_jit.move(regArgGPR, GPRInfo::argumentGPR1);
            m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR0);
        } else {
            m_jit.move(regArgGPR, GPRInfo::argumentGPR0);
            m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR1);
        }
        m_jit.appendCall(operationArithAdd);
        break;
        
    case ArithSub:
        ASSERT(!commute);
        m_jit.move(regArgGPR, GPRInfo::argumentGPR0);
        m_jit.move(MacroAssembler::ImmPtr(static_cast<const void*>(JSValue::encode(jsNumber(imm)))), GPRInfo::argumentGPR1);
        m_jit.appendCall(operationArithSub);
        break;
        
    default:
        ASSERT_NOT_REACHED();
    }
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    silentFillAllRegisters(resultGPR);
    
    done.link(&m_jit);
        
    jsValueResult(resultGPR, m_compileIndex);
}

void NonSpeculativeJIT::basicArithOp(NodeType op, Node &node)
{
    JSValueOperand arg1(this, node.child1);
    JSValueOperand arg2(this, node.child2);
    
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary temp(this, arg2);
    GPRTemporary result(this);

    GPRReg tempGPR = temp.gpr();
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::JumpList slowPath;
    JITCompiler::JumpList overflowSlowPath;
    
    if (!isKnownInteger(node.child1))
        slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister));
    if (!isKnownInteger(node.child2))
        slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister));
    
    m_jit.zeroExtend32ToPtr(arg1GPR, resultGPR);
    m_jit.zeroExtend32ToPtr(arg2GPR, tempGPR);
    
    switch (op) {
    case ValueAdd:
    case ArithAdd: {
        overflowSlowPath.append(m_jit.branchAdd32(MacroAssembler::Overflow, tempGPR, resultGPR));
        break;
    }
        
    case ArithSub: {
        overflowSlowPath.append(m_jit.branchSub32(MacroAssembler::Overflow, tempGPR, resultGPR));
        break;
    }
        
    case ArithMul: {
        overflowSlowPath.append(m_jit.branchMul32(MacroAssembler::Overflow, tempGPR, resultGPR));
        overflowSlowPath.append(m_jit.branchTest32(MacroAssembler::Zero, resultGPR));
        break;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
    
    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, resultGPR);
        
    JITCompiler::Jump done = m_jit.jump();
    
    overflowSlowPath.link(&m_jit);
    
    if (arg2GPR == tempGPR)
        m_jit.orPtr(GPRInfo::tagTypeNumberRegister, arg2GPR);
    
    slowPath.link(&m_jit);
    
    silentSpillAllRegisters(resultGPR, arg1GPR, arg2GPR);
    if (op == ValueAdd) {
        setupStubArguments(arg1GPR, arg2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationValueAdd);
    } else {
        setupTwoStubArgs<GPRInfo::argumentGPR0, GPRInfo::argumentGPR1>(arg1GPR, arg2GPR);
        switch (op) {
        case ArithAdd:
            m_jit.appendCall(operationArithAdd);
            break;
            
        case ArithSub:
            m_jit.appendCall(operationArithSub);
            break;
            
        case ArithMul:
            m_jit.appendCall(operationArithMul);
            break;
            
        default:
            ASSERT_NOT_REACHED();
        }
    }
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    silentFillAllRegisters(resultGPR);
    
    done.link(&m_jit);
        
    jsValueResult(resultGPR, m_compileIndex);
}

void NonSpeculativeJIT::compare(Node& node, MacroAssembler::RelationalCondition cond, const Z_DFGOperation_EJJ& helperFunction)
{
    // FIXME: should do some peephole to fuse compare/branch
        
    JSValueOperand arg1(this, node.child1);
    JSValueOperand arg2(this, node.child2);
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRTemporary temp(this);
    GPRReg resultGPR = result.gpr();
    GPRReg tempGPR = temp.gpr();
    
    JITCompiler::JumpList slowPath;
    
    if (!isKnownInteger(node.child1))
        slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister));
    if (!isKnownInteger(node.child2))
        slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister));
    
    m_jit.zeroExtend32ToPtr(arg1GPR, tempGPR);
    m_jit.zeroExtend32ToPtr(arg2GPR, resultGPR);
    
    m_jit.compare32(cond, tempGPR, resultGPR, resultGPR);
    
    JITCompiler::Jump haveResult = m_jit.jump();
    
    slowPath.link(&m_jit);
    
    silentSpillAllRegisters(resultGPR, arg1GPR, arg2GPR);
    setupStubArguments(arg1GPR, arg2GPR);
    m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    appendCallWithExceptionCheck(helperFunction);
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    silentFillAllRegisters(resultGPR);
        
    m_jit.andPtr(TrustedImm32(static_cast<int32_t>(1)), resultGPR);
    
    haveResult.link(&m_jit);
    
    m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
    jsValueResult(resultGPR, m_compileIndex);
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
        JSValueOperand thisValue(this, node.child1);
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
        JSValueOperand value(this, node.child1);
        m_jit.storePtr(value.gpr(), JITCompiler::addressFor(node.local()));
        noResult(m_compileIndex);
        break;
    }

    case BitAnd:
    case BitOr:
    case BitXor:
        if (isInt32Constant(node.child1)) {
            IntegerOperand op2(this, node.child2);
            GPRTemporary result(this, op2);

            bitOp(op, valueOfInt32Constant(node.child1), op2.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else if (isInt32Constant(node.child2)) {
            IntegerOperand op1(this, node.child1);
            GPRTemporary result(this, op1);

            bitOp(op, valueOfInt32Constant(node.child2), op1.gpr(), result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else {
            IntegerOperand op1(this, node.child1);
            IntegerOperand op2(this, node.child2);
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
        if (isInt32Constant(node.child2)) {
            IntegerOperand op1(this, node.child1);
            GPRTemporary result(this, op1);

            int shiftAmount = valueOfInt32Constant(node.child2) & 0x1f;
            // Shifts by zero should have been optimized out of the graph!
            ASSERT(shiftAmount);
            shiftOp(op, op1.gpr(), shiftAmount, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        } else {
            // Do not allow shift amount to be used as the result, MacroAssembler does not permit this.
            IntegerOperand op1(this, node.child1);
            IntegerOperand op2(this, node.child2);
            GPRTemporary result(this, op1);

            GPRReg reg1 = op1.gpr();
            GPRReg reg2 = op2.gpr();
            shiftOp(op, reg1, reg2, result.gpr());

            integerResult(result.gpr(), m_compileIndex);
        }
        break;

    case UInt32ToNumber: {
        IntegerOperand op1(this, node.child1);
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
        ASSERT(!isInt32Constant(node.child1));

        if (isKnownInteger(node.child1)) {
            IntegerOperand op1(this, node.child1);
            GPRTemporary result(this, op1);
            m_jit.move(op1.gpr(), result.gpr());
            integerResult(result.gpr(), m_compileIndex);
            break;
        }

        GenerationInfo& childInfo = m_generationInfo[m_jit.graph()[node.child1].virtualRegister()];
        if ((childInfo.registerFormat() | DataFormatJS) == DataFormatJSDouble) {
            DoubleOperand op1(this, node.child1);
            GPRTemporary result(this);
            numberToInt32(op1.fpr(), result.gpr());
            integerResult(result.gpr(), m_compileIndex);
            break;
        }

        JSValueOperand op1(this, node.child1);
        GPRTemporary result(this, op1);
        valueToInt32(op1, result.gpr());
        integerResult(result.gpr(), m_compileIndex);
        break;
    }

    case ValueToNumber: {
        ASSERT(!isInt32Constant(node.child1));
        ASSERT(!isDoubleConstant(node.child1));

        if (isKnownNumeric(node.child1)) {
            JSValueOperand op1(this, node.child1);
            GPRTemporary result(this, op1);
            m_jit.move(op1.gpr(), result.gpr());
            jsValueResult(result.gpr(), m_compileIndex);
            break;
        }

        JSValueOperand op1(this, node.child1);
        GPRTemporary result(this);
        valueToNumber(op1, result.gpr());
        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case ValueAdd:
    case ArithAdd: {
        if (isInt32Constant(node.child1)) {
            knownConstantArithOp(op, node.child2, node.child1, true);
            break;
        }
        
        if (isInt32Constant(node.child2)) {
            knownConstantArithOp(op, node.child1, node.child2, false);
            break;
        }
        
        basicArithOp(op, node);
        break;
    }
        
    case ArithSub: {
        if (isInt32Constant(node.child2)) {
            knownConstantArithOp(ArithSub, node.child1, node.child2, false);
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
        JSValueOperand arg1(this, node.child1);
        JSValueOperand arg2(this, node.child2);
        GPRReg arg1GPR = arg1.gpr();
        GPRReg arg2GPR = arg2.gpr();
        
        flushRegisters();

        GPRResult result(this);

        ASSERT(isFlushed());
        setupTwoStubArgs<GPRInfo::argumentGPR0, GPRInfo::argumentGPR1>(arg1GPR, arg2GPR);
        m_jit.appendCall(operationArithDiv);
        m_jit.move(GPRInfo::returnValueGPR, result.gpr());

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case ArithMod: {
        JSValueOperand arg1(this, node.child1);
        JSValueOperand arg2(this, node.child2);
        GPRReg arg1GPR = arg1.gpr();
        GPRReg arg2GPR = arg2.gpr();
        flushRegisters();

        GPRResult result(this);

        ASSERT(isFlushed());
        setupTwoStubArgs<GPRInfo::argumentGPR0, GPRInfo::argumentGPR1>(arg1GPR, arg2GPR);
        m_jit.appendCall(operationArithMod);
        m_jit.move(GPRInfo::returnValueGPR, result.gpr());

        jsValueResult(result.gpr(), m_compileIndex);
        break;
    }

    case LogicalNot: {
        JSValueOperand arg1(this, node.child1);
        GPRTemporary result(this);

        GPRReg arg1GPR = arg1.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(arg1GPR, resultGPR);
        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), resultGPR);
        JITCompiler::Jump fastCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, TrustedImm32(static_cast<int32_t>(~1)));
        
        silentSpillAllRegisters(resultGPR, arg1GPR);
        m_jit.move(arg1GPR, GPRInfo::argumentGPR1);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(dfgConvertJSValueToBoolean);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        fastCase.link(&m_jit);

        m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueTrue)), resultGPR);
        jsValueResult(resultGPR, m_compileIndex);
        break;
    }

    case CompareLess:
        compare(node, MacroAssembler::LessThan, operationCompareLess);
        break;
        
    case CompareLessEq:
        compare(node, MacroAssembler::LessThanOrEqual, operationCompareLessEq);
        break;
        
    case CompareEq:
        compare(node, MacroAssembler::Equal, operationCompareEq);
        break;

    case CompareStrictEq:
        compare(node, MacroAssembler::Equal, operationCompareStrictEq);
        break;

    case GetByVal: {
        JSValueOperand base(this, node.child1);
        JSValueOperand property(this, node.child2);

        GPRTemporary storage(this);
        GPRTemporary cleanIndex(this);

        GPRReg baseGPR = base.gpr();
        GPRReg propertyGPR = property.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg cleanIndexGPR = cleanIndex.gpr();

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

        silentSpillAllRegisters(storageGPR, baseGPR, propertyGPR);
        setupStubArguments(baseGPR, propertyGPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationGetByVal);
        m_jit.move(GPRInfo::returnValueGPR, storageGPR);
        silentFillAllRegisters(storageGPR);

        done.link(&m_jit);

        jsValueResult(storageGPR, m_compileIndex);
        break;
    }

    case PutByVal:
    case PutByValAlias: {
        JSValueOperand arg1(this, node.child1);
        JSValueOperand arg2(this, node.child2);
        JSValueOperand arg3(this, node.child3);
        GPRReg arg1GPR = arg1.gpr();
        GPRReg arg2GPR = arg2.gpr();
        GPRReg arg3GPR = arg3.gpr();
        flushRegisters();

        GPRResult result(this);
        callOperation(m_jit.codeBlock()->isStrictMode() ? operationPutByValStrict : operationPutByValNonStrict, arg1GPR, arg2GPR, arg3GPR);

        noResult(m_compileIndex);
        break;
    }

    case GetById: {
        JSValueOperand base(this, node.child1);
        GPRReg baseGPR = base.gpr();
        GPRTemporary result(this, base);
        GPRReg resultGPR = result.gpr();

        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedGetById(baseGPR, resultGPR, node.identifierNumber(), notCell);

        jsValueResult(resultGPR, m_compileIndex);
        break;
    }

    case PutById: {
        JSValueOperand base(this, node.child1);
        JSValueOperand value(this, node.child2);
        GPRTemporary scratch(this, base);
        GPRReg valueGPR = value.gpr();
        GPRReg baseGPR = base.gpr();
        
        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedPutById(baseGPR, valueGPR, scratch.gpr(), node.identifierNumber(), NotDirect, notCell);

        noResult(m_compileIndex);
        break;
    }

    case PutByIdDirect: {
        JSValueOperand base(this, node.child1);
        JSValueOperand value(this, node.child2);
        GPRTemporary scratch(this, base);
        GPRReg valueGPR = value.gpr();
        GPRReg baseGPR = base.gpr();
        
        JITCompiler::Jump notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, baseGPR, GPRInfo::tagMaskRegister);

        cachedPutById(baseGPR, valueGPR, scratch.gpr(), node.identifierNumber(), Direct, notCell);

        noResult(m_compileIndex);
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
        JSValueOperand value(this, node.child1);
        GPRTemporary temp(this);

        JSVariableObject* globalObject = m_jit.codeBlock()->globalObject();
        m_jit.loadPtr(globalObject->addressOfRegisters(), temp.gpr());
        m_jit.storePtr(value.gpr(), JITCompiler::addressForGlobalVar(temp.gpr(), node.varNumber()));

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

    case Branch: {
        JSValueOperand value(this, node.child1);
        GPRReg valueGPR = value.gpr();
        flushRegisters();

        GPRResult result(this);
        callOperation(dfgConvertJSValueToBoolean, result.gpr(), valueGPR);

        BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(node.takenBytecodeOffset());
        BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(node.notTakenBytecodeOffset());

        addBranch(m_jit.branchTest8(MacroAssembler::NonZero, result.gpr()), taken);
        if (notTaken != (m_block + 1))
            addBranch(m_jit.jump(), notTaken);

        noResult(m_compileIndex);
        break;
    }

    case Return: {
        ASSERT(GPRInfo::callFrameRegister != GPRInfo::regT1);
        ASSERT(GPRInfo::regT1 != GPRInfo::returnValueGPR);
        ASSERT(GPRInfo::returnValueGPR != GPRInfo::callFrameRegister);

#if DFG_SUCCESS_STATS
        static SamplingCounter counter("NonSpeculativeJIT");
        m_jit.emitCount(counter);
#endif

        // Return the result in returnValueGPR.
        JSValueOperand op1(this, node.child1);
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

    case Phi:
        ASSERT_NOT_REACHED();
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
