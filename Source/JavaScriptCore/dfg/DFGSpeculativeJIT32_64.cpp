/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
#include "CallFrameShuffler.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGCallArrayAllocatorSlowPathGenerator.h"
#include "DFGDoesGC.h"
#include "DFGOperations.h"
#include "DFGSlowPathGenerator.h"
#include "DirectArguments.h"
#include "GetterSetter.h"
#include "HasOwnPropertyCache.h"
#include "HashMapImpl.h"
#include "JSLexicalEnvironment.h"
#include "JSPropertyNameEnumerator.h"
#include "ObjectPrototype.h"
#include "JSCInlines.h"
#include "SetupVarargsFrame.h"
#include "SuperSampler.h"
#include "Watchdog.h"

namespace JSC { namespace DFG {

#if USE(JSVALUE32_64)

static_assert(SpecCellCheck == SpecCell, "This is strongly assumed in the 32-bit DFG backend.");

bool SpeculativeJIT::fillJSValue(Edge edge, GPRReg& tagGPR, GPRReg& payloadGPR, FPRReg& fpr)
{
    // FIXME: For double we could fill with a FPR.
    UNUSED_PARAM(fpr);

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        payloadGPR = allocate();
        tagGPR = allocate();

        if (edge->hasConstant()) {
            JSValue value = edge->asJSValue();
            m_jit.move(Imm32(value.tag()), tagGPR);
            m_jit.move(Imm32(value.payload()), payloadGPR);
            m_gprs.retain(tagGPR, virtualRegister, SpillOrderConstant);
            m_gprs.retain(payloadGPR, virtualRegister, SpillOrderConstant);
            info.fillJSValue(m_stream, tagGPR, payloadGPR, DataFormatJS);
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat != DataFormatNone && spillFormat != DataFormatStorage);
            switch (spillFormat) {
            case DataFormatInt32:
                m_jit.move(TrustedImm32(JSValue::Int32Tag), tagGPR);
                m_jit.load32(JITCompiler::payloadFor(virtualRegister), payloadGPR);
                spillFormat = DataFormatJSInt32; // This will be used as the new register format.
                break;
            case DataFormatCell:
                m_jit.move(TrustedImm32(JSValue::CellTag), tagGPR);
                m_jit.load32(JITCompiler::payloadFor(virtualRegister), payloadGPR);
                spillFormat = DataFormatJSCell; // This will be used as the new register format.
                break;
            case DataFormatBoolean:
                m_jit.move(TrustedImm32(JSValue::BooleanTag), tagGPR);
                m_jit.load32(JITCompiler::payloadFor(virtualRegister), payloadGPR);
                spillFormat = DataFormatJSBoolean; // This will be used as the new register format.
                break;
            default:
                m_jit.loadValue(JITCompiler::addressFor(virtualRegister), JSValueRegs { tagGPR, payloadGPR });
                break;
            }
            m_gprs.retain(tagGPR, virtualRegister, SpillOrderSpilled);
            m_gprs.retain(payloadGPR, virtualRegister, SpillOrderSpilled);
            info.fillJSValue(m_stream, tagGPR, payloadGPR, spillFormat == DataFormatJSDouble ? DataFormatJS : spillFormat);
        }

        return true;
    }

    case DataFormatInt32:
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
        int32_t tag = JSValue::EmptyValueTag;
        DataFormat fillFormat = DataFormatJS;
        switch (info.registerFormat()) {
        case DataFormatInt32:
            tag = JSValue::Int32Tag;
            fillFormat = DataFormatJSInt32;
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
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        m_jit.move(TrustedImm32(tag), tagGPR);
        m_gprs.release(gpr);
        m_gprs.retain(tagGPR, virtualRegister, SpillOrderJS);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderJS);
        info.fillJSValue(m_stream, tagGPR, payloadGPR, fillFormat);
        return true;
    }

    case DataFormatJSDouble:
    case DataFormatJS:
    case DataFormatJSInt32:
    case DataFormatJSCell:
    case DataFormatJSBoolean: {
        tagGPR = info.tagGPR();
        payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);
        return true;
    }
        
    case DataFormatStorage:
    case DataFormatDouble:
        // this type currently never occurs
        RELEASE_ASSERT_NOT_REACHED();

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    }
}

void SpeculativeJIT::cachedGetById(CodeOrigin origin, JSValueRegs base, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratchGPR, CacheableIdentifier identifier, JITCompiler::Jump slowPathTarget , SpillRegistersMode mode, AccessType type)
{
    cachedGetById(origin, base.tagGPR(), base.payloadGPR(), result.tagGPR(), result.payloadGPR(), stubInfoGPR, scratchGPR, identifier, slowPathTarget, mode, type);
}

void SpeculativeJIT::cachedGetById(
    CodeOrigin codeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, GPRReg stubInfoGPR,
    GPRReg scratchGPR, CacheableIdentifier identifier, JITCompiler::Jump slowPathTarget, SpillRegistersMode spillMode, AccessType type)
{
    UNUSED_PARAM(stubInfoGPR);
    // This is a hacky fix for when the register allocator decides to alias the base payload with the result tag. This only happens
    // in the case of GetByIdFlush/GetByIdDirectFlush, which has a relatively expensive register allocation story already so we probably don't need to
    // trip over one move instruction.
    if (basePayloadGPR == resultTagGPR) {
        RELEASE_ASSERT(basePayloadGPR != resultPayloadGPR);
        
        if (baseTagGPROrNone == resultPayloadGPR) {
            m_jit.swap(basePayloadGPR, baseTagGPROrNone);
            baseTagGPROrNone = resultTagGPR;
        } else
            m_jit.move(basePayloadGPR, resultPayloadGPR);
        basePayloadGPR = resultPayloadGPR;
    }
    
    RegisterSet usedRegisters = this->usedRegisters();
    if (spillMode == DontSpill) {
        // We've already flushed registers to the stack, we don't need to spill these.
        usedRegisters.set(JSValueRegs(baseTagGPROrNone, basePayloadGPR), false);
        usedRegisters.set(JSValueRegs(resultTagGPR, resultPayloadGPR), false);
    }
    
    CallSiteIndex callSite = m_jit.recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    auto [ stubInfo, stubInfoConstant ] = m_jit.addStructureStubInfo();
    JITGetByIdGenerator gen(
        m_jit.codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        JSValueRegs(baseTagGPROrNone, basePayloadGPR), JSValueRegs(resultTagGPR, resultPayloadGPR), InvalidGPRReg, type);
    
    gen.generateFastPath(m_jit, scratchGPR);
    
    JITCompiler::JumpList slowCases;
    if (slowPathTarget.isSet())
        slowCases.append(slowPathTarget);
    slowCases.append(gen.slowPathJump());

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (baseTagGPROrNone == InvalidGPRReg) {
        slowPath = slowPathCall(
            slowCases, this, appropriateOptimizingGetByIdFunction(type),
            JSValueRegs(resultTagGPR, resultPayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(codeOrigin)), TrustedImmPtr(gen.stubInfo()),
            CCallHelpers::CellValue(basePayloadGPR),
            identifier.rawBits());
    } else {
        slowPath = slowPathCall(
            slowCases, this, appropriateOptimizingGetByIdFunction(type),
            JSValueRegs(resultTagGPR, resultPayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(codeOrigin)), TrustedImmPtr(gen.stubInfo()), JSValueRegs(baseTagGPROrNone, basePayloadGPR), identifier.rawBits());
    }

    m_jit.addGetById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::cachedGetByIdWithThis(
    CodeOrigin codeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg thisTagGPR, GPRReg thisPayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, GPRReg stubInfoGPR, GPRReg scratchGPR,
    CacheableIdentifier identifier, const JITCompiler::JumpList& slowPathTarget)
{
    UNUSED_PARAM(stubInfoGPR);
    RegisterSet usedRegisters = this->usedRegisters();
    
    CallSiteIndex callSite = m_jit.recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    auto [ stubInfo, stubInfoConstant ] = m_jit.addStructureStubInfo();
    JITGetByIdWithThisGenerator gen(
        m_jit.codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        JSValueRegs(resultTagGPR, resultPayloadGPR), JSValueRegs(baseTagGPROrNone, basePayloadGPR), JSValueRegs(thisTagGPR, thisPayloadGPR), InvalidGPRReg);
    
    gen.generateFastPath(m_jit, scratchGPR);
    
    JITCompiler::JumpList slowCases;
    if (!slowPathTarget.empty())
        slowCases.append(slowPathTarget);
    slowCases.append(gen.slowPathJump());

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (baseTagGPROrNone == InvalidGPRReg && thisTagGPR == InvalidGPRReg) {
        slowPath = slowPathCall(
            slowCases, this, operationGetByIdWithThisOptimize,
            JSValueRegs(resultTagGPR, resultPayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(codeOrigin)), TrustedImmPtr(gen.stubInfo()),
            CCallHelpers::CellValue(basePayloadGPR),
            CCallHelpers::CellValue(thisPayloadGPR),
            identifier.rawBits());
    } else {
        ASSERT(baseTagGPROrNone != InvalidGPRReg);
        ASSERT(thisTagGPR != InvalidGPRReg);
        
        slowPath = slowPathCall(
            slowCases, this, operationGetByIdWithThisOptimize,
            JSValueRegs(resultTagGPR, resultPayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(codeOrigin)), TrustedImmPtr(gen.stubInfo()), JSValueRegs(baseTagGPROrNone, basePayloadGPR), JSValueRegs(thisTagGPR, thisPayloadGPR), identifier.rawBits());
    }

    m_jit.addGetByIdWithThis(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompareNullOrUndefined(Edge operand)
{
    JSValueOperand arg(this, operand, ManualOperandSpeculation);
    GPRReg argTagGPR = arg.tagGPR();
    GPRReg argPayloadGPR = arg.payloadGPR();

    GPRTemporary resultPayload(this, Reuse, arg, PayloadWord);
    GPRReg resultPayloadGPR = resultPayload.gpr();

    JITCompiler::Jump notCell;
    JITCompiler::Jump notMasqueradesAsUndefined;   
    if (masqueradesAsUndefinedWatchpointIsStillValid()) {
        if (!isKnownCell(operand.node()))
            notCell = m_jit.branchIfNotCell(arg.jsValueRegs());
        
        m_jit.move(TrustedImm32(0), resultPayloadGPR);
        notMasqueradesAsUndefined = m_jit.jump();
    } else {
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();

        if (!isKnownCell(operand.node()))
            notCell = m_jit.branchIfNotCell(arg.jsValueRegs());
        
        JITCompiler::Jump isMasqueradesAsUndefined = m_jit.branchTest8(
            JITCompiler::NonZero, 
            JITCompiler::Address(argPayloadGPR, JSCell::typeInfoFlagsOffset()), 
            JITCompiler::TrustedImm32(MasqueradesAsUndefined));
        
        m_jit.move(TrustedImm32(0), resultPayloadGPR);
        notMasqueradesAsUndefined = m_jit.jump();

        isMasqueradesAsUndefined.link(&m_jit);
        m_jit.loadLinkableConstant(JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(m_currentNode->origin.semantic)), localGlobalObjectGPR);
        m_jit.loadPtr(JITCompiler::Address(argPayloadGPR, JSCell::structureIDOffset()), resultPayloadGPR);
        m_jit.loadPtr(JITCompiler::Address(resultPayloadGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        m_jit.compare32(JITCompiler::Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, resultPayloadGPR);
    }
 
    if (!isKnownCell(operand.node())) {
        JITCompiler::Jump done = m_jit.jump();
        
        notCell.link(&m_jit);
        // null or undefined?
        static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
        m_jit.or32(TrustedImm32(1), argTagGPR, resultPayloadGPR);
        m_jit.compare32(JITCompiler::Equal, resultPayloadGPR, TrustedImm32(JSValue::NullTag), resultPayloadGPR);

        done.link(&m_jit);
    }
    
    notMasqueradesAsUndefined.link(&m_jit);
 
    booleanResult(resultPayloadGPR, m_currentNode);
}

void SpeculativeJIT::nonSpeculativePeepholeBranchNullOrUndefined(Edge operand, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    bool invert = false;
    if (taken == nextBlock()) {
        invert = !invert;
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    JSValueOperand arg(this, operand, ManualOperandSpeculation);
    GPRReg argTagGPR = arg.tagGPR();
    GPRReg argPayloadGPR = arg.payloadGPR();
    
    GPRTemporary result(this, Reuse, arg, TagWord);
    GPRReg resultGPR = result.gpr();

    JITCompiler::Jump notCell;

    if (masqueradesAsUndefinedWatchpointIsStillValid()) {
        if (!isKnownCell(operand.node()))
            notCell = m_jit.branchIfNotCell(arg.jsValueRegs());
        
        jump(invert ? taken : notTaken, ForceJump);
    } else {
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();

        if (!isKnownCell(operand.node()))
            notCell = m_jit.branchIfNotCell(arg.jsValueRegs());
        
        branchTest8(JITCompiler::Zero, 
            JITCompiler::Address(argPayloadGPR, JSCell::typeInfoFlagsOffset()), 
            JITCompiler::TrustedImm32(MasqueradesAsUndefined), 
            invert ? taken : notTaken);
   
        m_jit.loadLinkableConstant(JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(m_currentNode->origin.semantic)), localGlobalObjectGPR);
        m_jit.loadPtr(JITCompiler::Address(argPayloadGPR, JSCell::structureIDOffset()), resultGPR);
        m_jit.loadPtr(JITCompiler::Address(resultGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        branchPtr(JITCompiler::Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, invert ? notTaken : taken);
    }
 
    if (!isKnownCell(operand.node())) {
        jump(notTaken, ForceJump);
        
        notCell.link(&m_jit);
        // null or undefined?
        static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
        m_jit.or32(TrustedImm32(1), argTagGPR, resultGPR);
        branch32(invert ? JITCompiler::NotEqual : JITCompiler::Equal, resultGPR, JITCompiler::TrustedImm32(JSValue::NullTag), taken);
    }
    
    jump(notTaken);
}

void SpeculativeJIT::nonSpeculativePeepholeStrictEq(Node* node, Node* branchNode, bool invert)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        invert = !invert;
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }
    
    JSValueOperand arg1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand arg2(this, node->child2(), ManualOperandSpeculation);
    speculate(node, node->child1());
    speculate(node, node->child2());
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();
    
    GPRTemporary resultPayload(this, Reuse, arg1, PayloadWord);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node->child1().node()) && isKnownCell(node->child2().node())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        branchPtr(JITCompiler::Equal, arg1PayloadGPR, arg2PayloadGPR, invert ? notTaken : taken);
        
        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEqCell, resultPayloadGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), arg1PayloadGPR, arg2PayloadGPR);
        m_jit.exceptionCheck();
        silentFillAllRegisters();

        branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultPayloadGPR, taken);
    } else {
        // FIXME: Add fast paths for twoCells, number etc.

        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEq, resultPayloadGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), arg1Regs, arg2Regs);
        m_jit.exceptionCheck();
        silentFillAllRegisters();

        branchTest32(invert ? JITCompiler::Zero : JITCompiler::NonZero, resultPayloadGPR, taken);
    }
    
    jump(notTaken);
}

