
/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "WasmFunctionIPIntMetadataGenerator.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC {

namespace Wasm {

static inline int sizeOfLEB128(uint64_t value)
{
    // Efficiency!
    // Start masking from high bits
    //      6666 5555 5555 5544 4444 4444 3333 3333 3322 2222 2222 1111 1111 1100 0000 0000
    //      3210 9876 5432 1098 7654 3210 9876 5432 1098 7654 3210 9876 5432 1098 7654 3210
    // use: A999 9999 8888 8887 7777 7766 6666 6555 5555 4444 4443 3333 3322 2222 2111 1111
    //      hex0 hex1 hex2 hex3 hex4 hex5 hex6 hex7 hex8 hex9 hexa hexb hexc hexd hexe hexf
    //            0123456789abcdef
    if (value & 0xf000000000000000)
        return 10;
    if (value & 0x7f00000000000000)
        return 9;
    if (value & 0x00fe000000000000)
        return 8;
    if (value & 0x0001fc0000000000)
        return 7;
    if (value & 0x000004f800000000)
        return 6;
    if (value & 0x00000007f0000000)
        return 5;
    if (value & 0x000000000fe00000)
        return 4;
    if (value & 0x00000000001fc000)
        return 3;
    if (value & 0x0000000000003f80)
        return 2;
    return 1;
}

void FunctionIPIntMetadataGenerator::addBlankSpace(uint32_t size)
{
    m_metadata.grow(m_metadata.size() + size);
}

void FunctionIPIntMetadataGenerator::addRawValue(uint64_t value)
{
    auto size = m_metadata.size();
    m_metadata.grow(m_metadata.size() + 8);
    WRITE_TO_METADATA(m_metadata.data() + size, value, uint64_t);
}

void FunctionIPIntMetadataGenerator::addLEB128ConstantInt32AndLength(uint32_t value, uint32_t length)
{
    size_t size = m_metadata.size();
    m_metadata.grow(size + 8);
    WRITE_TO_METADATA(m_metadata.data() + size, value, uint32_t);
    WRITE_TO_METADATA(m_metadata.data() + size + 4, static_cast<uint32_t>(sizeOfLEB128(length)), uint32_t);
}

void FunctionIPIntMetadataGenerator::addLEB128ConstantAndLengthForType(Type type, uint64_t value, uint32_t length)
{

    // Metadata format:
    //      0 1 2 3 4 5 6 7  8 9 A B C D E F
    // I32: <value> <length>
    // I64: <    value    >  <   length   >
    // F32: <value> <blank>
    // F64: <    value    >

    if (type.isI32()) {
        size_t size = m_metadata.size();
        m_metadata.grow(size + 8);
        WRITE_TO_METADATA(m_metadata.data() + size, static_cast<uint32_t>(value), uint32_t);
        WRITE_TO_METADATA(m_metadata.data() + size + 4, static_cast<uint32_t>(length), uint32_t);
    } else if (type.isI64()) {
        size_t size = m_metadata.size();
        m_metadata.grow(size + 16);
        WRITE_TO_METADATA(m_metadata.data() + size, static_cast<uint64_t>(value), uint64_t);
        WRITE_TO_METADATA(m_metadata.data() + size + 8, static_cast<uint64_t>(length), uint64_t);
    } else if (!type.isF32() && !type.isF64())
        ASSERT_NOT_IMPLEMENTED_YET();
}

void FunctionIPIntMetadataGenerator::addReturnData(const Vector<Type>& types)
{
    size_t size = m_metadata.size();
    // 2 bytes for count (just in case we have a lot. if you're returning more than 65k values,
    // congratulations?) Actually is count to skip since the round up
    // 1 byte for each value returned (bit 0 = use FPR, bit 1 = on stack, bit 2 = valid)
    m_metadata.grow(size + roundUpToMultipleOf(8, types.size() + 2));
    WRITE_TO_METADATA(m_metadata.data() + size, m_metadata.size() - size, uint16_t);
    int returnRegLeft = 2;
    int floatReturnLeft = 1;
    for (size_t i = 0; i < types.size(); ++i) {
        auto type = types[i];
        if (type.isI32() || type.isI64()) {
            if (!returnRegLeft)
                m_metadata[size + 2 + i] = 0b110;
            else {
                m_metadata[size + 2 + i] = 0b100;
                returnRegLeft--;
            }
        } else if (type.isF32() || type.isF64()) {
            if (!floatReturnLeft)
                m_metadata[size + 2 + i] = 0b111;
            else {
                m_metadata[size + 2 + i] = 0b101;
                floatReturnLeft--;
            }
        }
    }
    for (size_t i = size + types.size() + 2; i < m_metadata.size(); ++i)
        m_metadata[i] = 0;
}

} }

#endif // ENABLE(WEBASSEMBLY)
