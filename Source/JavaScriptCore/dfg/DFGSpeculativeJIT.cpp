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

#include "JSByteArray.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

// On Windows we need to wrap fmod; on other platforms we can call it directly.
// On ARMv7 we assert that all function pointers have to low bit set (point to thumb code).
#if CALLING_CONVENTION_IS_STDCALL || CPU(ARM_THUMB2)
static double DFG_OPERATION fmodAsDFGOperation(double x, double y)
{
    return fmod(x, y);
}
#else
#define fmodAsDFGOperation fmod
#endif

const double SpeculativeJIT::twoToThe32 = (double)0x100000000ull;

void SpeculativeJIT::clearGenerationInfo()
{
    for (unsigned i = 0; i < m_generationInfo.size(); ++i)
        m_generationInfo[i] = GenerationInfo();
    m_gprs = RegisterBank<GPRInfo>();
    m_fprs = RegisterBank<FPRInfo>();
}

GPRReg SpeculativeJIT::fillStorage(NodeIndex nodeIndex)
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

void SpeculativeJIT::useChildren(Node& node)
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

bool SpeculativeJIT::isStrictInt32(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex))
        return true;
    
    Node& node = m_jit.graph()[nodeIndex];
    GenerationInfo& info = m_generationInfo[node.virtualRegister()];
    
    return info.registerFormat() == DataFormatInteger;
}

bool SpeculativeJIT::isKnownInteger(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex))
        return true;

    Node& node = m_jit.graph()[nodeIndex];
    
    if (node.hasInt32Result())
        return true;
    
    GenerationInfo& info = m_generationInfo[node.virtualRegister()];

    return info.isJSInteger();
}

bool SpeculativeJIT::isKnownNumeric(NodeIndex nodeIndex)
{
    if (isInt32Constant(nodeIndex) || isNumberConstant(nodeIndex))
        return true;

    Node& node = m_jit.graph()[nodeIndex];
    
    if (node.hasNumberResult())
        return true;
    
    GenerationInfo& info = m_generationInfo[node.virtualRegister()];

    return info.isJSInteger() || info.isJSDouble();
}

bool SpeculativeJIT::isKnownCell(NodeIndex nodeIndex)
{
    return m_generationInfo[m_jit.graph()[nodeIndex].virtualRegister()].isJSCell();
}

bool SpeculativeJIT::isKnownNotCell(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    if (node.hasConstant() && !valueOfJSConstant(nodeIndex).isCell())
        return true;
    return !(info.isJSCell() || info.isUnknownJS());
}

bool SpeculativeJIT::isKnownNotInteger(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    return info.isJSDouble() || info.isJSCell() || info.isJSBoolean()
        || (node.hasConstant() && !valueOfJSConstant(nodeIndex).isInt32());
}

bool SpeculativeJIT::isKnownNotNumber(NodeIndex nodeIndex)
{
    Node& node = m_jit.graph()[nodeIndex];
    VirtualRegister virtualRegister = node.virtualRegister();
    GenerationInfo& info = m_generationInfo[virtualRegister];
    
    return (!info.isJSDouble() && !info.isJSInteger() && !info.isUnknownJS())
        || (node.hasConstant() && !isNumberConstant(nodeIndex));
}

bool SpeculativeJIT::isKnownBoolean(NodeIndex nodeIndex)
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

void SpeculativeJIT::writeBarrier(MacroAssembler& jit, GPRReg owner, GPRReg scratch1, GPRReg scratch2, WriteBarrierUseKind useKind)
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

void SpeculativeJIT::markCellCard(MacroAssembler& jit, GPRReg owner, GPRReg scratch1, GPRReg scratch2)
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

void SpeculativeJIT::writeBarrier(GPRReg ownerGPR, GPRReg valueGPR, NodeIndex valueIndex, WriteBarrierUseKind useKind, GPRReg scratch1, GPRReg scratch2)
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

void SpeculativeJIT::writeBarrier(GPRReg ownerGPR, JSCell* value, WriteBarrierUseKind useKind, GPRReg scratch1, GPRReg scratch2)
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

void SpeculativeJIT::writeBarrier(JSCell* owner, GPRReg valueGPR, NodeIndex valueIndex, WriteBarrierUseKind useKind, GPRReg scratch)
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

bool SpeculativeJIT::nonSpeculativeCompare(Node& node, MacroAssembler::RelationalCondition cond, S_DFGOperation_EJJ helperFunction)
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

bool SpeculativeJIT::nonSpeculativeStrictEq(Node& node, bool invert)
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

void SpeculativeJIT::dump(const char* label)
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
void SpeculativeJIT::checkConsistency()
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

GPRTemporary::GPRTemporary(SpeculativeJIT* jit)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
#if CPU(X86)
    // we currenty lazily allocate the reg, as the number of regs on X86 is limited.
#else
    m_gpr = m_jit->allocate();
#endif
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, GPRReg specific)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate(specific);
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, SpeculateIntegerOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, SpeculateIntegerOperand& op1, SpeculateIntegerOperand& op2)
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

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, SpeculateStrictInt32Operand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, IntegerOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, IntegerOperand& op1, IntegerOperand& op2)
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

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, SpeculateCellOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, SpeculateBooleanOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}

