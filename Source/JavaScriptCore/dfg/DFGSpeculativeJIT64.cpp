/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#include "AtomicsObject.h"
#include "CallFrameShuffler.h"
#include "ClonedArguments.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGDoesGC.h"
#include "DFGOperations.h"
#include "DFGSlowPathGenerator.h"
#include "DateInstance.h"
#include "HasOwnPropertyCache.h"
#include "MegamorphicCache.h"
#include "SetupVarargsFrame.h"
#include "SpillRegistersMode.h"
#include "StructureChain.h"
#include "SuperSampler.h"
#include "YarrJITRegisters.h"

namespace JSC { namespace DFG {

#if USE(JSVALUE64)

void SpeculativeJIT::boxInt52(GPRReg sourceGPR, GPRReg targetGPR, DataFormat format)
{
    GPRReg tempGPR;
    if (sourceGPR == targetGPR)
        tempGPR = allocate();
    else
        tempGPR = targetGPR;
    
    FPRReg fpr = fprAllocate();

    if (format == DataFormatInt52)
        rshift64(TrustedImm32(JSValue::int52ShiftAmount), sourceGPR);
    else
        ASSERT(format == DataFormatStrictInt52);
    
    boxInt52(sourceGPR, targetGPR, tempGPR, fpr);
    
    if (format == DataFormatInt52 && sourceGPR != targetGPR)
        lshift64(TrustedImm32(JSValue::int52ShiftAmount), sourceGPR);
    
    if (tempGPR != targetGPR)
        unlock(tempGPR);
    
    unlock(fpr);
}

GPRReg SpeculativeJIT::fillJSValue(Edge edge)
{
    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
    
    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (edge->hasConstant()) {
            JSValue jsValue = edge->asJSValue();
            move(TrustedImm64(JSValue::encode(jsValue)), gpr);
            info.fillJSValue(m_stream, gpr, DataFormatJS);
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
        } else {
            DataFormat spillFormat = info.spillFormat();
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            switch (spillFormat) {
            case DataFormatInt32: {
                load32(addressFor(virtualRegister), gpr);
                or64(GPRInfo::numberTagRegister, gpr);
                spillFormat = DataFormatJSInt32;
                break;
            }
                
            default:
                load64(addressFor(virtualRegister), gpr);
                DFG_ASSERT(m_graph, m_currentNode, spillFormat & DataFormatJS, spillFormat);
                break;
            }
            info.fillJSValue(m_stream, gpr, spillFormat);
        }
        return gpr;
    }

    case DataFormatInt32: {
        GPRReg gpr = info.gpr();
        // If the register has already been locked we need to take a copy.
        // If not, we'll zero extend in place, so mark on the info that this is now type DataFormatInt32, not DataFormatJSInt32.
        if (m_gprs.isLocked(gpr)) {
            GPRReg result = allocate();
            or64(GPRInfo::numberTagRegister, gpr, result);
            return result;
        }
        m_gprs.lock(gpr);
        or64(GPRInfo::numberTagRegister, gpr);
        info.fillJSValue(m_stream, gpr, DataFormatJSInt32);
        return gpr;
    }

    case DataFormatCell:
        // No retag required on JSVALUE64!
    case DataFormatJS:
    case DataFormatJSInt32:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatJSBoolean:
    case DataFormatJSBigInt32: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }
        
    case DataFormatBoolean:
    case DataFormatStorage:
    case DataFormatDouble:
    case DataFormatInt52:
        // this type currently never occurs
        DFG_CRASH(m_graph, m_currentNode, "Bad data format");
        
    default:
        DFG_CRASH(m_graph, m_currentNode, "Corrupt data format");
        return InvalidGPRReg;
    }
}

void SpeculativeJIT::cachedGetById(Node* node, CodeOrigin origin, JSValueRegs base, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratchGPR, CacheableIdentifier identifier, Jump slowPathTarget , SpillRegistersMode mode, AccessType type)
{
    cachedGetById(node, origin, base.gpr(), result.gpr(), stubInfoGPR, scratchGPR, identifier, slowPathTarget, mode, type);
}

void SpeculativeJIT::cachedGetById(Node* node, CodeOrigin codeOrigin, GPRReg baseGPR, GPRReg resultGPR, GPRReg stubInfoGPR, GPRReg scratchGPR, CacheableIdentifier identifier, Jump slowPathTarget, SpillRegistersMode spillMode, AccessType type)
{
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    if (spillMode == DontSpill) {
        // We've already flushed registers to the stack, we don't need to spill these.
        usedRegisters.remove(baseGPR);
        usedRegisters.remove(resultGPR);
        if (stubInfoGPR != InvalidGPRReg)
            usedRegisters.remove(stubInfoGPR);
        if (scratchGPR != InvalidGPRReg)
            usedRegisters.remove(scratchGPR);
    }
    JSValueRegs baseRegs { baseGPR };
    JSValueRegs resultRegs { resultGPR };
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITGetByIdGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        baseRegs, resultRegs, stubInfoGPR, type);
    
    JumpList slowCases;
    slowCases.append(slowPathTarget);

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (m_graph.m_plan.isUnlinked()) {
        gen.generateDFGDataICFastPath(*this, stubInfoConstant.index(), baseRegs, resultRegs, stubInfoGPR, scratchGPR);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoConstant.index();
        ASSERT(!gen.stubInfo());
        slowPath = slowPathICCall(
            slowCases, this, stubInfoConstant, stubInfoGPR, Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), appropriateOptimizingGetByIdFunction(type),
            spillMode, ExceptionCheckRequirement::CheckNeeded,
            resultGPR, LinkableConstant::globalObject(*this, node), stubInfoGPR, baseGPR, identifier.rawBits());
    } else {
        gen.generateFastPath(*this, scratchGPR);
        slowCases.append(gen.slowPathJump());
        slowPath = slowPathCall(
            slowCases, this, appropriateOptimizingGetByIdFunction(type),
            spillMode, ExceptionCheckRequirement::CheckNeeded,
            resultGPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(gen.stubInfo()), baseGPR, identifier.rawBits());
    }
    
    addGetById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::cachedGetByIdWithThis(Node* node, CodeOrigin codeOrigin, GPRReg baseGPR, GPRReg thisGPR, GPRReg resultGPR, GPRReg stubInfoGPR, GPRReg scratchGPR, CacheableIdentifier identifier, const JumpList& slowPathTarget)
{
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();
    // We've already flushed registers to the stack, we don't need to spill these.
    if (baseGPR != InvalidGPRReg)
        usedRegisters.remove(baseGPR);
    if (thisGPR != InvalidGPRReg)
        usedRegisters.remove(thisGPR);
    if (resultGPR != InvalidGPRReg)
        usedRegisters.remove(resultGPR);
    if (stubInfoGPR != InvalidGPRReg)
        usedRegisters.remove(stubInfoGPR);
    if (scratchGPR != InvalidGPRReg)
        usedRegisters.remove(scratchGPR);
    
    JSValueRegs baseRegs { baseGPR };
    JSValueRegs resultRegs { resultGPR };
    JSValueRegs thisRegs { thisGPR };
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITGetByIdWithThisGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, usedRegisters, identifier,
        resultRegs, baseRegs, thisRegs, stubInfoGPR);
    
    JumpList slowCases;
    slowCases.append(slowPathTarget);

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (m_graph.m_plan.isUnlinked()) {
        gen.generateDFGDataICFastPath(*this, stubInfoConstant.index(), baseRegs, resultRegs, stubInfoGPR, scratchGPR);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoConstant.index();
        slowPath = slowPathICCall(
            slowCases, this, stubInfoConstant, stubInfoGPR, Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), operationGetByIdWithThisOptimize,
            DontSpill, ExceptionCheckRequirement::CheckNeeded,
            resultGPR, LinkableConstant::globalObject(*this, node), stubInfoGPR, baseGPR, thisGPR, identifier.rawBits());
    } else {
        gen.generateFastPath(*this, scratchGPR);
        slowCases.append(gen.slowPathJump());
        slowPath = slowPathCall(
            slowCases, this, operationGetByIdWithThisOptimize,
            DontSpill, ExceptionCheckRequirement::CheckNeeded,
            resultGPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(gen.stubInfo()), baseGPR, thisGPR, identifier.rawBits());
    }
    
    addGetByIdWithThis(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompareNullOrUndefined(Edge operand)
{
    JSValueOperand arg(this, operand, ManualOperandSpeculation);
    GPRReg argGPR = arg.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();

    move(TrustedImm32(0), resultGPR);

    JumpList done;
    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        if (!isKnownNotCell(operand.node()))
            done.append(branchIfCell(JSValueRegs(argGPR)));
    } else {
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        Jump notCell;
        if (!isKnownCell(operand.node()))
            notCell = branchIfNotCell(JSValueRegs(argGPR));
        
        Jump isNotMasqueradesAsUndefined = branchTest8(
            Zero,
            Address(argGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(MasqueradesAsUndefined));
        done.append(isNotMasqueradesAsUndefined);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();
        loadLinkableConstant(LinkableConstant::globalObject(*this, m_currentNode), localGlobalObjectGPR);
        emitLoadStructure(vm(), argGPR, resultGPR);
        loadPtr(Address(resultGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        comparePtr(Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, resultGPR);
        done.append(jump());
        if (!isKnownCell(operand.node()))
            notCell.link(this);
    }
 
    if (!isKnownNotOther(operand.node())) {
        move(argGPR, resultGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), resultGPR);
        compare64(Equal, resultGPR, TrustedImm32(JSValue::ValueNull), resultGPR);
    }

    done.link(this);
 
    or32(TrustedImm32(JSValue::ValueFalse), resultGPR);
    jsValueResult(resultGPR, m_currentNode, DataFormatJSBoolean);
}

void SpeculativeJIT::nonSpeculativePeepholeBranchNullOrUndefined(Edge operand, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    JSValueOperand arg(this, operand, ManualOperandSpeculation);
    GPRReg argGPR = arg.gpr();
    
    GPRTemporary result(this, Reuse, arg);
    GPRReg resultGPR = result.gpr();

    // First, handle the case where "operand" is a cell.
    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        if (!isKnownNotCell(operand.node())) {
            Jump isCell = branchIfCell(JSValueRegs(argGPR));
            addBranch(isCell, notTaken);
        }
    } else {
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
        GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();

        Jump notCell;
        if (!isKnownCell(operand.node()))
            notCell = branchIfNotCell(JSValueRegs(argGPR));
        
        branchTest8(Zero,
            Address(argGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(MasqueradesAsUndefined), notTaken);

        loadLinkableConstant(LinkableConstant::globalObject(*this, m_currentNode), localGlobalObjectGPR);
        emitLoadStructure(vm(), argGPR, resultGPR);
        loadPtr(Address(resultGPR, Structure::globalObjectOffset()), remoteGlobalObjectGPR);
        branchPtr(Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, taken);

        if (!isKnownCell(operand.node())) {
            jump(notTaken, ForceJump);
            notCell.link(this);
        }
    }

    if (isKnownNotOther(operand.node()))
        jump(notTaken);
    else {
        RelationalCondition condition = Equal;
        if (taken == nextBlock()) {
            condition = NotEqual;
            std::swap(taken, notTaken);
        }
        move(argGPR, resultGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), resultGPR);
        branch64(condition, resultGPR, TrustedImm64(JSValue::ValueNull), taken);
        jump(notTaken);
    }
}

void SpeculativeJIT::compileNeitherDoubleNorHeapBigIntToNotDoubleStrictEquality(Node* node, Edge leftNeitherDoubleNorHeapBigIntChild, Edge rightNotDoubleChild)
{
    ASSERT(leftNeitherDoubleNorHeapBigIntChild.useKind() == NeitherDoubleNorHeapBigIntUse);
    ASSERT(rightNotDoubleChild.useKind() == NotDoubleUse);

    JSValueOperand left(this, leftNeitherDoubleNorHeapBigIntChild, ManualOperandSpeculation);
    JSValueOperand right(this, rightNotDoubleChild, ManualOperandSpeculation);

    GPRTemporary temp(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRTemporary leftTemp2(this, Reuse, left);
    GPRTemporary rightTemp2(this, Reuse, right);
    JSValueRegs leftRegs = left.jsValueRegs();
    JSValueRegs rightRegs = right.jsValueRegs();

    GPRReg tempGPR = temp.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftTemp2GPR = leftTemp2.gpr();
    GPRReg rightTemp2GPR = rightTemp2.gpr();

    // We try to avoid repeated and redundant checks here, which leads to the following pseudo-code:
    /*
     if (left == right) {
       speculateNeitherDoubleNorHeapBigInt(left);
       return true;
     }
     speculateNotDouble(right);
     speculateNotDouble(left);
     if (left is not Cell)
       return false;
     Check that left is not HeapBigInt;
     if (left is not String)
       return false;
     if (right is not Cell)
       return false;
     if (right is not String)
       return false;
     return stringEquality(left, right)
     }
     */
    // Mercifully we can often eliminate much of this soup of conditionals with the help of the abstract interpreter.

    JumpList trueCase;
    JumpList falseCase;

    Jump notEqual = branch64(NotEqual, leftRegs.payloadGPR(), rightRegs.payloadGPR());
    // We cannot use speculateNeitherDoubleNorHeapBigInt here, because it updates the interpreter state, and we can skip over it.
    // So we would always skip the speculateNotDouble that follows.
    if (needsTypeCheck(leftNeitherDoubleNorHeapBigIntChild, ~SpecFullDouble)) {
        if (needsTypeCheck(leftNeitherDoubleNorHeapBigIntChild, ~SpecInt32Only))
            trueCase.append(branchIfInt32(leftRegs));
        speculationCheck(BadType, leftRegs, leftNeitherDoubleNorHeapBigIntChild.node(), branchIfNumber(leftRegs, tempGPR));
    }
    if (needsTypeCheck(leftNeitherDoubleNorHeapBigIntChild, ~SpecHeapBigInt)) {
        if (needsTypeCheck(leftNeitherDoubleNorHeapBigIntChild, SpecCell))
            trueCase.append(branchIfNotCell(leftRegs));
        speculationCheck(BadType, leftRegs, leftNeitherDoubleNorHeapBigIntChild.node(), branchIfHeapBigInt(leftRegs.payloadGPR()));
    }
    trueCase.append(jump());
    notEqual.link(this);

    speculateNotDouble(rightNotDoubleChild, rightRegs, tempGPR);
    speculateNotDouble(leftNeitherDoubleNorHeapBigIntChild, leftRegs, tempGPR);

    if (needsTypeCheck(leftNeitherDoubleNorHeapBigIntChild, SpecCellCheck))
        falseCase.append(branchIfNotCell(leftRegs));

    DFG_TYPE_CHECK(leftRegs, leftNeitherDoubleNorHeapBigIntChild, ~SpecHeapBigInt, branchIfHeapBigInt(leftRegs.payloadGPR()));

    if (needsTypeCheck(leftNeitherDoubleNorHeapBigIntChild, SpecString))
        falseCase.append(branchIfNotString(leftRegs.payloadGPR()));

    if (needsTypeCheck(rightNotDoubleChild, SpecCellCheck))
        falseCase.append(branchIfNotCell(rightRegs));

    if (needsTypeCheck(rightNotDoubleChild, SpecString))
        falseCase.append(branchIfNotString(rightRegs.payloadGPR()));

    compileStringEquality(node, leftRegs.payloadGPR(), rightRegs.payloadGPR(), tempGPR, leftTempGPR, rightTempGPR, leftTemp2GPR, rightTemp2GPR, trueCase, falseCase);
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
    
    ASSERT(node->isBinaryUseKind(UntypedUse) || node->isBinaryUseKind(AnyBigIntUse));
    JSValueOperand arg1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand arg2(this, node->child2(), ManualOperandSpeculation);
    speculate(node, node->child1());
    speculate(node, node->child2());
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node->child1().node()) && isKnownCell(node->child2().node())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        branch64(Equal, arg1GPR, arg2GPR, invert ? notTaken : taken);
        
        silentSpillAllRegisters(resultGPR);
        callOperation(operationCompareStrictEqCell, resultGPR, LinkableConstant::globalObject(*this, node), arg1GPR, arg2GPR);
        silentFillAllRegisters();
        exceptionCheck();
        
        branchTest32(invert ? Zero : NonZero, resultGPR, taken);
    } else {
        /* At a high level we do (assuming 'invert' to be false):
         If (left is Double || right is Double)
            goto slowPath;
         if (left == right)
            goto taken;
         if (left is Cell || right is Cell)
            goto slowPath;
         goto notTaken;
         */

        JumpList slowPathCases;

        // This fragment implements (left is Double || right is Double), with a single branch instead of the 4 that would be naively required if we used branchIfInt32/branchIfNumber
        // The trick is that if a JSValue is an Int32, then adding 1<<49 to it will make it overflow, leaving all high bits at 0
        // If it is not a number at all, then 1<<49 will be its only high bit set
        // Leaving only doubles above or equal 1<<50.
        GPRTemporary scratch(this);
        move(arg1GPR, resultGPR);
        move(arg2GPR, scratch.gpr());
        add64(TrustedImm64(JSValue::LowestOfHighBits), resultGPR);
        add64(TrustedImm64(JSValue::LowestOfHighBits), scratch.gpr());
        or64(scratch.gpr(), resultGPR, resultGPR);
        constexpr uint64_t nextLowestOfHighBits = JSValue::LowestOfHighBits << 1;
        slowPathCases.append(branch64(AboveOrEqual, resultGPR, TrustedImm64(nextLowestOfHighBits)));

        branch64(Equal, arg1GPR, arg2GPR, invert ? notTaken : taken);
        
        // If we support BigInt32 we must go to a slow path if at least one operand is a cell (for HeapBigInt === BigInt32)
        // If we don't support BigInt32, we only have to go to a slow path if both operands are cells (for HeapBigInt === HeapBigInt and String === String)
        // Instead of doing two branches, we can do a single one, by observing that
        // 1. (left is Cell && right is Cell) is the same as ((left | right) is Cell)
        //      Both are "All high bits are 0"
        // 2. Since we know that neither is a double, (left is Cell || right is Cell) is equivalent to ((left & right) is Cell)
        //      If both are Int32, then the top bits will be set and the test will fail
        //      If at least one is not Int32, then the top bits will be 0.
        //      And if at least one is a cell, then the 'Other' tag will also be 0, making the test succeed
#if USE(BIGINT32)
        and64(arg1GPR, arg2GPR, resultGPR);
#else
        or64(arg1GPR, arg2GPR, resultGPR);
#endif
        slowPathCases.append(branchIfCell(resultGPR));

        jump(invert ? taken : notTaken, ForceJump);

        addSlowPathGenerator(slowPathCall(slowPathCases, this, operationCompareStrictEq, resultGPR, LinkableConstant::globalObject(*this, node), arg1GPR, arg2GPR));
        branchTest32(invert ? Zero : NonZero, resultGPR, taken);
    }

    jump(notTaken);
}

