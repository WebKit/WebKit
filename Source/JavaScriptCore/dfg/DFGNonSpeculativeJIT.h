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

#ifndef DFGNonSpeculativeJIT_h
#define DFGNonSpeculativeJIT_h

#if ENABLE(DFG_JIT)

#include <dfg/DFGJITCodeGenerator.h>

namespace JSC { namespace DFG {

class SpeculationCheckIndexIterator;

// === EntryLocation ===
//
// This structure describes an entry point into the non-speculative
// code path. This is used in linking bail-outs from the speculative path.
struct EntryLocation {
    EntryLocation(MacroAssembler::Label, NonSpeculativeJIT*);

    // The node this entry point corresponds to, and the label
    // marking the start of code for the given node.
    MacroAssembler::Label m_entry;
    NodeIndex m_nodeIndex;

    // For every entry point we record a map recording for every
    // machine register which, if any, values it contains. For
    // GPR registers we must also record the format of the value.
    struct RegisterInfo {
        NodeIndex nodeIndex;
        DataFormat format;
    };
    RegisterInfo m_gprInfo[numberOfGPRs];
    NodeIndex m_fprInfo[numberOfFPRs];
};

// === NonSpeculativeJIT ===
//
// This class is used to generate code for the non-speculative path.
// Code generation will take advantage of static information avaialble
// in the dataflow to perform safe optimizations - for example, avoiding
// boxing numeric values between arithmetic operations, but will not
// perform any unsafe optimizations that would render the code unable
// to produce the correct results for any possible input.
class NonSpeculativeJIT : public JITCodeGenerator {
    friend struct EntryLocation;
public:
    NonSpeculativeJIT(JITCompiler& jit)
        : JITCodeGenerator(jit, false)
    {
    }

    void compile(SpeculationCheckIndexIterator&);

    typedef SegmentedVector<EntryLocation, 16> EntryLocationVector;
    EntryLocationVector& entryLocations() { return m_entryLocations; }

private:
    // These methods are used when generating 'unexpected'
    // calls out from JIT code to C++ helper routines -
    // they spill all live values to the appropriate
    // slots in the RegisterFile without changing any state
    // in the GenerationInfo.
    void silentSpill(VirtualRegister spillMe, GPRReg canTrample)
    {
        GenerationInfo& info = m_generationInfo[spillMe];
        ASSERT(info.registerFormat() != DataFormatNone);
        if (info.needsSpill()) {
            DataFormat spillFormat = info.registerFormat();

            if (spillFormat == DataFormatDouble) {
                boxDouble(info.fpr(), canTrample);
                m_jit.storePtr(JITCompiler::gprToRegisterID(canTrample), JITCompiler::addressFor(spillMe));
            } else {
                JITCompiler::RegisterID reg = JITCompiler::gprToRegisterID(info.gpr());

                if (spillFormat == DataFormatInteger) {
                    m_jit.orPtr(JITCompiler::tagTypeNumberRegister, reg);
                    m_jit.storePtr(reg, JITCompiler::addressFor(spillMe));
                } else {
                    ASSERT(spillFormat & DataFormatJS || spillFormat == DataFormatCell);
                    m_jit.storePtr(reg, JITCompiler::addressFor(spillMe));
                }
            }
        }
    }
    void silentFill(VirtualRegister spillMe, GPRReg canTrample)
    {
        GenerationInfo& info = m_generationInfo[spillMe];
        NodeIndex nodeIndex = info.nodeIndex();
        Node& node = m_jit.graph()[nodeIndex];
        ASSERT(info.registerFormat() != DataFormatNone);
        DataFormat spillFormat = info.registerFormat();

        if (node.isConstant()) {
            JITCompiler::RegisterID reg = JITCompiler::gprToRegisterID(info.gpr());
            m_jit.move(constantAsJSValueAsImmPtr(nodeIndex), reg);
        } else if (node.isArgument()) {
            JITCompiler::RegisterID reg = JITCompiler::gprToRegisterID(info.gpr());
            m_jit.loadPtr(m_jit.addressForArgument(m_jit.graph()[nodeIndex].argumentNumber()), reg);
        } else if (spillFormat == DataFormatDouble) {
            m_jit.loadPtr(JITCompiler::addressFor(spillMe), JITCompiler::gprToRegisterID(canTrample));
            unboxDouble(canTrample, info.fpr());
        } else if (spillFormat == DataFormatInteger) {
            JITCompiler::RegisterID reg = JITCompiler::gprToRegisterID(info.gpr());
            m_jit.load32(JITCompiler::addressFor(spillMe), reg);
        } else {
            ASSERT(spillFormat & DataFormatJS || spillFormat == DataFormatCell);
            JITCompiler::RegisterID reg = JITCompiler::gprToRegisterID(info.gpr());
            m_jit.loadPtr(JITCompiler::addressFor(spillMe), reg);
        }
    }
    void silentSpillAllRegisters(GPRReg dontTrample)
    {
        GPRReg canTrample = (dontTrample == gpr0) ? gpr1 : gpr0;
        
        for (GPRReg gpr = gpr0; gpr < numberOfGPRs; next(gpr)) {
            VirtualRegister name = m_gprs.name(gpr);
            if (name != InvalidVirtualRegister)
                silentSpill(name, canTrample);
        }
        for (FPRReg fpr = fpr0; fpr < numberOfFPRs; next(fpr)) {
            VirtualRegister name = m_fprs.name(fpr);
            if (name != InvalidVirtualRegister)
                silentSpill(name, canTrample);
        }
    }
    void silentFillAllRegisters(GPRReg dontTrample)
    {
        GPRReg canTrample = (dontTrample == gpr0) ? gpr1 : gpr0;
        
        for (FPRReg fpr = fpr0; fpr < numberOfFPRs; next(fpr)) {
            VirtualRegister name = m_fprs.name(fpr);
            if (name != InvalidVirtualRegister)
                silentFill(name, canTrample);
        }
        for (GPRReg gpr = gpr0; gpr < numberOfGPRs; next(gpr)) {
            VirtualRegister name = m_gprs.name(gpr);
            if (name != InvalidVirtualRegister)
                silentFill(name, canTrample);
        }
    }
    void silentFillAllRegisters(FPRReg dontTrample)
    {
        GPRReg canTrample = gpr0;
        
        for (FPRReg fpr = fpr0; fpr < numberOfFPRs; next(fpr)) {
            VirtualRegister name = m_fprs.name(fpr);
            if (name != InvalidVirtualRegister) {
#ifndef NDEBUG
                ASSERT(fpr != dontTrample);
#else
                UNUSED_PARAM(dontTrample);
#endif
                silentFill(name, canTrample);
            }
        }
        for (GPRReg gpr = gpr0; gpr < numberOfGPRs; next(gpr)) {
            VirtualRegister name = m_gprs.name(gpr);
            if (name != InvalidVirtualRegister)
                silentFill(name, canTrample);
        }
    }

    // These methods are used to plant calls out to C++
    // helper routines to convert between types.
    void valueToNumber(JSValueOperand&, FPRReg result);
    void valueToInt32(JSValueOperand&, GPRReg result);
    void numberToInt32(FPRReg, GPRReg result);

    // Record an entry location into the non-speculative code path;
    // for every bail-out on the speculative path we record information
    // to be able to re-enter into the non-speculative one.
    void trackEntry(MacroAssembler::Label entry)
    {
        m_entryLocations.append(EntryLocation(entry, this));
    }

    EntryLocationVector m_entryLocations;
};

} } // namespace JSC::DFG

#endif
#endif