void SpeculativeJIT::genericJSValueNonPeepholeStrictEq(Node* node, bool invert)
{
    JSValueOperand arg1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand arg2(this, node->child2(), ManualOperandSpeculation);
    speculate(node, node->child1());
    speculate(node, node->child2());
    GPRReg arg1PayloadGPR = arg1.payloadGPR();
    GPRReg arg2PayloadGPR = arg2.payloadGPR();
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();
    
    GPRTemporary resultPayload(this, Reuse, arg1, PayloadWord);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node->child1().node()) && isKnownCell(node->child2().node())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        // FIXME: this should flush registers instead of silent spill/fill.
        JITCompiler::Jump notEqualCase = m_jit.branchPtr(JITCompiler::NotEqual, arg1PayloadGPR, arg2PayloadGPR);
        
        m_jit.move(JITCompiler::TrustedImm32(!invert), resultPayloadGPR);
        JITCompiler::Jump done = m_jit.jump();

        notEqualCase.link(&m_jit);
        
        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEqCell, resultPayloadGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), arg1PayloadGPR, arg2PayloadGPR);
        m_jit.exceptionCheck();
        silentFillAllRegisters();

        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultPayloadGPR);
        
        done.link(&m_jit);
    } else {
        // FIXME: Add fast paths.

        silentSpillAllRegisters(resultPayloadGPR);
        callOperation(operationCompareStrictEq, resultPayloadGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), arg1Regs, arg2Regs);
        silentFillAllRegisters();
        m_jit.exceptionCheck();
        
        m_jit.andPtr(JITCompiler::TrustedImm32(1), resultPayloadGPR);
    }

    booleanResult(resultPayloadGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileCompareEqPtr(Node* node)
{
    JSValueOperand operand(this, node->child1());
    GPRTemporary result(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg resultGPR = result.gpr();
    m_jit.boxBooleanPayload(false, resultGPR);
    JITCompiler::JumpList notEqual = m_jit.branchIfNotEqual(regs, node->cellOperand()->value());
    m_jit.boxBooleanPayload(true, resultGPR);
    notEqual.link(&m_jit);
    blessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::emitCall(Node* node)
{
    CallLinkInfo::CallType callType;
    bool isVarargs = false;
    bool isForwardVarargs = false;
    bool isTail = false;
    bool isDirect = false;
    bool isEmulatedTail = false;
    switch (node->op()) {
    case Call:
    case CallDirectEval:
        callType = CallLinkInfo::Call;
        break;
    case TailCall:
        callType = CallLinkInfo::TailCall;
        isTail = true;
        break;
    case TailCallInlinedCaller:
        callType = CallLinkInfo::Call;
        isEmulatedTail = true;
        break;
    case Construct:
        callType = CallLinkInfo::Construct;
        break;
    case CallVarargs:
        callType = CallLinkInfo::CallVarargs;
        isVarargs = true;
        break;
    case TailCallVarargs:
        callType = CallLinkInfo::TailCallVarargs;
        isVarargs = true;
        isTail = true;
        break;
    case TailCallVarargsInlinedCaller:
        callType = CallLinkInfo::CallVarargs;
        isVarargs = true;
        isEmulatedTail = true;
        break;
    case ConstructVarargs:
        callType = CallLinkInfo::ConstructVarargs;
        isVarargs = true;
        break;
    case CallForwardVarargs:
        callType = CallLinkInfo::CallVarargs;
        isForwardVarargs = true;
        break;
    case TailCallForwardVarargs:
        callType = CallLinkInfo::TailCallVarargs;
        isTail = true;
        isForwardVarargs = true;
        break;
    case TailCallForwardVarargsInlinedCaller:
        callType = CallLinkInfo::CallVarargs;
        isEmulatedTail = true;
        isForwardVarargs = true;
        break;
    case ConstructForwardVarargs:
        callType = CallLinkInfo::ConstructVarargs;
        isForwardVarargs = true;
        break;
    case DirectCall:
        callType = CallLinkInfo::DirectCall;
        isDirect = true;
        break;
    case DirectConstruct:
        callType = CallLinkInfo::DirectConstruct;
        isDirect = true;
        break;
    case DirectTailCall:
        callType = CallLinkInfo::DirectTailCall;
        isTail = true;
        isDirect = true;
        break;
    case DirectTailCallInlinedCaller:
        callType = CallLinkInfo::DirectCall;
        isEmulatedTail = true;
        isDirect = true;
        break;
    default:
        DFG_CRASH(m_graph, node, "bad node type");
        break;
    }

    Edge calleeEdge = m_graph.child(node, 0);
    GPRReg calleeTagGPR = InvalidGPRReg;
    GPRReg calleePayloadGPR = InvalidGPRReg;
    CallFrameShuffleData shuffleData;
    
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    ExecutableBase* executable = nullptr;
    FunctionExecutable* functionExecutable = nullptr;
    if (isDirect) {
        executable = node->castOperand<ExecutableBase*>();
        functionExecutable = jsDynamicCast<FunctionExecutable*>(executable);
    }
    
    unsigned numPassedArgs = 0;
    unsigned numAllocatedArgs = 0;
    
    // Gotta load the arguments somehow. Varargs is trickier.
    if (isVarargs || isForwardVarargs) {
        RELEASE_ASSERT(!isDirect);
        CallVarargsData* data = node->callVarargsData();

        int numUsedStackSlots = m_graph.m_nextMachineLocal;
        
        if (isForwardVarargs) {
            flushRegisters();
            if (node->child3())
                use(node->child3());
            
            GPRReg scratchGPR1;
            GPRReg scratchGPR2;
            GPRReg scratchGPR3;
            
            scratchGPR1 = JITCompiler::selectScratchGPR();
            scratchGPR2 = JITCompiler::selectScratchGPR(scratchGPR1);
            scratchGPR3 = JITCompiler::selectScratchGPR(scratchGPR1, scratchGPR2);
            
            m_jit.move(TrustedImm32(numUsedStackSlots), scratchGPR2);
            JITCompiler::JumpList slowCase;
            InlineCallFrame* inlineCallFrame;
            if (node->child3())
                inlineCallFrame = node->child3()->origin.semantic.inlineCallFrame();
            else
                inlineCallFrame = node->origin.semantic.inlineCallFrame();
            // emitSetupVarargsFrameFastCase modifies the stack pointer if it succeeds.
            emitSetupVarargsFrameFastCase(vm(), m_jit, scratchGPR2, scratchGPR1, scratchGPR2, scratchGPR3, inlineCallFrame, data->firstVarArgOffset, slowCase);
            JITCompiler::Jump done = m_jit.jump();
            slowCase.link(&m_jit);
            callOperation(operationThrowStackOverflowForVarargs, JITCompiler::LinkableConstant(m_jit, globalObject));
            m_jit.exceptionCheck();
            m_jit.abortWithReason(DFGVarargsThrowingPathDidNotThrow);
            done.link(&m_jit);
        } else {
            GPRReg argumentsPayloadGPR;
            GPRReg argumentsTagGPR;
            GPRReg scratchGPR1;
            GPRReg scratchGPR2;
            GPRReg scratchGPR3;
        
            auto loadArgumentsGPR = [&] (GPRReg reservedGPR) {
                if (reservedGPR != InvalidGPRReg)
                    lock(reservedGPR);
                JSValueOperand arguments(this, node->child3());
                argumentsTagGPR = arguments.tagGPR();
                argumentsPayloadGPR = arguments.payloadGPR();
                if (reservedGPR != InvalidGPRReg)
                    unlock(reservedGPR);
                flushRegisters();
                
                scratchGPR1 = JITCompiler::selectScratchGPR(argumentsPayloadGPR, argumentsTagGPR, reservedGPR);
                scratchGPR2 = JITCompiler::selectScratchGPR(argumentsPayloadGPR, argumentsTagGPR, scratchGPR1, reservedGPR);
                scratchGPR3 = JITCompiler::selectScratchGPR(argumentsPayloadGPR, argumentsTagGPR, scratchGPR1, scratchGPR2, reservedGPR);
            };
            
            loadArgumentsGPR(InvalidGPRReg);
        
            DFG_ASSERT(m_graph, node, isFlushed());

            // Right now, arguments is in argumentsTagGPR/argumentsPayloadGPR and the register file is
            // flushed.
            callOperation(operationSizeFrameForVarargs, GPRInfo::returnValueGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), JSValueRegs(argumentsTagGPR, argumentsPayloadGPR), numUsedStackSlots, data->firstVarArgOffset);
            m_jit.exceptionCheck();
            
            // Now we have the argument count of the callee frame, but we've lost the arguments operand.
            // Reconstruct the arguments operand while preserving the callee frame.
            loadArgumentsGPR(GPRInfo::returnValueGPR);
            m_jit.move(TrustedImm32(numUsedStackSlots), scratchGPR1);
            emitSetVarargsFrame(m_jit, GPRInfo::returnValueGPR, false, scratchGPR1, scratchGPR1);
            m_jit.addPtr(TrustedImm32(-(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf(stackAlignmentBytes(), 6 * sizeof(void*)))), scratchGPR1, JITCompiler::stackPointerRegister);
            
            callOperation(operationSetupVarargsFrame, GPRInfo::returnValueGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), scratchGPR1, JSValueRegs(argumentsTagGPR, argumentsPayloadGPR), data->firstVarArgOffset, GPRInfo::returnValueGPR);
            m_jit.exceptionCheck();
            m_jit.addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::returnValueGPR, JITCompiler::stackPointerRegister);
        }
        
        DFG_ASSERT(m_graph, node, isFlushed());
        
        // We don't need the arguments array anymore.
        if (isVarargs)
            use(node->child3());

        // Now set up the "this" argument.
        JSValueOperand thisArgument(this, node->child2());
        JSValueRegs thisArgumentRegs = thisArgument.jsValueRegs();
        thisArgument.use();
        
        m_jit.storeValue(thisArgumentRegs, JITCompiler::calleeArgumentSlot(0));
    } else {
        // The call instruction's first child is either the function (normal call) or the
        // receiver (method call). subsequent children are the arguments.
        numPassedArgs = node->op() == CallDirectEval ? node->numChildren() - 3 : node->numChildren() - 1;
        numAllocatedArgs = numPassedArgs;
        
        if (functionExecutable) {
            // Allocate more args if this would let us avoid arity checks. This is throttled by
            // CallLinkInfo's limit. It's probably good to throttle it - if the callee wants a
            // ginormous amount of argument space then it's better for them to do it so that when we
            // make calls to other things, we don't waste space.
            unsigned desiredNumAllocatedArgs = static_cast<unsigned>(functionExecutable->parameterCount()) + 1;
            if (desiredNumAllocatedArgs <= Options::maximumDirectCallStackSize()) {
                numAllocatedArgs = std::max(numAllocatedArgs, desiredNumAllocatedArgs);
                
                // Whoever converts to DirectCall should do this adjustment. It's too late for us to
                // do this adjustment now since we will have already emitted code that relied on the
                // value of m_parameterSlots.
                DFG_ASSERT(
                    m_graph, node,
                    Graph::parameterSlotsForArgCount(numAllocatedArgs)
                    <= m_graph.m_parameterSlots);
            }
        }

        if (isTail) {
            JSValueOperand callee(this, calleeEdge);
            calleeTagGPR = callee.tagGPR();
            calleePayloadGPR = callee.payloadGPR();
            if (!isDirect)
                use(calleeEdge);

            shuffleData.numLocals = m_graph.frameRegisterCount();
            shuffleData.callee = ValueRecovery::inPair(calleeTagGPR, calleePayloadGPR);
            shuffleData.args.resize(numAllocatedArgs);
            shuffleData.numPassedArgs = numPassedArgs;
            shuffleData.numParameters = m_jit.codeBlock()->numParameters();

            for (unsigned i = 0; i < numPassedArgs; ++i) {
                Edge argEdge = m_graph.varArgChild(node, i + 1);
                GenerationInfo& info = generationInfo(argEdge.node());
                if (!isDirect)
                    use(argEdge);
                shuffleData.args[i] = info.recovery(argEdge->virtualRegister());
            }
            
            for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                shuffleData.args[i] = ValueRecovery::constant(jsUndefined());

            shuffleData.setupCalleeSaveRegisters(&RegisterAtOffsetList::dfgCalleeSaveRegisters());
        } else {
            m_jit.store32(MacroAssembler::TrustedImm32(numPassedArgs), m_jit.calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis));
        
            for (unsigned i = 0; i < numPassedArgs; i++) {
                Edge argEdge = m_graph.m_varArgChildren[node->firstChild() + 1 + i];
                JSValueOperand arg(this, argEdge);
                JSValueRegs argRegs = arg.jsValueRegs();
                use(argEdge);

                m_jit.storeValue(argRegs, m_jit.calleeArgumentSlot(i));
            }
            
            for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                m_jit.storeTrustedValue(jsUndefined(), JITCompiler::calleeArgumentSlot(i));
        }
    }

    GPRReg evalScopeGPR = InvalidGPRReg;
    JSValueRegs evalThisValueJSR;
    if (!isTail || isVarargs || isForwardVarargs) {
        JSValueOperand callee(this, calleeEdge);
        calleeTagGPR = callee.tagGPR();
        calleePayloadGPR = callee.payloadGPR();
        JSValueRegs calleRegs = callee.jsValueRegs();

        m_jit.storeValue(calleRegs, m_jit.calleeFrameSlot(CallFrameSlot::callee));

        callee.use();

        std::optional<SpeculateCellOperand> scope;
        std::optional<JSValueOperand> thisValue;
        if (node->op() == CallDirectEval) {
            Edge scopeEdge = m_graph.m_varArgChildren[node->firstChild() + node->numChildren() - 1];
            Edge thisValueEdge = m_graph.m_varArgChildren[node->firstChild() + node->numChildren() - 2];
            scope.emplace(this, scopeEdge);
            thisValue.emplace(this, thisValueEdge);
            evalScopeGPR = scope->gpr();
            evalThisValueJSR = thisValue->jsValueRegs();
        }

        if (scope)
            scope->use();
        if (thisValue)
            thisValue->use();

        if (!isTail)
            flushRegisters();
    }

    JITCompiler::DataLabelPtr targetToCheck;
    JITCompiler::JumpList slowPath;

    CodeOrigin staticOrigin = node->origin.semantic;
    InlineCallFrame* staticInlineCallFrame = staticOrigin.inlineCallFrame();
    ASSERT(!isTail || !staticInlineCallFrame || !staticInlineCallFrame->getCallerSkippingTailCalls());
    ASSERT(!isEmulatedTail || (staticInlineCallFrame && staticInlineCallFrame->getCallerSkippingTailCalls()));
    CodeOrigin dynamicOrigin =
        isEmulatedTail ? *staticInlineCallFrame->getCallerSkippingTailCalls() : staticOrigin;
    CallSiteIndex callSite = m_jit.recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(dynamicOrigin, m_stream.size());
    
    auto* info = m_jit.jitCode()->common.addCallLinkInfo(node->origin.semantic, CallLinkInfo::UseDataIC::No);
    info->setUpCall(callType, calleePayloadGPR);
    
    auto setResultAndResetStack = [&] () {
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        m_jit.setupResults(resultRegs);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        // After the calls are done, we need to reestablish our stack
        // pointer. We rely on this for varargs calls, calls with arity
        // mismatch (the callframe is slided) and tail calls.
        m_jit.addPtr(TrustedImm32(m_graph.stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, JITCompiler::stackPointerRegister);
    };
    
    if (node->op() == CallDirectEval) {
        // We want to call operationCallDirectEvalSloppy/operationCallDirectEvalStrict but we don't want to overwrite the parameter area in
        // which we have created a prototypical eval call frame. This means that we have to
        // subtract stack to make room for the call. Lucky for us, at this point we have the whole
        // register file to ourselves.
        
        GPRReg calleeFrameGPR = JITCompiler::selectScratchGPR(evalScopeGPR, evalThisValueJSR.tagGPR(), evalThisValueJSR.payloadGPR());
        m_jit.emitStoreCallSiteIndex(callSite);
        m_jit.addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), JITCompiler::stackPointerRegister, calleeFrameGPR);
        m_jit.storePtr(GPRInfo::callFrameRegister, JITCompiler::Address(calleeFrameGPR, CallFrame::callerFrameOffset()));
        
        // Now we need to make room for:
        // - The caller frame and PC of a call to operationCallDirectEvalSloppy/operationCallDirectEvalStrict.
        // - Potentially two arguments on the stack.
        unsigned requiredBytes = sizeof(CallerFrameAndPC) + sizeof(CallFrame*) * 2;
        requiredBytes = WTF::roundUpToMultipleOf(stackAlignmentBytes(), requiredBytes);
        m_jit.subPtr(TrustedImm32(requiredBytes), JITCompiler::stackPointerRegister);
        m_jit.setupArguments<decltype(operationCallDirectEvalSloppy)>(calleeFrameGPR, evalScopeGPR, evalThisValueJSR);
        prepareForExternalCall();
        m_jit.appendCall(node->ecmaMode().isStrict() ? operationCallDirectEvalStrict : operationCallDirectEvalSloppy);
        m_jit.exceptionCheck();
        JITCompiler::Jump done = m_jit.branchIfNotEmpty(GPRInfo::returnValueGPR2);
        
        // This is the part where we meant to make a normal call. Oops.
        m_jit.addPtr(TrustedImm32(requiredBytes), JITCompiler::stackPointerRegister);
        m_jit.loadValue(JITCompiler::calleeFrameSlot(CallFrameSlot::callee), JSValueRegs { GPRInfo::regT1, GPRInfo::regT0 });
        m_jit.loadLinkableConstant(JITCompiler::LinkableConstant(m_jit, globalObject), GPRInfo::regT3);
        m_jit.loadLinkableConstant(JITCompiler::LinkableConstant::nonCellPointer(m_jit, info), GPRInfo::regT2);
        m_jit.emitVirtualCallWithoutMovingGlobalObject(vm(), GPRInfo::regT2, CallMode::Regular);
        ASSERT(info->callMode() == CallMode::Regular);
        
        done.link(&m_jit);
        setResultAndResetStack();
        return;
    }

    if (isDirect) {
        info->setExecutableDuringCompilation(executable);
        info->setMaxArgumentCountIncludingThis(numAllocatedArgs);

        if (isTail) {
            RELEASE_ASSERT(node->op() == DirectTailCall);
            
            JITCompiler::Label mainPath = m_jit.label();
            
            m_jit.emitStoreCallSiteIndex(callSite);
            
            info->emitDirectTailCallFastPath(m_jit, scopedLambda<void()>([&]{
                info->setFrameShuffleData(shuffleData);
                CallFrameShuffler(m_jit, shuffleData).prepareForTailCall();
            }));

            JITCompiler::Label slowPath = m_jit.label();
            silentSpillAllRegisters(InvalidGPRReg);
            callOperation(operationLinkDirectCall, TrustedImmPtr(info), calleePayloadGPR);
            silentFillAllRegisters();
            m_jit.exceptionCheck();
            m_jit.jump().linkTo(mainPath, &m_jit);
            
            useChildren(node);
            
            m_jit.addJSDirectCall(slowPath, info);
            return;
        }
        
        JITCompiler::Label mainPath = m_jit.label();
        
        m_jit.emitStoreCallSiteIndex(callSite);
        
        info->emitDirectFastPath(m_jit);
        JITCompiler::Jump done = m_jit.jump();
        
        JITCompiler::Label slowPath = m_jit.label();
        callOperation(operationLinkDirectCall, TrustedImmPtr(info), calleePayloadGPR);
        m_jit.exceptionCheck();
        m_jit.jump().linkTo(mainPath, &m_jit);
        
        done.link(&m_jit);
        
        setResultAndResetStack();
        
        m_jit.addJSDirectCall(slowPath, info);
        return;
    }
    
    m_jit.emitStoreCallSiteIndex(callSite);
    
    slowPath.append(m_jit.branchIfNotCell(JSValueRegs(calleeTagGPR, calleePayloadGPR)));
    slowPath.append(m_jit.branchPtrWithPatch(MacroAssembler::NotEqual, calleePayloadGPR, targetToCheck));

    CCallHelpers::JumpList slowCases;
    if (isTail) {
        slowCases = info->emitTailCallFastPath(m_jit, calleePayloadGPR, InvalidGPRReg, scopedLambda<void()>([&]{
            if (node->op() == TailCall) {
                info->setFrameShuffleData(shuffleData);
                CallFrameShuffler(m_jit, shuffleData).prepareForTailCall();
            } else {
                m_jit.emitRestoreCalleeSaves();
                m_jit.prepareForTailCallSlow();
            }
        }));
    } else
        slowCases = info->emitFastPath(m_jit, calleePayloadGPR, InvalidGPRReg);

    JITCompiler::Jump done = m_jit.jump();

    slowPath.link(&m_jit);
    slowCases.link(&m_jit);
    auto slowPathStart = m_jit.label();

    if (node->op() == TailCall) {
        CallFrameShuffler callFrameShuffler(m_jit, shuffleData);
        callFrameShuffler.setCalleeJSValueRegs(JSValueRegs(
            GPRInfo::regT1, GPRInfo::regT0));
        callFrameShuffler.prepareForSlowPath();
    } else {
        // Callee payload needs to be in regT0, tag in regT1
        if (calleeTagGPR == GPRInfo::regT0) {
            if (calleePayloadGPR == GPRInfo::regT1)
                m_jit.swap(GPRInfo::regT1, GPRInfo::regT0);
            else {
                m_jit.move(calleeTagGPR, GPRInfo::regT1);
                m_jit.move(calleePayloadGPR, GPRInfo::regT0);
            }
        } else {
            m_jit.move(calleePayloadGPR, GPRInfo::regT0);
            m_jit.move(calleeTagGPR, GPRInfo::regT1);
        }

        if (isTail)
            m_jit.emitRestoreCalleeSaves();
    }

    ASSERT(!m_graph.m_plan.isUnlinked());
    m_jit.loadLinkableConstant(JITCompiler::LinkableConstant(m_jit, globalObject), GPRInfo::regT3);
    info->emitSlowPath(vm(), m_jit);

    done.link(&m_jit);
    auto doneLocation = m_jit.label();

    if (isTail)
        m_jit.abortWithReason(JITDidReturnFromTailCall);
    else
        setResultAndResetStack();

    m_jit.addJSCall(slowPathStart, doneLocation, info);
}

