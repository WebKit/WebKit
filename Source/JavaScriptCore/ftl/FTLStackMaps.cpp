/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#include "FTLLocation.h"
#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/ListDump.h>

namespace JSC { namespace FTL {

template<typename T>
T readObject(StackMaps::ParseContext& context)
{
    T result;
    result.parse(context);
    return result;
}

void StackMaps::Constant::parse(StackMaps::ParseContext& context)
{
    integer = context.view->read<int64_t>(context.offset, true);
}

void StackMaps::Constant::dump(PrintStream& out) const
{
    out.printf("0x%016llx", static_cast<unsigned long long>(integer));
}

void StackMaps::StackSize::parse(StackMaps::ParseContext& context)
{
    switch (context.version) {
    case 0:
        functionOffset = context.view->read<uint32_t>(context.offset, true);
        size = context.view->read<uint32_t>(context.offset, true);
        break;
        
    default:
        functionOffset = context.view->read<uint64_t>(context.offset, true);
        size = context.view->read<uint64_t>(context.offset, true);
        break;
    }
}

void StackMaps::StackSize::dump(PrintStream& out) const
{
    out.print("(off:", functionOffset, ", size:", size, ")");
}

void StackMaps::Location::parse(StackMaps::ParseContext& context)
{
    kind = static_cast<Kind>(context.view->read<uint8_t>(context.offset, true));
    size = context.view->read<uint8_t>(context.offset, true);
    dwarfReg = DWARFRegister(context.view->read<uint16_t>(context.offset, true));
    this->offset = context.view->read<int32_t>(context.offset, true);
}

void StackMaps::Location::dump(PrintStream& out) const
{
    out.print("(", kind, ", ", dwarfReg, ", off:", offset, ", size:", size, ")");
}

GPRReg StackMaps::Location::directGPR() const
{
    return FTL::Location::forStackmaps(nullptr, *this).directGPR();
}

void StackMaps::Location::restoreInto(
    MacroAssembler& jit, StackMaps& stackmaps, char* savedRegisters, GPRReg result) const
{
    FTL::Location::forStackmaps(&stackmaps, *this).restoreInto(jit, savedRegisters, result);
}

void StackMaps::LiveOut::parse(StackMaps::ParseContext& context)
{
    dwarfReg = DWARFRegister(context.view->read<uint16_t>(context.offset, true)); // regnum
    context.view->read<uint8_t>(context.offset, true); // reserved
    size = context.view->read<uint8_t>(context.offset, true); // size in bytes
}

void StackMaps::LiveOut::dump(PrintStream& out) const
{
    out.print("(", dwarfReg, ", ", size, ")");
}

bool StackMaps::Record::parse(StackMaps::ParseContext& context)
{
    int64_t id = context.view->read<int64_t>(context.offset, true);
    ASSERT(static_cast<int32_t>(id) == id);
    patchpointID = static_cast<uint32_t>(id);
    if (static_cast<int32_t>(patchpointID) < 0)
        return false;
    
    instructionOffset = context.view->read<uint32_t>(context.offset, true);
    flags = context.view->read<uint16_t>(context.offset, true);
    
    unsigned length = context.view->read<uint16_t>(context.offset, true);
    while (length--)
        locations.append(readObject<Location>(context));
    
    if (context.version >= 1)
        context.view->read<uint16_t>(context.offset, true); // padding

    unsigned numLiveOuts = context.view->read<uint16_t>(context.offset, true);
    while (numLiveOuts--)
        liveOuts.append(readObject<LiveOut>(context));

    if (context.version >= 1) {
        if (context.offset & 7) {
            ASSERT(!(context.offset & 3));
            context.view->read<uint32_t>(context.offset, true); // padding
        }
    }
    
    return true;
}

void StackMaps::Record::dump(PrintStream& out) const
{
    out.print(
        "(#", patchpointID, ", offset = ", instructionOffset, ", flags = ", flags,
        ", locations = [", listDump(locations), "], liveOuts = [",
        listDump(liveOuts), "])");
}

RegisterSet StackMaps::Record::locationSet() const
{
    RegisterSet result;
    for (unsigned i = locations.size(); i--;) {
        Reg reg = locations[i].dwarfReg.reg();
        if (!reg)
            continue;
        result.set(reg);
    }
    return result;
}

RegisterSet StackMaps::Record::liveOutsSet() const
{
    RegisterSet result;
    for (unsigned i = liveOuts.size(); i--;) {
        LiveOut liveOut = liveOuts[i];
        Reg reg = liveOut.dwarfReg.reg();
        // FIXME: Either assert that size is not greater than sizeof(pointer), or actually
        // save the high bits of registers.
        // https://bugs.webkit.org/show_bug.cgi?id=130885
        if (!reg) {
            dataLog("Invalid liveOuts entry in: ", *this, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        result.set(reg);
    }
    return result;
}

RegisterSet StackMaps::Record::usedRegisterSet() const
{
    RegisterSet result;
    result.merge(locationSet());
    result.merge(liveOutsSet());
    return result;
}

bool StackMaps::parse(DataView* view)
{
    ParseContext context;
    context.offset = 0;
    context.view = view;
    
    version = context.version = context.view->read<uint8_t>(context.offset, true);

    context.view->read<uint8_t>(context.offset, true); // Reserved
    context.view->read<uint8_t>(context.offset, true); // Reserved
    context.view->read<uint8_t>(context.offset, true); // Reserved

    uint32_t numFunctions;
    uint32_t numConstants;
    uint32_t numRecords;
    
    numFunctions = context.view->read<uint32_t>(context.offset, true);
    if (context.version >= 1) {
        numConstants = context.view->read<uint32_t>(context.offset, true);
        numRecords = context.view->read<uint32_t>(context.offset, true);
    }
    while (numFunctions--)
        stackSizes.append(readObject<StackSize>(context));
    
    if (!context.version)
        numConstants = context.view->read<uint32_t>(context.offset, true);
    while (numConstants--)
        constants.append(readObject<Constant>(context));
    
    if (!context.version)
        numRecords = context.view->read<uint32_t>(context.offset, true);
    while (numRecords--) {
        Record record;
        if (!record.parse(context))
            return false;
        records.append(record);
    }
    
    return true;
}

void StackMaps::dump(PrintStream& out) const
{
    out.print("Version:", version, ", StackSizes[", listDump(stackSizes), "], Constants:[", listDump(constants), "], Records:[", listDump(records), "]");
}

void StackMaps::dumpMultiline(PrintStream& out, const char* prefix) const
{
    out.print(prefix, "Version: ", version, "\n");
    out.print(prefix, "StackSizes:\n");
    for (unsigned i = 0; i < stackSizes.size(); ++i)
        out.print(prefix, "    ", stackSizes[i], "\n");
    out.print(prefix, "Constants:\n");
    for (unsigned i = 0; i < constants.size(); ++i)
        out.print(prefix, "    ", constants[i], "\n");
    out.print(prefix, "Records:\n");
    for (unsigned i = 0; i < records.size(); ++i)
        out.print(prefix, "    ", records[i], "\n");
}

StackMaps::RecordMap StackMaps::computeRecordMap() const
{
    RecordMap result;
    for (unsigned i = records.size(); i--;)
        result.add(records[i].patchpointID, Vector<Record>()).iterator->value.append(records[i]);
    return result;
}

unsigned StackMaps::stackSize() const
{
    RELEASE_ASSERT(stackSizes.size() == 1);

    return stackSizes[0].size;
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
    case StackMaps::Location::Direct:
        out.print("Direct");
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

