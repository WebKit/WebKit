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
#include "DFGJITCodeGenerator.h"

#if ENABLE(DFG_JIT)

#include "DFGNonSpeculativeJIT.h"
#include "DFGSpeculativeJIT.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

GPRReg JITCodeGenerator::fillInteger(NodeIndex nodeIndex, DataFormat& returnFormat)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {
        GPRReg gpr = allocate();

        if (node.isConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            if (isInt32Constant(nodeIndex)) {
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                info.fillInteger(gpr);
                returnFormat = DataFormatInteger;
                return gpr;
            }
            if (isDoubleConstant(nodeIndex)) {
                JSValue jsValue = jsNumber(valueOfDoubleConstant(nodeIndex));
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            } else {
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            }
        } else {
            ASSERT(info.spillFormat() == DataFormatJS || info.spillFormat() == DataFormatJSInteger);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
        }

        // Since we statically know that we're filling an integer, and values
        // in the RegisterFile are boxed, this must be DataFormatJSInteger.
        // We will check this with a jitAssert below.
        info.fillJSValue(gpr, DataFormatJSInteger);
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
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
}

FPRReg JITCodeGenerator::fillDouble(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    if (info.registerFormat() == DataFormatNone) {
        GPRReg gpr = allocate();

        if (node.isConstant()) {
            if (isInt32Constant(nodeIndex)) {
                // FIXME: should not be reachable?
                m_jit.move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillInteger(gpr);
                unlock(gpr);
            } else if (isDoubleConstant(nodeIndex)) {
                FPRReg fpr = fprAllocate();
                m_jit.move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfDoubleConstant(nodeIndex)))), gpr);
                m_jit.movePtrToDouble(gpr, fpr);
                unlock(gpr);

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(fpr);
                return fpr;
            } else {
                // FIXME: should not be reachable?
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
                m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
                info.fillJSValue(gpr, DataFormatJS);
                unlock(gpr);
            }
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(gpr, m_isSpeculative ? spillFormat : DataFormatJS);
            unlock(gpr);
        }
    }

    switch (info.registerFormat()) {
    case DataFormatNone:
        // Should have filled, above.
    case DataFormatCell:
    case DataFormatJSCell:
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

GPRReg JITCodeGenerator::fillJSValue(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (node.isConstant()) {
            if (isInt32Constant(nodeIndex)) {
                info.fillJSValue(gpr, DataFormatJSInteger);
                JSValue jsValue = jsNumber(valueOfInt32Constant(nodeIndex));
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            } else if (isDoubleConstant(nodeIndex)) {
                info.fillJSValue(gpr, DataFormatJSDouble);
                JSValue jsValue(JSValue::EncodeAsDouble, valueOfDoubleConstant(nodeIndex));
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
            } else {
                ASSERT(isJSConstant(nodeIndex));
                JSValue jsValue = valueOfJSConstant(nodeIndex);
                m_jit.move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
                info.fillJSValue(gpr, DataFormatJS);
            }

            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat & DataFormatJS);
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillJSValue(gpr, m_isSpeculative ? spillFormat : DataFormatJS);
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
        info.fillJSValue(gpr, DataFormatJSInteger);
        return gpr;
    }

    case DataFormatDouble: {
        FPRReg fpr = info.fpr();
        GPRReg gpr = boxDouble(fpr);

        // Update all info
        info.fillJSValue(gpr, DataFormatJSDouble);
        m_fprs.release(fpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderJS);

        return gpr;
    }

    case DataFormatCell:
        // No retag required on JSVALUE64!
    case DataFormatJS:
    case DataFormatJSInteger:
    case DataFormatJSDouble:
    case DataFormatJSCell: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }
    }

    ASSERT_NOT_REACHED();
    return InvalidGPRReg;
}

void JITCodeGenerator::useChildren(Node& node)
{
    if (node.op & NodeHasVarArgs) {
        for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
            use(m_jit.graph().m_varArgChildren[childIdx]);
    } else {
        NodeIndex child1 = node.child1();
        if (child1 == NoNode) {
            ASSERT(node.child2() == NoNode && node.child3() == NoNode);
            return;
        }
        use(child1);
        
        NodeIndex child2 = node.child2();
        if (child2 == NoNode) {
            ASSERT(node.child3() == NoNode);
            return;
        }
        use(child2);
        
        NodeIndex child3 = node.child3();
        if (child3 == NoNode)
            return;
        use(child3);
    }
}

