/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WasmBBQDisassembler.h"

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)

#include "Disassembler.h"
#include "LinkBuffer.h"
#include <wtf/HexNumber.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BBQDisassembler);

BBQDisassembler::BBQDisassembler() = default;

BBQDisassembler::~BBQDisassembler() = default;

void BBQDisassembler::dump(PrintStream& out, LinkBuffer& linkBuffer)
{
    m_codeStart = linkBuffer.entrypoint<DisassemblyPtrTag>().untaggedPtr();
    m_codeEnd = bitwise_cast<uint8_t*>(m_codeStart) + linkBuffer.size();

    dumpHeader(out, linkBuffer);
    if (m_labels.isEmpty())
        dumpDisassembly(out, linkBuffer, m_startOfCode, m_endOfCode);
    else {
        dumpDisassembly(out, linkBuffer, m_startOfCode, std::get<0>(m_labels[0]));
        dumpForInstructions(out, linkBuffer, "    ", m_labels, m_endOfOpcode);
        out.print("    (End Of Main Code)\n");
        dumpDisassembly(out, linkBuffer, m_endOfOpcode, m_endOfCode);
    }
}

void BBQDisassembler::dump(LinkBuffer& linkBuffer)
{
    dump(WTF::dataFile(), linkBuffer);
}

void BBQDisassembler::dumpHeader(PrintStream& out, LinkBuffer& linkBuffer)
{
    out.print("   Code at [", RawPointer(linkBuffer.debugAddress()), ", ", RawPointer(static_cast<char*>(linkBuffer.debugAddress()) + linkBuffer.size()), "):\n");
}

Vector<BBQDisassembler::DumpedOp> BBQDisassembler::dumpVectorForInstructions(LinkBuffer& linkBuffer, const char* prefix, Vector<std::tuple<MacroAssembler::Label, OpType, size_t>>& labels, MacroAssembler::Label endLabel)
{
    StringPrintStream out;
    Vector<DumpedOp> result;

    for (unsigned i = 0; i < labels.size();) {
        out.reset();
        auto opcode = std::get<1>(labels[i]);
        auto offset = std::get<2>(labels[i]);
        result.append(DumpedOp { { } });
        out.print(prefix);
        out.println("[", makeString(pad(' ', 8, makeString("0x", hex(offset, 0, Lowercase)))), "] ", makeString(opcode));
        unsigned nextIndex = i + 1;
        if (nextIndex >= labels.size()) {
            dumpDisassembly(out, linkBuffer, std::get<0>(labels[i]), endLabel);
            result.last().disassembly = out.toCString();
            return result;
        }
        dumpDisassembly(out, linkBuffer, std::get<0>(labels[i]), std::get<0>(labels[nextIndex]));
        result.last().disassembly = out.toCString();
        i = nextIndex;
    }

    return result;
}

void BBQDisassembler::dumpForInstructions(PrintStream& out, LinkBuffer& linkBuffer, const char* prefix, Vector<std::tuple<MacroAssembler::Label, OpType, size_t>>& labels, MacroAssembler::Label endLabel)
{
    Vector<DumpedOp> dumpedOps = dumpVectorForInstructions(linkBuffer, prefix, labels, endLabel);

    for (unsigned i = 0; i < dumpedOps.size(); ++i)
        out.print(dumpedOps[i].disassembly);
}

void BBQDisassembler::dumpDisassembly(PrintStream& out, LinkBuffer& linkBuffer, MacroAssembler::Label from, MacroAssembler::Label to)
{
    CodeLocationLabel<DisassemblyPtrTag> fromLocation = linkBuffer.locationOf<DisassemblyPtrTag>(from);
    CodeLocationLabel<DisassemblyPtrTag> toLocation = linkBuffer.locationOf<DisassemblyPtrTag>(to);
    disassemble(fromLocation, toLocation.dataLocation<uintptr_t>() - fromLocation.dataLocation<uintptr_t>(), m_codeStart, m_codeEnd, "        ", out);
}

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