void SpeculativeJIT::genericJSValueNonPeepholeStrictEq(Node* node, bool invert)
{
    // FIXME: some of this code should be shareable with nonSpeculativePeepholeStrictEq
    JSValueOperand arg1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand arg2(this, node->child2(), ManualOperandSpeculation);
    speculate(node, node->child1());
    speculate(node, node->child2());
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();
    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    arg1.use();
    arg2.use();
    
    if (isKnownCell(node->child1().node()) && isKnownCell(node->child2().node())) {
        // see if we get lucky: if the arguments are cells and they reference the same
        // cell, then they must be strictly equal.
        // FIXME: this should flush registers instead of silent spill/fill.
        Jump notEqualCase = branch64(NotEqual, arg1Regs.gpr(), arg2Regs.gpr());
        
        move(TrustedImm64(!invert), resultGPR);
        
        Jump done = jump();

        notEqualCase.link(this);
        
        silentSpillAllRegisters(resultGPR);
        callOperation(operationCompareStrictEqCell, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);
        silentFillAllRegisters();
        exceptionCheck();
        
        done.link(this);
        unblessedBooleanResult(resultGPR, m_currentNode, UseChildrenCalledExplicitly);
        return;
    }
    /* At a high level we do (assuming 'invert' to be false):
    If (left is Double || right is Double)
        goto slowPath;
    result = (left == right);
    if (result)
        goto done;
    if (left is Cell || right is Cell)
        goto slowPath;
    done:
    return result;
    */

    JumpList slowPathCases;

    // This fragment implements (left is Double || right is Double), with a single branch instead of the 4 that would be naively required if we used branchIfInt32/branchIfNumber
    // The trick is that if a JSValue is an Int32, then adding 1<<49 to it will make it overflow, leaving all high bits at 0
    // If it is not a number at all, then 1<<49 will be its only high bit set
    // Leaving only doubles above or equal 1<<50.
    GPRTemporary scratch(this);
    move(arg1GPR, resultGPR);
    move(arg2GPR, scratch.gpr());
    add64(TrustedImm64(JSValue::LowestOfHighBits), resultGPR);
    add64(TrustedImm64(JSValue::LowestOfHighBits), scratch.gpr());
    or64(scratch.gpr(), resultGPR, resultGPR);
    constexpr uint64_t nextLowestOfHighBits = JSValue::LowestOfHighBits << 1;
    slowPathCases.append(branch64(AboveOrEqual, resultGPR, TrustedImm64(nextLowestOfHighBits)));

    compare64(Equal, arg1GPR, arg2GPR, resultGPR);
    Jump done = branchTest64(NonZero, resultGPR);

    // If we support BigInt32 we must go to a slow path if at least one operand is a cell (for HeapBigInt === BigInt32)
    // If we don't support BigInt32, we only have to go to a slow path if both operands are cells (for HeapBigInt === HeapBigInt and String === String)
    // Instead of doing two branches, we can do a single one, by observing that
    // 1. (left is Cell && right is Cell) is the same as ((left | right) is Cell)
    //      Both are "All high bits are 0"
    // 2. Since we know that neither is a double, (left is Cell || right is Cell) is equivalent to ((left & right) is Cell)
    //      If both are Int32, then the top bits will be set and the test will fail
    //      If at least one is not Int32, then the top bits will be 0.
    //      And if at least one is a cell, then the 'Other' tag will also be 0, making the test succeed
#if USE(BIGINT32)
    and64(arg1GPR, arg2GPR, resultGPR);
#else
    or64(arg1GPR, arg2GPR, resultGPR);
#endif
    slowPathCases.append(branchIfCell(resultGPR));

    move(TrustedImm64(0), resultGPR);

    addSlowPathGenerator(slowPathCall(slowPathCases, this, operationCompareStrictEq, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs));

    done.link(this);

    xor64(TrustedImm64(invert), resultGPR);

    unblessedBooleanResult(resultGPR, m_currentNode, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitCall(Node* node)
{
    CallLinkInfo::CallType callType;
    bool isVarargs = false;
    bool isForwardVarargs = false;
    bool isTail = false;
    bool isEmulatedTail = false;
    bool isDirect = false;
    switch (node->op()) {
    case DFG::Call:
        callType = CallLinkInfo::Call;
        break;
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
    case ConstructForwardVarargs:
        callType = CallLinkInfo::ConstructVarargs;
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

    GPRReg calleeGPR = InvalidGPRReg;
    GPRReg callLinkInfoGPR = InvalidGPRReg;
    GPRReg globalObjectGPR = InvalidGPRReg;
    CallFrameShuffleData shuffleData;
    
    ExecutableBase* executable = nullptr;
    FunctionExecutable* functionExecutable = nullptr;
    if (isDirect) {
        executable = node->castOperand<ExecutableBase*>();
        functionExecutable = jsDynamicCast<FunctionExecutable*>(executable);
    }
    
    unsigned numPassedArgs = 0;
    unsigned numAllocatedArgs = 0;

    auto [ callLinkInfo, callLinkInfoConstant ] = addCallLinkInfo(m_currentNode->origin.semantic);

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
            exceptionCheck();
            abortWithReason(DFGVarargsThrowingPathDidNotThrow);
            done.link(this);
        } else {
            GPRReg argumentsGPR;
            GPRReg scratchGPR1;
            GPRReg scratchGPR2;
            GPRReg scratchGPR3;
            
            auto loadArgumentsGPR = [&] (GPRReg reservedGPR) {
                if (reservedGPR != InvalidGPRReg)
                    lock(reservedGPR);
                JSValueOperand arguments(this, node->child3());
                argumentsGPR = arguments.gpr();
                if (reservedGPR != InvalidGPRReg)
                    unlock(reservedGPR);
                flushRegisters();
                
                scratchGPR1 = selectScratchGPR(argumentsGPR, reservedGPR);
                scratchGPR2 = selectScratchGPR(argumentsGPR, scratchGPR1, reservedGPR);
                scratchGPR3 = selectScratchGPR(argumentsGPR, scratchGPR1, scratchGPR2, reservedGPR);
            };
            
            loadArgumentsGPR(InvalidGPRReg);
            
            DFG_ASSERT(m_graph, node, isFlushed());
            
            // Right now, arguments is in argumentsGPR and the register file is flushed.
            callOperation(operationSizeFrameForVarargs, GPRInfo::returnValueGPR, LinkableConstant::globalObject(*this, node), argumentsGPR, numUsedStackSlots, data->firstVarArgOffset);
            exceptionCheck();
            
            // Now we have the argument count of the callee frame, but we've lost the arguments operand.
            // Reconstruct the arguments operand while preserving the callee frame.
            loadArgumentsGPR(GPRInfo::returnValueGPR);
            move(TrustedImm32(numUsedStackSlots), scratchGPR1);
            emitSetVarargsFrame(*this, GPRInfo::returnValueGPR, false, scratchGPR1, scratchGPR1);
            addPtr(TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(void*)))), scratchGPR1, stackPointerRegister);
            
            callOperation(operationSetupVarargsFrame, GPRInfo::returnValueGPR, LinkableConstant::globalObject(*this, node), scratchGPR1, argumentsGPR, data->firstVarArgOffset, GPRInfo::returnValueGPR);
            exceptionCheck();
            addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::returnValueGPR, stackPointerRegister);
        }
        
        DFG_ASSERT(m_graph, node, isFlushed());
        
        // We don't need the arguments array anymore.
        if (isVarargs)
            use(node->child3());

        // Now set up the "this" argument.
        JSValueOperand thisArgument(this, node->child2());
        GPRReg thisArgumentGPR = thisArgument.gpr();
        thisArgument.use();
        
        store64(thisArgumentGPR, calleeArgumentSlot(0));
    } else {
        // The call instruction's first child is the function; the subsequent children are the
        // arguments.
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
            Edge calleeEdge = m_graph.child(node, 0);
            JSValueOperand callee(this, calleeEdge);
            calleeGPR = callee.gpr();

            // callLinkInfoGPR/globalObjectGPR must be non callee-save register. Otherwise, tail-call preparation will fill it
            // with saved callee-save. Also, it should not be the same to calleeGPR and regT0 since both will
            // be used later differently.
            // We also do not keep GPRTemporary (it is immediately destroyed) because
            // 1. We do not want to keep the register locked in the following sequence of the Call.
            // 2. This must be the last register allocation from DFG register bank, so it is OK (otherwise, callee.use() is wrong).
            if (!isDirect) {
                std::optional<GPRTemporary> callLinkInfoTemp;
                std::optional<GPRTemporary> globalObjectTemp;
                if (m_graph.m_plan.isUnlinked()) {
                    callLinkInfoTemp.emplace(this, selectScratchGPR(calleeGPR, GPRInfo::regT0, GPRInfo::regT3));
                    callLinkInfoGPR = callLinkInfoTemp->gpr();
                }
                // Regardless of whether we are using DataIC, we need globalObjectGPR.
                if (node->op() == TailCall) {
                    globalObjectTemp.emplace(this, selectScratchGPR(calleeGPR, GPRInfo::regT0, callLinkInfoGPR));
                    globalObjectGPR = globalObjectTemp->gpr();
                }
            }
            if (!isDirect)
                callee.use();

            shuffleData.numberTagRegister = GPRInfo::numberTagRegister;
            shuffleData.numLocals = m_graph.frameRegisterCount();
            shuffleData.callee = ValueRecovery::inGPR(calleeGPR, DataFormatJS);
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

            if (m_graph.m_plan.isUnlinked())
                shuffleData.registers[callLinkInfoGPR] = ValueRecovery::inGPR(callLinkInfoGPR, DataFormatJS);
            shuffleData.setupCalleeSaveRegisters(&RegisterAtOffsetList::dfgCalleeSaveRegisters());
        } else {
            store32(TrustedImm32(numPassedArgs), calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis));

            for (unsigned i = 0; i < numPassedArgs; i++) {
                Edge argEdge = m_graph.m_varArgChildren[node->firstChild() + 1 + i];
                JSValueOperand arg(this, argEdge);
                GPRReg argGPR = arg.gpr();
                use(argEdge);
                
                store64(argGPR, calleeArgumentSlot(i));
            }
            
            for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                storeTrustedValue(jsUndefined(), calleeArgumentSlot(i));
        }
    }
    
    GPRReg evalScopeGPR = InvalidGPRReg;
    GPRReg evalThisValueGPR = InvalidGPRReg;
    if (!isTail || isVarargs || isForwardVarargs) {
        Edge calleeEdge = m_graph.child(node, 0);
        JSValueOperand callee(this, calleeEdge);
        calleeGPR = callee.gpr();

        std::optional<SpeculateCellOperand> scope;
        std::optional<JSValueOperand> thisValue;
        if (node->op() == CallDirectEval) {
            Edge scopeEdge = m_graph.m_varArgChildren[node->firstChild() + node->numChildren() - 1];
            Edge thisValueEdge = m_graph.m_varArgChildren[node->firstChild() + node->numChildren() - 2];
            scope.emplace(this, scopeEdge);
            thisValue.emplace(this, thisValueEdge);
            evalScopeGPR = scope->gpr();
            evalThisValueGPR = thisValue->gpr();
        }

        // callLinkInfoGPR must be non callee-save register. Otherwise, tail-call preparation will fill it
        // with saved callee-save. Also, it should not be the same to calleeGPR and regT0 since both will
        // be used later differently.
        // We also do not keep GPRTemporary (it is immediately destroyed) because
        // 1. We do not want to keep the register locked in the following sequence of the Call.
        // 2. This must be the last register allocation from DFG register bank, so it is OK (otherwise, callee.use() is wrong).
        if (m_graph.m_plan.isUnlinked()) {
            GPRTemporary callLinkInfoTemp(this, selectScratchGPR(calleeGPR, GPRInfo::regT0, GPRInfo::regT3));
            callLinkInfoGPR = callLinkInfoTemp.gpr();
        }

        callee.use();
        if (scope)
            scope->use();
        if (thisValue)
            thisValue->use();
        store64(calleeGPR, calleeFrameSlot(CallFrameSlot::callee));

        flushRegisters();
    }

    CodeOrigin staticOrigin = node->origin.semantic;
    InlineCallFrame* staticInlineCallFrame = staticOrigin.inlineCallFrame();
    ASSERT(!isTail || !staticInlineCallFrame || !staticInlineCallFrame->getCallerSkippingTailCalls());
    ASSERT(!isEmulatedTail || (staticInlineCallFrame && staticInlineCallFrame->getCallerSkippingTailCalls()));
    CodeOrigin dynamicOrigin =
        isEmulatedTail ? *staticInlineCallFrame->getCallerSkippingTailCalls() : staticOrigin;

    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(dynamicOrigin, m_stream.size());
    
    auto setResultAndResetStack = [&] () {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        move(GPRInfo::returnValueGPR, resultGPR);

        jsValueResult(resultGPR, m_currentNode, DataFormatJS, UseChildrenCalledExplicitly);

        // After the calls are done, we need to reestablish our stack
        // pointer. We rely on this for varargs calls, calls with arity
        // mismatch (the callframe is slided) and tail calls.
        addPtr(TrustedImm32(m_graph.stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, stackPointerRegister);
    };
    
    std::visit([&](auto* callLinkInfo) {
        callLinkInfo->setUpCall(callType, calleeGPR);
    }, callLinkInfo);

    if (node->op() == CallDirectEval) {
        // We want to call operationCallDirectEvalSloppy/operationCallDirectEvalStrict but we don't want to overwrite the parameter area in
        // which we have created a prototypical eval call frame. This means that we have to
        // subtract stack to make room for the call. Lucky for us, at this point we have the whole
        // register file to ourselves.
        
        GPRReg calleeFrameGPR = selectScratchGPR(evalScopeGPR, evalThisValueGPR);
        emitStoreCallSiteIndex(callSite);
        addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, calleeFrameGPR);
        storePtr(GPRInfo::callFrameRegister, Address(calleeFrameGPR, CallFrame::callerFrameOffset()));
        
        // Now we need to make room for:
        // - The caller frame and PC of a call to operationCallDirectEvalSloppy/operationCallDirectEvalStrict.
        // - Potentially two arguments on the stack.
        unsigned requiredBytes = sizeof(CallerFrameAndPC) + sizeof(CallFrame*) * 2;
        requiredBytes = WTF::roundUpToMultipleOf(stackAlignmentBytes(), requiredBytes);
        subPtr(TrustedImm32(requiredBytes), stackPointerRegister);
        setupArguments<decltype(operationCallDirectEvalSloppy)>(calleeFrameGPR, evalScopeGPR, evalThisValueGPR);
        prepareForExternalCall();
        appendCall(node->ecmaMode().isStrict() ? operationCallDirectEvalStrict : operationCallDirectEvalSloppy);
        exceptionCheck();
        Jump done = branchIfNotEmpty(GPRInfo::returnValueGPR);
        
        // This is the part where we meant to make a normal call. Oops.
        addPtr(TrustedImm32(requiredBytes), stackPointerRegister);
        load64(calleeFrameSlot(CallFrameSlot::callee), GPRInfo::regT0);
        loadLinkableConstant(LinkableConstant::globalObject(*this, node), GPRInfo::regT3);
        loadLinkableConstant(callLinkInfoConstant, GPRInfo::regT2);
        emitVirtualCallWithoutMovingGlobalObject(vm(), GPRInfo::regT2, CallMode::Regular);
        
        done.link(this);
        setResultAndResetStack();
        return;
    }
    
    if (isDirect) {
        ASSERT(!m_graph.m_plan.isUnlinked());
        auto* linkedCallLinkInfo = std::get<OptimizingCallLinkInfo*>(callLinkInfo);
        linkedCallLinkInfo->setExecutableDuringCompilation(executable);
        linkedCallLinkInfo->setDirectCallMaxArgumentCountIncludingThis(numAllocatedArgs);

        if (isTail) {
            RELEASE_ASSERT(node->op() == DirectTailCall);
            
            Label mainPath = label();
            emitStoreCallSiteIndex(callSite);

            linkedCallLinkInfo->emitDirectTailCallFastPath(*this, scopedLambda<void()>([&]{
                linkedCallLinkInfo->setFrameShuffleData(shuffleData);
                CallFrameShuffler(*this, shuffleData).prepareForTailCall();
            }));
            Label slowPath = label();
            
            silentSpillAllRegisters(InvalidGPRReg);
            callOperation(operationLinkDirectCall, TrustedImmPtr(linkedCallLinkInfo), calleeGPR);
            silentFillAllRegisters();
            exceptionCheck();
            jump().linkTo(mainPath, this);
            
            useChildren(node);
            
            addJSDirectCall(slowPath, linkedCallLinkInfo);
            return;
        }
        
        Label mainPath = label();
        emitStoreCallSiteIndex(callSite);
        linkedCallLinkInfo->emitDirectFastPath(*this);
        Jump done = jump();
        
        Label slowPath = label();
        if (isX86())
            pop(selectScratchGPR(calleeGPR));

        callOperation(operationLinkDirectCall, TrustedImmPtr(linkedCallLinkInfo), calleeGPR);
        exceptionCheck();
        jump().linkTo(mainPath, this);
        
        done.link(this);
        
        setResultAndResetStack();
        
        addJSDirectCall(slowPath, linkedCallLinkInfo);
        return;
    }
    
    emitStoreCallSiteIndex(callSite);
    
    JumpList slowCases;
    std::optional<Jump> done;
    if (m_graph.m_plan.isUnlinked())
        loadLinkableConstant(callLinkInfoConstant, callLinkInfoGPR);
    if (isTail) {
        slowCases = CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, calleeGPR, callLinkInfoGPR, scopedLambda<void()>([&, callLinkInfo = callLinkInfo]{
            if (node->op() == TailCall) {
                std::visit([&](auto* callLinkInfo) {
                    callLinkInfo->setFrameShuffleData(shuffleData);
                }, callLinkInfo);
                CallFrameShuffler(*this, shuffleData).prepareForTailCall();
            } else {
                emitRestoreCalleeSaves();
                prepareForTailCallSlow(callLinkInfoGPR);
            }
        }));
    } else {
        slowCases = CallLinkInfo::emitFastPath(*this, callLinkInfo, calleeGPR, callLinkInfoGPR);
        done = jump();
    }

    slowCases.link(this);
    auto slowPathStart = label();

    if (node->op() == TailCall) {
        loadLinkableConstant(LinkableConstant::globalObject(*this, node), globalObjectGPR);
        shuffleData.registers[GPRInfo::regT3] = ValueRecovery::inGPR(globalObjectGPR, DataFormatJS);
        CallFrameShuffler callFrameShuffler(*this, shuffleData);
        callFrameShuffler.setCalleeJSValueRegs(JSValueRegs(GPRInfo::regT0));
        callFrameShuffler.prepareForSlowPath();
    } else {
        move(calleeGPR, GPRInfo::regT0); // Callee needs to be in regT0
        loadLinkableConstant(LinkableConstant::globalObject(*this, node), GPRInfo::regT3); // JSGlobalObject needs to be in regT3
        if (isTail)
            emitRestoreCalleeSaves(); // This needs to happen after we moved calleeGPR to regT0
    }

    CallLinkInfo::emitSlowPath(vm(), *this, callLinkInfo, callLinkInfoGPR);

    if (done)
        done->link(this);
    auto doneLocation = label();

    if (isTail)
        abortWithReason(JITDidReturnFromTailCall);
    else
        setResultAndResetStack();

    addJSCall(slowPathStart, doneLocation, callLinkInfo);
}

// Clang should allow unreachable [[clang::fallthrough]] in template functions if any template expansion uses it
// http://llvm.org/bugs/show_bug.cgi?id=18619
IGNORE_WARNINGS_BEGIN("implicit-fallthrough")
template<bool strict>
GPRReg SpeculativeJIT::fillSpeculateInt32Internal(Edge edge, DataFormat& returnFormat)
{
    AbstractValue& value = m_state.forNode(edge);
    SpeculatedType type = value.m_type;
    ASSERT(edge.useKind() != KnownInt32Use || !(value.m_type & ~SpecInt32Only));

    m_interpreter.filter(value, SpecInt32Only);
    if (value.isClear()) {
        if (mayHaveTypeCheck(edge.useKind()))
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
        returnFormat = DataFormatInt32;
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (edge->hasConstant()) {
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            ASSERT(edge->isInt32Constant());
            move(Imm32(edge->asInt32()), gpr);
            info.fillInt32(m_stream, gpr);
            returnFormat = DataFormatInt32;
            return gpr;
        }
        
        DataFormat spillFormat = info.spillFormat();
        
        DFG_ASSERT(m_graph, m_currentNode, (spillFormat & DataFormatJS) || spillFormat == DataFormatInt32, spillFormat);
        
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        
        if (spillFormat == DataFormatJSInt32 || spillFormat == DataFormatInt32) {
            // If we know this was spilled as an integer we can fill without checking.
            if (strict) {
                load32(addressFor(virtualRegister), gpr);
                info.fillInt32(m_stream, gpr);
                returnFormat = DataFormatInt32;
                return gpr;
            }
            if (spillFormat == DataFormatInt32) {
                load32(addressFor(virtualRegister), gpr);
                info.fillInt32(m_stream, gpr);
                returnFormat = DataFormatInt32;
            } else {
                load64(addressFor(virtualRegister), gpr);
                info.fillJSValue(m_stream, gpr, DataFormatJSInt32);
                returnFormat = DataFormatJSInt32;
            }
            return gpr;
        }
        load64(addressFor(virtualRegister), gpr);

        // Fill as JSValue, and fall through.
        info.fillJSValue(m_stream, gpr, DataFormatJSInt32);
        m_gprs.unlock(gpr);
        FALLTHROUGH;
    }

    case DataFormatJS: {
        DFG_ASSERT(m_graph, m_currentNode, !(type & SpecInt52Any));
        // Check the value is an integer.
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (type & ~SpecInt32Only)
            speculationCheck(BadType, JSValueRegs(gpr), edge, branchIfNotInt32(gpr));
        info.fillJSValue(m_stream, gpr, DataFormatJSInt32);
        // If !strict we're done, return.
        if (!strict) {
            returnFormat = DataFormatJSInt32;
            return gpr;
        }
        // else fall through & handle as DataFormatJSInt32.
        m_gprs.unlock(gpr);
        FALLTHROUGH;
    }

    case DataFormatJSInt32: {
        // In a strict fill we need to strip off the value tag.
        if (strict) {
            GPRReg gpr = info.gpr();
            GPRReg result;
            // If the register has already been locked we need to take a copy.
            // If not, we'll zero extend in place, so mark on the info that this is now type DataFormatInt32, not DataFormatJSInt32.
            if (m_gprs.isLocked(gpr))
                result = allocate();
            else {
                m_gprs.lock(gpr);
                info.fillInt32(m_stream, gpr);
                result = gpr;
            }
            zeroExtend32ToWord(gpr, result);
            returnFormat = DataFormatInt32;
            return result;
        }

        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        returnFormat = DataFormatJSInt32;
        return gpr;
    }

    case DataFormatInt32: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        returnFormat = DataFormatInt32;
        return gpr;
    }
        
    case DataFormatJSDouble:
    case DataFormatCell:
    case DataFormatBoolean:
    case DataFormatJSCell:
    case DataFormatJSBoolean:
    case DataFormatDouble:
    case DataFormatStorage:
    case DataFormatInt52:
    case DataFormatStrictInt52:
    case DataFormatBigInt32:
    case DataFormatJSBigInt32:
        DFG_CRASH(m_graph, m_currentNode, "Bad data format");
        
    default:
        DFG_CRASH(m_graph, m_currentNode, "Corrupt data format");
        return InvalidGPRReg;
    }
}
IGNORE_WARNINGS_END

GPRReg SpeculativeJIT::fillSpeculateInt32(Edge edge, DataFormat& returnFormat)
{
    return fillSpeculateInt32Internal<false>(edge, returnFormat);
}

GPRReg SpeculativeJIT::fillSpeculateInt32Strict(Edge edge)
{
    DataFormat mustBeDataFormatInt32;
    GPRReg result = fillSpeculateInt32Internal<true>(edge, mustBeDataFormatInt32);
    DFG_ASSERT(m_graph, m_currentNode, mustBeDataFormatInt32 == DataFormatInt32, mustBeDataFormatInt32);
    return result;
}

GPRReg SpeculativeJIT::fillSpeculateInt52(Edge edge, DataFormat desiredFormat)
{
    ASSERT(desiredFormat == DataFormatInt52 || desiredFormat == DataFormatStrictInt52);
    AbstractValue& value = m_state.forNode(edge);

    m_interpreter.filter(value, SpecInt52Any);
    if (value.isClear()) {
        if (mayHaveTypeCheck(edge.useKind()))
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (edge->hasConstant()) {
            JSValue jsValue = edge->asJSValue();
            ASSERT(jsValue.isAnyInt());
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            int64_t value = jsValue.asAnyInt();
            if (desiredFormat == DataFormatInt52)
                value = value << JSValue::int52ShiftAmount;
            move(Imm64(value), gpr);
            info.fillGPR(m_stream, gpr, desiredFormat);
            return gpr;
        }
        
        DataFormat spillFormat = info.spillFormat();
        
        DFG_ASSERT(m_graph, m_currentNode, spillFormat == DataFormatInt52 || spillFormat == DataFormatStrictInt52, spillFormat);
        
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        
        load64(addressFor(virtualRegister), gpr);
        if (desiredFormat == DataFormatStrictInt52) {
            if (spillFormat == DataFormatInt52)
                rshift64(TrustedImm32(JSValue::int52ShiftAmount), gpr);
            info.fillStrictInt52(m_stream, gpr);
            return gpr;
        }
        if (spillFormat == DataFormatStrictInt52)
            lshift64(TrustedImm32(JSValue::int52ShiftAmount), gpr);
        info.fillInt52(m_stream, gpr);
        return gpr;
    }

    case DataFormatStrictInt52: {
        GPRReg gpr = info.gpr();
        bool wasLocked = m_gprs.isLocked(gpr);
        lock(gpr);
        if (desiredFormat == DataFormatStrictInt52)
            return gpr;
        if (wasLocked) {
            GPRReg result = allocate();
            move(gpr, result);
            unlock(gpr);
            gpr = result;
        } else
            info.fillInt52(m_stream, gpr);
        lshift64(TrustedImm32(JSValue::int52ShiftAmount), gpr);
        return gpr;
    }
        
    case DataFormatInt52: {
        GPRReg gpr = info.gpr();
        bool wasLocked = m_gprs.isLocked(gpr);
        lock(gpr);
        if (desiredFormat == DataFormatInt52)
            return gpr;
        if (wasLocked) {
            GPRReg result = allocate();
            move(gpr, result);
            unlock(gpr);
            gpr = result;
        } else
            info.fillStrictInt52(m_stream, gpr);
        rshift64(TrustedImm32(JSValue::int52ShiftAmount), gpr);
        return gpr;
    }

    default:
        DFG_CRASH(m_graph, m_currentNode, "Bad data format");
        return InvalidGPRReg;
    }
}

FPRReg SpeculativeJIT::fillSpeculateDouble(Edge edge)
{
    ASSERT(edge.useKind() == DoubleRepUse || edge.useKind() == DoubleRepRealUse || edge.useKind() == DoubleRepAnyIntUse);
    ASSERT(edge->hasDoubleResult());
    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    if (info.registerFormat() == DataFormatNone) {
        if (edge->hasConstant()) {
            if (edge->isNumberConstant()) {
                FPRReg fpr = fprAllocate();
                int64_t doubleAsInt = reinterpretDoubleToInt64(edge->asNumber());
                if (!doubleAsInt)
                    moveZeroToDouble(fpr);
                else {
                    GPRReg gpr = allocate();
                    move(Imm64(doubleAsInt), gpr);
                    move64ToDouble(gpr, fpr);
                    unlock(gpr);
                }

                m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
                info.fillDouble(m_stream, fpr);
                return fpr;
            }
            if (mayHaveTypeCheck(edge.useKind()))
                terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
            return fprAllocate();
        }
        
        DataFormat spillFormat = info.spillFormat();
        if (spillFormat != DataFormatDouble) {
            DFG_CRASH(
                m_graph, m_currentNode, toCString(
                    "Expected ", edge, " to have double format but instead it is spilled as ",
                    dataFormatToString(spillFormat)).data());
        }
        DFG_ASSERT(m_graph, m_currentNode, spillFormat == DataFormatDouble, spillFormat);
        FPRReg fpr = fprAllocate();
        loadDouble(addressFor(virtualRegister), fpr);
        m_fprs.retain(fpr, virtualRegister, SpillOrderDouble);
        info.fillDouble(m_stream, fpr);
        return fpr;
    }

    DFG_ASSERT(m_graph, m_currentNode, info.registerFormat() == DataFormatDouble, info.registerFormat());
    FPRReg fpr = info.fpr();
    m_fprs.lock(fpr);
    return fpr;
}