template<bool strict>
GPRReg SpeculativeJIT::fillSpeculateInt32Internal(Edge edge, DataFormat& returnFormat)
{
    AbstractValue& value = m_state.forNode(edge);
    SpeculatedType type = value.m_type;
    ASSERT(edge.useKind() != KnownInt32Use || !(value.m_type & ~SpecInt32Only));

    m_interpreter.filter(value, SpecInt32Only);
    if (value.isClear()) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), 0);
        returnFormat = DataFormatInt32;
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (edge->hasConstant()) {
            ASSERT(edge->isInt32Constant());
            GPRReg gpr = allocate();
            m_jit.move(MacroAssembler::Imm32(edge->asInt32()), gpr);
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            info.fillInt32(m_stream, gpr);
            returnFormat = DataFormatInt32;
            return gpr;
        }

        DataFormat spillFormat = info.spillFormat();

        ASSERT_UNUSED(spillFormat, (spillFormat & DataFormatJS) || spillFormat == DataFormatInt32);

        // If we know this was spilled as an integer we can fill without checking.
        if (type & ~SpecInt32Only)
            speculationCheck(BadType, JSValueSource(JITCompiler::addressFor(virtualRegister)), edge, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)));

        GPRReg gpr = allocate();
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillInt32(m_stream, gpr);
        returnFormat = DataFormatInt32;
        return gpr;
    }

    case DataFormatJSInt32:
    case DataFormatJS: {
        // Check the value is an integer.
        GPRReg tagGPR = info.tagGPR();
        GPRReg payloadGPR = info.payloadGPR();
        m_gprs.lock(tagGPR);
        m_gprs.lock(payloadGPR);
        if (type & ~SpecInt32Only)
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), edge, m_jit.branchIfNotInt32(tagGPR));
        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderInteger);
        info.fillInt32(m_stream, payloadGPR);
        // If !strict we're done, return.
        returnFormat = DataFormatInt32;
        return payloadGPR;
    }

    case DataFormatInt32: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        returnFormat = DataFormatInt32;
        return gpr;
    }

    case DataFormatCell:
    case DataFormatBoolean:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatJSBoolean:
    case DataFormatDouble:
    case DataFormatStorage:
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateInt32(Edge edge, DataFormat& returnFormat)
{
    return fillSpeculateInt32Internal<false>(edge, returnFormat);
}

GPRReg SpeculativeJIT::fillSpeculateInt32Strict(Edge edge)
{
    DataFormat mustBeDataFormatInt32;
    GPRReg result = fillSpeculateInt32Internal<true>(edge, mustBeDataFormatInt32);
    ASSERT(mustBeDataFormatInt32 == DataFormatInt32);
    return result;
}

FPRReg SpeculativeJIT::fillSpeculateDouble(Edge edge)
{
    ASSERT(isDouble(edge.useKind()));
    ASSERT(edge->hasDoubleResult());
    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    if (info.registerFormat() == DataFormatNone) {

        if (edge->hasConstant()) {
            RELEASE_ASSERT(edge->isNumberConstant());
            FPRReg fpr = fprAllocate();
            m_jit.loadDouble(TrustedImmPtr(m_jit.addressOfDoubleConstant(edge.node())), fpr);
            m_fprs.retain(fpr, virtualRegister, SpillOrderConstant);
            info.fillDouble(m_stream, fpr);
            return fpr;
        }
        
        RELEASE_ASSERT(info.spillFormat() == DataFormatDouble);
        FPRReg fpr = fprAllocate();
        m_jit.loadDouble(JITCompiler::addressFor(virtualRegister), fpr);
        m_fprs.retain(fpr, virtualRegister, SpillOrderSpilled);
        info.fillDouble(m_stream, fpr);
        return fpr;
    }

    RELEASE_ASSERT(info.registerFormat() == DataFormatDouble);
    FPRReg fpr = info.fpr();
    m_fprs.lock(fpr);
    return fpr;
}

GPRReg SpeculativeJIT::fillSpeculateCell(Edge edge)
{
    AbstractValue& value = m_state.forNode(edge);
    SpeculatedType type = value.m_type;
    ASSERT((edge.useKind() != KnownCellUse && edge.useKind() != KnownStringUse) || !(value.m_type & ~SpecCell));

    m_interpreter.filter(value, SpecCell);
    if (value.isClear()) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), 0);
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (edge->hasConstant()) {
            GPRReg gpr = allocate();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            m_jit.move(TrustedImmPtr(edge->constant()), gpr);
            info.fillCell(m_stream, gpr);
            return gpr;
        }

        ASSERT((info.spillFormat() & DataFormatJS) || info.spillFormat() == DataFormatCell);
        if (type & ~SpecCell) {
            speculationCheck(
                BadType,
                JSValueSource(JITCompiler::addressFor(virtualRegister)),
                edge,
                m_jit.branchIfNotCell(JITCompiler::tagFor(virtualRegister)));
        }
        GPRReg gpr = allocate();
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillCell(m_stream, gpr);
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
        if (type & ~SpecCell) {
            speculationCheck(
                BadType, JSValueRegs(tagGPR, payloadGPR), edge,
                m_jit.branchIfNotCell(info.jsValueRegs()));
        }
        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderCell);
        info.fillCell(m_stream, payloadGPR);
        return payloadGPR;
    }

    case DataFormatJSInt32:
    case DataFormatInt32:
    case DataFormatJSDouble:
    case DataFormatJSBoolean:
    case DataFormatBoolean:
    case DataFormatDouble:
    case DataFormatStorage:
        RELEASE_ASSERT_NOT_REACHED();

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

GPRReg SpeculativeJIT::fillSpeculateBoolean(Edge edge)
{
    AbstractValue& value = m_state.forNode(edge);
    SpeculatedType type = value.m_type;
    ASSERT(edge.useKind() != KnownBooleanUse || !(value.m_type & ~SpecBoolean));

    m_interpreter.filter(value, SpecBoolean);
    if (value.isClear()) {
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), 0);
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (edge->hasConstant()) {
            JSValue jsValue = edge->asJSValue();
            GPRReg gpr = allocate();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            m_jit.move(MacroAssembler::TrustedImm32(jsValue.asBoolean()), gpr);
            info.fillBoolean(m_stream, gpr);
            return gpr;
        }

        ASSERT((info.spillFormat() & DataFormatJS) || info.spillFormat() == DataFormatBoolean);

        if (type & ~SpecBoolean)
            speculationCheck(BadType, JSValueSource(JITCompiler::addressFor(virtualRegister)), edge, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)));

        GPRReg gpr = allocate();
        m_jit.load32(JITCompiler::payloadFor(virtualRegister), gpr);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        info.fillBoolean(m_stream, gpr);
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
        if (type & ~SpecBoolean)
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), edge, m_jit.branchIfNotBoolean(tagGPR, InvalidGPRReg));

        m_gprs.unlock(tagGPR);
        m_gprs.release(tagGPR);
        m_gprs.release(payloadGPR);
        m_gprs.retain(payloadGPR, virtualRegister, SpillOrderBoolean);
        info.fillBoolean(m_stream, payloadGPR);
        return payloadGPR;
    }

    case DataFormatJSInt32:
    case DataFormatInt32:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatCell:
    case DataFormatDouble:
    case DataFormatStorage:
        RELEASE_ASSERT_NOT_REACHED();

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }
}

void SpeculativeJIT::compileObjectStrictEquality(Edge objectChild, Edge otherChild)
{
    SpeculateCellOperand op1(this, objectChild);
    JSValueOperand op2(this, otherChild);

    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.payloadGPR();

    DFG_TYPE_CHECK(JSValueSource::unboxedCell(op1GPR), objectChild, SpecObject, m_jit.branchIfNotObject(op1GPR));

    GPRTemporary resultPayload(this, Reuse, op1);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    MacroAssembler::Jump op2CellJump = m_jit.branchIfCell(op2.jsValueRegs());
    
    m_jit.move(TrustedImm32(0), resultPayloadGPR);
    MacroAssembler::Jump op2NotCellJump = m_jit.jump();
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because we are doing strict equality.
    op2CellJump.link(&m_jit);
    m_jit.compare32(MacroAssembler::Equal, op1GPR, op2GPR, resultPayloadGPR);
    
    op2NotCellJump.link(&m_jit);
    booleanResult(resultPayloadGPR, m_currentNode);
}
    
