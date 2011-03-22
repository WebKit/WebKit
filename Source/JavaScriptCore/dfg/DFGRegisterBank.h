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

#ifndef DFGRegisterBank_h
#define DFGRegisterBank_h

#if ENABLE(DFG_JIT)

#include <dfg/DFGJITCompiler.h>

namespace JSC { namespace DFG {

// === RegisterBank ===
//
// This class is used to implement the GPR and FPR register banks.
// All registers have two pieces of state associated with them:
// a lock count (used to indicate this register is already in use
// in code generation of the current node, and cannot be spilled or
// allocated as a temporary), and VirtualRegister 'name', recording
// which value (if any) a machine register currently holds.
// Either or both of these pieces of information may be valid for a
// given register. A register may be:
//
//  - unlocked, and unnamed: Available for allocation.
//  - locked, but unnamed:   Already allocated as a temporary or
//                           result for the current node.
//  - unlocked, but named:   Contains the result of a prior operation,
//                           not yet in use for this node,
//  - locked, but named:     Contains the result of a prior operation,
//                           already allocated as a operand to the
//                           current operation.
//
// For every named register we also record a hint value indicating
// the order in which registers should be selected to be spilled;
// registers that can be more cheaply spilled and/or filled should
// be selected first.
//
// Locking register is a strong retention mechanism; a locked register
// will never be reallocated (this is used to ensure the operands to
// the current node are in registers). Naming, conversely, in a weak
// retention mechanism - allocating a register may force a named value
// to be spilled.
//
// All named values must be given a hint that is greater than Min and
// less than Max.
template<typename RegID, size_t NUM_REGS, typename SpillHint, SpillHint SpillHintMin, SpillHint SpillHintMax>
class RegisterBank {
public:
    RegisterBank()
        : m_lastAllocated(NUM_REGS - 1)
    {
    }

    // Allocate a register - this function finds an unlocked register,
    // locks it, and returns it. If any named registers exist, one
    // of these should be selected to be allocated. If all unlocked
    // registers are named, then one of the named registers will need
    // to be spilled. In this case the register selected to be spilled
    // will be one of the registers that has the lowest 'spillOrder'
    // cost associated with it.
    //
    // This method select the register to be allocated, and calls the
    // private 'allocateInternal' method to update internal data
    // structures accordingly.
    RegID allocate(VirtualRegister &spillMe)
    {
        uint32_t currentLowest = NUM_REGS;
        SpillHint currentSpillOrder = SpillHintMax;

        // Scan through all register, starting at the last allocated & looping around.
        ASSERT(m_lastAllocated < NUM_REGS);

        // This loop is broken into two halves, looping from the last allocated
        // register (the register returned last time this method was called) to
        // the maximum register value, then from 0 to the last allocated.
        // This implements a simple round-robin like approach to try to reduce
        // thrash, and minimize time spent scanning locked registers in allocation.
        // If a unlocked and unnamed register is found return it immediately.
        // Otherwise, find the first unlocked register with the lowest spillOrder.
        for (uint32_t i = m_lastAllocated + 1; i < NUM_REGS; ++i) {
            // (1) If the current register is locked, it is not a candidate.
            if (m_data[i].lockCount)
                continue;
            // (2) If the current register's spill order is 0, pick this! – unassigned registers have spill order 0.
            SpillHint spillOrder = m_data[i].spillOrder;
            if (!spillOrder)
                return allocateInternal(i, spillMe);
            // If this register is better (has a lower spill order value) than any prior
            // candidate, then record it.
            if (spillOrder < currentSpillOrder) {
                currentSpillOrder = spillOrder;
                currentLowest = i;
            }
        }
        // Loop over the remaining entries.
        for (uint32_t i = 0; i <= m_lastAllocated; ++i) {
            if (m_data[i].lockCount)
                continue;
            SpillHint spillOrder = m_data[i].spillOrder;
            if (!spillOrder)
                return allocateInternal(i, spillMe);
            if (spillOrder < currentSpillOrder) {
                currentSpillOrder = spillOrder;
                currentLowest = i;
            }
        }

        // Deadlock check - this could only occur is all registers are locked!
        ASSERT(currentLowest != NUM_REGS && currentSpillOrder != SpillHintMax);
        // There were no available registers; currentLowest will need to be spilled.
        return allocateInternal(currentLowest, spillMe);
    }