#if USE(JSVALUE64)
GPRTemporary::GPRTemporary(SpeculativeJIT* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(op1.gpr());
    else
        m_gpr = m_jit->allocate();
}
#else
GPRTemporary::GPRTemporary(SpeculativeJIT* jit, JSValueOperand& op1, bool tag)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (!op1.isDouble() && m_jit->canReuse(op1.index()))
        m_gpr = m_jit->reuse(tag ? op1.tagGPR() : op1.payloadGPR());
    else
        m_gpr = m_jit->allocate();
}
#endif

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, StorageOperand& op1)
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

FPRTemporary::FPRTemporary(SpeculativeJIT* jit)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, DoubleOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, DoubleOperand& op1, DoubleOperand& op2)
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

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, SpeculateDoubleOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, SpeculateDoubleOperand& op1, SpeculateDoubleOperand& op2)
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
FPRTemporary::FPRTemporary(SpeculativeJIT* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (op1.isDouble() && m_jit->canReuse(op1.index()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}
#endif

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
#if DFG_ENABLE(XOR_DEBUG_AID)
            m_jit.xorPtr(JITCompiler::TrustedImm32(m_compileIndex), GPRInfo::regT0);
            m_jit.xorPtr(JITCompiler::TrustedImm32(m_compileIndex), GPRInfo::regT0);
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
        } else if (isInt8ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->int8ArrayDescriptor().m_vptr)));
        } else if (isInt16ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->int16ArrayDescriptor().m_vptr)));
        } else if (isInt32ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->int32ArrayDescriptor().m_vptr)));
        } else if (isUint8ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->uint8ArrayDescriptor().m_vptr)));
        } else if (isUint16ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->uint16ArrayDescriptor().m_vptr)));
        } else if (isUint32ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->uint32ArrayDescriptor().m_vptr)));
        } else if (isFloat32ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->float32ArrayDescriptor().m_vptr)));
        } else if (isFloat64ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchTestPtr(MacroAssembler::NonZero, temp.gpr(), GPRInfo::tagMaskRegister));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->float64ArrayDescriptor().m_vptr)));
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
        else if (isInt8ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->int8ArrayDescriptor().m_vptr)));
        } else if (isInt16ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->int16ArrayDescriptor().m_vptr)));
        } else if (isInt32ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->int32ArrayDescriptor().m_vptr)));
        } else if (isUint8ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->uint8ArrayDescriptor().m_vptr)));
        } else if (isUint16ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->uint16ArrayDescriptor().m_vptr)));
        }  else if (isUint32ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->uint32ArrayDescriptor().m_vptr)));
        }  else if (isFloat32ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->float32ArrayDescriptor().m_vptr)));
        }   else if (isFloat64ArrayPrediction(predictedType)) {
            GPRTemporary temp(this);
            m_jit.load32(JITCompiler::tagFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::NotEqual, temp.gpr(), TrustedImm32(JSValue::CellTag)));
            m_jit.load32(JITCompiler::payloadFor(virtualRegister), temp.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(temp.gpr()), MacroAssembler::TrustedImmPtr(m_jit.globalData()->float64ArrayDescriptor().m_vptr)));
        } 
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
    StorageOperand storage(this, node.child3());

    GPRReg stringReg = string.gpr();
    GPRReg indexReg = index.gpr();
    GPRReg storageReg = storage.gpr();
    
    if (!isStringPrediction(m_state.forNode(node.child1()).m_type)) {
        ASSERT(!(at(node.child1()).prediction() & PredictString));
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        noResult(m_compileIndex);
        return;
    }

    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, indexReg, MacroAssembler::Address(stringReg, JSString::offsetOfLength())));

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();

    m_jit.loadPtr(MacroAssembler::Address(stringReg, JSString::offsetOfValue()), scratchReg);

    // Load the character into scratchReg
    JITCompiler::Jump is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.load8(MacroAssembler::BaseIndex(storageReg, indexReg, MacroAssembler::TimesOne, 0), scratchReg);
    JITCompiler::Jump cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);

    m_jit.load16(MacroAssembler::BaseIndex(storageReg, indexReg, MacroAssembler::TimesTwo, 0), scratchReg);

    cont8Bit.link(&m_jit);

    integerResult(scratchReg, m_compileIndex);
}

void SpeculativeJIT::compileGetByValOnString(Node& node)
{
    SpeculateCellOperand base(this, node.child1());
    SpeculateStrictInt32Operand property(this, node.child2());
    StorageOperand storage(this, node.child3());
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();

    if (!isStringPrediction(m_state.forNode(node.child1()).m_type)) {
        ASSERT(!(at(node.child1()).prediction() & PredictString));
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        noResult(m_compileIndex);
        return;
    }

    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(baseReg, JSString::offsetOfLength())));

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();

    m_jit.loadPtr(MacroAssembler::Address(baseReg, JSString::offsetOfValue()), scratchReg);

    // Load the character into scratchReg
    JITCompiler::Jump is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.load8(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne, 0), scratchReg);
    JITCompiler::Jump cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);

    m_jit.load16(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesTwo, 0), scratchReg);

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
    // FIXME: This should probably just use a floating point round!
    // https://bugs.webkit.org/show_bug.cgi?id=72054
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
        tooBig.link(&m_jit);
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
    ASSERT_UNUSED(valueGPR, valueGPR != property);
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
    SpeculateCellOperand base(this, node.child1());
    SpeculateStrictInt32Operand property(this, node.child2());

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();

    if (!isByteArrayPrediction(m_state.forNode(node.child1()).m_type)) {
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        noResult(m_compileIndex);
        return;
    }

    // Load the character into scratchReg
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    m_jit.loadPtr(MacroAssembler::Address(baseReg, JSByteArray::offsetOfStorage()), storageReg);
    
    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, ByteArray::offsetOfSize())));

    m_jit.load8(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne, ByteArray::offsetOfData()), storageReg);
    integerResult(storageReg, m_compileIndex);
}