GPRReg SpeculativeJIT::fillSpeculateCell(Edge edge)
{
    AbstractValue& value = m_state.forNode(edge);
    SpeculatedType type = value.m_type;
    ASSERT((edge.useKind() != KnownCellUse && edge.useKind() != KnownStringUse) || !(value.m_type & ~SpecCellCheck));

    m_interpreter.filter(value, SpecCellCheck);
    if (value.isClear()) {
        if (mayHaveTypeCheck(edge.useKind()))
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    // FIXME: some of these cases look like they could share code.
    // Look at fillSpeculateInt32Internal for an example.
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (edge->hasConstant()) {
            JSValue jsValue = edge->asJSValue();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            move(TrustedImm64(JSValue::encode(jsValue)), gpr);
            info.fillJSValue(m_stream, gpr, DataFormatJSCell);
            return gpr;
        }

        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        load64(addressFor(virtualRegister), gpr);

        info.fillJSValue(m_stream, gpr, DataFormatJS);
        if (type & ~SpecCellCheck)
            speculationCheck(BadType, JSValueRegs(gpr), edge, branchIfNotCell(JSValueRegs(gpr)));
        info.fillJSValue(m_stream, gpr, DataFormatJSCell);
        return gpr;
    }

    case DataFormatCell:
    case DataFormatJSCell: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (ASSERT_ENABLED) {
            Jump checkCell = branchIfCell(JSValueRegs(gpr));
            abortWithReason(DFGIsNotCell);
            checkCell.link(this);
        }
        return gpr;
    }

    case DataFormatJS: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (type & ~SpecCellCheck)
            speculationCheck(BadType, JSValueRegs(gpr), edge, branchIfNotCell(JSValueRegs(gpr)));
        info.fillJSValue(m_stream, gpr, DataFormatJSCell);
        return gpr;
    }

    case DataFormatJSInt32:
    case DataFormatInt32:
    case DataFormatJSDouble:
    case DataFormatJSBoolean:
    case DataFormatBoolean:
    case DataFormatDouble:
    case DataFormatStorage:
    case DataFormatInt52:
    case DataFormatStrictInt52:
    case DataFormatBigInt32:
    case DataFormatJSBigInt32:
        DFG_CRASH(m_graph, m_currentNode, "Bad data format");
        
    default:
        DFG_CRASH(m_graph, m_currentNode, "Corrupt data format");
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
        if (mayHaveTypeCheck(edge.useKind()))
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (edge->hasConstant()) {
            JSValue jsValue = edge->asJSValue();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            move(TrustedImm64(JSValue::encode(jsValue)), gpr);
            info.fillJSValue(m_stream, gpr, DataFormatJSBoolean);
            return gpr;
        }
        DFG_ASSERT(m_graph, m_currentNode, info.spillFormat() & DataFormatJS, info.spillFormat());
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        load64(addressFor(virtualRegister), gpr);

        info.fillJSValue(m_stream, gpr, DataFormatJS);
        if (type & ~SpecBoolean) {
            xor64(TrustedImm32(JSValue::ValueFalse), gpr);
            speculationCheck(BadType, JSValueRegs(gpr), edge, branchTest64(NonZero, gpr, TrustedImm32(static_cast<int32_t>(~1))), SpeculationRecovery(BooleanSpeculationCheck, gpr, InvalidGPRReg));
            xor64(TrustedImm32(JSValue::ValueFalse), gpr);
        }
        info.fillJSValue(m_stream, gpr, DataFormatJSBoolean);
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
        if (type & ~SpecBoolean) {
            xor64(TrustedImm32(JSValue::ValueFalse), gpr);
            speculationCheck(BadType, JSValueRegs(gpr), edge, branchTest64(NonZero, gpr, TrustedImm32(static_cast<int32_t>(~1))), SpeculationRecovery(BooleanSpeculationCheck, gpr, InvalidGPRReg));
            xor64(TrustedImm32(JSValue::ValueFalse), gpr);
        }
        info.fillJSValue(m_stream, gpr, DataFormatJSBoolean);
        return gpr;
    }

    case DataFormatJSInt32:
    case DataFormatInt32:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatCell:
    case DataFormatDouble:
    case DataFormatStorage:
    case DataFormatInt52:
    case DataFormatStrictInt52:
    case DataFormatBigInt32:
    case DataFormatJSBigInt32:
        DFG_CRASH(m_graph, m_currentNode, "Bad data format");
        
    default:
        DFG_CRASH(m_graph, m_currentNode, "Corrupt data format");
        return InvalidGPRReg;
    }
}

#if USE(BIGINT32)
void SpeculativeJIT::speculateBigInt32(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBigInt32))
        return;

    (SpeculateBigInt32Operand(this, edge)).gpr();
}

void SpeculativeJIT::speculateAnyBigInt(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBigInt))
        return;

    JSValueOperand value(this, edge, ManualOperandSpeculation);
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    Jump notCell = branchIfNotCell(valueRegs);
    // I inlined speculateHeapBigInt because it would be incorrect to call it here if it did JSValueOperand / SpeculateXXXOperand,
    // as it would confuse the DFG register allocator.
    DFG_TYPE_CHECK(valueRegs, edge, ~SpecCellCheck | SpecHeapBigInt, branchIfNotHeapBigInt(valueRegs.gpr()));
    auto done = jump();
    notCell.link(this);
    DFG_TYPE_CHECK(valueRegs, edge, SpecCellCheck | SpecBigInt32, branchIfNotBigInt32(valueRegs.gpr(), tempGPR));
    done.link(this);
}

GPRReg SpeculativeJIT::fillSpeculateBigInt32(Edge edge)
{
    AbstractValue& value = m_state.forNode(edge);
    SpeculatedType type = value.m_type;

    m_interpreter.filter(value, SpecBigInt32);
    if (value.isClear()) {
        if (mayHaveTypeCheck(edge.useKind()))
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
        return allocate();
    }

    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();

        if (edge->hasConstant()) {
            JSValue jsValue = edge->asJSValue();
            m_gprs.retain(gpr, virtualRegister, SpillOrderConstant);
            ASSERT(jsValue.isBigInt32());
            move(TrustedImm64(JSValue::encode(jsValue)), gpr);
            info.fillJSValue(m_stream, gpr, DataFormatJSBigInt32);
            return gpr;
        }

        DataFormat spillFormat = info.spillFormat();
        DFG_ASSERT(m_graph, m_currentNode, (spillFormat & DataFormatJS) || spillFormat == DataFormatBigInt32, spillFormat);

        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);

        if (spillFormat == DataFormatBigInt32) {
            // We have not yet implemented this
            RELEASE_ASSERT_NOT_REACHED();
        }
        if (spillFormat == DataFormatJSBigInt32) {
            load64(addressFor(virtualRegister), gpr);
            info.fillJSValue(m_stream, gpr, DataFormatJSBigInt32);
            return gpr;
        }

        load64(addressFor(virtualRegister), gpr);

        info.fillJSValue(m_stream, gpr, DataFormatJS);
        m_gprs.unlock(gpr);
        FALLTHROUGH;
    }

    case DataFormatJS: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        if (type & ~SpecBigInt32) {
            JumpList failureCases;
            GPRReg tempGPR = allocate();
            failureCases.append(branchIfNotBigInt32(gpr, tempGPR));
            speculationCheck(BadType, JSValueRegs(gpr), edge, failureCases);
            unlock(tempGPR);
        }
        info.fillJSValue(m_stream, gpr, DataFormatJSBigInt32);
        return gpr;
    }

    case DataFormatJSBigInt32: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }

    case DataFormatBoolean:
    case DataFormatJSBoolean:
    case DataFormatJSInt32:
    case DataFormatInt32:
    case DataFormatJSDouble:
    case DataFormatJSCell:
    case DataFormatCell:
    case DataFormatDouble:
    case DataFormatStorage:
    case DataFormatInt52:
    case DataFormatStrictInt52:
    case DataFormatBigInt32:
        DFG_CRASH(m_graph, m_currentNode, "Bad data format");

    default:
        DFG_CRASH(m_graph, m_currentNode, "Corrupt data format");
        return InvalidGPRReg;
    }
}
#endif // USE(BIGINT32)

void SpeculativeJIT::compileObjectStrictEquality(Edge objectChild, Edge otherChild)
{
    SpeculateCellOperand op1(this, objectChild);
    JSValueOperand op2(this, otherChild);
    GPRTemporary result(this);

    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();

    DFG_TYPE_CHECK(JSValueSource::unboxedCell(op1GPR), objectChild, SpecObject, branchIfNotObject(op1GPR));

    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because we are doing strict equality.
    compare64(Equal, op1GPR, op2GPR, resultGPR);
    or32(TrustedImm32(JSValue::ValueFalse), resultGPR);
    jsValueResult(resultGPR, m_currentNode, DataFormatJSBoolean);
}
    
void SpeculativeJIT::compilePeepHoleObjectStrictEquality(Edge objectChild, Edge otherChild, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;
    
    SpeculateCellOperand op1(this, objectChild);
    JSValueOperand op2(this, otherChild);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(op1GPR), objectChild, SpecObject, branchIfNotObject(op1GPR));

    if (taken == nextBlock()) {
        branchPtr(NotEqual, op1GPR, op2GPR, notTaken);
        jump(taken);
    } else {
        branchPtr(Equal, op1GPR, op2GPR, taken);
        jump(notTaken);
    }
}

void SpeculativeJIT::compileObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild)
{
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild, ManualOperandSpeculation);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
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
    Jump rightNotCell = branchIfNotCell(JSValueRegs(op2GPR));
    
    // We know that within this branch, rightChild must be a cell. 
    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(op2GPR), rightChild, (~SpecCellCheck) | SpecObject, branchIfNotObject(op2GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(op2GPR), rightChild, (~SpecCellCheck) | SpecObject, branchIfNotObject(op2GPR));
        speculationCheck(BadType, JSValueRegs(op2GPR), rightChild,
            branchTest8(
                NonZero,
                Address(op2GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    compare64(Equal, op1GPR, op2GPR, resultGPR);
    Jump done = jump();
    
    rightNotCell.link(this);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (needsTypeCheck(rightChild, SpecCellCheck | SpecOther)) {
        move(op2GPR, resultGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), resultGPR);
        
        typeCheck(
            JSValueRegs(op2GPR), rightChild, SpecCellCheck | SpecOther,
            branch64(
                NotEqual, resultGPR,
                TrustedImm64(JSValue::ValueNull)));
    }
    move(TrustedImm32(0), result.gpr());

    done.link(this);
    or32(TrustedImm32(JSValue::ValueFalse), resultGPR);
    jsValueResult(resultGPR, m_currentNode, DataFormatJSBoolean);
}

void SpeculativeJIT::compilePeepHoleObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;
    
    SpeculateCellOperand op1(this, leftChild);
    JSValueOperand op2(this, rightChild, ManualOperandSpeculation);
    GPRTemporary result(this);
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
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
    Jump rightNotCell = branchIfNotCell(JSValueRegs(op2GPR));
    
    // We know that within this branch, rightChild must be a cell. 
    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(op2GPR), rightChild, (~SpecCellCheck) | SpecObject, branchIfNotObject(op2GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(op2GPR), rightChild, (~SpecCellCheck) | SpecObject, branchIfNotObject(op2GPR));
        speculationCheck(BadType, JSValueRegs(op2GPR), rightChild,
            branchTest8(
                NonZero,
                Address(op2GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }
    
    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because both left and right are pointers to objects that have no special equality
    // protocols.
    branch64(Equal, op1GPR, op2GPR, taken);
    
    // We know that within this branch, rightChild must not be a cell. Check if that is enough to
    // prove that it is either null or undefined.
    if (!needsTypeCheck(rightChild, SpecCellCheck | SpecOther))
        rightNotCell.link(this);
    else {
        jump(notTaken, ForceJump);
        
        rightNotCell.link(this);
        move(op2GPR, resultGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), resultGPR);
        
        typeCheck(
            JSValueRegs(op2GPR), rightChild, SpecCellCheck | SpecOther, branch64(
                NotEqual, resultGPR,
                TrustedImm64(JSValue::ValueNull)));
    }
    
    jump(notTaken);
}

void SpeculativeJIT::compileSymbolUntypedEquality(Node* node, Edge symbolEdge, Edge untypedEdge)
{
    SpeculateCellOperand symbol(this, symbolEdge);
    JSValueOperand untyped(this, untypedEdge);
    GPRTemporary result(this, Reuse, symbol, untyped);

    GPRReg symbolGPR = symbol.gpr();
    GPRReg untypedGPR = untyped.gpr();
    GPRReg resultGPR = result.gpr();

    speculateSymbol(symbolEdge, symbolGPR);

    // At this point we know that we can perform a straight-forward equality comparison on pointer
    // values because we are doing strict equality.
    compare64(Equal, symbolGPR, untypedGPR, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileInt52Compare(Node* node, RelationalCondition condition)
{
    SpeculateWhicheverInt52Operand op1(this, node->child1());
    SpeculateWhicheverInt52Operand op2(this, node->child2(), op1);
    GPRTemporary result(this, Reuse, op1, op2);
    
    compare64(condition, op1.gpr(), op2.gpr(), result.gpr());
    
    // If we add a DataFormatBool, we should use it here.
    or32(TrustedImm32(JSValue::ValueFalse), result.gpr());
    jsValueResult(result.gpr(), m_currentNode, DataFormatJSBoolean);
}

void SpeculativeJIT::compilePeepHoleInt52Branch(Node* node, Node* branchNode, RelationalCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        condition = invert(condition);
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }
    
    SpeculateWhicheverInt52Operand op1(this, node->child1());
    SpeculateWhicheverInt52Operand op2(this, node->child2(), op1);
    
    branch64(condition, op1.gpr(), op2.gpr(), taken);
    jump(notTaken);
}

#if USE(BIGINT32)
void SpeculativeJIT::compileBigInt32Compare(Node* node, RelationalCondition condition)
{
    SpeculateBigInt32Operand op1(this, node->child1());
    SpeculateBigInt32Operand op2(this, node->child2());
    GPRTemporary result(this, Reuse, op1, op2);

    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();

    if (condition == Equal || condition == NotEqual) {
        // No need to unbox the operands, since the tag bits are identical
        compare64(condition, op1GPR, op2GPR, resultGPR);
    } else {
        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();

        unboxBigInt32(op1GPR, tempGPR);
        unboxBigInt32(op2GPR, resultGPR);
        compare32(condition, tempGPR, resultGPR, resultGPR);
    }
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compilePeepHoleBigInt32Branch(Node* node, Node* branchNode, RelationalCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        condition = invert(condition);
        std::swap(taken, notTaken);
    }

    SpeculateBigInt32Operand op1(this, node->child1());
    SpeculateBigInt32Operand op2(this, node->child2());
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();

    if (condition == Equal || condition == NotEqual) {
        branch64(condition, op1GPR, op2GPR, taken);
        jump(notTaken);
    } else {
        GPRTemporary lhs(this, Reuse, op1);
        GPRTemporary rhs(this, Reuse, op2);
        GPRReg lhsGPR = lhs.gpr();
        GPRReg rhsGPR = rhs.gpr();
        unboxBigInt32(op1GPR, lhsGPR);
        unboxBigInt32(op2GPR, rhsGPR);
        branch32(condition, lhsGPR, rhsGPR, taken);
        jump(notTaken);
    }

}
#endif // USE(BIGINT32)

void SpeculativeJIT::compileCompareEqPtr(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary result(this);
    GPRReg valueGPR = value.gpr();
    GPRReg resultGPR = result.gpr();

    loadLinkableConstant(LinkableConstant(*this, node->cellOperand()->cell()), resultGPR);
    compare64(Equal, valueGPR, resultGPR, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileToBooleanObjectOrOther(Edge nodeUse, bool invert)
{
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRTemporary result(this);
    GPRReg valueGPR = value.gpr();
    GPRReg resultGPR = result.gpr();
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

    Jump notCell = branchIfNotCell(JSValueRegs(valueGPR));
    if (masqueradesAsUndefinedWatchpointSetValid) {
        DFG_TYPE_CHECK(
            JSValueRegs(valueGPR), nodeUse, (~SpecCellCheck) | SpecObject, branchIfNotObject(valueGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(valueGPR), nodeUse, (~SpecCellCheck) | SpecObject, branchIfNotObject(valueGPR));

        Jump isNotMasqueradesAsUndefined =
            branchTest8(
                Zero,
                Address(valueGPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined));

        emitLoadStructure(vm(), valueGPR, structureGPR);
        speculationCheck(BadType, JSValueRegs(valueGPR), nodeUse, 
            branchLinkableConstant(
                Equal,
                Address(structureGPR, Structure::globalObjectOffset()),
                LinkableConstant::globalObject(*this, m_currentNode)));

        isNotMasqueradesAsUndefined.link(this);
    }
    move(invert ? TrustedImm32(JSValue::ValueFalse) : TrustedImm32(JSValue::ValueTrue), resultGPR);
    Jump done = jump();
    
    notCell.link(this);

    if (needsTypeCheck(nodeUse, SpecCellCheck | SpecOther)) {
        move(valueGPR, resultGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), resultGPR);
        typeCheck(
            JSValueRegs(valueGPR), nodeUse, SpecCellCheck | SpecOther, branch64(
                NotEqual,
                resultGPR, 
                TrustedImm64(JSValue::ValueNull)));
    }
    move(invert ? TrustedImm32(JSValue::ValueTrue) : TrustedImm32(JSValue::ValueFalse), resultGPR);
    
    done.link(this);
    
    jsValueResult(resultGPR, m_currentNode, DataFormatJSBoolean);
}

void SpeculativeJIT::compileToBoolean(Node* node, bool invert)
{
    switch (node->child1().useKind()) {
    case ObjectOrOtherUse: {
        compileToBooleanObjectOrOther(node->child1(), invert);
        return;
    }
        
    case Int32Use: {
        SpeculateInt32Operand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);
        compare32(invert ? Equal : NotEqual, value.gpr(), TrustedImm32(0), result.gpr());
        or32(TrustedImm32(JSValue::ValueFalse), result.gpr());
        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
        return;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        FPRTemporary scratch(this);
        GPRTemporary result(this);
        move(invert ? TrustedImm32(JSValue::ValueFalse) : TrustedImm32(JSValue::ValueTrue), result.gpr());
        Jump nonZero = branchDoubleNonZero(value.fpr(), scratch.fpr());
        move(invert ? TrustedImm32(JSValue::ValueTrue) : TrustedImm32(JSValue::ValueFalse), result.gpr());
        nonZero.link(this);
        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
        return;
    }
    
    case BooleanUse:
    case KnownBooleanUse: {
        if (!needsTypeCheck(node->child1(), SpecBoolean)) {
            SpeculateBooleanOperand value(this, node->child1());
            GPRTemporary result(this, Reuse, value);
            
            if (invert)
                xor32(TrustedImm32(1), value.gpr(), result.gpr());
            else
                move(value.gpr(), result.gpr());
            
            jsValueResult(result.gpr(), node, DataFormatJSBoolean);
            return;
        }
        
        JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
        GPRTemporary result(this); // FIXME: We could reuse, but on speculation fail would need recovery to restore tag (akin to add).
        
        move(value.gpr(), result.gpr());
        xor64(TrustedImm32(JSValue::ValueFalse), result.gpr());
        typeCheck(
            JSValueRegs(value.gpr()), node->child1(), SpecBoolean, branchTest64(
                NonZero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
        xor64(invert ? TrustedImm32(JSValue::ValueTrue) : TrustedImm32(JSValue::ValueFalse), result.gpr());
        
        // If we add a DataFormatBool, we should use it here.
        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
        return;
    }
        
    case UntypedUse: {
        JSValueOperand arg1(this, node->child1());
        GPRTemporary result(this);
    
        GPRReg arg1GPR = arg1.gpr();
        GPRReg resultGPR = result.gpr();

        FPRTemporary valueFPR(this);
        FPRTemporary tempFPR(this);

        bool shouldCheckMasqueradesAsUndefined = !masqueradesAsUndefinedWatchpointSetIsStillValid();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        std::optional<GPRTemporary> scratch;
        GPRReg scratchGPR = InvalidGPRReg;
        if (shouldCheckMasqueradesAsUndefined) {
            scratch.emplace(this);
            scratchGPR = scratch->gpr();
        }
        emitConvertValueToBoolean(vm(), JSValueRegs(arg1GPR), resultGPR, scratchGPR, valueFPR.fpr(), tempFPR.fpr(), shouldCheckMasqueradesAsUndefined, globalObject, invert);
        or32(TrustedImm32(JSValue::ValueFalse), resultGPR);
        jsValueResult(resultGPR, node, DataFormatJSBoolean);
        return;
    }
    case StringUse:
        compileToBooleanString(node, invert);
        return;

    case StringOrOtherUse:
        compileToBooleanStringOrOther(node, invert);
        return;

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::emitObjectOrOtherBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRTemporary scratch(this);
    GPRTemporary structure;
    GPRReg valueGPR = value.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg structureGPR = InvalidGPRReg;

    bool objectMayMasqueradeAsUndefined = !masqueradesAsUndefinedWatchpointSetIsStillValid();
    if (objectMayMasqueradeAsUndefined) {
        GPRTemporary realStructure(this);
        structure.adopt(realStructure);
        structureGPR = structure.gpr();
    }

    Jump notCell = branchIfNotCell(JSValueRegs(valueGPR));
    if (!objectMayMasqueradeAsUndefined) {
        DFG_TYPE_CHECK(
            JSValueRegs(valueGPR), nodeUse, (~SpecCellCheck) | SpecObject, branchIfNotObject(valueGPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueRegs(valueGPR), nodeUse, (~SpecCellCheck) | SpecObject, branchIfNotObject(valueGPR));

        Jump isNotMasqueradesAsUndefined = branchTest8(
            Zero,
            Address(valueGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(MasqueradesAsUndefined));

        emitLoadStructure(vm(), valueGPR, structureGPR);
        speculationCheck(BadType, JSValueRegs(valueGPR), nodeUse,
            branchLinkableConstant(
                Equal,
                Address(structureGPR, Structure::globalObjectOffset()),
                LinkableConstant::globalObject(*this, m_currentNode)));

        isNotMasqueradesAsUndefined.link(this);
    }
    jump(taken, ForceJump);
    
    notCell.link(this);
    
    if (needsTypeCheck(nodeUse, SpecCellCheck | SpecOther)) {
        move(valueGPR, scratchGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), scratchGPR);
        typeCheck(
            JSValueRegs(valueGPR), nodeUse, SpecCellCheck | SpecOther, branch64(
                NotEqual, scratchGPR, TrustedImm64(JSValue::ValueNull)));
    }
    jump(notTaken);
    
    noResult(m_currentNode);
}

void SpeculativeJIT::emitUntypedBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    // Implements the following control flow structure, inspired by branchIfValue():
    // if (value is cell) {
    //     if (value is string or value is HeapBigInt)
    //         result = !!value->length
    //     else {
    //         do evil things for masquerades-as-undefined
    //         result = true
    //     }
    // } else if (value is Boolean) {
    //     result = value == jsTrue
    // } else if (value is int32) {
    //     result = !!unboxInt32(value)
    // } else if (value is number) {
    //     result = !!unboxDouble(value)
    // } else if (value is BigInt32) {
    //     result = !!unboxBigInt32(value)
    // } else {
    //     result = false
    // }
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRReg valueGPR = value.gpr();
    JSValueRegs valueRegs = JSValueRegs(valueGPR);
    GPRTemporary temp1(this);
    GPRTemporary temp2(this);
    FPRTemporary fprValue(this);
    FPRTemporary fprTemp(this);

    GPRReg temp1GPR = temp1.gpr();
    GPRReg temp2GPR = temp2.gpr();
    FPRReg valueFPR = fprValue.fpr();
    FPRReg tempFPR = fprTemp.fpr();

    if (needsTypeCheck(nodeUse, ~SpecCell)) {
        bool isDefinitelyCell = !needsTypeCheck(nodeUse, SpecCell);
        Jump skipCellCase;
        if (!isDefinitelyCell)
            skipCellCase = branchIfNotCell(valueRegs);

        if (needsTypeCheck(nodeUse, ~SpecString)) {
            bool isDefinitelyString = !needsTypeCheck(nodeUse, SpecString | ~SpecCellCheck);
            Jump skipStringCase;
            if (!isDefinitelyString)
                skipStringCase = branchIfNotString(valueGPR);

            branchLinkableConstant(Equal, valueGPR, LinkableConstant(*this, jsEmptyString(vm())), notTaken);
            jump(taken, ForceJump);

            if (!isDefinitelyString)
                skipStringCase.link(this);
        }

        if (needsTypeCheck(nodeUse, ~SpecHeapBigInt)) {
            bool isDefinitelyHeapBigInt = !needsTypeCheck(nodeUse, SpecHeapBigInt | ~SpecCellCheck);
            Jump skipHeapBigIntCase;
            if (!isDefinitelyHeapBigInt)
                skipHeapBigIntCase = branchIfNotHeapBigInt(valueGPR);

            branchTest32(NonZero, Address(valueGPR, JSBigInt::offsetOfLength()), taken);
            jump(notTaken, ForceJump);

            if (!isDefinitelyHeapBigInt)
                skipHeapBigIntCase.link(this);
        }

        bool shouldCheckMasqueradesAsUndefined = !masqueradesAsUndefinedWatchpointSetIsStillValid();
        if (shouldCheckMasqueradesAsUndefined) {
            branchTest8(Zero, Address(valueGPR, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined), taken);
            emitLoadStructure(vm(), valueGPR, temp1GPR);
            loadLinkableConstant(LinkableConstant::globalObject(*this, m_currentNode), temp2GPR);
            branchPtr(NotEqual, Address(temp1GPR, Structure::globalObjectOffset()), temp2GPR, taken);
            jump(notTaken, ForceJump);
        } else
            jump(taken, ForceJump);

        if (!isDefinitelyCell)
            skipCellCase.link(this);
    }

    if (needsTypeCheck(nodeUse, ~SpecBoolean)) {
        branch64(Equal, valueGPR, TrustedImm64(JSValue::encode(jsBoolean(false))), notTaken);
        branch64(Equal, valueGPR, TrustedImm64(JSValue::encode(jsBoolean(true))), taken);
    }

    if (needsTypeCheck(nodeUse, ~SpecInt32Only)) {
        branch64(Above, valueGPR, GPRInfo::numberTagRegister, taken);
        branch64(Equal, valueGPR, GPRInfo::numberTagRegister, notTaken);
    }

    if (needsTypeCheck(nodeUse, ~SpecFullDouble)) {
        bool isDefinitelyDouble = !needsTypeCheck(nodeUse, SpecCell | SpecBoolean | SpecInt32Only | SpecFullDouble);
        Jump skipDoubleCase;
        if (!isDefinitelyDouble)
            skipDoubleCase = branchIfNotNumber(valueGPR);

        unboxDouble(valueGPR, temp1GPR, valueFPR);
        branchDoubleZeroOrNaN(valueFPR, tempFPR, notTaken);
        jump(taken, ForceJump);

        if (!isDefinitelyDouble)
            skipDoubleCase.link(this);
    }

#if USE(BIGINT32)
    if (needsTypeCheck(nodeUse, ~SpecBigInt32)) {
        bool isDefinitelyBigInt32 = !needsTypeCheck(nodeUse, SpecCell | SpecBoolean | SpecInt32Only | SpecFullDouble | SpecBigInt32);
        Jump skipBigInt32Case;
        if (!isDefinitelyBigInt32)
            skipBigInt32Case = branchIfNotBigInt32(valueGPR, tempGPR);

        move(valueGPR, tempGPR);
        urshift64(TrustedImm32(16), tempGPR);
        branchTest32(NonZero, tempGPR, taken);
        // Here we fallthrough to the jump(notTaken) below and outside this branch.

        if (!isDefinitelyBigInt32)
            skipBigInt32Case.link(this);
    }
#endif // USE(BIGINT32)

    jump(notTaken);

    value.use();
    noResult(m_currentNode, UseChildrenCalledExplicitly);
    return;
}

void SpeculativeJIT::emitBranch(Node* node)
{
    BasicBlock* taken = node->branchData()->taken.block;
    BasicBlock* notTaken = node->branchData()->notTaken.block;
    
    switch (node->child1().useKind()) {
    case ObjectOrOtherUse: {
        emitObjectOrOtherBranch(node->child1(), taken, notTaken);
        return;
    }
        
    case Int32Use: {
        ResultCondition condition = NonZero;

        if (taken == nextBlock()) {
            condition = Zero;
            BasicBlock* tmp = taken;
            taken = notTaken;
            notTaken = tmp;
        }

        SpeculateInt32Operand value(this, node->child1());
        branchTest32(condition, value.gpr(), taken);
        jump(notTaken);

        noResult(node);
        return;
    }
    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        FPRTemporary scratch(this);
        branchDoubleNonZero(value.fpr(), scratch.fpr(), taken);
        jump(notTaken);
        
        noResult(node);
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

    case BooleanUse:
    case KnownBooleanUse: {
        JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
        GPRReg valueGPR = value.gpr();

        if (!needsTypeCheck(node->child1(), SpecBoolean)) {
            ResultCondition condition = NonZero;

            if (taken == nextBlock()) {
                condition = Zero;
                BasicBlock* tmp = taken;
                taken = notTaken;
                notTaken = tmp;
            }

            branchTest32(condition, valueGPR, TrustedImm32(true), taken);
            jump(notTaken);
        } else {
            branch64(Equal, valueGPR, TrustedImm64(JSValue::encode(jsBoolean(false))), notTaken);
            branch64(Equal, valueGPR, TrustedImm64(JSValue::encode(jsBoolean(true))), taken);

            typeCheck(JSValueRegs(valueGPR), node->child1(), SpecBoolean, jump());
        }
        value.use();
        noResult(node, UseChildrenCalledExplicitly);
        return;
    }

    case UntypedUse: {
        emitUntypedBranch(node->child1(), taken, notTaken);
        return;
    }
        
    default:
        DFG_CRASH(m_graph, m_currentNode, "Bad use kind");
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
        DFG_CRASH(m_graph, node, "Bad array mode type");
        break;
    case Array::Undecided: {
        SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 1));
        GPRReg indexGPR = index.gpr();

        JSValueRegs resultRegs;
        std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);

        speculationCheck(OutOfBounds, JSValueRegs(), node,
            branch32(LessThan, indexGPR, TrustedImm32(0)));

        move(TrustedImm64(JSValue::ValueUndefined), resultRegs.gpr());

        jsValueResult(resultRegs, node);
        break;
    }
    case Array::BigInt64Array:
    case Array::BigUint64Array:
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
            GPRReg baseGPR = base.gpr();
            GPRReg propertyGPR = property.gpr();

            JSValueRegs resultRegs;
            CanUseFlush canUseFlush = CanUseFlush::Yes;
            std::tie(resultRegs, std::ignore, canUseFlush) = prefix(DataFormatJS);

            if (canUseFlush == CanUseFlush::No)
                silentSpillAllRegisters(resultRegs);
            else
                flushRegisters();
            callOperation(operationGetByVal, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, propertyGPR);
            if (canUseFlush == CanUseFlush::No)
                silentFillAllRegisters();
            exceptionCheck();

            jsValueResult(resultRegs, node);
            return;
        }

        JSValueOperand base(this, m_graph.varArgChild(node, 0), ManualOperandSpeculation);
        JSValueOperand property(this, m_graph.varArgChild(node, 1), ManualOperandSpeculation);
        GPRReg baseGPR = base.gpr();
        GPRReg propertyGPR = property.gpr();

        GPRTemporary stubInfoTemp;
        GPRReg stubInfoGPR = InvalidGPRReg;
        if (m_graph.m_plan.isUnlinked()) {
            stubInfoTemp = GPRTemporary(this);
            stubInfoGPR = stubInfoTemp.gpr();
        }

        speculate(node, m_graph.varArgChild(node, 0));
        speculate(node, m_graph.varArgChild(node, 1));

        JSValueRegs resultRegs;
        std::tie(resultRegs, std::ignore, std::ignore) = prefix(DataFormatJS);
        GPRReg resultGPR = resultRegs.gpr();

        CodeOrigin codeOrigin = node->origin.semantic;
        CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
        RegisterSetBuilder usedRegisters = this->usedRegisters();

        JumpList slowCases;
        if (!m_state.forNode(m_graph.varArgChild(node, 0)).isType(SpecCell))
            slowCases.append(branchIfNotCell(baseGPR));

        JSValueRegs baseRegs { baseGPR };
        JSValueRegs propertyRegs { propertyGPR };
        auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
        JITGetByValGenerator gen(
            codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::GetByVal, usedRegisters,
            baseRegs, propertyRegs, resultRegs, stubInfoGPR);

        std::visit([&](auto* stubInfo) {
            if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecString))
                stubInfo->propertyIsString = true;
            else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecInt32Only))
                stubInfo->propertyIsInt32 = true;
            else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecSymbol))
                stubInfo->propertyIsSymbol = true;
            stubInfo->isEnumerator = node->op() == EnumeratorGetByVal;
        }, stubInfo);

        std::unique_ptr<SlowPathGenerator> slowPath;
        if (m_graph.m_plan.isUnlinked()) {
            gen.generateDFGDataICFastPath(*this, stubInfoConstant.index(), stubInfoGPR);
            gen.m_unlinkedStubInfoConstantIndex = stubInfoConstant.index();
            slowPath = slowPathICCall(
                slowCases, this, stubInfoConstant, stubInfoGPR, Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), operationGetByValOptimize,
                resultGPR, LinkableConstant::globalObject(*this, node), stubInfoGPR, nullptr, baseGPR, propertyGPR);
        } else {
            gen.generateFastPath(*this);
            slowCases.append(gen.slowPathJump());
            slowPath = slowPathCall(
                slowCases, this, operationGetByValOptimize,
                resultGPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(gen.stubInfo()), nullptr, baseGPR, propertyGPR);
        }

        addGetByVal(gen, slowPath.get());
        addSlowPathGenerator(WTFMove(slowPath));

        jsValueResult(resultGPR, node);
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
            std::tie(resultRegs, format, std::ignore) = prefix(node->arrayMode().type() == Array::Int32 ? DataFormatJSInt32 : DataFormatJS);
            GPRReg result = resultRegs.gpr();

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

            load64(BaseIndex(storageReg, propertyReg, TimesEight), result);
            if (node->arrayMode().isInBoundsSaneChain()) {
                ASSERT(node->arrayMode().type() == Array::Contiguous);
                Jump notHole = branchIfNotEmpty(result);
                move(TrustedImm64(JSValue::encode(jsUndefined())), result);
                notHole.link(this);
            } else {
                speculationCheck(
                    LoadFromHole, JSValueRegs(), nullptr,
                    branchIfEmpty(result));
            }
            jsValueResult(result, node, format);
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
        GPRReg resultReg = resultRegs.gpr();

        JumpList slowCases;

        slowCases.append(branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

        load64(BaseIndex(storageReg, propertyReg, TimesEight), resultReg);

        if (node->arrayMode().isOutOfBoundsSaneChain()) {
            auto done = branchIfNotEmpty(resultReg);
            slowCases.link(this);
            speculationCheck(NegativeIndex, JSValueRegs(), nullptr, branch32(LessThan, propertyReg, TrustedImm32(0)));
            move(TrustedImm64(JSValue::encode(jsUndefined())), resultReg);
            done.link(this);
        } else {
            slowCases.append(branchIfEmpty(resultReg));
            addSlowPathGenerator(
                slowPathCall(
                    slowCases, this, operationGetByValObjectInt,
                    resultReg, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));
        }


        jsValueResult(resultReg, node);
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

        bool resultIsUnboxed = node->arrayMode().isOutOfBoundsSaneChain() && !(node->flags() & NodeBytecodeUsesAsOther);

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
        DataFormat format;
        std::tie(resultRegs, format, std::ignore) = prefix(resultIsUnboxed ? DataFormatDouble : DataFormatJS);

        JumpList slowCases;

        slowCases.append(branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));

        loadDouble(BaseIndex(storageReg, propertyReg, TimesEight), tempReg);
        if (node->arrayMode().isOutOfBoundsSaneChain()) {
            if (format == DataFormatDouble) {
                auto done = jump();
                slowCases.link(this);
                speculationCheck(NegativeIndex, JSValueRegs(), nullptr, branch32(LessThan, propertyReg, TrustedImm32(0)));
                static const double NaN = PNaN;
                loadDouble(TrustedImmPtr(&NaN), tempReg);
                done.link(this);
                ASSERT(!resultRegs);
                doubleResult(tempReg, node);
            } else {
                slowCases.append(branchIfNaN(tempReg));
                boxDouble(tempReg, resultRegs.gpr());
                auto done = jump();
                slowCases.link(this);
                speculationCheck(NegativeIndex, JSValueRegs(), nullptr, branch32(LessThan, propertyReg, TrustedImm32(0)));
                move(TrustedImm64(JSValue::encode(jsUndefined())), resultRegs.gpr());
                done.link(this);
                jsValueResult(resultRegs.gpr(), node);
            }
        } else {
            slowCases.append(branchIfNaN(tempReg));
            boxDouble(tempReg, resultRegs.gpr());
            addSlowPathGenerator(
                slowPathCall(
                    slowCases, this, operationGetByValObjectInt,
                    resultRegs.gpr(), LinkableConstant::globalObject(*this, node), baseReg, propertyReg));
            jsValueResult(resultRegs.gpr(), node);
        }

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
            GPRReg resultGPR = resultRegs.gpr();

            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, branch32(AboveOrEqual, propertyReg, Address(storageReg, ArrayStorage::vectorLengthOffset())));


            load64(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset()), resultGPR);
            speculationCheck(LoadFromHole, JSValueRegs(), nullptr, branchIfEmpty(resultGPR));

            jsValueResult(resultGPR, node);
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
        GPRReg resultReg = resultRegs.gpr();

        JumpList slowCases;

        slowCases.append(branch32(AboveOrEqual, propertyReg, Address(storageReg, ArrayStorage::vectorLengthOffset())));

        load64(BaseIndex(storageReg, propertyReg, TimesEight, ArrayStorage::vectorOffset()), resultReg);
        slowCases.append(branchIfEmpty(resultReg));

        addSlowPathGenerator(
            slowPathCall(
                slowCases, this, operationGetByValObjectInt,
                resultReg, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));

        jsValueResult(resultReg, node);
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