bool JITCodeGenerator::isKnownInteger(NodeIndex nodeIndex)
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

bool JITCodeGenerator::isKnownNumeric(NodeIndex nodeIndex)
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

bool JITCodeGenerator::isKnownCell(NodeIndex nodeIndex)
{
    GenerationInfo& info = m_generationInfo[m_jit.graph()[nodeIndex].virtualRegister()];
    
    DataFormat registerFormat = info.registerFormat();
    if (registerFormat != DataFormatNone)
        return (registerFormat | DataFormatJS) == DataFormatJSCell;
    
    DataFormat spillFormat = info.spillFormat();
    if (spillFormat != DataFormatNone)
        return (spillFormat | DataFormatJS) == DataFormatJSCell;
    
    return false;
}

bool JITCodeGenerator::isKnownNotInteger(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    return (info.registerFormat() | DataFormatJS) == DataFormatJSDouble
        || (info.registerFormat() | DataFormatJS) == DataFormatJSCell
        || (node.isConstant() && !valueOfJSConstant(nodeIndex).isInt32());
}

template<typename To, typename From>
inline To safeCast(From value)
{
    To result = static_cast<To>(value);
    ASSERT(result == value);
    return result;
}

JITCompiler::Call JITCodeGenerator::cachedGetById(GPRReg baseGPR, GPRReg resultGPR, GPRReg scratchGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget, NodeType nodeType)
{
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::Jump structureCheck = m_jit.branchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(baseGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    
    m_jit.loadPtr(JITCompiler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), resultGPR);
    JITCompiler::DataLabelCompact loadWithPatch = m_jit.loadPtrWithCompactAddressOffsetPatch(JITCompiler::Address(resultGPR, 0), resultGPR);
    
    JITCompiler::Jump done = m_jit.jump();

    structureCheck.link(&m_jit);
    
    if (slowPathTarget.isSet())
        slowPathTarget.link(&m_jit);
    
    JITCompiler::Label slowCase = m_jit.label();

    silentSpillAllRegisters(resultGPR);
    m_jit.move(baseGPR, GPRInfo::argumentGPR1);
    m_jit.move(JITCompiler::ImmPtr(identifier(identifierNumber)), GPRInfo::argumentGPR2);
    m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    JITCompiler::Call functionCall;
    switch (nodeType) {
    case GetById:
        functionCall = appendCallWithExceptionCheck(operationGetByIdOptimize);
        break;
        
    case GetMethod:
        functionCall = appendCallWithExceptionCheck(operationGetMethodOptimize);
        break;
        
    default:
        ASSERT_NOT_REACHED();
        return JITCompiler::Call();
    }
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    silentFillAllRegisters(resultGPR);
    
    done.link(&m_jit);
    
    JITCompiler::Label doneLabel = m_jit.label();

    int16_t checkImmToCall = safeCast<int16_t>(m_jit.differenceBetween(structureToCompare, functionCall));
    int16_t callToCheck = safeCast<int16_t>(m_jit.differenceBetween(functionCall, structureCheck));
    int16_t callToLoad = safeCast<int16_t>(m_jit.differenceBetween(functionCall, loadWithPatch));
    int16_t callToSlowCase = safeCast<int16_t>(m_jit.differenceBetween(functionCall, slowCase));
    int16_t callToDone = safeCast<int16_t>(m_jit.differenceBetween(functionCall, doneLabel));
    
    m_jit.addPropertyAccess(functionCall, checkImmToCall, callToCheck, callToLoad, callToSlowCase, callToDone, safeCast<int8_t>(baseGPR), safeCast<int8_t>(resultGPR), safeCast<int8_t>(scratchGPR));
    
    if (scratchGPR != resultGPR && scratchGPR != InvalidGPRReg)
        unlock(scratchGPR);
    
    return functionCall;
}

void JITCodeGenerator::writeBarrier(MacroAssembler&, GPRReg owner, GPRReg scratch)
{
    UNUSED_PARAM(owner);
    UNUSED_PARAM(scratch);
    ASSERT(owner != scratch);
}

