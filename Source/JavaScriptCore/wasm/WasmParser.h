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
#include "SIMDInfo.h"
#include "VirtualRegister.h"
#include "WasmFormat.h"
#include "WasmLimits.h"
#include "WasmModuleInformation.h"
#include "WasmOps.h"
#include "WasmSections.h"
#include "WasmTypeDefinitionInlines.h"
#include "Width.h"
#include <type_traits>
#include <wtf/Expected.h>
#include <wtf/LEBDecoder.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace JSC { namespace Wasm {

namespace FailureHelper {
// FIXME We should move this to makeString. It's in its own namespace to enable C++ Argument Dependent Lookup à la std::swap: user code can deblare its own "boxFailure" and the fail() helper will find it.
template<typename T>
inline String makeString(const T& failure) { return WTF::toString(failure); }
}

class ParserBase {
public:
    typedef String ErrorType;
    typedef Unexpected<ErrorType> UnexpectedResult;
    typedef Expected<void, ErrorType> PartialResult;

    std::span<const uint8_t> source() const { return m_source; }
    size_t offset() const { return m_offset; }

protected:
    struct RecursionGroupInformation {
        bool inRecursionGroup;
        uint32_t start;
        uint32_t end;
    };

    explicit ParserBase(std::span<const uint8_t>);

    bool WARN_UNUSED_RETURN consumeCharacter(char);
    bool WARN_UNUSED_RETURN consumeString(const char*);
    bool WARN_UNUSED_RETURN consumeUTF8String(Name&, size_t);

    bool WARN_UNUSED_RETURN parseVarUInt1(uint8_t&);
    bool WARN_UNUSED_RETURN parseInt7(int8_t&);
    bool WARN_UNUSED_RETURN peekInt7(int8_t&);
    bool WARN_UNUSED_RETURN parseUInt7(uint8_t&);
    bool WARN_UNUSED_RETURN peekUInt8(uint8_t&);
    bool WARN_UNUSED_RETURN parseUInt8(uint8_t&);
    bool WARN_UNUSED_RETURN parseUInt32(uint32_t&);
    bool WARN_UNUSED_RETURN parseUInt64(uint64_t&);
    bool WARN_UNUSED_RETURN parseImmByteArray16(v128_t&);
    PartialResult WARN_UNUSED_RETURN parseImmLaneIdx(uint8_t laneCount, uint8_t&);
    bool WARN_UNUSED_RETURN parseVarUInt32(uint32_t&);
    bool WARN_UNUSED_RETURN parseVarUInt64(uint64_t&);

    bool WARN_UNUSED_RETURN parseVarInt32(int32_t&);
    bool WARN_UNUSED_RETURN parseVarInt64(int64_t&);

    PartialResult WARN_UNUSED_RETURN parseBlockSignature(const ModuleInformation&, BlockSignature&);
    PartialResult WARN_UNUSED_RETURN parseReftypeSignature(const ModuleInformation&, BlockSignature&);
    bool WARN_UNUSED_RETURN parseValueType(const ModuleInformation&, Type&);
    bool WARN_UNUSED_RETURN parseRefType(const ModuleInformation&, Type&);
    bool WARN_UNUSED_RETURN parseExternalKind(ExternalKind&);
    bool WARN_UNUSED_RETURN parseHeapType(const ModuleInformation&, int32_t&);

    size_t m_offset = 0;

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in namespace above.
        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, m_offset, ": "_s, makeString(args)...));
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
    std::span<const uint8_t> m_source;

protected:
    // We keep a local reference to the global table so we don't have to fetch it to find thunk types.
    const TypeInformation& m_typeInformation;
    // Used to track whether we are in a recursion group and the group's type indices, if any.
    RecursionGroupInformation m_recursionGroupInformation;
};

template<typename SuccessType> class Parser : public ParserBase {
public:
    using Result = Expected<SuccessType, ErrorType>;

    explicit Parser(std::span<const uint8_t> span)
        : ParserBase { span }
    { }
};

ALWAYS_INLINE ParserBase::ParserBase(std::span<const uint8_t> source)
    : m_source(source)
    , m_typeInformation(TypeInformation::singleton())
    , m_recursionGroupInformation({ })
{
}

ALWAYS_INLINE bool ParserBase::consumeCharacter(char c)
{
    if (m_offset >= m_source.size())
        return false;
    if (c == m_source[m_offset]) {
        m_offset++;
        return true;
    }
    return false;
}

ALWAYS_INLINE bool ParserBase::consumeString(const char* str)
{
    unsigned start = m_offset;
    if (m_offset >= m_source.size())
        return false;
    for (size_t i = 0; str[i]; i++) {
        if (!consumeCharacter(str[i])) {
            m_offset = start;
            return false;
        }
    }
    return true;
}

