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
#include "DFGJITCompiler.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGJITCodeGenerator.h"
#include "DFGNonSpeculativeJIT.h"
#include "DFGOperations.h"
#include "DFGRegisterBank.h"
#include "DFGSpeculativeJIT.h"
#include "JSGlobalData.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

// This method used to fill a numeric value to a FPR when linking speculative -> non-speculative.
void JITCompiler::fillNumericToDouble(NodeIndex nodeIndex, FPRReg fpr, GPRReg temporary)
{
    Node& node = graph()[nodeIndex];

    if (node.isConstant()) {
        ASSERT(isDoubleConstant(nodeIndex));
        move(MacroAssembler::ImmPtr(reinterpret_cast<void*>(reinterpretDoubleToIntptr(valueOfDoubleConstant(nodeIndex)))), temporary);
        movePtrToDouble(temporary, fpr);
    } else {
        loadPtr(addressFor(node.virtualRegister()), temporary);
        Jump isInteger = branchPtr(MacroAssembler::AboveOrEqual, temporary, GPRInfo::tagTypeNumberRegister);
        jitAssertIsJSDouble(temporary);
        addPtr(GPRInfo::tagTypeNumberRegister, temporary);
        movePtrToDouble(temporary, fpr);
        Jump hasUnboxedDouble = jump();
        isInteger.link(this);
        convertInt32ToDouble(temporary, fpr);
        hasUnboxedDouble.link(this);
    }
}

// This method used to fill an integer value to a GPR when linking speculative -> non-speculative.
void JITCompiler::fillInt32ToInteger(NodeIndex nodeIndex, GPRReg gpr)
{
    Node& node = graph()[nodeIndex];

    if (node.isConstant()) {
        ASSERT(isInt32Constant(nodeIndex));
        move(MacroAssembler::Imm32(valueOfInt32Constant(nodeIndex)), gpr);
    } else {
#if DFG_JIT_ASSERT
        // Redundant load, just so we can check the tag!
        loadPtr(addressFor(node.virtualRegister()), gpr);
        jitAssertIsJSInt32(gpr);
#endif
        load32(addressFor(node.virtualRegister()), gpr);
    }
}

// This method used to fill a JSValue to a GPR when linking speculative -> non-speculative.
void JITCompiler::fillToJS(NodeIndex nodeIndex, GPRReg gpr)
{
    Node& node = graph()[nodeIndex];

    if (node.isConstant()) {
        if (isInt32Constant(nodeIndex)) {
            JSValue jsValue = jsNumber(valueOfInt32Constant(nodeIndex));
            move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
        } else if (isDoubleConstant(nodeIndex)) {
            JSValue jsValue(JSValue::EncodeAsDouble, valueOfDoubleConstant(nodeIndex));
            move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
        } else {
            ASSERT(isJSConstant(nodeIndex));
            JSValue jsValue = valueOfJSConstant(nodeIndex);
            move(MacroAssembler::ImmPtr(JSValue::encode(jsValue)), gpr);
        }
        return;
    }

    loadPtr(addressFor(node.virtualRegister()), gpr);
}

class GeneralizedRegister {
public:
    GeneralizedRegister() { }
    
    static GeneralizedRegister createGPR(GPRReg gpr)
    {
        GeneralizedRegister result;
        result.m_isFPR = false;
        result.m_register.gpr = gpr;
        return result;
    }
    
    static GeneralizedRegister createFPR(FPRReg fpr)
    {
        GeneralizedRegister result;
        result.m_isFPR = true;
        result.m_register.fpr = fpr;
        return result;
    }
    
    bool isFPR() const
    {
        return m_isFPR;
    }
    
    GPRReg gpr() const
    {
        ASSERT(!m_isFPR);
        return m_register.gpr;
    }
    
    FPRReg fpr() const
    {
        ASSERT(m_isFPR);
        return m_register.fpr;
    }
    
    const SpeculationCheck::RegisterInfo& findInSpeculationCheck(const SpeculationCheck& check)
    {
        if (isFPR())
            return check.m_fprInfo[FPRInfo::toIndex(fpr())];
        return check.m_gprInfo[GPRInfo::toIndex(gpr())];
    }
    
    const EntryLocation::RegisterInfo& findInEntryLocation(const EntryLocation& entry)
    {
        if (isFPR())
            return entry.m_fprInfo[FPRInfo::toIndex(fpr())];
        return entry.m_gprInfo[GPRInfo::toIndex(gpr())];
    }
    
    DataFormat previousDataFormat(const SpeculationCheck& check)
    {
        return findInSpeculationCheck(check).format;
    }
    
    DataFormat nextDataFormat(const EntryLocation& entry)
    {
        return findInEntryLocation(entry).format;
    }
    
    void convert(DataFormat oldDataFormat, DataFormat newDataFormat, JITCompiler& jit)
    {
        if (LIKELY(!needDataFormatConversion(oldDataFormat, newDataFormat)))
            return;
        
        if (oldDataFormat == DataFormatInteger) {
            jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr());
            return;
        }
        
