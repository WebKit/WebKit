/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "Disassembler.h"

#if ENABLE(ZYDIS)

#include "AssemblyComments.h"
#include "MacroAssemblerCodeRef.h"
#include "Zydis.h"

namespace JSC {

bool tryToDisassemble(const CodePtr<DisassemblyPtrTag>& codePtr, size_t size, void*, void*, const char* prefix, PrintStream& out)
{
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_ATT);
    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_FORCE_SIZE, ZYAN_TRUE);
    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_HEX_UPPERCASE, ZYAN_FALSE);
    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_ADDR_PADDING_ABSOLUTE, ZYDIS_PADDING_DISABLED);
    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_ADDR_PADDING_RELATIVE, ZYDIS_PADDING_DISABLED);
    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_DISP_PADDING, ZYDIS_PADDING_DISABLED);
    ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_IMM_PADDING, ZYDIS_PADDING_DISABLED);

    const auto* data = codePtr.dataLocation<unsigned char*>();
    ZyanUSize offset = 0;
    ZydisDecodedInstruction instruction;
    char formatted[1024];
    while (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&decoder, data + offset, size - offset, &instruction))) {
        if (ZYAN_SUCCESS(ZydisFormatterFormatInstruction(&formatter, &instruction, formatted, sizeof(formatted), bitwise_cast<unsigned long long>(data + offset))))
            out.printf("%s%#16llx: %s", prefix, static_cast<unsigned long long>(bitwise_cast<uintptr_t>(data + offset)), formatted);
        else
            out.printf("%s%#16llx: failed-to-format", prefix, static_cast<unsigned long long>(bitwise_cast<uintptr_t>(data + offset)));
        if (auto str = AssemblyCommentRegistry::singleton().comment(reinterpret_cast<void*>(static_cast<unsigned long long>(bitwise_cast<uintptr_t>(data + offset)))))
            out.printf("; %s\n", str->ascii().data());
        else
            out.printf("\n");
        offset += instruction.length;
    }

    return true;
}

} // namespace JSC

#endif // ENABLE(ZYDIS)
