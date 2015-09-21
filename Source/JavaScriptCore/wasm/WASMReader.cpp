/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 *
 * =========================================================================
 *
 * Copyright (c) 2015 by the repository authors of
 * WebAssembly/polyfill-prototype-1.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "WASMReader.h"

#if ENABLE(WEBASSEMBLY)

#include <wtf/text/StringBuilder.h>

#define CHECK_READ(length) do { if (m_cursor + length > m_buffer.end()) return false; } while (0)

namespace JSC {

bool WASMReader::readUInt32(uint32_t& result)
{
    CHECK_READ(4);
    result = m_cursor[0] | m_cursor[1] << 8 | m_cursor[2] << 16 | m_cursor[3] << 24;
    m_cursor += 4;
    return true;
}

bool WASMReader::readFloat(float& result)
{
    CHECK_READ(4);
    union {
        uint8_t bytes[4];
        float floatValue;
    } u = {
#if CPU(BIG_ENDIAN)
        { m_cursor[3], m_cursor[2], m_cursor[1], m_cursor[0] }
#else
        { m_cursor[0], m_cursor[1], m_cursor[2], m_cursor[3] }
#endif
    };
    result = u.floatValue;
    m_cursor += 4;
    return true;
}

bool WASMReader::readDouble(double& result)
{
    CHECK_READ(8);
    union {
        uint8_t bytes[8];
        double doubleValue;
    } u = {
#if CPU(BIG_ENDIAN)
        { m_cursor[7], m_cursor[6], m_cursor[5], m_cursor[4], m_cursor[3], m_cursor[2], m_cursor[1], m_cursor[0] }
#else
        { m_cursor[0], m_cursor[1], m_cursor[2], m_cursor[3], m_cursor[4], m_cursor[5], m_cursor[6], m_cursor[7] }
#endif
    };
    result = u.doubleValue;
    m_cursor += 8;
    return true;
}

bool WASMReader::readCompactInt32(uint32_t& result)
{
    uint32_t sum = 0;
    unsigned shift = 0;
    do {
        CHECK_READ(1);
        uint8_t byte = *m_cursor++;
        if (byte < 0x80) {
            sum |= byte << shift;
            int signExtend = (32 - 7) - shift;
            if (signExtend > 0)
                result = int32_t(sum) << signExtend >> signExtend;
            else
                result = int32_t(sum);
            return true;
        }
        sum |= (byte & firstSevenBitsMask) << shift;
        shift += 7;
    } while (shift < 35);
    return false;
}

bool WASMReader::readCompactUInt32(uint32_t& result)
{
    uint32_t sum = 0;
    unsigned shift = 0;
    do {
        CHECK_READ(1);
        uint32_t byte = *m_cursor++;
        if (byte < 0x80) {
            if ((shift == 28 && byte >= 0x10) || (shift && !byte))
                return false;
            result = sum | (byte << shift);
            return true;
        }
        sum |= (byte & firstSevenBitsMask) << shift;
        shift += 7;
    } while (shift < 35);
    return false;
}

bool WASMReader::readString(String& result)
{
    StringBuilder builder;
    while (true) {
        CHECK_READ(1);
        char c = *m_cursor++;
        if (!c)
            break;
        builder.append(c);
    }
    result = builder.toString();
    return true;
}

bool WASMReader::readType(WASMType& result)
{
    return readByte<WASMType>(result, static_cast<uint8_t>(WASMType::NumberOfTypes));
}

bool WASMReader::readExpressionType(WASMExpressionType& result)
{
    return readByte<WASMExpressionType>(result, static_cast<uint8_t>(WASMExpressionType::NumberOfExpressionTypes));
}

bool WASMReader::readExportFormat(WASMExportFormat& result)
{
    return readByte<WASMExportFormat>(result, static_cast<uint8_t>(WASMExportFormat::NumberOfExportFormats));
}

template <class T> bool WASMReader::readByte(T& result, uint8_t numberOfValues)
{
    CHECK_READ(1);
    uint8_t byte = *m_cursor++;
    if (byte >= numberOfValues)
        return false;
    result = T(byte);
    return true;
}

bool WASMReader::readOpStatement(bool& hasImmediate, WASMOpStatement& op, WASMOpStatementWithImmediate& opWithImmediate, uint8_t& immediate)
{
    return readOp(hasImmediate, op, opWithImmediate, immediate,
        static_cast<uint8_t>(WASMOpStatement::NumberOfWASMOpStatements),
        static_cast<uint8_t>(WASMOpStatementWithImmediate::NumberOfWASMOpStatementWithImmediates));
}

bool WASMReader::readOpExpressionI32(bool& hasImmediate, WASMOpExpressionI32& op, WASMOpExpressionI32WithImmediate& opWithImmediate, uint8_t& immediate)
{
    return readOp(hasImmediate, op, opWithImmediate, immediate,
        static_cast<uint8_t>(WASMOpExpressionI32::NumberOfWASMOpExpressionI32s),
        static_cast<uint8_t>(WASMOpExpressionI32WithImmediate::NumberOfWASMOpExpressionI32WithImmediates));
}

bool WASMReader::readOpExpressionF32(bool& hasImmediate, WASMOpExpressionF32& op, WASMOpExpressionF32WithImmediate& opWithImmediate, uint8_t& immediate)
{
    return readOp(hasImmediate, op, opWithImmediate, immediate,
        static_cast<uint8_t>(WASMOpExpressionF32::NumberOfWASMOpExpressionF32s),
        static_cast<uint8_t>(WASMOpExpressionF32WithImmediate::NumberOfWASMOpExpressionF32WithImmediates));
}

bool WASMReader::readOpExpressionF64(bool& hasImmediate, WASMOpExpressionF64& op, WASMOpExpressionF64WithImmediate& opWithImmediate, uint8_t& immediate)
{
    return readOp(hasImmediate, op, opWithImmediate, immediate,
        static_cast<uint8_t>(WASMOpExpressionF64::NumberOfWASMOpExpressionF64s),
        static_cast<uint8_t>(WASMOpExpressionF64WithImmediate::NumberOfWASMOpExpressionF64WithImmediates));
}

bool WASMReader::readOpExpressionVoid(WASMOpExpressionVoid& op)
{
    return readByte<WASMOpExpressionVoid>(op,
        static_cast<uint8_t>(WASMOpExpressionVoid::NumberOfWASMOpExpressionVoids));
}

bool WASMReader::readVariableTypes(bool& hasImmediate, WASMVariableTypes& variableTypes, WASMVariableTypesWithImmediate& variableTypesWithImmediate, uint8_t& immediate)
{
    return readOp(hasImmediate, variableTypes, variableTypesWithImmediate, immediate,
        static_cast<uint8_t>(WASMVariableTypes::NumberOfVariableTypes),
        static_cast<uint8_t>(WASMVariableTypesWithImmediate::NumberOfVariableTypesWithImmediates));
}

template <class T, class TWithImmediate>
bool WASMReader::readOp(bool& hasImmediate, T& op, TWithImmediate& opWithImmediate, uint8_t& immediate, uint8_t numberOfValues, uint8_t numberOfValuesWithImmediate)
{
    CHECK_READ(1);
    uint8_t byte = *m_cursor++;

    if (!(byte & hasImmediateInOpFlag)) {
        if (byte >= numberOfValues)
            return false;
        hasImmediate = false;
        op = T(byte);
        return true;
    }

    uint8_t byteWithoutImmediate = (byte >> immediateBits) & (opWithImmediateLimit - 1);
    if (byteWithoutImmediate >= numberOfValuesWithImmediate)
        return false;
    hasImmediate = true;
    opWithImmediate = TWithImmediate(byteWithoutImmediate);
    immediate = byte & (immediateLimit - 1);
    return true;
}

bool WASMReader::readSwitchCase(WASMSwitchCase& result)
{
    return readByte<WASMSwitchCase>(result, static_cast<uint8_t>(WASMSwitchCase::NumberOfSwitchCases));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
