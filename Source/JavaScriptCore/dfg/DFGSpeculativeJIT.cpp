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

#if USE(JSVALUE32_64)
#include "DFGJITCompilerInlineMethods.h"
#endif

namespace JSC { namespace DFG {

#ifndef NDEBUG
void ValueSource::dump(FILE* out) const
{
    switch (kind()) {
    case SourceNotSet:
        fprintf(out, "NotSet");
        break;
    case ValueInRegisterFile:
        fprintf(out, "InRegFile");
        break;
    case Int32InRegisterFile:
        fprintf(out, "Int32");
        break;
    case CellInRegisterFile:
        fprintf(out, "Cell");
        break;
    case BooleanInRegisterFile:
        fprintf(out, "Bool");
        break;
    case HaveNode:
        fprintf(out, "Node(%d)", m_nodeIndex);
        break;
    }
}
#endif

OSRExit::OSRExit(JSValueSource jsValueSource, ValueProfile* valueProfile, MacroAssembler::Jump check, SpeculativeJIT* jit, unsigned recoveryIndex)
    : m_jsValueSource(jsValueSource)
    , m_valueProfile(valueProfile)
    , m_check(check)
    , m_nodeIndex(jit->m_compileIndex)
    , m_codeOrigin(jit->m_codeOriginForOSR)
    , m_recoveryIndex(recoveryIndex)
    , m_arguments(jit->m_arguments.size())
    , m_variables(jit->m_variables.size())
    , m_lastSetOperand(jit->m_lastSetOperand)
{
    ASSERT(m_codeOrigin.isSet());
    for (unsigned argument = 0; argument < m_arguments.size(); ++argument)
        m_arguments[argument] = jit->computeValueRecoveryFor(jit->m_arguments[argument]);
    for (unsigned variable = 0; variable < m_variables.size(); ++variable)
        m_variables[variable] = jit->computeValueRecoveryFor(jit->m_variables[variable]);
}

#ifndef NDEBUG
void OSRExit::dump(FILE* out) const
{
    for (unsigned argument = 0; argument < m_arguments.size(); ++argument)
        m_arguments[argument].dump(out);
    fprintf(out, " : ");
    for (unsigned variable = 0; variable < m_variables.size(); ++variable)
        m_variables[variable].dump(out);
}
#endif

void SpeculativeJIT::compilePeepHoleDoubleBranch(Node& node, NodeIndex branchNodeIndex, JITCompiler::DoubleCondition condition)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();
    
    SpeculateDoubleOperand op1(this, node.child1());
    SpeculateDoubleOperand op2(this, node.child2());
    
    addBranch(m_jit.branchDouble(condition, op1.fpr(), op2.fpr()), taken);
    
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

void SpeculativeJIT::compilePeepHoleObjectEquality(Node& node, NodeIndex branchNodeIndex, void* vptr, PredictionChecker predictionCheck)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();

    MacroAssembler::RelationalCondition condition = MacroAssembler::Equal;
    
    if (taken == (m_block + 1)) {
        condition = MacroAssembler::NotEqual;
        BlockIndex tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    SpeculateCellOperand op1(this, node.child1());
    SpeculateCellOperand op2(this, node.child2());
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    if (!predictionCheck(m_state.forNode(node.child1()).m_type))
        speculationCheck(JSValueSource::unboxedCell(op1GPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op1GPR), MacroAssembler::TrustedImmPtr(vptr)));
    if (!predictionCheck(m_state.forNode(node.child2()).m_type))
        speculationCheck(JSValueSource::unboxedCell(op2GPR), node.child2(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(op2GPR), MacroAssembler::TrustedImmPtr(vptr)));
    
