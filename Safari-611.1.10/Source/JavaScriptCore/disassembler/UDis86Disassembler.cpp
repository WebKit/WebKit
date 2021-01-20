/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "UDis86Disassembler.h"

#if ENABLE(UDIS86)

#include "MacroAssemblerCodeRef.h"
#include "udis86.h"

namespace JSC {

bool tryToDisassembleWithUDis86(const MacroAssemblerCodePtr<DisassemblyPtrTag>& codePtr, size_t size, const char* prefix, PrintStream& out)
{
    ud_t disassembler;
    ud_init(&disassembler);
    ud_set_input_buffer(&disassembler, codePtr.untaggedExecutableAddress<unsigned char*>(), size);
#if CPU(X86_64)
    ud_set_mode(&disassembler, 64);
#else
    ud_set_mode(&disassembler, 32);
#endif
    ud_set_pc(&disassembler, codePtr.untaggedExecutableAddress<uintptr_t>());
    ud_set_syntax(&disassembler, UD_SYN_ATT);
    
    uint64_t currentPC = disassembler.pc;
    while (ud_disassemble(&disassembler)) {
        char pcString[20];
        snprintf(pcString, sizeof(pcString), "0x%" PRIxPTR, static_cast<uintptr_t>(currentPC));
        out.printf("%s%16s: %s\n", prefix, pcString, ud_insn_asm(&disassembler));
        currentPC = disassembler.pc;
    }
    
    return true;
}

} // namespace JSC

#endif // ENABLE(UDIS86)