void JITCodeGenerator::cachedPutById(GPRReg baseGPR, GPRReg valueGPR, GPRReg scratchGPR, unsigned identifierNumber, PutKind putKind, JITCompiler::Jump slowPathTarget)
{
    JITCompiler::DataLabelPtr structureToCompare;
    JITCompiler::Jump structureCheck = m_jit.branchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(baseGPR, JSCell::structureOffset()), structureToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    
    writeBarrier(m_jit, baseGPR, scratchGPR);

    m_jit.loadPtr(JITCompiler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
    JITCompiler::DataLabel32 storeWithPatch = m_jit.storePtrWithAddressOffsetPatch(valueGPR, JITCompiler::Address(scratchGPR, 0));

    JITCompiler::Jump done = m_jit.jump();

    structureCheck.link(&m_jit);

    if (slowPathTarget.isSet())
        slowPathTarget.link(&m_jit);

    JITCompiler::Label slowCase = m_jit.label();

    silentSpillAllRegisters(InvalidGPRReg);
    setupTwoStubArgs<GPRInfo::argumentGPR1, GPRInfo::argumentGPR2>(valueGPR, baseGPR);
    m_jit.move(JITCompiler::ImmPtr(identifier(identifierNumber)), GPRInfo::argumentGPR3);
    m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    V_DFGOperation_EJJI optimizedCall;
    if (m_jit.codeBlock()->isStrictMode()) {
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
    JITCompiler::Call functionCall = appendCallWithExceptionCheck(optimizedCall);
    silentFillAllRegisters(InvalidGPRReg);

    done.link(&m_jit);
    JITCompiler::Label doneLabel = m_jit.label();

    int16_t checkImmToCall = safeCast<int16_t>(m_jit.differenceBetween(structureToCompare, functionCall));
    int16_t callToCheck = safeCast<int16_t>(m_jit.differenceBetween(functionCall, structureCheck));
    int16_t callToStore = safeCast<int16_t>(m_jit.differenceBetween(functionCall, storeWithPatch));
    int16_t callToSlowCase = safeCast<int16_t>(m_jit.differenceBetween(functionCall, slowCase));
    int16_t callToDone = safeCast<int16_t>(m_jit.differenceBetween(functionCall, doneLabel));

    m_jit.addPropertyAccess(functionCall, checkImmToCall, callToCheck, callToStore, callToSlowCase, callToDone, safeCast<int8_t>(baseGPR), safeCast<int8_t>(valueGPR), safeCast<int8_t>(scratchGPR));
}

void JITCodeGenerator::cachedGetMethod(GPRReg baseGPR, GPRReg resultGPR, GPRReg scratchGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget)
{
    JITCompiler::Call slowCall;
    JITCompiler::DataLabelPtr structToCompare, protoObj, protoStructToCompare, putFunction;
    
    JITCompiler::Jump wrongStructure = m_jit.branchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(baseGPR, JSCell::structureOffset()), structToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    protoObj = m_jit.moveWithPatch(JITCompiler::TrustedImmPtr(0), resultGPR);
    JITCompiler::Jump wrongProtoStructure = m_jit.branchPtrWithPatch(JITCompiler::NotEqual, JITCompiler::Address(resultGPR, JSCell::structureOffset()), protoStructToCompare, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(-1)));
    
    putFunction = m_jit.moveWithPatch(JITCompiler::TrustedImmPtr(0), resultGPR);
    
    JITCompiler::Jump done = m_jit.jump();
    
    wrongStructure.link(&m_jit);
    wrongProtoStructure.link(&m_jit);
    
    slowCall = cachedGetById(baseGPR, resultGPR, scratchGPR, identifierNumber, slowPathTarget, GetMethod);
    
    done.link(&m_jit);
    
    m_jit.addMethodGet(slowCall, structToCompare, protoObj, protoStructToCompare, putFunction);
}

void JITCodeGenerator::nonSpeculativeNonPeepholeCompareNull(NodeIndex operand, bool invert)
{
    JSValueOperand arg(this, operand);
    GPRReg argGPR = arg.gpr();
    
    GPRTemporary result(this, arg);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::Jump notCell;
    
    if (!isKnownCell(operand))
        notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, argGPR, GPRInfo::tagMaskRegister);
    
    m_jit.loadPtr(JITCompiler::Address(argGPR, JSCell::structureOffset()), resultGPR);
    m_jit.test8(invert ? JITCompiler::Zero : JITCompiler::NonZero, JITCompiler::Address(resultGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined), resultGPR);
    
    if (!isKnownCell(operand)) {
        JITCompiler::Jump done = m_jit.jump();
        
        notCell.link(&m_jit);
        
        m_jit.move(argGPR, resultGPR);
        m_jit.andPtr(JITCompiler::TrustedImm32(~TagBitUndefined), resultGPR);
        m_jit.comparePtr(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImm32(ValueNull), resultGPR);
        
        done.link(&m_jit);
    }
    
    m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
    jsValueResult(resultGPR, m_compileIndex);
}