    addBranch(m_jit.branchPtr(condition, op1GPR, op2GPR), taken);
    if (notTaken != (m_block + 1))
        addBranch(m_jit.jump(), notTaken);
}

void SpeculativeJIT::compilePeepHoleIntegerBranch(Node& node, NodeIndex branchNodeIndex, JITCompiler::RelationalCondition condition)
{
    Node& branchNode = at(branchNodeIndex);
    BlockIndex taken = branchNode.takenBlockIndex();
    BlockIndex notTaken = branchNode.notTakenBlockIndex();

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

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compilePeepHoleBranch(Node& node, MacroAssembler::RelationalCondition condition, MacroAssembler::DoubleCondition doubleCondition, S_DFGOperation_EJJ operation)
{
    // Fused compare & branch.
    NodeIndex branchNodeIndex = detectPeepHoleBranch();
    if (branchNodeIndex != NoNode) {
        // detectPeepHoleBranch currently only permits the branch to be the very next node,
        // so can be no intervening nodes to also reference the compare. 
        ASSERT(node.adjustedRefCount() == 1);

        if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2()))) {
            compilePeepHoleIntegerBranch(node, branchNodeIndex, condition);
            use(node.child1());
            use(node.child2());
        } else if (Node::shouldSpeculateNumber(at(node.child1()), at(node.child2()))) {
            compilePeepHoleDoubleBranch(node, branchNodeIndex, doubleCondition);
            use(node.child1());
            use(node.child2());
        } else if (node.op == CompareEq && Node::shouldSpeculateFinalObject(at(node.child1()), at(node.child2()))) {
            compilePeepHoleObjectEquality(node, branchNodeIndex, m_jit.globalData()->jsFinalObjectVPtr, isFinalObjectPrediction);
            use(node.child1());
            use(node.child2());
        } else if (node.op == CompareEq && Node::shouldSpeculateArray(at(node.child1()), at(node.child2()))) {
            compilePeepHoleObjectEquality(node, branchNodeIndex, m_jit.globalData()->jsArrayVPtr, isArrayPrediction);
            use(node.child1());
            use(node.child2());
        } else
            nonSpeculativePeepholeBranch(node, branchNodeIndex, condition, operation);

        m_compileIndex = branchNodeIndex;
        return true;
    }
    return false;
}

void SpeculativeJIT::compileMovHint(Node& node)
{
    ASSERT(node.op == SetLocal);
    
    setNodeIndexForOperand(node.child1(), node.local());
    m_lastSetOperand = node.local();
}

