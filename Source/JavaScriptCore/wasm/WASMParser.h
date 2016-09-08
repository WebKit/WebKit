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
#include "WASMFormat.h"
#include "WASMOps.h"
#include "WASMSections.h"
#include <wtf/LEBDecoder.h>

namespace JSC {

namespace WASM {

class Parser {
protected:
    Parser(const Vector<uint8_t>&, size_t start, size_t end);

    bool WARN_UNUSED_RETURN consumeCharacter(char);
    bool WARN_UNUSED_RETURN consumeString(const char*);

    bool WARN_UNUSED_RETURN parseVarUInt1(uint8_t& result);
    bool WARN_UNUSED_RETURN parseUInt7(uint8_t& result);
    bool WARN_UNUSED_RETURN parseUInt32(uint32_t& result);
    bool WARN_UNUSED_RETURN parseVarUInt32(uint32_t& result) { return decodeUInt32(m_source.data(), m_sourceLength, m_offset, result); }


    bool WARN_UNUSED_RETURN parseValueType(Type& result);

    const Vector<uint8_t>& m_source;
    size_t m_sourceLength;
    size_t m_offset;
};

ALWAYS_INLINE Parser::Parser(const Vector<uint8_t>& sourceBuffer, size_t start, size_t end)
    : m_source(sourceBuffer)
    , m_sourceLength(end)
    , m_offset(start)
{
    ASSERT(end <= sourceBuffer.size());
    ASSERT(start < end);
}

ALWAYS_INLINE bool Parser::consumeCharacter(char c)
{
    if (m_offset >= m_sourceLength)
        return false;
    if (c == m_source[m_offset]) {
        m_offset++;
        return true;
    }
    return false;
}

ALWAYS_INLINE bool Parser::consumeString(const char* str)
{
    unsigned start = m_offset;
    for (unsigned i = 0; str[i]; i++) {
        if (!consumeCharacter(str[i])) {
            m_offset = start;
            return false;
        }
    }
    return true;
}

ALWAYS_INLINE bool Parser::parseUInt32(uint32_t& result)
{
    if (m_offset + 4 >= m_sourceLength)
        return false;
    result = *reinterpret_cast<const uint32_t*>(m_source.data() + m_offset);
    m_offset += 4;
    return true;
}

ALWAYS_INLINE bool Parser::parseUInt7(uint8_t& result)
{
    if (m_offset >= m_sourceLength)
        return false;
    result = m_source[m_offset++];
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

ALWAYS_INLINE bool Parser::parseValueType(Type& result)
{
    uint8_t value;
    if (!parseUInt7(value))
        return false;
    if (value >= static_cast<uint8_t>(Type::LastValueType))
        return false;
    result = static_cast<Type>(value);
    return true;
}

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
