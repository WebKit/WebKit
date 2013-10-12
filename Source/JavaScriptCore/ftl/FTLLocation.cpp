/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLLocation.h"

#if ENABLE(FTL_JIT)

#include "FTLSaveRestore.h"
#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/ListDump.h>

namespace JSC { namespace FTL {

Location Location::forStackmaps(const StackMaps& stackmaps, const StackMaps::Location& location)
{
    switch (location.kind) {
    case StackMaps::Location::Unprocessed:
        return Location();
        
    case StackMaps::Location::Register:
        return forRegister(location.dwarfRegNum);
        
    case StackMaps::Location::Indirect:
        return forIndirect(location.dwarfRegNum, location.offset);
        
    case StackMaps::Location::Constant:
        return forConstant(location.offset);
        
    case StackMaps::Location::ConstantIndex:
        return forConstant(stackmaps.constants[location.offset].integer);
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void Location::dump(PrintStream& out) const
{
    out.print("(", kind());
    if (hasDwarfRegNum())
        out.print(", reg", dwarfRegNum());
    if (hasOffset())
        out.print(", ", offset());
    if (hasConstant())
        out.print(", ", constant());
    out.print(")");
}

bool Location::involvesGPR() const
{
    return isGPR() || kind() == Indirect;
}

#if CPU(X86_64) // CPU cases for Location methods
// This decodes Dwarf flavour 0 for x86-64.
bool Location::isGPR() const
{
    return kind() == Register && dwarfRegNum() < 16;
}

GPRReg Location::gpr() const
{
    // Stupidly, Dwarf doesn't number the registers in the same way as the architecture;
    // for example, the architecture encodes CX as 1 and DX as 2 while Dwarf does the
    // opposite. Hence we need the switch.
    
    ASSERT(involvesGPR());
    
    switch (dwarfRegNum()) {
    case 0:
        return X86Registers::eax;
    case 1:
        return X86Registers::edx;
    case 2:
        return X86Registers::ecx;
    case 3:
        return X86Registers::ebx;
    case 4:
        return X86Registers::esi;
    case 5:
        return X86Registers::edi;
    case 6:
        return X86Registers::ebp;
    case 7:
        return X86Registers::esp;
    default:
        RELEASE_ASSERT(dwarfRegNum() < 16);
        // Registers r8..r15 are numbered sensibly.
        return static_cast<GPRReg>(dwarfRegNum());
    }
}

bool Location::isFPR() const
{
    return kind() == Register && dwarfRegNum() >= 17 && dwarfRegNum() <= 32;
}

FPRReg Location::fpr() const
{
    ASSERT(isFPR());
    return static_cast<FPRReg>(dwarfRegNum() - 17);
}

void Location::restoreInto(MacroAssembler& jit, char* savedRegisters, GPRReg result) const
{
    if (isGPR()) {
        if (MacroAssembler::isStackRelated(gpr())) {
            // These don't get saved.
            jit.move(gpr(), result);
            return;
        }
        
        jit.load64(savedRegisters + offsetOfGPR(gpr()), result);
        return;
    }
    
    if (isFPR()) {
        jit.load64(savedRegisters + offsetOfFPR(fpr()), result);
        return;
    }
    
    switch (kind()) {
    case Register:
        // LLVM used some register that we don't know about!
        RELEASE_ASSERT_NOT_REACHED();
        return;
        
    case Indirect:
        if (MacroAssembler::isStackRelated(gpr())) {
            // These don't get saved.
            jit.load64(MacroAssembler::Address(gpr(), offset()), result);
            return;
        }
        
        jit.load64(savedRegisters + offsetOfGPR(gpr()), result);
        jit.load64(MacroAssembler::Address(result, offset()), result);
        return;
        
    case Constant:
        jit.move(MacroAssembler::TrustedImm64(constant()), result);
        return;
        
    case Unprocessed:
        // Should never see this - it's an enumeration entry on LLVM's side that means that
        // it hasn't processed this location.
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}
#else // CPU cases for Location methods
#error "CPU architecture not supported."
#endif // CPU cases for Location methods

} } // namespace JSC::FTL

namespace WTF {

using namespace JSC::FTL;

void printInternal(PrintStream& out, JSC::FTL::Location::Kind kind)
{
    switch (kind) {
    case Location::Unprocessed:
        out.print("Unprocessed");
        return;
    case Location::Register:
        out.print("Register");
        return;
    case Location::Indirect:
        out.print("Indirect");
        return;
    case Location::Constant:
        out.print("Constant");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(FTL_JIT)

