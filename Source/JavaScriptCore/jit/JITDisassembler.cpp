/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "JITDisassembler.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "JIT.h"

namespace JSC {

JITDisassembler::JITDisassembler(CodeBlock *codeBlock)
    : m_codeBlock(codeBlock)
    , m_labelForBytecodeIndexInMainPath(codeBlock->instructionCount())
    , m_labelForBytecodeIndexInSlowPath(codeBlock->instructionCount())
{
}

JITDisassembler::~JITDisassembler()
{
}

void JITDisassembler::dump(LinkBuffer& linkBuffer)
{
    dataLog("Baseline JIT code for CodeBlock %p, instruction count = %u:\n", m_codeBlock, m_codeBlock->instructionCount());
    dataLog("    Code at [%p, %p):\n", linkBuffer.debugAddress(), static_cast<char*>(linkBuffer.debugAddress()) + linkBuffer.debugSize());
    dumpDisassembly(linkBuffer, m_startOfCode, m_labelForBytecodeIndexInMainPath[0]);
    
    MacroAssembler::Label firstSlowLabel;
    for (unsigned i = 0; i < m_labelForBytecodeIndexInSlowPath.size(); ++i) {
        if (m_labelForBytecodeIndexInSlowPath[i].isSet()) {
            firstSlowLabel = m_labelForBytecodeIndexInSlowPath[i];
            break;
        }
    }
    dumpForInstructions(linkBuffer, "    ", m_labelForBytecodeIndexInMainPath, firstSlowLabel.isSet() ? firstSlowLabel : m_endOfSlowPath);
    dataLog("    (End Of Main Path)\n");
    dumpForInstructions(linkBuffer, "    (S) ", m_labelForBytecodeIndexInSlowPath, m_endOfSlowPath);
    dataLog("    (End Of Slow Path)\n");

    dumpDisassembly(linkBuffer, m_endOfSlowPath, m_endOfCode);
}

void JITDisassembler::dumpForInstructions(LinkBuffer& linkBuffer, const char* prefix, Vector<MacroAssembler::Label>& labels, MacroAssembler::Label endLabel)
{
    for (unsigned i = 0 ; i < labels.size();) {
        if (!labels[i].isSet()) {
            i++;
            continue;
        }
        dataLog("%s", prefix);
        m_codeBlock->dump(i);
        for (unsigned nextIndex = i + 1; ; nextIndex++) {
            if (nextIndex >= labels.size()) {
                dumpDisassembly(linkBuffer, labels[i], endLabel);
                return;
            }
            if (labels[nextIndex].isSet()) {
                dumpDisassembly(linkBuffer, labels[i], labels[nextIndex]);
                i = nextIndex;
                break;
            }
        }
    }
}

void JITDisassembler::dumpDisassembly(LinkBuffer& linkBuffer, MacroAssembler::Label from, MacroAssembler::Label to)
{
    CodeLocationLabel fromLocation = linkBuffer.locationOf(from);
    CodeLocationLabel toLocation = linkBuffer.locationOf(to);
    if (tryToDisassemble(fromLocation, bitwise_cast<uintptr_t>(toLocation.executableAddress()) - bitwise_cast<uintptr_t>(fromLocation.executableAddress()), "        ", WTF::dataFile()))
        return;
    
    dataLog("        disassembly not available for range %p...%p\n", fromLocation.executableAddress(), toLocation.executableAddress());
}

} // namespace JSC

#endif // ENABLE(JIT)

