/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "B3Procedure.h"
#include "WasmFormat.h"
#include "WasmOps.h"
#include "WasmSections.h"
#include <wtf/LEBDecoder.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Wasm {

class Parser {
protected:
    Parser(const uint8_t*, size_t);

    bool WARN_UNUSED_RETURN consumeCharacter(char);
    bool WARN_UNUSED_RETURN consumeString(const char*);
    bool WARN_UNUSED_RETURN consumeUTF8String(String&, size_t);

    bool WARN_UNUSED_RETURN parseVarUInt1(uint8_t&);
    bool WARN_UNUSED_RETURN parseInt7(int8_t&);
    bool WARN_UNUSED_RETURN parseUInt7(uint8_t&);
    bool WARN_UNUSED_RETURN parseUInt8(uint8_t&);
    bool WARN_UNUSED_RETURN parseUInt32(uint32_t&);
    bool WARN_UNUSED_RETURN parseVarUInt32(uint32_t&);
    bool WARN_UNUSED_RETURN parseVarUInt64(uint64_t&);

    bool WARN_UNUSED_RETURN parseResultType(Type&);
    bool WARN_UNUSED_RETURN parseValueType(Type&);
    bool WARN_UNUSED_RETURN parseExternalKind(External::Kind&);

    const uint8_t* source() const { return m_source; }
    size_t length() const { return m_sourceLength; }

    size_t m_offset = 0;

private:
    const uint8_t* m_source;
    size_t m_sourceLength;
};

ALWAYS_INLINE Parser::Parser(const uint8_t* sourceBuffer, size_t sourceLength)
    : m_source(sourceBuffer)
    , m_sourceLength(sourceLength)
{
}

ALWAYS_INLINE bool Parser::consumeCharacter(char c)
{
    if (m_offset >= length())
        return false;
    if (c == source()[m_offset]) {
        m_offset++;
        return true;
    }
    return false;
}

ALWAYS_INLINE bool Parser::consumeString(const char* str)
{
    unsigned start = m_offset;
    if (m_offset >= length())
        return false;
    for (size_t i = 0; str[i]; i++) {
        if (!consumeCharacter(str[i])) {
            m_offset = start;
            return false;
        }
    }
    return true;
}

ALWAYS_INLINE bool Parser::consumeUTF8String(String& result, size_t stringLength)
{
    if (stringLength == 0) {
        result = emptyString();
        return true;
    }
    if (length() < stringLength || m_offset > length() - stringLength)
        return false;
    result = String::fromUTF8(static_cast<const LChar*>(&source()[m_offset]), stringLength);
    m_offset += stringLength;
    if (result.isEmpty())
        return false;
    return true;
}

ALWAYS_INLINE bool Parser::parseVarUInt32(uint32_t& result)
{
    return WTF::LEBDecoder::decodeUInt32(m_source, m_sourceLength, m_offset, result);
}

ALWAYS_INLINE bool Parser::parseVarUInt64(uint64_t& result)
{
    return WTF::LEBDecoder::decodeUInt64(m_source, m_sourceLength, m_offset, result);
}

ALWAYS_INLINE bool Parser::parseUInt32(uint32_t& result)
{
    if (length() < 4 || m_offset > length() - 4)
        return false;
    result = *reinterpret_cast<const uint32_t*>(source() + m_offset);
    m_offset += 4;
    return true;
}

ALWAYS_INLINE bool Parser::parseUInt8(uint8_t& result)
{
    if (m_offset >= length())
        return false;
    result = source()[m_offset++];
    return true;
}

ALWAYS_INLINE bool Parser::parseInt7(int8_t& result)
{
    if (m_offset >= length())
        return false;
    uint8_t v = source()[m_offset++];
    result = (v & 0x40) ? WTF::bitwise_cast<int8_t>(uint8_t(v | 0x80)) : v;
    return (v & 0x80) == 0;
}

ALWAYS_INLINE bool Parser::parseUInt7(uint8_t& result)
{
    if (m_offset >= length())
        return false;
    result = source()[m_offset++];
    return result < 0x80;
}

ALWAYS_INLINE bool Parser::parseVarUInt1(uint8_t& result)
{
    uint32_t temp;
    if (!parseVarUInt32(temp))
        return false;
    result = static_cast<uint8_t>(temp);
    return temp <= 1;
}

ALWAYS_INLINE bool Parser::parseResultType(Type& result)
{
    int8_t value;
    if (!parseInt7(value))
        return false;
    if (!isValidType(value))
        return false;
    result = static_cast<Type>(value);
    return true;
}

ALWAYS_INLINE bool Parser::parseValueType(Type& result)
{
    return parseResultType(result) && isValueType(result);
}
    
ALWAYS_INLINE bool Parser::parseExternalKind(External::Kind& result)
{
    uint8_t value;
    if (!parseUInt7(value))
        return false;
    if (!External::isValid(value))
        return false;
    result = static_cast<External::Kind>(value);
    return true;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
