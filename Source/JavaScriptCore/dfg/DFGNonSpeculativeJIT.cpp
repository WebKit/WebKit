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

void NonSpeculativeJIT::compile(SpeculationCheckIndexIterator& checkIterator, Node& node)
{
#if ENABLE(DFG_OSR_EXIT)
    UNUSED_PARAM(checkIterator);
#else
    // Check for speculation checks from the corresponding instruction in the
    // speculative path. Do not check for NodeIndex 0, since this is checked
    // in the outermost compile layer, at the head of the non-speculative path
    // (for index 0 we may need to check regardless of whether or not the node
    // will be generated, since argument type speculation checks will appear
    // as speculation checks at this index).
    if (m_compileIndex && checkIterator.hasCheckAtIndex(m_compileIndex))
        trackEntry(m_jit.label());
#endif

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
        nonSpeculativeUInt32ToNumber(node);
        break;
    }

    case ValueToInt32: {
        nonSpeculativeValueToInt32(node);
        break;
    }

    case ValueToNumber:
    case ValueToDouble: {
        nonSpeculativeValueToNumber(node);
        break;
    }

    case ValueAdd:
    case ArithAdd: {
        nonSpeculativeAdd(op, node);
        break;
    }
        
    case ArithSub: {
        nonSpeculativeArithSub(node);
        break;
    }

    case ArithMul: {
        nonSpeculativeBasicArithOp(ArithMul, node);
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
        nonSpeculativeArithMod(node);
        break;
    }

    case LogicalNot: {
        nonSpeculativeLogicalNot(node);
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
        
        writeBarrier(m_jit, baseGPR, storageGPR, WriteBarrierForPropertyAccess);
        
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

        writeBarrier(m_jit, globalObjectReg, scratchReg, WriteBarrierForVariableAccess);

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

#if ENABLE(DFG_SUCCESS_STATS)
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
        nonSpeculativeCheckHasInstance(node);
        break;
    }

    case InstanceOf: {
        nonSpeculativeInstanceOf(node);
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

#if ENABLE(DFG_JIT_BREAK_ON_EVERY_BLOCK)
    m_jit.breakpoint();
#endif

    for (; m_compileIndex < block.end; ++m_compileIndex) {
        Node& node = m_jit.graph()[m_compileIndex];
        if (!node.shouldGenerate())
            continue;

#if ENABLE(DFG_DEBUG_VERBOSE)
        fprintf(stderr, "NonSpeculativeJIT generating Node @%d at code offset 0x%x   ", (int)m_compileIndex, m_jit.debugOffset());
#endif
#if ENABLE(DFG_JIT_BREAK_ON_EVERY_NODE)
        m_jit.breakpoint();
#endif

        checkConsistency();
        compile(checkIterator, node);
#if ENABLE(DFG_DEBUG_VERBOSE)
        if (node.hasResult())
            fprintf(stderr, "-> %s\n", dataFormatToString(m_generationInfo[node.virtualRegister()].registerFormat()));
        else
            fprintf(stderr, "\n");
#endif
        checkConsistency();
    }
}

void NonSpeculativeJIT::compile(SpeculationCheckIndexIterator& checkIterator)
{
    // Check for speculation checks added at function entry (checking argument types).
#if !ENABLE(DFG_OSR_EXIT)
    if (checkIterator.hasCheckAtIndex(m_compileIndex))
        trackEntry(m_jit.label());
#endif

    ASSERT(!m_compileIndex);
    for (m_block = 0; m_block < m_jit.graph().m_blocks.size(); ++m_block)
        compile(checkIterator, *m_jit.graph().m_blocks[m_block]);
    linkBranches();
}

} } // namespace JSC::DFG

#endif