void SpeculativeJIT::compileGetTypedArrayLength(const TypedArrayDescriptor& descriptor, Node& node, bool needsSpeculationCheck)
{
    SpeculateCellOperand base(this, node.child1());
    GPRTemporary result(this);
    
    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();
    
    if (needsSpeculationCheck)
        speculationCheck(JSValueSource::unboxedCell(baseGPR), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
    
    m_jit.load32(MacroAssembler::Address(baseGPR, descriptor.m_lengthOffset), resultGPR);
    
    integerResult(resultGPR, m_compileIndex);
}

void SpeculativeJIT::compileGetByValOnIntTypedArray(const TypedArrayDescriptor& descriptor, Node& node, size_t elementSize, TypedArraySpeculationRequirements speculationRequirements, TypedArraySignedness signedness)
{
    SpeculateCellOperand base(this, node.child1());
    SpeculateStrictInt32Operand property(this, node.child2());
    StorageOperand storage(this, node.child3());

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();

    GPRTemporary result(this);
    GPRReg resultReg = result.gpr();

    if (speculationRequirements != NoTypedArrayTypeSpecCheck) {
        ASSERT_NOT_REACHED();
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        noResult(m_compileIndex);
        return;
    }

    MacroAssembler::Jump inBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(baseReg, descriptor.m_lengthOffset));
    m_jit.xorPtr(resultReg, resultReg);
    MacroAssembler::Jump outOfBounds = m_jit.jump();
    inBounds.link(&m_jit);
    switch (elementSize) {
    case 1:
        m_jit.load8(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne), resultReg);
        break;
    case 2:
        m_jit.load16(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesTwo), resultReg);
        break;
    case 4:
        m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesFour), resultReg);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    outOfBounds.link(&m_jit);
    if (elementSize < 4 || signedness == SignedTypedArray)
        integerResult(resultReg, m_compileIndex);
    else {
        FPRTemporary fresult(this);
        m_jit.convertInt32ToDouble(resultReg, fresult.fpr());
        JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, resultReg, TrustedImm32(0));
        m_jit.addDouble(JITCompiler::AbsoluteAddress(&twoToThe32), fresult.fpr());
        positive.link(&m_jit);
        doubleResult(fresult.fpr(), m_compileIndex);
    }
}

void SpeculativeJIT::compilePutByValForIntTypedArray(const TypedArrayDescriptor& descriptor, GPRReg base, GPRReg property, Node& node, size_t elementSize, TypedArraySpeculationRequirements speculationRequirements, TypedArraySignedness signedness)
{
    NodeIndex baseIndex = node.child1();
    NodeIndex valueIndex = node.child3();
    
    if (speculationRequirements != NoTypedArrayTypeSpecCheck)
        speculationCheck(JSValueSource::unboxedCell(base), baseIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(base), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
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
        value.adopt(scratch);
        valueGPR = scratchReg;
    } else {
        SpeculateDoubleOperand valueOp(this, valueIndex);
        GPRTemporary result(this);
        FPRReg fpr = valueOp.fpr();
        GPRReg gpr = result.gpr();
        MacroAssembler::Jump notNaN = m_jit.branchDouble(MacroAssembler::DoubleEqual, fpr, fpr);
        m_jit.xorPtr(gpr, gpr);
        MacroAssembler::Jump fixed = m_jit.jump();
        notNaN.link(&m_jit);
    
        if (signedness == SignedTypedArray)
            m_jit.truncateDoubleToInt32(fpr, gpr);
        else
            m_jit.truncateDoubleToUint32(fpr, gpr);
        fixed.link(&m_jit);
        value.adopt(result);
        valueGPR = gpr;
    }
    ASSERT_UNUSED(valueGPR, valueGPR != property);
    ASSERT(valueGPR != base);
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    ASSERT(valueGPR != storageReg);
    m_jit.loadPtr(MacroAssembler::Address(base, descriptor.m_storageOffset), storageReg);
    MacroAssembler::Jump outOfBounds;
    if (speculationRequirements != NoTypedArraySpecCheck)
        outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, property, MacroAssembler::Address(base, descriptor.m_lengthOffset));

    switch (elementSize) {
    case 1:
        m_jit.store8(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesOne));
        break;
    case 2:
        m_jit.store16(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesTwo));
        break;
    case 4:
        m_jit.store32(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesFour));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (speculationRequirements != NoTypedArraySpecCheck)
        outOfBounds.link(&m_jit);
    noResult(m_compileIndex);
}