void SpeculativeJIT::compile(BasicBlock& block)
{
    ASSERT(m_compileOkay);
    ASSERT(m_compileIndex == block.begin);
    
    if (!block.isReachable) {
        m_compileIndex = block.end;
        return;
    }

    m_blockHeads[m_block] = m_jit.label();
#if DFG_ENABLE(JIT_BREAK_ON_EVERY_BLOCK)
    m_jit.breakpoint();
#endif

    ASSERT(m_arguments.size() == block.variablesAtHead.numberOfArguments());
    for (size_t i = 0; i < m_arguments.size(); ++i) {
        NodeIndex nodeIndex = block.variablesAtHead.argument(i);
        if (nodeIndex == NoNode)
            m_arguments[i] = ValueSource(ValueInRegisterFile);
        else
            m_arguments[i] = ValueSource::forPrediction(at(nodeIndex).variableAccessData()->prediction());
    }
    
    m_state.reset();
    m_state.beginBasicBlock(&block);
    
    ASSERT(m_variables.size() == block.variablesAtHead.numberOfLocals());
    for (size_t i = 0; i < m_variables.size(); ++i) {
        NodeIndex nodeIndex = block.variablesAtHead.local(i);
        if (nodeIndex == NoNode)
            m_variables[i] = ValueSource(ValueInRegisterFile);
        else
            m_variables[i] = ValueSource::forPrediction(at(nodeIndex).variableAccessData()->prediction());
    }
    
    m_lastSetOperand = std::numeric_limits<int>::max();
    m_codeOriginForOSR = CodeOrigin();

    for (; m_compileIndex < block.end; ++m_compileIndex) {
        Node& node = at(m_compileIndex);
        m_codeOriginForOSR = node.codeOrigin;
        if (!node.shouldGenerate()) {
#if DFG_ENABLE(DEBUG_VERBOSE)
            fprintf(stderr, "SpeculativeJIT skipping Node @%d (bc#%u) at JIT offset 0x%x     ", (int)m_compileIndex, node.codeOrigin.bytecodeIndex, m_jit.debugOffset());
#endif
            switch (node.op) {
            case SetLocal:
                compileMovHint(node);
                break;

            case InlineStart: {
                InlineCallFrame* inlineCallFrame = node.codeOrigin.inlineCallFrame;
                unsigned argumentsStart = inlineCallFrame->stackOffset - RegisterFile::CallFrameHeaderSize - inlineCallFrame->arguments.size();
                for (unsigned i = 0; i < inlineCallFrame->arguments.size(); ++i) {
                    ValueRecovery recovery = computeValueRecoveryFor(m_variables[argumentsStart + i]);
                    // The recovery cannot point to registers, since the call frame reification isn't
                    // as smart as OSR, so it can't handle that. The exception is the this argument,
                    // which we don't really need to be able to recover.
                    ASSERT(!i || !recovery.isInRegisters());
                    inlineCallFrame->arguments[i] = recovery;
                }
                break;
            }
                
            default:
                break;
            }
        } else {
            
#if DFG_ENABLE(DEBUG_VERBOSE)
            fprintf(stderr, "SpeculativeJIT generating Node @%d (bc#%u) at JIT offset 0x%x   ", (int)m_compileIndex, node.codeOrigin.bytecodeIndex, m_jit.debugOffset());
#endif
#if DFG_ENABLE(JIT_BREAK_ON_EVERY_NODE)
            m_jit.breakpoint();
#endif
            checkConsistency();
            compile(node);
            if (!m_compileOkay) {
                m_compileOkay = true;
                m_compileIndex = block.end;
                clearGenerationInfo();
                return;
            }
            
#if DFG_ENABLE(DEBUG_VERBOSE)
            if (node.hasResult()) {
                GenerationInfo& info = m_generationInfo[node.virtualRegister()];
                fprintf(stderr, "-> %s, vr#%d", dataFormatToString(info.registerFormat()), (int)node.virtualRegister());
                if (info.registerFormat() != DataFormatNone) {
                    if (info.registerFormat() == DataFormatDouble)
                        fprintf(stderr, ", %s", FPRInfo::debugName(info.fpr()));
#if USE(JSVALUE32_64)
                    else if (info.registerFormat() & DataFormatJS)
                        fprintf(stderr, ", %s %s", GPRInfo::debugName(info.tagGPR()), GPRInfo::debugName(info.payloadGPR()));
#endif
                    else
                        fprintf(stderr, ", %s", GPRInfo::debugName(info.gpr()));
                }
                fprintf(stderr, "    ");
            } else
                fprintf(stderr, "    ");
#endif
        }
        
#if DFG_ENABLE(VERBOSE_VALUE_RECOVERIES)
        for (int operand = -m_arguments.size() - RegisterFile::CallFrameHeaderSize; operand < -RegisterFile::CallFrameHeaderSize; ++operand)
            computeValueRecoveryFor(operand).dump(stderr);
        
        fprintf(stderr, " : ");
        
        for (int operand = 0; operand < (int)m_variables.size(); ++operand)
            computeValueRecoveryFor(operand).dump(stderr);
#endif

#if DFG_ENABLE(DEBUG_VERBOSE)
        fprintf(stderr, "\n");
#endif
        
        // Make sure that the abstract state is rematerialized for the next node.
        m_state.execute(m_compileIndex);
        
        if (node.shouldGenerate())
            checkConsistency();
    }
}

// If we are making type predictions about our arguments then
// we need to check that they are correct on function entry.
void SpeculativeJIT::checkArgumentTypes()
{
    ASSERT(!m_compileIndex);
    m_codeOriginForOSR = CodeOrigin(0);

    for (size_t i = 0; i < m_arguments.size(); ++i)
        m_arguments[i] = ValueSource(ValueInRegisterFile);
    for (size_t i = 0; i < m_variables.size(); ++i)
        m_variables[i] = ValueSource(ValueInRegisterFile);
    
    for (int i = 0; i < m_jit.codeBlock()->m_numParameters; ++i) {
        VirtualRegister virtualRegister = (VirtualRegister)(m_jit.codeBlock()->thisRegister() + i);
        PredictedType predictedType = at(m_jit.graph().m_arguments[i]).variableAccessData()->prediction();
#if USE(JSVALUE64)
        if (isInt32Prediction(predictedType))
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::Below, JITCompiler::addressFor(virtualRegister), GPRInfo::tagTypeNumberRegister));
        else if (isArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        } else if (isByteArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
        } else if (isBooleanPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            m_jit.xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
        }