#if ENABLE(YARR_JIT_REGEXP_TEST_INLINE)
void SpeculativeJIT::compileRegExpTestInline(Node* node)
{
    RegExp* regExp = jsCast<RegExp*>(node->cellOperand2()->value());

    auto jitCodeBlock = regExp->getRegExpJITCodeBlock();
    ASSERT(jitCodeBlock);
    auto inlineCodeStats8Bit = jitCodeBlock->get8BitInlineStats();

    ASSERT(!regExp->globalOrSticky());

    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand base(this, node->child2());

    GPRTemporary stringImpl(this);
    GPRTemporary stringData(this);
    GPRTemporary strLength(this);
    GPRTemporary output(this);
    GPRTemporary temp0(this);
    std::optional<GPRTemporary> temp1;

    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg baseGPR = base.gpr();
    GPRReg stringImplGPR = stringImpl.gpr();
    GPRReg stringDataGPR = stringData.gpr();
    GPRReg outputGPR = output.gpr();
    GPRReg strLengthGPR = strLength.gpr();
    GPRReg temp0GPR = temp0.gpr();
    GPRReg temp1GPR = InvalidGPRReg;

    if (inlineCodeStats8Bit.needsTemp2()) {
        temp1.emplace(this);
        temp1GPR = temp1->gpr();
    }

    speculateRegExpObject(node->child2(), baseGPR);

    JumpList slowCases;

    auto regExpTestInlineCase = [&](GPRReg argumentGPR, JumpList& slowCases) {
        loadPtr(Address(argumentGPR, JSString::offsetOfValue()), stringImplGPR);
        // If the string is a rope or 16 bit, we call the operation.
        slowCases.append(branchIfRopeStringImpl(stringImplGPR));
        slowCases.append(branchTest32(
            Zero,
            Address(stringImplGPR, StringImpl::flagsOffset()),
            TrustedImm32(StringImpl::flagIs8Bit())));

        loadPtr(Address(stringImplGPR, StringImpl::dataOffset()), stringDataGPR);
        load32(Address(stringImplGPR, StringImpl::lengthMemoryOffset()), strLengthGPR);

        // Clobbering input registers is OK since we already called flushRegisters.
        // All slowCases jumps, except the JIT failure check below are already done.
        // The JIT failure is generated in the inlined code before we trash/reuse baseGPR (regT1).
        // The other registers needed for the slow paths are globalObjectGPR and argumentGPR, which are preserved for the inlined result.
        Yarr::YarrJITRegisters yarrRegisters;
        yarrRegisters.input = stringDataGPR;
        yarrRegisters.index = stringImplGPR;
        yarrRegisters.length = strLengthGPR;
        yarrRegisters.output = outputGPR;
        yarrRegisters.regT0 = temp0GPR;
        yarrRegisters.regT1 = baseGPR;
        if (inlineCodeStats8Bit.needsTemp2())
            yarrRegisters.regT2 = temp1GPR;

        yarrRegisters.returnRegister = temp0GPR;
        yarrRegisters.returnRegister2 = stringDataGPR;

        auto commonData = jitCode()->dfgCommon();
        move(TrustedImm32(0), yarrRegisters.index);
        Yarr::jitCompileInlinedTest(&m_graph.m_stackChecker, regExp->pattern(), regExp->flags(), Yarr::CharSize::Char8, &vm(), commonData->m_boyerMooreData, *this, yarrRegisters);

        slowCases.append(branch32(Equal, yarrRegisters.returnRegister, TrustedImm32(static_cast<int32_t>(Yarr::JSRegExpResult::JITCodeFailure))));

        auto failedMatch = branch32(LessThan, yarrRegisters.returnRegister, TrustedImm32(0));

        //  Saved cached result
        ptrdiff_t offset = JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult();

        storeLinkableConstant(LinkableConstant(*this, regExp), Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfLastRegExp()));
        storePtr(argumentGPR, Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfLastInput()));
        store32(yarrRegisters.returnRegister, Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, start)));
        store32(yarrRegisters.returnRegister2, Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, end)));
        store8(TrustedImm32(0), Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfReified()));

        JumpList doneCases;

        move(TrustedImm32(1), temp0GPR);
        doneCases.append(jump());

        failedMatch.link(this);
        move(TrustedImm32(0), temp0GPR);
        doneCases.append(jump());

        return doneCases;
    };

    if (node->child3().useKind() == StringUse) {
        SpeculateCellOperand argument(this, node->child3());
        GPRReg argumentGPR = argument.gpr();
        speculateString(node->child3(), argumentGPR);

        flushRegisters();

        auto doneCases = regExpTestInlineCase(argumentGPR, slowCases);

        slowCases.link(this);
        callOperation(operationRegExpTestString, temp0GPR, globalObjectGPR, baseGPR, argumentGPR);
        exceptionCheck();

        doneCases.link(this);
        unblessedBooleanResult(temp0GPR, node);
        return;
    }

    JSValueOperand argument(this, node->child3());
    GPRReg argumentGPR = argument.gpr();

    flushRegisters();

    slowCases.append(branchIfNotCell(argumentGPR));
    slowCases.append(branchIfNotString(argumentGPR));

    auto doneCases = regExpTestInlineCase(argumentGPR, slowCases);

    slowCases.link(this);
    callOperation(operationRegExpTest, temp0GPR, globalObjectGPR, baseGPR, argumentGPR);
    exceptionCheck();

    doneCases.link(this);
    unblessedBooleanResult(temp0GPR, node);
}
#else
void SpeculativeJIT::compileRegExpTestInline(Node* node)
{
    UNUSED_PARAM(node);
    ASSERT_NOT_REACHED();
    compileRegExpTest(node);
}
#endif

#if USE(LARGE_TYPED_ARRAYS)
void SpeculativeJIT::compileNewTypedArrayWithInt52Size(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    auto typedArrayType = node->typedArrayType();
    bool isResizableOrGrowableShared = false;
    RegisteredStructure structure = m_graph.registerStructure(globalObject->typedArrayStructureConcurrently(typedArrayType, isResizableOrGrowableShared));
    RELEASE_ASSERT(structure.get());

    SpeculateStrictInt52Operand size(this, node->child1());
    GPRReg sizeGPR = size.gpr();

    emitNewTypedArrayWithSizeInRegister(node, typedArrayType, structure, sizeGPR);
}

void SpeculativeJIT::compileGetTypedArrayLengthAsInt52(Node* node)
{
    // If arrayMode is ForceExit, we would not compile this node and hence, should not have arrived here.
    DFG_ASSERT(m_graph, node, node->arrayMode().isSomeTypedArrayView());
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary scratch1(this);
        GPRTemporary result(this);

        GPRReg baseGPR = base.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg resultGPR = result.gpr();

        loadTypedArrayLength(baseGPR, resultGPR, scratch1GPR, resultGPR, node->arrayMode().type() == Array::AnyTypedArray ? std::nullopt : std::optional { node->arrayMode().typedArrayType() });

        strictInt52Result(resultGPR, node);
        return;
    }

    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this, Reuse, base);
    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();
    speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(baseGPR), node, branchTest8(NonZero, Address(baseGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));
    load64(Address(baseGPR, JSArrayBufferView::offsetOfLength()), resultGPR);
    static_assert(MAX_ARRAY_BUFFER_SIZE < (1ull << 52), "there is a risk that the size of a typed array won't fit in an Int52");
    strictInt52Result(resultGPR, node);
}

void SpeculativeJIT::compileGetTypedArrayByteOffsetAsInt52(Node* node)
{
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary scratch1(this);
        GPRTemporary scratch2(this);
        GPRTemporary result(this);

        GPRReg baseGPR = base.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg scratch2GPR = scratch2.gpr();
        GPRReg resultGPR = result.gpr();

        auto outOfBounds = branchIfResizableOrGrowableSharedTypedArrayIsOutOfBounds(baseGPR, scratch1GPR, scratch2GPR, node->arrayMode().type() == Array::AnyTypedArray ? std::nullopt : std::optional { node->arrayMode().typedArrayType() });
        load64(Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), resultGPR);
        auto done = jump();

        outOfBounds.link(this);
        move(TrustedImm32(0), resultGPR);
        done.link(this);

        strictInt52Result(resultGPR, node);
        return;
    }
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();

    speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(baseGPR), node, branchTest8(NonZero, Address(baseGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));

    load64(Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), resultGPR);
    strictInt52Result(resultGPR, node);
}
#endif // USE(LARGE_TYPED_ARRAYS)

