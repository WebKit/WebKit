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
#include "ClonedArguments.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGCallArrayAllocatorSlowPathGenerator.h"
#include "DFGDoesGC.h"
#include "DFGOperations.h"
#include "DFGSlowPathGenerator.h"
#include "DirectArguments.h"
#include "GetterSetter.h"
#include "HasOwnPropertyCache.h"
#include "JSLexicalEnvironment.h"
#include "JSMap.h"
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
            move(Imm32(value.tag()), tagGPR);
            move(Imm32(value.payload()), payloadGPR);
            m_gprs.retain(tagGPR, virtualRegister, SpillOrderConstant);
            m_gprs.retain(payloadGPR, virtualRegister, SpillOrderConstant);
            info.fillJSValue(m_stream, tagGPR, payloadGPR, DataFormatJS);
        } else {
            DataFormat spillFormat = info.spillFormat();
            ASSERT(spillFormat != DataFormatNone && spillFormat != DataFormatStorage);
            switch (spillFormat) {
            case DataFormatInt32:
                move(TrustedImm32(JSValue::Int32Tag), tagGPR);
                load32(payloadFor(virtualRegister), payloadGPR);
                spillFormat = DataFormatJSInt32; // This will be used as the new register format.
                break;
            case DataFormatCell:
                move(TrustedImm32(JSValue::CellTag), tagGPR);
                load32(payloadFor(virtualRegister), payloadGPR);
                spillFormat = DataFormatJSCell; // This will be used as the new register format.
                break;
            case DataFormatBoolean:
                move(TrustedImm32(JSValue::BooleanTag), tagGPR);
                load32(payloadFor(virtualRegister), payloadGPR);
                spillFormat = DataFormatJSBoolean; // This will be used as the new register format.
                break;
            default:
                loadValue(addressFor(virtualRegister), JSValueRegs { tagGPR, payloadGPR });
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
            move(gpr, payloadGPR);
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
        move(TrustedImm32(tag), tagGPR);
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

void SpeculativeJIT::cachedGetById(Node* node, CodeOrigin origin, JSValueRegs base, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratchGPR, CacheableIdentifier identifier, Jump slowPathTarget , SpillRegistersMode mode, AccessType type)
{
    cachedGetById(node, origin, base.tagGPR(), base.payloadGPR(), result.tagGPR(), result.payloadGPR(), stubInfoGPR, scratchGPR, identifier, slowPathTarget, mode, type);
}

void SpeculativeJIT::cachedGetById(
    Node* node, CodeOrigin codeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, GPRReg stubInfoGPR,
    GPRReg scratchGPR, CacheableIdentifier identifier, Jump slowPathTarget, SpillRegistersMode spillMode, AccessType type)
{
    UNUSED_PARAM(node);
    UNUSED_PARAM(stubInfoGPR);
    UNUSED_PARAM(scratchGPR);
    // This is a hacky fix for when the register allocator decides to alias the base payload with the result tag. This only happens
    // in the case of GetByIdFlush/GetByIdDirectFlush, which has a relatively expensive register allocation story already so we probably don't need to
    // trip over one move instruction.
    if (basePayloadGPR == resultTagGPR) {
        RELEASE_ASSERT(basePayloadGPR != resultPayloadGPR);
        
        if (baseTagGPROrNone == resultPayloadGPR) {
            swap(basePayloadGPR, baseTagGPROrNone);
            baseTagGPROrNone = resultTagGPR;
        } else
            move(basePayloadGPR, resultPayloadGPR);
        basePayloadGPR = resultPayloadGPR;
    }
    
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    if (spillMode == DontSpill) {
        // We've already flushed registers to the stack, we don't need to spill these.
        usedRegisters.remove(JSValueRegs(baseTagGPROrNone, basePayloadGPR));
        usedRegisters.remove(JSValueRegs(resultTagGPR, resultPayloadGPR));
    }
    
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITGetByIdGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        JSValueRegs(baseTagGPROrNone, basePayloadGPR), JSValueRegs(resultTagGPR, resultPayloadGPR), InvalidGPRReg, type, CacheType::GetByIdSelf);
    
    gen.generateFastPath(*this);
    
    JumpList slowCases;
    if (slowPathTarget.isSet())
        slowCases.append(slowPathTarget);
    slowCases.append(gen.slowPathJump());

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (baseTagGPROrNone == InvalidGPRReg) {
        slowPath = slowPathCall(
            slowCases, this, appropriateGetByIdOptimizeFunction(type),
            JSValueRegs(resultTagGPR, resultPayloadGPR),
            CellValue(basePayloadGPR), TrustedImmPtr(gen.stubInfo()));
    } else {
        slowPath = slowPathCall(
            slowCases, this, appropriateGetByIdOptimizeFunction(type),
            JSValueRegs(resultTagGPR, resultPayloadGPR),
            JSValueRegs(baseTagGPROrNone, basePayloadGPR), TrustedImmPtr(gen.stubInfo()));
    }

    addGetById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::cachedGetByIdWithThis(Node* node,
    CodeOrigin codeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg thisTagGPR, GPRReg thisPayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, GPRReg stubInfoGPR, GPRReg scratchGPR,
    CacheableIdentifier identifier, const JumpList& slowPathTarget)
{
    UNUSED_PARAM(node);
    UNUSED_PARAM(stubInfoGPR);
    UNUSED_PARAM(scratchGPR);
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITGetByIdWithThisGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        JSValueRegs(resultTagGPR, resultPayloadGPR), JSValueRegs(baseTagGPROrNone, basePayloadGPR), JSValueRegs(thisTagGPR, thisPayloadGPR), InvalidGPRReg);
    
    gen.generateFastPath(*this);
    
    JumpList slowCases;
    if (!slowPathTarget.empty())
        slowCases.append(slowPathTarget);
    slowCases.append(gen.slowPathJump());

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (baseTagGPROrNone == InvalidGPRReg && thisTagGPR == InvalidGPRReg) {
        slowPath = slowPathCall(
            slowCases, this, operationGetByIdWithThisOptimize,
            JSValueRegs(resultTagGPR, resultPayloadGPR),
            CellValue(basePayloadGPR), CellValue(thisPayloadGPR), TrustedImmPtr(gen.stubInfo()));
    } else {
        ASSERT(baseTagGPROrNone != InvalidGPRReg);
        ASSERT(thisTagGPR != InvalidGPRReg);
        
        slowPath = slowPathCall(
            slowCases, this, operationGetByIdWithThisOptimize,
            JSValueRegs(resultTagGPR, resultPayloadGPR),
            JSValueRegs(baseTagGPROrNone, basePayloadGPR), JSValueRegs(thisTagGPR, thisPayloadGPR), TrustedImmPtr(gen.stubInfo()));
    }

    addGetByIdWithThis(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompareNullOrUndefined(Edge operand)
{
    JSValueOperand arg(this, operand, ManualOperandSpeculation);
    GPRReg argTagGPR = arg.tagGPR();
    GPRReg argPayloadGPR = arg.payloadGPR();

    GPRTemporary resultPayload(this, Reuse, arg, PayloadWord);
    GPRReg resultPayloadGPR = resultPayload.gpr();

    Jump notCell;
    Jump notMasqueradesAsUndefined;
    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        if (!isKnownCell(operand.node()))
            notCell = branchIfNotCell(arg.jsValueRegs());
        
        move(TrustedImm32(0), resultPayloadGPR);
        notMasqueradesAsUndefined = jump();
    } else {
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();

        if (!isKnownCell(operand.node()))
            notCell = branchIfNotCell(arg.jsValueRegs());
        
        Jump isMasqueradesAsUndefined = branchTest8(
            NonZero,
            Address(argPayloadGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(MasqueradesAsUndefined));
        
        move(TrustedImm32(0), resultPayloadGPR);
        notMasqueradesAsUndefined = jump();

        isMasqueradesAsUndefined.link(this);
        loadLinkableConstant(LinkableConstant::globalObject(*this, m_currentNode), localGlobalObjectGPR);
        loadPtr(Address(argPayloadGPR, JSCell::structureIDOffset()), resultPayloadGPR);
        loadPtr(Address(resultPayloadGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        compare32(Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, resultPayloadGPR);
    }
 
    if (!isKnownCell(operand.node())) {
        Jump done = jump();
        
        notCell.link(this);
        // null or undefined?
        static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
        or32(TrustedImm32(1), argTagGPR, resultPayloadGPR);
        compare32(Equal, resultPayloadGPR, TrustedImm32(JSValue::NullTag), resultPayloadGPR);

        done.link(this);
    }
    
    notMasqueradesAsUndefined.link(this);
 
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

    Jump notCell;

    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        if (!isKnownCell(operand.node()))
            notCell = branchIfNotCell(arg.jsValueRegs());
        
        jump(invert ? taken : notTaken, ForceJump);
    } else {
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();

        if (!isKnownCell(operand.node()))
            notCell = branchIfNotCell(arg.jsValueRegs());
        
        branchTest8(Zero,
            Address(argPayloadGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(MasqueradesAsUndefined),
            invert ? taken : notTaken);
   
        loadLinkableConstant(LinkableConstant::globalObject(*this, m_currentNode), localGlobalObjectGPR);
        loadPtr(Address(argPayloadGPR, JSCell::structureIDOffset()), resultGPR);
        loadPtr(Address(resultGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        branchPtr(Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, invert ? notTaken : taken);
    }
 
    if (!isKnownCell(operand.node())) {
        jump(notTaken, ForceJump);
        
        notCell.link(this);
        // null or undefined?
        static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
        or32(TrustedImm32(1), argTagGPR, resultGPR);
        branch32(invert ? NotEqual : Equal, resultGPR, TrustedImm32(JSValue::NullTag), taken);
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
        branchPtr(Equal, arg1PayloadGPR, arg2PayloadGPR, invert ? notTaken : taken);
        callOperationWithSilentSpill(operationCompareStrictEqCell, resultPayloadGPR, LinkableConstant::globalObject(*this, node), arg1PayloadGPR, arg2PayloadGPR);
        branchTest32(invert ? Zero : NonZero, resultPayloadGPR, taken);
    } else {
        // FIXME: Add fast paths for twoCells, number etc.
        callOperationWithSilentSpill(operationCompareStrictEq, resultPayloadGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);
        branchTest32(invert ? Zero : NonZero, resultPayloadGPR, taken);
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
        Jump notEqualCase = branchPtr(NotEqual, arg1PayloadGPR, arg2PayloadGPR);

        move(TrustedImm32(!invert), resultPayloadGPR);
        Jump done = jump();

        notEqualCase.link(this);

        callOperationWithSilentSpill(operationCompareStrictEqCell, resultPayloadGPR, LinkableConstant::globalObject(*this, node), arg1PayloadGPR, arg2PayloadGPR);
        andPtr(TrustedImm32(1), resultPayloadGPR);

        done.link(this);
    } else {
        // FIXME: Add fast paths.
        callOperationWithSilentSpill(operationCompareStrictEq, resultPayloadGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);
        andPtr(TrustedImm32(1), resultPayloadGPR);
    }

    booleanResult(resultPayloadGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileCompareEqPtr(Node* node)
{
    JSValueOperand operand(this, node->child1());
    GPRTemporary result(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg resultGPR = result.gpr();
    boxBooleanPayload(false, resultGPR);
    JumpList notEqual = branchIfNotEqual(regs, node->cellOperand()->value());
    boxBooleanPayload(true, resultGPR);
    notEqual.link(this);
    blessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::emitCall(Node* node)
{
    CallLinkInfo::CallType callType;
    bool isVarargs = false;
    bool isForwardVarargs = false;
    bool isTail = false;
    bool isEmulatedTail = false;
    bool isDirect = false;
    bool isConstruct = false;
    switch (node->op()) {
    case DFG::Call:
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
        isConstruct = true;
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
        isConstruct = true;
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
        isConstruct = true;
        break;
    case DirectCall:
        callType = CallLinkInfo::DirectCall;
        isDirect = true;
        break;
    case DirectConstruct:
        callType = CallLinkInfo::DirectConstruct;
        isDirect = true;
        isConstruct = true;
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
    UNUSED_VARIABLE(isConstruct);

    Edge calleeEdge = m_graph.child(node, 0);
    GPRReg calleeTagGPR = InvalidGPRReg;
    GPRReg calleePayloadGPR = InvalidGPRReg;
    GPRReg callLinkInfoGPR = InvalidGPRReg;
    GPRReg callTargetGPR = InvalidGPRReg;
    CallFrameShuffleData shuffleData;
    
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
            
            scratchGPR1 = selectScratchGPR();
            scratchGPR2 = selectScratchGPR(scratchGPR1);
            scratchGPR3 = selectScratchGPR(scratchGPR1, scratchGPR2);
            
            move(TrustedImm32(numUsedStackSlots), scratchGPR2);
            JumpList slowCase;
            InlineCallFrame* inlineCallFrame;
            if (node->child3())
                inlineCallFrame = node->child3()->origin.semantic.inlineCallFrame();
            else
                inlineCallFrame = node->origin.semantic.inlineCallFrame();
            // emitSetupVarargsFrameFastCase modifies the stack pointer if it succeeds.
            emitSetupVarargsFrameFastCase(vm(), *this, scratchGPR2, scratchGPR1, scratchGPR2, scratchGPR3, inlineCallFrame, data->firstVarArgOffset, slowCase);
            Jump done = jump();
            slowCase.link(this);
            callOperation(operationThrowStackOverflowForVarargs, LinkableConstant::globalObject(*this, node));
            abortWithReason(DFGVarargsThrowingPathDidNotThrow);
            done.link(this);
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
                
                scratchGPR1 = selectScratchGPR(argumentsPayloadGPR, argumentsTagGPR, reservedGPR);
                scratchGPR2 = selectScratchGPR(argumentsPayloadGPR, argumentsTagGPR, scratchGPR1, reservedGPR);
                scratchGPR3 = selectScratchGPR(argumentsPayloadGPR, argumentsTagGPR, scratchGPR1, scratchGPR2, reservedGPR);
            };
            
            loadArgumentsGPR(InvalidGPRReg);
        
            DFG_ASSERT(m_graph, node, isFlushed());

            // Right now, arguments is in argumentsTagGPR/argumentsPayloadGPR and the register file is
            // flushed.
            callOperation(operationSizeFrameForVarargs, GPRInfo::returnValueGPR, LinkableConstant::globalObject(*this, node), JSValueRegs(argumentsTagGPR, argumentsPayloadGPR), numUsedStackSlots, data->firstVarArgOffset);
            
            // Now we have the argument count of the callee frame, but we've lost the arguments operand.
            // Reconstruct the arguments operand while preserving the callee frame.
            loadArgumentsGPR(GPRInfo::returnValueGPR);
            move(TrustedImm32(numUsedStackSlots), scratchGPR1);
            emitSetVarargsFrame(*this, GPRInfo::returnValueGPR, false, scratchGPR1, scratchGPR1);
            addPtr(TrustedImm32(-(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf<stackAlignmentBytes()>(6 * sizeof(void*)))), scratchGPR1, stackPointerRegister);
            
            callOperation(operationSetupVarargsFrame, GPRInfo::returnValueGPR, LinkableConstant::globalObject(*this, node), scratchGPR1, JSValueRegs(argumentsTagGPR, argumentsPayloadGPR), data->firstVarArgOffset, GPRInfo::returnValueGPR);
            addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::returnValueGPR, stackPointerRegister);
        }
        
        DFG_ASSERT(m_graph, node, isFlushed());
        
        // We don't need the arguments array anymore.
        if (isVarargs)
            use(node->child3());

        // Now set up the "this" argument.
        JSValueOperand thisArgument(this, node->child2());
        JSValueRegs thisArgumentRegs = thisArgument.jsValueRegs();
        thisArgument.use();
        
        storeValue(thisArgumentRegs, calleeArgumentSlot(0));
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
            std::optional<GPRTemporary> calleeDestinationPayload;
            std::optional<GPRTemporary> calleeDestinationTag;
            std::optional<GPRTemporary> callTarget;
            if (!isDirect) {
                calleeDestinationPayload.emplace(this, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
                calleeDestinationTag.emplace(this, BaselineJITRegisters::Call::calleeJSR.tagGPR());
                callTarget.emplace(this, BaselineJITRegisters::Call::callTargetGPR);
                calleeDestinationPayload->gpr();
                calleeDestinationTag->gpr();
                callTargetGPR = callTarget->gpr();
            }

            std::optional<GPRTemporary> callLinkInfoTemp;
            callLinkInfoTemp.emplace(this, BaselineJITRegisters::Call::callLinkInfoGPR);
            callLinkInfoGPR = callLinkInfoTemp->gpr();

            JSValueOperand callee(this, calleeEdge);
            calleeTagGPR = callee.tagGPR();
            calleePayloadGPR = callee.payloadGPR();
            if (!isDirect) {
                move(calleePayloadGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
                move(calleeTagGPR, BaselineJITRegisters::Call::calleeJSR.tagGPR());
                calleePayloadGPR = BaselineJITRegisters::Call::calleeJSR.payloadGPR();
                calleeTagGPR = BaselineJITRegisters::Call::calleeJSR.tagGPR();
            }
            calleeDestinationPayload = std::nullopt;
            calleeDestinationTag = std::nullopt;
            callTarget = std::nullopt;
            callLinkInfoTemp = std::nullopt;

            if (!isDirect)
                use(calleeEdge);

            shuffleData.numLocals = m_graph.frameRegisterCount();
            shuffleData.callee = ValueRecovery::inPair(calleeTagGPR, calleePayloadGPR);
            shuffleData.args.resize(numAllocatedArgs);
            shuffleData.numPassedArgs = numPassedArgs;
            shuffleData.numParameters = codeBlock()->numParameters();

            for (unsigned i = 0; i < numPassedArgs; ++i) {
                Edge argEdge = m_graph.varArgChild(node, i + 1);
                GenerationInfo& info = generationInfo(argEdge.node());
                if (!isDirect)
                    use(argEdge);
                shuffleData.args[i] = info.recovery(argEdge->virtualRegister());
            }
            
            for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                shuffleData.args[i] = ValueRecovery::constant(jsUndefined());

            shuffleData.registers[callLinkInfoGPR] = ValueRecovery::inGPR(callLinkInfoGPR, DataFormatInt32);
            if (callTargetGPR != InvalidGPRReg)
                shuffleData.registers[callTargetGPR] = ValueRecovery::inGPR(callTargetGPR, DataFormatInt32);
            shuffleData.setupCalleeSaveRegisters(&RegisterAtOffsetList::dfgCalleeSaveRegisters());
        } else {
            store32(TrustedImm32(numPassedArgs), calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis));
        
            for (unsigned i = 0; i < numPassedArgs; i++) {
                Edge argEdge = m_graph.m_varArgChildren[node->firstChild() + 1 + i];
                JSValueOperand arg(this, argEdge);
                JSValueRegs argRegs = arg.jsValueRegs();
                use(argEdge);

                storeValue(argRegs, calleeArgumentSlot(i));
            }
            
            for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                storeTrustedValue(jsUndefined(), calleeArgumentSlot(i));
        }
    }

    GPRReg evalScopeGPR = InvalidGPRReg;
    JSValueRegs evalThisValueJSR;
    if (!isTail || isVarargs || isForwardVarargs) {
        std::optional<GPRTemporary> calleeDestinationPayload;
        std::optional<GPRTemporary> calleeDestinationTag;
        std::optional<GPRTemporary> callTarget;
        if (!isDirect) {
            calleeDestinationPayload.emplace(this, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
            calleeDestinationTag.emplace(this, BaselineJITRegisters::Call::calleeJSR.tagGPR());
            callTarget.emplace(this, BaselineJITRegisters::Call::callTargetGPR);
            calleeDestinationPayload->gpr();
            calleeDestinationTag->gpr();
            callTargetGPR = callTarget->gpr();
        }

        std::optional<GPRTemporary> callLinkInfoTemp;
        callLinkInfoTemp.emplace(this, BaselineJITRegisters::Call::callLinkInfoGPR);
        callLinkInfoGPR = callLinkInfoTemp->gpr();

        JSValueOperand callee(this, calleeEdge);
        calleeTagGPR = callee.tagGPR();
        calleePayloadGPR = callee.payloadGPR();
        JSValueRegs calleRegs = callee.jsValueRegs();
        if (!isDirect) {
            move(calleePayloadGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
            move(calleeTagGPR, BaselineJITRegisters::Call::calleeJSR.tagGPR());
            calleePayloadGPR = BaselineJITRegisters::Call::calleeJSR.payloadGPR();
            calleeTagGPR = BaselineJITRegisters::Call::calleeJSR.tagGPR();
        }

        storeValue(calleRegs, calleeFrameSlot(CallFrameSlot::callee));

        calleeDestinationPayload = std::nullopt;
        calleeDestinationTag = std::nullopt;
        callTarget = std::nullopt;
        callLinkInfoTemp = std::nullopt;
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

    CodeOrigin staticOrigin = node->origin.semantic;
    InlineCallFrame* staticInlineCallFrame = staticOrigin.inlineCallFrame();
    ASSERT(!isTail || !staticInlineCallFrame || !staticInlineCallFrame->getCallerSkippingTailCalls());
    ASSERT(!isEmulatedTail || (staticInlineCallFrame && staticInlineCallFrame->getCallerSkippingTailCalls()));
    CodeOrigin dynamicOrigin =
        isEmulatedTail ? *staticInlineCallFrame->getCallerSkippingTailCalls() : staticOrigin;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(dynamicOrigin, m_stream.size());

    CompileTimeCallLinkInfo callLinkInfo;
    LinkableConstant callLinkInfoConstant;
    if (!isDirect) {
        std::tie(callLinkInfo, callLinkInfoConstant) = addCallLinkInfo(m_currentNode->origin.semantic);
        std::visit([&](auto* callLinkInfo) {
            callLinkInfo->setUpCall(callType);
        }, callLinkInfo);
    }

    auto setResultAndResetStack = [&] () {
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        setupResults(resultRegs);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        // After the calls are done, we need to reestablish our stack
        // pointer. We rely on this for varargs calls, calls with arity
        // mismatch (the callframe is slided) and tail calls.
        addPtr(TrustedImm32(m_graph.stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, stackPointerRegister);
    };
    
    if (node->op() == CallDirectEval) {
        // We want to call operationCallDirectEvalSloppy/operationCallDirectEvalStrict but we don't want to overwrite the parameter area in
        // which we have created a prototypical eval call frame. This means that we have to
        // subtract stack to make room for the call. Lucky for us, at this point we have the whole
        // register file to ourselves.
        
        GPRReg calleeFrameGPR = selectScratchGPR(evalScopeGPR, evalThisValueJSR.tagGPR(), evalThisValueJSR.payloadGPR());
        emitStoreCallSiteIndex(callSite);
        addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, calleeFrameGPR);
        storePtr(GPRInfo::callFrameRegister, Address(calleeFrameGPR, CallFrame::callerFrameOffset()));
        
        // Now we need to make room for:
        // - The caller frame and PC of a call to operationCallDirectEvalSloppy/operationCallDirectEvalStrict.
        // - Potentially two arguments on the stack.
        unsigned requiredBytes = sizeof(CallerFrameAndPC) + sizeof(CallFrame*) * 2;
        requiredBytes = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(requiredBytes);
        subPtr(TrustedImm32(requiredBytes), stackPointerRegister);
        setupArguments<decltype(operationCallDirectEvalSloppy)>(calleeFrameGPR, evalScopeGPR, evalThisValueJSR);
        prepareForExternalCall();
        appendCall(selectCallDirectEvalOperation(node->lexicallyScopedFeatures()));
        exceptionCheck();
        Jump done = branchIfNotEmpty(GPRInfo::returnValueGPR2);
        
        // This is the part where we meant to make a normal call. Oops.
        addPtr(TrustedImm32(requiredBytes), stackPointerRegister);
        loadValue(calleeFrameSlot(CallFrameSlot::callee), JSValueRegs { GPRInfo::regT1, GPRInfo::regT0 });
        loadLinkableConstant(callLinkInfoConstant, GPRInfo::regT2);
        emitVirtualCallWithoutMovingGlobalObject(vm(), GPRInfo::regT2, CallMode::Regular);
        
        done.link(this);
        setResultAndResetStack();
        return;
    }

    if (isDirect) {
        auto* callLinkInfo = jitCode()->common.m_directCallLinkInfos.add(m_currentNode->origin.semantic, DirectCallLinkInfo::UseDataIC::No, m_graph.m_codeBlock, executable);
        callLinkInfo->setCallType(callType);
        callLinkInfo->setMaxArgumentCountIncludingThis(numAllocatedArgs);

        if (isTail) {
            RELEASE_ASSERT(node->op() == DirectTailCall);

            SuppressRegisetrAllocationValidation suppressScope(*this);
            Label mainPath = label();
            emitStoreCallSiteIndex(callSite);
            auto slowCases = callLinkInfo->emitDirectTailCallFastPath(*this, scopedLambda<void()>([&] {
                CallFrameShuffler shuffler { *this, shuffleData };
                shuffler.prepareForTailCall();
            }));

            Label slowPath = label();
            if (!callLinkInfo->isDataIC() || !slowCases.empty()) {
                slowCases.link(this);

                callOperationWithSilentSpill(operationLinkDirectCall, CCallHelpers::TrustedImmPtr(callLinkInfo), calleePayloadGPR);
                jump().linkTo(mainPath, this);
            }

            useChildren(node);

            addJSDirectCall(slowPath, callLinkInfo);
            return;
        }

        SuppressRegisetrAllocationValidation suppressScope(*this);
        Label mainPath = label();
        emitStoreCallSiteIndex(callSite);
        auto slowCases = callLinkInfo->emitDirectFastPath(*this);
        Label slowPath = label();
        if (!callLinkInfo->isDataIC() || !slowCases.empty()) {
            Jump done = jump();

            slowPath = label();
            slowCases.link(this);

            callOperation(operationLinkDirectCall, CCallHelpers::TrustedImmPtr(callLinkInfo), calleePayloadGPR);
            jump().linkTo(mainPath, this);

            done.link(this);
        }
        setResultAndResetStack();

        addJSDirectCall(slowPath, callLinkInfo);
        return;
    }
    
    emitStoreCallSiteIndex(callSite);
    
    if (isTail) {
        CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, scopedLambda<void()>([&] {
            if (node->op() == TailCall) {
                CallFrameShuffler shuffler { *this, shuffleData };
                shuffler.setCalleeJSValueRegs(BaselineJITRegisters::Call::calleeJSR);
                shuffler.prepareForTailCall();
            } else {
                emitRestoreCalleeSaves();
                RegisterSet preserved;
                preserved.add(callLinkInfoGPR, IgnoreVectors);
                if (callTargetGPR != InvalidGPRReg)
                    preserved.add(callTargetGPR, IgnoreVectors);
                preserved.add(BaselineJITRegisters::Call::calleeJSR.payloadGPR(), IgnoreVectors);
                preserved.add(BaselineJITRegisters::Call::calleeJSR.tagGPR(), IgnoreVectors);
                prepareForTailCallSlow(WTFMove(preserved));
            }
        }));
        abortWithReason(JITDidReturnFromTailCall);
    } else {
        CallLinkInfo::emitFastPath(*this, callLinkInfo);
        setResultAndResetStack();
    }
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
            move(Imm32(edge->asInt32()), gpr);
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            info.fillInt32(m_stream, gpr);
            returnFormat = DataFormatInt32;
            return gpr;
        }

        DataFormat spillFormat = info.spillFormat();

        ASSERT_UNUSED(spillFormat, (spillFormat & DataFormatJS) || spillFormat == DataFormatInt32);

        // If we know this was spilled as an integer we can fill without checking.
        if (type & ~SpecInt32Only)
            speculationCheck(BadType, JSValueSource(addressFor(virtualRegister)), edge, branch32(NotEqual, tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)));

        GPRReg gpr = allocate();
        load32(payloadFor(virtualRegister), gpr);
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
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), edge, branchIfNotInt32(tagGPR));
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
            loadDouble(TrustedImmPtr(addressOfDoubleConstant(edge.node())), fpr);
            m_fprs.retain(fpr, virtualRegister, SpillOrderConstant);
            info.fillDouble(m_stream, fpr);
            return fpr;
        }
        
        RELEASE_ASSERT(info.spillFormat() == DataFormatDouble);
        FPRReg fpr = fprAllocate();
        loadDouble(addressFor(virtualRegister), fpr);
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
            move(TrustedImmPtr(edge->constant()), gpr);
            info.fillCell(m_stream, gpr);
            return gpr;
        }

        ASSERT((info.spillFormat() & DataFormatJS) || info.spillFormat() == DataFormatCell);
        if (type & ~SpecCell) {
            speculationCheck(
                BadType,
                JSValueSource(addressFor(virtualRegister)),
                edge,
                branchIfNotCell(tagFor(virtualRegister)));
        }
        GPRReg gpr = allocate();
        load32(payloadFor(virtualRegister), gpr);
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
                branchIfNotCell(info.jsValueRegs()));
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
            move(TrustedImm32(jsValue.asBoolean()), gpr);
            info.fillBoolean(m_stream, gpr);
            return gpr;
        }

        ASSERT((info.spillFormat() & DataFormatJS) || info.spillFormat() == DataFormatBoolean);

        if (type & ~SpecBoolean)
            speculationCheck(BadType, JSValueSource(addressFor(virtualRegister)), edge, branch32(NotEqual, tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)));

        GPRReg gpr = allocate();
        load32(payloadFor(virtualRegister), gpr);
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
            speculationCheck(BadType, JSValueRegs(tagGPR, payloadGPR), edge, branchIfNotBoolean(tagGPR, InvalidGPRReg));

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

    DFG_TYPE_CHECK(JSValueSource::unboxedCell(op1GPR), objectChild, SpecObject, branchIfNotObject(op1GPR));

    GPRTemporary resultPayload(this, Reuse, op1);
    GPRReg resultPayloadGPR = resultPayload.gpr();
    
    Jump op2CellJump = branchIfCell(op2.jsValueRegs());
    
    move(TrustedImm32(0), resultPayloadGPR);
    Jump op2NotCellJump = jump();
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because we are doing strict equality.
    op2CellJump.link(this);
    compare32(Equal, op1GPR, op2GPR, resultPayloadGPR);
    
    op2NotCellJump.link(this);
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

    DFG_TYPE_CHECK(JSValueSource::unboxedCell(op1GPR), objectChild, SpecObject, branchIfNotObject(op1GPR));

    Jump notCell = branchIfNotCell(op2.tagGPR());
    addBranch(notCell, notTaken);
    
    if (taken == nextBlock()) {
        branch32(NotEqual, op1GPR, op2GPR, notTaken);
        jump(taken);
    } else {
        branch32(Equal, op1GPR, op2GPR, taken);
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

    bool masqueradesAsUndefinedWatchpointSetValid =
        masqueradesAsUndefinedWatchpointSetIsStillValid();

    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, branchIfNotObject(op1GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, branchIfNotObject(op1GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild, 
            branchTest8(
                NonZero,
                Address(op1GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }
    
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    Jump rightNotCell = branchIfNotCell(op2.jsValueRegs());
    
    // We know that within this branch, rightChild must be a cell.
    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject, branchIfNotObject(op2PayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject, branchIfNotObject(op2PayloadGPR));
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, 
            branchTest8(
                NonZero,
                Address(op2PayloadGPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    Jump falseCase = branchPtr(NotEqual, op1GPR, op2PayloadGPR);
    Jump trueCase = jump();
    
    rightNotCell.link(this);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (needsTypeCheck(rightChild, SpecCell | SpecOther)) {
        or32(TrustedImm32(1), op2TagGPR, resultGPR);
        
        typeCheck(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, SpecCell | SpecOther,
            branch32(
                NotEqual, resultGPR,
                TrustedImm32(JSValue::NullTag)));
    }
    
    falseCase.link(this);
    move(TrustedImm32(0), resultGPR);
    Jump done = jump();
    trueCase.link(this);
    move(TrustedImm32(1), resultGPR);
    done.link(this);
    
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

    bool masqueradesAsUndefinedWatchpointSetValid =
        masqueradesAsUndefinedWatchpointSetIsStillValid();

    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, branchIfNotObject(op1GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), leftChild, SpecObject, branchIfNotObject(op1GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), leftChild,
            branchTest8(
                NonZero,
                Address(op1GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // It seems that most of the time when programs do a == b where b may be either null/undefined
    // or an object, b is usually an object. Balance the branches to make that case fast.
    Jump rightNotCell = branchIfNotCell(op2.jsValueRegs());
    
    // We know that within this branch, rightChild must be a cell.
    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject,
            branchIfNotObject(op2PayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, (~SpecCell) | SpecObject,
            branchIfNotObject(op2PayloadGPR));
        speculationCheck(BadType, JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild,
            branchTest8(
                NonZero,
                Address(op2PayloadGPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    branch32(Equal, op1GPR, op2PayloadGPR, taken);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (!needsTypeCheck(rightChild, SpecCell | SpecOther))
        rightNotCell.link(this);
    else {
        jump(notTaken, ForceJump);
        
        rightNotCell.link(this);
        or32(TrustedImm32(1), op2TagGPR, resultGPR);
        
        typeCheck(
            JSValueRegs(op2TagGPR, op2PayloadGPR), rightChild, SpecCell | SpecOther,
            branch32(
                NotEqual, resultGPR,
                TrustedImm32(JSValue::NullTag)));
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

    Jump untypedCellJump = branchIfCell(untyped.jsValueRegs());

    move(TrustedImm32(0), resultPayloadGPR);
    Jump untypedNotCellJump = jump();

    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because we are doing strict equality.
    untypedCellJump.link(this);
    compare32(Equal, symbolGPR, untypedGPR, resultPayloadGPR);

    untypedNotCellJump.link(this);
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

    bool masqueradesAsUndefinedWatchpointSetValid =
        masqueradesAsUndefinedWatchpointSetIsStillValid();

    if (!masqueradesAsUndefinedWatchpointSetValid) {
        // The masquerades as undefined case will use the structure register, so allocate it here.
        // Do this at the top of the function to avoid branching around a register allocation.
        GPRTemporary realStructure(this);
        structure.adopt(realStructure);
        structureGPR = structure.gpr();
    }

    Jump notCell = branchIfNotCell(value.jsValueRegs());
    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            branchIfNotObject(valuePayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            branchIfNotObject(valuePayloadGPR));

        Jump isNotMasqueradesAsUndefined =
            branchTest8(
                Zero,
                Address(valuePayloadGPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined));

        loadPtr(Address(valuePayloadGPR, JSCell::structureIDOffset()), structureGPR);
        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, 
            branchLinkableConstant(
                Equal,
                Address(structureGPR, Structure::globalObjectOffset()),
                LinkableConstant::globalObject(*this, m_currentNode)));

        isNotMasqueradesAsUndefined.link(this);
    }
    move(invert ? TrustedImm32(0) : TrustedImm32(1), resultPayloadGPR);
    Jump done = jump();
    
    notCell.link(this);
 
    static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
    if (needsTypeCheck(nodeUse, SpecCell | SpecOther)) {
        or32(TrustedImm32(1), valueTagGPR, resultPayloadGPR);
        typeCheck(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, SpecCell | SpecOther,
            branch32(
                NotEqual,
                resultPayloadGPR, 
                TrustedImm32(JSValue::NullTag)));
    }
    move(invert ? TrustedImm32(1) : TrustedImm32(0), resultPayloadGPR);
    
    done.link(this);
    
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
            xor32(TrustedImm32(1), value.gpr(), result.gpr());
        else
            move(value.gpr(), result.gpr());

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
        compare32(invert ? Equal : NotEqual, value.gpr(), TrustedImm32(0), resultPayload.gpr());
        booleanResult(resultPayload.gpr(), node);
        return;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        FPRTemporary scratch(this);
        GPRTemporary resultPayload(this);
        move(invert ? TrustedImm32(0) : TrustedImm32(1), resultPayload.gpr());
        Jump nonZero = branchDoubleNonZero(value.fpr(), scratch.fpr());
        move(invert ? TrustedImm32(1) : TrustedImm32(0), resultPayload.gpr());
        nonZero.link(this);
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

        bool shouldCheckMasqueradesAsUndefined = !masqueradesAsUndefinedWatchpointSetIsStillValid();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        emitConvertValueToBoolean(vm(), arg1.jsValueRegs(), resultGPR, temp.gpr(), valueFPR.fpr(), tempFPR.fpr(), shouldCheckMasqueradesAsUndefined, globalObject, invert);
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
    
    Jump notCell = branchIfNotCell(value.jsValueRegs());
    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            branchIfNotObject(valuePayloadGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, (~SpecCell) | SpecObject,
            branchIfNotObject(valuePayloadGPR));

        Jump isNotMasqueradesAsUndefined = branchTest8(
            Zero,
            Address(valuePayloadGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(MasqueradesAsUndefined));

        loadPtr(Address(valuePayloadGPR, JSCell::structureIDOffset()), scratchGPR);
        speculationCheck(BadType, JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse,
            branchLinkableConstant(
                Equal,
                Address(scratchGPR, Structure::globalObjectOffset()),
                LinkableConstant::globalObject(*this, m_currentNode)));

        isNotMasqueradesAsUndefined.link(this);
    }
    jump(taken, ForceJump);
    
    notCell.link(this);
    
    static_assert((JSValue::UndefinedTag | 1) == JSValue::NullTag, "UndefinedTag OR 1 equals to NullTag");
    if (needsTypeCheck(nodeUse, SpecCell | SpecOther)) {
        or32(TrustedImm32(1), valueTagGPR, scratchGPR);
        typeCheck(
            JSValueRegs(valueTagGPR, valuePayloadGPR), nodeUse, SpecCell | SpecOther,
            branch32(NotEqual, scratchGPR, TrustedImm32(JSValue::NullTag)));
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
        ResultCondition condition = NonZero;

        if (taken == nextBlock()) {
            condition = Zero;
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
            branchTest32(invert ? Zero : NonZero, value.gpr(), taken);
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

        bool shouldCheckMasqueradesAsUndefined = !masqueradesAsUndefinedWatchpointSetIsStillValid();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        auto falsey = branchIfFalsey(vm(), valueRegs, resultGPR, temp.gpr(), valueFPR.fpr(), tempFPR.fpr(), shouldCheckMasqueradesAsUndefined, globalObject);
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

void SpeculativeJIT::compileGetByVal(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
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
        constexpr bool needsFlush = false;
        std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);

        speculationCheck(OutOfBounds, JSValueRegs(), node,
            branch32(LessThan, indexGPR, TrustedImm32(0)));

        move(TrustedImm32(JSValue::UndefinedTag), resultRegs.tagGPR());
        move(TrustedImm32(0), resultRegs.payloadGPR());
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

            constexpr bool needsFlush = true;
            auto [resultRegs, dataFormat] = prefix(DataFormatJS, needsFlush);
            callOperation(operationGetByValGeneric, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, propertyRegs);
            exceptionCheck();
            jsValueResult(resultRegs, node);
            return;
        }

        JSValueOperand property(this, m_graph.varArgChild(node, 1), ManualOperandSpeculation);
        JSValueRegs propertyRegs = property.jsValueRegs();
        speculate(node, m_graph.varArgChild(node, 1));

        auto generate = [&] (JSValueRegs baseRegs) {
            constexpr bool needsFlush = true;
            auto [resultRegs, dataFormat] = prefix(DataFormatJS, needsFlush);

            CodeOrigin codeOrigin = node->origin.semantic;
            CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
            RegisterSetBuilder usedRegisters = this->usedRegisters();

            JumpList slowCases;
            if (!m_state.forNode(m_graph.varArgChild(node, 0)).isType(SpecCell))
                slowCases.append(branchIfNotCell(baseRegs));

            auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
            JITGetByValGenerator gen(
                codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::GetByVal, usedRegisters,
                baseRegs, propertyRegs, resultRegs, InvalidGPRReg, InvalidGPRReg);

            if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecString))
                gen.stubInfo()->propertyIsString = true;
            else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecInt32Only))
                gen.stubInfo()->propertyIsInt32 = true;
            else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecSymbol))
                gen.stubInfo()->propertyIsSymbol = true;

            gen.generateFastPath(*this);

            slowCases.append(gen.slowPathJump());

            std::unique_ptr<SlowPathGenerator> slowPath;
            if (baseRegs.tagGPR() == InvalidGPRReg) {
                slowPath = slowPathCall(
                    slowCases, this, operationGetByValOptimize,
                    resultRegs, CellValue(baseRegs.payloadGPR()), propertyRegs, TrustedImmPtr(gen.stubInfo()), nullptr);
            } else {
                slowPath = slowPathCall(
                    slowCases, this, operationGetByValOptimize,
                    resultRegs, baseRegs, propertyRegs, TrustedImmPtr(gen.stubInfo()), nullptr);
            }

            addGetByVal(gen, slowPath.get());
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
            constexpr bool needsFlush = false;
            std::tie(resultRegs, format) = prefix(node->arrayMode().type() == Array::Int32 ? DataFormatInt32 : DataFormatJS, needsFlush);

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

            if (format == DataFormatInt32) {
                ASSERT(!node->arrayMode().isInBoundsSaneChain());

                speculationCheck(
                    OutOfBounds, JSValueRegs(), 0,
                    branch32(
                        Equal,
                        BaseIndex(
                            storageReg, propertyReg, TimesEight, TagOffset),
                        TrustedImm32(JSValue::EmptyValueTag)));
                load32(
                    BaseIndex(
                        storageReg, propertyReg, TimesEight, PayloadOffset),
                    resultRegs.payloadGPR());
                strictInt32Result(resultRegs.payloadGPR(), node);
                break;
            }

            ASSERT(format == DataFormatJS);
            loadValue(
                BaseIndex(storageReg, propertyReg, TimesEight),
                resultRegs);
            if (node->arrayMode().isInBoundsSaneChain()) {
                Jump notHole = branchIfNotEmpty(resultRegs.tagGPR());
                moveTrustedValue(jsUndefined(), resultRegs);
                notHole.link(this);
            } else {
                speculationCheck(
                    LoadFromHole, JSValueRegs(), nullptr,
                    branchIfEmpty(resultRegs.tagGPR()));
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
        constexpr bool needsFlush = false;
        std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);

        JumpList slowCases;

        slowCases.append(branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

        loadValue(BaseIndex(storageReg, propertyReg, TimesEight), resultRegs);

        slowCases.append(branchIfEmpty(resultRegs.tagGPR()));
        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultRegs, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));

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
            constexpr bool needsFlush = false;
            std::tie(resultRegs, format) = prefix(DataFormatDouble, needsFlush);

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

            loadDouble(BaseIndex(storageReg, propertyReg, TimesEight), resultReg);
            if (!node->arrayMode().isInBoundsSaneChain())
                speculationCheck(LoadFromHole, JSValueRegs(), nullptr, branchIfNaN(resultReg));
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
        constexpr bool needsFlush = false;
        std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);

        JumpList slowCases;

        slowCases.append(branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

        loadDouble(BaseIndex(storageReg, propertyReg, TimesEight), tempReg);
        slowCases.append(branchIfNaN(tempReg));
        boxDouble(tempReg, resultRegs);

        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultRegs, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));

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
            constexpr bool needsFlush = false;
            std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, branch32(AboveOrEqual, propertyReg, Address(storageReg, ArrayStorage::vectorLengthOffset())));

            load32(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultRegs.tagGPR());
            speculationCheck(LoadFromHole, JSValueRegs(), 0, branchIfEmpty(resultRegs.tagGPR()));
            load32(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultRegs.payloadGPR());

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
        constexpr bool needsFlush = false;
        std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);

        Jump outOfBounds = branch32(
            AboveOrEqual, propertyReg,
            Address(storageReg, ArrayStorage::vectorLengthOffset()));

        load32(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultRegs.tagGPR());
        Jump hole = branchIfEmpty(resultRegs.tagGPR());
        load32(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultRegs.payloadGPR());

        JumpList slowCases;
        slowCases.append(outOfBounds);
        slowCases.append(hole);
        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultRegs, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));

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
    case Array::Float16Array:
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
            store32(TrustedImm32(check.u.other), &vm().addressOfDoesGC()->u.other);
            store32(TrustedImm32(check.u.nodeIndex), &vm().addressOfDoesGC()->u.nodeIndex);
        }
    }

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
    clearRegisterAllocationOffsets();
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

    case ExtractFromTuple: {
        compileExtractFromTuple(node);
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
            loadDouble(addressFor(node->machineLocal()), result.fpr());
            VirtualRegister virtualRegister = node->virtualRegister();
            m_fprs.retain(result.fpr(), virtualRegister, SpillOrderDouble);
            generationInfoFromVirtualRegister(virtualRegister).initDouble(node, node->refCount(), result.fpr());
            break;
        }
        
        case FlushedInt32: {
            GPRTemporary result(this);
            load32(payloadFor(node->machineLocal()), result.gpr());
            
            // Like strictInt32Result, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderInteger);
            generationInfoFromVirtualRegister(virtualRegister).initInt32(node, node->refCount(), result.gpr());
            break;
        }
        
        case FlushedCell: {
            GPRTemporary result(this);
            load32(payloadFor(node->machineLocal()), result.gpr());
            
            // Like cellResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderCell);
            generationInfoFromVirtualRegister(virtualRegister).initCell(node, node->refCount(), result.gpr());
            break;
        }
            
        case FlushedBoolean: {
            GPRTemporary result(this);
            load32(payloadFor(node->machineLocal()), result.gpr());
            
            // Like booleanResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderBoolean);
            generationInfoFromVirtualRegister(virtualRegister).initBoolean(node, node->refCount(), result.gpr());
            break;
        }
            
        case FlushedJSValue: {
            JSValueRegsTemporary result(this);
            loadValue(addressFor(node->machineLocal()), result.regs());

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

    case MovHint:
    case ZombieHint: {
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
            storeDouble(value.fpr(), addressFor(node->machineLocal()));
            noResult(node);
            // Indicate that it's no longer necessary to retrieve the value of
            // this bytecode variable from registers or other locations in the stack,
            // but that it is stored as a double.
            recordSetLocal(DataFormatDouble);
            break;
        }
            
        case FlushedInt32: {
            SpeculateInt32Operand value(this, node->child1());
            store32(value.gpr(), payloadFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatInt32);
            break;
        }
            
        case FlushedCell: {
            SpeculateCellOperand cell(this, node->child1());
            GPRReg cellGPR = cell.gpr();
            storePtr(cellGPR, payloadFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatCell);
            break;
        }
            
        case FlushedBoolean: {
            SpeculateBooleanOperand value(this, node->child1());
            store32(value.gpr(), payloadFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatBoolean);
            break;
        }
            
        case FlushedJSValue: {
            JSValueOperand value(this, node->child1());
            JSValueRegs valueRegs = value.jsValueRegs();
            storeValue(valueRegs, addressFor(node->machineLocal()));
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

    case MakeAtomString:
        compileMakeAtomString(node);
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

    case ArithF16Round:
        compileArithF16Round(node);
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
        if (compare(node, LessThan, DoubleLessThanAndOrdered, operationCompareLess))
            return;
        break;

    case CompareLessEq:
        if (compare(node, LessThanOrEqual, DoubleLessThanOrEqualAndOrdered, operationCompareLessEq))
            return;
        break;

    case CompareGreater:
        if (compare(node, GreaterThan, DoubleGreaterThanAndOrdered, operationCompareGreater))
            return;
        break;

    case CompareGreaterEq:
        if (compare(node, GreaterThanOrEqual, DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq))
            return;
        break;

    case CompareBelow:
        compileCompareUnsigned(node, Below);
        break;

    case CompareBelowEq:
        compileCompareUnsigned(node, BelowOrEqual);
        break;

    case CompareEq:
        if (compare(node, Equal, DoubleEqualAndOrdered, operationCompareEq))
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

    case StringAt:
    case StringCharAt: {
        // Relies on StringCharAt and StringAt node having same basic layout as GetByVal
        JSValueRegsTemporary result;
        compileGetByValOnString(node, scopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>([&](DataFormat preferredFormat, bool needsFlush) {
            result = JSValueRegsTemporary(this);
            ASSERT(preferredFormat == DataFormatJS || preferredFormat == DataFormatCell);
            JSValueRegs regs = result.regs();
            if (needsFlush)
                flushRegisters();
            return std::tuple { regs, preferredFormat };
        }));
        break;
    }

    case StringFromCharCode: {
        compileFromCharCode(node);
        break;
    }

    case StringLocaleCompare: {
        compileStringLocaleCompare(node);
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
        std::optional<JSValueRegsFlushedCallResult> flushedResult;
        GPRTemporary oneRegResult;
        compileGetByVal(node, scopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>([&](DataFormat preferredFormat, bool needsFlush) {
            JSValueRegs resultRegs;
            switch (preferredFormat) {
            case DataFormatDouble:
                break;
            case DataFormatInt32:
            case DataFormatCell:
                ASSERT(!needsFlush);
                oneRegResult = GPRTemporary(this);
                resultRegs = JSValueRegs::payloadOnly(oneRegResult.gpr());
                break;
            default: {
                ASSERT(preferredFormat == DataFormatJS);
                if (!needsFlush) {
                    jsValueResult = JSValueRegsTemporary(this);
                    resultRegs = jsValueResult.regs();
                } else {
                    flushRegisters();
                    flushedResult.emplace(this);
                    resultRegs = flushedResult->regs();
                }
                break;
            }
            };
            return std::tuple { resultRegs, preferredFormat };
        }));
        break;
    }
    
    case StringSlice: {
        compileStringSlice(node);
        break;
    }

    case StringSubstring: {
        compileStringSubstring(node);
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
            LinkableConstant::globalObject(*this, node), baseRegs, thisRegs, propertyRegs, valueRegs);
        exceptionCheck();

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

    case StringReplaceString: {
        compileStringReplaceString(node);
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
            load32(
                Address(storageGPR, Butterfly::offsetOfPublicLength()), valuePayloadGPR);
            Jump undefinedCase =
                branchTest32(Zero, valuePayloadGPR);
            sub32(TrustedImm32(1), valuePayloadGPR);
            store32(
                valuePayloadGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
            load32(
                BaseIndex(storageGPR, valuePayloadGPR, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)),
                valueTagGPR);
            Jump slowCase = branchIfEmpty(valueTagGPR);
            store32(
                TrustedImm32(JSValue::EmptyValueTag),
                BaseIndex(storageGPR, valuePayloadGPR, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            load32(
                BaseIndex(storageGPR, valuePayloadGPR, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)),
                valuePayloadGPR);

            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPopAndRecoverLength,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), LinkableConstant::globalObject(*this, node), baseGPR));
            
            jsValueResult(valueTagGPR, valuePayloadGPR, node);
            break;
        }
            
        case Array::Double: {
            FPRTemporary temp(this);
            FPRReg tempFPR = temp.fpr();
            
            load32(
                Address(storageGPR, Butterfly::offsetOfPublicLength()), valuePayloadGPR);
            Jump undefinedCase =
                branchTest32(Zero, valuePayloadGPR);
            sub32(TrustedImm32(1), valuePayloadGPR);
            store32(
                valuePayloadGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
            loadDouble(
                BaseIndex(storageGPR, valuePayloadGPR, TimesEight),
                tempFPR);
            Jump slowCase = branchIfNaN(tempFPR);
            JSValue nan = JSValue(JSValue::EncodeAsDouble, PNaN);
            storeTrustedValue(
                nan,
                BaseIndex(storageGPR, valuePayloadGPR, TimesEight));
            boxDouble(tempFPR, valueTagGPR, valuePayloadGPR);

            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPopAndRecoverLength,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), LinkableConstant::globalObject(*this, node), baseGPR));
            
            jsValueResult(valueTagGPR, valuePayloadGPR, node);
            break;
        }

        case Array::ArrayStorage: {
            GPRTemporary storageLength(this);
            GPRReg storageLengthGPR = storageLength.gpr();

            load32(Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);
        
            JumpList setUndefinedCases;
            setUndefinedCases.append(branchTest32(Zero, storageLengthGPR));
        
            sub32(TrustedImm32(1), storageLengthGPR);
        
            Jump slowCase = branch32(AboveOrEqual, storageLengthGPR, Address(storageGPR, ArrayStorage::vectorLengthOffset()));

            loadValue(BaseIndex(storageGPR, storageLengthGPR, TimesEight, ArrayStorage::vectorOffset()), JSValueRegs { valueTagGPR, valuePayloadGPR });
        
            store32(storageLengthGPR, Address(storageGPR, ArrayStorage::lengthOffset()));

            setUndefinedCases.append(branchIfEmpty(valueTagGPR));
        
            store32(TrustedImm32(JSValue::EmptyValueTag), BaseIndex(storageGPR, storageLengthGPR, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

            sub32(TrustedImm32(1), Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
            addSlowPathGenerator(
                slowPathMove(
                    setUndefinedCases, this,
                    TrustedImm32(jsUndefined().tag()), valueTagGPR,
                    TrustedImm32(jsUndefined().payload()), valuePayloadGPR));
        
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPop,
                    JSValueRegs(valueTagGPR, valuePayloadGPR), LinkableConstant::globalObject(*this, node), baseGPR));

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

    case ArraySplice: {
        compileArraySplice(node);
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
                swap(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
            else if (op1.payloadGPR() == GPRInfo::returnValueGPR2) {
                move(op1.payloadGPR(), GPRInfo::returnValueGPR);
                move(op1.tagGPR(), GPRInfo::returnValueGPR2);
            } else {
                move(op1.tagGPR(), GPRInfo::returnValueGPR2);
                move(op1.payloadGPR(), GPRInfo::returnValueGPR);
            }
        }

        emitRestoreCalleeSaves();
        emitFunctionEpilogue();
        ret();
        
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
            
            move(value.gpr(), result.gpr());

            strictInt32Result(result.gpr(), node);
            break;
        }
            
        case UntypedUse: {
            JSValueOperand value(this, node->child1());
            
            if (!m_interpreter.needsTypeCheck(node->child1(), SpecBoolInt32 | SpecBoolean)) {
                GPRTemporary result(this);
                
                GPRReg valueGPR = value.payloadGPR();
                GPRReg resultGPR = result.gpr();
                
                move(valueGPR, resultGPR);
                strictInt32Result(result.gpr(), node);
                break;
            }
            
            GPRTemporary resultTag(this);
            GPRTemporary resultPayload(this);
            
            GPRReg valueTagGPR = value.tagGPR();
            GPRReg valuePayloadGPR = value.payloadGPR();
            GPRReg resultTagGPR = resultTag.gpr();
            GPRReg resultPayloadGPR = resultPayload.gpr();
            
            move(valuePayloadGPR, resultPayloadGPR);
            Jump isBoolean = branchIfBoolean(valueTagGPR, InvalidGPRReg);
            move(valueTagGPR, resultTagGPR);
            Jump done = jump();
            isBoolean.link(this);
            move(TrustedImm32(JSValue::Int32Tag), resultTagGPR);
            done.link(this);
            
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

    case ToPropertyKeyOrNumber: {
        compileToPropertyKeyOrNumber(node);
        break;
    }

    case ToNumber: {
        switch (node->child1().useKind()) {
        case StringUse: {
            SpeculateCellOperand argument(this, node->child1());
            GPRReg argumentGPR = argument.gpr();
            speculateString(node->child1(), argumentGPR);
            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationToNumberString, resultRegs, LinkableConstant::globalObject(*this, node), argumentGPR);
            jsValueResult(resultRegs, node);
            break;
        }
        case UntypedUse: {
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
                callOperation(operationToNumber, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs);
            } else {
                Jump notNumber;
                {
                    GPRTemporary scratch(this);
                    notNumber = branchIfNotNumber(argument.jsValueRegs(), scratch.gpr());
                }
                move(argumentTagGPR, resultRegs.tagGPR());
                move(argumentPayloadGPR, resultRegs.payloadGPR());
                Jump done = jump();

                notNumber.link(this);
                callOperationWithSilentSpill(operationToNumber, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs);

                done.link(this);
            }

            jsValueResult(resultRegs.tagGPR(), resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
            break;
        }
        default:
            DFG_CRASH(m_graph, node, "Bad use kind");
            break;
        }
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

    case StringIndexOf: {
        compileStringIndexOf(node);
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

    case NewArrayWithConstantSize: {
        compileNewArrayWithConstantSize(node);
        break;
    }

    case NewArrayWithSpecies: {
        compileNewArrayWithSpecies(node);
        break;
    }

    case NewArrayWithSizeAndStructure: {
        compileNewArrayWithSizeAndStructure(node);
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

    case NewMap: {
        compileNewMap(node);
        break;
    }

    case NewSet: {
        compileNewSet(node);
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
    case ObjectGetOwnPropertyNames:
    case ObjectGetOwnPropertySymbols:
    case ReflectOwnKeys: {
        compileOwnPropertyKeysVariant(node);
        break;
    }

    case ObjectToString: {
        compileObjectToString(node);
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

    case UnwrapGlobalProxy:
        compileUnwrapGlobalProxy(node);
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

    case CallCustomAccessorGetter: {
        compileCallCustomAccessorGetter(node);
        break;
    }

    case CallCustomAccessorSetter: {
        compileCallCustomAccessorSetter(node);
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

            cachedGetByIdWithThis(node, node->origin.semantic, InvalidGPRReg, baseGPR, InvalidGPRReg, thisGPR, resultTagGPR, resultPayloadGPR, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier());

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

            JumpList notCellList;
            notCellList.append(branchIfNotCell(base.jsValueRegs()));
            notCellList.append(branchIfNotCell(thisValue.jsValueRegs()));

            cachedGetByIdWithThis(node, node->origin.semantic, baseTagGPR, basePayloadGPR, thisTagGPR, thisPayloadGPR, resultTagGPR, resultPayloadGPR, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), notCellList);

            jsValueResult(resultTagGPR, resultPayloadGPR, node);
        }
        
        break;
    }

    case GetArrayLength:
    case GetUndetachedTypeArrayLength:
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
        storePtr(TrustedImmPtr(newStructure), Address(baseGPR, JSCell::structureIDOffset()));
        
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

    case GetWebAssemblyInstanceExports: {
        compileGetWebAssemblyInstanceExports(node);
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
        
        load32(Address(storageGPR, offsetRelativeToBase(storageAccessData.offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultPayloadGPR);
        
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

    case ToIntegerOrInfinity: {
        compileToIntegerOrInfinity(node);
        break;
    }

    case ToLength: {
        compileToLength(node);
        break;
    }

    case CheckTypeInfoFlags: {
        compileCheckTypeInfoFlags(node);
        break;
    }

    case HasStructureWithFlags: {
        compileHasStructureWithFlags(node);
        break;
    }

    case OverridesHasInstance: {
        compileOverridesHasInstance(node);
        break;
    }

    case InstanceOf:
    case InstanceOfMegamorphic: {
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
        comparePtr(Equal, value.tagGPR(), TrustedImm32(JSValue::EmptyValueTag), result.gpr());
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

        Jump isCell = branchIfCell(value.jsValueRegs());
        
        compare32(Equal, value.tagGPR(), TrustedImm32(JSValue::UndefinedTag), result.gpr());
        Jump done = jump();
        
        isCell.link(this);
        Jump notMasqueradesAsUndefined;
        if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
            move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = jump();
        } else {
            Jump isMasqueradesAsUndefined = branchTest8(
                NonZero,
                Address(value.payloadGPR(), JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined));
            move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = jump();
            
            isMasqueradesAsUndefined.link(this);
            loadLinkableConstant(LinkableConstant::globalObject(*this, node), localGlobalObjectGPR);
            loadPtr(Address(value.payloadGPR(), JSCell::structureIDOffset()), result.gpr());
            loadPtr(Address(result.gpr(), Structure::globalObjectOffset()), remoteGlobalObjectGPR);
            compare32(Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, result.gpr());
        }

        notMasqueradesAsUndefined.link(this);
        done.link(this);
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

        move(valueTagGPR, resultGPR);
        static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1));
        or32(TrustedImm32(1), resultGPR);
        compare32(Equal, resultGPR, TrustedImm32(JSValue::NullTag), resultGPR);

        booleanResult(resultGPR, node);
        break;
    }


    case IsBoolean: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, TagWord);
        
        compare32(Equal, value.tagGPR(), TrustedImm32(JSValue::BooleanTag), result.gpr());
        booleanResult(result.gpr(), node);
        break;
    }

    case IsNumber: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, TagWord);
        
        add32(TrustedImm32(1), value.tagGPR(), result.gpr());
        compare32(Below, result.gpr(), TrustedImm32(JSValue::LowestTag + 1), result.gpr());
        booleanResult(result.gpr(), node);
        break;
    }

    case NumberIsInteger: {
        JSValueOperand input(this, node->child1());
        JSValueRegs inputRegs = input.jsValueRegs();
        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationNumberIsInteger, resultGPR, LinkableConstant::globalObject(*this, node), inputRegs);
        booleanResult(resultGPR, node);
        break;
    }

    case GlobalIsNaN: {
        compileGlobalIsNaN(node);
        break;
    }

    case NumberIsNaN: {
        compileNumberIsNaN(node);
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
        callOperation(operationMapHash, resultGPR, LinkableConstant::globalObject(*this, node), inputRegs);
        strictInt32Result(resultGPR, node);
        break;
    }

    case NormalizeMapKey: {
        compileNormalizeMapKey(node);
        break;
    }

    case MapGet: {
        SpeculateCellOperand map(this, node->child1());
        JSValueOperand key(this, node->child2());
        SpeculateInt32Operand hash(this, node->child3());

        GPRReg mapGPR = map.gpr();
        JSValueRegs keyRegs = key.jsValueRegs();
        GPRReg hashGPR = hash.gpr();

        bool isMapObjectUse = node->child1().useKind() == MapObjectUse;
        if (isMapObjectUse)
            speculateMapObject(node->child1(), mapGPR);
        else if (node->child1().useKind() == SetObjectUse)
            speculateSetObject(node->child1(), mapGPR);
        else
            RELEASE_ASSERT_NOT_REACHED();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        auto operation = isMapObjectUse ? operationMapGet : operationSetGet;
        callOperation(operation, resultGPR, LinkableConstant::globalObject(*this, node), mapGPR, keyRegs, hashGPR);
        storageResult(resultGPR, node);
        break;
    }

    case LoadMapValue:
        compileLoadMapValue(node);
        break;

    case IsEmptyStorage:
        compileIsEmptyStorage(node);
        break;

    case MapStorage:
        compileMapStorage(node);
        break;

    case MapIteratorNext:
        compileMapIteratorNext(node);
        break;

    case MapIteratorKey:
        compileMapIteratorKey(node);
        break;

    case MapIteratorValue:
        compileMapIteratorValue(node);
        break;

    case MapIterationNext:
        compileMapIterationNext(node);
        break;

    case MapIterationEntry:
        compileMapIterationEntry(node);
        break;

    case MapIterationEntryKey:
        compileMapIterationEntryKey(node);
        break;

    case MapIterationEntryValue:
        compileMapIterationEntryValue(node);
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

    case MapOrSetDelete:
        compileMapOrSetDelete(node);
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

    case DFG::Call:
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

        JumpList slowPath;
        switch (node->child2().useKind()) {
        case SymbolUse: {
            speculateSymbol(node->child2(), keyRegs.payloadGPR());
            loadPtr(Address(keyRegs.payloadGPR(), Symbol::offsetOfSymbolImpl()), implGPR);
            break;
        }
        case StringUse: {
            speculateString(node->child2(), keyRegs.payloadGPR());
            loadPtr(Address(keyRegs.payloadGPR(), JSString::offsetOfValue()), implGPR);
            slowPath.append(branchIfRopeStringImpl(implGPR));
            slowPath.append(branchTest32(
                Zero, Address(implGPR, StringImpl::flagsOffset()),
                TrustedImm32(StringImpl::flagIsAtom())));
            break;
        }
        case UntypedUse: {
            slowPath.append(branchIfNotCell(keyRegs));
            auto isNotString = branchIfNotString(keyRegs.payloadGPR());
            loadPtr(Address(keyRegs.payloadGPR(), JSString::offsetOfValue()), implGPR);
            slowPath.append(branchIfRopeStringImpl(implGPR));
            slowPath.append(branchTest32(
                Zero, Address(implGPR, StringImpl::flagsOffset()),
                TrustedImm32(StringImpl::flagIsAtom())));
            auto hasUniquedImpl = jump();

            isNotString.link(this);
            slowPath.append(branchIfNotSymbol(keyRegs.payloadGPR()));
            loadPtr(Address(keyRegs.payloadGPR(), Symbol::offsetOfSymbolImpl()), implGPR);

            hasUniquedImpl.link(this);
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
        load32(Address(implGPR, UniquedStringImpl::flagsOffset()), hashGPR);
        urshift32(TrustedImm32(StringImpl::s_flagCount), hashGPR);
        load32(Address(objectGPR, JSCell::structureIDOffset()), structureIDGPR);
        add32(structureIDGPR, hashGPR);
        and32(TrustedImm32(HasOwnPropertyCache::mask), hashGPR);
        mul32(TrustedImm32(sizeof(HasOwnPropertyCache::Entry)), hashGPR, hashGPR);
        ASSERT(vm().hasOwnPropertyCache());
        move(TrustedImmPtr(vm().hasOwnPropertyCache()), tempGPR);
        slowPath.append(branchPtr(NotEqual,
            BaseIndex(tempGPR, hashGPR, TimesOne, HasOwnPropertyCache::Entry::offsetOfImpl()), implGPR));
        load8(BaseIndex(tempGPR, hashGPR, TimesOne, HasOwnPropertyCache::Entry::offsetOfResult()), resultGPR);
        load32(BaseIndex(tempGPR, hashGPR, TimesOne, HasOwnPropertyCache::Entry::offsetOfStructureID()), tempGPR);
        slowPath.append(branch32(NotEqual, tempGPR, structureIDGPR));
        auto done = jump();

        slowPath.link(this);
        if (node->child2().useKind() != UntypedUse) {
            move(TrustedImm32(JSValue::CellTag), tempGPR);
            keyRegs = JSValueRegs(tempGPR, keyRegs.payloadGPR());
        }
        callOperationWithSilentSpill(operationHasOwnProperty, resultGPR, LinkableConstant::globalObject(*this, node), objectGPR, keyRegs);

        done.link(this);
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
        basicBlockLocation->emitExecuteCode(*this, scratch1.gpr());
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
        add64(TrustedImm32(1), AbsoluteAddress(node->executionCounter()->address()));
        break;

    case SuperSamplerBegin:
        add32(TrustedImm32(1), AbsoluteAddress(std::bit_cast<void*>(&g_superSamplerCount)));
        break;

    case SuperSamplerEnd:
        sub32(TrustedImm32(1), AbsoluteAddress(std::bit_cast<void*>(&g_superSamplerCount)));
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
    case DateSetTime:
    case StringCodePointAt:
    case CallWasm:
    case FunctionBind:
    case NewBoundFunction:
    case EnumeratorPutByVal:
    case GetByIdMegamorphic:
    case GetByIdWithThisMegamorphic:
    case GetByValMegamorphic:
    case GetByValWithThisMegamorphic:
    case PutByIdMegamorphic:
    case PutByValMegamorphic:
    case InByIdMegamorphic:
    case InByValMegamorphic:
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
    move(TrustedImm32(1), gpr);
}

void SpeculativeJIT::moveFalseTo(GPRReg gpr)
{
    move(TrustedImm32(0), gpr);
}

void SpeculativeJIT::blessBoolean(GPRReg)
{
}

void SpeculativeJIT::compileArithRandom(Node* node)
{
    flushRegisters();

    FPRResult result(this);
    callOperation(operationRandom, result.fpr(), LinkableConstant::globalObject(*this, node));
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
    callOperation(operationGetByValWithThisGeneric, resultRegs, LinkableConstant::globalObject(*this, node), baseRegs, subscriptRegs, thisValueRegs);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCreateClonedArguments(Node* node)
{
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();

    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    // We set up the arguments ourselves, because we have the whole register file and we can
    // set them up directly into the argument registers.

    // Arguments: 0:JSGlobalObject*, 1:structure, 2:start, 3:length, 4:callee, 5: butterfly
    setupArgument(5, [&] (GPRReg destGPR) { move(TrustedImm32(0), destGPR); });
    setupArgument(4, [&] (GPRReg destGPR) { emitGetCallee(node->origin.semantic, destGPR); });
    setupArgument(3, [&] (GPRReg destGPR) { emitGetLength(node->origin.semantic, destGPR); });
    setupArgument(2, [&] (GPRReg destGPR) { emitGetArgumentStart(node->origin.semantic, destGPR); });
    setupArgument(
        1, [&] (GPRReg destGPR) {
            loadLinkableConstant(LinkableConstant(*this, globalObject->clonedArgumentsStructure()), destGPR);
        });
    setupArgument(
        0, [&] (GPRReg destGPR) {
            loadLinkableConstant(LinkableConstant::globalObject(*this, node), destGPR);
        });

    appendCall(operationCreateClonedArguments);
    setupResults(resultGPR);
    exceptionCheck();

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetById(Node* node, AccessType accessType)
{
    ASSERT(accessType == AccessType::GetById || accessType == AccessType::GetByIdDirect || accessType == AccessType::TryGetById);

    switch (node->child1().useKind()) {
    case CellUse: {
        SpeculateCellOperand base(this, node->child1());
        JSValueRegsTemporary result(this, Reuse, base);

        JSValueRegs baseRegs = JSValueRegs::payloadOnly(base.gpr());
        JSValueRegs resultRegs = result.regs();

        base.use();

        cachedGetById(node, node->origin.semantic, baseRegs, resultRegs, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), Jump(), NeedToSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    case UntypedUse: {
        JSValueOperand base(this, node->child1());
        JSValueRegsTemporary result(this, Reuse, base);

        JSValueRegs baseRegs = base.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        base.use();

        Jump notCell = branchIfNotCell(baseRegs);

        cachedGetById(node, node->origin.semantic, baseRegs, resultRegs, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), notCell, NeedToSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileGetByIdFlush(Node* node, AccessType accessType)
{
    switch (node->child1().useKind()) {
    case CellUse: {
        SpeculateCellOperand base(this, node->child1());
        JSValueRegsFlushedCallResult result(this);

        JSValueRegs baseRegs = JSValueRegs::payloadOnly(base.gpr());
        JSValueRegs resultRegs = result.regs();

        base.use();

        flushRegisters();

        cachedGetById(node, node->origin.semantic, baseRegs, resultRegs, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), Jump(), DontSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    case UntypedUse: {
        JSValueOperand base(this, node->child1());
        JSValueRegsFlushedCallResult result(this);

        JSValueRegs baseRegs = base.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        base.use();

        flushRegisters();

        Jump notCell = branchIfNotCell(baseRegs);

        cachedGetById(node, node->origin.semantic, baseRegs, resultRegs, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), notCell, DontSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileDeleteById(Node* node)
{
    if (node->child1().useKind() == CellUse) {
        SpeculateCellOperand base(this, node->child1());
        JSValueRegsTemporary result(this);

        JumpList slowCases;

        JSValueRegs resultRegs = result.regs();
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = resultRegs.payloadGPR();

        CodeOrigin codeOrigin = node->origin.semantic;
        CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
        RegisterSetBuilder usedRegisters = this->usedRegisters();

        auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
        JITDelByIdGenerator gen(
            codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, node->ecmaMode().isStrict() ? AccessType::DeleteByIdStrict : AccessType::DeleteByIdSloppy, usedRegisters, node->cacheableIdentifier(),
            JSValueRegs::payloadOnly(baseGPR), resultRegs, InvalidGPRReg);

        auto* operation = node->ecmaMode().isStrict() ? operationDeleteByIdStrictOptimize : operationDeleteByIdSloppyOptimize;
        gen.generateFastPath(*this);
        slowCases.append(gen.slowPathJump());
        std::unique_ptr<SlowPathGenerator> slowPath = slowPathCall(
            slowCases, this, operation,
            resultGPR, CellValue(baseGPR), TrustedImmPtr(gen.stubInfo()));

        addDelById(gen, slowPath.get());
        addSlowPathGenerator(WTFMove(slowPath));

        unblessedBooleanResult(resultGPR, node);
        return;
    }

    // FIXME: We should use IC even if child1 is UntypedUse. In that case, we should emit write-barrier after the fast path of IC.
    // https://bugs.webkit.org/show_bug.cgi?id=209397
    ASSERT(node->child1().useKind() == UntypedUse);
    JSValueOperand base(this, node->child1());

    JSValueRegs baseRegs = base.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(node->ecmaMode().isStrict() ? operationDeleteByIdStrictGeneric : operationDeleteByIdSloppyGeneric, resultGPR, LinkableConstant::globalObject(*this, node), baseRegs, node->cacheableIdentifier().rawBits());

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileDeleteByVal(Node* node)
{
    if (node->child1().useKind() == CellUse) {
        SpeculateCellOperand base(this, node->child1());
        JSValueOperand key(this, node->child2(), ManualOperandSpeculation);
        JSValueRegsTemporary result(this, Reuse, key);

        JumpList slowCases;

        GPRReg baseGPR = base.gpr();
        JSValueRegs keyRegs = key.jsValueRegs();
        JSValueRegs resultRegs = result.regs();
        GPRReg resultGPR = resultRegs.payloadGPR();

        speculate(node, node->child2());

        if (needsTypeCheck(node->child2(), SpecCell))
            slowCases.append(branchIfNotCell(keyRegs));

        CodeOrigin codeOrigin = node->origin.semantic;
        CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
        RegisterSetBuilder usedRegisters = this->usedRegisters();

        auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
        JITDelByValGenerator gen(
            codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, node->ecmaMode().isStrict() ? AccessType::DeleteByValStrict : AccessType::DeleteByValSloppy, usedRegisters,
            JSValueRegs::payloadOnly(baseGPR), keyRegs, resultRegs, InvalidGPRReg);

        auto* operation = node->ecmaMode().isStrict() ? operationDeleteByValStrictOptimize : operationDeleteByValSloppyOptimize;
        gen.generateFastPath(*this);
        slowCases.append(gen.slowPathJump());
        std::unique_ptr<SlowPathGenerator> slowPath = slowPathCall(
            slowCases, this, operation,
            resultGPR, CellValue(baseGPR), keyRegs, TrustedImmPtr(gen.stubInfo()));

        addDelByVal(gen, slowPath.get());
        addSlowPathGenerator(WTFMove(slowPath));

        unblessedBooleanResult(resultGPR, node);
        return;
    }

    // FIXME: We should use IC even if child1 is UntypedUse. In that case, we should emit write-barrier after the fast path of IC.
    // https://bugs.webkit.org/show_bug.cgi?id=209397
    JSValueOperand base(this, node->child1());
    JSValueOperand key(this, node->child2());

    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs keyRegs = key.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(node->ecmaMode().isStrict() ? operationDeleteByValStrictGeneric : operationDeleteByValSloppyGeneric, resultGPR, LinkableConstant::globalObject(*this, node), baseRegs, keyRegs);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileInById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, base, PayloadWord);

    GPRReg baseGPR = base.gpr();
    JSValueRegs resultRegs = result.regs();

    base.use();

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITInByIdGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, node->cacheableIdentifier(),
        JSValueRegs::payloadOnly(baseGPR), resultRegs, InvalidGPRReg);

    JumpList slowCases;

    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(
        slowCases, this, operationInByIdOptimize,
        NeedToSpill, ExceptionCheckRequirement::CheckNeeded,
        resultRegs, CellValue(baseGPR), TrustedImmPtr(gen.stubInfo()));

    addInById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    blessedBooleanResult(resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileInByVal(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand key(this, node->child2());
    JSValueRegsTemporary result(this, Reuse, key);

    GPRReg baseGPR = base.gpr();
    JSValueRegs keyRegs = key.jsValueRegs();
    JSValueRegs resultRegs = result.regs();

    base.use();
    key.use();

    JumpList slowCases;

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITInByValGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::InByVal, usedRegisters,
        JSValueRegs::payloadOnly(baseGPR), keyRegs, resultRegs, InvalidGPRReg, InvalidGPRReg);

    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(
        slowCases, this, operationInByValOptimize,
        NeedToSpill, ExceptionCheckRequirement::CheckNeeded,
        resultRegs, CellValue(baseGPR), keyRegs, TrustedImmPtr(gen.stubInfo()), nullptr);

    addInByVal(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    blessedBooleanResult(resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileHasPrivate(Node* node, AccessType type)
{
    SpeculateCellOperand base(this, node->child1());
    SpeculateCellOperand propertyOrBrand(this, node->child2());
    JSValueRegsTemporary result(this, Reuse, base);

    GPRReg baseGPR = base.gpr();
    GPRReg propertyOrBrandGPR = propertyOrBrand.gpr();
    JSValueRegs resultRegs = result.regs();

    speculateSymbol(node->child2(), propertyOrBrandGPR);

    base.use();
    propertyOrBrand.use();

    JumpList slowCases;

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITInByValGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, type, usedRegisters,
        JSValueRegs::payloadOnly(baseGPR), JSValueRegs::payloadOnly(propertyOrBrandGPR), resultRegs, InvalidGPRReg, InvalidGPRReg);

    std::visit([&](auto* stubInfo) {
        stubInfo->propertyIsSymbol = true;
    }, stubInfo);

    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(
        slowCases, this, type == AccessType::HasPrivateName ? operationHasPrivateNameOptimize : operationHasPrivateBrandOptimize,
        NeedToSpill, ExceptionCheckRequirement::CheckNeeded,
        resultRegs, CellValue(baseGPR), CellValue(propertyOrBrandGPR), TrustedImmPtr(gen.stubInfo()));

    addInByVal(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    blessedBooleanResult(resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileHasPrivateName(Node* node)
{
    compileHasPrivate(node, AccessType::HasPrivateName);
}

void SpeculativeJIT::compileHasPrivateBrand(Node* node)
{
    compileHasPrivate(node, AccessType::HasPrivateBrand);
}

void SpeculativeJIT::compilePutByVal(Node* node)
{
    ArrayMode arrayMode = node->arrayMode().modeForPut();
    Edge child1 = m_graph.varArgChild(node, 0);
    Edge child2 = m_graph.varArgChild(node, 1);
    Edge child3 = m_graph.varArgChild(node, 2);
    Edge child4 = m_graph.varArgChild(node, 3);

    switch (arrayMode.type()) {
    case Array::AnyTypedArray:
    case Array::ForceExit:
    case Array::SelectUsingArguments:
    case Array::SelectUsingPredictions:
    case Array::Unprofiled:
    case Array::String:
    case Array::DirectArguments:
    case Array::ScopedArguments:
    case Array::Undecided:
    case Array::BigInt64Array:
    case Array::BigUint64Array:
        DFG_CRASH(m_graph, node, "Bad array mode type");
        break;
    case Array::Generic: {
        DFG_ASSERT(m_graph, node, node->op() == PutByVal || node->op() == PutByValDirect, node->op());
        if (m_graph.m_slowPutByVal.contains(node) || (child1.useKind() != CellUse && child1.useKind() != KnownCellUse)) {
            if (child1.useKind() == CellUse || child1.useKind() == KnownCellUse) {
                if (child2.useKind() == StringUse) {
                    compilePutByValForCellWithString(node);
                    break;
                }

                if (child2.useKind() == SymbolUse) {
                    compilePutByValForCellWithSymbol(node);
                    break;
                }
            }

            JSValueOperand base(this, child1);
            JSValueOperand property(this, child2);
            JSValueOperand value(this, child3);
            JSValueRegs baseRegs = base.jsValueRegs();
            JSValueRegs propertyRegs = property.jsValueRegs();
            JSValueRegs valueRegs = value.jsValueRegs();

            flushRegisters();
            if (node->op() == PutByValDirect)
                callOperation(node->ecmaMode().isStrict() ? operationDirectPutByValStrictGeneric : operationDirectPutByValSloppyGeneric, LinkableConstant::globalObject(*this, node), baseRegs, propertyRegs, valueRegs);
            else
                callOperation(node->ecmaMode().isStrict() ? operationPutByValStrictGeneric : operationPutByValSloppyGeneric, LinkableConstant::globalObject(*this, node), baseRegs, propertyRegs, valueRegs);

            noResult(node);
            break;
        }

        JSValueOperand base(this, child1, ManualOperandSpeculation);
        JSValueOperand property(this, child2, ManualOperandSpeculation);
        JSValueOperand value(this, child3, ManualOperandSpeculation);
        JSValueRegs baseRegs = base.jsValueRegs();
        JSValueRegs propertyRegs = property.jsValueRegs();
        JSValueRegs valueRegs = value.jsValueRegs();

        speculate(node, child1);
        speculate(node, child2);
        speculate(node, child3);

        CodeOrigin codeOrigin = node->origin.semantic;
        CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
        RegisterSetBuilder usedRegisters = this->usedRegisters();
        bool isDirect = node->op() == PutByValDirect;
        ECMAMode ecmaMode = node->ecmaMode();

        auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
        JITPutByValGenerator gen(
            codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, isDirect ? (ecmaMode.isStrict() ? AccessType::PutByValDirectStrict : AccessType::PutByValDirectSloppy) : (ecmaMode.isStrict() ? AccessType::PutByValStrict : AccessType::PutByValSloppy), usedRegisters,
            baseRegs, propertyRegs, valueRegs, InvalidGPRReg, InvalidGPRReg);

        std::visit([&](auto* stubInfo) {
            if (m_state.forNode(child2).isType(SpecString))
                stubInfo->propertyIsString = true;
            else if (m_state.forNode(child2).isType(SpecInt32Only))
                stubInfo->propertyIsInt32 = true;
            else if (m_state.forNode(child2).isType(SpecSymbol))
                stubInfo->propertyIsSymbol = true;
        }, stubInfo);

        JumpList slowCases;

        auto operation = isDirect ? (ecmaMode.isStrict() ? operationDirectPutByValStrictOptimize : operationDirectPutByValSloppyOptimize) : (ecmaMode.isStrict() ? operationPutByValStrictOptimize : operationPutByValSloppyOptimize);
        gen.generateFastPath(*this);
        slowCases.append(gen.slowPathJump());
        auto slowPath = slowPathCall(
            slowCases, this, operation,
            NoResult, baseRegs, propertyRegs, valueRegs, TrustedImmPtr(gen.stubInfo()), nullptr);

        addPutByVal(gen, slowPath.get());
        addSlowPathGenerator(WTFMove(slowPath));

        noResult(node);
        break;
    }
    case Array::Int32: {
        speculateInt32(child3);
        FALLTHROUGH;
    }
    case Array::Contiguous: {
        compileContiguousPutByVal(node);
        break;
    }
    case Array::Double: {
        compileDoublePutByVal(node);
        break;
    }
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        SpeculateCellOperand base(this, child1);
        SpeculateStrictInt32Operand property(this, child2);
        JSValueOperand value(this, child3);
        StorageOperand storage(this, child4);

        GPRReg baseReg = base.gpr();
        GPRReg propertyReg = property.gpr();
        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg storageReg = storage.gpr();

        if (node->op() == PutByValAlias) {
            // Store the value to the array.
            GPRReg propertyReg = property.gpr();
            storeValue(valueRegs, BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset()));
            noResult(node);
            break;
        }

        GPRTemporary temporary;
        GPRReg temporaryReg = temporaryRegisterForPutByVal(temporary, node);

        JumpList slowCases;

        Jump beyondArrayBounds = branch32(AboveOrEqual, propertyReg, Address(storageReg, ArrayStorage::vectorLengthOffset()));
        if (!arrayMode.isOutOfBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, beyondArrayBounds);
        else
            slowCases.append(beyondArrayBounds);

        // Check if we're writing to a hole; if so increment m_numValuesInVector.
        if (arrayMode.isInBounds()) {
            speculationCheck(
                StoreToHole, JSValueRegs(), nullptr,
                branchIfEmpty(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset())));
        } else {
            Jump notHoleValue = branchIfNotEmpty(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset()));
            if (arrayMode.isSlowPut()) {
                // This is sort of strange. If we wanted to optimize this code path, we would invert
                // the above branch. But it's simply not worth it since this only happens if we're
                // already having a bad time.
                slowCases.append(jump());
            } else {
                add32(TrustedImm32(1), Address(storageReg, ArrayStorage::numValuesInVectorOffset()));

                // If we're writing to a hole we might be growing the array;
                Jump lengthDoesNotNeedUpdate = branch32(Below, propertyReg, Address(storageReg, ArrayStorage::lengthOffset()));
                add32(TrustedImm32(1), propertyReg, temporaryReg);
                store32(temporaryReg, Address(storageReg, ArrayStorage::lengthOffset()));

                lengthDoesNotNeedUpdate.link(this);
            }
            notHoleValue.link(this);
        }

        // Store the value to the array.
        storeValue(valueRegs, BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset()));

        base.use();
        property.use();
        value.use();
        storage.use();

        if (!slowCases.empty()) {
            addSlowPathGenerator(slowPathCall(
                slowCases, this,
                node->ecmaMode().isStrict() ?
                    (node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsStrict) :
                    (node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsSloppy : operationPutByValBeyondArrayBoundsSloppy),
                NoResult, LinkableConstant::globalObject(*this, node), baseReg, propertyReg, valueRegs));
        }

        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
    case Array::Int8Array:
    case Array::Int16Array:
    case Array::Int32Array:
    case Array::Uint8Array:
    case Array::Uint8ClampedArray:
    case Array::Uint16Array:
    case Array::Uint32Array:
    case Array::Float16Array:
    case Array::Float32Array:
    case Array::Float64Array: {
        TypedArrayType type = arrayMode.typedArrayType();
        if (isInt(type))
            compilePutByValForIntTypedArray(node, type);
        else
            compilePutByValForFloatTypedArray(node, type);
    } }
}

void SpeculativeJIT::compileGetPrivateNameByVal(Node* node, JSValueRegs baseRegs, JSValueRegs propertyRegs)
{
    DFG_ASSERT(m_graph, node, node->op() == GetPrivateName);
    DFG_ASSERT(m_graph, node, m_graph.child(node, 1).useKind() == SymbolUse);

    JSValueRegsTemporary result(this);

    JSValueRegs resultRegs = result.regs();

    speculateSymbol(m_graph.child(node, 1));

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();

    JumpList slowCases;
    const bool baseIsKnownCell = m_state.forNode(m_graph.child(node, 0)).isType(SpecCell);
    if (!baseIsKnownCell)
        slowCases.append(branchIfNotCell(baseRegs));

    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITGetByValGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::GetPrivateName, usedRegisters,
        baseRegs, propertyRegs, resultRegs, InvalidGPRReg, InvalidGPRReg);

    std::visit([&](auto* stubInfo) {
        stubInfo->propertyIsSymbol = true;
    }, stubInfo);

    auto makeSlowPathICCall = [&](auto base, auto) {
        gen.generateFastPath(*this);
        slowCases.append(gen.slowPathJump());
        return slowPathCall(
            slowCases, this, operationGetPrivateNameOptimize,
            result.regs(), base, CellValue(propertyRegs.payloadGPR()), TrustedImmPtr(gen.stubInfo()));
    };

    std::unique_ptr<SlowPathGenerator> slowPath = baseIsKnownCell
        ? makeSlowPathICCall(CellValue(baseRegs.payloadGPR()), stubInfoConstant)
        : makeSlowPathICCall(baseRegs, stubInfoConstant);

    addGetByVal(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    jsValueResult(result.regs(), node, DataFormatJS);
}

void SpeculativeJIT::compileGetPrivateNameById(Node* node)
{
    switch (m_graph.child(node, 0).useKind()) {
    case CellUse: {
        SpeculateCellOperand base(this, m_graph.child(node, 0));
        JSValueRegsTemporary result(this, Reuse, base);

        JSValueRegs baseRegs = JSValueRegs::payloadOnly(base.gpr());
        JSValueRegs resultRegs = result.regs();

        cachedGetById(node, node->origin.semantic, baseRegs, resultRegs, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), Jump(), NeedToSpill, AccessType::GetPrivateNameById);

        jsValueResult(resultRegs, node, DataFormatJS);
        break;
    }

    case UntypedUse: {
        JSValueOperand base(this, m_graph.child(node, 0));
        JSValueRegsTemporary result(this, Reuse, base);

        JSValueRegs baseRegs = base.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        Jump notCell = branchIfNotCell(baseRegs);

        cachedGetById(node, node->origin.semantic, baseRegs, resultRegs, InvalidGPRReg, InvalidGPRReg, node->cacheableIdentifier(), notCell, NeedToSpill, AccessType::GetPrivateNameById);

        jsValueResult(resultRegs, node, DataFormatJS);
        break;
    }

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compilePutPrivateName(Node* node)
{
    Edge& child1 = node->child1();
    Edge& child2 = node->child2();
    Edge& child3 = node->child3();
    if (m_graph.m_slowPutByVal.contains(node) || (child1.useKind() != CellUse && child1.useKind() != KnownCellUse)) {
        ASSERT(child1.useKind() == UntypedUse);
        JSValueOperand base(this, child1);
        SpeculateCellOperand propertyValue(this, child2);
        JSValueOperand value(this, child3);

        JSValueRegs valueRegs = value.jsValueRegs();
        JSValueRegs baseRegs = base.jsValueRegs();

        GPRReg propertyGPR = propertyValue.gpr();

        speculateSymbol(child2, propertyGPR);

        flushRegisters();
        auto operation = node->privateFieldPutKind().isDefine() ? operationPutByValDefinePrivateFieldGeneric : operationPutByValSetPrivateFieldGeneric;
        callOperation(operation, LinkableConstant::globalObject(*this, node), baseRegs, CellValue(propertyGPR), valueRegs);

        noResult(node);
        return;
    }

    SpeculateCellOperand base(this, child1);
    SpeculateCellOperand propertyValue(this, child2);
    JSValueOperand value(this, child3);

    GPRReg baseGPR = base.gpr();
    GPRReg propertyGPR = propertyValue.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    speculateSymbol(child2, propertyGPR);

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();

    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITPutByValGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, node->privateFieldPutKind().isDefine() ? AccessType::DefinePrivateNameByVal : AccessType::SetPrivateNameByVal, usedRegisters,
        JSValueRegs::payloadOnly(baseGPR), JSValueRegs::payloadOnly(propertyGPR), valueRegs, InvalidGPRReg, InvalidGPRReg);

    std::visit([&](auto* stubInfo) {
        stubInfo->propertyIsSymbol = true;
    }, stubInfo);

    JumpList slowCases;

    auto operation = node->privateFieldPutKind().isDefine() ? operationPutByValDefinePrivateFieldOptimize : operationPutByValSetPrivateFieldOptimize;
    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(
        slowCases, this, operation,
        NoResult, CellValue(baseGPR), CellValue(propertyGPR), valueRegs, TrustedImmPtr(gen.stubInfo()), nullptr);

    addPutByVal(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    noResult(node);
}

void SpeculativeJIT::compilePutPrivateNameById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg baseGPR = base.gpr();
    GPRReg scratchGPR = scratch.gpr();

    // We emit property check during DFG generation, so we don't need
    // to check it here.
    cachedPutById(node, node->origin.semantic, baseGPR, valueRegs, InvalidGPRReg, scratchGPR, InvalidGPRReg, node->cacheableIdentifier(), node->privateFieldPutKind().isDefine() ? AccessType::DefinePrivateNameById : AccessType::SetPrivateNameById);

    noResult(node);
}

void SpeculativeJIT::compileCheckPrivateBrand(Node* node)
{
    JSValueOperand base(this, node->child1());
    SpeculateCellOperand brandValue(this, node->child2());

    JSValueRegs baseRegs = base.jsValueRegs();
    GPRReg brandGPR = brandValue.gpr();

    speculateSymbol(node->child2(), brandGPR);

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();

    JumpList slowCases;
    if (needsTypeCheck(node->child1(), SpecCell))
        slowCases.append(branchIfNotCell(baseRegs));

    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITPrivateBrandAccessGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::CheckPrivateBrand, usedRegisters,
        baseRegs, JSValueRegs::payloadOnly(brandGPR), InvalidGPRReg);

    std::visit([&](auto* stubInfo) {
        stubInfo->propertyIsSymbol = true;
    }, stubInfo);

    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(
        slowCases, this, operationCheckPrivateBrandOptimize, NoResult,
        baseRegs, CellValue(brandGPR), TrustedImmPtr(gen.stubInfo()));

    addPrivateBrandAccess(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    noResult(node);
}

void SpeculativeJIT::compileSetPrivateBrand(Node* node)
{
    ASSERT(node->child1().useKind() == CellUse);
    SpeculateCellOperand base(this, node->child1());
    SpeculateCellOperand brandValue(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg brandGPR = brandValue.gpr();

    speculateSymbol(node->child2(), brandGPR);

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();

    JumpList slowCases;
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITPrivateBrandAccessGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::SetPrivateBrand, usedRegisters,
        JSValueRegs::payloadOnly(baseGPR), JSValueRegs::payloadOnly(brandGPR), InvalidGPRReg);

    std::visit([&](auto* stubInfo) {
        stubInfo->propertyIsSymbol = true;
    }, stubInfo);

    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(
        slowCases, this, operationSetPrivateBrandOptimize, NoResult,
        CellValue(baseGPR), CellValue(brandGPR), TrustedImmPtr(gen.stubInfo()));

    addPrivateBrandAccess(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    noResult(node);
}

void SpeculativeJIT::compileInstanceOfForCells(Node* node, JSValueRegs valueRegs, JSValueRegs prototypeRegs, GPRReg resultGPR, GPRReg stubInfoGPR, Jump slowCase)
{
    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSiteIndex = addCallSite(codeOrigin);

    bool prototypeIsKnownObject = m_state.forNode(node->child2()).isType(SpecObject | ~SpecCell);
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITInstanceOfGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, node->origin.semantic, callSiteIndex, usedRegisters, resultGPR,
        valueRegs.payloadGPR(), prototypeRegs.payloadGPR(), stubInfoGPR, prototypeIsKnownObject);
    JumpList slowCases;
    slowCases.append(slowCase);

    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(slowCases, this, operationInstanceOfOptimize, resultGPR, valueRegs, prototypeRegs, TrustedImmPtr(gen.stubInfo()));

    addInstanceOf(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::compileInstanceOf(Node* node)
{
    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse);
    DFG_ASSERT(m_graph, node, node->child2().useKind() == UntypedUse);

    JSValueOperand value(this, node->child1());
    JSValueOperand prototype(this, node->child2());

    GPRTemporary result(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    JSValueRegs prototypeRegs = prototype.jsValueRegs();

    GPRReg resultGPR = result.gpr();

    Jump isCell = branchIfCell(valueRegs);
    moveFalseTo(resultGPR);

    Jump done = jump();

    isCell.link(this);

    Jump slowCase = branchIfNotCell(prototypeRegs);

    compileInstanceOfForCells(node, valueRegs, prototypeRegs, resultGPR, InvalidGPRReg, slowCase);

    done.link(this);
    blessedBooleanResult(resultGPR, node);
    return;
}

void SpeculativeJIT::compilePutByIdFlush(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratchGPR = scratch.gpr();
    flushRegisters();

    cachedPutById(node, node->origin.semantic, baseGPR, valueRegs, InvalidGPRReg, scratchGPR, InvalidGPRReg, node->cacheableIdentifier(), node->ecmaMode().isStrict() ? AccessType::PutByIdStrict : AccessType::PutByIdSloppy, Jump(), DontSpill);

    noResult(node);
}

void SpeculativeJIT::compilePutById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratchGPR = scratch.gpr();

    cachedPutById(node, node->origin.semantic, baseGPR, valueRegs, InvalidGPRReg, scratchGPR, InvalidGPRReg, node->cacheableIdentifier(), node->ecmaMode().isStrict() ? AccessType::PutByIdStrict : AccessType::PutByIdSloppy);

    noResult(node);
}

void SpeculativeJIT::compilePutByIdDirect(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratchGPR = scratch.gpr();

    cachedPutById(node, node->origin.semantic, baseGPR, valueRegs, InvalidGPRReg, scratchGPR, InvalidGPRReg, node->cacheableIdentifier(), node->ecmaMode().isStrict() ? AccessType::PutByIdDirectStrict : AccessType::PutByIdDirectSloppy);

    noResult(node);
}

void SpeculativeJIT::cachedPutById(Node* node, CodeOrigin codeOrigin, GPRReg baseGPR, JSValueRegs valueRegs, GPRReg stubInfoGPR, GPRReg scratchGPR, GPRReg scratch2GPR, CacheableIdentifier identifier, AccessType accessType, Jump slowPathTarget, SpillRegistersMode spillMode)
{
    UNUSED_PARAM(node);
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    if (spillMode == DontSpill) {
        // We've already flushed registers to the stack, we don't need to spill these.
        usedRegisters.remove(baseGPR);
        usedRegisters.remove(valueRegs);
        if (stubInfoGPR != InvalidGPRReg)
            usedRegisters.remove(stubInfoGPR);
        if (scratchGPR != InvalidGPRReg)
            usedRegisters.remove(scratchGPR);
        if (scratch2GPR != InvalidGPRReg)
            usedRegisters.remove(scratch2GPR);
    }
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITPutByIdGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        JSValueRegs::payloadOnly(baseGPR), valueRegs, stubInfoGPR,
        scratchGPR, accessType);

    JumpList slowCases;
    if (slowPathTarget.isSet())
        slowCases.append(slowPathTarget);

    auto* operation = appropriatePutByIdOptimizeFunction(accessType);
    gen.generateFastPath(*this);
    slowCases.append(gen.slowPathJump());
    auto slowPath = slowPathCall(slowCases, this, operation, NoResult, valueRegs, CellValue(baseGPR), TrustedImmPtr(gen.stubInfo()));

    addPutById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

#endif

} } // namespace JSC::DFG

#endif
