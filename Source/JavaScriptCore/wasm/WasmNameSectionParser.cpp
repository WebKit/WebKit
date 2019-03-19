/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WasmNameSectionParser.h"

#if ENABLE(WEBASSEMBLY)

#include "IdentifierInlines.h"
#include "WasmNameSection.h"

namespace JSC { namespace Wasm {

auto NameSectionParser::parse() -> Result
{
    Ref<NameSection> nameSection = NameSection::create();
    WASM_PARSER_FAIL_IF(!nameSection->functionNames.tryReserveCapacity(m_info.functionIndexSpaceSize()), "can't allocate enough memory for function names");
    nameSection->functionNames.resize(m_info.functionIndexSpaceSize());

    for (size_t payloadNumber = 0; m_offset < length(); ++payloadNumber) {
        uint8_t nameType;
        uint32_t payloadLength;
        WASM_PARSER_FAIL_IF(!parseUInt7(nameType), "can't get name type for payload ", payloadNumber);
        WASM_PARSER_FAIL_IF(!parseVarUInt32(payloadLength), "can't get payload length for payload ", payloadNumber);
        WASM_PARSER_FAIL_IF(payloadLength > length() - m_offset, "payload length is too big for payload ", payloadNumber);
        const auto payloadStart = m_offset;
        
        if (!isValidNameType(nameType)) {
            // Unknown name section entries are simply ignored. This allows us to support newer toolchains without breaking older features.
            m_offset += payloadLength;
            continue;
        }

        switch (static_cast<NameType>(nameType)) {
        case NameType::Module: {
            uint32_t nameLen;
            Name nameString;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(nameLen), "can't get module's name length for payload ", payloadNumber);
            WASM_PARSER_FAIL_IF(!consumeUTF8String(nameString, nameLen), "can't get module's name of length ", nameLen, " for payload ", payloadNumber);
            nameSection->moduleName = WTFMove(nameString);
            break;
        }
        case NameType::Function: {
            uint32_t count;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get function count for payload ", payloadNumber);
            for (uint32_t function = 0; function < count; ++function) {
                uint32_t index;
                uint32_t nameLen;
                Name nameString;
                WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get function ", function, " index for payload ", payloadNumber);
                WASM_PARSER_FAIL_IF(m_info.functionIndexSpaceSize() <= index, "function ", function, " index ", index, " is larger than function index space ", m_info.functionIndexSpaceSize(), " for payload ", payloadNumber);
                WASM_PARSER_FAIL_IF(!parseVarUInt32(nameLen), "can't get functions ", function, "'s name length for payload ", payloadNumber);
                WASM_PARSER_FAIL_IF(!consumeUTF8String(nameString, nameLen), "can't get function ", function, "'s name of length ", nameLen, " for payload ", payloadNumber);
                nameSection->functionNames[index] = WTFMove(nameString);
            }
            break;
        }
        case NameType::Local: {
            // Ignore local names for now, we don't do anything with them but we still need to parse them in order to properly ignore them.
            uint32_t functionIndex;
            uint32_t count;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't get local's function index for payload ", payloadNumber);
            WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get local count for payload ", payloadNumber);
            for (uint32_t local = 0; local < count; ++local) {
                uint32_t index;
                uint32_t nameLen;
                Name nameString;
                WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get local ", local, " index for payload ", payloadNumber);
                WASM_PARSER_FAIL_IF(!parseVarUInt32(nameLen), "can't get local ", local, "'s name length for payload ", payloadNumber);
                WASM_PARSER_FAIL_IF(!consumeUTF8String(nameString, nameLen), "can't get local ", local, "'s name of length ", nameLen, " for payload ", payloadNumber);
            }
            break;
        }
        }
        WASM_PARSER_FAIL_IF(payloadStart + payloadLength != m_offset);
    }
    return nameSection;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