void JITCodeGenerator::nonSpeculativePeepholeBranchNull(NodeIndex operand, NodeIndex branchNodeIndex, bool invert)
{
    Node& branchNode = m_jit.graph()[branchNodeIndex];
    BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.takenBytecodeOffset());
    BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.notTakenBytecodeOffset());
    
    if (taken == (m_block + 1)) {
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
    
    if (!isKnownCell(operand))
        notCell = m_jit.branchTestPtr(MacroAssembler::NonZero, argGPR, GPRInfo::tagMaskRegister);
    
    m_jit.loadPtr(JITCompiler::Address(argGPR, JSCell::structureOffset()), resultGPR);
    addBranch(m_jit.branchTest8(invert ? JITCompiler::Zero : JITCompiler::NonZero, JITCompiler::Address(resultGPR, Structure::typeInfoFlagsOffset()), JITCompiler::TrustedImm32(MasqueradesAsUndefined)), taken);
    
    if (!isKnownCell(operand)) {
        addBranch(m_jit.jump(), notTaken);
        
        notCell.link(&m_jit);
        
        m_jit.move(argGPR, resultGPR);
        m_jit.andPtr(JITCompiler::TrustedImm32(~TagBitUndefined), resultGPR);
        addBranch(m_jit.branchPtr(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImmPtr(reinterpret_cast<void*>(ValueNull))), taken);
    }
    
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

bool JITCodeGenerator::nonSpeculativeCompareNull(Node& node, NodeIndex operand, bool invert)
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

void JITCodeGenerator::nonSpeculativePeepholeBranch(Node& node, NodeIndex branchNodeIndex, MacroAssembler::RelationalCondition cond, Z_DFGOperation_EJJ helperFunction)
{
    Node& branchNode = m_jit.graph()[branchNodeIndex];
    BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.takenBytecodeOffset());
    BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.notTakenBytecodeOffset());

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
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1()) || isKnownNotInteger(node.child2())) {
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();
    
        flushRegisters();

        callOperation(helperFunction, resultGPR, arg1GPR, arg2GPR);
        addBranch(m_jit.branchTest8(callResultCondition, resultGPR), taken);
    } else {
        GPRTemporary result(this, arg2);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();
    
        if (!isKnownInteger(node.child1()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister));
        if (!isKnownInteger(node.child2()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister));
    
        addBranch(m_jit.branch32(cond, arg1GPR, arg2GPR), taken);
    
        if (!isKnownInteger(node.child1()) || !isKnownInteger(node.child2())) {
            addBranch(m_jit.jump(), notTaken);
    
            slowPath.link(&m_jit);
    
            silentSpillAllRegisters(resultGPR);
            setupStubArguments(arg1GPR, arg2GPR);
            m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
            appendCallWithExceptionCheck(helperFunction);
            m_jit.move(GPRInfo::returnValueGPR, resultGPR);
            silentFillAllRegisters(resultGPR);
        
            addBranch(m_jit.branchTest8(callResultCondition, resultGPR), taken);
        }
    }

    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

void JITCodeGenerator::nonSpeculativeNonPeepholeCompare(Node& node, MacroAssembler::RelationalCondition cond, Z_DFGOperation_EJJ helperFunction)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    JITCompiler::JumpList slowPath;
    
    if (isKnownNotInteger(node.child1()) || isKnownNotInteger(node.child2())) {
        GPRResult result(this);
        GPRReg resultGPR = result.gpr();
    
        arg1.use();
        arg2.use();
    
        flushRegisters();
        
        callOperation(helperFunction, resultGPR, arg1GPR, arg2GPR);
        
        m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
    } else {
        GPRTemporary result(this, arg2);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();
    
        if (!isKnownInteger(node.child1()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg1GPR, GPRInfo::tagTypeNumberRegister));
        if (!isKnownInteger(node.child2()))
            slowPath.append(m_jit.branchPtr(MacroAssembler::Below, arg2GPR, GPRInfo::tagTypeNumberRegister));
    
        m_jit.compare32(cond, arg1GPR, arg2GPR, resultGPR);
    
        if (!isKnownInteger(node.child1()) || !isKnownInteger(node.child2())) {
            JITCompiler::Jump haveResult = m_jit.jump();
    
            slowPath.link(&m_jit);
        
            silentSpillAllRegisters(resultGPR);
            setupStubArguments(arg1GPR, arg2GPR);
            m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
            appendCallWithExceptionCheck(helperFunction);
            m_jit.move(GPRInfo::returnValueGPR, resultGPR);
            silentFillAllRegisters(resultGPR);
        
            m_jit.andPtr(TrustedImm32(1), resultGPR);
        
            haveResult.link(&m_jit);
        }
        
        m_jit.or32(TrustedImm32(ValueFalse), resultGPR);
        
        jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
    }
}