void SpeculativeJIT::compileGetByValOnFloatTypedArray(const TypedArrayDescriptor& descriptor, Node& node, size_t elementSize, TypedArraySpeculationRequirements speculationRequirements)
{
    SpeculateCellOperand base(this, node.child1());
    SpeculateStrictInt32Operand property(this, node.child2());
    StorageOperand storage(this, node.child3());

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();
    
    if (speculationRequirements != NoTypedArrayTypeSpecCheck) {
        ASSERT_NOT_REACHED();
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        noResult(m_compileIndex);
        return;
    }

    FPRTemporary result(this);
    FPRReg resultReg = result.fpr();
    ASSERT(speculationRequirements != NoTypedArraySpecCheck);
    MacroAssembler::Jump inBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(baseReg, descriptor.m_lengthOffset));
    static const double zero = 0;
    m_jit.loadDouble(&zero, resultReg);
    MacroAssembler::Jump outOfBounds = m_jit.jump();
    inBounds.link(&m_jit);
    switch (elementSize) {
    case 4:
        m_jit.loadFloat(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesFour), resultReg);
        m_jit.convertFloatToDouble(resultReg, resultReg);
        break;
    case 8: {
        m_jit.loadDouble(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), resultReg);
        MacroAssembler::Jump notNaN = m_jit.branchDouble(MacroAssembler::DoubleEqual, resultReg, resultReg);
        static const double NaN = std::numeric_limits<double>::quiet_NaN();
        m_jit.loadDouble(&NaN, resultReg);
        notNaN.link(&m_jit);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    outOfBounds.link(&m_jit);
    doubleResult(resultReg, m_compileIndex);
}

void SpeculativeJIT::compilePutByValForFloatTypedArray(const TypedArrayDescriptor& descriptor, GPRReg base, GPRReg property, Node& node, size_t elementSize, TypedArraySpeculationRequirements speculationRequirements)
{
    NodeIndex baseIndex = node.child1();
    NodeIndex valueIndex = node.child3();
    
    SpeculateDoubleOperand valueOp(this, valueIndex);
    
    if (speculationRequirements != NoTypedArrayTypeSpecCheck)
        speculationCheck(JSValueSource::unboxedCell(base), baseIndex, m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(base), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
    
    GPRTemporary result(this);
    
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    
    m_jit.loadPtr(MacroAssembler::Address(base, descriptor.m_storageOffset), storageReg);
    MacroAssembler::Jump outOfBounds;
    if (speculationRequirements != NoTypedArraySpecCheck)
        outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, property, MacroAssembler::Address(base, descriptor.m_lengthOffset));
    
    switch (elementSize) {
    case 4: {
        FPRTemporary scratch(this);
        m_jit.moveDouble(valueOp.fpr(), scratch.fpr());
        m_jit.convertDoubleToFloat(valueOp.fpr(), scratch.fpr());
        m_jit.storeFloat(scratch.fpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesFour));
        break;
    }
    case 8:
        m_jit.storeDouble(valueOp.fpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesEight));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (speculationRequirements != NoTypedArraySpecCheck)
        outOfBounds.link(&m_jit);
    noResult(m_compileIndex);
}

void SpeculativeJIT::compileInstanceOfForObject(Node&, GPRReg valueReg, GPRReg prototypeReg, GPRReg scratchReg)
{
    // Check that prototype is an object.
    m_jit.loadPtr(MacroAssembler::Address(prototypeReg, JSCell::structureOffset()), scratchReg);
    speculationCheck(JSValueRegs(), NoNode, m_jit.branchIfNotObject(scratchReg));
    
    // Initialize scratchReg with the value being checked.
    m_jit.move(valueReg, scratchReg);
    
    // Walk up the prototype chain of the value (in scratchReg), comparing to prototypeReg.
    MacroAssembler::Label loop(&m_jit);
    m_jit.loadPtr(MacroAssembler::Address(scratchReg, JSCell::structureOffset()), scratchReg);
#if USE(JSVALUE64)
    m_jit.loadPtr(MacroAssembler::Address(scratchReg, Structure::prototypeOffset()), scratchReg);
#else
    m_jit.load32(MacroAssembler::Address(scratchReg, Structure::prototypeOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), scratchReg);
#endif
    MacroAssembler::Jump isInstance = m_jit.branchPtr(MacroAssembler::Equal, scratchReg, prototypeReg);
#if USE(JSVALUE64)
    m_jit.branchTestPtr(MacroAssembler::Zero, scratchReg, GPRInfo::tagMaskRegister).linkTo(loop, &m_jit);
#else
    m_jit.branchTest32(MacroAssembler::NonZero, scratchReg).linkTo(loop, &m_jit);
#endif
    
    // No match - result is false.
#if USE(JSVALUE64)
    m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(false))), scratchReg);
#else
    m_jit.move(MacroAssembler::TrustedImm32(0), scratchReg);
#endif
    MacroAssembler::Jump putResult = m_jit.jump();
    
    isInstance.link(&m_jit);
#if USE(JSVALUE64)
    m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(true))), scratchReg);
#else
    m_jit.move(MacroAssembler::TrustedImm32(1), scratchReg);
#endif
    
    putResult.link(&m_jit);
}