void SpeculativeJIT::compilePeepHoleObjectStrictEquality(Edge objectChild, Edge otherChild, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    SpeculateCellOperand op1(this, objectChild);
    JSValueOperand op2(this, otherChild);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.payloadGPR();

    DFG_TYPE_CHECK(JSValueSource::unboxedCell(op1GPR), objectChild, SpecObject, m_jit.branchIfNotObject(op1GPR));

    MacroAssembler::Jump notCell = m_jit.branchIfNotCell(op2.tagGPR());
    addBranch(notCell, notTaken);
    
    if (taken == nextBlock()) {
        branch32(MacroAssembler::NotEqual, op1GPR, op2GPR, notTaken);
        jump(taken);
    } else {
        branch32(MacroAssembler::Equal, op1GPR, op2GPR, taken);
        jump(notTaken);
    }
}

void SpeculativeJIT::compileObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild)
{
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild, ManualOperandSpeculation);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2TagGPR = op2.tagGPR();
    GPRReg op2PayloadGPR = op2.payloadGPR();
    GPRReg resultGPR = result.gpr();

    bool masqueradesAsUndefinedWatchpointValid =
        masqueradesAsUndefinedWatchpointIsStillValid();

    if (masqueradesAsUndefinedWatchpointValid) {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, m_jit.branchIfNotObject(op1GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, m_jit.branchIfNotObject(op1GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild, 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(op1GPR, JSCell::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    MacroAssembler::Jump rightNotCell = m_jit.branchIfNotCell(op2.jsValueRegs());
    
    // We know that within this branch, rightChild must be a cell.
    if (masqueradesAsUndefinedWatchpointValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject, m_jit.branchIfNotObject(op2PayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject, m_jit.branchIfNotObject(op2PayloadGPR));
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, 
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(op2PayloadGPR, JSCell::typeInfoFlagsOffset()), 
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
    if (needsTypeCheck(rightChild, SpecCell | SpecOther)) {
        m_jit.or32(TrustedImm32(1), op2TagGPR, resultGPR);
        
        typeCheck(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, SpecCell | SpecOther,
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
    
    booleanResult(resultGPR, m_currentNode);
}

void SpeculativeJIT::compilePeepHoleObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;
    
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild, ManualOperandSpeculation);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2TagGPR = op2.tagGPR();
    GPRReg op2PayloadGPR = op2.payloadGPR();
    GPRReg resultGPR = result.gpr();

    bool masqueradesAsUndefinedWatchpointValid =
        masqueradesAsUndefinedWatchpointIsStillValid();

    if (masqueradesAsUndefinedWatchpointValid) {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, m_jit.branchIfNotObject(op1GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, m_jit.branchIfNotObject(op1GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild,
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(op1GPR, JSCell::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    MacroAssembler::Jump rightNotCell = m_jit.branchIfNotCell(op2.jsValueRegs());
    
    // We know that within this branch, rightChild must be a cell.
    if (masqueradesAsUndefinedWatchpointValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject,
            m_jit.branchIfNotObject(op2PayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject,
            m_jit.branchIfNotObject(op2PayloadGPR));
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild,
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(op2PayloadGPR, JSCell::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    branch32(MacroAssembler::Equal, op1GPR, op2PayloadGPR, taken);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (!needsTypeCheck(rightChild, SpecCell | SpecOther))
        rightNotCell.link(&m_jit);
    else {
        jump(notTaken, ForceJump);
        
        rightNotCell.link(&m_jit);
        m_jit.or32(TrustedImm32(1), op2TagGPR, resultGPR);
        
        typeCheck(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, SpecCell | SpecOther,
            m_jit.branch32(
                MacroAssembler::NotEqual, resultGPR,
                MacroAssembler::TrustedImm32(JSValue::NullTag)));
    }
    
    jump(notTaken);
}

void SpeculativeJIT::compileSymbolUntypedEquality(Node* node, Edge symbolEdge, Edge untypedEdge)
{
    SpeculateCellOperand symbol(this, symbolEdge);
    JSValueOperand untyped(this, untypedEdge);

    GPRReg symbolGPR = symbol.gpr();
    GPRReg untypedGPR = untyped.payloadGPR();

    speculateSymbol(symbolEdge, symbolGPR);

    GPRTemporary resultPayload(this, Reuse, symbol);
    GPRReg resultPayloadGPR = resultPayload.gpr();

    MacroAssembler::Jump untypedCellJump = m_jit.branchIfCell(untyped.jsValueRegs());

    m_jit.move(TrustedImm32(0), resultPayloadGPR);
    MacroAssembler::Jump untypedNotCellJump = m_jit.jump();

    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because we are doing strict equality.
    untypedCellJump.link(&m_jit);
    m_jit.compare32(MacroAssembler::Equal, symbolGPR, untypedGPR, resultPayloadGPR);

    untypedNotCellJump.link(&m_jit);
    booleanResult(resultPayloadGPR, node);
}

void SpeculativeJIT::compileToBooleanObjectOrOther(Edge nodeUse, bool invert)
{
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRTemporary resultPayload(this);
    GPRReg valueTagGPR = value.tagGPR();
    GPRReg valuePayloadGPR = value.payloadGPR();
    GPRReg resultPayloadGPR = resultPayload.gpr();
    GPRTemporary structure;
    GPRReg structureGPR = InvalidGPRReg;

    bool masqueradesAsUndefinedWatchpointValid =
        masqueradesAsUndefinedWatchpointIsStillValid();

    if (!masqueradesAsUndefinedWatchpointValid) {
        // The masquerades as undefined case will use the structure register, so allocate it here.
        // Do this at the top of the function to avoid branching around a register allocation.
        GPRTemporary realStructure(this);
        structure.adopt(realStructure);
        structureGPR = structure.gpr();
    }

    MacroAssembler::Jump notCell = m_jit.branchIfNotCell(value.jsValueRegs());
    if (masqueradesAsUndefinedWatchpointValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            m_jit.branchIfNotObject(valuePayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            m_jit.branchIfNotObject(valuePayloadGPR));

        MacroAssembler::Jump isNotMasqueradesAsUndefined = 
            m_jit.branchTest8(
                MacroAssembler::Zero, 
                MacroAssembler::Address(valuePayloadGPR, JSCell::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined));

        m_jit.loadPtr(MacroAssembler::Address(valuePayloadGPR, JSCell::structureIDOffset()), structureGPR);
        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, 
            m_jit.branchLinkableConstant(
                MacroAssembler::Equal, 
                MacroAssembler::Address(structureGPR, Structure::globalObjectOffset()), 
                JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(m_currentNode->origin.semantic))));

        isNotMasqueradesAsUndefined.link(&m_jit);
    }
    m_jit.move(invert ? TrustedImm32(0) : TrustedImm32(1), resultPayloadGPR);
    MacroAssembler::Jump done = m_jit.jump();
    
    notCell.link(&m_jit);
 
    static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
    if (needsTypeCheck(nodeUse, SpecCell | SpecOther)) {
        m_jit.or32(TrustedImm32(1), valueTagGPR, resultPayloadGPR);
        typeCheck(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, SpecCell | SpecOther,
            m_jit.branch32(
                MacroAssembler::NotEqual, 
                resultPayloadGPR, 
                TrustedImm32(JSValue::NullTag)));
    }
    m_jit.move(invert ? TrustedImm32(1) : TrustedImm32(0), resultPayloadGPR);
    
    done.link(&m_jit);
    
    booleanResult(resultPayloadGPR, m_currentNode);
}

void SpeculativeJIT::compileToBoolean(Node* node, bool invert)
{
    switch (node->child1().useKind()) {
    case BooleanUse:
    case KnownBooleanUse: {
        SpeculateBooleanOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);

        if (invert)
            m_jit.xor32(TrustedImm32(1), value.gpr(), result.gpr());
        else
            m_jit.move(value.gpr(), result.gpr());

        booleanResult(result.gpr(), node);
        return;
    }
        
    case ObjectOrOtherUse: {
        compileToBooleanObjectOrOther(node->child1(), invert);
        return;
    }
        
    case Int32Use: {
        SpeculateInt32Operand value(this, node->child1());
        GPRTemporary resultPayload(this, Reuse, value);
        m_jit.compare32(invert ? MacroAssembler::Equal : MacroAssembler::NotEqual, value.gpr(), MacroAssembler::TrustedImm32(0), resultPayload.gpr());
        booleanResult(resultPayload.gpr(), node);
        return;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        FPRTemporary scratch(this);
        GPRTemporary resultPayload(this);
        m_jit.move(invert ? TrustedImm32(0) : TrustedImm32(1), resultPayload.gpr());
        MacroAssembler::Jump nonZero = m_jit.branchDoubleNonZero(value.fpr(), scratch.fpr());
        m_jit.move(invert ? TrustedImm32(1) : TrustedImm32(0), resultPayload.gpr());
        nonZero.link(&m_jit);
        booleanResult(resultPayload.gpr(), node);
        return;
    }

    case UntypedUse: {
        JSValueOperand arg1(this, node->child1());
        GPRTemporary result(this);
        GPRTemporary temp(this);
        FPRTemporary valueFPR(this);
        FPRTemporary tempFPR(this);

        GPRReg resultGPR = result.gpr();

        bool shouldCheckMasqueradesAsUndefined = !masqueradesAsUndefinedWatchpointIsStillValid();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        m_jit.emitConvertValueToBoolean(vm(), arg1.jsValueRegs(), resultGPR, temp.gpr(), valueFPR.fpr(), tempFPR.fpr(), shouldCheckMasqueradesAsUndefined, globalObject, invert);
        booleanResult(resultGPR, node);
        return;
    }
    case StringUse:
        compileToBooleanString(node, invert);
        return;

    case StringOrOtherUse:
        compileToBooleanStringOrOther(node, invert);
        return;

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::emitObjectOrOtherBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRTemporary scratch(this);
    GPRReg valueTagGPR = value.tagGPR();
    GPRReg valuePayloadGPR = value.payloadGPR();
    GPRReg scratchGPR = scratch.gpr();
    
    MacroAssembler::Jump notCell = m_jit.branchIfNotCell(value.jsValueRegs());
    if (masqueradesAsUndefinedWatchpointIsStillValid()) {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            m_jit.branchIfNotObject(valuePayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            m_jit.branchIfNotObject(valuePayloadGPR));

        JITCompiler::Jump isNotMasqueradesAsUndefined = m_jit.branchTest8(
            JITCompiler::Zero, 
            MacroAssembler::Address(valuePayloadGPR, JSCell::typeInfoFlagsOffset()), 
            TrustedImm32(MasqueradesAsUndefined));

        m_jit.loadPtr(MacroAssembler::Address(valuePayloadGPR, JSCell::structureIDOffset()), scratchGPR);
        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse,
            m_jit.branchLinkableConstant(
                MacroAssembler::Equal, 
                MacroAssembler::Address(scratchGPR, Structure::globalObjectOffset()), 
                JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(m_currentNode->origin.semantic))));

        isNotMasqueradesAsUndefined.link(&m_jit);
    }
    jump(taken, ForceJump);
    
    notCell.link(&m_jit);
    
    static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
    if (needsTypeCheck(nodeUse, SpecCell | SpecOther)) {
        m_jit.or32(TrustedImm32(1), valueTagGPR, scratchGPR);
        typeCheck(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, SpecCell | SpecOther,
            m_jit.branch32(MacroAssembler::NotEqual, scratchGPR, TrustedImm32(JSValue::NullTag)));
    }

    jump(notTaken);
    
    noResult(m_currentNode);
}

void SpeculativeJIT::emitBranch(Node* node)
{
    BasicBlock* taken = node->branchData()->taken.block;
    BasicBlock* notTaken = node->branchData()->notTaken.block;

    switch (node->child1().useKind()) {
    case BooleanUse:
    case KnownBooleanUse: {
        SpeculateBooleanOperand value(this, node->child1());
        MacroAssembler::ResultCondition condition = MacroAssembler::NonZero;

        if (taken == nextBlock()) {
            condition = MacroAssembler::Zero;
            BasicBlock* tmp = taken;
            taken = notTaken;
            notTaken = tmp;
        }

        branchTest32(condition, value.gpr(), TrustedImm32(1), taken);
        jump(notTaken);

        noResult(node);
        return;
    }
    
    case ObjectOrOtherUse: {
        emitObjectOrOtherBranch(node->child1(), taken, notTaken);
        return;
    }

    case StringUse: {
        emitStringBranch(node->child1(), taken, notTaken);
        return;
    }

    case StringOrOtherUse: {
        emitStringOrOtherBranch(node->child1(), taken, notTaken);
        return;
    }

    case DoubleRepUse:
    case Int32Use: {
        if (node->child1().useKind() == Int32Use) {
            bool invert = false;
            
            if (taken == nextBlock()) {
                invert = true;
                BasicBlock* tmp = taken;
                taken = notTaken;
                notTaken = tmp;
            }

            SpeculateInt32Operand value(this, node->child1());
            branchTest32(invert ? MacroAssembler::Zero : MacroAssembler::NonZero, value.gpr(), taken);
        } else {
            SpeculateDoubleOperand value(this, node->child1());
            FPRTemporary scratch(this);
            branchDoubleNonZero(value.fpr(), scratch.fpr(), taken);
        }
        
        jump(notTaken);
        
        noResult(node);
        return;
    }
    
    case UntypedUse: {
        JSValueOperand value(this, node->child1());
        FPRTemporary valueFPR(this);
        FPRTemporary tempFPR(this);
        GPRTemporary result(this);
        GPRTemporary temp(this);

        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg resultGPR = result.gpr();
    
        use(node->child1());

        bool shouldCheckMasqueradesAsUndefined = !masqueradesAsUndefinedWatchpointIsStillValid();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        auto falsey = m_jit.branchIfFalsey(vm(), valueRegs, resultGPR, temp.gpr(), valueFPR.fpr(), tempFPR.fpr(), shouldCheckMasqueradesAsUndefined, globalObject);
        addBranch(falsey, notTaken);
        jump(taken, ForceJump);

        noResult(node, UseChildrenCalledExplicitly);
        return;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileGetByVal(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat, CanUseFlush>(DataFormat preferredFormat)>& prefix)
{
    switch (node->arrayMode().type()) {
    case Array::AnyTypedArray:
    case Array::ForceExit:
    case Array::SelectUsingArguments:
    case Array::SelectUsingPredictions:
    case Array::Unprofiled:
    case Array::BigInt64Array:
    case Array::BigUint64Array:
        DFG_CRASH(m_graph, node, "Bad array mode type");
        break;
    case Array::Undecided: {
        SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 1));

        GPRReg indexGPR = index.gpr();

        JSValueRegs resultRegs;
        std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

        speculationCheck(OutOfBounds, JSValueRegs(), node,
            m_jit.branch32(MacroAssembler::LessThan, indexGPR, MacroAssembler::TrustedImm32(0)));

        m_jit.move(MacroAssembler::TrustedImm32(JSValue::UndefinedTag), resultRegs.tagGPR());
        m_jit.move(MacroAssembler::TrustedImm32(0), resultRegs.payloadGPR());
        jsValueResult(resultRegs, node);
        break;
    }
    case Array::Generic: {
        if (m_graph.m_slowGetByVal.contains(node)) {
            if (m_graph.varArgChild(node, 0).useKind() == ObjectUse) {
                if (m_graph.varArgChild(node, 1).useKind() == StringUse) {
                    compileGetByValForObjectWithString(node, prefix);
                    return;
                }

                if (m_graph.varArgChild(node, 1).useKind() == SymbolUse) {
                    compileGetByValForObjectWithSymbol(node, prefix);
                    return;
                }
            }

            JSValueOperand base(this, m_graph.varArgChild(node, 0));
            JSValueOperand property(this, m_graph.varArgChild(node, 1));
            JSValueRegs baseGPR = base.jsValueRegs();
            JSValueRegs propertyRegs = property.jsValueRegs();

            JSValueRegs resultRegs;
            CanUseFlush canUseFlush = CanUseFlush::Yes;
            std::tie(resultRegs, std::ignore, canUseFlush) = prefix(DataFormatJS);

            if (canUseFlush == CanUseFlush::No)
                silentSpillAllRegisters(resultRegs);
            else
                flushRegisters();
            callOperation(operationGetByVal, resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseGPR, propertyRegs);
            if (canUseFlush == CanUseFlush::No)
                silentFillAllRegisters();
            m_jit.exceptionCheck();

            jsValueResult(resultRegs, node);
            return;
        }

        JSValueOperand property(this, m_graph.varArgChild(node, 1), ManualOperandSpeculation);
        JSValueRegs propertyRegs = property.jsValueRegs();
        speculate(node, m_graph.varArgChild(node, 1));

        auto generate = [&] (JSValueRegs baseRegs) {
            JSValueRegs resultRegs;
            std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

            CodeOrigin codeOrigin = node->origin.semantic;
            CallSiteIndex callSite = m_jit.recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
            RegisterSet usedRegisters = this->usedRegisters();

            JITCompiler::JumpList slowCases;
            if (!m_state.forNode(m_graph.varArgChild(node, 0)).isType(SpecCell))
                slowCases.append(m_jit.branchIfNotCell(baseRegs));

            auto [ stubInfo, stubInfoConstant ] = m_jit.addStructureStubInfo();
            JITGetByValGenerator gen(
                m_jit.codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::GetByVal, usedRegisters,
                baseRegs, propertyRegs, resultRegs, InvalidGPRReg);

            if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecString))
                gen.stubInfo()->propertyIsString = true;
            else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecInt32Only))
                gen.stubInfo()->propertyIsInt32 = true;
            else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecSymbol))
                gen.stubInfo()->propertyIsSymbol = true;

            gen.generateFastPath(m_jit);

            slowCases.append(gen.slowPathJump());

            std::unique_ptr<SlowPathGenerator> slowPath;
            if (baseRegs.tagGPR() == InvalidGPRReg) {
                slowPath = slowPathCall(
                    slowCases, this, operationGetByValOptimize,
                    resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(codeOrigin)), TrustedImmPtr(gen.stubInfo()), nullptr, CCallHelpers::CellValue(baseRegs.payloadGPR()), propertyRegs);
            } else {
                slowPath = slowPathCall(
                    slowCases, this, operationGetByValOptimize,
                    resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(codeOrigin)), TrustedImmPtr(gen.stubInfo()), nullptr, baseRegs, propertyRegs);
            }

            m_jit.addGetByVal(gen, slowPath.get());
            addSlowPathGenerator(WTFMove(slowPath));

            jsValueResult(resultRegs, node);
        };

        if (isCell(m_graph.varArgChild(node, 0).useKind())) {
            SpeculateCellOperand base(this, m_graph.varArgChild(node, 0), ManualOperandSpeculation);
            speculate(node, m_graph.varArgChild(node, 0));
            generate(JSValueRegs::payloadOnly(base.gpr()));
        } else {
            JSValueOperand base(this, m_graph.varArgChild(node, 0), ManualOperandSpeculation);
            speculate(node, m_graph.varArgChild(node, 0));
            generate(base.jsValueRegs());
        }
        break;
    }
    case Array::Int32:
    case Array::Contiguous: {
        if (node->arrayMode().isInBounds()) {
            SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
            StorageOperand storage(this, m_graph.varArgChild(node, 2));

            GPRReg propertyReg = property.gpr();
            GPRReg storageReg = storage.gpr();

            if (!m_compileOkay)
                return;

            JSValueRegs resultRegs;
            DataFormat format;
            std::tie(resultRegs, format, std::ignore) = prefix(node->arrayMode().type() == Array::Int32 ? DataFormatInt32 : DataFormatJS);

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));

            if (format == DataFormatInt32) {
                ASSERT(!node->arrayMode().isInBoundsSaneChain());

                speculationCheck(
                    OutOfBounds, JSValueRegs(), 0,
                    m_jit.branch32(
                        MacroAssembler::Equal,
                        MacroAssembler::BaseIndex(
                            storageReg, propertyReg, MacroAssembler::TimesEight, TagOffset),
                        TrustedImm32(JSValue::EmptyValueTag)));
                m_jit.load32(
                    MacroAssembler::BaseIndex(
                        storageReg, propertyReg, MacroAssembler::TimesEight, PayloadOffset),
                    resultRegs.payloadGPR());
                strictInt32Result(resultRegs.payloadGPR(), node);
                break;
            }

            ASSERT(format == DataFormatJS);
            m_jit.loadValue(
                MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight),
                resultRegs);
            if (node->arrayMode().isInBoundsSaneChain()) {
                JITCompiler::Jump notHole = m_jit.branchIfNotEmpty(resultRegs.tagGPR());
                m_jit.moveTrustedValue(jsUndefined(), resultRegs);
                notHole.link(&m_jit);
            } else {
                speculationCheck(
                    LoadFromHole, JSValueRegs(), nullptr,
                    m_jit.branchIfEmpty(resultRegs.tagGPR()));
            }
            jsValueResult(resultRegs, node);
            break;
        }

        SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
        SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
        StorageOperand storage(this, m_graph.varArgChild(node, 2));

        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg storageReg = storage.gpr();

        if (!m_compileOkay)
            return;

        JSValueRegs resultRegs;
        std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

        MacroAssembler::JumpList slowCases;

        slowCases.append(m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));

        m_jit.loadValue(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), resultRegs);

        slowCases.append(m_jit.branchIfEmpty(resultRegs.tagGPR()));
        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseReg, propertyReg));

        jsValueResult(resultRegs, node);
        break;
    }
    case Array::Double: {
        if (node->arrayMode().isInBounds()) {
            SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
            StorageOperand storage(this, m_graph.varArgChild(node, 2));

            GPRReg propertyReg = property.gpr();
            GPRReg storageReg = storage.gpr();

            if (!m_compileOkay)
                return;

            FPRTemporary result(this);
            FPRReg resultReg = result.fpr();

            JSValueRegs resultRegs;
            DataFormat format;
            std::tie(resultRegs, format, std::ignore) = prefix(DataFormatDouble);

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));

            m_jit.loadDouble(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), resultReg);
            if (!node->arrayMode().isInBoundsSaneChain())
                speculationCheck(LoadFromHole, JSValueRegs(), nullptr, m_jit.branchIfNaN(resultReg));
            if (format == DataFormatJS) {
                boxDouble(resultReg, resultRegs);
                jsValueResult(resultRegs, node);
            } else {
                ASSERT(format == DataFormatDouble && !resultRegs);
                doubleResult(resultReg, node);
            }
            break;
        }

        SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
        SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
        StorageOperand storage(this, m_graph.varArgChild(node, 2));

        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg storageReg = storage.gpr();

        if (!m_compileOkay)
            return;

        FPRTemporary temp(this);
        FPRReg tempReg = temp.fpr();

        JSValueRegs resultRegs;
        std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

        MacroAssembler::JumpList slowCases;

        slowCases.append(m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));

        m_jit.loadDouble(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), tempReg);
        slowCases.append(m_jit.branchIfNaN(tempReg));
        boxDouble(tempReg, resultRegs);

        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseReg, propertyReg));

        jsValueResult(resultRegs, node);
        break;
    }
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        if (node->arrayMode().isInBounds()) {
            SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
            StorageOperand storage(this, m_graph.varArgChild(node, 2));
            GPRReg propertyReg = property.gpr();
            GPRReg storageReg = storage.gpr();

            if (!m_compileOkay)
                return;

            JSValueRegs resultRegs;
            std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, ArrayStorage::vectorLengthOffset())));

            m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultRegs.tagGPR());
            speculationCheck(LoadFromHole, JSValueRegs(), 0, m_jit.branchIfEmpty(resultRegs.tagGPR()));
            m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultRegs.payloadGPR());

            jsValueResult(resultRegs, node);
            break;
        }

        SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
        SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
        StorageOperand storage(this, m_graph.varArgChild(node, 2));

        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        GPRReg storageReg = storage.gpr();

        if (!m_compileOkay)
            return;

        JSValueRegs resultRegs;
        std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

        JITCompiler::Jump outOfBounds = m_jit.branch32(
            MacroAssembler::AboveOrEqual, propertyReg,
            MacroAssembler::Address(storageReg, ArrayStorage::vectorLengthOffset()));

        m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultRegs.tagGPR());
        JITCompiler::Jump hole = m_jit.branchIfEmpty(resultRegs.tagGPR());
        m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultRegs.payloadGPR());

        JITCompiler::JumpList slowCases;
        slowCases.append(outOfBounds);
        slowCases.append(hole);
        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseReg, propertyReg));

        jsValueResult(resultRegs, node);
        break;
    }
    case Array::String:
        compileGetByValOnString(node, prefix);
        break;
    case Array::DirectArguments:
        compileGetByValOnDirectArguments(node, prefix);
        break;
    case Array::ScopedArguments:
        compileGetByValOnScopedArguments(node, prefix);
        break;
    case Array::Int8Array:
    case Array::Int16Array:
    case Array::Int32Array:
    case Array::Uint8Array:
    case Array::Uint8ClampedArray:
    case Array::Uint16Array:
    case Array::Uint32Array:
    case Array::Float32Array:
    case Array::Float64Array: {
        TypedArrayType type = node->arrayMode().typedArrayType();
        if (isInt(type))
            compileGetByValOnIntTypedArray(node, type, prefix);
        else
            compileGetByValOnFloatTypedArray(node, type, prefix);
    } }
}