bool JITCodeGenerator::nonSpeculativeCompare(Node& node, MacroAssembler::RelationalCondition cond, Z_DFGOperation_EJJ helperFunction)
{
    NodeIndex branchNodeIndex = detectPeepHoleBranch();
    if (branchNodeIndex != NoNode) {
        ASSERT(node.adjustedRefCount() == 1);
        
        nonSpeculativePeepholeBranch(node, branchNodeIndex, cond, helperFunction);
    
        m_compileIndex = branchNodeIndex;
        
        return true;
    }
    
    nonSpeculativeNonPeepholeCompare(node, cond, helperFunction);
    
    return false;
}

void JITCodeGenerator::nonSpeculativePeepholeStrictEq(Node& node, NodeIndex branchNodeIndex, bool invert)
{
    Node& branchNode = m_jit.graph()[branchNodeIndex];
    BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.takenBytecodeOffset());
    BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(branchNode.notTakenBytecodeOffset());

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
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node.child1()) && isKnownCell(node.child2())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        addBranch(m_jit.branchPtr(JITCompiler::Equal, arg1GPR, arg2GPR), invert ? notTaken : taken);
        
        silentSpillAllRegisters(resultGPR);
        setupStubArguments(arg1GPR, arg2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationCompareStrictEqCell);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        addBranch(m_jit.branchTest8(invert ? JITCompiler::NonZero : JITCompiler::Zero, resultGPR), taken);
    } else {
        m_jit.orPtr(arg1GPR, arg2GPR, resultGPR);
        
        JITCompiler::Jump twoCellsCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, GPRInfo::tagMaskRegister);
        
        JITCompiler::Jump numberCase = m_jit.branchTestPtr(JITCompiler::NonZero, resultGPR, GPRInfo::tagTypeNumberRegister);
        
        addBranch(m_jit.branch32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, arg1GPR, arg2GPR), taken);
        addBranch(m_jit.jump(), notTaken);
        
        twoCellsCase.link(&m_jit);
        addBranch(m_jit.branchPtr(JITCompiler::Equal, arg1GPR, arg2GPR), invert ? notTaken : taken);
        
        numberCase.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        setupStubArguments(arg1GPR, arg2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationCompareStrictEq);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        addBranch(m_jit.branchTest8(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultGPR), taken);
    }
    
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

