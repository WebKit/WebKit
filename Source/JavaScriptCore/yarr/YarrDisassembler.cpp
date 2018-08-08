/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "YarrDisassembler.h"

#if ENABLE(JIT)

#include "Disassembler.h"
#include "LinkBuffer.h"
#include <wtf/StringPrintStream.h>

namespace JSC { namespace Yarr {

static constexpr char s_spaces[] = "                        ";
static constexpr unsigned s_maxIndent = sizeof(s_spaces) - 1;

const char* YarrDisassembler::indentString(unsigned level)
{
    unsigned indent = 6 + level * 2;
    indent = std::min(indent, s_maxIndent);

    return s_spaces + s_maxIndent - indent;
}

YarrDisassembler::YarrDisassembler(YarrJITInfo* yarrJITInfo)
    : m_jitInfo(yarrJITInfo)
    , m_labelForGenerateYarrOp(yarrJITInfo->opCount())
    , m_labelForBacktrackYarrOp(yarrJITInfo->opCount())
{
}

YarrDisassembler::~YarrDisassembler()
{
}

void YarrDisassembler::dump(PrintStream& out, LinkBuffer& linkBuffer)
{
    dumpHeader(out, linkBuffer);
    dumpDisassembly(out, indentString(), linkBuffer, m_startOfCode, m_labelForGenerateYarrOp[0]);

    out.print("     == Matching ==\n");
    dumpForInstructions(out, linkBuffer, m_labelForGenerateYarrOp, m_endOfGenerate);
    out.print("     == Backtracking ==\n");
    dumpForInstructions(out, linkBuffer, m_labelForBacktrackYarrOp, m_endOfBacktrack, VectorOrder::IterateReverse);

    if (!(m_endOfBacktrack == m_endOfCode)) {
        out.print("     == Helpers ==\n");

        dumpDisassembly(out, indentString(), linkBuffer, m_endOfBacktrack, m_endOfCode);
    }

    linkBuffer.didAlreadyDisassemble();
}

void YarrDisassembler::dump(LinkBuffer& linkBuffer)
{
    dump(WTF::dataFile(), linkBuffer);
}

void YarrDisassembler::dumpHeader(PrintStream& out, LinkBuffer& linkBuffer)
{
    out.print("Generated JIT code for ", m_jitInfo->variant(), " ");
    m_jitInfo->dumpPatternString(out);
    out.print(":\n");
    out.print("    Code at [", RawPointer(linkBuffer.debugAddress()), ", ", RawPointer(static_cast<char*>(linkBuffer.debugAddress()) + linkBuffer.size()), "):\n");
}

Vector<YarrDisassembler::DumpedOp> YarrDisassembler::dumpVectorForInstructions(LinkBuffer& linkBuffer, Vector<MacroAssembler::Label>& labels, MacroAssembler::Label endLabel, YarrDisassembler::VectorOrder vectorOrder)
{
    StringPrintStream out;
    Vector<DumpedOp> result;

    unsigned directionBias = (vectorOrder == VectorOrder::IterateForward) ? 0 : labels.size() - 1;

    auto realIndex = [&](unsigned rawIndex) {
        if (directionBias)
            return directionBias - rawIndex;
        return rawIndex;
    };

    for (unsigned i = 0; i < labels.size();) {
        if (!labels[realIndex(i)].isSet()) {
            i++;
            continue;
        }
        out.reset();
        result.append(DumpedOp());
        result.last().index = realIndex(i);

        int delta = m_jitInfo->dumpFor(out, realIndex(i));
        m_indentLevel += (vectorOrder == VectorOrder::IterateForward) ? delta : -delta;

        for (unsigned nextIndex = i + 1; ; nextIndex++) {
            if (nextIndex >= labels.size()) {
                dumpDisassembly(out, indentString(), linkBuffer, labels[realIndex(i)], endLabel);
                result.last().disassembly = out.toCString();
                return result;
            }
            if (labels[realIndex(nextIndex)].isSet()) {
                dumpDisassembly(out, indentString(), linkBuffer, labels[realIndex(i)], labels[realIndex(nextIndex)]);
                result.last().disassembly = out.toCString();
                i = nextIndex;
                break;
            }
        }
    }

    return result;
}

void YarrDisassembler::dumpForInstructions(PrintStream& out, LinkBuffer& linkBuffer, Vector<MacroAssembler::Label>& labels, MacroAssembler::Label endLabel, YarrDisassembler::VectorOrder vectorOrder)
{
    Vector<DumpedOp> dumpedOps = dumpVectorForInstructions(linkBuffer, labels, endLabel, vectorOrder);

    for (unsigned i = 0; i < dumpedOps.size(); ++i)
        out.print(dumpedOps[i].disassembly);
}

void YarrDisassembler::dumpDisassembly(PrintStream& out, const char* prefix, LinkBuffer& linkBuffer, MacroAssembler::Label from, MacroAssembler::Label to)
{
    CodeLocationLabel<DisassemblyPtrTag> fromLocation = linkBuffer.locationOf<DisassemblyPtrTag>(from);
    CodeLocationLabel<DisassemblyPtrTag> toLocation = linkBuffer.locationOf<DisassemblyPtrTag>(to);
    disassemble(fromLocation, toLocation.dataLocation<uintptr_t>() - fromLocation.dataLocation<uintptr_t>(), prefix, out);
}

}} // namespace Yarr namespace JSC

#endif // ENABLE(JIT)