void SpeculativeJIT::compileInstanceOf(Node& node)
{
    if (!!(at(node.child1()).prediction() & ~PredictCell) && !!(m_state.forNode(node.child1()).m_type & ~PredictCell)) {
        // It might not be a cell. Speculate less aggressively.
        
        JSValueOperand value(this, node.child1());
        SpeculateCellOperand prototype(this, node.child3());
        GPRTemporary scratch(this);
        
        GPRReg prototypeReg = prototype.gpr();
        GPRReg scratchReg = scratch.gpr();
        
#if USE(JSVALUE64)
        GPRReg valueReg = value.gpr();
        MacroAssembler::Jump isCell = m_jit.branchTestPtr(MacroAssembler::Zero, valueReg, GPRInfo::tagMaskRegister);
        m_jit.move(MacroAssembler::TrustedImmPtr(JSValue::encode(jsBoolean(false))), scratchReg);
#else
        GPRReg valueTagReg = value.tagGPR();
        GPRReg valueReg = value.payloadGPR();
        MacroAssembler::Jump isCell = m_jit.branch32(MacroAssembler::Equal, valueTagReg, TrustedImm32(JSValue::CellTag));
        m_jit.move(MacroAssembler::TrustedImm32(0), scratchReg);
#endif

        MacroAssembler::Jump done = m_jit.jump();
        
        isCell.link(&m_jit);
        
        compileInstanceOfForObject(node, valueReg, prototypeReg, scratchReg);
        
        done.link(&m_jit);

#if USE(JSVALUE64)
        jsValueResult(scratchReg, m_compileIndex, DataFormatJSBoolean);
#else
        booleanResult(scratchReg, m_compileIndex);
#endif
        return;
    }
    
    SpeculateCellOperand value(this, node.child1());
    // Base unused since we speculate default InstanceOf behaviour in CheckHasInstance.
    SpeculateCellOperand prototype(this, node.child3());
    
    GPRTemporary scratch(this);
    
    GPRReg valueReg = value.gpr();
    GPRReg prototypeReg = prototype.gpr();
    GPRReg scratchReg = scratch.gpr();
    
    compileInstanceOfForObject(node, valueReg, prototypeReg, scratchReg);

#if USE(JSVALUE64)
    jsValueResult(scratchReg, m_compileIndex, DataFormatJSBoolean);
#else
    booleanResult(scratchReg, m_compileIndex);
#endif
}

static bool isPowerOfTwo(int32_t num)
{
    return num && !(num & (num - 1));
}

void SpeculativeJIT::compileSoftModulo(Node& node)
{
    bool shouldGeneratePowerOfTwoCheck = true;

    // In the fast path, the dividend value could be the final result
    // (in case of |dividend| < |divisor|), so we speculate it as strict int32.
    SpeculateStrictInt32Operand op1(this, node.child1());
    GPRReg op1Gpr = op1.gpr();

    if (isInt32Constant(node.child2())) {
        int32_t divisor = valueOfInt32Constant(node.child2());
        if (divisor < 0)
            divisor = -divisor;

        if (isPowerOfTwo(divisor)) {
            GPRTemporary result(this);
            GPRReg resultGPR = result.gpr();
            m_jit.move(op1Gpr, resultGPR);
            JITCompiler::Jump positiveDividend = m_jit.branch32(JITCompiler::GreaterThanOrEqual, op1Gpr, TrustedImm32(0));
            m_jit.neg32(resultGPR);
            m_jit.and32(TrustedImm32(divisor - 1), resultGPR);
            m_jit.neg32(resultGPR);
            JITCompiler::Jump done = m_jit.jump();

            positiveDividend.link(&m_jit);
            m_jit.and32(TrustedImm32(divisor - 1), resultGPR);

            done.link(&m_jit);
            integerResult(resultGPR, m_compileIndex);
            return;
        }
#if CPU(X86) || CPU(X86_64)
        if (divisor) {
            GPRTemporary eax(this, X86Registers::eax);
            GPRTemporary edx(this, X86Registers::edx);
            GPRTemporary scratch(this);
            GPRReg scratchGPR = scratch.gpr();

            m_jit.move(op1Gpr, eax.gpr());
            m_jit.move(TrustedImm32(divisor), scratchGPR);
            m_jit.assembler().cdq();
            m_jit.assembler().idivl_r(scratchGPR);
            integerResult(edx.gpr(), m_compileIndex);
            return;
        }
#endif
        // Fallback to non-constant case but avoid unnecessary checks.
        shouldGeneratePowerOfTwoCheck = false;
    }

    SpeculateIntegerOperand op2(this, node.child2());
    GPRReg op2Gpr = op2.gpr();

    speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(JITCompiler::Zero, op2Gpr));

#if CPU(X86) || CPU(X86_64)
    GPRTemporary eax(this, X86Registers::eax);
    GPRTemporary edx(this, X86Registers::edx);
    GPRReg temp2 = InvalidGPRReg;
    if (op2Gpr == X86Registers::eax || op2Gpr == X86Registers::edx) {
        temp2 = allocate();
        m_jit.move(op2Gpr, temp2);
        op2Gpr = temp2;
    }
    GPRReg resultGPR = edx.gpr();
    GPRReg scratchGPR = eax.gpr();
#else
    GPRTemporary result(this);
    GPRTemporary scratch(this);
    GPRReg resultGPR = result.gpr();
    GPRReg scratchGPR = scratch.gpr();
