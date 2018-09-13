/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "WasmModuleParser.h"

#if ENABLE(WEBASSEMBLY)

#include "IdentifierInlines.h"
#include "WasmMemoryInformation.h"
#include "WasmNameSectionParser.h"
#include "WasmOps.h"
#include "WasmSectionParser.h"
#include "WasmSections.h"
#include <wtf/SHA1.h>

namespace JSC { namespace Wasm {

auto ModuleParser::parse() -> Result
{
    const size_t minSize = 8;
    uint32_t versionNumber;

    WASM_PARSER_FAIL_IF(length() < minSize, "expected a module of at least ", minSize, " bytes");
    WASM_PARSER_FAIL_IF(length() > maxModuleSize, "module size ", length(), " is too large, maximum ", maxModuleSize);
    WASM_PARSER_FAIL_IF(!consumeCharacter(0) || !consumeString("asm"), "modules doesn't start with '\\0asm'");
    WASM_PARSER_FAIL_IF(!parseUInt32(versionNumber), "can't parse version number");
    WASM_PARSER_FAIL_IF(versionNumber != expectedVersionNumber, "unexpected version number ", versionNumber, " expected ", expectedVersionNumber);

    // This is not really a known section.
    Section previousKnownSection = Section::Begin;
    while (m_offset < length()) {
        uint8_t sectionByte;

        WASM_PARSER_FAIL_IF(!parseUInt7(sectionByte), "can't get section byte");

        Section section = Section::Custom;
        WASM_PARSER_FAIL_IF(!decodeSection(sectionByte, section));
        ASSERT(section != Section::Begin);

        uint32_t sectionLength;
        WASM_PARSER_FAIL_IF(!validateOrder(previousKnownSection, section), "invalid section order, ", previousKnownSection, " followed by ", section);
        WASM_PARSER_FAIL_IF(!parseVarUInt32(sectionLength), "can't get ", section, " section's length");
        WASM_PARSER_FAIL_IF(sectionLength > length() - m_offset, section, " section of size ", sectionLength, " would overflow Module's size");

        SectionParser parser(source() + m_offset, sectionLength, m_offset, m_info.get());

        switch (section) {
#define WASM_SECTION_PARSE(NAME, ID, DESCRIPTION)                   \
        case Section::NAME: {                                       \
            WASM_FAIL_IF_HELPER_FAILS(parser.parse ## NAME());             \
            break;                                                  \
        }
        FOR_EACH_KNOWN_WASM_SECTION(WASM_SECTION_PARSE)
#undef WASM_SECTION_PARSE

        case Section::Custom: {
            WASM_FAIL_IF_HELPER_FAILS(parser.parseCustom());
            break;
        }

        case Section::Begin: {
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        }

        WASM_PARSER_FAIL_IF(parser.length() != parser.offset(), "parsing ended before the end of ", section, " section");

        m_offset += sectionLength;


        if (isKnownSection(section))
            previousKnownSection = section;
    }

    if (UNLIKELY(Options::useEagerWebAssemblyModuleHashing())) {
        SHA1 hasher;
        hasher.addBytes(source(), length());
        m_info->nameSection->setHash(hasher.computeHexDigest());
    }

    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
