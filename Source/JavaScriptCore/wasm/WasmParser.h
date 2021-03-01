/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "B3Procedure.h"
#include "JITCompilation.h"
#include "VirtualRegister.h"
#include "WasmFormat.h"
#include "WasmLimits.h"
#include "WasmModuleInformation.h"
#include "WasmOps.h"
#include "WasmSections.h"
#include <type_traits>
#include <wtf/Expected.h>
#include <wtf/LEBDecoder.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace JSC { namespace Wasm {

namespace FailureHelper {
// FIXME We should move this to makeString. It's in its own namespace to enable C++ Argument Dependent Lookup à la std::swap: user code can deblare its own "boxFailure" and the fail() helper will find it.
template<typename T>
inline String makeString(const T& failure) { return WTF::toString(failure); }
}

template<typename SuccessType>
class Parser {
public:
    typedef String ErrorType;
    typedef Unexpected<ErrorType> UnexpectedResult;
    typedef Expected<void, ErrorType> PartialResult;
    typedef Expected<SuccessType, ErrorType> Result;

    const uint8_t* source() const { return m_source; }
    size_t length() const { return m_sourceLength; }
    size_t offset() const { return m_offset; }

protected:
    Parser(const uint8_t*, size_t);

    bool WARN_UNUSED_RETURN consumeCharacter(char);
    bool WARN_UNUSED_RETURN consumeString(const char*);
    bool WARN_UNUSED_RETURN consumeUTF8String(Name&, size_t);

    bool WARN_UNUSED_RETURN parseVarUInt1(uint8_t&);
    bool WARN_UNUSED_RETURN parseInt7(int8_t&);
    bool WARN_UNUSED_RETURN peekInt7(int8_t&);
    bool WARN_UNUSED_RETURN parseUInt7(uint8_t&);
    bool WARN_UNUSED_RETURN parseUInt8(uint8_t&);
    bool WARN_UNUSED_RETURN parseUInt32(uint32_t&);
    bool WARN_UNUSED_RETURN parseUInt64(uint64_t&);
    bool WARN_UNUSED_RETURN parseVarUInt32(uint32_t&);
    bool WARN_UNUSED_RETURN parseVarUInt64(uint64_t&);

    bool WARN_UNUSED_RETURN parseVarInt32(int32_t&);
    bool WARN_UNUSED_RETURN parseVarInt64(int64_t&);

    PartialResult WARN_UNUSED_RETURN parseBlockSignature(const ModuleInformation&, BlockSignature&);
    bool WARN_UNUSED_RETURN parseValueType(Type&);
    bool WARN_UNUSED_RETURN parseRefType(Type&);
    bool WARN_UNUSED_RETURN parseExternalKind(ExternalKind&);

    size_t m_offset = 0;

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in namespace above.
        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_offset), ": "_s, makeString(args)...));
    }
#define WASM_PARSER_FAIL_IF(condition, ...) do { \
    if (UNLIKELY(condition))                     \
        return fail(__VA_ARGS__);                \
    } while (0)

#define WASM_FAIL_IF_HELPER_FAILS(helper) do {                      \
        auto helperResult = helper;                                 \
        if (UNLIKELY(!helperResult))                                \
            return makeUnexpected(WTFMove(helperResult.error()));   \
    } while (0)

private:
    const uint8_t* m_source;
    size_t m_sourceLength;
    // We keep a local reference to the global table so we don't have to fetch it to find thunk signatures.
    const SignatureInformation& m_signatureInformation;
};