#else
        if (isInt32Prediction(predictedType))
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)));
        else if (isArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        } else if (isByteArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
        } else if (isBooleanPrediction(predictedType))
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)));
#endif
    }
}

bool SpeculativeJIT::compile()
{
    checkArgumentTypes();

    ASSERT(!m_compileIndex);
    for (m_block = 0; m_block < m_jit.graph().m_blocks.size(); ++m_block)
        compile(*m_jit.graph().m_blocks[m_block]);
    linkBranches();
    return true;
}

void SpeculativeJIT::linkOSREntries(LinkBuffer& linkBuffer)
{
    for (BlockIndex blockIndex = 0; blockIndex < m_jit.graph().m_blocks.size(); ++blockIndex) {
        BasicBlock& block = *m_jit.graph().m_blocks[blockIndex];
        if (block.isOSRTarget)
            m_jit.noticeOSREntry(block, m_blockHeads[blockIndex], linkBuffer);
    }
}

ValueRecovery SpeculativeJIT::computeValueRecoveryFor(const ValueSource& valueSource)
{
    switch (valueSource.kind()) {
    case ValueInRegisterFile:
        return ValueRecovery::alreadyInRegisterFile();
        
    case Int32InRegisterFile:
        return ValueRecovery::alreadyInRegisterFileAsUnboxedInt32();

    case CellInRegisterFile:
        return ValueRecovery::alreadyInRegisterFileAsUnboxedCell();

    case BooleanInRegisterFile:
        return ValueRecovery::alreadyInRegisterFileAsUnboxedBoolean();

    case HaveNode: {
        if (m_jit.isConstant(valueSource.nodeIndex()))
            return ValueRecovery::constant(m_jit.valueOfJSConstant(valueSource.nodeIndex()));
    
        Node* nodePtr = &at(valueSource.nodeIndex());
        if (!nodePtr->shouldGenerate()) {
            // It's legitimately dead. As in, nobody will ever use this node, or operand,
            // ever. Set it to Undefined to make the GC happy after the OSR.
            return ValueRecovery::constant(jsUndefined());
        }
    
        GenerationInfo* infoPtr = &m_generationInfo[nodePtr->virtualRegister()];
        if (!infoPtr->alive() || infoPtr->nodeIndex() != valueSource.nodeIndex()) {
            // Try to see if there is an alternate node that would contain the value we want.
            // There are four possibilities:
            //
            // ValueToNumber: If the only live version of the value is a ValueToNumber node
            //    then it means that all remaining uses of the value would have performed a
            //    ValueToNumber conversion anyway. Thus, we can substitute ValueToNumber.
            //
            // ValueToInt32: Likewise, if the only remaining live version of the value is
            //    ValueToInt32, then we can use it. But if there is both a ValueToInt32
            //    and a ValueToNumber, then we better go with ValueToNumber because it
            //    means that some remaining uses would have converted to number while
            //    others would have converted to Int32.
            //
            // UInt32ToNumber: If the only live version of the value is a UInt32ToNumber
            //    then the only remaining uses are ones that want a properly formed number
            //    rather than a UInt32 intermediate.
            //
            // The reverse of the above: This node could be a UInt32ToNumber, but its
            //    alternative is still alive. This means that the only remaining uses of
            //    the number would be fine with a UInt32 intermediate.
        
            bool found = false;
        
            if (nodePtr->op == UInt32ToNumber) {
                NodeIndex nodeIndex = nodePtr->child1();
                nodePtr = &at(nodeIndex);
                infoPtr = &m_generationInfo[nodePtr->virtualRegister()];
                if (infoPtr->alive() && infoPtr->nodeIndex() == nodeIndex)
                    found = true;
            }
        
            if (!found) {
                NodeIndex valueToNumberIndex = NoNode;
                NodeIndex valueToInt32Index = NoNode;
                NodeIndex uint32ToNumberIndex = NoNode;
            
                for (unsigned virtualRegister = 0; virtualRegister < m_generationInfo.size(); ++virtualRegister) {
                    GenerationInfo& info = m_generationInfo[virtualRegister];
                    if (!info.alive())
                        continue;
                    if (info.nodeIndex() == NoNode)
                        continue;
                    Node& node = at(info.nodeIndex());
                    if (node.child1Unchecked() != valueSource.nodeIndex())
                        continue;
                    switch (node.op) {
                    case ValueToNumber:
                    case ValueToDouble:
                        valueToNumberIndex = info.nodeIndex();
                        break;
                    case ValueToInt32:
                        valueToInt32Index = info.nodeIndex();
                        break;
                    case UInt32ToNumber:
                        uint32ToNumberIndex = info.nodeIndex();
                        break;
                    default:
                        break;
                    }
                }
            
                NodeIndex nodeIndexToUse;
                if (valueToNumberIndex != NoNode)
                    nodeIndexToUse = valueToNumberIndex;
                else if (valueToInt32Index != NoNode)
                    nodeIndexToUse = valueToInt32Index;
                else if (uint32ToNumberIndex != NoNode)
                    nodeIndexToUse = uint32ToNumberIndex;
                else
                    nodeIndexToUse = NoNode;
            
                if (nodeIndexToUse != NoNode) {
                    nodePtr = &at(nodeIndexToUse);
                    infoPtr = &m_generationInfo[nodePtr->virtualRegister()];
                    ASSERT(infoPtr->alive() && infoPtr->nodeIndex() == nodeIndexToUse);
                    found = true;
                }
            }
        
            if (!found)
                return ValueRecovery::constant(jsUndefined());
        }
    
        ASSERT(infoPtr->alive());

        if (infoPtr->registerFormat() != DataFormatNone) {
            if (infoPtr->registerFormat() == DataFormatDouble)
                return ValueRecovery::inFPR(infoPtr->fpr());
#if USE(JSVALUE32_64)
            if (infoPtr->registerFormat() & DataFormatJS)
                return ValueRecovery::inPair(infoPtr->tagGPR(), infoPtr->payloadGPR());
#endif
            return ValueRecovery::inGPR(infoPtr->gpr(), infoPtr->registerFormat());
        }
        if (infoPtr->spillFormat() != DataFormatNone)
            return ValueRecovery::displacedInRegisterFile(static_cast<VirtualRegister>(nodePtr->virtualRegister()), infoPtr->spillFormat());
    
        ASSERT_NOT_REACHED();
        return ValueRecovery();
    }
        
    default:
        ASSERT_NOT_REACHED();
        return ValueRecovery();
    }
}