        ASSERT(newDataFormat == DataFormatInteger);
        jit.zeroExtend32ToPtr(gpr(), gpr());
        return;
    }
    
    void moveTo(GeneralizedRegister& other, DataFormat myDataFormat, DataFormat otherDataFormat, JITCompiler& jit, FPRReg scratchFPR)
    {
        if (UNLIKELY(isFPR())) {
            if (UNLIKELY(other.isFPR())) {
                jit.moveDouble(fpr(), other.fpr());
                return;
            }
            
            JITCompiler::Jump done;
            
            if (scratchFPR != InvalidFPRReg) {
                // we have a scratch FPR, so attempt a conversion to int
                JITCompiler::JumpList notInt;
                jit.branchConvertDoubleToInt32(fpr(), other.gpr(), notInt, scratchFPR);
                jit.orPtr(GPRInfo::tagTypeNumberRegister, other.gpr());
                done = jit.jump();
                notInt.link(&jit);
            }
            
            jit.moveDoubleToPtr(fpr(), other.gpr());
            jit.subPtr(GPRInfo::tagTypeNumberRegister, other.gpr());
            
            if (done.isSet())
                done.link(&jit);
            return;
        }
        
        if (UNLIKELY(other.isFPR())) {
            jit.addPtr(GPRInfo::tagTypeNumberRegister, gpr());
            jit.movePtrToDouble(gpr(), other.fpr());
            return;
        }
        
        if (LIKELY(!needDataFormatConversion(myDataFormat, otherDataFormat))) {
            jit.move(gpr(), other.gpr());
            return;
        }
        
        if (myDataFormat == DataFormatInteger) {
            jit.orPtr(gpr(), GPRInfo::tagTypeNumberRegister, other.gpr());
            return;
        }
        
        ASSERT(otherDataFormat == DataFormatInteger);
        jit.zeroExtend32ToPtr(gpr(), other.gpr());
    }
    
    void swapWith(GeneralizedRegister& other, DataFormat myDataFormat, DataFormat myNewDataFormat, DataFormat otherDataFormat, DataFormat otherNewDataFormat, JITCompiler& jit, GPRReg scratchGPR, FPRReg scratchFPR)
    {
        if (UNLIKELY(isFPR())) {
            if (UNLIKELY(other.isFPR())) {
                if (scratchFPR == InvalidFPRReg)
                    jit.moveDoubleToPtr(fpr(), scratchGPR);
                else
                    jit.moveDouble(fpr(), scratchFPR);
                jit.moveDouble(other.fpr(), fpr());
                if (scratchFPR == InvalidFPRReg)
                    jit.movePtrToDouble(scratchGPR, other.fpr());
                else
                    jit.moveDouble(scratchFPR, other.fpr());
                return;
            }
            
            jit.move(other.gpr(), scratchGPR);
            
            JITCompiler::Jump done;
            
            if (scratchFPR != InvalidFPRReg) {
                JITCompiler::JumpList notInt;
                jit.branchConvertDoubleToInt32(fpr(), other.gpr(), notInt, scratchFPR);
                jit.orPtr(GPRInfo::tagTypeNumberRegister, other.gpr());
                done = jit.jump();
                notInt.link(&jit);
            }
            
            jit.moveDoubleToPtr(fpr(), other.gpr());
            jit.subPtr(GPRInfo::tagTypeNumberRegister, other.gpr());
            
            if (done.isSet())
                done.link(&jit);
            
            jit.addPtr(GPRInfo::tagTypeNumberRegister, scratchGPR);
            jit.movePtrToDouble(scratchGPR, fpr());
            return;
        }
        
        if (UNLIKELY(other.isFPR())) {
            other.swapWith(*this, otherDataFormat, otherNewDataFormat, myDataFormat, myNewDataFormat, jit, scratchGPR, scratchFPR);
            return;
        }
        
        jit.swap(gpr(), other.gpr());
        
        if (UNLIKELY(needDataFormatConversion(otherDataFormat, myNewDataFormat))) {
            if (otherDataFormat == DataFormatInteger)
                jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr());
            else if (myNewDataFormat == DataFormatInteger)
                jit.zeroExtend32ToPtr(gpr(), gpr());
        }
        
        if (UNLIKELY(needDataFormatConversion(myDataFormat, otherNewDataFormat))) {
            if (myDataFormat == DataFormatInteger)
                jit.orPtr(GPRInfo::tagTypeNumberRegister, other.gpr());
            else if (otherNewDataFormat == DataFormatInteger)
                jit.zeroExtend32ToPtr(other.gpr(), other.gpr());
        }
    }

private:
    bool m_isFPR;
    union {
        GPRReg gpr;
        FPRReg fpr;
    } m_register;
};

struct ShuffledRegister {
    GeneralizedRegister reg;
    ShuffledRegister* previous;
    bool hasFrom;
    bool hasTo;
    bool handled;
    
    ShuffledRegister() { }
    
    ShuffledRegister(GeneralizedRegister reg)
        : reg(reg)
        , previous(0)
        , hasFrom(false)
        , hasTo(false)
        , handled(false)
    {
    }
    
    bool isEndOfNonCyclingPermutation()
    {
        return hasTo && !hasFrom;
    }
    
    void handleNonCyclingPermutation(const SpeculationCheck& check, const EntryLocation& entry, JITCompiler& jit, FPRReg& scratchFPR1, FPRReg& scratchFPR2)
    {
        ShuffledRegister* cur = this;
        while (cur->previous) {
            cur->previous->reg.moveTo(cur->reg, cur->previous->reg.previousDataFormat(check), cur->reg.nextDataFormat(entry), jit, scratchFPR1);
            cur->handled = true;
            if (cur->reg.isFPR()) {
                if (scratchFPR1 == InvalidFPRReg)
                    scratchFPR1 = cur->reg.fpr();
                else {
                    ASSERT(scratchFPR1 != cur->reg.fpr());
                    scratchFPR2 = cur->reg.fpr();
                }
            }
            cur = cur->previous;
        }
        cur->handled = true;
        if (cur->reg.isFPR()) {
            if (scratchFPR1 == InvalidFPRReg)
                scratchFPR1 = cur->reg.fpr();
            else {
                ASSERT(scratchFPR1 != cur->reg.fpr());
                scratchFPR2 = cur->reg.fpr();
            }
        }
    }
    