void SpeculativeJIT::compile(Node* node)
{
    NodeType op = node->op();

    if constexpr (validateDFGDoesGC) {
        if (Options::validateDoesGC()) {
            bool expectDoesGC = doesGC(m_graph, node);
            store64(TrustedImm64(DoesGCCheck::encode(expectDoesGC, node->index(), node->op())), vm().addressOfDoesGC());
        }
    }

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
    clearRegisterAllocationOffsets();
#endif

    switch (op) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
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
            
        case FlushedInt52: {
            GPRTemporary result(this);
            load64(addressFor(node->machineLocal()), result.gpr());
            
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);
            generationInfoFromVirtualRegister(virtualRegister).initInt52(node, node->refCount(), result.gpr());
            break;
        }
            
        default:
            GPRTemporary result(this);
            load64(addressFor(node->machineLocal()), result.gpr());
            
            // Like jsValueResult, but don't useChildren - our children are phi nodes,
            // and don't represent values within this dataflow with virtual registers.
            VirtualRegister virtualRegister = node->virtualRegister();
            m_gprs.retain(result.gpr(), virtualRegister, SpillOrderJS);
            
            DataFormat format;
            if (isCellSpeculation(value.m_type))
                format = DataFormatJSCell;
            else if (isBooleanSpeculation(value.m_type))
                format = DataFormatJSBoolean;
            else
                format = DataFormatJS;
            
            generationInfoFromVirtualRegister(virtualRegister).initJSValue(node, node->refCount(), result.gpr(), format);
            break;
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
            
        case FlushedInt52: {
            SpeculateInt52Operand value(this, node->child1());
            store64(value.gpr(), addressFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatInt52);
            break;
        }
            
        case FlushedCell: {
            SpeculateCellOperand cell(this, node->child1());
            GPRReg cellGPR = cell.gpr();
            store64(cellGPR, addressFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatCell);
            break;
        }
            
        case FlushedBoolean: {
            SpeculateBooleanOperand boolean(this, node->child1());
            store64(boolean.gpr(), addressFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(DataFormatBoolean);
            break;
        }
            
        case FlushedJSValue: {
            JSValueOperand value(this, node->child1());
            store64(value.gpr(), addressFor(node->machineLocal()));
            noResult(node);
            recordSetLocal(dataFormatFor(node->variableAccessData()->flushFormat()));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, node, "Bad flush format");
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

    case ValueBitNot:
        compileValueBitNot(node);
        break;

    case ArithBitNot:
        compileBitwiseNot(node);
        break;

    case ValueBitAnd:
    case ValueBitXor:
    case ValueBitOr:
        compileValueBitwiseOp(node);
        break;

    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
        compileBitwiseOp(node);
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
        
    case Int52Rep: {
        switch (node->child1().useKind()) {
        case Int32Use: {
            SpeculateInt32Operand operand(this, node->child1());
            GPRTemporary result(this, Reuse, operand);
            
            signExtend32ToPtr(operand.gpr(), result.gpr());
            
            strictInt52Result(result.gpr(), node);
            break;
        }
            
        case AnyIntUse: {
            GPRTemporary result(this);
            GPRReg resultGPR = result.gpr();
            
            convertAnyInt(node->child1(), resultGPR);
            
            strictInt52Result(resultGPR, node);
            break;
        }
            
        case DoubleRepAnyIntUse: {
            SpeculateDoubleOperand value(this, node->child1());
            GPRTemporary result(this);
            GPRTemporary scratch1(this);
            FPRTemporary scratch2(this);

            FPRReg valueFPR = value.fpr();
            GPRReg resultGPR = result.gpr();
            GPRReg scratch1GPR = scratch1.gpr();
            FPRReg scratch2FPR = scratch2.fpr();

            JumpList failureCases;
            branchConvertDoubleToInt52(valueFPR, resultGPR, failureCases, scratch1GPR, scratch2FPR);

            DFG_TYPE_CHECK_WITH_EXIT_KIND(Int52Overflow, JSValueRegs(), node->child1(), SpecAnyIntAsDouble, failureCases);

            strictInt52Result(resultGPR, node);
            break;
        }

        default:
            DFG_CRASH(m_graph, node, "Bad use kind");
        }
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

    case ArithAbs:
        compileArithAbs(node);
        break;
        
    case ArithMin:
    case ArithMax: {
        compileArithMinMax(node);
        break;
    }

    case ValuePow:
        compileValuePow(node);
        break;

    case ArithPow:
        compileArithPow(node);
        break;

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

    case StringCodePointAt: {
        compileStringCodePointAt(node);
        break;
    }

    case StringIndexOf: {
        compileStringIndexOf(node);
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

    case StringLocaleCompare: {
        compileStringLocaleCompare(node);
        break;
    }

    case CheckDetached: {
        compileCheckDetached(node);
        break;
    }

    case CheckArrayOrEmpty:
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
        JSValueRegsTemporary result;
        compileGetByVal(node, scopedLambda<std::tuple<JSValueRegs, DataFormat, CanUseFlush>(DataFormat preferredFormat)>([&] (DataFormat preferredFormat) {
            JSValueRegs resultRegs;
            switch (preferredFormat) {
            case DataFormatDouble:
                break;
            default: {
                result = JSValueRegsTemporary(this);
                resultRegs = result.regs();
                break;
            }
            };
            return std::tuple { resultRegs, preferredFormat, CanUseFlush::Yes };
        }));
        break;
    }

    case GetByValWithThis: {
        compileGetByValWithThis(node);
        break;
    }

    case GetByValWithThisMegamorphic: {
        compileGetByValWithThisMegamorphic(node);
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

    case PutByValMegamorphic: {
        compilePutByValMegamorphic(node);
        break;
    }

    case AtomicsAdd:
    case AtomicsAnd:
    case AtomicsCompareExchange:
    case AtomicsExchange:
    case AtomicsLoad:
    case AtomicsOr:
    case AtomicsStore:
    case AtomicsSub:
    case AtomicsXor: {
        unsigned numExtraArgs = numExtraAtomicsArgs(node->op());
        Edge baseEdge = m_graph.child(node, 0);
        Edge indexEdge = m_graph.child(node, 1);
        Edge argEdges[maxNumExtraAtomicsArgs];
        for (unsigned i = numExtraArgs; i--;)
            argEdges[i] = m_graph.child(node, 2 + i);
        Edge storageEdge = m_graph.child(node, 2 + numExtraArgs);

        GPRReg baseGPR;
        GPRReg indexGPR;
        GPRReg argGPRs[2];
        GPRReg resultGPR;

        if (!storageEdge) {
            auto callSlowPath = [&] () {
                auto globalObjectImmPtr = LinkableConstant::globalObject(*this, node);
                switch (node->op()) {
                case AtomicsAdd:
                    callOperation(operationAtomicsAdd, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                case AtomicsAnd:
                    callOperation(operationAtomicsAnd, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                case AtomicsCompareExchange:
                    callOperation(operationAtomicsCompareExchange, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0], argGPRs[1]);
                    break;
                case AtomicsExchange:
                    callOperation(operationAtomicsExchange, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                case AtomicsLoad:
                    callOperation(operationAtomicsLoad, resultGPR, globalObjectImmPtr, baseGPR, indexGPR);
                    break;
                case AtomicsOr:
                    callOperation(operationAtomicsOr, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                case AtomicsStore:
                    callOperation(operationAtomicsStore, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                case AtomicsSub:
                    callOperation(operationAtomicsSub, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                case AtomicsXor:
                    callOperation(operationAtomicsXor, resultGPR, globalObjectImmPtr, baseGPR, indexGPR, argGPRs[0]);
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
            };

            // We are in generic mode!
            JSValueOperand base(this, baseEdge);
            JSValueOperand index(this, indexEdge);
            std::optional<JSValueOperand> args[2];
            baseGPR = base.gpr();
            indexGPR = index.gpr();
            for (unsigned i = numExtraArgs; i--;) {
                args[i].emplace(this, argEdges[i]);
                argGPRs[i] = args[i]->gpr();
            }
            
            flushRegisters();
            GPRFlushedCallResult result(this);
            resultGPR = result.gpr();
            callSlowPath();
            exceptionCheck();
            
            jsValueResult(resultGPR, node);
            break;
        }
        
        TypedArrayType type = node->arrayMode().typedArrayType();
        
        SpeculateCellOperand base(this, baseEdge);
        SpeculateStrictInt32Operand index(this, indexEdge);
        GPRTemporary scratch(this);

        baseGPR = base.gpr();
        indexGPR = index.gpr();
        GPRReg scratchGPR = scratch.gpr();

        std::optional<GPRTemporary> scratch2;
        GPRReg scratch2GPR = InvalidGPRReg;
        if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
            scratch2.emplace(this);
            scratch2GPR = scratch2->gpr();
        }
        
        emitTypedArrayBoundsCheck(node, baseGPR, indexGPR, scratchGPR, scratch2GPR);
        
        GPRTemporary args[2];
        
        bool ok = true;
        for (unsigned i = numExtraArgs; i--;) {
            if (!getIntTypedArrayStoreOperandForAtomics(args[i], indexGPR, argEdges[i])) {
                noResult(node);
                ok = false;
            }
            argGPRs[i] = args[i].gpr();
        }
        if (!ok)
            break;
        
        StorageOperand storage(this, storageEdge);
        GPRTemporary oldValue(this);
        GPRTemporary result(this);
        GPRTemporary newValue(this);
        GPRReg storageGPR = storage.gpr();
        GPRReg oldValueGPR = oldValue.gpr();
        resultGPR = result.gpr();
        GPRReg newValueGPR = newValue.gpr();
        std::optional<FPRTemporary> fprTemp;
        FPRReg resultFPR = InvalidFPRReg;
        if (elementSize(type) == 4 && !JSC::isSigned(type)) {
            fprTemp.emplace(this);
            resultFPR = fprTemp->fpr();
        }
        
        // FIXME: It shouldn't be necessary to nop-pad between register allocation and a jump label.
        // https://bugs.webkit.org/show_bug.cgi?id=170974
        nop();
        
        Label loop = label();
        
        loadFromIntTypedArray(storageGPR, indexGPR, oldValueGPR, type);
        move(oldValueGPR, newValueGPR);
        move(oldValueGPR, resultGPR);
        
        switch (node->op()) {
        case AtomicsAdd:
            add32(argGPRs[0], newValueGPR);
            break;
        case AtomicsAnd:
            and32(argGPRs[0], newValueGPR);
            break;
        case AtomicsCompareExchange: {
            switch (elementSize(type)) {
            case 1:
                if (JSC::isSigned(type))
                    signExtend8To32(argGPRs[0], argGPRs[0]);
                else
                    and32(TrustedImm32(0xff), argGPRs[0]);
                break;
            case 2:
                if (JSC::isSigned(type))
                    signExtend16To32(argGPRs[0], argGPRs[0]);
                else
                    and32(TrustedImm32(0xffff), argGPRs[0]);
                break;
            case 4:
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            Jump fail = branch32(NotEqual, oldValueGPR, argGPRs[0]);
            move(argGPRs[1], newValueGPR);
            fail.link(this);
            break;
        }
        case AtomicsExchange:
            move(argGPRs[0], newValueGPR);
            break;
        case AtomicsLoad:
            break;
        case AtomicsOr:
            or32(argGPRs[0], newValueGPR);
            break;
        case AtomicsStore:
            move(argGPRs[0], newValueGPR);
            move(argGPRs[0], resultGPR);
            break;
        case AtomicsSub:
            sub32(argGPRs[0], newValueGPR);
            break;
        case AtomicsXor:
            xor32(argGPRs[0], newValueGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        JumpList success;
        switch (elementSize(type)) {
        case 1:
            success = branchAtomicWeakCAS8(Success, oldValueGPR, newValueGPR, BaseIndex(storageGPR, indexGPR, TimesOne));
            break;
        case 2:
            success = branchAtomicWeakCAS16(Success, oldValueGPR, newValueGPR, BaseIndex(storageGPR, indexGPR, TimesTwo));
            break;
        case 4:
            success = branchAtomicWeakCAS32(Success, oldValueGPR, newValueGPR, BaseIndex(storageGPR, indexGPR, TimesFour));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        jump().linkTo(loop, this);
        
        success.link(this);

        if (node->op() == AtomicsStore) {
            Edge operand = argEdges[0];
            switch (operand.useKind()) {
            case Int32Use:
                zeroExtend32ToWord(resultGPR, resultGPR);
                strictInt32Result(resultGPR, node);
                break;
            case Int52RepUse:
                strictInt52Result(resultGPR, node);
                break;
            default:
                DFG_CRASH(m_graph, node, "Bad result type");
                break;
            }
            break;
        }
        constexpr bool canSpeculate = false;
        constexpr bool shouldBox = false;
        setIntTypedArrayLoadResult(node, JSValueRegs(resultGPR), type, canSpeculate, shouldBox, resultFPR);
        break;
    }
        
    case AtomicsIsLockFree: {
        Edge child1 = m_graph.child(node, 0);
        if (child1.useKind() != Int32Use) {
            JSValueOperand operand(this, child1);
            GPRReg operandGPR = operand.gpr();
            flushRegisters();
            GPRFlushedCallResult result(this);
            GPRReg resultGPR = result.gpr();
            callOperation(operationAtomicsIsLockFree, resultGPR, LinkableConstant::globalObject(*this, node), operandGPR);
            exceptionCheck();
            jsValueResult(resultGPR, node);
            break;
        }

        SpeculateInt32Operand operand(this, child1);
        GPRTemporary result(this);
        GPRReg operandGPR = operand.gpr();
        GPRReg resultGPR = result.gpr();
        move(TrustedImm32(JSValue::ValueTrue), resultGPR);
        JumpList done;
        done.append(branch32(Equal, operandGPR, TrustedImm32(8)));
        done.append(branch32(Equal, operandGPR, TrustedImm32(4)));
        done.append(branch32(Equal, operandGPR, TrustedImm32(1)));
        done.append(branch32(Equal, operandGPR, TrustedImm32(2)));
        move(TrustedImm32(JSValue::ValueFalse), resultGPR);
        done.link(this);
        jsValueResult(resultGPR, node);
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

    case RegExpTest: {
        compileRegExpTest(node);
        break;
    }

    case RegExpTestInline: {
        compileRegExpTestInline(node);
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

    case ArraySlice: {
        compileArraySlice(node);
        break;
    }

    case ArraySpliceExtract: {
        compileArraySpliceExtract(node);
        break;
    }

    case ArrayIndexOf: {
        compileArrayIndexOf(node);
        break;
    }
        
    case ArrayPop: {
        ASSERT(node->arrayMode().isJSArray());

        SpeculateCellOperand base(this, node->child1());
        StorageOperand storage(this, node->child2());
        GPRTemporary value(this);
        GPRTemporary storageLength(this);
        FPRTemporary temp(this); // This is kind of lame, since we don't always need it. I'm relying on the fact that we don't have FPR pressure, especially in code that uses pop().
        
        GPRReg baseGPR = base.gpr();
        GPRReg storageGPR = storage.gpr();
        GPRReg valueGPR = value.gpr();
        GPRReg storageLengthGPR = storageLength.gpr();
        FPRReg tempFPR = temp.fpr();
        
        switch (node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            load32(
                Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            Jump undefinedCase =
                branchTest32(Zero, storageLengthGPR);
            sub32(TrustedImm32(1), storageLengthGPR);
            store32(
                storageLengthGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
            Jump slowCase;
            if (node->arrayMode().type() == Array::Double) {
                loadDouble(
                    BaseIndex(storageGPR, storageLengthGPR, TimesEight),
                    tempFPR);
                // FIXME: This would not have to be here if changing the publicLength also zeroed the values between the old
                // length and the new length.
                store64(
                    TrustedImm64(bitwise_cast<int64_t>(PNaN)), BaseIndex(storageGPR, storageLengthGPR, TimesEight));
                slowCase = branchIfNaN(tempFPR);
                boxDouble(tempFPR, valueGPR);
            } else {
                load64(
                    BaseIndex(storageGPR, storageLengthGPR, TimesEight),
                    valueGPR);
                // FIXME: This would not have to be here if changing the publicLength also zeroed the values between the old
                // length and the new length.
                store64(
                TrustedImm64((int64_t)0), BaseIndex(storageGPR, storageLengthGPR, TimesEight));
                slowCase = branchIfEmpty(valueGPR);
            }

            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    TrustedImm64(JSValue::encode(jsUndefined())), valueGPR));
            addSlowPathGenerator(
                slowPathCall(
                    slowCase, this, operationArrayPopAndRecoverLength, valueGPR, LinkableConstant::globalObject(*this, node), baseGPR));
            
            // We can't know for sure that the result is an int because of the slow paths. :-/
            jsValueResult(valueGPR, node);
            break;
        }
            
        case Array::ArrayStorage: {
            load32(Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);
        
            Jump undefinedCase =
                branchTest32(Zero, storageLengthGPR);
        
            sub32(TrustedImm32(1), storageLengthGPR);
        
            JumpList slowCases;
            slowCases.append(branch32(AboveOrEqual, storageLengthGPR, Address(storageGPR, ArrayStorage::vectorLengthOffset())));
        
            load64(BaseIndex(storageGPR, storageLengthGPR, TimesEight, ArrayStorage::vectorOffset()), valueGPR);
            slowCases.append(branchIfEmpty(valueGPR));
        
            store32(storageLengthGPR, Address(storageGPR, ArrayStorage::lengthOffset()));
        
            store64(TrustedImm64((int64_t)0), BaseIndex(storageGPR, storageLengthGPR, TimesEight,  ArrayStorage::vectorOffset()));
            sub32(TrustedImm32(1), Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        
            addSlowPathGenerator(
                slowPathMove(
                    undefinedCase, this,
                    TrustedImm64(JSValue::encode(jsUndefined())), valueGPR));
        
            addSlowPathGenerator(
                slowPathCall(
                    slowCases, this, operationArrayPop, valueGPR, LinkableConstant::globalObject(*this, node), baseGPR));

            jsValueResult(valueGPR, node);
            break;
        }
            
        default:
            CRASH();
            break;
        }
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
        ASSERT(GPRInfo::callFrameRegister != GPRInfo::regT1);
        ASSERT(GPRInfo::regT1 != GPRInfo::returnValueGPR);
        ASSERT(GPRInfo::returnValueGPR != GPRInfo::callFrameRegister);

        // Return the result in returnValueGPR.
        JSValueOperand op1(this, node->child1());
        move(op1.gpr(), GPRInfo::returnValueGPR);

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
            JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
            GPRTemporary result(this); // FIXME: We could reuse, but on speculation fail would need recovery to restore tag (akin to add).
            
            move(value.gpr(), result.gpr());
            xor64(TrustedImm32(JSValue::ValueFalse), result.gpr());
            DFG_TYPE_CHECK(
                JSValueRegs(value.gpr()), node->child1(), SpecBoolean, branchTest64(
                    NonZero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1))));

            strictInt32Result(result.gpr(), node);
            break;
        }
            
        case UntypedUse: {
            JSValueOperand value(this, node->child1());
            GPRTemporary result(this);
            
            if (!m_interpreter.needsTypeCheck(node->child1(), SpecBoolInt32 | SpecBoolean)) {
                move(value.gpr(), result.gpr());
                and32(TrustedImm32(1), result.gpr());
                strictInt32Result(result.gpr(), node);
                break;
            }
            
            move(value.gpr(), result.gpr());
            xor64(TrustedImm32(JSValue::ValueFalse), result.gpr());
            Jump isBoolean = branchTest64(
                Zero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1)));
            move(value.gpr(), result.gpr());
            Jump done = jump();
            isBoolean.link(this);
            or64(GPRInfo::numberTagRegister, result.gpr());
            done.link(this);
            
            jsValueResult(result.gpr(), node);
            break;
        }
            
        default:
            DFG_CRASH(m_graph, node, "Bad use kind");
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
        switch (node->child1().useKind()) {
        case StringUse: {
            SpeculateCellOperand argument(this, node->child1());
            GPRReg argumentGPR = argument.gpr();
            speculateString(node->child1(), argumentGPR);

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationToNumberString, resultRegs, LinkableConstant::globalObject(*this, node), argumentGPR);
            exceptionCheck();

            jsValueResult(resultRegs, node);
            break;
        }
        case UntypedUse: {
            JSValueOperand argument(this, node->child1());
            GPRTemporary result(this, Reuse, argument);

            GPRReg argumentGPR = argument.gpr();
            GPRReg resultGPR = result.gpr();

            argument.use();

            // We have several attempts to remove ToNumber. But ToNumber still exists.
            // It means that converting non-numbers to numbers by this ToNumber is not rare.
            // Instead of the slow path generator, we emit callOperation here.
            if (!(m_state.forNode(node->child1()).m_type & SpecBytecodeNumber)) {
                flushRegisters();
                callOperation(operationToNumber, resultGPR, LinkableConstant::globalObject(*this, node), argumentGPR);
                exceptionCheck();
            } else {
                Jump notNumber = branchIfNotNumber(argumentGPR);
                move(argumentGPR, resultGPR);
                Jump done = jump();

                notNumber.link(this);
                silentSpillAllRegisters(resultGPR);
                callOperation(operationToNumber, resultGPR, LinkableConstant::globalObject(*this, node), argumentGPR);
                silentFillAllRegisters();
                exceptionCheck();

                done.link(this);
            }

            jsValueResult(resultGPR, node, UseChildrenCalledExplicitly);
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

    case FunctionToString:
        compileFunctionToString(node);
        break;

    case FunctionBind:
        compileFunctionBind(node);
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

    case GetRestLength: {
        compileGetRestLength(node);
        break;
    }
        
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

    case GetByIdMegamorphic: {
        compileGetByIdMegamorphic(node);
        break;
    }

    case GetByValMegamorphic: {
        compileGetByValMegamorphic(node);
        break;
    }

    case GetByIdWithThis: {
        if (node->child1().useKind() == CellUse && node->child2().useKind() == CellUse) {
            SpeculateCellOperand base(this, node->child1());
            SpeculateCellOperand thisValue(this, node->child2());

            GPRReg baseGPR = base.gpr();
            GPRReg thisValueGPR = thisValue.gpr();
            
            GPRFlushedCallResult result(this);
            GPRReg resultGPR = result.gpr();

            std::optional<GPRTemporary> stubInfo;
            std::optional<GPRTemporary> scratch;
            GPRReg stubInfoGPR = InvalidGPRReg;
            GPRReg scratchGPR = InvalidGPRReg;
            if (m_graph.m_plan.isUnlinked()) {
                stubInfo.emplace(this);
                scratch.emplace(this);
                stubInfoGPR = stubInfo->gpr();
                scratchGPR = scratch->gpr();
            }
            
            flushRegisters();
            
            cachedGetByIdWithThis(node, node->origin.semantic, baseGPR, thisValueGPR, resultGPR, stubInfoGPR, scratchGPR, node->cacheableIdentifier(), JumpList());
            
            jsValueResult(resultGPR, node);
            
        } else {
            JSValueOperand base(this, node->child1());
            JSValueOperand thisValue(this, node->child2());

            GPRReg baseGPR = base.gpr();
            GPRReg thisValueGPR = thisValue.gpr();
            
            GPRFlushedCallResult result(this);
            GPRReg resultGPR = result.gpr();

            std::optional<GPRTemporary> stubInfo;
            std::optional<GPRTemporary> scratch;
            GPRReg stubInfoGPR = InvalidGPRReg;
            GPRReg scratchGPR = InvalidGPRReg;
            if (m_graph.m_plan.isUnlinked()) {
                stubInfo.emplace(this);
                scratch.emplace(this);
                stubInfoGPR = stubInfo->gpr();
                scratchGPR = scratch->gpr();
            }
            
            flushRegisters();
            
            JumpList notCellList;
            notCellList.append(branchIfNotCell(JSValueRegs(baseGPR)));
            notCellList.append(branchIfNotCell(JSValueRegs(thisValueGPR)));
            
            cachedGetByIdWithThis(node, node->origin.semantic, baseGPR, thisValueGPR, resultGPR, stubInfoGPR, scratchGPR, node->cacheableIdentifier(), notCellList);
            
            jsValueResult(resultGPR, node);
        }
        
        break;
    }

    case GetByIdWithThisMegamorphic:
        compileGetByIdWithThisMegamorphic(node);
        break;

    case GetArrayLength:
        compileGetArrayLength(node);
        break;

    case GetTypedArrayLengthAsInt52:
#if USE(LARGE_TYPED_ARRAYS)
        compileGetTypedArrayLengthAsInt52(node);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
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

    case AssertNotEmpty: {
        if (validationEnabled()) {
            JSValueOperand operand(this, node->child1());
            GPRReg input = operand.gpr();
            auto done = branchIfNotEmpty(input);
            breakpoint();
            done.link(this);
        }
        noResult(node);
        break;
    }

    case CheckIdent:
        compileCheckIdent(node);
        break;

    case GetExecutable: {
        compileGetExecutable(node);
        break;
    }
        
    case CheckStructureOrEmpty: {
        SpeculateCellOperand cell(this, node->child1());
        GPRReg cellGPR = cell.gpr();

        GPRReg tempGPR = InvalidGPRReg;
        std::optional<GPRTemporary> temp;
        if (node->structureSet().size() > 1) {
            temp.emplace(this);
            tempGPR = temp->gpr();
        }

        Jump isEmpty;
        if (m_interpreter.forNode(node->child1()).m_type & SpecEmpty)
            isEmpty = branchIfEmpty(cellGPR);

        emitStructureCheck(node, cellGPR, tempGPR);

        if (isEmpty.isSet())
            isEmpty.link(this);

        noResult(node);
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
        store32(TrustedImm32(newStructure->id().bits()), Address(baseGPR, JSCell::structureIDOffset()));
        
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
#if USE(LARGE_TYPED_ARRAYS)
        compileGetTypedArrayByteOffsetAsInt52(node);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
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

    case GetByOffset:
    case GetGetterSetterByOffset: {
        compileGetByOffset(node);
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

    case PutByIdMegamorphic: {
        compilePutByIdMegamorphic(node);
        break;
    }

    case PutByIdWithThis: {
        compilePutByIdWithThis(node);
        break;
    }

    case PutByValWithThis: {
        JSValueOperand base(this, m_graph.varArgChild(node, 0));
        GPRReg baseGPR = base.gpr();
        JSValueOperand thisValue(this, m_graph.varArgChild(node, 1));
        GPRReg thisValueGPR = thisValue.gpr();
        JSValueOperand property(this, m_graph.varArgChild(node, 2));
        GPRReg propertyGPR = property.gpr();
        JSValueOperand value(this, m_graph.varArgChild(node, 3));
        GPRReg valueGPR = value.gpr();

        flushRegisters();
        callOperation(node->ecmaMode().isStrict() ? operationPutByValWithThisStrict : operationPutByValWithThis, LinkableConstant::globalObject(*this, node), baseGPR, thisValueGPR, propertyGPR, valueGPR);
        exceptionCheck();

        noResult(node);
        break;
    }

    case PutByIdDirect: {
        compilePutByIdDirect(node);
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

    case NotifyWrite: {
        compileNotifyWrite(node);
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

    case ParseInt: {
        compileParseInt(node);
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
        GPRTemporary result(this, Reuse, value);

        comparePtr(Equal, value.gpr(), TrustedImm32(JSValue::encode(JSValue())), result.gpr());
        or32(TrustedImm32(JSValue::ValueFalse), result.gpr());

        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
        break;
    }
        
    case TypeOfIsUndefined: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this);
        GPRTemporary localGlobalObject(this);
        GPRTemporary remoteGlobalObject(this);

        Jump isCell = branchIfCell(value.jsValueRegs());

        compare64(Equal, value.gpr(), TrustedImm32(JSValue::ValueUndefined), result.gpr());
        Jump done = jump();
        
        isCell.link(this);
        Jump notMasqueradesAsUndefined;
        if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
            move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = jump();
        } else {
            Jump isMasqueradesAsUndefined = branchTest8(
                NonZero,
                Address(value.gpr(), JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined));
            move(TrustedImm32(0), result.gpr());
            notMasqueradesAsUndefined = jump();

            isMasqueradesAsUndefined.link(this);
            GPRReg localGlobalObjectGPR = localGlobalObject.gpr();
            GPRReg remoteGlobalObjectGPR = remoteGlobalObject.gpr();
            loadLinkableConstant(LinkableConstant::globalObject(*this, node), localGlobalObjectGPR);
            emitLoadStructure(vm(), value.gpr(), result.gpr());
            loadPtr(Address(result.gpr(), Structure::globalObjectOffset()), remoteGlobalObjectGPR);
            comparePtr(Equal, localGlobalObjectGPR, remoteGlobalObjectGPR, result.gpr());
        }

        notMasqueradesAsUndefined.link(this);
        done.link(this);
        or32(TrustedImm32(JSValue::ValueFalse), result.gpr());
        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
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
        GPRTemporary result(this, Reuse, value);

        GPRReg valueGPR = value.gpr();
        GPRReg resultGPR = result.gpr();

        move(valueGPR, resultGPR);
        and64(TrustedImm32(~JSValue::UndefinedTag), resultGPR);
        compare64(Equal, resultGPR, TrustedImm32(JSValue::ValueNull), resultGPR);

        unblessedBooleanResult(resultGPR, node);
        break;
    }
        
    case IsBoolean: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);
        
        move(value.gpr(), result.gpr());
        xor64(TrustedImm32(JSValue::ValueFalse), result.gpr());
        test64(Zero, result.gpr(), TrustedImm32(static_cast<int32_t>(~1)), result.gpr());
        or32(TrustedImm32(JSValue::ValueFalse), result.gpr());
        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
        break;
    }
        
    case IsNumber: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);
        
        test64(NonZero, value.gpr(), GPRInfo::numberTagRegister, result.gpr());
        or32(TrustedImm32(JSValue::ValueFalse), result.gpr());
        jsValueResult(result.gpr(), node, DataFormatJSBoolean);
        break;
    }

    case IsBigInt: {
#if USE(BIGINT32)
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        Jump isCell = branchIfCell(value.gpr());

        move(TrustedImm64(JSValue::BigInt32Mask), resultGPR);
        and64(value.gpr(), result.gpr());
        compare64(Equal, resultGPR, TrustedImm32(JSValue::BigInt32Tag), resultGPR);
        Jump continuation = jump();

        isCell.link(this);
        JSValueRegs valueRegs = value.jsValueRegs();
        compare8(Equal, Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()), TrustedImm32(HeapBigIntType), resultGPR);

        continuation.link(this);
        unblessedBooleanResult(resultGPR, node);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
        break;
    }

    case NumberIsInteger: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);

        FPRTemporary temp1(this);
        FPRTemporary temp2(this);

        JSValueRegs valueRegs = JSValueRegs(value.gpr());
        GPRReg resultGPR = result.gpr();

        FPRReg tempFPR1 = temp1.fpr();
        FPRReg tempFPR2 = temp2.fpr();

        JumpList done;

        auto isInt32 = branchIfInt32(valueRegs);
        auto notNumber = branchIfNotDoubleKnownNotInt32(valueRegs);

        // We're a double here.
        unboxDouble(valueRegs.gpr(), resultGPR, tempFPR1);
        urshift64(TrustedImm32(52), resultGPR);
        and32(TrustedImm32(0x7ff), resultGPR);
        auto notNanNorInfinity = branch32(NotEqual, TrustedImm32(0x7ff), resultGPR);
        move(TrustedImm32(JSValue::ValueFalse), resultGPR);
        done.append(jump());

        notNanNorInfinity.link(this);
        roundTowardZeroDouble(tempFPR1, tempFPR2);
        compareDouble(DoubleEqualAndOrdered, tempFPR1, tempFPR2, resultGPR);
        or32(TrustedImm32(JSValue::ValueFalse), resultGPR);
        done.append(jump());

        isInt32.link(this);
        move(TrustedImm32(JSValue::ValueTrue), resultGPR);
        done.append(jump());

        notNumber.link(this);
        move(TrustedImm32(JSValue::ValueFalse), resultGPR);

        done.link(this);
        jsValueResult(resultGPR, node, DataFormatJSBoolean);
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

    case MapHash: {
        switch (node->child1().useKind()) {
#if USE(BIGINT32)
        case BigInt32Use:
#endif
        case BooleanUse:
        case Int32Use:
        case SymbolUse:
        case ObjectUse: {
            JSValueOperand input(this, node->child1(), ManualOperandSpeculation);
            GPRTemporary result(this, Reuse, input);
            GPRTemporary temp(this);

            GPRReg inputGPR = input.gpr();
            GPRReg resultGPR = result.gpr();
            GPRReg tempGPR = temp.gpr();

            speculate(node, node->child1());

            move(inputGPR, resultGPR);
            wangsInt64Hash(resultGPR, tempGPR);
            strictInt32Result(resultGPR, node);
            break;
        }
        case HeapBigIntUse: {
            SpeculateCellOperand input(this, node->child1());
            GPRReg inputGPR = input.gpr();

            speculateHeapBigInt(node->child1(), inputGPR);

            flushRegisters();
            GPRFlushedCallResult result(this);
            GPRReg resultGPR = result.gpr();
            callOperation(operationMapHashHeapBigInt, resultGPR, TrustedImmPtr(&vm()), inputGPR);
            strictInt32Result(resultGPR, node);
            break;
        }
        case CellUse:
        case StringUse: {
            SpeculateCellOperand input(this, node->child1());
            GPRTemporary result(this);
            std::optional<GPRTemporary> temp;

            GPRReg tempGPR = InvalidGPRReg;
            if (node->child1().useKind() == CellUse) {
                temp.emplace(this);
                tempGPR = temp->gpr();
            }

            GPRReg inputGPR = input.gpr();
            GPRReg resultGPR = result.gpr();

            JumpList slowPath;
            JumpList done;

            if (node->child1().useKind() == StringUse)
                speculateString(node->child1(), inputGPR);
            else {
                auto isString = branchIfString(inputGPR);
                auto isHeapBigInt = branchIfHeapBigInt(inputGPR);
                move(inputGPR, resultGPR);
                wangsInt64Hash(resultGPR, tempGPR);
                addSlowPathGenerator(slowPathCall(isHeapBigInt, this, operationMapHashHeapBigInt, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, resultGPR, TrustedImmPtr(&vm()), inputGPR));
                done.append(jump());
                isString.link(this);
            }

            loadPtr(Address(inputGPR, JSString::offsetOfValue()), resultGPR);
            slowPath.append(branchIfRopeStringImpl(resultGPR));
            load32(Address(resultGPR, StringImpl::flagsOffset()), resultGPR);
            urshift32(TrustedImm32(StringImpl::s_flagCount), resultGPR);
            slowPath.append(branchTest32(Zero, resultGPR));
            done.append(jump());

            slowPath.link(this);
            silentSpillAllRegisters(resultGPR);
            callOperation(operationMapHash, resultGPR, LinkableConstant::globalObject(*this, node), JSValueRegs(inputGPR));
            silentFillAllRegisters();
            exceptionCheck();

            done.link(this);
            strictInt32Result(resultGPR, node);
            break;
        }
        default:
            RELEASE_ASSERT(node->child1().useKind() == UntypedUse);
            break;
        }
        
        if (node->child1().useKind() != UntypedUse)
            break;

        JSValueOperand input(this, node->child1());
        GPRTemporary temp(this);
        GPRTemporary result(this);

        GPRReg inputGPR = input.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();

        JumpList straightHash;
        JumpList done;
        straightHash.append(branchIfNotCell(inputGPR));
        JumpList slowPath;
        auto isHeapBigInt = branchIfHeapBigInt(inputGPR);
        straightHash.append(branchIfNotString(inputGPR));
        loadPtr(Address(inputGPR, JSString::offsetOfValue()), resultGPR);
        slowPath.append(branchIfRopeStringImpl(resultGPR));
        load32(Address(resultGPR, StringImpl::flagsOffset()), resultGPR);
        urshift32(TrustedImm32(StringImpl::s_flagCount), resultGPR);
        slowPath.append(branchTest32(Zero, resultGPR));
        done.append(jump());

        straightHash.link(this);
        move(inputGPR, resultGPR);
        wangsInt64Hash(resultGPR, tempGPR);
        addSlowPathGenerator(slowPathCall(isHeapBigInt, this, operationMapHashHeapBigInt, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, resultGPR, TrustedImmPtr(&vm()), inputGPR));
        done.append(jump());

        slowPath.link(this);
        silentSpillAllRegisters(resultGPR);
        callOperation(operationMapHash, resultGPR, LinkableConstant::globalObject(*this, node), JSValueRegs(inputGPR));
        silentFillAllRegisters();
        exceptionCheck();

        done.link(this);
        strictInt32Result(resultGPR, node);
        break;
    }

    case NormalizeMapKey: {
        compileNormalizeMapKey(node);
        break;
    }

    case GetMapBucket: {
        SpeculateCellOperand map(this, node->child1());
        JSValueOperand key(this, node->child2(), ManualOperandSpeculation);
        SpeculateInt32Operand hash(this, node->child3());
        GPRTemporary mask(this);
        GPRTemporary index(this);
        GPRTemporary buffer(this);
        GPRTemporary bucket(this);
        GPRTemporary result(this);

        GPRReg hashGPR = hash.gpr();
        GPRReg mapGPR = map.gpr();
        GPRReg maskGPR = mask.gpr();
        GPRReg indexGPR = index.gpr();
        GPRReg bufferGPR = buffer.gpr();
        GPRReg bucketGPR = bucket.gpr();
        GPRReg keyGPR = key.gpr();
        GPRReg resultGPR = result.gpr();

        if (node->child1().useKind() == MapObjectUse)
            speculateMapObject(node->child1(), mapGPR);
        else if (node->child1().useKind() == SetObjectUse)
            speculateSetObject(node->child1(), mapGPR);
        else
            RELEASE_ASSERT_NOT_REACHED();

        if (node->child2().useKind() != UntypedUse)
            speculate(node, node->child2());

        JumpList notPresentInTable;

        loadPtr(Address(mapGPR, HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::offsetOfBuffer()), bufferGPR);
        notPresentInTable.append(branchTestPtr(Zero, bufferGPR));
        load32(Address(mapGPR, HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::offsetOfCapacity()), maskGPR);
        sub32(TrustedImm32(1), maskGPR);
        move(hashGPR, indexGPR);

        Label loop = label();
        JumpList done;
        JumpList slowPathCases;
        JumpList loopAround;

        and32(maskGPR, indexGPR);
        loadPtr(BaseIndex(bufferGPR, indexGPR, TimesEight), bucketGPR);
        move(bucketGPR, resultGPR);

        notPresentInTable.append(branchPtr(Equal,
            bucketGPR, TrustedImmPtr(bitwise_cast<size_t>(HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::emptyValue()))));
        loopAround.append(branchPtr(Equal,
            bucketGPR, TrustedImmPtr(bitwise_cast<size_t>(HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::deletedValue()))));

        load64(Address(bucketGPR, HashMapBucket<HashMapBucketDataKey>::offsetOfKey()), bucketGPR);

        // Perform Object.is()
        switch (node->child2().useKind()) {
        case BooleanUse:
#if USE(BIGINT32)
        case BigInt32Use:
#endif
        case Int32Use:
        case SymbolUse:
        case ObjectUse: {
            done.append(branch64(Equal, bucketGPR, keyGPR)); // They're definitely the same value, we found the bucket we were looking for!
            // Otherwise, loop around.
            break;
        }
        case CellUse: {
            // if (bucket.isString()) {
            //     if (key.isString())
            //         => slow path
            // } else if (bucket.isHeapBigInt()) {
            //     if (key.isHeapBigInt())
            //         => slow path
            // }
            done.append(branch64(Equal, bucketGPR, keyGPR));
            loopAround.append(branchIfNotCell(JSValueRegs(bucketGPR)));

            auto bucketIsString = branchIfString(bucketGPR);
            loopAround.append(branchIfNotHeapBigInt(bucketGPR));

            // bucket is HeapBigInt.
            slowPathCases.append(branchIfHeapBigInt(keyGPR));
            loopAround.append(jump());

            // bucket is String.
            bucketIsString.link(this);
            loopAround.append(branchIfNotString(keyGPR));
            slowPathCases.append(jump());
            break;
        }
        case StringUse: {
            done.append(branch64(Equal, bucketGPR, keyGPR)); // They're definitely the same value, we found the bucket we were looking for!
            loopAround.append(branchIfNotCell(JSValueRegs(bucketGPR)));
            loopAround.append(branchIfNotString(bucketGPR));
            slowPathCases.append(jump());
            break;
        }
        case HeapBigIntUse: {
            done.append(branch64(Equal, bucketGPR, keyGPR)); // They're definitely the same value, we found the bucket we were looking for!
            loopAround.append(branchIfNotCell(JSValueRegs(bucketGPR)));
            loopAround.append(branchIfNotHeapBigInt(bucketGPR));
            slowPathCases.append(jump());
            break;
        }
        case UntypedUse: { 
            done.append(branch64(Equal, bucketGPR, keyGPR)); // They're definitely the same value, we found the bucket we were looking for!
            // The input key and bucket's key are already normalized. So if 64-bit compare fails and one is not a cell, they're definitely not equal.
            loopAround.append(branchIfNotCell(JSValueRegs(bucketGPR)));
            // first is a cell here.
            loopAround.append(branchIfNotCell(JSValueRegs(keyGPR)));
            // Both are cells here.
            auto bucketIsString = branchIfString(bucketGPR);
            // bucket is not String.
            loopAround.append(branchIfNotHeapBigInt(bucketGPR));
            // bucket is HeapBigInt.
            slowPathCases.append(branchIfHeapBigInt(keyGPR));
            loopAround.append(jump());
            // bucket is String.
            bucketIsString.link(this);
            loopAround.append(branchIfNotString(keyGPR));
            slowPathCases.append(jump());
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

            
        if (!loopAround.empty())
            loopAround.link(this);

        add32(TrustedImm32(1), indexGPR);
        jump().linkTo(loop, this);

        if (!slowPathCases.empty()) {
            slowPathCases.link(this);
            silentSpillAllRegisters(indexGPR);
            if (node->child1().useKind() == MapObjectUse)
                callOperation(operationJSMapFindBucket, resultGPR, LinkableConstant::globalObject(*this, node), mapGPR, keyGPR, hashGPR);
            else
                callOperation(operationJSSetFindBucket, resultGPR, LinkableConstant::globalObject(*this, node), mapGPR, keyGPR, hashGPR);
            silentFillAllRegisters();
            exceptionCheck();
            done.append(jump());
        }

        notPresentInTable.link(this);
        if (node->child1().useKind() == MapObjectUse)
            loadLinkableConstant(LinkableConstant(*this, vm().sentinelMapBucket()), resultGPR);
        else
            loadLinkableConstant(LinkableConstant(*this, vm().sentinelSetBucket()), resultGPR);
        done.link(this);
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

    case Flush:
        break;

    case DFG::Call:
    case TailCall:
    case TailCallInlinedCaller:
    case Construct:
    case CallVarargs:
    case TailCallVarargs:
    case TailCallVarargsInlinedCaller:
    case CallForwardVarargs:
    case ConstructVarargs:
    case ConstructForwardVarargs:
    case TailCallForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
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

    case NewFunction:
    case NewGeneratorFunction:
    case NewAsyncGeneratorFunction:
    case NewAsyncFunction:
        compileNewFunction(node);
        break;

    case NewBoundFunction:
        compileNewBoundFunction(node);
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
        GPRReg keyGPR;
        if (node->child2().useKind() == UntypedUse) {
            keyAsValue.emplace(this, node->child2());
            keyGPR = keyAsValue->gpr();
        } else {
            ASSERT(node->child2().useKind() == StringUse || node->child2().useKind() == SymbolUse);
            keyAsCell.emplace(this, node->child2());
            keyGPR = keyAsCell->gpr();
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
            speculateSymbol(node->child2(), keyGPR);
            loadPtr(Address(keyGPR, Symbol::offsetOfSymbolImpl()), implGPR);
            break;
        }
        case StringUse: {
            speculateString(node->child2(), keyGPR);
            loadPtr(Address(keyGPR, JSString::offsetOfValue()), implGPR);
            slowPath.append(branchIfRopeStringImpl(implGPR));
            slowPath.append(branchTest32(
                Zero, Address(implGPR, StringImpl::flagsOffset()),
                TrustedImm32(StringImpl::flagIsAtom())));
            break;
        }
        case UntypedUse: {
            slowPath.append(branchIfNotCell(JSValueRegs(keyGPR)));
            auto isNotString = branchIfNotString(keyGPR);
            loadPtr(Address(keyGPR, JSString::offsetOfValue()), implGPR);
            slowPath.append(branchIfRopeStringImpl(implGPR));
            slowPath.append(branchTest32(
                Zero, Address(implGPR, StringImpl::flagsOffset()),
                TrustedImm32(StringImpl::flagIsAtom())));
            auto hasUniquedImpl = jump();

            isNotString.link(this);
            slowPath.append(branchIfNotSymbol(keyGPR));
            loadPtr(Address(keyGPR, Symbol::offsetOfSymbolImpl()), implGPR);

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
        if (hasOneBitSet(sizeof(HasOwnPropertyCache::Entry))) // is a power of 2
            lshift32(TrustedImm32(getLSBSet(sizeof(HasOwnPropertyCache::Entry))), hashGPR);
        else
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
        silentSpillAllRegisters(resultGPR);
        callOperation(operationHasOwnProperty, resultGPR, LinkableConstant::globalObject(*this, node), objectGPR, keyGPR);
        silentFillAllRegisters();
        exceptionCheck();

        done.link(this);
        or32(TrustedImm32(JSValue::ValueFalse), resultGPR);
        jsValueResult(resultGPR, node, DataFormatJSBoolean);
        break;
    }
        
    case CountExecution:
        add64(TrustedImm32(1), AbsoluteAddress(node->executionCounter()->address()));
        break;

    case SuperSamplerBegin:
        add32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
        break;

    case SuperSamplerEnd:
        sub32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
        break;

    case ForceOSRExit: {
        terminateSpeculativeExecution(InadequateCoverage, JSValueRegs(), nullptr);
        break;
    }
        
    case InvalidationPoint:
        compileInvalidationPoint(node);
        break;

    case CheckTraps:
        compileCheckTraps(node);
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

    case Unreachable:
        unreachable(node);
        break;

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

    case EnumeratorPutByVal: {
        compileEnumeratorPutByVal(node);
        break;
    }

    case ProfileType: {
        compileProfileType(node);
        break;
    }
    case ProfileControlFlow: {
        BasicBlockLocation* basicBlockLocation = node->basicBlockLocation();
        basicBlockLocation->emitExecuteCode(*this);
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

    case MaterializeNewObject:
        compileMaterializeNewObject(node);
        break;

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

    case ExtractCatchLocal: {
        compileExtractCatchLocal(node);
        break;
    }

    case ClearCatchLocals:
        compileClearCatchLocals(node);
        break;

    case DataViewGetFloat:
    case DataViewGetInt: {
        SpeculateCellOperand dataView(this, node->child1());
        GPRReg dataViewGPR = dataView.gpr();
        speculateDataViewObject(node->child1(), dataViewGPR);

        DataViewData data = node->dataViewData();

        SpeculateInt32Operand index(this, node->child2());
        GPRReg indexGPR = index.gpr();

        GPRTemporary temp1(this);
        GPRTemporary temp2(this);

        GPRReg t1 = temp1.gpr();
        GPRReg t2 = temp2.gpr();

        std::optional<SpeculateBooleanOperand> isLittleEndianOperand;
        if (node->child3())
            isLittleEndianOperand.emplace(this, node->child3());
        GPRReg isLittleEndianGPR = isLittleEndianOperand ? isLittleEndianOperand->gpr() : InvalidGPRReg;

        if (data.isResizable)
            loadTypedArrayLength(dataViewGPR, t1, t2, t1, TypeDataView);
        else {
            speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(dataViewGPR), node, branchTest8(NonZero, Address(dataViewGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));
#if USE(LARGE_TYPED_ARRAYS)
            load64(Address(dataViewGPR, JSArrayBufferView::offsetOfLength()), t1);
#else
            // No need for an explicit check against 0 here, negative indices are caught by the comparison with length right after
            load32(Address(dataViewGPR, JSArrayBufferView::offsetOfLength()), t1);
#endif
        }

        speculationCheck(OutOfBounds, JSValueRegs(), node, branch32(LessThan, indexGPR, TrustedImm32(0)));
        zeroExtend32ToWord(indexGPR, t2);
        if (data.byteSize > 1)
            add64(TrustedImm32(data.byteSize - 1), t2);
        speculationCheck(OutOfBounds, JSValueRegs(), node, branch64(AboveOrEqual, t2, t1));

        loadPtr(Address(dataViewGPR, JSArrayBufferView::offsetOfVector()), t2);
        cageTypedArrayStorage(dataViewGPR, t2, false);

        zeroExtend32ToWord(indexGPR, t1);
        auto baseIndex = BaseIndex(t2, t1, TimesOne);

        if (node->op() == DataViewGetInt) {
            switch (data.byteSize) {
            case 1:
                if (data.isSigned)
                    load8SignedExtendTo32(baseIndex, t2);
                else
                    load8(baseIndex, t2);
                strictInt32Result(t2, node);
                break;
            case 2: {
                auto emitLittleEndianLoad = [&] {
                    if (data.isSigned)
                        load16SignedExtendTo32(baseIndex, t2);
                    else
                        load16(baseIndex, t2);
                };
                auto emitBigEndianLoad = [&] {
                    load16(baseIndex, t2);
                    byteSwap16(t2);
                    if (data.isSigned)
                        signExtend16To32(t2, t2);
                };

                if (data.isLittleEndian == TriState::False)
                    emitBigEndianLoad();
                else if (data.isLittleEndian == TriState::True)
                    emitLittleEndianLoad();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianLoad();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianLoad();
                    done.link(this);
                }
                strictInt32Result(t2, node);
                break;
            }
            case 4: {
                load32(baseIndex, t2);

                if (data.isLittleEndian == TriState::False)
                    byteSwap32(t2);
                else if (data.isLittleEndian == TriState::Indeterminate) {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isLittleEndian = branchTest32(NonZero, isLittleEndianGPR, TrustedImm32(1));
                    byteSwap32(t2);
                    isLittleEndian.link(this);
                }

                if (data.isSigned)
                    strictInt32Result(t2, node);
                else
                    strictInt52Result(t2, node);
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            FPRTemporary result(this);
            FPRReg resultFPR = result.fpr();

            switch (data.byteSize) {
            case 4: {
                auto emitLittleEndianCode = [&] {
                    loadFloat(baseIndex, resultFPR);
                    convertFloatToDouble(resultFPR, resultFPR);
                };

                auto emitBigEndianCode = [&] {
                    load32(baseIndex, t2);
                    byteSwap32(t2);
                    move32ToFloat(t2, resultFPR);
                    convertFloatToDouble(resultFPR, resultFPR);
                };

                if (data.isLittleEndian == TriState::True)
                    emitLittleEndianCode();
                else if (data.isLittleEndian == TriState::False)
                    emitBigEndianCode();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianCode();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianCode();
                    done.link(this);
                }

                break;
            }
            case 8: {
                auto emitLittleEndianCode = [&] {
                    loadDouble(baseIndex, resultFPR);
                };

                auto emitBigEndianCode = [&] {
                    load64(baseIndex, t2);
                    byteSwap64(t2);
                    move64ToDouble(t2, resultFPR);
                };

                if (data.isLittleEndian == TriState::True)
                    emitLittleEndianCode();
                else if (data.isLittleEndian == TriState::False)
                    emitBigEndianCode();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianCode();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianCode();
                    done.link(this);
                }

                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            doubleResult(resultFPR, node);
        }

        break;
    }

    case DateGetInt32OrNaN:
    case DateGetTime:
        compileDateGet(node);
        break;

    case DateSetTime:
        compileDateSet(node);
        break;

    case DataViewSet: {
        SpeculateCellOperand dataView(this, m_graph.varArgChild(node, 0));
        GPRReg dataViewGPR = dataView.gpr();
        speculateDataViewObject(m_graph.varArgChild(node, 0), dataViewGPR);

        SpeculateInt32Operand index(this, m_graph.varArgChild(node, 1));
        GPRReg indexGPR = index.gpr();

        std::optional<SpeculateStrictInt52Operand> int52Value;
        std::optional<SpeculateDoubleOperand> doubleValue;
        std::optional<SpeculateInt32Operand> int32Value;
        std::optional<FPRTemporary> fprTemporary;
        GPRReg valueGPR = InvalidGPRReg;
        FPRReg valueFPR = InvalidFPRReg;
        FPRReg tempFPR = InvalidFPRReg;

        DataViewData data = node->dataViewData();

        Edge& valueEdge = m_graph.varArgChild(node, 2);
        switch (valueEdge.useKind()) {
        case Int32Use:
            int32Value.emplace(this, valueEdge);
            valueGPR = int32Value->gpr();
            break;
        case DoubleRepUse:
            doubleValue.emplace(this, valueEdge);
            valueFPR = doubleValue->fpr();
            if (data.byteSize == 4) {
                fprTemporary.emplace(this);
                tempFPR = fprTemporary->fpr();
            }
            break;
        case Int52RepUse:
            int52Value.emplace(this, valueEdge);
            valueGPR = int52Value->gpr();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        GPRTemporary temp1(this);
        GPRTemporary temp2(this);
        GPRTemporary temp3(this);

        GPRReg t1 = temp1.gpr();
        GPRReg t2 = temp2.gpr();
        GPRReg t3 = temp3.gpr();

        std::optional<SpeculateBooleanOperand> isLittleEndianOperand;
        if (m_graph.varArgChild(node, 3))
            isLittleEndianOperand.emplace(this, m_graph.varArgChild(node, 3));
        GPRReg isLittleEndianGPR = isLittleEndianOperand ? isLittleEndianOperand->gpr() : InvalidGPRReg;

        if (data.isResizable)
            loadTypedArrayLength(dataViewGPR, t1, t2, t1, TypeDataView);
        else {
            speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(dataViewGPR), node, branchTest8(NonZero, Address(dataViewGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));
#if USE(LARGE_TYPED_ARRAYS)
            load64(Address(dataViewGPR, JSArrayBufferView::offsetOfLength()), t1);
#else
            // No need for an explicit check against 0 here, negative indices are caught by the comparison with length right after
            load32(Address(dataViewGPR, JSArrayBufferView::offsetOfLength()), t1);
#endif
        }

        speculationCheck(OutOfBounds, JSValueRegs(), node, branch32(LessThan, indexGPR, TrustedImm32(0)));
        zeroExtend32ToWord(indexGPR, t2);
        if (data.byteSize > 1)
            add64(TrustedImm32(data.byteSize - 1), t2);
        speculationCheck(OutOfBounds, JSValueRegs(), node, branch64(AboveOrEqual, t2, t1));

        loadPtr(Address(dataViewGPR, JSArrayBufferView::offsetOfVector()), t2);
        cageTypedArrayStorage(dataViewGPR, t2, false);

        zeroExtend32ToWord(indexGPR, t1);
        auto baseIndex = BaseIndex(t2, t1, TimesOne);

        if (data.isFloatingPoint) {
            RELEASE_ASSERT(valueFPR != InvalidFPRReg);
            if (data.byteSize == 4) {
                RELEASE_ASSERT(tempFPR != InvalidFPRReg);
                convertDoubleToFloat(valueFPR, tempFPR);

                auto emitLittleEndianCode = [&] {
                    storeFloat(tempFPR, baseIndex);
                };

                auto emitBigEndianCode = [&] {
                    moveFloatTo32(tempFPR, t3);
                    byteSwap32(t3);
                    store32(t3, baseIndex);
                };

                if (data.isLittleEndian == TriState::False)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TriState::True)
                    emitLittleEndianCode();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianCode();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianCode();
                    done.link(this);
                }
            } else {
                RELEASE_ASSERT(data.byteSize == 8);
                RELEASE_ASSERT(valueFPR != InvalidFPRReg);

                auto emitLittleEndianCode = [&] {
                    storeDouble(valueFPR, baseIndex);
                };
                auto emitBigEndianCode = [&] {
                    moveDoubleTo64(valueFPR, t3);
                    byteSwap64(t3);
                    store64(t3, baseIndex);
                };

                if (data.isLittleEndian == TriState::False)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TriState::True)
                    emitLittleEndianCode();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianCode();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianCode();
                    done.link(this);
                }
            }
        } else {
            switch (data.byteSize) {
            case 1:
                RELEASE_ASSERT(valueEdge.useKind() == Int32Use);
                RELEASE_ASSERT(valueGPR != InvalidGPRReg);
                store8(valueGPR, baseIndex);
                break;
            case 2: {
                RELEASE_ASSERT(valueEdge.useKind() == Int32Use);
                RELEASE_ASSERT(valueGPR != InvalidGPRReg);

                auto emitLittleEndianCode = [&] {
                    store16(valueGPR, baseIndex);
                };
                auto emitBigEndianCode = [&] {
                    move(valueGPR, t3);
                    byteSwap16(t3);
                    store16(t3, baseIndex);
                };

                if (data.isLittleEndian == TriState::False)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TriState::True)
                    emitLittleEndianCode();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianCode();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianCode();
                    done.link(this);
                }
                break;
            }
            case 4: {
                RELEASE_ASSERT(valueEdge.useKind() == Int32Use || valueEdge.useKind() == Int52RepUse);

                auto emitLittleEndianCode = [&] {
                    store32(valueGPR, baseIndex);
                };

                auto emitBigEndianCode = [&] {
                    zeroExtend32ToWord(valueGPR, t3);
                    byteSwap32(t3);
                    store32(t3, baseIndex);
                };

                if (data.isLittleEndian == TriState::False)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TriState::True)
                    emitLittleEndianCode();
                else {
                    RELEASE_ASSERT(isLittleEndianGPR != InvalidGPRReg);
                    auto isBigEndian = branchTest32(Zero, isLittleEndianGPR, TrustedImm32(1));
                    emitLittleEndianCode();
                    auto done = jump();
                    isBigEndian.link(this);
                    emitBigEndianCode();
                    done.link(this);
                }

                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        noResult(node);
        break;
    }

#if ENABLE(FTL_JIT)        
    case CheckTierUpInLoop: {
        Jump callTierUp = branchAdd32(
            PositiveOrZero,
            TrustedImm32(Options::ftlTierUpCounterIncrementForLoop()),
            AbsoluteAddress(&jitCode()->tierUpCounter.m_counter));

        Label toNextOperation = label();

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, InvalidGPRReg);
        BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();

        addSlowPathGeneratorLambda([=, this]() {
            callTierUp.link(this);

            silentSpill(savePlans);
            callOperation(operationTriggerTierUpNowInLoop, TrustedImmPtr(&vm()), TrustedImm32(bytecodeIndex.asBits()));
            silentFill(savePlans);

            jump().linkTo(toNextOperation, this);
        });
        break;
    }
        
    case CheckTierUpAtReturn: {
        Jump done = branchAdd32(
            Signed,
            TrustedImm32(Options::ftlTierUpCounterIncrementForReturn()),
            AbsoluteAddress(&jitCode()->tierUpCounter.m_counter));
        
        silentSpillAllRegisters(InvalidGPRReg);
        callOperation(operationTriggerTierUpNow, TrustedImmPtr(&vm()));
        silentFillAllRegisters();
        
        done.link(this);
        break;
    }
        
    case CheckTierUpAndOSREnter: {
        ASSERT(!node->origin.semantic.inlineCallFrame());

        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();

        BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();
        auto triggerIterator = jitCode()->tierUpEntryTriggers.find(bytecodeIndex);
        DFG_ASSERT(m_graph, node, triggerIterator != jitCode()->tierUpEntryTriggers.end());
        JITCode::TriggerReason* forceEntryTrigger = &(jitCode()->tierUpEntryTriggers.find(bytecodeIndex)->value);
        static_assert(!static_cast<uint8_t>(JITCode::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
        static_assert(sizeof(JITCode::TriggerReason) == 1, "branchTest8 assumes this size");

        Jump forceOSREntry = branchTest8(NonZero, AbsoluteAddress(forceEntryTrigger));
        Jump overflowedCounter = branchAdd32(
            PositiveOrZero,
            TrustedImm32(Options::ftlTierUpCounterIncrementForLoop()),
            AbsoluteAddress(&jitCode()->tierUpCounter.m_counter));
        Label toNextOperation = label();

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, tempGPR);

        unsigned streamIndex = m_stream.size();
        jitCode()->bytecodeIndexToStreamIndex.add(bytecodeIndex, streamIndex);

        addSlowPathGeneratorLambda([=, this]() {
            forceOSREntry.link(this);
            overflowedCounter.link(this);

            silentSpill(savePlans);
            callOperation(operationTriggerOSREntryNow, tempGPR, TrustedImmPtr(&vm()), TrustedImm32(bytecodeIndex.asBits()));

            if (savePlans.isEmpty())
                branchTestPtr(Zero, tempGPR).linkTo(toNextOperation, this);
            else {
                Jump osrEnter = branchTestPtr(NonZero, tempGPR);
                silentFill(savePlans);
                jump().linkTo(toNextOperation, this);
                osrEnter.link(this);
            }
            emitRestoreCalleeSaves();
            farJump(tempGPR, GPRInfo::callFrameRegister);
        });
        break;
    }

#else // ENABLE(FTL_JIT)
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckTierUpAndOSREnter:
        DFG_CRASH(m_graph, node, "Unexpected tier-up node");
        break;
#endif // ENABLE(FTL_JIT)

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
    case EntrySwitch:
    case InitializeEntrypointArguments:
    case Phi:
    case Upsilon:
    case ExtractOSREntryLocal:
    case AssertInBounds:
    case CheckInBounds:
    case CheckInBoundsInt52:
    case ArithIMul:
    case MultiGetByOffset:
    case MultiPutByOffset:
    case MultiDeleteByOffset:
    case FiatInt52:
    case CheckBadValue:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewInternalFieldObject:
    case PhantomCreateActivation:
    case PhantomNewRegexp:
    case GetMyArgumentByVal:
    case GetMyArgumentByValOutOfBounds:
    case GetVectorLength:
    case PutHint:
    case CheckStructureImmediate:
    case MaterializeCreateActivation:
    case MaterializeNewInternalFieldObject:
    case PutStack:
    case KillStack:
    case GetStack:
    case PhantomCreateRest:
    case PhantomSpread:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case IdentityWithProfile:
    case CPUIntrinsic:
    case CallWasm:
        DFG_CRASH(m_graph, node, "Unexpected node");
        break;
    }

    if (!m_compileOkay)
        return;
    
    if (node->hasResult() && node->mustGenerate())
        use(node);
}

void SpeculativeJIT::moveTrueTo(GPRReg gpr)
{
    move(TrustedImm32(JSValue::ValueTrue), gpr);
}

void SpeculativeJIT::moveFalseTo(GPRReg gpr)
{
    move(TrustedImm32(JSValue::ValueFalse), gpr);
}

void SpeculativeJIT::blessBoolean(GPRReg gpr)
{
    or32(TrustedImm32(JSValue::ValueFalse), gpr);
}

void SpeculativeJIT::convertAnyInt(Edge valueEdge, GPRReg resultGPR)
{
    JSValueOperand value(this, valueEdge, ManualOperandSpeculation);
    GPRTemporary scratch1(this);
    FPRTemporary scratch2(this);
    FPRTemporary scratch3(this);

    GPRReg valueGPR = value.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    FPRReg scratch2FPR = scratch2.fpr();
    FPRReg scratch3FPR = scratch3.fpr();

    JumpList failureCases;

    failureCases.append(branchIfNotNumber(valueGPR));
    Jump notInt32 = branchIfNotInt32(valueGPR);
    signExtend32ToPtr(valueGPR, resultGPR);
    auto done = jump();

    notInt32.link(this);
    unboxDouble(valueGPR, resultGPR, scratch2FPR);
    branchConvertDoubleToInt52(scratch2FPR, resultGPR, failureCases, scratch1GPR, scratch3FPR);
    done.link(this);
    DFG_TYPE_CHECK(JSValueRegs(valueGPR), valueEdge, SpecInt32Only | SpecAnyIntAsDouble, failureCases);
}

void SpeculativeJIT::speculateAnyInt(Edge edge)
{
    if (!needsTypeCheck(edge, SpecInt32Only | SpecAnyIntAsDouble))
        return;
    
    GPRTemporary temp(this);
    convertAnyInt(edge, temp.gpr());
}

void SpeculativeJIT::speculateInt32(Edge edge, JSValueRegs regs)
{
    DFG_TYPE_CHECK(regs, edge, SpecInt32Only, branchIfNotInt32(regs));
}

void SpeculativeJIT::speculateDoubleRepAnyInt(Edge edge)
{
    if (!needsTypeCheck(edge, SpecAnyIntAsDouble))
        return;

    SpeculateDoubleOperand value(this, edge);
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    FPRTemporary scratch2(this);

    FPRReg valueFPR = value.fpr();
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    FPRReg scratch2FPR = scratch2.fpr();

    JumpList failureCases;
    branchConvertDoubleToInt52(valueFPR, resultGPR, failureCases, scratch1GPR, scratch2FPR);
    DFG_TYPE_CHECK(JSValueRegs(), edge, SpecAnyIntAsDouble, failureCases);
}

void SpeculativeJIT::compileArithRandom(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    GPRTemporary temp1(this);
    GPRTemporary temp2(this);
    GPRTemporary temp3(this);
    FPRTemporary result(this);
    emitRandomThunk(globalObject, temp1.gpr(), temp2.gpr(), temp3.gpr(), result.fpr());
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileStringCodePointAt(Node* node)
{
    // We emit CheckArray on this node as we do in StringCharCodeAt node so that we do not need to check SpecString here.
    // And CheckArray also ensures that this String is not a rope.
    SpeculateCellOperand string(this, node->child1());
    SpeculateStrictInt32Operand index(this, node->child2());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);

    GPRReg stringGPR = string.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();

    loadPtr(Address(stringGPR, JSString::offsetOfValue()), scratch1GPR);
    load32(Address(scratch1GPR, StringImpl::lengthMemoryOffset()), scratch2GPR);

    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(Uncountable, JSValueRegs(), nullptr, branch32(AboveOrEqual, indexGPR, scratch2GPR));

    // Load the character into scratch1GPR
    loadPtr(Address(scratch1GPR, StringImpl::dataOffset()), scratch4GPR);
    auto is16Bit = branchTest32(Zero, Address(scratch1GPR, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    JumpList done;

    load8(BaseIndex(scratch4GPR, indexGPR, TimesOne, 0), scratch1GPR);
    done.append(jump());

    is16Bit.link(this);
    load16(BaseIndex(scratch4GPR, indexGPR, TimesTwo, 0), scratch1GPR);
    // This is ok. indexGPR must be positive int32_t here and adding 1 never causes overflow if we treat indexGPR as uint32_t.
    add32(TrustedImm32(1), indexGPR, scratch3GPR);
    done.append(branch32(AboveOrEqual, scratch3GPR, scratch2GPR));
    and32(TrustedImm32(0xfffffc00), scratch1GPR, scratch2GPR);
    done.append(branch32(NotEqual, scratch2GPR, TrustedImm32(0xd800)));
    load16(BaseIndex(scratch4GPR, scratch3GPR, TimesTwo, 0), scratch3GPR);
    and32(TrustedImm32(0xfffffc00), scratch3GPR, scratch2GPR);
    done.append(branch32(NotEqual, scratch2GPR, TrustedImm32(0xdc00)));
    lshift32(TrustedImm32(10), scratch1GPR);
    getEffectiveAddress(BaseIndex(scratch1GPR, scratch3GPR, TimesOne, -U16_SURROGATE_OFFSET), scratch1GPR);
    done.link(this);

    strictInt32Result(scratch1GPR, m_currentNode);
}

void SpeculativeJIT::compileDateGet(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRReg baseGPR = base.gpr();
    speculateDateObject(node->child1(), baseGPR);

    auto emitGetCodeWithCallback = [&] (ptrdiff_t cachedDoubleOffset, ptrdiff_t cachedDataOffset, auto* operation, auto callback) {
        JSValueRegsTemporary result(this);
        FPRTemporary temp1(this);
        FPRTemporary temp2(this);

        JSValueRegs resultRegs = result.regs();
        FPRReg temp1FPR = temp1.fpr();
        FPRReg temp2FPR = temp2.fpr();

        JumpList slowCases;

        loadPtr(Address(baseGPR, DateInstance::offsetOfData()), resultRegs.payloadGPR());
        slowCases.append(branchTestPtr(Zero, resultRegs.payloadGPR()));
        loadDouble(Address(baseGPR, DateInstance::offsetOfInternalNumber()), temp1FPR);
        loadDouble(Address(resultRegs.payloadGPR(), cachedDoubleOffset), temp2FPR);
        slowCases.append(branchDouble(DoubleNotEqualOrUnordered, temp1FPR, temp2FPR));
        load32(Address(resultRegs.payloadGPR(), cachedDataOffset), resultRegs.payloadGPR());
        callback(resultRegs.payloadGPR());
        boxInt32(resultRegs.payloadGPR(), resultRegs);

        addSlowPathGenerator(slowPathCall(slowCases, this, operation, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, resultRegs, TrustedImmPtr(&vm()), baseGPR));

        jsValueResult(resultRegs, node);
    };

    auto emitGetCode = [&] (ptrdiff_t cachedDoubleOffset, ptrdiff_t cachedDataOffset, auto* operation) {
        emitGetCodeWithCallback(cachedDoubleOffset, cachedDataOffset, operation, [] (GPRReg) { });
    };

    switch (node->intrinsic()) {
    case DatePrototypeGetTimeIntrinsic: {
        FPRTemporary result(this);
        FPRReg resultFPR = result.fpr();
        loadDouble(Address(baseGPR, DateInstance::offsetOfInternalNumber()), resultFPR);
        doubleResult(resultFPR, node);
        break;
    }

    // We do not have any timezone offset which affects on milliseconds.
    // So Date#getMilliseconds and Date#getUTCMilliseconds have the same implementation.
    case DatePrototypeGetMillisecondsIntrinsic:
    case DatePrototypeGetUTCMillisecondsIntrinsic: {
        JSValueRegsTemporary result(this);
        FPRTemporary temp1(this);
        FPRTemporary temp2(this);
        FPRTemporary temp3(this);
        JSValueRegs resultRegs = result.regs();
        FPRReg temp1FPR = temp1.fpr();
        FPRReg temp2FPR = temp2.fpr();
        FPRReg temp3FPR = temp3.fpr();

        moveTrustedValue(jsNaN(), resultRegs);
        loadDouble(Address(baseGPR, DateInstance::offsetOfInternalNumber()), temp1FPR);
        auto isNaN = branchIfNaN(temp1FPR);

        static const double msPerSecondConstant = msPerSecond;
        loadDouble(TrustedImmPtr(&msPerSecondConstant), temp2FPR);
        divDouble(temp1FPR, temp2FPR, temp3FPR);
        floorDouble(temp3FPR, temp3FPR);
        mulDouble(temp3FPR, temp2FPR, temp3FPR);
        subDouble(temp1FPR, temp3FPR, temp1FPR);
        truncateDoubleToInt32(temp1FPR, resultRegs.payloadGPR());
        boxInt32(resultRegs.payloadGPR(), resultRegs);

        isNaN.link(this);
        jsValueResult(resultRegs, node);
        break;
    }

    case DatePrototypeGetFullYearIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfYear(), operationDateGetFullYear);
        break;
    case DatePrototypeGetUTCFullYearIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfYear(), operationDateGetUTCFullYear);
        break;
    case DatePrototypeGetMonthIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMonth(), operationDateGetMonth);
        break;
    case DatePrototypeGetUTCMonthIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMonth(), operationDateGetUTCMonth);
        break;
    case DatePrototypeGetDateIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMonthDay(), operationDateGetDate);
        break;
    case DatePrototypeGetUTCDateIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMonthDay(), operationDateGetUTCDate);
        break;
    case DatePrototypeGetDayIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfWeekDay(), operationDateGetDay);
        break;
    case DatePrototypeGetUTCDayIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfWeekDay(), operationDateGetUTCDay);
        break;
    case DatePrototypeGetHoursIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfHour(), operationDateGetHours);
        break;
    case DatePrototypeGetUTCHoursIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfHour(), operationDateGetUTCHours);
        break;
    case DatePrototypeGetMinutesIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMinute(), operationDateGetMinutes);
        break;
    case DatePrototypeGetUTCMinutesIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMinute(), operationDateGetUTCMinutes);
        break;
    case DatePrototypeGetSecondsIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfSecond(), operationDateGetSeconds);
        break;
    case DatePrototypeGetUTCSecondsIntrinsic:
        emitGetCode(DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfSecond(), operationDateGetUTCSeconds);
        break;

    case DatePrototypeGetTimezoneOffsetIntrinsic: {
        emitGetCodeWithCallback(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfUTCOffsetInMinute(), operationDateGetTimezoneOffset, [&] (GPRReg offsetGPR) {
            neg32(offsetGPR);
        });
        break;
    }

    case DatePrototypeGetYearIntrinsic: {
        emitGetCodeWithCallback(DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfYear(), operationDateGetYear, [&] (GPRReg yearGPR) {
            sub32(TrustedImm32(1900), yearGPR);
        });
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileDateSet(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    SpeculateDoubleOperand time(this, node->child2());

    FPRTemporary scratch1(this);
    FPRTemporary scratch2(this);
    FPRTemporary scratch3(this);
    FPRTemporary scratch4(this);

    GPRReg baseGPR = base.gpr();
    FPRReg timeFPR = time.fpr();
    FPRReg scratch1FPR = scratch1.fpr();
    FPRReg scratch2FPR = scratch2.fpr();
    FPRReg scratch3FPR = scratch3.fpr();
    FPRReg scratch4FPR = scratch4.fpr();

    speculateDateObject(node->child1(), baseGPR);

    roundTowardZeroDouble(timeFPR, scratch1FPR);
    moveZeroToDouble(scratch2FPR);
    addDouble(scratch2FPR, scratch1FPR);

    static const double NaN = PNaN;
    static const double max = WTF::maxECMAScriptTime;
    loadDouble(TrustedImmPtr(&max), scratch3FPR);
    loadDouble(TrustedImmPtr(&NaN), scratch4FPR);
    absDouble(timeFPR, scratch2FPR);
    moveDoubleConditionallyDouble(DoubleGreaterThanAndOrdered, scratch2FPR, scratch3FPR, scratch4FPR, scratch1FPR, scratch1FPR);

    storeDouble(scratch1FPR, Address(baseGPR, DateInstance::offsetOfInternalNumber()));
    doubleResult(scratch1FPR, node);
}

void SpeculativeJIT::compileGetByValWithThis(Node* node)
{
    JSValueOperand base(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand thisValue(this, node->child2(), ManualOperandSpeculation);
    JSValueOperand property(this, node->child3(), ManualOperandSpeculation);

    GPRReg baseGPR = base.gpr();
    GPRReg thisValueGPR = thisValue.gpr();
    GPRReg propertyGPR = property.gpr();

    GPRTemporary stubInfoTemp;
    GPRReg stubInfoGPR = InvalidGPRReg;
    if (m_graph.m_plan.isUnlinked()) {
        stubInfoTemp = GPRTemporary(this);
        stubInfoGPR = stubInfoTemp.gpr();
    }

    speculate(node, node->child1());
    speculate(node, node->child2());
    speculate(node, node->child3());

    JSValueRegsTemporary results(this);
    JSValueRegs resultRegs = results.regs();
    GPRReg resultGPR = resultRegs.gpr();

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
    RegisterSetBuilder usedRegisters = this->usedRegisters();

    JumpList slowCases;
    if (!m_state.forNode(node->child1()).isType(SpecCell))
        slowCases.append(branchIfNotCell(baseGPR));

    JSValueRegs baseRegs { baseGPR };
    JSValueRegs propertyRegs { propertyGPR };
    JSValueRegs thisValueRegs { thisValueGPR };
    auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
    JITGetByValWithThisGenerator gen(
        codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, AccessType::GetByValWithThis, usedRegisters,
        baseRegs, propertyRegs, thisValueRegs, resultRegs, stubInfoGPR);

    std::visit([&](auto* stubInfo) {
        if (m_state.forNode(node->child3()).isType(SpecString))
            stubInfo->propertyIsString = true;
        else if (m_state.forNode(node->child3()).isType(SpecInt32Only))
            stubInfo->propertyIsInt32 = true;
        else if (m_state.forNode(node->child3()).isType(SpecSymbol))
            stubInfo->propertyIsSymbol = true;
    }, stubInfo);

    std::unique_ptr<SlowPathGenerator> slowPath;
    if (m_graph.m_plan.isUnlinked()) {
        gen.generateDFGDataICFastPath(*this, stubInfoConstant.index(), stubInfoGPR);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoConstant.index();
        slowPath = slowPathICCall(
            slowCases, this, stubInfoConstant, stubInfoGPR, Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), operationGetByValWithThisOptimize,
            resultGPR, LinkableConstant::globalObject(*this, node), stubInfoGPR, nullptr, baseGPR, propertyGPR, thisValueGPR);
    } else {
        gen.generateFastPath(*this);
        slowCases.append(gen.slowPathJump());
        slowPath = slowPathCall(
            slowCases, this, operationGetByValWithThisOptimize,
            resultGPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(gen.stubInfo()), nullptr, baseGPR, propertyGPR, thisValueGPR);
    }

    addGetByValWithThis(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    jsValueResult(resultGPR, node);
}

void SpeculativeJIT::compileGetByIdMegamorphic(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);

    GPRReg baseGPR = base.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();

    UniquedStringImpl* uid = node->cacheableIdentifier().uid();
    JumpList slowCases = loadMegamorphicProperty(vm(), baseGPR, InvalidGPRReg, uid, scratch3GPR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR);
    addSlowPathGenerator(slowPathCall(slowCases, this, operationGetByIdMegamorphic, scratch3GPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(nullptr), baseGPR, node->cacheableIdentifier().rawBits()));
    jsValueResult(scratch3GPR, node);
}

void SpeculativeJIT::compileGetByIdWithThisMegamorphic(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand thisValue(this, node->child2());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs thisValueRegs = thisValue.jsValueRegs();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();

    UniquedStringImpl* uid = node->cacheableIdentifier().uid();
    JumpList slowCases = loadMegamorphicProperty(vm(), baseGPR, InvalidGPRReg, uid, scratch3GPR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR);
    addSlowPathGenerator(slowPathCall(slowCases, this, operationGetByIdWithThisMegamorphic, scratch3GPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(nullptr), baseGPR, thisValueRegs, node->cacheableIdentifier().rawBits()));
    jsValueResult(scratch3GPR, node);
}

void SpeculativeJIT::compileGetByValMegamorphic(Node* node)
{
    SpeculateCellOperand base(this, m_graph.child(node, 0));
    SpeculateCellOperand subscript(this, m_graph.child(node, 1));
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);
    GPRTemporary scratch5(this);

    GPRReg baseGPR = base.gpr();
    GPRReg subscriptGPR = subscript.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();
    GPRReg scratch5GPR = scratch5.gpr();

    speculateObject(m_graph.child(node, 0), baseGPR);
    speculateString(m_graph.child(node, 1), subscriptGPR);

    JumpList slowCases;

    loadPtr(Address(subscriptGPR, JSString::offsetOfValue()), scratch5GPR);
    slowCases.append(branchIfRopeStringImpl(scratch5GPR));
    slowCases.append(branchTest32(Zero, Address(scratch5GPR, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIsAtom())));

    slowCases.append(loadMegamorphicProperty(vm(), baseGPR, scratch5GPR, nullptr, scratch3GPR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR));
    addSlowPathGenerator(slowPathCall(slowCases, this, operationGetByValMegamorphic, scratch3GPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(nullptr), TrustedImmPtr(nullptr), baseGPR, subscriptGPR));
    jsValueResult(scratch3GPR, node);
}