void SpeculativeJIT::compileGetCharCodeAt(Node& node)
{
    ASSERT(node.child3() == NoNode);
    SpeculateCellOperand string(this, node.child1());
    SpeculateStrictInt32Operand index(this, node.child2());

    GPRReg stringReg = string.gpr();
    GPRReg indexReg = index.gpr();

    if (!isStringPrediction(m_state.forNode(node.child1()).m_type))
        speculationCheck(JSValueSource::unboxedCell(stringReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(stringReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));

    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, indexReg, MacroAssembler::Address(stringReg, JSString::offsetOfLength())));

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();

    m_jit.loadPtr(MacroAssembler::Address(stringReg, JSString::offsetOfValue()), scratchReg);

    // Speculate that we're not accessing a rope
    speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(MacroAssembler::Zero, scratchReg));

    // Load the character into scratchReg
    JITCompiler::Jump is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.loadPtr(MacroAssembler::Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    m_jit.load8(MacroAssembler::BaseIndex(scratchReg, indexReg, MacroAssembler::TimesOne, 0), scratchReg);
    JITCompiler::Jump cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);

    m_jit.loadPtr(MacroAssembler::Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    m_jit.load16(MacroAssembler::BaseIndex(scratchReg, indexReg, MacroAssembler::TimesTwo, 0), scratchReg);

    cont8Bit.link(&m_jit);

    integerResult(scratchReg, m_compileIndex);
}

