/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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
#include "AssemblyComments.h"
#include "Disassembler.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64DOpcode.h"
#include "MacroAssemblerCodeRef.h"

namespace JSC {

bool tryToDisassemble(const MacroAssemblerCodePtr<DisassemblyPtrTag>& codePtr, size_t size, void* codeStart, void* codeEnd, const char* prefix, PrintStream& out)
{
    uint32_t* currentPC = codePtr.untaggedExecutableAddress<uint32_t*>();
    size_t byteCount = size;

    uint32_t* armCodeStart = bitwise_cast<uint32_t*>(codeStart);
    uint32_t* armCodeEnd = bitwise_cast<uint32_t*>(codeEnd);
    A64DOpcode arm64Opcode(armCodeStart, armCodeEnd);

    unsigned pcOffset = (currentPC - armCodeStart) * sizeof(uint32_t);
    char pcInfo[25];
    while (byteCount) {
        if (codeStart)
            snprintf(pcInfo, sizeof(pcInfo) - 1, "<%u> %#llx", pcOffset, static_cast<unsigned long long>(bitwise_cast<uintptr_t>(currentPC)));
        else
            snprintf(pcInfo, sizeof(pcInfo) - 1, "%#llx", static_cast<unsigned long long>(bitwise_cast<uintptr_t>(currentPC)));
        out.printf("%s%24s: %s", prefix, pcInfo, arm64Opcode.disassemble(currentPC));
        if (auto str = AssemblyCommentRegistry::singleton().comment(reinterpret_cast<void*>(currentPC)))
            out.printf("; %s\n", str->ascii().data());
        else
            out.printf("\n");
        pcOffset += sizeof(uint32_t);
        currentPC++;
        byteCount -= sizeof(uint32_t);
    }

    return true;
}

} // namespace JSC

#endif // ENABLE(ARM64_DISASSEMBLER)

