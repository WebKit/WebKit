/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#if ENABLE(DISASSEMBLER) && USE(CAPSTONE)

#include "MacroAssemblerCodeRef.h"
#include "Options.h"
#include <capstone/capstone.h>

namespace JSC {

bool tryToDisassemble(const MacroAssemblerCodePtr<DisassemblyPtrTag>& codePtr, size_t size, const char* prefix, PrintStream& out)
{
    csh handle;
    cs_insn* instructions;

#if CPU(X86)
    if (cs_open(CS_ARCH_X86, CS_MODE_32, &handle) != CS_ERR_OK)
        return false;
#elif CPU(X86_64)
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
        return false;
#elif CPU(ARM_THUMB2)
    if (cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &handle) != CS_ERR_OK)
        return false;
#elif CPU(ARM64)
    if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &handle) != CS_ERR_OK)
        return false;
#elif CPU(MIPS)
    if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS32, &handle) != CS_ERR_OK)
        return false;
#else
    return false;
#endif

#if CPU(X86) || CPU(X86_64)
    if (cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_ATT) != CS_ERR_OK) {
        cs_close(&handle);
        return false;
    }
#endif

    size_t count = cs_disasm(handle, codePtr.dataLocation<unsigned char*>(), size, codePtr.dataLocation<uintptr_t>(), 0, &instructions);
    if (count > 0) {
        for (size_t i = 0; i < count; ++i) {
            auto& instruction = instructions[i];
            char pcString[20];
            snprintf(pcString, sizeof(pcString), "0x%llx", static_cast<unsigned long long>(instruction.address));
            out.printf("%s%16s: %s %s\n", prefix, pcString, instruction.mnemonic, instruction.op_str);
        }
        cs_free(instructions, count);
    }
    cs_close(&handle);
    return true;
}

} // namespace JSC

#endif // ENABLE(DISASSEMBLER) && USE(CAPSTONE)