void SpeculativeJIT::compileGetByValOnString(Node& node)
{
    ASSERT(node.child3() == NoNode);
    SpeculateCellOperand base(this, node.child1());
    SpeculateStrictInt32Operand property(this, node.child2());

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();

    if (!isStringPrediction(m_state.forNode(node.child1()).m_type))
        speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));

    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSString::offsetOfLength())));

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();

    m_jit.loadPtr(MacroAssembler::Address(baseReg, JSString::offsetOfValue()), scratchReg);

    // Speculate that we're not accessing a rope
    speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(MacroAssembler::Zero, scratchReg));

    // Load the character into scratchReg
    JITCompiler::Jump is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.loadPtr(MacroAssembler::Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    m_jit.load8(MacroAssembler::BaseIndex(scratchReg, propertyReg, MacroAssembler::TimesOne, 0), scratchReg);
    JITCompiler::Jump cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);

    m_jit.loadPtr(MacroAssembler::Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    m_jit.load16(MacroAssembler::BaseIndex(scratchReg, propertyReg, MacroAssembler::TimesTwo, 0), scratchReg);

    // We only support ascii characters
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, scratchReg, TrustedImm32(0x100)));

    // 8 bit string values don't need the isASCII check.
    cont8Bit.link(&m_jit);

    GPRTemporary smallStrings(this);
    GPRReg smallStringsReg = smallStrings.gpr();
    m_jit.move(MacroAssembler::TrustedImmPtr(m_jit.globalData()->smallStrings.singleCharacterStrings()), smallStringsReg);
    m_jit.loadPtr(MacroAssembler::BaseIndex(smallStringsReg, scratchReg, MacroAssembler::ScalePtr, 0), scratchReg);
    speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(MacroAssembler::Zero, scratchReg));
    cellResult(scratchReg, m_compileIndex);
}

void SpeculativeJIT::compileValueToInt32(Node& node)
{
    if (at(node.child1()).shouldNotSpeculateInteger()) {
        if (at(node.child1()).shouldSpeculateDouble()) {
            SpeculateDoubleOperand op1(this, node.child1());
            GPRTemporary result(this);
            FPRReg fpr = op1.fpr();
            GPRReg gpr = result.gpr();
            JITCompiler::Jump truncatedToInteger = m_jit.branchTruncateDoubleToInt32(fpr, gpr, JITCompiler::BranchIfTruncateSuccessful);
            
            silentSpillAllRegisters(gpr);
            callOperation(toInt32, gpr, fpr);
            silentFillAllRegisters(gpr);
            
            truncatedToInteger.link(&m_jit);
            integerResult(gpr, m_compileIndex);
            return;
        }
        // Do it the safe way.
        nonSpeculativeValueToInt32(node);
        return;
    }
    
    SpeculateIntegerOperand op1(this, node.child1());
    GPRTemporary result(this, op1);
    m_jit.move(op1.gpr(), result.gpr());
    integerResult(result.gpr(), m_compileIndex, op1.format());
}

static void compileClampDoubleToByte(JITCompiler& jit, GPRReg result, FPRReg source, FPRReg scratch)
{
    // Unordered compare so we pick up NaN
    static const double zero = 0;
    static const double byteMax = 255;
    static const double half = 0.5;
    jit.loadDouble(&zero, scratch);
    MacroAssembler::Jump tooSmall = jit.branchDouble(MacroAssembler::DoubleLessThanOrEqualOrUnordered, source, scratch);
    jit.loadDouble(&byteMax, scratch);
    MacroAssembler::Jump tooBig = jit.branchDouble(MacroAssembler::DoubleGreaterThan, source, scratch);
    
    jit.loadDouble(&half, scratch);
    jit.addDouble(source, scratch);
    jit.truncateDoubleToInt32(scratch, result);   
    MacroAssembler::Jump truncatedInt = jit.jump();
    
    tooSmall.link(&jit);
    jit.xorPtr(result, result);
    MacroAssembler::Jump zeroed = jit.jump();
    
    tooBig.link(&jit);
    jit.move(JITCompiler::TrustedImm32(255), result);
    
    truncatedInt.link(&jit);
    zeroed.link(&jit);

}