void SpeculativeJIT::compileGetByValWithThisMegamorphic(Node* node)
{
    SpeculateCellOperand base(this, m_graph.child(node, 0));
    JSValueOperand thisValue(this, m_graph.child(node, 1));
    JSValueOperand subscript(this, m_graph.child(node, 2));
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);
    GPRTemporary scratch5(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs subscriptRegs = subscript.jsValueRegs();
    JSValueRegs thisValueRegs = thisValue.jsValueRegs();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();
    GPRReg scratch5GPR = scratch5.gpr();

    JumpList slowCases;

    slowCases.append(branchIfNotCell(subscriptRegs));
    slowCases.append(branchIfNotString(subscriptRegs.payloadGPR()));
    loadPtr(Address(subscriptRegs.payloadGPR(), JSString::offsetOfValue()), scratch5GPR);
    slowCases.append(branchIfRopeStringImpl(scratch5GPR));
    slowCases.append(branchTest32(Zero, Address(scratch5GPR, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIsAtom())));

    slowCases.append(loadMegamorphicProperty(vm(), baseGPR, scratch5GPR, nullptr, scratch3GPR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR));
    addSlowPathGenerator(slowPathCall(slowCases, this, operationGetByValWithThisMegamorphic, scratch3GPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(nullptr), TrustedImmPtr(nullptr), baseGPR, subscriptRegs, thisValueRegs));
    jsValueResult(scratch3GPR, node);
}

