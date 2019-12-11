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

#pragma once

#if ENABLE(JIT)

#include "MacroAssembler.h"
#include <wtf/Vector.h>
#include <wtf/text/CString.h>


namespace JSC {

class LinkBuffer;

namespace Yarr {

class YarrCodeBlock;

class YarrJITInfo {
public:
    virtual ~YarrJITInfo() { };
    virtual const char* variant() = 0;
    virtual unsigned opCount() = 0;
    virtual void dumpPatternString(PrintStream&) = 0;
    virtual int dumpFor(PrintStream&, unsigned) = 0;
};

class YarrDisassembler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    YarrDisassembler(YarrJITInfo*);
    ~YarrDisassembler();

    void setStartOfCode(MacroAssembler::Label label) { m_startOfCode = label; }
    void setForGenerate(unsigned opIndex, MacroAssembler::Label label)
    {
        m_labelForGenerateYarrOp[opIndex] = label;
    }

    void setForBacktrack(unsigned opIndex, MacroAssembler::Label label)
    {
        m_labelForBacktrackYarrOp[opIndex] = label;
    }

    void setEndOfGenerate(MacroAssembler::Label label) { m_endOfGenerate = label; }
    void setEndOfBacktrack(MacroAssembler::Label label) { m_endOfBacktrack = label; }
    void setEndOfCode(MacroAssembler::Label label) { m_endOfCode = label; }

    void dump(LinkBuffer&);
    void dump(PrintStream&, LinkBuffer&);

private:
    enum class VectorOrder {
        IterateForward,
        IterateReverse
    };

    void dumpHeader(PrintStream&, LinkBuffer&);
    MacroAssembler::Label firstSlowLabel();

    struct DumpedOp {
        unsigned index;
        CString disassembly;
    };

    const char* indentString(unsigned);
    const char* indentString()
    {
        return indentString(m_indentLevel);
    }

    Vector<DumpedOp> dumpVectorForInstructions(LinkBuffer&, Vector<MacroAssembler::Label>& labels, MacroAssembler::Label endLabel, YarrDisassembler::VectorOrder vectorOrder = VectorOrder::IterateForward);

    void dumpForInstructions(PrintStream&, LinkBuffer&, Vector<MacroAssembler::Label>& labels, MacroAssembler::Label endLabel, YarrDisassembler::VectorOrder vectorOrder = VectorOrder::IterateForward);

    void dumpDisassembly(PrintStream&, const char* prefix, LinkBuffer&, MacroAssembler::Label from, MacroAssembler::Label to);

    YarrJITInfo* m_jitInfo;
    MacroAssembler::Label m_startOfCode;
    Vector<MacroAssembler::Label> m_labelForGenerateYarrOp;
    Vector<MacroAssembler::Label> m_labelForBacktrackYarrOp;
    MacroAssembler::Label m_endOfGenerate;
    MacroAssembler::Label m_endOfBacktrack;
    MacroAssembler::Label m_endOfCode;
    unsigned m_indentLevel { 0 };
};

}} // namespace Yarr namespace JSC

#endif // ENABLE(JIT)