#endif

    GPRTemporary scratch2(this);
    GPRReg scratchGPR2 = scratch2.gpr();
    JITCompiler::JumpList exitBranch;

    // resultGPR is to hold the ABS value of the dividend before final result is produced
    m_jit.move(op1Gpr, resultGPR);
    // scratchGPR2 is to hold the ABS value of the divisor
    m_jit.move(op2Gpr, scratchGPR2);

    // Check for negative result remainder
    // According to ECMA-262, the sign of the result equals the sign of the dividend
    JITCompiler::Jump positiveDividend = m_jit.branch32(JITCompiler::GreaterThanOrEqual, op1Gpr, TrustedImm32(0));
    m_jit.neg32(resultGPR);
    m_jit.move(TrustedImm32(1), scratchGPR);
    JITCompiler::Jump saveCondition = m_jit.jump();

    positiveDividend.link(&m_jit);
    m_jit.move(TrustedImm32(0), scratchGPR);

    // Save the condition for negative remainder
    saveCondition.link(&m_jit);
    m_jit.push(scratchGPR);

    JITCompiler::Jump positiveDivisor = m_jit.branch32(JITCompiler::GreaterThanOrEqual, op2Gpr, TrustedImm32(0));
    m_jit.neg32(scratchGPR2);

    positiveDivisor.link(&m_jit);
    exitBranch.append(m_jit.branch32(JITCompiler::LessThan, resultGPR, scratchGPR2));

    // Power of two fast case
    if (shouldGeneratePowerOfTwoCheck) {
        m_jit.move(scratchGPR2, scratchGPR);
        m_jit.sub32(TrustedImm32(1), scratchGPR);
        JITCompiler::Jump notPowerOfTwo = m_jit.branchTest32(JITCompiler::NonZero, scratchGPR, scratchGPR2);
        m_jit.and32(scratchGPR, resultGPR);
        exitBranch.append(m_jit.jump());

        notPowerOfTwo.link(&m_jit);
    }

#if CPU(X86) || CPU(X86_64)
    m_jit.move(resultGPR, eax.gpr());
    m_jit.assembler().cdq();
    m_jit.assembler().idivl_r(scratchGPR2);
#elif CPU(ARM_THUMB2)
    GPRTemporary scratch3(this);
    GPRReg scratchGPR3 = scratch3.gpr();
    m_jit.countLeadingZeros32(scratchGPR2, scratchGPR);
    m_jit.countLeadingZeros32(resultGPR, scratchGPR3);
    m_jit.sub32(scratchGPR3, scratchGPR);

    JITCompiler::Jump useFullTable = m_jit.branch32(JITCompiler::Equal, scratchGPR, TrustedImm32(31));

    m_jit.neg32(scratchGPR);
    m_jit.add32(TrustedImm32(31), scratchGPR);

    int elementSizeByShift = -1;
    elementSizeByShift = 3;
    m_jit.relativeTableJump(scratchGPR, elementSizeByShift);

    useFullTable.link(&m_jit);
    // Modulo table
    for (int i = 31; i > 0; --i) {
        ShiftTypeAndAmount shift(SRType_LSL, i);
        m_jit.assembler().sub_S(scratchGPR, resultGPR, scratchGPR2, shift);
        m_jit.assembler().it(ARMv7Assembler::ConditionCS);
        m_jit.assembler().mov(resultGPR, scratchGPR);
    }

    JITCompiler::Jump lower = m_jit.branch32(JITCompiler::Below, resultGPR, scratchGPR2);
    m_jit.sub32(scratchGPR2, resultGPR);
    lower.link(&m_jit);
#endif // CPU(X86) || CPU(X86_64)

    exitBranch.link(&m_jit);

    // Check for negative remainder
    m_jit.pop(scratchGPR);
    JITCompiler::Jump positiveResult = m_jit.branch32(JITCompiler::Equal, scratchGPR, TrustedImm32(0));
    m_jit.neg32(resultGPR);
    positiveResult.link(&m_jit);

    integerResult(resultGPR, m_compileIndex);

#if CPU(X86) || CPU(X86_64)
    if (temp2 != InvalidGPRReg)
        unlock(temp2);
#endif
}

void SpeculativeJIT::compileArithMul(Node& node)
{
    if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())) && node.canSpeculateInteger()) {
        SpeculateIntegerOperand op1(this, node.child1());
        SpeculateIntegerOperand op2(this, node.child2());
        GPRTemporary result(this);

        GPRReg reg1 = op1.gpr();
        GPRReg reg2 = op2.gpr();

        // What is unfortunate is that we cannot take advantage of nodeCanTruncateInteger()
        // here. A multiply on integers performed in the double domain and then truncated to
        // an integer will give a different result than a multiply performed in the integer
        // domain and then truncated, if the integer domain result would have resulted in
        // something bigger than what a 32-bit integer can hold. JavaScript mandates that
        // the semantics are always as if the multiply had been performed in the double
        // domain.
            
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchMul32(MacroAssembler::Overflow, reg1, reg2, result.gpr()));
            
        // Check for negative zero, if the users of this node care about such things.
        if (!nodeCanIgnoreNegativeZero(node.arithNodeFlags())) {
            MacroAssembler::Jump resultNonZero = m_jit.branchTest32(MacroAssembler::NonZero, result.gpr());
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, reg1, TrustedImm32(0)));
            speculationCheck(JSValueRegs(), NoNode, m_jit.branch32(MacroAssembler::LessThan, reg2, TrustedImm32(0)));
            resultNonZero.link(&m_jit);
        }

        integerResult(result.gpr(), m_compileIndex);
        return;
    }

    SpeculateDoubleOperand op1(this, node.child1());
    SpeculateDoubleOperand op2(this, node.child2());
    FPRTemporary result(this, op1, op2);

    FPRReg reg1 = op1.fpr();
    FPRReg reg2 = op2.fpr();
        
    m_jit.mulDouble(reg1, reg2, result.fpr());
        
    doubleResult(result.fpr(), m_compileIndex);
}

