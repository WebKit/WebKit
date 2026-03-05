/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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

#include "WasmFormat.h"
#include "WasmOps.h"
#include "WasmParser.h"
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

class SectionParser final : public Parser<void> {
public:
    SectionParser(std::span<const uint8_t> data, size_t offsetInSource, ModuleInformation& info)
        : Parser(data)
        , m_offsetInSource(offsetInSource)
        , m_info(info)
    {
    }

#define WASM_SECTION_DECLARE_PARSER(NAME, ID, ORDERING, DESCRIPTION) [[nodiscard]] PartialResult parse ## NAME();
    FOR_EACH_KNOWN_WASM_SECTION(WASM_SECTION_DECLARE_PARSER)
#undef WASM_SECTION_DECLARE_PARSER

    [[nodiscard]] PartialResult parseCustom();

private:
    template <typename ...Args>
    [[nodiscard]] NEVER_INLINE UnexpectedResult fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in namespace above.
        if (ASSERT_ENABLED && Options::crashOnFailedWasmValidate()) [[unlikely]]
            CRASH();

        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_offset + m_offsetInSource), ": "_s, makeString(args)...));
    }

    [[nodiscard]] PartialResult parseGlobalType(GlobalInformation&);
    [[nodiscard]] PartialResult parseMemoryHelper(bool isImport);
    [[nodiscard]] PartialResult parseTableHelper(bool isImport);
    enum class LimitsType { Memory, Table };
    template <LimitsType T>
    [[nodiscard]] PartialResult parseResizableLimits(uint64_t& initial, std::optional<uint64_t>& maximum, bool& isShared, bool& is64bit);
    [[nodiscard]] PartialResult parseInitExpr(uint8_t&, bool&, uint64_t&, v128_t&, Type, Type& initExprType);
    [[nodiscard]] PartialResult parseI32InitExpr(std::optional<I32InitExpr>&, ASCIILiteral failMessage);

    [[nodiscard]] PartialResult parseFunctionType(uint32_t position, RefPtr<TypeDefinition>&);
    [[nodiscard]] PartialResult parsePackedType(PackedType&);
    [[nodiscard]] PartialResult parseStorageType(StorageType&);
    [[nodiscard]] PartialResult parseStructType(uint32_t position, RefPtr<TypeDefinition>&);
    [[nodiscard]] PartialResult parseArrayType(uint32_t position, RefPtr<TypeDefinition>&);
    [[nodiscard]] PartialResult parseRecursionGroup(uint32_t position, RefPtr<TypeDefinition>&);
    [[nodiscard]] PartialResult parseSubtype(uint32_t position, RefPtr<TypeDefinition>&, Vector<TypeIndex>&, bool);

    [[nodiscard]] PartialResult validateElementTableIdx(uint32_t, Type);
    [[nodiscard]] PartialResult parseI32InitExprForElementSection(std::optional<I32InitExpr>&);
    [[nodiscard]] PartialResult parseElementKind(uint8_t& elementKind);
    [[nodiscard]] PartialResult parseIndexCountForElementSection(uint32_t&, const unsigned);
    [[nodiscard]] PartialResult parseElementSegmentVectorOfExpressions(Type, Vector<Element::InitializationType>&, Vector<uint64_t>&, const unsigned, const unsigned);
    [[nodiscard]] PartialResult parseElementSegmentVectorOfIndexes(Vector<Element::InitializationType>&, Vector<uint64_t>&, const unsigned, const unsigned);

    [[nodiscard]] PartialResult parseI32InitExprForDataSection(std::optional<I32InitExpr>&);

    static bool checkStructuralSubtype(const TypeDefinition&, const TypeDefinition&);
    [[nodiscard]] PartialResult checkSubtypeValidity(const TypeDefinition&);

    size_t m_offsetInSource;
    const Ref<ModuleInformation> m_info;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