    // retain/release - these methods are used to associate/disassociate names
    // with values in registers. retain should only be called on locked registers.
    void retain(RegID reg, VirtualRegister name, SpillHint spillOrder)
    {
        // 'reg' must be a valid, locked register.
        ASSERT(reg < NUM_REGS);
        ASSERT(m_data[reg].lockCount);
        // 'reg' should not currently be named, the new name must be valid.
        ASSERT(m_data[reg].name == InvalidVirtualRegister);
        ASSERT(name != InvalidVirtualRegister);
        // 'reg' should not currently have a spillOrder, the new spill order must be valid.
        ASSERT(spillOrder && spillOrder < SpillHintMax);
        ASSERT(m_data[reg].spillOrder == SpillHintMin);

        m_data[reg].name = name;
        m_data[reg].spillOrder = spillOrder;
    }
    void release(RegID reg)
    {
        // 'reg' must be a valid register.
        ASSERT(reg < NUM_REGS);
        // 'reg' should currently be named.
        ASSERT(m_data[reg].name != InvalidVirtualRegister);
        // 'reg' should currently have a valid spill order.
        ASSERT(m_data[reg].spillOrder > SpillHintMin && m_data[reg].spillOrder < SpillHintMax);

        m_data[reg].name = InvalidVirtualRegister;
        m_data[reg].spillOrder = SpillHintMin;
    }

    // lock/unlock register, ensures that they are not spilled.
    void lock(RegID reg)
    {
        ASSERT(reg < NUM_REGS);
        ++m_data[reg].lockCount;
        ASSERT(m_data[reg].lockCount);
    }
    void unlock(RegID reg)
    {
        ASSERT(reg < NUM_REGS);
        ASSERT(m_data[reg].lockCount);
        --m_data[reg].lockCount;
    }
    bool isLocked(RegID reg)
    {
        ASSERT(reg < NUM_REGS);
        return m_data[reg].lockCount;
    }

    // Get the name (VirtualRegister) associated with the
    // given register (or InvalidVirtualRegister for none).
    VirtualRegister name(RegID reg)
    {
        ASSERT(reg < NUM_REGS);
        return m_data[reg].name;
    }

#ifndef NDEBUG
    void dump()
    {
        // For each register, print the VirtualRegister 'name'.
        for (uint32_t i =0; i < NUM_REGS; ++i) {
            if (m_data[i].name != InvalidVirtualRegister)
                fprintf(stderr, "[%02d]", m_data[i].name);
            else
                fprintf(stderr, "[--]");
        }
        fprintf(stderr, "\n");
    }
#endif

private:
    // Used by 'allocate', above, to update inforamtion in the map.
    RegID allocateInternal(uint32_t i, VirtualRegister &spillMe)
    {
        // 'i' must be a valid, unlocked register.
        ASSERT(i < NUM_REGS && !m_data[i].lockCount);

        // Return the VirtualRegister of the named value currently stored in
        // the register being returned - or InvalidVirtualRegister if none.
        spillMe = m_data[i].name;

        // Clear any name/spillOrder currently associated with the register,
        m_data[i] = MapEntry();
        m_data[i].lockCount = 1;
        // Mark the register as locked (with a lock count of 1).
        m_lastAllocated = i;
        return (RegID)i;
    }

    // === MapEntry ===
    //
    // This structure provides information for an individual machine register
    // being managed by the RegisterBank. For each register we track a lock
    // count, name and spillOrder hint.
    struct MapEntry {
        MapEntry()
            : name(InvalidVirtualRegister)
            , spillOrder(SpillHintMin)
            , lockCount(0)
        {
        }

        VirtualRegister name;
        SpillHint spillOrder;
        uint32_t lockCount;
    };

    // Holds the current status of all registers.
    MapEntry m_data[NUM_REGS];
    // Used to to implement a simple round-robin like allocation scheme.
    uint32_t m_lastAllocated;
};

} } // namespace JSC::DFG

#endif
#endif