template<typename SuccessType>
ALWAYS_INLINE Parser<SuccessType>::Parser(const uint8_t* sourceBuffer, size_t sourceLength)
    : m_source(sourceBuffer)
    , m_sourceLength(sourceLength)
    , m_signatureInformation(SignatureInformation::singleton())
{
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::consumeCharacter(char c)
{
    if (m_offset >= length())
        return false;
    if (c == source()[m_offset]) {
        m_offset++;
        return true;
    }
    return false;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::consumeString(const char* str)
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

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::consumeUTF8String(Name& result, size_t stringLength)
{
    if (length() < stringLength || m_offset > length() - stringLength)
        return false;
    if (stringLength > maxStringSize)
        return false;
    if (!result.tryReserveCapacity(stringLength))
        return false;

    const uint8_t* stringStart = source() + m_offset;

    // We don't cache the UTF-16 characters since it seems likely the string is ASCII.
    if (UNLIKELY(!charactersAreAllASCII(stringStart, stringLength))) {
        Vector<UChar, 1024> buffer(stringLength);
        UChar* bufferStart = buffer.data();

        UChar* bufferCurrent = bufferStart;
        const char* stringCurrent = reinterpret_cast<const char*>(stringStart);
        if (!WTF::Unicode::convertUTF8ToUTF16(stringCurrent, reinterpret_cast<const char *>(stringStart + stringLength), &bufferCurrent, bufferCurrent + buffer.size()))
            return false;
    }

    result.grow(stringLength);
    memcpy(result.data(), stringStart, stringLength);
    m_offset += stringLength;
    return true;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseVarUInt32(uint32_t& result)
{
    return WTF::LEBDecoder::decodeUInt32(m_source, m_sourceLength, m_offset, result);
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseVarUInt64(uint64_t& result)
{
    return WTF::LEBDecoder::decodeUInt64(m_source, m_sourceLength, m_offset, result);
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseVarInt32(int32_t& result)
{
    return WTF::LEBDecoder::decodeInt32(m_source, m_sourceLength, m_offset, result);
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseVarInt64(int64_t& result)
{
    return WTF::LEBDecoder::decodeInt64(m_source, m_sourceLength, m_offset, result);
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseUInt32(uint32_t& result)
{
    if (length() < 4 || m_offset > length() - 4)
        return false;
    result = *reinterpret_cast<const uint32_t*>(source() + m_offset);
    m_offset += 4;
    return true;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseUInt64(uint64_t& result)
{
    if (length() < 8 || m_offset > length() - 8)
        return false;
    result = *reinterpret_cast<const uint64_t*>(source() + m_offset);
    m_offset += 8;
    return true;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseUInt8(uint8_t& result)
{
    if (m_offset >= length())
        return false;
    result = source()[m_offset++];
    return true;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseInt7(int8_t& result)
{
    if (m_offset >= length())
        return false;
    uint8_t v = source()[m_offset++];
    result = (v & 0x40) ? WTF::bitwise_cast<int8_t>(uint8_t(v | 0x80)) : v;
    return (v & 0x80) == 0;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::peekInt7(int8_t& result)
{
    if (m_offset >= length())
        return false;
    uint8_t v = source()[m_offset];
    result = (v & 0x40) ? WTF::bitwise_cast<int8_t>(uint8_t(v | 0x80)) : v;
    return (v & 0x80) == 0;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseUInt7(uint8_t& result)
{
    if (m_offset >= length())
        return false;
    result = source()[m_offset++];
    return result < 0x80;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseVarUInt1(uint8_t& result)
{
    uint32_t temp;
    if (!parseVarUInt32(temp))
        return false;
    if (temp > 1)
        return false;
    result = static_cast<uint8_t>(temp);
    return true;
}

template<typename SuccessType>
ALWAYS_INLINE typename Parser<SuccessType>::PartialResult Parser<SuccessType>::parseBlockSignature(const ModuleInformation& info, BlockSignature& result)
{
    int8_t value;
    if (peekInt7(value) && isValidType(value)) {
        Type type = static_cast<Type>(value);
        WASM_PARSER_FAIL_IF(!(isValueType(type) || type == Void), "result type of block: ", makeString(type), " is not a value type or Void");
        result = m_signatureInformation.thunkFor(type);
        m_offset++;
        return { };
    }

    WASM_PARSER_FAIL_IF(!Options::useWebAssemblyMultiValues(), "Type table indices for block signatures are not supported yet");

    int64_t index;
    WASM_PARSER_FAIL_IF(!parseVarInt64(index), "Block-like instruction doesn't return value type but can't decode type section index");
    WASM_PARSER_FAIL_IF(index < 0, "Block-like instruction signature index is negative");
    WASM_PARSER_FAIL_IF(static_cast<size_t>(index) >= info.usedSignatures.size(), "Block-like instruction signature index is out of bounds. Index: ", index, " type index space: ", info.usedSignatures.size());

    result = &info.usedSignatures[index].get();
    return { };
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseValueType(Type& result)
{
    int8_t value;
    if (!parseInt7(value))
        return false;
    if (!isValidType(value) || !isValueType(static_cast<Type>(value)))
        return false;
    result = static_cast<Type>(value);
    return true;
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseRefType(Type& result)
{
    const bool parsed = parseValueType(result);
    return parsed && isRefType(result);
}

template<typename SuccessType>
ALWAYS_INLINE bool Parser<SuccessType>::parseExternalKind(ExternalKind& result)
{
    uint8_t value;
    if (!parseUInt7(value))
        return false;
    if (!isValidExternalKind(value))
        return false;
    result = static_cast<ExternalKind>(value);
    return true;
}

ALWAYS_INLINE I32InitExpr makeI32InitExpr(uint8_t opcode, uint32_t bits)
{
    RELEASE_ASSERT(opcode == I32Const || opcode == GetGlobal);
    if (opcode == I32Const)
        return I32InitExpr::constValue(bits);
    return I32InitExpr::globalImport(bits);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