void SpeculativeJIT::compile(Node* node)
{
    NodeType op = node->op();

    if constexpr (validateDFGDoesGC) {
        if (Options::validateDoesGC()) {
            bool expectDoesGC = doesGC(m_graph, node);
            DoesGCCheck check;
            check.u.encoded = DoesGCCheck::encode(expectDoesGC, node->index(), node->op());
            m_jit.store32(CCallHelpers::TrustedImm32(check.u.other), &vm().addressOfDoesGC()->u.other);
            m_jit.store32(CCallHelpers::TrustedImm32(check.u.nodeIndex), &vm().addressOfDoesGC()->u.nodeIndex);
        }
    }

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
    m_jit.clearRegisterAllocationOffsets();
#endif

    switch (op) {
    case JSConstant:
    case DoubleConstant:
    case PhantomDirectArguments:
    case PhantomClonedArguments:
        initConstantInfo(node);
        break;

    case LazyJSConstant:
        compileLazyJSConstant(node);
        break;

    case Identity: {
        compileIdentity(node);
        break;
    }

    case Inc:
    case Dec:
        compileIncOrDec(node);
        break;

    case GetLocal: {
        AbstractValue& value = m_state.operand(node->operand());

        // If the CFA is tracking this variable and it found that the variable
        // cannot have been assigned, then don't attempt to proceed.
        if (value.isClear()) {
            m_compileOkay = false;
            break;
        }
        
        switch (node->variableAccessData()->flushFormat()) {
        case FlushedDouble: {
            FPRTemporary result(this);
            m_jit.loadDouble(JITCompiler::addressFor(node->machineLocal()), result.fpr());
            VirtualRegister virtualRegister = node->virtualRegister();
            m_fprs.retain(result.fpr(), virtualRegister, SpillOrderDouble);
            generationInfoFromVirtualRegister(virtualRegister).initDouble(node, node->refCount(), result.fpr());
            break;
        }
        
        case FlushedInt32: {
            GPRTemporary result(this);
            m_jit.load32(JITCompiler::payloadFor(node->machineLocal()), result.gpr());
            
            // Like strictInt32Result, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderInteger);
            generationInfoFromVirtualRegister(virtualRegister).initInt32(node, node->refCount(), result.gpr());
            break;
        }
        
        case FlushedCell: {
            GPRTemporary result(this);
            m_jit.load32(JITCompiler::payloadFor(node->machineLocal()), result.gpr());
            
            // Like cellResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderCell);
            generationInfoFromVirtualRegister(virtualRegister).initCell(node, node->refCount(), result.gpr());
            break;
        }
            
        case FlushedBoolean: {
            GPRTemporary result(this);
            m_jit.load32(JITCompiler::payloadFor(node->machineLocal()), result.gpr());
            
            // Like booleanResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderBoolean);
            generationInfoFromVirtualRegister(virtualRegister).initBoolean(node, node->refCount(), result.gpr());
            break;
        }
            
        case FlushedJSValue: {
            JSValueRegsTemporary result(this);
            m_jit.loadValue(JITCompiler::addressFor(node->machineLocal()), result.regs());

            // Like jsValueResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.regs().payloadGPR(), virtualRegister, SpillOrderJS);
            m_gprs.retain(result.regs().tagGPR(), virtualRegister, SpillOrderJS);
            
            generationInfoFromVirtualRegister(virtualRegister).initJSValue(node, node->refCount(), result.regs().tagGPR(), result.regs().payloadGPR(), DataFormatJS);
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    }

    case MovHint: {
        compileMovHint(m_currentNode);
        noResult(node);
        break;
    }

    case ExitOK: {
        noResult(node);
        break;
    }
        
    case SetLocal: {
        switch (node->variableAccessData()->flushFormat()) {
        case FlushedDouble: {
            SpeculateDoubleOperand value(this, node->child1());
            m_jit.storeDouble(value.fpr(), JITCompiler::addressFor(node->machineLocal()));
            noResult(node);
            // Indicate that it's no longer necessary to retrieve the value of
            // this bytecode variable from registers or other locations in the stack,
            // but that it is stored as a double.
            recordSetLocal(DataFormatDouble);
            break;
        }
            
        case FlushedInt32: {
            SpeculateInt32Operand value(this, node->child1());
            m_jit.store32(value.gpr(), JITCompiler::payloadFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatInt32);
            break;
        }
            
        case FlushedCell: {
            SpeculateCellOperand cell(this, node->child1());
            GPRReg cellGPR = cell.gpr();
            m_jit.storePtr(cellGPR, JITCompiler::payloadFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatCell);
            break;
        }
            
        case FlushedBoolean: {
            SpeculateBooleanOperand value(this, node->child1());
            m_jit.store32(value.gpr(), JITCompiler::payloadFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatBoolean);
            break;
        }
            
        case FlushedJSValue: {
            JSValueOperand value(this, node->child1());
            JSValueRegs valueRegs = value.jsValueRegs();
            m_jit.storeValue(valueRegs, JITCompiler::addressFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(dataFormatFor(node->variableAccessData()->flushFormat()));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case SetArgumentDefinitely:
    case SetArgumentMaybe:
        // This is a no-op; it just marks the fact that the argument is being used.
        // But it may be profitable to use this as a hook to run speculation checks
        // on arguments, thereby allowing us to trivially eliminate such checks if
        // the argument is not used.
        recordSetLocal(dataFormatFor(node->variableAccessData()->flushFormat()));
        break;

    case ValueBitOr:
    case ValueBitAnd:
    case ValueBitXor:
        compileValueBitwiseOp(node);
        break;

    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
        compileBitwiseOp(node);
        break;

    case ValueBitNot:
        compileValueBitNot(node);
        break;

    case ArithBitNot:
        compileBitwiseNot(node);
        break;

    case ValueBitLShift:
        compileValueLShiftOp(node);
        break;

    case ValueBitRShift:
        compileValueBitRShift(node);
        break;

    case ArithBitRShift:
    case ArithBitLShift:
    case BitURShift:
        compileShiftOp(node);
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
        
    case DoubleRep: {
        compileDoubleRep(node);
        break;
    }
        
    case ValueRep: {
        compileValueRep(node);
        break;
    }

    case ValueNegate:
        compileValueNegate(node);
        break;

    case ValueAdd:
        compileValueAdd(node);
        break;

    case ValueSub:
        compileValueSub(node);
        break;

    case StrCat: {
        compileStrCat(node);
        break;
    }

    case ArithAdd:
        compileArithAdd(node);
        break;

    case ArithClz32:
        compileArithClz32(node);
        break;

    case MakeRope:
        compileMakeRope(node);
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

    case ValueMul:
        compileValueMul(node);
        break;
            
    case ValueDiv: {
        compileValueDiv(node);
        break;
    }

    case ArithDiv: {
        compileArithDiv(node);
        break;
    }

    case ValueMod: {
        compileValueMod(node);
        break;
    }

    case ArithMod: {
        compileArithMod(node);
        break;
    }

    case ValuePow: {
        compileValuePow(node);
        break;
    }

    case ArithPow: {
        compileArithPow(node);
        break;
    }

    case ArithAbs:
        compileArithAbs(node);
        break;

    case ArithMin:
    case ArithMax: {
        compileArithMinMax(node);
        break;
    }

    case ArithSqrt:
        compileArithSqrt(node);
        break;

    case ArithFRound:
        compileArithFRound(node);
        break;

    case ArithRandom:
        compileArithRandom(node);
        break;

    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc:
        compileArithRounding(node);
        break;

    case ArithUnary:
        compileArithUnary(node);
        break;

    case ToBoolean: {
        bool invert = false;
        compileToBoolean(node, invert);
        break;
    }

    case LogicalNot: {
        bool invert = true;
        compileToBoolean(node, invert);
        break;
    }

    case CompareLess:
        if (compare(node, JITCompiler::LessThan, JITCompiler::DoubleLessThanAndOrdered, operationCompareLess))
            return;
        break;

    case CompareLessEq:
        if (compare(node, JITCompiler::LessThanOrEqual, JITCompiler::DoubleLessThanOrEqualAndOrdered, operationCompareLessEq))
            return;
        break;

    case CompareGreater:
        if (compare(node, JITCompiler::GreaterThan, JITCompiler::DoubleGreaterThanAndOrdered, operationCompareGreater))
            return;
        break;

    case CompareGreaterEq:
        if (compare(node, JITCompiler::GreaterThanOrEqual, JITCompiler::DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq))
            return;
        break;

    case CompareBelow:
        compileCompareUnsigned(node, JITCompiler::Below);
        break;

    case CompareBelowEq:
        compileCompareUnsigned(node, JITCompiler::BelowOrEqual);
        break;

    case CompareEq:
        if (compare(node, JITCompiler::Equal, JITCompiler::DoubleEqualAndOrdered, operationCompareEq))
            return;
        break;

    case CompareStrictEq:
        if (compileStrictEq(node))
            return;
        break;
        
    case CompareEqPtr:
        compileCompareEqPtr(node);
        break;

    case SameValue:
        compileSameValue(node);
        break;

    case StringCharCodeAt: {
        compileGetCharCodeAt(node);
        break;
    }

    case StringCharAt: {
        // Relies on StringCharAt node having same basic layout as GetByVal
        JSValueRegsTemporary result;
        compileGetByValOnString(node, scopedLambda<std::tuple<JSValueRegs, DataFormat, CanUseFlush>(DataFormat preferredFormat)>([&] (DataFormat preferredFormat) {
            result = JSValueRegsTemporary(this);
            ASSERT(preferredFormat == DataFormatJS || preferredFormat == DataFormatCell);
            return std::tuple { result.regs(), preferredFormat, CanUseFlush::Yes };
        }));
        break;
    }

    case StringFromCharCode: {
        compileFromCharCode(node);
        break;
    }
        
    case CheckDetached: {
        compileCheckDetached(node);
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

    case GetPrivateName: {
        compileGetPrivateName(node);
        break;
    }

    case GetPrivateNameById: {
        compileGetPrivateNameById(node);
        break;
    }

    case GetByVal: {
        JSValueRegsTemporary jsValueResult;
        GPRTemporary oneRegResult;
        compileGetByVal(node, scopedLambda<std::tuple<JSValueRegs, DataFormat, CanUseFlush>(DataFormat preferredFormat)>([&] (DataFormat preferredFormat) {
            JSValueRegs resultRegs;
            switch (preferredFormat) {
            case DataFormatDouble:
                break;
            case DataFormatInt32:
            case DataFormatCell:
                oneRegResult = GPRTemporary(this);
                resultRegs = JSValueRegs::payloadOnly(oneRegResult.gpr());
                break;
            default: {
                ASSERT(preferredFormat == DataFormatJS);
                jsValueResult = JSValueRegsTemporary(this);
                resultRegs = jsValueResult.regs();
                break;
            }
            };
            return std::tuple { resultRegs, preferredFormat, CanUseFlush::Yes };
        }));
        break;
    }
    
    case StringSlice: {
        compileStringSlice(node);
        break;
    }

    case ToLowerCase: {
        compileToLowerCase(node);
        break;
    }

    case NumberToStringWithRadix: {
        compileNumberToStringWithRadix(node);
        break;
    }

    case NumberToStringWithValidRadixConstant: {
        compileNumberToStringWithValidRadixConstant(node);
        break;
    }

    case GetByValWithThis: {
        compileGetByValWithThis(node);
        break;
    }

    case PutPrivateName: {
        compilePutPrivateName(node);
        break;
    }

    case PutPrivateNameById: {
        compilePutPrivateNameById(node);
        break;
    }

    case CheckPrivateBrand: {
        compileCheckPrivateBrand(node);
        break;
    }

    case SetPrivateBrand: {
        compileSetPrivateBrand(node);
        break;
    }

    case PutByValDirect:
    case PutByVal:
    case PutByValAlias: {
        compilePutByVal(node);
        break;
    }

    case PutByValWithThis: {
        static_assert(GPRInfo::numberOfRegisters >= 8, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");

        JSValueOperand base(this, m_graph.varArgChild(node, 0));
        JSValueRegs baseRegs = base.jsValueRegs();

        JSValueOperand thisValue(this, m_graph.varArgChild(node, 1));
        JSValueRegs thisRegs = thisValue.jsValueRegs();

        JSValueOperand property(this, m_graph.varArgChild(node, 2));
        JSValueRegs propertyRegs = property.jsValueRegs();

        JSValueOperand value(this, m_graph.varArgChild(node, 3));
        JSValueRegs valueRegs = value.jsValueRegs();

        flushRegisters();
        callOperation(node->ecmaMode().isStrict() ? operationPutByValWithThisStrict : operationPutByValWithThis,
            JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseRegs, thisRegs, propertyRegs, valueRegs);
        m_jit.exceptionCheck();

        noResult(node);
        break;
    }

    case RegExpExec: {
        compileRegExpExec(node);
        break;
    }

    case RegExpExecNonGlobalOrSticky: {
        compileRegExpExecNonGlobalOrSticky(node);
        break;
    }

    case RegExpMatchFastGlobal: {
        compileRegExpMatchFastGlobal(node);
        break;
    }
        
    case RegExpTest:
    case RegExpTestInline: {
        compileRegExpTest(node);
        break;
    }

    case RegExpMatchFast: {
        compileRegExpMatchFast(node);
        break;
    }

    case StringReplace:
    case StringReplaceRegExp: {
        compileStringReplace(node);
        break;
    }

    case GetRegExpObjectLastIndex: {
        compileGetRegExpObjectLastIndex(node);
        break;
    }
        
    case SetRegExpObjectLastIndex: {
        compileSetRegExpObjectLastIndex(node);
        break;
    }
        
    case RecordRegExpCachedResult: {
        compileRecordRegExpCachedResult(node);
        break;
    }
        
    case ArrayPush: {
        compileArrayPush(node);
        break;
    }
        
    case ArrayPop: {
        ASSERT(node->arrayMode().isJSArray());
        
        SpeculateCellOperand base(this, node->child1());
        StorageOperand storage(this, node->child2());
        GPRTemporary valueTag(this);
        GPRTemporary valuePayload(this);
        
        GPRReg baseGPR = base.gpr();
        GPRReg valueTagGPR = valueTag.gpr();
        GPRReg valuePayloadGPR = valuePayload.gpr();
        GPRReg storageGPR = storage.gpr();
        
        switch (node->arrayMode().type()) {
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
            MacroAssembler::Jump slowCase = m_jit.branchIfEmpty(valueTagGPR);
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
                    JSValueRegs(valueTagGPR, valuePayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseGPR));
            
            jsValueResult(valueTagGPR, valuePayloadGPR, node);
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
            MacroAssembler::Jump slowCase = m_jit.branchIfNaN(tempFPR);
            JSValue nan = JSValue(JSValue::EncodeAsDouble, PNaN);
            m_jit.storeTrustedValue(
                nan,
                MacroAssembler::BaseIndex(storageGPR, valuePayloadGPR, MacroAssembler::TimesEight));
            boxDouble(tempFPR, valueTagGPR, valuePayloadGPR);

            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    MacroAssembler::TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    MacroAssembler::TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPopAndRecoverLength,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseGPR));
            
            jsValueResult(valueTagGPR, valuePayloadGPR, node);
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

            m_jit.loadValue(MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs { valueTagGPR, valuePayloadGPR });
        
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()));

            setUndefinedCases.append(m_jit.branchIfEmpty(valueTagGPR));
        
            m_jit.store32(TrustedImm32(JSValue::EmptyValueTag), MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

            m_jit.sub32(TrustedImm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
            addSlowPathGenerator(
                slowPathMove(
                    setUndefinedCases, this,
                    MacroAssembler::TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    MacroAssembler::TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
        
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPop,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseGPR));

            jsValueResult(valueTagGPR, valuePayloadGPR, node);
            break;
        }
            
        default:
            CRASH();
            break;
        }
        break;
    }

    case ArraySlice: {
        compileArraySlice(node);
        break;
    }

    case ArrayIndexOf: {
        compileArrayIndexOf(node);
        break;
    }

    case DFG::Jump: {
        jump(node->targetBlock());
        noResult(node);
        break;
    }

    case Branch:
        emitBranch(node);
        break;
        
    case Switch:
        emitSwitch(node);
        break;

    case Return: {
        ASSERT(GPRInfo::callFrameRegister != GPRInfo::regT2);
        ASSERT(GPRInfo::regT1 != GPRInfo::returnValueGPR);
        ASSERT(GPRInfo::returnValueGPR != GPRInfo::callFrameRegister);

        // Return the result in returnValueGPR.
        JSValueOperand op1(this, node->child1());
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

        m_jit.emitRestoreCalleeSaves();
        m_jit.emitFunctionEpilogue();
        m_jit.ret();
        
        noResult(node);
        break;
    }
        
    case Throw: {
        compileThrow(node);
        break;
    }

    case ThrowStaticError: {
        compileThrowStaticError(node);
        break;
    }

    case BooleanToNumber: {
        switch (node->child1().useKind()) {
        case BooleanUse: {
            SpeculateBooleanOperand value(this, node->child1());
            GPRTemporary result(this); // FIXME: We could reuse, but on speculation fail would need recovery to restore tag (akin to add).
            
            m_jit.move(value.gpr(), result.gpr());

            strictInt32Result(result.gpr(), node);
            break;
        }
            
        case UntypedUse: {
            JSValueOperand value(this, node->child1());
            
            if (!m_interpreter.needsTypeCheck(node->child1(), SpecBoolInt32 | SpecBoolean)) {
                GPRTemporary result(this);
                
                GPRReg valueGPR = value.payloadGPR();
                GPRReg resultGPR = result.gpr();
                
                m_jit.move(valueGPR, resultGPR);
                strictInt32Result(result.gpr(), node);
                break;
            }
            
            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            
            GPRReg valueTagGPR = value.tagGPR();
            GPRReg valuePayloadGPR = value.payloadGPR();
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();
            
            m_jit.move(valuePayloadGPR, resultPayloadGPR);
            JITCompiler::Jump isBoolean = m_jit.branchIfBoolean(valueTagGPR, InvalidGPRReg);
            m_jit.move(valueTagGPR, resultTagGPR);
            JITCompiler::Jump done = m_jit.jump();
            isBoolean.link(&m_jit);
            m_jit.move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
            done.link(&m_jit);
            
            jsValueResult(resultTagGPR, resultPayloadGPR, node);
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case ToPrimitive: {
        compileToPrimitive(node);
        break;
    }

    case ToPropertyKey: {
        compileToPropertyKey(node);
        break;
    }

    case ToNumber: {
        JSValueOperand argument(this, node->child1());
        GPRTemporary resultTag(this, Reuse, argument, TagWord);
        GPRTemporary resultPayload(this, Reuse, argument, PayloadWord);

        GPRReg argumentPayloadGPR = argument.payloadGPR();
        GPRReg argumentTagGPR = argument.tagGPR();
        JSValueRegs argumentRegs = argument.jsValueRegs();
        JSValueRegs resultRegs(resultTag.gpr(), resultPayload.gpr());

        argument.use();

        // We have several attempts to remove ToNumber. But ToNumber still exists.
        // It means that converting non-numbers to numbers by this ToNumber is not rare.
        // Instead of the slow path generator, we emit callOperation here.
        if (!(m_state.forNode(node->child1()).m_type & SpecBytecodeNumber)) {
            flushRegisters();
            callOperation(operationToNumber, resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), argumentRegs);
            m_jit.exceptionCheck();
        } else {
            MacroAssembler::Jump notNumber;
            {
                GPRTemporary scratch(this);
                notNumber = m_jit.branchIfNotNumber(argument.jsValueRegs(), scratch.gpr());
            }
            m_jit.move(argumentTagGPR, resultRegs.tagGPR());
            m_jit.move(argumentPayloadGPR, resultRegs.payloadGPR());
            MacroAssembler::Jump done = m_jit.jump();

            notNumber.link(&m_jit);
            silentSpillAllRegisters(resultRegs);
            callOperation(operationToNumber, resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), argumentRegs);
            silentFillAllRegisters();
            m_jit.exceptionCheck();

            done.link(&m_jit);
        }

        jsValueResult(resultRegs.tagGPR(), resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
        break;
    }

    case ToNumeric: {
        compileToNumeric(node);
        break;
    }

    case CallNumberConstructor:
        compileCallNumberConstructor(node);
        break;
        
    case ToString:
    case CallStringConstructor:
    case StringValueOf: {
        compileToStringOrCallStringConstructorOrStringValueOf(node);
        break;
    }

    case FunctionToString:
        compileFunctionToString(node);
        break;
        
    case NewStringObject: {
        compileNewStringObject(node);
        break;
    }

    case NewSymbol: {
        compileNewSymbol(node);
        break;
    }
        
    case NewArray: {
        compileNewArray(node);
        break;
    }

    case NewArrayWithSpread: {
        compileNewArrayWithSpread(node);
        break;
    }

    case Spread: {
        compileSpread(node);
        break;
    }

    case NewArrayWithSize: {
        compileNewArrayWithSize(node);
        break;
    }
        
    case NewArrayBuffer: {
        compileNewArrayBuffer(node);
        break;
    }
        
    case NewTypedArray: {
        compileNewTypedArray(node);
        break;
    }
        
    case NewRegexp: {
        compileNewRegexp(node);
        break;
    }

    case ToObject:
    case CallObjectConstructor: {
        compileToObjectOrCallObjectConstructor(node);
        break;
    }
        
    case ToThis: {
        compileToThis(node);
        break;
    }

    case ObjectAssign: {
        compileObjectAssign(node);
        break;
    }

    case ObjectCreate: {
        compileObjectCreate(node);
        break;
    }

    case ObjectKeys:
    case ObjectGetOwnPropertyNames: {
        compileObjectKeysOrObjectGetOwnPropertyNames(node);
        break;
    }

    case CreateThis: {
        compileCreateThis(node);
        break;
    }

    case CreatePromise: {
        compileCreatePromise(node);
        break;
    }

    case CreateGenerator: {
        compileCreateGenerator(node);
        break;
    }

    case CreateAsyncGenerator: {
        compileCreateAsyncGenerator(node);
        break;
    }

    case NewObject: {
        compileNewObject(node);
        break;
    }

    case NewGenerator: {
        compileNewGenerator(node);
        break;
    }

    case NewAsyncGenerator: {
        compileNewAsyncGenerator(node);
        break;
    }

    case NewInternalFieldObject: {
        compileNewInternalFieldObject(node);
        break;
    }

    case GetCallee: {
        compileGetCallee(node);
        break;
    }

    case SetCallee: {
        compileSetCallee(node);
        break;
    }
        
    case GetArgumentCountIncludingThis: {
        compileGetArgumentCountIncludingThis(node);
        break;
    }

    case SetArgumentCountIncludingThis:
        compileSetArgumentCountIncludingThis(node);
        break;
        
    case GetScope:
        compileGetScope(node);
        break;
            
    case SkipScope:
        compileSkipScope(node);
        break;
        
    case GetGlobalObject:
        compileGetGlobalObject(node);
        break;

    case GetGlobalThis:
        compileGetGlobalThis(node);
        break;
        
    case GetClosureVar: {
        compileGetClosureVar(node);
        break;
    }
    
    case PutClosureVar: {
        compilePutClosureVar(node);
        break;
    }

    case GetInternalField: {
        compileGetInternalField(node);
        break;
    }

    case PutInternalField: {
        compilePutInternalField(node);
        break;
    }

    case TryGetById: {
        compileGetById(node, AccessType::TryGetById);
        break;
    }

    case GetByIdDirect: {
        compileGetById(node, AccessType::GetByIdDirect);
        break;
    }

    case GetByIdDirectFlush: {
        compileGetByIdFlush(node, AccessType::GetByIdDirect);
        break;
    }

    case GetById: {
        compileGetById(node, AccessType::GetById);
        break;
    }

    case GetByIdFlush: {
        compileGetByIdFlush(node, AccessType::GetById);
        break;
    }

    case GetByIdWithThis: {
        if (node->child1().useKind() == CellUse && node->child2().useKind() == CellUse) {
            SpeculateCellOperand base(this, node->child1());
            SpeculateCellOperand thisValue(this, node->child2());
            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            
            GPRReg baseGPR = base.gpr();
            GPRReg thisGPR = thisValue.gpr();
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();
            
            cachedGetByIdWithThis(node->origin.semantic, InvalidGPRReg, baseGPR, InvalidGPRReg, thisGPR, resultTagGPR, resultPayloadGPR, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier());
            
            jsValueResult(resultTagGPR, resultPayloadGPR, node);
        } else {
            JSValueOperand base(this, node->child1());
            JSValueOperand thisValue(this, node->child2());
            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            
            GPRReg baseTagGPR = base.tagGPR();
            GPRReg basePayloadGPR = base.payloadGPR();
            GPRReg thisTagGPR = thisValue.tagGPR();
            GPRReg thisPayloadGPR = thisValue.payloadGPR();
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();
            
            JITCompiler::JumpList notCellList;
            notCellList.append(m_jit.branchIfNotCell(base.jsValueRegs()));
            notCellList.append(m_jit.branchIfNotCell(thisValue.jsValueRegs()));
            
            cachedGetByIdWithThis(node->origin.semantic, baseTagGPR, basePayloadGPR, thisTagGPR, thisPayloadGPR, resultTagGPR, resultPayloadGPR, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), notCellList);
            
            jsValueResult(resultTagGPR, resultPayloadGPR, node);
        }
        
        break;
    }

    case GetArrayLength:
        compileGetArrayLength(node);
        break;

    case GetTypedArrayLengthAsInt52:
        // We do not support typed arrays larger than 2GB on 32-bit platforms.
        RELEASE_ASSERT_NOT_REACHED();
        break;

    case DeleteById: {
        compileDeleteById(node);
        break;
    }

    case DeleteByVal: {
        compileDeleteByVal(node);
        break;
    }
        
    case CheckIsConstant: {
        compileCheckIsConstant(node);
        break;
    }

    case CheckNotEmpty: {
        compileCheckNotEmpty(node);
        break;
    }

    case CheckIdent:
        compileCheckIdent(node);
        break;

    case GetExecutable: {
        compileGetExecutable(node);
        break;
    }
        
    case CheckStructure: {
        compileCheckStructure(node);
        break;
    }
        
    case PutStructure: {
        RegisteredStructure oldStructure = node->transition()->previous;
        RegisteredStructure newStructure = node->transition()->next;
        m_graph.m_plan.transitions().addLazily(node->origin.semantic.codeOriginOwner(), oldStructure.get(), newStructure.get());

        SpeculateCellOperand base(this, node->child1());
        GPRReg baseGPR = base.gpr();
        
        ASSERT_UNUSED(oldStructure, oldStructure->indexingMode() == newStructure->indexingMode());
        ASSERT(oldStructure->typeInfo().type() == newStructure->typeInfo().type());
        ASSERT(oldStructure->typeInfo().inlineTypeFlags() == newStructure->typeInfo().inlineTypeFlags());
        m_jit.storePtr(TrustedImmPtr(newStructure), MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()));
        
        noResult(node);
        break;
    }
        
    case AllocatePropertyStorage:
        compileAllocatePropertyStorage(node);
        break;
        
    case ReallocatePropertyStorage:
        compileReallocatePropertyStorage(node);
        break;
        
    case NukeStructureAndSetButterfly:
        compileNukeStructureAndSetButterfly(node);
        break;
        
    case GetButterfly:
        compileGetButterfly(node);
        break;

    case GetIndexedPropertyStorage: {
        compileGetIndexedPropertyStorage(node);
        break;
    }

    case ResolveRope:
        compileResolveRope(node);
        break;

    case ConstantStoragePointer: {
        compileConstantStoragePointer(node);
        break;
    }
        
    case GetTypedArrayByteOffset: {
        compileGetTypedArrayByteOffset(node);
        break;
    }

    case GetTypedArrayByteOffsetAsInt52: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    case GetPrototypeOf: {
        compileGetPrototypeOf(node);
        break;
    }
        
    case GetByOffset: {
        compileGetByOffset(node);
        break;
    }
        
    case GetGetterSetterByOffset: {
        StorageOperand storage(this, node->child1());
        GPRTemporary resultPayload(this);
        
        GPRReg storageGPR = storage.gpr();
        GPRReg resultPayloadGPR = resultPayload.gpr();
        
        StorageAccessData& storageAccessData = node->storageAccessData();
        
        m_jit.load32(JITCompiler::Address(storageGPR, offsetRelativeToBase(storageAccessData.offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
        
        cellResult(resultPayloadGPR, node);
        break;
    }
        
    case MatchStructure: {
        compileMatchStructure(node);
        break;
    }
        
    case GetGetter: {
        compileGetGetter(node);
        break;
    }
        
    case GetSetter: {
        compileGetSetter(node);
        break;
    }
        
    case PutByOffset: {
        compilePutByOffset(node);
        break;
    }

    case PutByIdFlush: {
        compilePutByIdFlush(node);
        break;
    }

    case PutById: {
        compilePutById(node);
        break;
    }

    case PutByIdDirect: {
        compilePutByIdDirect(node);
        break;
    }

    case PutByIdWithThis: {
        compilePutByIdWithThis(node);
        break;
    }

    case PutGetterById:
    case PutSetterById: {
        compilePutAccessorById(node);
        break;
    }

    case PutGetterSetterById: {
        compilePutGetterSetterById(node);
        break;
    }

    case PutGetterByVal:
    case PutSetterByVal: {
        compilePutAccessorByVal(node);
        break;
    }

    case DefineDataProperty: {
        compileDefineDataProperty(node);
        break;
    }

    case DefineAccessorProperty: {
        compileDefineAccessorProperty(node);
        break;
    }

    case GetGlobalLexicalVariable:
    case GetGlobalVar: {
        compileGetGlobalVariable(node);
        break;
    }

    case PutGlobalVariable: {
        compilePutGlobalVariable(node);
        break;
    }

    case NotifyWrite: {
        compileNotifyWrite(node);
        break;
    }

    case ParseInt: {
        compileParseInt(node);
        break;
    }

    case CheckTypeInfoFlags: {
        compileCheckTypeInfoFlags(node);
        break;
    }

    case OverridesHasInstance: {
        compileOverridesHasInstance(node);
        break;
    }

    case InstanceOf: {
        compileInstanceOf(node);
        break;
    }

    case InstanceOfCustom: {
        compileInstanceOfCustom(node);
        break;
    }

    case IsEmpty: {        
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, TagWord);
        m_jit.comparePtr(JITCompiler::Equal, value.tagGPR(), TrustedImm32(JSValue::EmptyValueTag), result.gpr());
        booleanResult(result.gpr(), node);
        break;
    }

    case TypeOfIsUndefined: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this);
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();

        JITCompiler::Jump isCell = m_jit.branchIfCell(value.jsValueRegs());
        
        m_jit.compare32(JITCompiler::Equal, value.tagGPR(), TrustedImm32(JSValue::UndefinedTag), result.gpr());
        JITCompiler::Jump done = m_jit.jump();
        
        isCell.link(&m_jit);
        JITCompiler::Jump notMasqueradesAsUndefined;
        if (masqueradesAsUndefinedWatchpointIsStillValid()) {
            m_jit.move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = m_jit.jump();
        } else {
            JITCompiler::Jump isMasqueradesAsUndefined = m_jit.branchTest8(
                JITCompiler::NonZero, 
                JITCompiler::Address(value.payloadGPR(), JSCell::typeInfoFlagsOffset()), 
                TrustedImm32(MasqueradesAsUndefined));
            m_jit.move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = m_jit.jump();
            
            isMasqueradesAsUndefined.link(&m_jit);
            m_jit.loadLinkableConstant(JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), localGlobalObjectGPR);
            m_jit.loadPtr(JITCompiler::Address(value.payloadGPR(), JSCell::structureIDOffset()), result.gpr());
            m_jit.loadPtr(JITCompiler::Address(result.gpr(), Structure::globalObjectOffset()), remoteGlobalObjectGPR); 
            m_jit.compare32(JITCompiler::Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, result.gpr());
        }

        notMasqueradesAsUndefined.link(&m_jit);
        done.link(&m_jit);
        booleanResult(result.gpr(), node);
        break;
    }

    case TypeOfIsObject: {
        compileTypeOfIsObject(node);
        break;
    }

    case TypeOfIsFunction: {
        compileIsCallable(node, operationTypeOfIsFunction);
        break;
    }

    case IsUndefinedOrNull: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, TagWord);

        GPRReg valueTagGPR = value.tagGPR();
        GPRReg resultGPR = result.gpr();

        m_jit.move(valueTagGPR, resultGPR);
        static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1));
        m_jit.or32(CCallHelpers::TrustedImm32(1), resultGPR);
        m_jit.compare32(CCallHelpers::Equal, resultGPR, CCallHelpers::TrustedImm32(JSValue::NullTag), resultGPR);

        booleanResult(resultGPR, node);
        break;
    }


    case IsBoolean: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, TagWord);
        
        m_jit.compare32(JITCompiler::Equal, value.tagGPR(), JITCompiler::TrustedImm32(JSValue::BooleanTag), result.gpr());
        booleanResult(result.gpr(), node);
        break;
    }

    case IsNumber: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, TagWord);
        
        m_jit.add32(TrustedImm32(1), value.tagGPR(), result.gpr());
        m_jit.compare32(JITCompiler::Below, result.gpr(), JITCompiler::TrustedImm32(JSValue::LowestTag + 1), result.gpr());
        booleanResult(result.gpr(), node);
        break;
    }

    case NumberIsInteger: {
        JSValueOperand input(this, node->child1());
        JSValueRegs inputRegs = input.jsValueRegs();
        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationNumberIsInteger, resultGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), inputRegs);
        booleanResult(resultGPR, node);
        break;
    }

    case IsObject: {
        compileIsObject(node);
        break;
    }

    case IsCallable: {
        compileIsCallable(node, operationObjectIsCallable);
        break;
    }

    case IsConstructor: {
        compileIsConstructor(node);
        break;
    }

    case IsCellWithType: {
        compileIsCellWithType(node);
        break;
    }

    case IsTypedArrayView: {
        compileIsTypedArrayView(node);
        break;
    }

    case TypeOf: {
        compileTypeOf(node);
        break;
    }

    case MapHash: {
        JSValueOperand input(this, node->child1());

        JSValueRegs inputRegs = input.jsValueRegs();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationMapHash, resultGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), inputRegs);
        m_jit.exceptionCheck();
        strictInt32Result(resultGPR, node);
        break;
    }

    case NormalizeMapKey: {
        compileNormalizeMapKey(node);
        break;
    }

    case GetMapBucket: {
        SpeculateCellOperand map(this, node->child1());
        JSValueOperand key(this, node->child2());
        SpeculateInt32Operand hash(this, node->child3());

        GPRReg mapGPR = map.gpr();
        JSValueRegs keyRegs = key.jsValueRegs();
        GPRReg hashGPR = hash.gpr();

        if (node->child1().useKind() == MapObjectUse)
            speculateMapObject(node->child1(), mapGPR);
        else if (node->child1().useKind() == SetObjectUse)
            speculateSetObject(node->child1(), mapGPR);
        else
            RELEASE_ASSERT_NOT_REACHED();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        if (node->child1().useKind() == MapObjectUse)
            callOperation(operationJSMapFindBucket, resultGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), mapGPR, keyRegs, hashGPR);
        else
            callOperation(operationJSSetFindBucket, resultGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), mapGPR, keyRegs, hashGPR);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        break;
    }

    case GetMapBucketHead:
        compileGetMapBucketHead(node);
        break;

    case GetMapBucketNext:
        compileGetMapBucketNext(node);
        break;

    case LoadKeyFromMapBucket:
        compileLoadKeyFromMapBucket(node);
        break;

    case LoadValueFromMapBucket:
        compileLoadValueFromMapBucket(node);
        break;

    case ExtractValueFromWeakMapGet:
        compileExtractValueFromWeakMapGet(node);
        break;

    case SetAdd:
        compileSetAdd(node);
        break;

    case MapSet:
        compileMapSet(node);
        break;

    case WeakMapGet:
        compileWeakMapGet(node);
        break;

    case WeakSetAdd:
        compileWeakSetAdd(node);
        break;

    case WeakMapSet:
        compileWeakMapSet(node);
        break;

    case Flush:
        break;

    case Call:
    case TailCall:
    case TailCallInlinedCaller:
    case Construct:
    case CallVarargs:
    case TailCallVarargs:
    case TailCallVarargsInlinedCaller:
    case ConstructVarargs:
    case CallForwardVarargs:
    case TailCallForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
    case ConstructForwardVarargs:
    case CallDirectEval:
    case DirectCall:
    case DirectConstruct:
    case DirectTailCall:
    case DirectTailCallInlinedCaller:
        emitCall(node);
        break;

    case VarargsLength: {
        compileVarargsLength(node);
        break;
    }

    case LoadVarargs: {
        compileLoadVarargs(node);
        break;
    }
        
    case ForwardVarargs: {
        compileForwardVarargs(node);
        break;
    }
        
    case CreateActivation: {
        compileCreateActivation(node);
        break;
    }

    case PushWithScope: {
        compilePushWithScope(node);
        break;
    }
        
    case CreateDirectArguments: {
        compileCreateDirectArguments(node);
        break;
    }
        
    case GetFromArguments: {
        compileGetFromArguments(node);
        break;
    }
        
    case PutToArguments: {
        compilePutToArguments(node);
        break;
    }

    case GetArgument: {
        compileGetArgument(node);
        break;
    }
        
    case CreateScopedArguments: {
        compileCreateScopedArguments(node);
        break;
    }
        
    case CreateClonedArguments: {
        compileCreateClonedArguments(node);
        break;
    }

    case CreateArgumentsButterfly: {
        compileCreateArgumentsButterfly(node);
        break;
    }

    case CreateRest: {
        compileCreateRest(node);
        break;
    }

    case GetRestLength: {
        compileGetRestLength(node);
        break;
    }

    case NewFunction:
    case NewGeneratorFunction:
    case NewAsyncFunction:
    case NewAsyncGeneratorFunction:
        compileNewFunction(node);
        break;

    case SetFunctionName:
        compileSetFunctionName(node);
        break;

    case InById:
        compileInById(node);
        break;

    case InByVal:
        compileInByVal(node);
        break;

    case HasPrivateName:
        compileHasPrivateName(node);
        break;

    case HasPrivateBrand:
        compileHasPrivateBrand(node);
        break;

    case HasOwnProperty: {
        SpeculateCellOperand object(this, node->child1());
        GPRTemporary uniquedStringImpl(this);
        GPRTemporary temp(this);
        GPRTemporary hash(this);
        GPRTemporary structureID(this);
        GPRTemporary result(this);

        std::optional<SpeculateCellOperand> keyAsCell;
        std::optional<JSValueOperand> keyAsValue;
        JSValueRegs keyRegs;
        if (node->child2().useKind() == UntypedUse) {
            keyAsValue.emplace(this, node->child2());
            keyRegs = keyAsValue->jsValueRegs();
        } else {
            ASSERT(node->child2().useKind() == StringUse || node->child2().useKind() == SymbolUse);
            keyAsCell.emplace(this, node->child2());
            keyRegs = JSValueRegs::payloadOnly(keyAsCell->gpr());
        }

        GPRReg objectGPR = object.gpr();
        GPRReg implGPR = uniquedStringImpl.gpr();
        GPRReg tempGPR = temp.gpr();
        GPRReg hashGPR = hash.gpr();
        GPRReg structureIDGPR = structureID.gpr();
        GPRReg resultGPR = result.gpr();

        speculateObject(node->child1());

        MacroAssembler::JumpList slowPath;
        switch (node->child2().useKind()) {
        case SymbolUse: {
            speculateSymbol(node->child2(), keyRegs.payloadGPR());
            m_jit.loadPtr(MacroAssembler::Address(keyRegs.payloadGPR(), Symbol::offsetOfSymbolImpl()), implGPR);
            break;
        }
        case StringUse: {
            speculateString(node->child2(), keyRegs.payloadGPR());
            m_jit.loadPtr(MacroAssembler::Address(keyRegs.payloadGPR(), JSString::offsetOfValue()), implGPR);
            slowPath.append(m_jit.branchIfRopeStringImpl(implGPR));
            slowPath.append(m_jit.branchTest32(
                MacroAssembler::Zero, MacroAssembler::Address(implGPR, StringImpl::flagsOffset()),
                MacroAssembler::TrustedImm32(StringImpl::flagIsAtom())));
            break;
        }
        case UntypedUse: {
            slowPath.append(m_jit.branchIfNotCell(keyRegs));
            auto isNotString = m_jit.branchIfNotString(keyRegs.payloadGPR());
            m_jit.loadPtr(MacroAssembler::Address(keyRegs.payloadGPR(), JSString::offsetOfValue()), implGPR);
            slowPath.append(m_jit.branchIfRopeStringImpl(implGPR));
            slowPath.append(m_jit.branchTest32(
                MacroAssembler::Zero, MacroAssembler::Address(implGPR, StringImpl::flagsOffset()),
                MacroAssembler::TrustedImm32(StringImpl::flagIsAtom())));
            auto hasUniquedImpl = m_jit.jump();

            isNotString.link(&m_jit);
            slowPath.append(m_jit.branchIfNotSymbol(keyRegs.payloadGPR()));
            m_jit.loadPtr(MacroAssembler::Address(keyRegs.payloadGPR(), Symbol::offsetOfSymbolImpl()), implGPR);

            hasUniquedImpl.link(&m_jit);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        // Note that we don't test if the hash is zero here. AtomStringImpl's can't have a zero
        // hash, however, a SymbolImpl may. But, because this is a cache, we don't care. We only
        // ever load the result from the cache if the cache entry matches what we are querying for.
        // So we either get super lucky and use zero for the hash and somehow collide with the entity
        // we're looking for, or we realize we're comparing against another entity, and go to the
        // slow path anyways.
        m_jit.load32(MacroAssembler::Address(implGPR, UniquedStringImpl::flagsOffset()), hashGPR);
        m_jit.urshift32(MacroAssembler::TrustedImm32(StringImpl::s_flagCount), hashGPR);
        m_jit.load32(MacroAssembler::Address(objectGPR, JSCell::structureIDOffset()), structureIDGPR);
        m_jit.add32(structureIDGPR, hashGPR);
        m_jit.and32(TrustedImm32(HasOwnPropertyCache::mask), hashGPR);
        m_jit.mul32(TrustedImm32(sizeof(HasOwnPropertyCache::Entry)), hashGPR, hashGPR);
        ASSERT(vm().hasOwnPropertyCache());
        m_jit.move(TrustedImmPtr(vm().hasOwnPropertyCache()), tempGPR);
        slowPath.append(m_jit.branchPtr(MacroAssembler::NotEqual, 
            MacroAssembler::BaseIndex(tempGPR, hashGPR, MacroAssembler::TimesOne, HasOwnPropertyCache::Entry::offsetOfImpl()), implGPR));
        m_jit.load8(MacroAssembler::BaseIndex(tempGPR, hashGPR, MacroAssembler::TimesOne, HasOwnPropertyCache::Entry::offsetOfResult()), resultGPR);
        m_jit.load32(MacroAssembler::BaseIndex(tempGPR, hashGPR, MacroAssembler::TimesOne, HasOwnPropertyCache::Entry::offsetOfStructureID()), tempGPR);
        slowPath.append(m_jit.branch32(MacroAssembler::NotEqual, tempGPR, structureIDGPR));
        auto done = m_jit.jump();

        slowPath.link(&m_jit);
        silentSpillAllRegisters(resultGPR);
        if (node->child2().useKind() != UntypedUse) {
            m_jit.move(MacroAssembler::TrustedImm32(JSValue::CellTag), tempGPR);
            keyRegs = JSValueRegs(tempGPR, keyRegs.payloadGPR());
        }
        callOperation(operationHasOwnProperty, resultGPR, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), objectGPR, keyRegs);
        silentFillAllRegisters();
        m_jit.exceptionCheck();

        done.link(&m_jit);
        booleanResult(resultGPR, node);
        break;
    }

    case StoreBarrier:
    case FencedStoreBarrier: {
        compileStoreBarrier(node);
        break;
    }

    case HasIndexedProperty: {
        SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 1));
        GPRTemporary result(this, Reuse, index);

        compileHasIndexedProperty(node, operationHasIndexedProperty, scopedLambda<std::tuple<GPRReg, GPRReg>()>([&] { return std::make_pair(index.gpr(), result.gpr()); }));
        unblessedBooleanResult(result.gpr(), node);
        break;
    }

    case GetPropertyEnumerator: {
        compileGetPropertyEnumerator(node);
        break;
    }

    case EnumeratorNextUpdateIndexAndMode: {
        compileEnumeratorNextUpdateIndexAndMode(node);
        break;
    }

    case EnumeratorNextExtractMode: {
        compileEnumeratorNextExtractMode(node);
        break;
    }

    case EnumeratorNextExtractIndex: {
        compileEnumeratorNextExtractIndex(node);
        break;
    }

    case EnumeratorNextUpdatePropertyName: {
        compileEnumeratorNextUpdatePropertyName(node);
        break;
    }

    case EnumeratorGetByVal: {
        compileEnumeratorGetByVal(node);
        break;
    }

    case EnumeratorInByVal: {
        compileEnumeratorInByVal(node);
        break;
    }

    case EnumeratorHasOwnProperty: {
        compileEnumeratorHasOwnProperty(node);
        break;
    }

    case ProfileType: {
        compileProfileType(node);
        break;
    }
    case ProfileControlFlow: {
        GPRTemporary scratch1(this);
        BasicBlockLocation* basicBlockLocation = node->basicBlockLocation();
        basicBlockLocation->emitExecuteCode(m_jit, scratch1.gpr());
        noResult(node);
        break;
    }

    case LogShadowChickenPrologue: {
        compileLogShadowChickenPrologue(node);
        break;
    }

    case LogShadowChickenTail: {
        compileLogShadowChickenTail(node);
        break;
    }

    case ForceOSRExit: {
        terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), 0);
        break;
    }

    case InvalidationPoint:
        compileInvalidationPoint(node);
        break;

    case CheckTraps:
        compileCheckTraps(node);
        break;

    case CountExecution:
        m_jit.add64(TrustedImm32(1), MacroAssembler::AbsoluteAddress(node->executionCounter()->address()));
        break;

    case SuperSamplerBegin:
        m_jit.add32(TrustedImm32(1), MacroAssembler::AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
        break;

    case SuperSamplerEnd:
        m_jit.sub32(TrustedImm32(1), MacroAssembler::AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
        break;

    case Phantom:
    case Check:
    case CheckVarargs:
        DFG_NODE_DO_TO_CHILDREN(m_graph, node, speculate);
        noResult(node);
        break;

    case PhantomLocal:
        // This is a no-op.
        noResult(node);
        break;
    case LoopHint:
        compileLoopHint(node);
        break;
        
    case MaterializeNewObject:
        compileMaterializeNewObject(node);
        break;

    case PutDynamicVar: {
        compilePutDynamicVar(node);
        break;
    }

    case GetDynamicVar: {
        compileGetDynamicVar(node);
        break;
    }

    case ResolveScopeForHoistingFuncDeclInEval: {
        compileResolveScopeForHoistingFuncDeclInEval(node);
        break;
    }

    case ResolveScope: {
        compileResolveScope(node);
        break;
    }

    case CallDOM:
        compileCallDOM(node);
        break;

    case CallDOMGetter:
        compileCallDOMGetter(node);
        break;

    case CheckJSCast:
    case CheckNotJSCast:
        compileCheckJSCast(node);
        break;

    case Unreachable:
        unreachable(node);
        break;

    case ExtractCatchLocal: {
        compileExtractCatchLocal(node);
        break;
    }

    case ClearCatchLocals:
        compileClearCatchLocals(node);
        break;

    case CheckStructureOrEmpty:
        DFG_CRASH(m_graph, node, "CheckStructureOrEmpty only used in 64-bit DFG");
        break;

    case CheckArrayOrEmpty:
        DFG_CRASH(m_graph, node, "CheckArrayOrEmpty only used in 64-bit DFG");
        break;

    case IsBigInt:
        DFG_CRASH(m_graph, node, "IsBigInt is only used when USE_BIGINT32, which can only be true in 64 bit mode");
        break;

    case FilterCallLinkStatus:
    case FilterGetByStatus:
    case FilterPutByStatus:
    case FilterInByStatus:
    case FilterDeleteByStatus:
    case FilterCheckPrivateBrandStatus:
    case FilterSetPrivateBrandStatus:
        m_interpreter.filterICStatus(node);
        noResult(node);
        break;

    case LastNodeType:
    case Phi:
    case Upsilon:
    case ExtractOSREntryLocal:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckTierUpAndOSREnter:
    case Int52Rep:
    case FiatInt52:
    case Int52Constant:
    case AssertInBounds:
    case CheckInBounds:
    case CheckInBoundsInt52:
    case ArithIMul:
    case MultiGetByOffset:
    case MultiPutByOffset:
    case MultiDeleteByOffset:
    case CheckBadValue:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomCreateActivation:
    case PhantomNewInternalFieldObject:
    case PhantomNewRegexp:
    case PutHint:
    case CheckStructureImmediate:
    case MaterializeCreateActivation:
    case MaterializeNewInternalFieldObject:
    case PutStack:
    case KillStack:
    case GetStack:
    case GetMyArgumentByVal:
    case GetMyArgumentByValOutOfBounds:
    case GetVectorLength:
    case PhantomCreateRest:
    case PhantomSpread:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case AtomicsIsLockFree:
    case AtomicsAdd:
    case AtomicsAnd:
    case AtomicsCompareExchange:
    case AtomicsExchange:
    case AtomicsLoad:
    case AtomicsOr:
    case AtomicsStore:
    case AtomicsSub:
    case AtomicsXor:
    case IdentityWithProfile:
    case InitializeEntrypointArguments:
    case EntrySwitch:
    case CPUIntrinsic:
    case AssertNotEmpty:
    case DataViewGetInt:
    case DataViewGetFloat:
    case DataViewSet:
    case DateGetInt32OrNaN:
    case DateGetTime:
    case StringCodePointAt:
        DFG_CRASH(m_graph, node, "unexpected node in DFG backend");
        break;
    }

    if (!m_compileOkay)
        return;
    
    if (node->hasResult() && node->mustGenerate())
        use(node);
}

void SpeculativeJIT::moveTrueTo(GPRReg gpr)
{
    m_jit.move(TrustedImm32(1), gpr);
}

void SpeculativeJIT::moveFalseTo(GPRReg gpr)
{
    m_jit.move(TrustedImm32(0), gpr);
}

void SpeculativeJIT::blessBoolean(GPRReg)
{
}

void SpeculativeJIT::compileArithRandom(Node* node)
{
    flushRegisters();

    FPRResult result(this);
    callOperation(operationRandom, result.fpr(), JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)));
    // operationRandom does not raise any exception.
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileGetByValWithThis(Node* node)
{
    JSValueOperand base(this, node->child1());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueOperand thisValue(this, node->child2());
    JSValueRegs thisValueRegs = thisValue.jsValueRegs();
    JSValueOperand subscript(this, node->child3());
    JSValueRegs subscriptRegs = subscript.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetByValWithThis, resultRegs, JITCompiler::LinkableConstant(m_jit, m_graph.globalObjectFor(node->origin.semantic)), baseRegs, subscriptRegs, thisValueRegs);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

#endif

} } // namespace JSC::DFG

#endif
