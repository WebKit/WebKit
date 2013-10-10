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
#include "FTLStackMaps.h"

#if ENABLE(FTL_JIT)

#include "FTLSaveRestore.h"
#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/ListDump.h>

namespace JSC { namespace FTL {

template<typename T>
T readObject(DataView* view, unsigned& offset)
{
    T result;
    result.parse(view, offset);
    return result;
}

void StackMaps::Constant::parse(DataView* view, unsigned& offset)
{
    integer = view->read<int64_t>(offset, true);
}

void StackMaps::Constant::dump(PrintStream& out) const
{
    out.printf("0x%016llx", integer);
}

void StackMaps::Location::parse(DataView* view, unsigned& offset)
{
    uint16_t taggedReg = view->read<uint16_t>(offset, true);
    if (static_cast<int16_t>(taggedReg) < 0) {
        dataLog(
            "Detected a negative tagged register ", static_cast<int16_t>(taggedReg),
            " at offset ", offset, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    dwarfRegNum = taggedReg & ((1 << 12) - 1);
    kind = static_cast<Kind>(taggedReg >> 12);

    this->offset = view->read<int16_t>(offset, true);
}

void StackMaps::Location::dump(PrintStream& out) const
{
    out.print("(", kind, ", reg", dwarfRegNum, ", ", offset, ")");
}

bool StackMaps::Location::involvesGPR() const
{
    return isGPR() || kind == Indirect;
}

#if CPU(X86_64) // CPU cases for StackMaps::Location
// This decodes Dwarf flavour 0 for x86-64.
bool StackMaps::Location::isGPR() const
{
    return kind == Register && dwarfRegNum < 16;
}

GPRReg StackMaps::Location::gpr() const
{
    // Stupidly, Dwarf doesn't number the registers in the same way as the architecture;
    // for example, the architecture encodes CX as 1 and DX as 2 while Dwarf does the
    // opposite. Hence we need the switch.
    
    switch (dwarfRegNum) {
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
        RELEASE_ASSERT(dwarfRegNum < 16);
        // Registers r8..r15 are numbered sensibly.
        return static_cast<GPRReg>(dwarfRegNum);
    }
}

void StackMaps::Location::restoreInto(
    MacroAssembler& jit, const StackMaps& stackmaps, char* savedRegisters, GPRReg result)
{
    if (isGPR()) {
        jit.load64(savedRegisters + offsetOfGPR(gpr()), result);
        return;
    }
    
    switch (kind) {
    case Register:
        // FIXME: We need to handle the other registers.
        // https://bugs.webkit.org/show_bug.cgi?id=122518
        RELEASE_ASSERT_NOT_REACHED();
        return;
        
    case Indirect:
        jit.load64(savedRegisters + offsetOfGPR(gpr()), result);
        jit.load64(MacroAssembler::Address(result, offset), result);
        return;
        
    case Constant:
        jit.move(MacroAssembler::TrustedImm32(offset), result);
        return;
        
    case ConstantIndex:
        jit.move(MacroAssembler::TrustedImm64(stackmaps.constants[offset].integer), result);
        return;
        
    case Unprocessed:
        // Should never see this - it's an enumeration entry on LLVM's side that means that
        // it hasn't processed this location.
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}
#else // CPU cases for StackMaps::Location
#error "CPU architecture not supported."
#endif // CPU cases for StackMaps::Location

bool StackMaps::Record::parse(DataView* view, unsigned& offset)
{
    patchpointID = view->read<uint32_t>(offset, true);
    if (static_cast<int32_t>(patchpointID) < 0)
        return false;
    
    instructionOffset = view->read<uint32_t>(offset, true);
    flags = view->read<uint16_t>(offset, true);
    
    unsigned length = view->read<uint16_t>(offset, true);
    while (length--)
        locations.append(readObject<Location>(view, offset));
    
    return true;
}

void StackMaps::Record::dump(PrintStream& out) const
{
    out.print(
        "(#", patchpointID, ", offset = ", instructionOffset, ", flags = ", flags,
        ", [", listDump(locations), "])");
}

bool StackMaps::parse(DataView* view)
{
    unsigned offset = 0;
    
    uint32_t numConstants = view->read<uint32_t>(offset, true);
    while (numConstants--)
        constants.append(readObject<Constant>(view, offset));
    
    uint32_t numRecords = view->read<uint32_t>(offset, true);
    while (numRecords--) {
        Record record;
        if (!record.parse(view, offset))
            return false;
        records.append(record);
    }
    
    return true;
}

void StackMaps::dump(PrintStream& out) const
{
    out.print("Constants:[", listDump(constants), "], Records:[", listDump(records), "]");
}

void StackMaps::dumpMultiline(PrintStream& out, const char* prefix) const
{
    out.print(prefix, "Constants:\n");
    for (unsigned i = 0; i < constants.size(); ++i)
        out.print(prefix, "    ", constants[i], "\n");
    out.print(prefix, "Records:\n");
    for (unsigned i = 0; i < records.size(); ++i)
        out.print(prefix, "    ", records[i], "\n");
}

StackMaps::RecordMap StackMaps::getRecordMap() const
{
    RecordMap result;
    for (unsigned i = records.size(); i--;)
        result.add(records[i].patchpointID, records[i]);
    return result;
}

} } // namespace JSC::FTL

namespace WTF {

using namespace JSC::FTL;

void printInternal(PrintStream& out, StackMaps::Location::Kind kind)
{
    switch (kind) {
    case StackMaps::Location::Unprocessed:
        out.print("Unprocessed");
        return;
    case StackMaps::Location::Register:
        out.print("Register");
        return;
    case StackMaps::Location::Indirect:
        out.print("Indirect");
        return;
    case StackMaps::Location::Constant:
        out.print("Constant");
        return;
    case StackMaps::Location::ConstantIndex:
        out.print("ConstantIndex");
        return;
    }
    dataLog("Unrecognized kind: ", static_cast<int>(kind), "\n");
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(FTL_JIT)