void SpeculativeJIT::compileArithMod(Node& node)
{
    if (!at(node.child1()).shouldNotSpeculateInteger() && !at(node.child2()).shouldNotSpeculateInteger()
        && node.canSpeculateInteger()) {
        compileSoftModulo(node);
        return;
    }
        
    SpeculateDoubleOperand op1(this, node.child1());
    SpeculateDoubleOperand op2(this, node.child2());
        
    FPRReg op1FPR = op1.fpr();
    FPRReg op2FPR = op2.fpr();
        
    flushRegisters();
        
    FPRResult result(this);

    callOperation(fmodAsDFGOperation, result.fpr(), op1FPR, op2FPR);
        
    doubleResult(result.fpr(), m_compileIndex);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compare(Node& node, MacroAssembler::RelationalCondition condition, MacroAssembler::DoubleCondition doubleCondition, S_DFGOperation_EJJ operation)
{
    if (compilePeepHoleBranch(node, condition, doubleCondition, operation))
        return true;

    if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2())))
        compileIntegerCompare(node, condition);
    else if (Node::shouldSpeculateNumber(at(node.child1()), at(node.child2())))
        compileDoubleCompare(node, doubleCondition);
    else if (node.op == CompareEq && Node::shouldSpeculateFinalObject(at(node.child1()), at(node.child2())))
        compileObjectEquality(node, m_jit.globalData()->jsFinalObjectVPtr, isFinalObjectPrediction);
    else if (node.op == CompareEq && Node::shouldSpeculateArray(at(node.child1()), at(node.child2())))
        compileObjectEquality(node, m_jit.globalData()->jsArrayVPtr, isArrayPrediction);
    else
        nonSpeculativeNonPeepholeCompare(node, condition, operation);
    
    return false;
}

bool SpeculativeJIT::compileStrictEqForConstant(Node& node, NodeIndex value, JSValue constant)
{
    JSValueOperand op1(this, value);
    
    NodeIndex branchNodeIndex = detectPeepHoleBranch();
    if (branchNodeIndex != NoNode) {
        Node& branchNode = at(branchNodeIndex);
        BlockIndex taken = branchNode.takenBlockIndex();
        BlockIndex notTaken = branchNode.notTakenBlockIndex();
        MacroAssembler::RelationalCondition condition = MacroAssembler::Equal;
        
        // The branch instruction will branch to the taken block.
        // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
        if (taken == (m_block + 1)) {
            condition = MacroAssembler::NotEqual;
            BlockIndex tmp = taken;
            taken = notTaken;
            notTaken = tmp;
        }

#if USE(JSVALUE64)
        addBranch(m_jit.branchPtr(condition, op1.gpr(), MacroAssembler::TrustedImmPtr(bitwise_cast<void*>(JSValue::encode(constant)))), taken);
#else
        GPRReg payloadGPR = op1.payloadGPR();
        GPRReg tagGPR = op1.tagGPR();
        if (condition == MacroAssembler::Equal) {
            // Drop down if not equal, go elsewhere if equal.
            MacroAssembler::Jump notEqual = m_jit.branch32(MacroAssembler::NotEqual, tagGPR, MacroAssembler::Imm32(constant.tag()));
            addBranch(m_jit.branch32(MacroAssembler::Equal, payloadGPR, MacroAssembler::Imm32(constant.payload())), taken);
            notEqual.link(&m_jit);
        } else {
            // Drop down if equal, go elsehwere if not equal.
            addBranch(m_jit.branch32(MacroAssembler::NotEqual, tagGPR, MacroAssembler::Imm32(constant.tag())), taken);
            addBranch(m_jit.branch32(MacroAssembler::NotEqual, payloadGPR, MacroAssembler::Imm32(constant.payload())), taken);
        }
#endif
        
        if (notTaken != (m_block + 1))
            addBranch(m_jit.jump(), notTaken);
        
        use(node.child1());
        use(node.child2());
        m_compileIndex = branchNodeIndex;
        return true;
    }
    
    GPRTemporary result(this);
    
#if USE(JSVALUE64)
    GPRReg op1GPR = op1.gpr();
    GPRReg resultGPR = result.gpr();
    m_jit.move(MacroAssembler::TrustedImmPtr(bitwise_cast<void*>(ValueFalse)), resultGPR);
    MacroAssembler::Jump notEqual = m_jit.branchPtr(MacroAssembler::NotEqual, op1GPR, MacroAssembler::TrustedImmPtr(bitwise_cast<void*>(JSValue::encode(constant))));
    m_jit.or32(MacroAssembler::Imm32(1), resultGPR);
    notEqual.link(&m_jit);
    jsValueResult(resultGPR, m_compileIndex, DataFormatJSBoolean);
#else
    GPRReg op1PayloadGPR = op1.payloadGPR();
    GPRReg op1TagGPR = op1.tagGPR();
    GPRReg resultGPR = result.gpr();
    m_jit.move(Imm32(0), resultGPR);
    MacroAssembler::JumpList notEqual;
    notEqual.append(m_jit.branch32(MacroAssembler::NotEqual, op1TagGPR, MacroAssembler::Imm32(constant.tag())));
    notEqual.append(m_jit.branch32(MacroAssembler::NotEqual, op1PayloadGPR, MacroAssembler::Imm32(constant.payload())));
    m_jit.move(Imm32(1), resultGPR);
    notEqual.link(&m_jit);
    booleanResult(resultGPR, m_compileIndex);
#endif
    
    return false;
}