ALWAYS_INLINE bool ParserBase::consumeUTF8String(Name& result, size_t stringLength)
{
    if (!stringLength)
        return true;
    if (m_source.size() < stringLength || m_offset > m_source.size() - stringLength)
        return false;
    if (stringLength > maxStringSize)
        return false;
    if (!result.tryReserveCapacity(stringLength))
        return false;

    auto string = spanReinterpretCast<const char8_t>(m_source.subspan(m_offset, stringLength));
    if (auto checkResult = WTF::Unicode::checkUTF8(string); checkResult.characters.size() != string.size())
        return false;

    result.grow(stringLength);
    memcpy(result.data(), string.data(), stringLength);
    m_offset += stringLength;
    return true;
}

ALWAYS_INLINE bool ParserBase::parseVarUInt32(uint32_t& result)
{
    return WTF::LEBDecoder::decodeUInt32(m_source, m_offset, result);
}

ALWAYS_INLINE bool ParserBase::parseVarUInt64(uint64_t& result)
{
    return WTF::LEBDecoder::decodeUInt64(m_source, m_offset, result);
}

ALWAYS_INLINE bool ParserBase::parseVarInt32(int32_t& result)
{
    return WTF::LEBDecoder::decodeInt32(m_source, m_offset, result);
}

ALWAYS_INLINE bool ParserBase::parseVarInt64(int64_t& result)
{
    return WTF::LEBDecoder::decodeInt64(m_source, m_offset, result);
}

ALWAYS_INLINE bool ParserBase::parseUInt32(uint32_t& result)
{
    if (m_source.size() < m_offset + 4)
        return false;
    memcpy(&result, m_source.data() + m_offset, sizeof(uint32_t)); // src can be unaligned
    m_offset += 4;
    return true;
}

ALWAYS_INLINE bool ParserBase::parseUInt64(uint64_t& result)
{
    if (m_source.size() < m_offset + 8)
        return false;
    memcpy(&result, m_source.data() + m_offset, sizeof(uint64_t)); // src can be unaligned
    m_offset += 8;
    return true;
}

ALWAYS_INLINE bool ParserBase::parseImmByteArray16(v128_t& result)
{
    if (m_source.size() < m_offset + 16)
        return false;
    std::copy_n(m_source.begin() + m_offset, 16, result.u8x16);
    m_offset += 16;
    return true;
}

ALWAYS_INLINE typename ParserBase::PartialResult ParserBase::parseImmLaneIdx(uint8_t laneCount, uint8_t& result)
{
    RELEASE_ASSERT(laneCount == 2 || laneCount == 4 || laneCount == 8 || laneCount == 16 || laneCount == 32);
    WASM_PARSER_FAIL_IF(!parseUInt8(result), "Could not parse the lane index immediate byte."_s);
    WASM_PARSER_FAIL_IF(result >= laneCount, "Lane index immediate is too large, saw "_s, laneCount, ", expected an ImmLaneIdx"_s, laneCount);
    return { };
}

ALWAYS_INLINE bool ParserBase::peekUInt8(uint8_t& result)
{
    if (m_offset >= m_source.size())
        return false;
    result = m_source[m_offset];
    return true;
}

ALWAYS_INLINE bool ParserBase::parseUInt8(uint8_t& result)
{
    if (m_offset >= m_source.size())
        return false;
    result = m_source[m_offset++];
    return true;
}

ALWAYS_INLINE bool ParserBase::parseInt7(int8_t& result)
{
    if (m_offset >= m_source.size())
        return false;
    uint8_t v = m_source[m_offset++];
    result = (v & 0x40) ? WTF::bitwise_cast<int8_t>(uint8_t(v | 0x80)) : v;
    return !(v & 0x80);
}

ALWAYS_INLINE bool ParserBase::peekInt7(int8_t& result)
{
    if (m_offset >= m_source.size())
        return false;
    uint8_t v = m_source[m_offset];
    result = (v & 0x40) ? WTF::bitwise_cast<int8_t>(uint8_t(v | 0x80)) : v;
    return !(v & 0x80);
}

ALWAYS_INLINE bool ParserBase::parseUInt7(uint8_t& result)
{
    if (m_offset >= m_source.size())
        return false;
    result = m_source[m_offset++];
    return result < 0x80;
}

ALWAYS_INLINE bool ParserBase::parseVarUInt1(uint8_t& result)
{
    uint32_t temp;
    if (!parseVarUInt32(temp))
        return false;
    if (temp > 1)
        return false;
    result = static_cast<uint8_t>(temp);
    return true;
}