void SpeculativeJIT::compileFunctionBind(Node* node)
{
    SpeculateCellOperand target(this, m_graph.child(node, 0));
    JSValueOperand boundThis(this, m_graph.child(node, 1));
    JSValueOperand arg0(this, m_graph.child(node, 2));
    JSValueOperand arg1(this, m_graph.child(node, 3));
    JSValueOperand arg2(this, m_graph.child(node, 4));

    GPRReg targetGPR = target.gpr();
    JSValueRegs boundThisRegs = boundThis.jsValueRegs();
    JSValueRegs arg0Regs = arg0.jsValueRegs();
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    speculateObject(m_graph.child(node, 0), targetGPR);

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();
    callOperation(operationFunctionBind, resultGPR, LinkableConstant::globalObject(*this, node), targetGPR, boundThisRegs, arg0Regs, arg1Regs, arg2Regs);
    exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewBoundFunction(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    SpeculateCellOperand target(this, m_graph.child(node, 0));
    JSValueOperand boundThis(this, m_graph.child(node, 1));
    JSValueOperand arg0(this, m_graph.child(node, 2));
    JSValueOperand arg1(this, m_graph.child(node, 3));
    JSValueOperand arg2(this, m_graph.child(node, 4));
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);

    GPRReg targetGPR = target.gpr();
    JSValueRegs boundThisRegs = boundThis.jsValueRegs();
    JSValueRegs arg0Regs = arg0.jsValueRegs();
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    speculateObject(m_graph.child(node, 0), targetGPR);

    NativeExecutable* executable = node->castOperand<NativeExecutable*>();

    RegisteredStructure structure = m_graph.registerStructure(globalObject->boundFunctionStructure());

    JumpList slowPath;

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<JSBoundFunction>(resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR, slowPath, sizeof(JSBoundFunction), SlowAllocationResult::UndefinedBehavior);
    storeLinkableConstant(LinkableConstant::globalObject(*this, node), Address(resultGPR, JSBoundFunction::offsetOfScopeChain()));
    storeLinkableConstant(LinkableConstant(*this, executable), Address(resultGPR, JSBoundFunction::offsetOfExecutableOrRareData()));
    storeValue(JSValueRegs { targetGPR }, Address(resultGPR, JSBoundFunction::offsetOfTargetFunction()));
    storeValue(boundThisRegs, Address(resultGPR, JSBoundFunction::offsetOfBoundThis()));
    storeValue(arg0Regs, Address(resultGPR, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 0));
    storeValue(arg1Regs, Address(resultGPR, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 1));
    storeValue(arg2Regs, Address(resultGPR, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 2));
    storePtr(TrustedImmPtr(nullptr), Address(resultGPR, JSBoundFunction::offsetOfNameMayBeNull()));
    store64(TrustedImm64(bitwise_cast<uint64_t>(PNaN)), Address(resultGPR, JSBoundFunction::offsetOfLength()));
    store32(TrustedImm32(node->numberOfBoundArguments()), Address(resultGPR, JSBoundFunction::offsetOfBoundArgsLength()));
    store8(TrustedImm32(static_cast<uint8_t>(TriState::Indeterminate)), Address(resultGPR, JSBoundFunction::offsetOfCanConstruct()));
    mutatorFence(vm());

    addSlowPathGenerator(slowPathCall(slowPath, this, operationNewBoundFunction, resultGPR, LinkableConstant::globalObject(*this, node), targetGPR, boundThisRegs, arg0Regs, arg1Regs, arg2Regs));
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileEnumeratorPutByVal(Node* node)
{
    Edge baseEdge = m_graph.varArgChild(node, 0);
    auto generate = [&] (JSValueRegs baseRegs) {
        JumpList doneCases;
        JumpList recoverGenericCase;

        JSValueOperand property(this, m_graph.varArgChild(node, 1));
        JSValueOperand value(this, m_graph.varArgChild(node, 2));
        SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 4));
        SpeculateStrictInt32Operand mode(this, m_graph.varArgChild(node, 5));
        SpeculateCellOperand enumerator(this, m_graph.varArgChild(node, 6));
        GPRTemporary scratch(this);
        GPRTemporary storageTemporary(this);

        JSValueRegs valueRegs = value.regs();
        JSValueRegs propertyRegs = property.regs();
        GPRReg modeGPR = mode.gpr();
        GPRReg indexGPR = index.gpr();
        GPRReg enumeratorGPR = enumerator.gpr();
        GPRReg scratchGPR = scratch.gpr();
        GPRReg storageGPR = storageTemporary.gpr();

        ECMAMode ecmaMode = node->ecmaMode();

        JumpList notFastNamedCases;
        // FIXME: Maybe we should have a better way to represent IndexedMode+OwnStructureMode?
        bool indexedAndOwnStructureMode = m_graph.varArgChild(node, 1).node() == m_graph.varArgChild(node, 4).node();
        JumpList& genericOrRecoverCase = indexedAndOwnStructureMode ? recoverGenericCase : notFastNamedCases;

        // FIXME: We shouldn't generate this code if we know base is not an object.
        notFastNamedCases.append(branchTest32(NonZero, modeGPR, TrustedImm32(JSPropertyNameEnumerator::IndexedMode | JSPropertyNameEnumerator::GenericMode)));
        {
            if (!m_state.forNode(baseEdge).isType(SpecCell))
                genericOrRecoverCase.append(branchIfNotCell(baseRegs));

            // Check the structure
            // FIXME: If we know there's only one structure for base we can just embed it here.
            load32(Address(baseRegs.payloadGPR(), JSCell::structureIDOffset()), scratchGPR);

            genericOrRecoverCase.append(branch32(NotEqual, scratchGPR, Address(enumeratorGPR, JSPropertyNameEnumerator::cachedStructureIDOffset())));
            emitNonNullDecodeZeroExtendedStructureID(scratchGPR, scratchGPR);
            genericOrRecoverCase.append(branchTest32(NonZero, Address(scratchGPR, Structure::bitFieldOffset()), TrustedImm32(Structure::s_isWatchingReplacementBits)));

            // Compute the offset
            // If index is less than the enumerator's cached inline storage, then it's an inline access
            Jump outOfLineAccess = branch32(AboveOrEqual,
                indexGPR, Address(enumeratorGPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));

            storeValue(valueRegs, BaseIndex(baseRegs.payloadGPR(), indexGPR, TimesEight, JSObject::offsetOfInlineStorage()));

            doneCases.append(jump());

            // Otherwise it's out of line
            outOfLineAccess.link(this);
            move(indexGPR, scratchGPR);
            sub32(Address(enumeratorGPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), scratchGPR);
            neg32(scratchGPR);
            signExtend32ToPtr(scratchGPR, scratchGPR);
            loadPtr(Address(baseRegs.payloadGPR(), JSObject::butterflyOffset()), storageGPR);
            constexpr intptr_t offsetOfFirstProperty = offsetInButterfly(firstOutOfLineOffset) * static_cast<intptr_t>(sizeof(EncodedJSValue));
            storeValue(valueRegs, BaseIndex(storageGPR, scratchGPR, TimesEight, offsetOfFirstProperty));
            doneCases.append(jump());
        }

        notFastNamedCases.link(this);
        {
            CodeOrigin codeOrigin = node->origin.semantic;
            CallSiteIndex callSite = recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream.size());
            RegisterSetBuilder usedRegisters = this->usedRegisters();

            auto [ stubInfo, stubInfoConstant ] = addStructureStubInfo();
            JITPutByValGenerator gen(
                codeBlock(), stubInfo, JITType::DFGJIT, codeOrigin, callSite, ecmaMode.isStrict() ? AccessType::PutByValStrict : AccessType::PutByValSloppy, usedRegisters,
                baseRegs, propertyRegs, valueRegs, InvalidGPRReg, scratchGPR, ecmaMode);

            std::visit([&](auto* stubInfo) {
                if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecString))
                    stubInfo->propertyIsString = true;
                else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecInt32Only))
                    stubInfo->propertyIsInt32 = true;
                else if (m_state.forNode(m_graph.varArgChild(node, 1)).isType(SpecSymbol))
                    stubInfo->propertyIsSymbol = true;
                stubInfo->isEnumerator = true;
            }, stubInfo);

            JumpList slowCases;

            std::unique_ptr<SlowPathGenerator> slowPath;
            auto operation = ecmaMode.isStrict() ? operationPutByValStrictOptimize : operationPutByValSloppyOptimize;
            if (m_graph.m_plan.isUnlinked()) {
                gen.generateDFGDataICFastPath(*this, stubInfoConstant.index(), scratchGPR);
                gen.m_unlinkedStubInfoConstantIndex = stubInfoConstant.index();
                ASSERT(!gen.stubInfo());
                slowPath = slowPathICCall(
                    slowCases, this, stubInfoConstant, scratchGPR, Address(scratchGPR, StructureStubInfo::offsetOfSlowOperation()), operation,
                    NoResult, LinkableConstant::globalObject(*this, node), baseRegs, propertyRegs, valueRegs, scratchGPR, nullptr);
            } else {
                gen.generateFastPath(*this);
                slowCases.append(gen.slowPathJump());
                slowPath = slowPathCall(
                    slowCases, this, operation,
                    NoResult, LinkableConstant::globalObject(*this, node), baseRegs, propertyRegs, valueRegs, TrustedImmPtr(gen.stubInfo()), nullptr);
            }

            addPutByVal(gen, slowPath.get());
            addSlowPathGenerator(WTFMove(slowPath));
        }

        if (!recoverGenericCase.empty()) {
            if (baseRegs.tagGPR() == InvalidGPRReg)
                addSlowPathGenerator(slowPathCall(recoverGenericCase, this, operationEnumeratorRecoverNameAndPutByVal, NoResult, LinkableConstant::globalObject(*this, node), CellValue(baseRegs.payloadGPR()), valueRegs, TrustedImm32(ecmaMode.isStrict()), indexGPR, enumeratorGPR));
            else
                addSlowPathGenerator(slowPathCall(recoverGenericCase, this, operationEnumeratorRecoverNameAndPutByVal, NoResult, LinkableConstant::globalObject(*this, node), baseRegs, valueRegs, TrustedImm32(ecmaMode.isStrict()), indexGPR, enumeratorGPR));
        }

        doneCases.link(this);
    };

    if (isCell(baseEdge.useKind())) {
        // Use manual operand speculation since Fixup may have picked a UseKind more restrictive than CellUse.
        SpeculateCellOperand base(this, baseEdge, ManualOperandSpeculation);
        speculate(node, baseEdge);
        generate(JSValueRegs::payloadOnly(base.gpr()));
    } else {
        JSValueOperand base(this, baseEdge);
        generate(base.regs());
    }
    noResult(node);
}