    void handleCyclingPermutation(const SpeculationCheck& check, const EntryLocation& entry, JITCompiler& jit, GPRReg scratchGPR, FPRReg scratchFPR1, FPRReg scratchFPR2)
    {
        // first determine the cycle length
        
        unsigned cycleLength = 0;
        
        ShuffledRegister* cur = this;
        ShuffledRegister* next = 0;
        do {
            ASSERT(cur);
            cycleLength++;
            cur->handled = true;
            next = cur;
            cur = cur->previous;
        } while (cur != this);
        
        ASSERT(cycleLength);
        ASSERT(next->previous == cur);
        
        // now determine the best way to handle the permutation, depending on the
        // length.
        
        switch (cycleLength) {
        case 1:
            reg.convert(reg.previousDataFormat(check), reg.nextDataFormat(entry), jit);
            break;
            
        case 2:
            reg.swapWith(previous->reg, reg.previousDataFormat(check), reg.nextDataFormat(entry), previous->reg.previousDataFormat(check), previous->reg.nextDataFormat(entry), jit, scratchGPR, scratchFPR1);
            break;
            
        default:
            GeneralizedRegister scratch;
            if (UNLIKELY(reg.isFPR() && next->reg.isFPR())) {
                if (scratchFPR2 == InvalidFPRReg) {
                    scratch = GeneralizedRegister::createGPR(scratchGPR);
                    reg.moveTo(scratch, DataFormatDouble, DataFormatJSDouble, jit, scratchFPR1);
                } else {
                    scratch = GeneralizedRegister::createFPR(scratchFPR2);
                    reg.moveTo(scratch, DataFormatDouble, DataFormatDouble, jit, scratchFPR1);
                }
            } else {
                scratch = GeneralizedRegister::createGPR(scratchGPR);
                reg.moveTo(scratch, reg.previousDataFormat(check), next->reg.nextDataFormat(entry), jit, scratchFPR1);
            }
            
            cur = this;
            while (cur->previous != this) {
                ASSERT(cur);
                cur->previous->reg.moveTo(cur->reg, cur->previous->reg.previousDataFormat(check), cur->reg.nextDataFormat(entry), jit, scratchFPR1);
                cur = cur->previous;
            }
            
            if (UNLIKELY(reg.isFPR() && next->reg.isFPR())) {
                if (scratchFPR2 == InvalidFPRReg)
                    scratch.moveTo(next->reg, DataFormatJSDouble, DataFormatDouble, jit, scratchFPR1);
                else
                    scratch.moveTo(next->reg, DataFormatDouble, DataFormatDouble, jit, scratchFPR1);
            } else
                scratch.moveTo(next->reg, next->reg.nextDataFormat(entry), next->reg.nextDataFormat(entry), jit, scratchFPR1);
            break;
        }
    }
    
    static ShuffledRegister* lookup(ShuffledRegister* gprs, ShuffledRegister* fprs, GeneralizedRegister& reg)
    {
        if (reg.isFPR())
            return fprs + FPRInfo::toIndex(reg.fpr());
        return gprs + GPRInfo::toIndex(reg.gpr());
    }
};

template<typename T>
T& lookupForRegister(T* gprs, T* fprs, unsigned index)
{
    ASSERT(index < GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);
    if (index < GPRInfo::numberOfRegisters)
        return gprs[index];
    return fprs[index - GPRInfo::numberOfRegisters];
}

// This is written in a way that allows for a HashMap<NodeIndex, GeneralizedRegister> to be
// easily substituted, if it is found to be wise to do so. So far performance measurements
// indicate that this is faster, likely because the HashMap would have never grown very big
// and we would thus be wasting time performing complex hashing logic that, though O(1) on
// average, would be less than the ~7 loop iterations that the find() method below would do
// (since it's uncommon that we'd have register allocated more than 7 registers, in the
// current scheme).
class NodeToRegisterMap {
public:
    struct Tuple {
        NodeIndex first;
        GeneralizedRegister second;
        
        Tuple()
        {
        }
    };
    
    typedef Tuple* iterator;
    
    NodeToRegisterMap()
        : m_occupancy(0)
    {
    }
    
    void set(NodeIndex first, GeneralizedRegister second)
    {
        m_payload[m_occupancy].first = first;
        m_payload[m_occupancy].second = second;
        m_occupancy++;
    }
    
    Tuple* end()
    {
        return 0;
    }
    
    Tuple* find(NodeIndex first)
    {
        for (unsigned i = m_occupancy; i-- > 0;) {
            if (m_payload[i].first == first)
                return m_payload + i;
        }
        return 0;
    }
    
    void clear()
    {
        m_occupancy = 0;
    }
    
private:
    Tuple m_payload[GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters];
    unsigned m_occupancy;
};