bool SpeculativeJIT::compileStrictEq(Node& node)
{
    // 1) If either operand is a constant and that constant is not a double, integer,
    //    or string, then do a JSValue comparison.
    
    if (isJSConstant(node.child1())) {
        JSValue value = valueOfJSConstant(node.child1());
        if (!value.isNumber() && !value.isString())
            return compileStrictEqForConstant(node, node.child2(), value);
    }
    
    if (isJSConstant(node.child2())) {
        JSValue value = valueOfJSConstant(node.child2());
        if (!value.isNumber() && !value.isString())
            return compileStrictEqForConstant(node, node.child1(), value);
    }
    
    // 2) If the operands are predicted integer, do an integer comparison.
    
    if (Node::shouldSpeculateInteger(at(node.child1()), at(node.child2()))) {
        NodeIndex branchNodeIndex = detectPeepHoleBranch();
        if (branchNodeIndex != NoNode) {
            compilePeepHoleIntegerBranch(node, branchNodeIndex, MacroAssembler::Equal);
            use(node.child1());
            use(node.child2());
            m_compileIndex = branchNodeIndex;
            return true;
        }
        compileIntegerCompare(node, MacroAssembler::Equal);
        return false;
    }
    
    // 3) If the operands are predicted double, do a double comparison.
    
    if (Node::shouldSpeculateNumber(at(node.child1()), at(node.child2()))) {
        NodeIndex branchNodeIndex = detectPeepHoleBranch();
        if (branchNodeIndex != NoNode) {
            compilePeepHoleDoubleBranch(node, branchNodeIndex, MacroAssembler::DoubleEqual);
            use(node.child1());
            use(node.child2());
            m_compileIndex = branchNodeIndex;
            return true;
        }
        compileDoubleCompare(node, MacroAssembler::DoubleEqual);
        return false;
    }
    
    // 4) If the operands are predicted final object or array, then do a final object
    //    or array comparison.
    
    if (Node::shouldSpeculateFinalObject(at(node.child1()), at(node.child2()))) {
        NodeIndex branchNodeIndex = detectPeepHoleBranch();
        if (branchNodeIndex != NoNode) {
            compilePeepHoleObjectEquality(node, branchNodeIndex, m_jit.globalData()->jsFinalObjectVPtr, isFinalObjectPrediction);
            use(node.child1());
            use(node.child2());
            m_compileIndex = branchNodeIndex;
            return true;
        }
        compileObjectEquality(node, m_jit.globalData()->jsFinalObjectVPtr, isFinalObjectPrediction);
        return false;
    }
    
    if (Node::shouldSpeculateArray(at(node.child1()), at(node.child2()))) {
        NodeIndex branchNodeIndex = detectPeepHoleBranch();
        if (branchNodeIndex != NoNode) {
            compilePeepHoleObjectEquality(node, branchNodeIndex, m_jit.globalData()->jsArrayVPtr, isArrayPrediction);
            use(node.child1());
            use(node.child2());
            m_compileIndex = branchNodeIndex;
            return true;
        }
        compileObjectEquality(node, m_jit.globalData()->jsArrayVPtr, isArrayPrediction);
        return false;
    }
    
    // 5) Fall back to non-speculative strict equality.
    
    return nonSpeculativeStrictEq(node);
}

void SpeculativeJIT::compileGetIndexedPropertyStorage(Node& node)
{
    SpeculateCellOperand base(this, node.child1());
    GPRReg baseReg = base.gpr();
    
    PredictedType basePrediction = at(node.child2()).prediction();
    if (!(basePrediction & PredictInt32) && basePrediction) {
        ASSERT_NOT_REACHED();
        terminateSpeculativeExecution(JSValueRegs(), NoNode);
        noResult(m_compileIndex);
        return;
    }
    
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    if (at(node.child1()).prediction() == PredictString) {
        if (!isStringPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsStringVPtr)));

        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSString::offsetOfValue()), storageReg);
        
        // Speculate that we're not accessing a rope
        speculationCheck(JSValueRegs(), NoNode, m_jit.branchTest32(MacroAssembler::Zero, storageReg));

        m_jit.loadPtr(MacroAssembler::Address(storageReg, StringImpl::dataOffset()), storageReg);
    } else if (at(node.child1()).shouldSpeculateByteArray()) {
        if (!isByteArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsByteArrayVPtr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSByteArray::offsetOfStorage()), storageReg);
    } else if (at(node.child1()).shouldSpeculateInt8Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->int8ArrayDescriptor();
        if (!isInt8ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateInt16Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->int16ArrayDescriptor();
        if (!isInt16ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateInt32Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->int32ArrayDescriptor();
        if (!isInt32ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateUint8Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->uint8ArrayDescriptor();
        if (!isUint8ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateUint16Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->uint16ArrayDescriptor();
        if (!isUint16ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateUint32Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->uint32ArrayDescriptor();
        if (!isUint32ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateFloat32Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->float32ArrayDescriptor();
        if (!isFloat32ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else if (at(node.child1()).shouldSpeculateFloat64Array()) {
        const TypedArrayDescriptor& descriptor = m_jit.globalData()->float64ArrayDescriptor();
        if (!isFloat64ArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(descriptor.m_vptr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, descriptor.m_storageOffset), storageReg);
    } else {
        if (!isArrayPrediction(m_state.forNode(node.child1()).m_type))
            speculationCheck(JSValueSource::unboxedCell(baseReg), node.child1(), m_jit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseReg), MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsArrayVPtr)));
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSArray::storageOffset()), storageReg);
    }
    storageResult(storageReg, m_compileIndex);
}

} } // namespace JSC::DFG

#endif