void SpeculativeJIT::compilePutByIdMegamorphic(Node* node)
{
    SpeculateCellOperand base(this, m_graph.child(node, 0));
    JSValueOperand value(this, m_graph.child(node, 1));
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();

    UniquedStringImpl* uid = node->cacheableIdentifier().uid();
    JumpList slowCases = storeMegamorphicProperty(vm(), baseGPR, InvalidGPRReg, uid, valueRegs.payloadGPR(), scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR);
    addSlowPathGenerator(slowPathCall(slowCases, this, node->ecmaMode().isStrict() ? operationPutByIdMegamorphicStrict : operationPutByIdMegamorphicSloppy, NoResult, LinkableConstant::globalObject(*this, node), TrustedImmPtr(nullptr), valueRegs, baseGPR, node->cacheableIdentifier().rawBits()));
    noResult(node);
}

void SpeculativeJIT::compilePutByValMegamorphic(Node* node)
{
    SpeculateCellOperand base(this, m_graph.child(node, 0));
    SpeculateCellOperand subscript(this, m_graph.child(node, 1));
    JSValueOperand value(this, m_graph.child(node, 2));
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
    GPRTemporary scratch4(this);
    GPRTemporary scratch5(this);

    GPRReg baseGPR = base.gpr();
    GPRReg subscriptGPR = subscript.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();
    GPRReg scratch4GPR = scratch4.gpr();
    GPRReg scratch5GPR = scratch5.gpr();

    speculateString(m_graph.child(node, 1), subscriptGPR);

    JumpList slowCases;

    loadPtr(Address(subscriptGPR, JSString::offsetOfValue()), scratch5GPR);
    slowCases.append(branchIfRopeStringImpl(scratch5GPR));
    slowCases.append(branchTest32(Zero, Address(scratch5GPR, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIsAtom())));

    slowCases.append(storeMegamorphicProperty(vm(), baseGPR, scratch5GPR, nullptr, valueRegs.payloadGPR(), scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR));
    addSlowPathGenerator(slowPathCall(slowCases, this, node->ecmaMode().isStrict() ? operationPutByValMegamorphicStrict : operationPutByValMegamorphicSloppy, NoResult, LinkableConstant::globalObject(*this, node), baseGPR, subscriptGPR, valueRegs, TrustedImmPtr(nullptr), TrustedImmPtr(nullptr)));
    noResult(node);
}

void SpeculativeJIT::compileCreateClonedArguments(Node* node)
{
    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

        GPRTemporary storage(this);
        GPRTemporary result(this);
        GPRTemporary size(this);
        GPRTemporary scratch(this);
        GPRTemporary scratch2(this);

        GPRReg storageGPR = storage.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg sizeGPR = size.gpr();
        GPRReg scratchGPR = scratch.gpr();
        GPRReg scratch2GPR = scratch2.gpr();

        JumpList slowCases;

        emitGetLength(node->origin.semantic, sizeGPR);

        move(TrustedImmPtr(nullptr), storageGPR);
        slowCases.append(branch32(AboveOrEqual, sizeGPR, TrustedImm32(MAX_STORAGE_VECTOR_LENGTH)));

        {
            // ClonedArguments' butterfly requires property storage for "length".
            size_t outOfLineCapacity = globalObject->clonedArgumentsStructure()->outOfLineCapacity();
            static_assert((1U << 3) == sizeof(JSValue));
            lshift32(sizeGPR, TrustedImm32(3), scratchGPR);
            add32(TrustedImm32(sizeof(IndexingHeader) + outOfLineCapacity * sizeof(JSValue)), scratchGPR, scratch2GPR);
            emitAllocateVariableSized(storageGPR, vm().jsValueGigacageAuxiliarySpace(), scratch2GPR, scratchGPR, resultGPR, slowCases);
            addPtr(TrustedImm32(sizeof(IndexingHeader) + outOfLineCapacity * sizeof(JSValue)), storageGPR);
            ASSERT(Butterfly::offsetOfPublicLength() + static_cast<ptrdiff_t>(sizeof(uint32_t)) == Butterfly::offsetOfVectorLength());
            storePair32(sizeGPR, sizeGPR, storageGPR, TrustedImm32(Butterfly::offsetOfPublicLength()));
            emitInitializeOutOfLineStorage(storageGPR, outOfLineCapacity, scratchGPR);
        }

        emitAllocateJSObject<ClonedArguments>(resultGPR, TrustedImmPtr(m_graph.registerStructure(globalObject->clonedArgumentsStructure())), storageGPR, scratchGPR, scratch2GPR, slowCases, SlowAllocationResult::UndefinedBehavior);

        emitGetCallee(node->origin.semantic, scratchGPR);
        storePtr(scratchGPR, Address(resultGPR, ClonedArguments::offsetOfCallee()));
        boxInt32(sizeGPR, JSValueRegs { scratchGPR });
        storeValue(JSValueRegs { scratchGPR }, Address(storageGPR, offsetRelativeToBase(clonedArgumentsLengthPropertyOffset)));

        emitGetArgumentStart(node->origin.semantic, scratchGPR);
        Jump done = branchTest32(Zero, sizeGPR);
        Label loop = label();
        sub32(TrustedImm32(1), sizeGPR);
        loadValue(BaseIndex(scratchGPR, sizeGPR, TimesEight), JSValueRegs { scratch2GPR });
        storeValue(JSValueRegs { scratch2GPR }, BaseIndex(storageGPR, sizeGPR, TimesEight));
        branchTest32(NonZero, sizeGPR).linkTo(loop, this);
        done.link(this);

        mutatorFence(vm());

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, resultGPR);
        auto doneFromSlowPath = label();
        addSlowPathGeneratorLambda([=, this, savePlans = WTFMove(savePlans), slowCases = WTFMove(slowCases)] () {
            slowCases.link(this);
            silentSpill(savePlans);

            setupArgument(5, [&] (GPRReg destGPR) { move(storageGPR, destGPR); });
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

            appendCallSetResult(operationCreateClonedArguments, resultGPR);
            silentFill(savePlans);
            exceptionCheck();

            jump().linkTo(doneFromSlowPath, this);
        });

        cellResult(resultGPR, node);
        return;
    }

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();

    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    // We set up the arguments ourselves, because we have the whole register file and we can
    // set them up directly into the argument registers.

    // Arguments: 0:JSGlobalObject*, 1:structure, 2:start, 3:length, 4:callee, 5:butterfly
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

    appendCallSetResult(operationCreateClonedArguments, resultGPR);
    exceptionCheck();

    cellResult(resultGPR, node);
}

#endif

} } // namespace JSC::DFG

#endif
