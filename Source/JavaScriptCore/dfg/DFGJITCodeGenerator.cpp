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

#include "DFGJITCompilerInlineMethods.h"
#include "DFGSpeculativeJIT.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

const double JITCodeGenerator::twoToThe32 = (double)0x100000000ull;

void JITCodeGenerator::clearGenerationInfo()
{
    for (unsigned i = 0; i < m_generationInfo.size(); ++i)
        m_generationInfo[i] = GenerationInfo();
    m_gprs = RegisterBank<GPRInfo>();
    m_fprs = RegisterBank<FPRInfo>();
}

GPRReg JITCodeGenerator::fillStorage(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    switch (info.registerFormat()) {
    case DataFormatNone: {
        GPRReg gpr = allocate();
        ASSERT(info.spillFormat() == DataFormatStorage);
        m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
        m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
        info.fillStorage(gpr);
        return gpr;
    }
        
    case DataFormatStorage: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
    
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

bool JITCodeGenerator::isStrictInt32(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex))
        return true;
    
    Node& node = m_jit.graph()[nodeIndex];
    GenerationInfo& info = m_generationInfo[node.virtualRegister()];
    
    return info.registerFormat() == DataFormatInteger;
}

bool JITCodeGenerator::isKnownInteger(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex))
        return true;

    Node& node = m_jit.graph()[nodeIndex];
    
    if (node.hasInt32Result())
        return true;
    
    GenerationInfo& info = m_generationInfo[node.virtualRegister()];

    return info.isJSInteger();
}

bool JITCodeGenerator::isKnownNumeric(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex) || isNumberConstant(nodeIndex))
        return true;

    Node& node = m_jit.graph()[nodeIndex];
    
    if (node.hasNumberResult())
        return true;
    
    GenerationInfo& info = m_generationInfo[node.virtualRegister()];

    return info.isJSInteger() || info.isJSDouble();
}

bool JITCodeGenerator::isKnownCell(NodeIndex nodeIndex)
{
    return m_generationInfo[m_jit.graph()[nodeIndex].virtualRegister()].isJSCell();
}

bool JITCodeGenerator::isKnownNotCell(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    if (node.hasConstant() && !valueOfJSConstant(nodeIndex).isCell())
        return true;
    return !(info.isJSCell() || info.isUnknownJS());
}

bool JITCodeGenerator::isKnownNotInteger(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    return info.isJSDouble() || info.isJSCell() || info.isJSBoolean()
        || (node.hasConstant() && !valueOfJSConstant(nodeIndex).isInt32());
}

bool JITCodeGenerator::isKnownNotNumber(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    return (!info.isJSDouble() && !info.isJSInteger() && !info.isUnknownJS())
        || (node.hasConstant() && !isNumberConstant(nodeIndex));
}

bool JITCodeGenerator::isKnownBoolean(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    if (node.hasBooleanResult())
        return true;
    
    if (isBooleanConstant(nodeIndex))
        return true;
    
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    return info.isJSBoolean();
}

void JITCodeGenerator::writeBarrier(MacroAssembler& jit, GPRReg owner, GPRReg scratch1, GPRReg scratch2, WriteBarrierUseKind useKind)
{
    UNUSED_PARAM(jit);
    UNUSED_PARAM(owner);
    UNUSED_PARAM(scratch1);
    UNUSED_PARAM(scratch2);
    UNUSED_PARAM(useKind);
    ASSERT(owner != scratch1);
    ASSERT(owner != scratch2);
    ASSERT(scratch1 != scratch2);

#if ENABLE(WRITE_BARRIER_PROFILING)
    JITCompiler::emitCount(jit, WriteBarrierCounters::jitCounterFor(useKind));
#endif
    markCellCard(jit, owner, scratch1, scratch2);
}

