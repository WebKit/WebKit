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
 */

#ifndef WASMReader_h
#define WASMReader_h

#if ENABLE(WEBASSEMBLY)

#include "WASMConstants.h"
#include "WASMFormat.h"
#include <wtf/Vector.h>

namespace JSC {

class WASMReader {
public:
    WASMReader(const Vector<uint8_t>& buffer)
        : m_buffer(buffer)
        , m_cursor(buffer.data())
    {
    }

    unsigned offset() const { return m_cursor - m_buffer.data(); }
    void setOffset(unsigned offset) { m_cursor = m_buffer.data() + offset; }

    bool readUInt32(uint32_t& result);
    bool readFloat(float& result);
    bool readDouble(double& result);
    bool readCompactInt32(uint32_t& result);
    bool readCompactUInt32(uint32_t& result);
    bool readString(String& result);
    bool readType(WASMType& result);
    bool readExpressionType(WASMExpressionType& result);
    bool readExportFormat(WASMExportFormat& result);
    bool readOpStatement(bool& hasImmediate, WASMOpStatement&, WASMOpStatementWithImmediate&, uint8_t& immediate);
    bool readOpExpressionI32(bool& hasImmediate, WASMOpExpressionI32&, WASMOpExpressionI32WithImmediate&, uint8_t& immediate);
    bool readOpExpressionF32(bool& hasImmediate, WASMOpExpressionF32&, WASMOpExpressionF32WithImmediate&, uint8_t& immediate);
    bool readOpExpressionF64(bool& hasImmediate, WASMOpExpressionF64&, WASMOpExpressionF64WithImmediate&, uint8_t& immediate);
    bool readOpExpressionVoid(WASMOpExpressionVoid&);
    bool readVariableTypes(bool& hasImmediate, WASMVariableTypes&, WASMVariableTypesWithImmediate&, uint8_t& immediate);
    bool readSwitchCase(WASMSwitchCase& result);

private:
    static const uint32_t firstSevenBitsMask = 0x7f;

    template <class T> bool readByte(T& result, uint8_t numberOfValues);
    template <class T, class TWithImmediate> bool readOp(bool& hasImmediate, T&, TWithImmediate&, uint8_t& immediate, uint8_t numberOfValues, uint8_t numberOfValuesWithImmediate);

    const Vector<uint8_t>& m_buffer;
    const uint8_t* m_cursor;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMReader_h