void JITCodeGenerator::nonSpeculativeNonPeepholeStrictEq(Node& node, bool invert)
{
    JSValueOperand arg1(this, node.child1());
    JSValueOperand arg2(this, node.child2());
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node.child1()) && isKnownCell(node.child2())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        JITCompiler::Jump notEqualCase = m_jit.branchPtr(JITCompiler::NotEqual, arg1GPR, arg2GPR);
        
        m_jit.move(JITCompiler::TrustedImmPtr(JSValue::encode(jsBoolean(!invert))), resultGPR);
        
        JITCompiler::Jump done = m_jit.jump();

        notEqualCase.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        setupStubArguments(arg1GPR, arg2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationCompareStrictEqCell);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultGPR);
        m_jit.or32(JITCompiler::TrustedImm32(ValueFalse), resultGPR);
        
        done.link(&m_jit);
    } else {
        m_jit.orPtr(arg1GPR, arg2GPR, resultGPR);
        
        JITCompiler::Jump twoCellsCase = m_jit.branchTestPtr(JITCompiler::Zero, resultGPR, GPRInfo::tagMaskRegister);
        
        JITCompiler::Jump numberCase = m_jit.branchTestPtr(JITCompiler::NonZero, resultGPR, GPRInfo::tagTypeNumberRegister);
        
        m_jit.compare32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, arg1GPR, arg2GPR, resultGPR);
        
        JITCompiler::Jump done1 = m_jit.jump();
        
        twoCellsCase.link(&m_jit);
        JITCompiler::Jump notEqualCase = m_jit.branchPtr(JITCompiler::NotEqual, arg1GPR, arg2GPR);
        
        m_jit.move(JITCompiler::TrustedImmPtr(JSValue::encode(jsBoolean(!invert))), resultGPR);
        
        JITCompiler::Jump done2 = m_jit.jump();
        
        numberCase.link(&m_jit);
        notEqualCase.link(&m_jit);
        
        silentSpillAllRegisters(resultGPR);
        setupStubArguments(arg1GPR, arg2GPR);
        m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        appendCallWithExceptionCheck(operationCompareStrictEq);
        m_jit.move(GPRInfo::returnValueGPR, resultGPR);
        silentFillAllRegisters(resultGPR);
        
        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultGPR);

        done1.link(&m_jit);

        m_jit.or32(JITCompiler::TrustedImm32(ValueFalse), resultGPR);
        
        done2.link(&m_jit);
    }
    
    jsValueResult(resultGPR, m_compileIndex, UseChildrenCalledExplicitly);
}

bool JITCodeGenerator::nonSpeculativeStrictEq(Node& node, bool invert)
{
    if (!invert && (isKnownNumeric(node.child1()) || isKnownNumeric(node.child2())))
        return nonSpeculativeCompare(node, MacroAssembler::Equal, operationCompareStrictEq);
    
    NodeIndex branchNodeIndex = detectPeepHoleBranch();
    if (branchNodeIndex != NoNode) {
        ASSERT(node.adjustedRefCount() == 1);
        
        nonSpeculativePeepholeStrictEq(node, branchNodeIndex, invert);
    
        m_compileIndex = branchNodeIndex;
        
        return true;
    }
    
    nonSpeculativeNonPeepholeStrictEq(node, invert);
    
    return false;
}

void JITCodeGenerator::emitBranch(Node& node)
{
    JSValueOperand value(this, node.child1());
    GPRReg valueGPR = value.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    value.use();
    
    BlockIndex taken = m_jit.graph().blockIndexForBytecodeOffset(node.takenBytecodeOffset());
    BlockIndex notTaken = m_jit.graph().blockIndexForBytecodeOffset(node.notTakenBytecodeOffset());

    addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsNumber(0)))), notTaken);
    addBranch(m_jit.branchPtr(MacroAssembler::AboveOrEqual, valueGPR, GPRInfo::tagTypeNumberRegister), taken);
    addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsBoolean(false)))), notTaken);
    addBranch(m_jit.branchPtr(MacroAssembler::Equal, valueGPR, MacroAssembler::ImmPtr(JSValue::encode(jsBoolean(true)))), taken);
    
    silentSpillAllRegisters(resultGPR);
    m_jit.move(valueGPR, GPRInfo::argumentGPR1);
    m_jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    appendCallWithExceptionCheck(dfgConvertJSValueToBoolean);
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    silentFillAllRegisters(resultGPR);
    
    addBranch(m_jit.branchTest8(MacroAssembler::NonZero, resultGPR), taken);
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
    
    noResult(m_compileIndex, UseChildrenCalledExplicitly);
}