ALWAYS_INLINE typename ParserBase::PartialResult ParserBase::parseBlockSignature(const ModuleInformation& info, BlockSignature& result)
{
    int8_t kindByte;
    if (peekInt7(kindByte) && isValidTypeKind(kindByte)) {
        TypeKind typeKind = static_cast<TypeKind>(kindByte);

        if ((isValidHeapTypeKind(kindByte) || typeKind == TypeKind::Ref || typeKind == TypeKind::RefNull))
            return parseReftypeSignature(info, result);

        Type type = { typeKind, TypeDefinition::invalidIndex };
        WASM_PARSER_FAIL_IF(!(isValueType(type) || type.isVoid()), "result type of block: "_s, makeString(type.kind), " is not a value type or Void"_s);
        result = m_typeInformation.thunkFor(type);
        m_offset++;
        return { };
    }

    int64_t index;
    WASM_PARSER_FAIL_IF(!parseVarInt64(index), "Block-like instruction doesn't return value type but can't decode type section index"_s);
    WASM_PARSER_FAIL_IF(index < 0, "Block-like instruction signature index is negative"_s);
    WASM_PARSER_FAIL_IF(static_cast<size_t>(index) >= info.typeCount(), "Block-like instruction signature index is out of bounds. Index: "_s, index, " type index space: "_s, info.typeCount());

    const auto& signature = info.typeSignatures[index].get().expand();
    WASM_PARSER_FAIL_IF(!signature.is<FunctionSignature>(), "Block-like instruction signature index does not refer to a function type definition"_s);

    result = signature.as<FunctionSignature>();
    return { };
}

inline typename ParserBase::PartialResult ParserBase::parseReftypeSignature(const ModuleInformation& info, BlockSignature& result)
{
    Type resultType;
    WASM_PARSER_FAIL_IF(!parseValueType(info, resultType), "result type of block is not a valid ref type"_s);
    Vector<Type, 16> returnTypes { resultType };
    const auto& typeDefinition = TypeInformation::typeDefinitionForFunction(returnTypes, { }).get();
    result = &TypeInformation::getFunctionSignature(typeDefinition->index());

    return { };
}

ALWAYS_INLINE bool ParserBase::parseHeapType(const ModuleInformation& info, int32_t& result)
{
    int32_t heapType;
    if (!parseVarInt32(heapType))
        return false;

    if (heapType < 0) {
        if (isValidHeapTypeKind(heapType)) {
            result = heapType;
            return true;
        }
        return false;
    }

    if (static_cast<size_t>(heapType) >= info.typeCount() && (!m_recursionGroupInformation.inRecursionGroup || !(static_cast<uint32_t>(heapType) >= m_recursionGroupInformation.start && static_cast<uint32_t>(heapType) < m_recursionGroupInformation.end)))
        return false;

    result = heapType;
    return true;
}

ALWAYS_INLINE bool ParserBase::parseValueType(const ModuleInformation& info, Type& result)
{
    int8_t kind;
    if (!parseInt7(kind))
        return false;
    if (!isValidTypeKind(kind))
        return false;

    TypeKind typeKind = static_cast<TypeKind>(kind);
    TypeIndex typeIndex = 0;
    if (isValidHeapTypeKind(kind)) {
        typeIndex = static_cast<TypeIndex>(typeKind);
        typeKind = TypeKind::RefNull;
    } else if (typeKind == TypeKind::Ref || typeKind == TypeKind::RefNull) {
        int32_t heapType;
        if (!parseHeapType(info, heapType))
            return false;
        if (heapType < 0)
            typeIndex = static_cast<TypeIndex>(heapType);
        else {
            // For recursive references inside recursion groups, we construct a
            // placeholder projection with an invalid group index. These should
            // be replaced with a real type index in expand() before use.
            if (m_recursionGroupInformation.inRecursionGroup && static_cast<uint32_t>(heapType) >= m_recursionGroupInformation.start) {
                ASSERT(static_cast<uint32_t>(heapType) >= info.typeCount() && static_cast<uint32_t>(heapType) < m_recursionGroupInformation.end);
                ProjectionIndex groupIndex = static_cast<ProjectionIndex>(heapType - m_recursionGroupInformation.start);
                RefPtr<TypeDefinition> def = TypeInformation::getPlaceholderProjection(groupIndex);
                typeIndex = def->index();
            } else {
                ASSERT(static_cast<uint32_t>(heapType) < info.typeCount());
                typeIndex = TypeInformation::get(info.typeSignatures[heapType].get());
            }
        }
    }

    Type type = { typeKind, typeIndex };
    if (!isValueType(type))
        return false;
    result = type;
    return true;
}

ALWAYS_INLINE bool ParserBase::parseRefType(const ModuleInformation& info, Type& result)
{
    const bool parsed = parseValueType(info, result);
    return parsed && isRefType(result);
}

ALWAYS_INLINE bool ParserBase::parseExternalKind(ExternalKind& result)
{
    uint8_t value;
    if (!parseUInt7(value))
        return false;
    if (!isValidExternalKind(value))
        return false;
    result = static_cast<ExternalKind>(value);
    return true;
}

ALWAYS_INLINE I32InitExpr makeI32InitExpr(uint8_t opcode, bool isExtendedConstantExpression, uint32_t bits)
{
    RELEASE_ASSERT(opcode == I32Const || opcode == GetGlobal);
    if (isExtendedConstantExpression)
        return I32InitExpr::extendedExpression(bits);
    if (opcode == I32Const)
        return I32InitExpr::constValue(bits);
    return I32InitExpr::globalImport(bits);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