void JITCodeGenerator::markCellCard(MacroAssembler& jit, GPRReg owner, GPRReg scratch1, GPRReg scratch2)
{
    UNUSED_PARAM(jit);
    UNUSED_PARAM(owner);
    UNUSED_PARAM(scratch1);
    UNUSED_PARAM(scratch2);
    
#if ENABLE(GGC)
    jit.move(owner, scratch1);
    jit.andPtr(TrustedImm32(static_cast<int32_t>(MarkedBlock::blockMask)), scratch1);
    jit.move(owner, scratch2);
    // consume additional 8 bits as we're using an approximate filter
    jit.rshift32(TrustedImm32(MarkedBlock::atomShift + 8), scratch2);
    jit.andPtr(TrustedImm32(MarkedBlock::atomMask >> 8), scratch2);
    MacroAssembler::Jump filter = jit.branchTest8(MacroAssembler::Zero, MacroAssembler::BaseIndex(scratch1, scratch2, MacroAssembler::TimesOne, MarkedBlock::offsetOfMarks()));
    jit.move(owner, scratch2);
    jit.rshift32(TrustedImm32(MarkedBlock::cardShift), scratch2);
    jit.andPtr(TrustedImm32(MarkedBlock::cardMask), scratch2);
    jit.store8(TrustedImm32(1), MacroAssembler::BaseIndex(scratch1, scratch2, MacroAssembler::TimesOne, MarkedBlock::offsetOfCards()));
    filter.link(&jit);
#endif
}

void JITCodeGenerator::writeBarrier(GPRReg ownerGPR, GPRReg valueGPR, NodeIndex valueIndex, WriteBarrierUseKind useKind, GPRReg scratch1, GPRReg scratch2)
{
    UNUSED_PARAM(ownerGPR);
    UNUSED_PARAM(valueGPR);
    UNUSED_PARAM(scratch1);
    UNUSED_PARAM(scratch2);
    UNUSED_PARAM(useKind);

    if (isKnownNotCell(valueIndex))
        return;

#if ENABLE(WRITE_BARRIER_PROFILING)
    JITCompiler::emitCount(m_jit, WriteBarrierCounters::jitCounterFor(useKind));
#endif

#if ENABLE(GGC)
    GPRTemporary temp1;
    GPRTemporary temp2;
    if (scratch1 == InvalidGPRReg) {
        GPRTemporary scratchGPR(this);
        temp1.adopt(scratchGPR);
        scratch1 = temp1.gpr();
    }
    if (scratch2 == InvalidGPRReg) {
        GPRTemporary scratchGPR(this);
        temp2.adopt(scratchGPR);
        scratch2 = temp2.gpr();
    }
    
    JITCompiler::Jump rhsNotCell;
    bool hadCellCheck = false;
    if (!isKnownCell(valueIndex) && !isCellPrediction(m_jit.getPrediction(valueIndex))) {
        hadCellCheck = true;
        rhsNotCell = m_jit.branchIfNotCell(valueGPR);
    }

    markCellCard(m_jit, ownerGPR, scratch1, scratch2);

    if (hadCellCheck)
        rhsNotCell.link(&m_jit);
#endif
}

void JITCodeGenerator::writeBarrier(GPRReg ownerGPR, JSCell* value, WriteBarrierUseKind useKind, GPRReg scratch1, GPRReg scratch2)
{
    UNUSED_PARAM(ownerGPR);
    UNUSED_PARAM(value);
    UNUSED_PARAM(scratch1);
    UNUSED_PARAM(scratch2);
    UNUSED_PARAM(useKind);
    
    if (Heap::isMarked(value))
        return;

#if ENABLE(WRITE_BARRIER_PROFILING)
    JITCompiler::emitCount(m_jit, WriteBarrierCounters::jitCounterFor(useKind));
#endif

#if ENABLE(GGC)
    GPRTemporary temp1;
    GPRTemporary temp2;
    if (scratch1 == InvalidGPRReg) {
        GPRTemporary scratchGPR(this);
        temp1.adopt(scratchGPR);
        scratch1 = temp1.gpr();
    }
    if (scratch2 == InvalidGPRReg) {
        GPRTemporary scratchGPR(this);
        temp2.adopt(scratchGPR);
        scratch2 = temp2.gpr();
    }

    markCellCard(m_jit, ownerGPR, scratch1, scratch2);
#endif
}