void JITCodeGenerator::emitCall(Node& node)
{
    P_DFGOperation_E slowCallFunction;
    bool isCall;
    
    if (node.op == Call) {
        slowCallFunction = operationLinkCall;
        isCall = true;
    } else {
        ASSERT(node.op == Construct);
        slowCallFunction = operationLinkConstruct;
        isCall = false;
    }
    
    NodeIndex calleeNodeIndex = m_jit.graph().m_varArgChildren[node.firstChild()];
    JSValueOperand callee(this, calleeNodeIndex);
    GPRReg calleeGPR = callee.gpr();
    use(calleeNodeIndex);
    
    // the call instruction's first child is either the function (normal call) or the
    // receiver (method call). subsequent children are the arguments.
    int numArgs = node.numChildren() - 1;
    
    // amount of stuff (in units of sizeof(Register)) that we need to place at the
    // top of the JS stack.
    int callDataSize = 0;

    // first there are the arguments
    callDataSize += numArgs;
    
    // and then there is the call frame header
    callDataSize += RegisterFile::CallFrameHeaderSize;
    
    m_jit.storePtr(MacroAssembler::TrustedImmPtr(JSValue::encode(jsNumber(numArgs))), addressOfCallData(RegisterFile::ArgumentCount));
    m_jit.storePtr(GPRInfo::callFrameRegister, addressOfCallData(RegisterFile::CallerFrame));
    
    if (node.op == Construct)
        use(m_jit.graph().m_varArgChildren[node.firstChild() + 1]);
    
    for (int argIdx = (node.op == Call ? 0 : 1); argIdx < numArgs; argIdx++) {
        NodeIndex argNodeIndex = m_jit.graph().m_varArgChildren[node.firstChild() + 1 + argIdx];
        JSValueOperand arg(this, argNodeIndex);
        GPRReg argGPR = arg.gpr();
        use(argNodeIndex);
        
        m_jit.storePtr(argGPR, addressOfCallData(-callDataSize + argIdx));
    }
    
    m_jit.storePtr(calleeGPR, addressOfCallData(RegisterFile::Callee));
    
    flushRegisters();
    
    GPRResult result(this);
    GPRReg resultGPR = result.gpr();

    JITCompiler::DataLabelPtr targetToCheck;
    JITCompiler::Jump slowPath;
    
    slowPath = m_jit.branchPtrWithPatch(MacroAssembler::NotEqual, calleeGPR, targetToCheck, MacroAssembler::TrustedImmPtr(JSValue::encode(JSValue())));
    m_jit.loadPtr(MacroAssembler::Address(calleeGPR, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), resultGPR);
    m_jit.storePtr(resultGPR, addressOfCallData(RegisterFile::ScopeChain));

    m_jit.addPtr(Imm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister);
    
    JITCompiler::Call fastCall = m_jit.nearCall();
    m_jit.notifyCall(fastCall, m_jit.graph()[m_compileIndex].exceptionInfo);
    
    JITCompiler::Jump done = m_jit.jump();
    
    slowPath.link(&m_jit);
    
    m_jit.addPtr(Imm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    JITCompiler::Call slowCall = m_jit.appendCallWithFastExceptionCheck(slowCallFunction, m_jit.graph()[m_compileIndex].exceptionInfo);
    m_jit.move(Imm32(numArgs), GPRInfo::regT1);
    m_jit.addPtr(Imm32(m_jit.codeBlock()->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister);
    m_jit.notifyCall(m_jit.call(GPRInfo::returnValueGPR), m_jit.graph()[m_compileIndex].exceptionInfo);
    
    done.link(&m_jit);
    
    m_jit.move(GPRInfo::returnValueGPR, resultGPR);
    
    jsValueResult(resultGPR, m_compileIndex, DataFormatJS, UseChildrenCalledExplicitly);
    
    m_jit.addJSCall(fastCall, slowCall, targetToCheck, isCall, m_jit.graph()[m_compileIndex].exceptionInfo);
}

#ifndef NDEBUG
static const char* dataFormatString(DataFormat format)
{
    // These values correspond to the DataFormat enum.
    const char* strings[] = {
        "[  ]",
        "[ i]",
        "[ d]",
        "[ c]",
        "Err!",
        "Err!",
        "Err!",
        "Err!",
        "[J ]",
        "[Ji]",
        "[Jd]",
        "[Jc]",
        "Err!",
        "Err!",
        "Err!",
        "Err!",
    };
    return strings[format];
}

void JITCodeGenerator::dump(const char* label)
{
    if (label)
        fprintf(stderr, "<%s>\n", label);

    fprintf(stderr, "  gprs:\n");
    m_gprs.dump();
    fprintf(stderr, "  fprs:\n");
    m_fprs.dump();
    fprintf(stderr, "  VirtualRegisters:\n");
    for (unsigned i = 0; i < m_generationInfo.size(); ++i) {
        GenerationInfo& info = m_generationInfo[i];
        if (info.alive())
            fprintf(stderr, "    % 3d:%s%s", i, dataFormatString(info.registerFormat()), dataFormatString(info.spillFormat()));
        else
            fprintf(stderr, "    % 3d:[__][__]", i);
        if (info.registerFormat() == DataFormatDouble)
            fprintf(stderr, ":fpr%d\n", info.fpr());
        else if (info.registerFormat() != DataFormatNone) {
            ASSERT(info.gpr() != InvalidGPRReg);
            fprintf(stderr, ":%s\n", GPRInfo::debugName(info.gpr()));
        } else
            fprintf(stderr, "\n");
    }
    if (label)
        fprintf(stderr, "</%s>\n", label);
}
#endif


#if DFG_CONSISTENCY_CHECK
void JITCodeGenerator::checkConsistency()
{
    bool failed = false;

    for (gpr_iterator iter = m_gprs.begin(); iter != m_gprs.end(); ++iter) {
        if (iter.isLocked()) {
            fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: gpr %s is locked.\n", iter.debugName());
            failed = true;
        }
    }
    for (fpr_iterator iter = m_fprs.begin(); iter != m_fprs.end(); ++iter) {
        if (iter.isLocked()) {
            fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: fpr %s is locked.\n", iter.debugName());
            failed = true;
        }
    }

    for (unsigned i = 0; i < m_generationInfo.size(); ++i) {
        VirtualRegister virtualRegister = (VirtualRegister)i;
        GenerationInfo& info = m_generationInfo[virtualRegister];
        if (!info.alive())
            continue;
        switch (info.registerFormat()) {
        case DataFormatNone:
            break;
        case DataFormatInteger:
        case DataFormatCell:
        case DataFormatJS:
        case DataFormatJSInteger:
        case DataFormatJSDouble:
        case DataFormatJSCell: {
            GPRReg gpr = info.gpr();
            ASSERT(gpr != InvalidGPRReg);
            if (m_gprs.name(gpr) != virtualRegister) {
                fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for virtual register %d (gpr %s).\n", virtualRegister, GPRInfo::debugName(gpr));
                failed = true;
            }
            break;
        }
        case DataFormatDouble: {
            FPRReg fpr = info.fpr();
            ASSERT(fpr != InvalidFPRReg);
            if (m_fprs.name(fpr) != virtualRegister) {
                fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for virtual register %d (fpr %s).\n", virtualRegister, FPRInfo::debugName(fpr));
                failed = true;
            }
            break;
        }
        }
    }

    for (gpr_iterator iter = m_gprs.begin(); iter != m_gprs.end(); ++iter) {
        VirtualRegister virtualRegister = iter.name();
        if (virtualRegister == InvalidVirtualRegister)
            continue;

        GenerationInfo& info = m_generationInfo[virtualRegister];
        if (iter.regID() != info.gpr()) {
            fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for gpr %s (virtual register %d).\n", iter.debugName(), virtualRegister);
            failed = true;
        }
    }

    for (fpr_iterator iter = m_fprs.begin(); iter != m_fprs.end(); ++iter) {
        VirtualRegister virtualRegister = iter.name();
        if (virtualRegister == InvalidVirtualRegister)
            continue;

        GenerationInfo& info = m_generationInfo[virtualRegister];
        if (iter.regID() != info.fpr()) {
            fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for fpr %s (virtual register %d).\n", iter.debugName(), virtualRegister);
            failed = true;
        }
    }

    if (failed) {
        dump();
        CRASH();
    }
}
#endif

GPRTemporary::GPRTemporary(JITCodeGenerator* jit)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, GPRReg specific)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate(specific);
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, SpeculateIntegerOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, SpeculateIntegerOperand& op1, SpeculateIntegerOperand& op2)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else if (m_jit->canReuse(op2.index()))
        m_gpr = m_jit->reuse(op2.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, IntegerOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, IntegerOperand& op1, IntegerOperand& op2)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else if (m_jit->canReuse(op2.index()))
        m_gpr = m_jit->reuse(op2.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, SpeculateCellOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

FPRTemporary::FPRTemporary(JITCodeGenerator* jit)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(JITCodeGenerator* jit, DoubleOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(JITCodeGenerator* jit, DoubleOperand& op1, DoubleOperand& op2)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else if (m_jit->canReuse(op2.index()))
        m_fpr = m_jit->reuse(op2.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(JITCodeGenerator* jit, SpeculateDoubleOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(JITCodeGenerator* jit, SpeculateDoubleOperand& op1, SpeculateDoubleOperand& op2)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else if (m_jit->canReuse(op2.index()))
        m_fpr = m_jit->reuse(op2.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

} } // namespace JSC::DFG

#endif