void SpeculativeJIT::compilePutByValForByteArray(GPRReg base, GPRReg property, Node& node)
{
    NodeIndex baseIndex = node.child1();
    NodeIndex valueIndex = node.child3();
    
    if (!isByteArrayPrediction(m_state.forNode(baseIndex).m_type))
        speculationCheck(JSValueSource::unboxedCell(base), baseIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(base), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
    GPRTemporary value;
    GPRReg valueGPR;

    if (at(valueIndex).isConstant()) {
        JSValue jsValue = valueOfJSConstant(valueIndex);
        if (!jsValue.isNumber()) {
            terminateSpeculativeExecution(JSValueRegs(), NoNode);
            noResult(m_compileIndex);
            return;
        }
        double d = jsValue.asNumber();
        d += 0.5;
        if (!(d > 0))
            d = 0;
        else if (d > 255)
            d = 255;
        GPRTemporary scratch(this);
        GPRReg scratchReg = scratch.gpr();
        m_jit.move(Imm32((int)d), scratchReg);
        value.adopt(scratch);
        valueGPR = scratchReg;
    } else if (!at(valueIndex).shouldNotSpeculateInteger()) {
        SpeculateIntegerOperand valueOp(this, valueIndex);
        GPRTemporary scratch(this);
        GPRReg scratchReg = scratch.gpr();
        m_jit.move(valueOp.gpr(), scratchReg);
        MacroAssembler::Jump inBounds = m_jit.branch32(MacroAssembler::BelowOrEqual, scratchReg, TrustedImm32(0xff));
        MacroAssembler::Jump tooBig = m_jit.branch32(MacroAssembler::GreaterThan, scratchReg, TrustedImm32(0xff));
        m_jit.xorPtr(scratchReg, scratchReg);
        MacroAssembler::Jump clamped = m_jit.jump();
        m_jit.move(TrustedImm32(255), scratchReg);
        clamped.link(&m_jit);
        inBounds.link(&m_jit);
        value.adopt(scratch);
        valueGPR = scratchReg;
    } else {
        SpeculateDoubleOperand valueOp(this, valueIndex);
        GPRTemporary result(this);
        FPRTemporary floatScratch(this);
        FPRReg fpr = valueOp.fpr();
        GPRReg gpr = result.gpr();
        compileClampDoubleToByte(m_jit, gpr, fpr, floatScratch.fpr());
        value.adopt(result);
        valueGPR = gpr;
    }
    ASSERT(valueGPR != property);
    ASSERT(valueGPR != base);
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    ASSERT(valueGPR != storageReg);
    m_jit.loadPtr(MacroAssembler::Address(base, JSByteArray::offsetOfStorage()), storageReg);
    MacroAssembler::Jump outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, property, MacroAssembler::Address(storageReg, ByteArray::offsetOfSize()));
    m_jit.store8(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesOne, ByteArray::offsetOfData()));
    outOfBounds.link(&m_jit);
    noResult(m_compileIndex);
}

void SpeculativeJIT::compileGetByValOnByteArray(Node& node)
{
    ASSERT(node.child3() == NoNode);
    SpeculateCellOperand base(this, node.child1());
    SpeculateStrictInt32Operand property(this, node.child2());
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    
    if (!isByteArrayPrediction(m_state.forNode(node.child1()).m_type))
        speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));

    // Load the character into scratchReg
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    m_jit.loadPtr(MacroAssembler::Address(baseReg, JSByteArray::offsetOfStorage()), storageReg);
    
    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, ByteArray::offsetOfSize())));

    m_jit.load8(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne, ByteArray::offsetOfData()), storageReg);
    integerResult(storageReg, m_compileIndex);
}

} } // namespace JSC::DFG

#endif
