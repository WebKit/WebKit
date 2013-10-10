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

#include "FTLLocation.h"
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

void StackMaps::Location::restoreInto(
    MacroAssembler& jit, StackMaps& stackmaps, char* savedRegisters, GPRReg result) const
{
    FTL::Location::forStackmaps(stackmaps, *this).restoreInto(jit, savedRegisters, result);
}

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