void JITCodeGenerator::writeBarrier(JSCell* owner, GPRReg valueGPR, NodeIndex valueIndex, WriteBarrierUseKind useKind, GPRReg scratch)
{
    UNUSED_PARAM(owner);
    UNUSED_PARAM(valueGPR);
    UNUSED_PARAM(scratch);
    UNUSED_PARAM(useKind);

    if (isKnownNotCell(valueIndex))
        return;

#if ENABLE(WRITE_BARRIER_PROFILING)
    JITCompiler::emitCount(m_jit, WriteBarrierCounters::jitCounterFor(useKind));
#endif

#if ENABLE(GGC)
    JITCompiler::Jump rhsNotCell;
    bool hadCellCheck = false;
    if (!isKnownCell(valueIndex) && !isCellPrediction(m_jit.getPrediction(valueIndex))) {
        hadCellCheck = true;
        rhsNotCell = m_jit.branchIfNotCell(valueGPR);
    }
    
    GPRTemporary temp;
    if (scratch == InvalidGPRReg) {
        GPRTemporary scratchGPR(this);
        temp.adopt(scratchGPR);
        scratch = temp.gpr();
    }

    uint8_t* cardAddress = Heap::addressOfCardFor(owner);
    m_jit.move(JITCompiler::TrustedImmPtr(cardAddress), scratch);
    m_jit.store8(JITCompiler::TrustedImm32(1), JITCompiler::Address(scratch));

    if (hadCellCheck)
        rhsNotCell.link(&m_jit);
#endif
}

bool JITCodeGenerator::nonSpeculativeCompare(Node& node, MacroAssembler::RelationalCondition cond, S_DFGOperation_EJJ helperFunction)
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
        else if (info.registerFormat() != DataFormatNone
#if USE(JSVALUE32_64)
            && !(info.registerFormat() & DataFormatJS)
#endif
            ) {
            ASSERT(info.gpr() != InvalidGPRReg);
            fprintf(stderr, ":%s\n", GPRInfo::debugName(info.gpr()));
        } else
            fprintf(stderr, "\n");
    }
    if (label)
        fprintf(stderr, "</%s>\n", label);
}
#endif


#if DFG_ENABLE(CONSISTENCY_CHECK)
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
        case DataFormatJS:
        case DataFormatJSInteger:
        case DataFormatJSDouble:
        case DataFormatJSCell:
        case DataFormatJSBoolean:
#if USE(JSVALUE32_64)
            break;
#endif
        case DataFormatInteger:
        case DataFormatCell:
        case DataFormatBoolean:
        case DataFormatStorage: {
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
#if USE(JSVALUE64)
        if (iter.regID() != info.gpr()) {
            fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for gpr %s (virtual register %d).\n", iter.debugName(), virtualRegister);
            failed = true;
        }
#else
        if (!(info.registerFormat() & DataFormatJS)) {
            if (iter.regID() != info.gpr()) {
                fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for gpr %s (virtual register %d).\n", iter.debugName(), virtualRegister);
                failed = true;
            }
        } else {
            if (iter.regID() != info.tagGPR() && iter.regID() != info.payloadGPR()) {
                fprintf(stderr, "DFG_CONSISTENCY_CHECK failed: name mismatch for gpr %s (virtual register %d).\n", iter.debugName(), virtualRegister);
                failed = true;
            }
        }
#endif
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

GPRTemporary::GPRTemporary()
    : m_jit(0)
    , m_gpr(InvalidGPRReg)
{
}

GPRTemporary::GPRTemporary(JITCodeGenerator* jit)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
#if CPU(X86)
    // we currenty lazily allocate the reg, as the number of regs on X86 is limited.
#else
    m_gpr = m_jit->allocate();
#endif
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

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, SpeculateStrictInt32Operand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
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

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, SpeculateBooleanOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

#if USE(JSVALUE64)
GPRTemporary::GPRTemporary(JITCodeGenerator* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}
#else
GPRTemporary::GPRTemporary(JITCodeGenerator* jit, JSValueOperand& op1, bool tag)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (!op1.isDouble() && m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(tag ? op1.tagGPR() : op1.payloadGPR());
    else
        m_gpr = m_jit->allocate();
}
#endif

GPRTemporary::GPRTemporary(JITCodeGenerator* jit, StorageOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

void GPRTemporary::adopt(GPRTemporary& other)
{
    ASSERT(!m_jit);
    ASSERT(m_gpr == InvalidGPRReg);
    ASSERT(other.m_jit);
    ASSERT(other.m_gpr != InvalidGPRReg);
    m_jit = other.m_jit;
    m_gpr = other.m_gpr;
    other.m_jit = 0;
    other.m_gpr = InvalidGPRReg;
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

#if USE(JSVALUE32_64)
FPRTemporary::FPRTemporary(JITCodeGenerator* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (op1.isDouble() && m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}
#endif

} } // namespace JSC::DFG

#endif