void JITCompiler::jumpFromSpeculativeToNonSpeculative(const SpeculationCheck& check, const EntryLocation& entry, SpeculationRecovery* recovery, NodeToRegisterMap& checkNodeToRegisterMap, NodeToRegisterMap& entryNodeToRegisterMap)
{
    ASSERT(check.m_nodeIndex == entry.m_nodeIndex);

    // Link the jump from the Speculative path to here.
    check.m_check.link(this);

#if DFG_DEBUG_VERBOSE
    fprintf(stderr, "Speculation failure for Node @%d at JIT offset 0x%x\n", (int)check.m_nodeIndex, debugOffset());
#endif
#if DFG_JIT_BREAK_ON_SPECULATION_FAILURE
    breakpoint();
#endif

    // Does this speculation check require any additional recovery to be performed,
    // to restore any state that has been overwritten before we enter back in to the
    // non-speculative path.
    if (recovery) {
        // The only additional recovery we currently support is for integer add operation
        ASSERT(recovery->type() == SpeculativeAdd);
        ASSERT(check.m_gprInfo[GPRInfo::toIndex(recovery->dest())].nodeIndex != NoNode);
        // Revert the add.
        sub32(recovery->src(), recovery->dest());
        
        // If recovery->dest() should have been boxed prior to the addition, then rebox
        // it.
        DataFormat format = check.m_gprInfo[GPRInfo::toIndex(recovery->dest())].format;
        ASSERT(format == DataFormatInteger || format == DataFormatJSInteger || format == DataFormatJS);
        if (format != DataFormatInteger)
            orPtr(GPRInfo::tagTypeNumberRegister, recovery->dest());
    }
    
    // First, we need a reverse mapping that tells us, for a NodeIndex, which register
    // that node is in.
    
    checkNodeToRegisterMap.clear();
    entryNodeToRegisterMap.clear();
    
    GPRReg scratchGPR = InvalidGPRReg;
    FPRReg scratchFPR1 = InvalidFPRReg;
    FPRReg scratchFPR2 = InvalidFPRReg;
    bool needToRestoreTagMaskRegister = false;
    
    for (unsigned index = 0; index < GPRInfo::numberOfRegisters; ++index) {
        NodeIndex nodeIndexInCheck = check.m_gprInfo[index].nodeIndex;
        if (nodeIndexInCheck != NoNode)
            checkNodeToRegisterMap.set(nodeIndexInCheck, GeneralizedRegister::createGPR(GPRInfo::toRegister(index)));
        NodeIndex nodeIndexInEntry = entry.m_gprInfo[index].nodeIndex;
        if (nodeIndexInEntry != NoNode)
            entryNodeToRegisterMap.set(nodeIndexInEntry, GeneralizedRegister::createGPR(GPRInfo::toRegister(index)));
        else if (nodeIndexInCheck == NoNode)
            scratchGPR = GPRInfo::toRegister(index);
    }
    
    for (unsigned index = 0; index < FPRInfo::numberOfRegisters; ++index) {
        NodeIndex nodeIndexInCheck = check.m_fprInfo[index].nodeIndex;
        if (nodeIndexInCheck != NoNode)
            checkNodeToRegisterMap.set(nodeIndexInCheck, GeneralizedRegister::createFPR(FPRInfo::toRegister(index)));
        NodeIndex nodeIndexInEntry = entry.m_fprInfo[index].nodeIndex;
        if (nodeIndexInEntry != NoNode)
            entryNodeToRegisterMap.set(nodeIndexInEntry, GeneralizedRegister::createFPR(FPRInfo::toRegister(index)));
        else if (nodeIndexInCheck == NoNode) {
            if (scratchFPR1 == InvalidFPRReg)
                scratchFPR1 = FPRInfo::toRegister(index);
            else
                scratchFPR2 = FPRInfo::toRegister(index);
        }
    }
    
    ASSERT((scratchFPR1 == InvalidFPRReg && scratchFPR2 == InvalidFPRReg) || (scratchFPR1 != scratchFPR2));
    
    // How this works:
    // 1) Spill any values that are not spilled on speculative, but are spilled
    //    on non-speculative.
    // 2) For the set of nodes that are in registers on both paths, perform a
    //    shuffling.
    // 3) Fill any values that were spilled on speculative, but are not spilled
    //    on non-speculative.
    
    // If we find registers that can be used as scratch registers along the way,
    // save them.
    
    // Part 1: spill any values that are not spilled on speculative, but are
    //         spilled on non-speculative.
    
    // This also sets up some data structures that Part 2 will need.
    
    ShuffledRegister gprs[GPRInfo::numberOfRegisters];
    ShuffledRegister fprs[FPRInfo::numberOfRegisters];
    
    for (unsigned index = 0; index < GPRInfo::numberOfRegisters; ++index)
        gprs[index] = ShuffledRegister(GeneralizedRegister::createGPR(GPRInfo::toRegister(index)));
    for (unsigned index = 0; index < FPRInfo::numberOfRegisters; ++index)
        fprs[index] = ShuffledRegister(GeneralizedRegister::createFPR(FPRInfo::toRegister(index)));

    for (unsigned index = 0; index < GPRInfo::numberOfRegisters; ++index) {
        NodeIndex nodeIndex = check.m_gprInfo[index].nodeIndex;
        
        // Bail out if this register isn't assigned to anything.
        if (nodeIndex == NoNode)
            continue;
        
        // If the non-speculative path also has a register for the nodeIndex that this
        // register stores, link them together.
        NodeToRegisterMap::iterator mapIterator = entryNodeToRegisterMap.find(nodeIndex);
        if (mapIterator != entryNodeToRegisterMap.end()) {
            gprs[index].hasFrom = true;
            
            ShuffledRegister* next = ShuffledRegister::lookup(gprs, fprs, mapIterator->second);
            next->previous = gprs + index;
            next->hasTo = true;
            
            // If the non-speculative path has not spilled this register, then skip the spillin
            // part below regardless of whether or not the speculative path has spilled it.
            if (!mapIterator->second.findInEntryLocation(entry).isSpilled)
                continue;
        } else {
            // If the non-speculative entry isn't using this register and it does not need
            // the value in this register to be placed into any other register, then this
            // register can be used for scratch.
            if (entry.m_gprInfo[index].nodeIndex == NoNode)
                scratchGPR = GPRInfo::toRegister(index);
        }
        
        // If the speculative path has already spilled the register then there is no need to
        // spill it.
        if (check.m_gprInfo[index].isSpilled)
            continue;
        
        DataFormat dataFormat = check.m_gprInfo[index].format;
        VirtualRegister virtualRegister = graph()[nodeIndex].virtualRegister();
        
        ASSERT(dataFormat == DataFormatInteger || DataFormatCell || dataFormat & DataFormatJS);
        if (dataFormat == DataFormatInteger)
            orPtr(GPRInfo::tagTypeNumberRegister, GPRInfo::toRegister(index));
        storePtr(GPRInfo::toRegister(index), addressFor(virtualRegister));
    }
    
    if (scratchGPR == InvalidGPRReg) {
        scratchGPR = GPRInfo::tagMaskRegister;
        needToRestoreTagMaskRegister = true;
    }

    for (unsigned index = 0; index < FPRInfo::numberOfRegisters; ++index) {
        NodeIndex nodeIndex = check.m_fprInfo[index].nodeIndex;
        if (nodeIndex == NoNode)
            continue;

        NodeToRegisterMap::iterator mapIterator = entryNodeToRegisterMap.find(nodeIndex);
        if (mapIterator != entryNodeToRegisterMap.end()) {
            fprs[index].hasFrom = true;
            
            ShuffledRegister* next = ShuffledRegister::lookup(gprs, fprs, mapIterator->second);
            next->previous = fprs + index;
            next->hasTo = true;

            if (!mapIterator->second.findInEntryLocation(entry).isSpilled)
                continue;
        } else {
            // If the non-speculative entry isn't using this register and it does not need
            // the value in this register to be placed into any other register, then this
            // register can be used for scratch.
            if (entry.m_fprInfo[index].nodeIndex == NoNode) {
                if (scratchFPR1 == InvalidFPRReg)
                    scratchFPR1 = FPRInfo::toRegister(index);
                else if (scratchFPR2)
                    scratchFPR2 = FPRInfo::toRegister(index);
                ASSERT((scratchFPR1 == InvalidFPRReg && scratchFPR2 == InvalidFPRReg) || (scratchFPR1 != scratchFPR2));
            }
        }
        
        if (check.m_fprInfo[index].isSpilled)
            continue;

        VirtualRegister virtualRegister = graph()[nodeIndex].virtualRegister();

        moveDoubleToPtr(FPRInfo::toRegister(index), scratchGPR);
        subPtr(GPRInfo::tagTypeNumberRegister, scratchGPR);
        storePtr(scratchGPR, addressFor(virtualRegister));
    }
    
#if !ASSERT_DISABLED
    // Assert that we've not assigned a scratch register to something that we're going to shuffle.
    ASSERT(scratchGPR != InvalidGPRReg);
    if (scratchGPR != GPRInfo::tagMaskRegister) {
        ASSERT(!gprs[GPRInfo::toIndex(scratchGPR)].hasTo);
        ASSERT(!gprs[GPRInfo::toIndex(scratchGPR)].hasFrom);
    }
    if (scratchFPR1 != InvalidFPRReg) {
        ASSERT(scratchFPR1 != scratchFPR2);
        ASSERT(!fprs[FPRInfo::toIndex(scratchFPR1)].hasTo);
        ASSERT(!fprs[FPRInfo::toIndex(scratchFPR1)].hasFrom);
        if (scratchFPR2 != InvalidFPRReg) {
            ASSERT(!fprs[FPRInfo::toIndex(scratchFPR2)].hasTo);
            ASSERT(!fprs[FPRInfo::toIndex(scratchFPR2)].hasFrom);
        }
    } else
        ASSERT(scratchFPR2 == InvalidFPRReg);
#endif
    
    // Part 2: For the set of nodes that are in registers on both paths,
    //         perform a shuffling.
    
    for (unsigned index = 0; index < GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters; ++index) {
        ShuffledRegister& reg = lookupForRegister(gprs, fprs, index);
        if (!reg.isEndOfNonCyclingPermutation() || reg.handled || (!reg.hasFrom && !reg.hasTo))
            continue;
        
        reg.handleNonCyclingPermutation(check, entry, *this, scratchFPR1, scratchFPR2);
        ASSERT((scratchFPR1 == InvalidFPRReg && scratchFPR2 == InvalidFPRReg) || (scratchFPR1 != scratchFPR2));
    }
    
    for (unsigned index = 0; index < GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters; ++index) {
        ShuffledRegister& reg = lookupForRegister(gprs, fprs, index);
        if (reg.handled || (!reg.hasFrom && !reg.hasTo))
            continue;
        
        reg.handleCyclingPermutation(check, entry, *this, scratchGPR, scratchFPR1, scratchFPR2);
        ASSERT((scratchFPR1 == InvalidFPRReg && scratchFPR2 == InvalidFPRReg) || (scratchFPR1 != scratchFPR2));
    }

#if !ASSERT_DISABLED
    for (unsigned index = 0; index < GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters; ++index) {
        ShuffledRegister& reg = lookupForRegister(gprs, fprs, index);
        ASSERT(reg.handled || (!reg.hasFrom && !reg.hasTo));
    }
#endif

    // Part 3: Fill any values that were spilled on speculative, but are not spilled
    //         on non-speculative.

    for (unsigned index = 0; index < FPRInfo::numberOfRegisters; ++index) {
        NodeIndex nodeIndex = entry.m_fprInfo[index].nodeIndex;
        if (nodeIndex == NoNode || entry.m_fprInfo[index].isSpilled)
            continue;
        
        NodeToRegisterMap::iterator mapIterator = checkNodeToRegisterMap.find(nodeIndex);
        if (mapIterator != checkNodeToRegisterMap.end()
            && !mapIterator->second.findInSpeculationCheck(check).isSpilled)
            continue;

        fillNumericToDouble(nodeIndex, FPRInfo::toRegister(index), GPRInfo::regT0);
    }

    for (unsigned index = 0; index < GPRInfo::numberOfRegisters; ++index) {
        NodeIndex nodeIndex = entry.m_gprInfo[index].nodeIndex;
        if (nodeIndex == NoNode || entry.m_gprInfo[index].isSpilled)
            continue;

        NodeToRegisterMap::iterator mapIterator = checkNodeToRegisterMap.find(nodeIndex);
        if (mapIterator != checkNodeToRegisterMap.end()
            && !mapIterator->second.findInSpeculationCheck(check).isSpilled)
            continue;

        DataFormat dataFormat = entry.m_gprInfo[index].format;
        if (dataFormat == DataFormatInteger)
            fillInt32ToInteger(nodeIndex, GPRInfo::toRegister(index));
        else {
            ASSERT(dataFormat & DataFormatJS || dataFormat == DataFormatCell); // Treat cell as JSValue for now!
            fillToJS(nodeIndex, GPRInfo::toRegister(index));
            // FIXME: For subtypes of DataFormatJS, should jitAssert the subtype?
        }
    }
    
    if (needToRestoreTagMaskRegister)
        move(TrustedImmPtr(reinterpret_cast<void*>(TagMask)), GPRInfo::tagMaskRegister);

    // Jump into the non-speculative path.
    jump(entry.m_entry);
}

void JITCompiler::linkSpeculationChecks(SpeculativeJIT& speculative, NonSpeculativeJIT& nonSpeculative)
{
    // Iterators to walk over the set of bail outs & corresponding entry points.
    SpeculationCheckVector::Iterator checksIter = speculative.speculationChecks().begin();
    SpeculationCheckVector::Iterator checksEnd = speculative.speculationChecks().end();
    NonSpeculativeJIT::EntryLocationVector::Iterator entriesIter = nonSpeculative.entryLocations().begin();
    NonSpeculativeJIT::EntryLocationVector::Iterator entriesEnd = nonSpeculative.entryLocations().end();
    
    NodeToRegisterMap checkNodeToRegisterMap;
    NodeToRegisterMap entryNodeToRegisterMap;
    
    // Iterate over the speculation checks.
    while (checksIter != checksEnd) {
        // For every bail out from the speculative path, we must have provided an entry point
        // into the non-speculative one.
        ASSERT(checksIter->m_nodeIndex == entriesIter->m_nodeIndex);

        // There may be multiple bail outs that map to the same entry point!
        do {
            ASSERT(checksIter != checksEnd);
            ASSERT(entriesIter != entriesEnd);

            // Plant code to link this speculation failure.
            const SpeculationCheck& check = *checksIter;
            const EntryLocation& entry = *entriesIter;
            jumpFromSpeculativeToNonSpeculative(check, entry, speculative.speculationRecovery(check.m_recoveryIndex), checkNodeToRegisterMap, entryNodeToRegisterMap);
             ++checksIter;
        } while (checksIter != checksEnd && checksIter->m_nodeIndex == entriesIter->m_nodeIndex);
         ++entriesIter;
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56289
    ASSERT(!(checksIter != checksEnd));
    ASSERT(!(entriesIter != entriesEnd));
}

void JITCompiler::compileEntry()
{
    // This code currently matches the old JIT. In the function header we need to
    // pop the return address (since we do not allow any recursion on the machine
    // stack), and perform a fast register file check.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56292
    // We'll need to convert the remaining cti_ style calls (specifically the register file
    // check) which will be dependent on stack layout. (We'd need to account for this in
    // both normal return code and when jumping to an exception handler).
    preserveReturnAddressAfterCall(GPRInfo::regT2);
    emitPutToCallFrameHeader(GPRInfo::regT2, RegisterFile::ReturnPC);
}

void JITCompiler::compileBody()
{
    // We generate the speculative code path, followed by the non-speculative
    // code for the function. Next we need to link the two together, making
    // bail-outs from the speculative path jump to the corresponding point on
    // the non-speculative one (and generating any code necessary to juggle
    // register values around, rebox values, and ensure spilled, to match the
    // non-speculative path's requirements).

#if DFG_JIT_BREAK_ON_EVERY_FUNCTION
    // Handy debug tool!
    breakpoint();
#endif

    // First generate the speculative path.
    Label speculativePathBegin = label();
    SpeculativeJIT speculative(*this);
#if !DFG_DEBUG_LOCAL_DISBALE_SPECULATIVE
    bool compiledSpeculative = speculative.compile();
#else
    bool compiledSpeculative = false;
#endif

    // Next, generate the non-speculative path. We pass this a SpeculationCheckIndexIterator
    // to allow it to check which nodes in the graph may bail out, and may need to reenter the
    // non-speculative path.
    if (compiledSpeculative) {
        SpeculationCheckIndexIterator checkIterator(speculative.speculationChecks());
        NonSpeculativeJIT nonSpeculative(*this);
        nonSpeculative.compile(checkIterator);

        // Link the bail-outs from the speculative path to the corresponding entry points into the non-speculative one.
        linkSpeculationChecks(speculative, nonSpeculative);
    } else {
#if DFG_DEBUG_VERBOSE
        fprintf(stderr, "SpeculativeJIT was terminated.\n");
#endif

        // If compilation through the SpeculativeJIT failed, throw away the code we generated.
        m_calls.clear();
        m_propertyAccesses.clear();
        m_jsCalls.clear();
        m_methodGets.clear();
        rewindToLabel(speculativePathBegin);

        SpeculationCheckVector noChecks;
        SpeculationCheckIndexIterator checkIterator(noChecks);
        NonSpeculativeJIT nonSpeculative(*this);
        nonSpeculative.compile(checkIterator);
    }

    // Iterate over the m_calls vector, checking for exception checks,
    // and linking them to here.
    for (unsigned i = 0; i < m_calls.size(); ++i) {
        Jump& exceptionCheck = m_calls[i].m_exceptionCheck;
        if (exceptionCheck.isSet()) {
            exceptionCheck.link(this);
            ++m_exceptionCheckCount;
        }
    }
    // If any exception checks were linked, generate code to lookup a handler.
    if (m_exceptionCheckCount) {
        // lookupExceptionHandler is passed two arguments, exec (the CallFrame*), and
        // an identifier for the operation that threw the exception, which we can use
        // to look up handler information. The identifier we use is the return address
        // of the call out from JIT code that threw the exception; this is still
        // available on the stack, just below the stack pointer!
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        peek(GPRInfo::argumentGPR1, -1);
        m_calls.append(CallRecord(call(), lookupExceptionHandler));
        // lookupExceptionHandler leaves the handler CallFrame* in the returnValueGPR,
        // and the address of the handler in returnValueGPR2.
        jump(GPRInfo::returnValueGPR2);
    }
}

void JITCompiler::link(LinkBuffer& linkBuffer)
{
    // Link the code, populate data in CodeBlock data structures.
#if DFG_DEBUG_VERBOSE
    fprintf(stderr, "JIT code start at %p\n", linkBuffer.debugAddress());
#endif

    // Link all calls out from the JIT code to their respective functions.
    for (unsigned i = 0; i < m_calls.size(); ++i) {
        if (m_calls[i].m_function.value())
            linkBuffer.link(m_calls[i].m_call, m_calls[i].m_function);
    }

    if (m_codeBlock->needsCallReturnIndices()) {
        m_codeBlock->callReturnIndexVector().reserveCapacity(m_exceptionCheckCount);
        for (unsigned i = 0; i < m_calls.size(); ++i) {
            if (m_calls[i].m_handlesExceptions) {
                unsigned returnAddressOffset = linkBuffer.returnAddressOffset(m_calls[i].m_call);
                unsigned exceptionInfo = m_calls[i].m_exceptionInfo;
                m_codeBlock->callReturnIndexVector().append(CallReturnOffsetToBytecodeOffset(returnAddressOffset, exceptionInfo));
            }
        }
    }

    m_codeBlock->setNumberOfStructureStubInfos(m_propertyAccesses.size());
    for (unsigned i = 0; i < m_propertyAccesses.size(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(i);
        info.callReturnLocation = linkBuffer.locationOf(m_propertyAccesses[i].m_functionCall);
        info.u.unset.deltaCheckImmToCall = m_propertyAccesses[i].m_deltaCheckImmToCall;
        info.deltaCallToStructCheck = m_propertyAccesses[i].m_deltaCallToStructCheck;
        info.u.unset.deltaCallToLoadOrStore = m_propertyAccesses[i].m_deltaCallToLoadOrStore;
        info.deltaCallToSlowCase = m_propertyAccesses[i].m_deltaCallToSlowCase;
        info.deltaCallToDone = m_propertyAccesses[i].m_deltaCallToDone;
        info.baseGPR = m_propertyAccesses[i].m_baseGPR;
        info.valueGPR = m_propertyAccesses[i].m_valueGPR;
        info.scratchGPR = m_propertyAccesses[i].m_scratchGPR;
    }
    
    m_codeBlock->setNumberOfCallLinkInfos(m_jsCalls.size());
    for (unsigned i = 0; i < m_jsCalls.size(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.isCall = m_jsCalls[i].m_isCall;
        info.callReturnLocation = CodeLocationLabel(linkBuffer.locationOf(m_jsCalls[i].m_slowCall));
        info.hotPathBegin = linkBuffer.locationOf(m_jsCalls[i].m_targetToCheck);
        info.hotPathOther = linkBuffer.locationOfNearCall(m_jsCalls[i].m_fastCall);
    }
    
    m_codeBlock->addMethodCallLinkInfos(m_methodGets.size());
    for (unsigned i = 0; i < m_methodGets.size(); ++i) {
        MethodCallLinkInfo& info = m_codeBlock->methodCallLinkInfo(i);
        info.cachedStructure.setLocation(linkBuffer.locationOf(m_methodGets[i].m_structToCompare));
        info.cachedPrototypeStructure.setLocation(linkBuffer.locationOf(m_methodGets[i].m_protoStructToCompare));
        info.cachedFunction.setLocation(linkBuffer.locationOf(m_methodGets[i].m_putFunction));
        info.cachedPrototype.setLocation(linkBuffer.locationOf(m_methodGets[i].m_protoObj));
        info.callReturnLocation = linkBuffer.locationOf(m_methodGets[i].m_slowCall);
    }
}

void JITCompiler::compile(JITCode& entry)
{
    // Preserve the return address to the callframe.
    compileEntry();
    // Generate the body of the program.
    compileBody();
    // Link
    LinkBuffer linkBuffer(*m_globalData, this, m_globalData->executableAllocator);
    link(linkBuffer);
    entry = JITCode(linkBuffer.finalizeCode(), JITCode::DFGJIT);
}

void JITCompiler::compileFunction(JITCode& entry, MacroAssemblerCodePtr& entryWithArityCheck)
{
    compileEntry();

    // === Function header code generation ===
    // This is the main entry point, without performing an arity check.
    // If we needed to perform an arity check we will already have moved the return address,
    // so enter after this.
    Label fromArityCheck(this);
    // Setup a pointer to the codeblock in the CallFrameHeader.
    emitPutImmediateToCallFrameHeader(m_codeBlock, RegisterFile::CodeBlock);
    // Plant a check that sufficient space is available in the RegisterFile.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56291
    addPtr(Imm32(m_codeBlock->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister, GPRInfo::regT1);
    Jump registerFileCheck = branchPtr(Below, AbsoluteAddress(m_globalData->interpreter->registerFile().addressOfEnd()), GPRInfo::regT1);
    // Return here after register file check.
    Label fromRegisterFileCheck = label();


    // === Function body code generation ===
    compileBody();

    // === Function footer code generation ===
    //
    // Generate code to perform the slow register file check (if the fast one in
    // the function header fails), and generate the entry point with arity check.
    //
    // Generate the register file check; if the fast check in the function head fails,
    // we need to call out to a helper function to check whether more space is available.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    registerFileCheck.link(this);
    move(stackPointerRegister, GPRInfo::argumentGPR0);
    poke(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
    Call callRegisterFileCheck = call();
    jump(fromRegisterFileCheck);
    
    // The fast entry point into a function does not check the correct number of arguments
    // have been passed to the call (we only use the fast entry point where we can statically
    // determine the correct number of arguments have been passed, or have already checked).
    // In cases where an arity check is necessary, we enter here.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    Label arityCheck = label();
    preserveReturnAddressAfterCall(GPRInfo::regT2);
    emitPutToCallFrameHeader(GPRInfo::regT2, RegisterFile::ReturnPC);
    branch32(Equal, GPRInfo::regT1, Imm32(m_codeBlock->m_numParameters)).linkTo(fromArityCheck, this);
    move(stackPointerRegister, GPRInfo::argumentGPR0);
    poke(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
    Call callArityCheck = call();
    move(GPRInfo::regT0, GPRInfo::callFrameRegister);
    jump(fromArityCheck);


    // === Link ===
    LinkBuffer linkBuffer(*m_globalData, this, m_globalData->executableAllocator);
    link(linkBuffer);
    
    // FIXME: switch the register file check & arity check over to DFGOpertaion style calls, not JIT stubs.
    linkBuffer.link(callRegisterFileCheck, cti_register_file_check);
    linkBuffer.link(callArityCheck, m_codeBlock->m_isConstructor ? cti_op_construct_arityCheck : cti_op_call_arityCheck);

    entryWithArityCheck = linkBuffer.locationOf(arityCheck);
    entry = JITCode(linkBuffer.finalizeCode(), JITCode::DFGJIT);
}

#if DFG_JIT_ASSERT
void JITCompiler::jitAssertIsInt32(GPRReg gpr)
{
#if CPU(X86_64)
    Jump checkInt32 = branchPtr(BelowOrEqual, gpr, TrustedImmPtr(reinterpret_cast<void*>(static_cast<uintptr_t>(0xFFFFFFFFu))));
    breakpoint();
    checkInt32.link(this);
#else
    UNUSED_PARAM(gpr);
#endif
}

void JITCompiler::jitAssertIsJSInt32(GPRReg gpr)
{
    Jump checkJSInt32 = branchPtr(AboveOrEqual, gpr, GPRInfo::tagTypeNumberRegister);
    breakpoint();
    checkJSInt32.link(this);
}

void JITCompiler::jitAssertIsJSNumber(GPRReg gpr)
{
    Jump checkJSNumber = branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagTypeNumberRegister);
    breakpoint();
    checkJSNumber.link(this);
}

void JITCompiler::jitAssertIsJSDouble(GPRReg gpr)
{
    Jump checkJSInt32 = branchPtr(AboveOrEqual, gpr, GPRInfo::tagTypeNumberRegister);
    Jump checkJSNumber = branchTestPtr(MacroAssembler::NonZero, gpr, GPRInfo::tagTypeNumberRegister);
    checkJSInt32.link(this);
    breakpoint();
    checkJSNumber.link(this);
}

void JITCompiler::jitAssertIsCell(GPRReg gpr)
{
    Jump checkCell = branchTestPtr(MacroAssembler::Zero, gpr, GPRInfo::tagMaskRegister);
    breakpoint();
    checkCell.link(this);
}
#endif

#if ENABLE(SAMPLING_COUNTERS) && CPU(X86_64) // Or any other 64-bit platform!
void JITCompiler::emitCount(AbstractSamplingCounter& counter, uint32_t increment)
{
    addPtr(TrustedImm32(increment), AbsoluteAddress(counter.addressOfCounter()));
}
#endif

#if ENABLE(SAMPLING_COUNTERS) && CPU(X86) // Or any other little-endian 32-bit platform!
void JITCompiler::emitCount(AbstractSamplingCounter& counter, uint32_t increment)
{
    intptr_t hiWord = reinterpret_cast<intptr_t>(counter.addressOfCounter()) + sizeof(int32_t);
    add32(TrustedImm32(increment), AbsoluteAddress(counter.addressOfCounter()));
    addWithCarry32(TrustedImm32(0), AbsoluteAddress(reinterpret_cast<void*>(hiWord)));
}
#endif

#if ENABLE(SAMPLING_FLAGS)
void JITCompiler::setSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    or32(TrustedImm32(1u << (flag - 1)), AbsoluteAddress(SamplingFlags::addressOfFlags()));
}

void JITCompiler::clearSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    and32(TrustedImm32(~(1u << (flag - 1))), AbsoluteAddress(SamplingFlags::addressOfFlags()));
}
#endif

} } // namespace JSC::DFG

#endif
