/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WasmSectionParser.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "WasmBranchHintsSectionParser.h"
#include "WasmConstExprGenerator.h"
#include "WasmMemoryInformation.h"
#include "WasmNameSectionParser.h"
#include "WasmOps.h"
#include "WasmSIMDOpcodes.h"
#include "WasmSourceMappingURLSectionParser.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/HexNumber.h>
#include <wtf/SetForScope.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

auto SectionParser::parseType() -> PartialResult
{
    uint32_t count;
    uint32_t recursionGroupCount = 0;

    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Type section's count"_s);
    WASM_PARSER_FAIL_IF(count > maxTypes, "Type section's count is too big "_s, count, " maximum "_s, maxTypes);
    RELEASE_ASSERT(!m_info->typeSignatures.capacity());
    RELEASE_ASSERT(!m_info->rtts.capacity());
    WASM_PARSER_FAIL_IF(!m_info->typeSignatures.tryReserveInitialCapacity(count), "can't allocate enough memory for Type section's "_s, count, " entries"_s);
    WASM_PARSER_FAIL_IF(!m_info->rtts.tryReserveInitialCapacity(count), "can't allocate enough memory for Type section's "_s, count, " canonical RTT entries"_s);

    for (uint32_t i = 0; i < count; ++i) {
        int8_t typeKind;
        WASM_PARSER_FAIL_IF(!parseInt7(typeKind), "can't get "_s, i, "th Type's type"_s);
        RefPtr<TypeDefinition> signature;

        // When GC is enabled, recursive references can show up in any of these cases.
        SetForScope<RecursionGroupInformation> recursionGroupInfo(m_recursionGroupInformation, Options::useWasmGC() ? RecursionGroupInformation { true, m_info->typeCount(), m_info->typeCount() + 1 } : RecursionGroupInformation { false, 0, 0 });

        switch (static_cast<TypeKind>(typeKind)) {
        case TypeKind::Func: {
            WASM_FAIL_IF_HELPER_FAILS(parseFunctionType(i, signature));
            break;
        }
        case TypeKind::Struct: {
            if (!Options::useWasmGC())
                return fail(i, "th type failed to parse because struct types are not enabled");

            WASM_FAIL_IF_HELPER_FAILS(parseStructType(i, signature));
            break;
        }
        case TypeKind::Array: {
            if (!Options::useWasmGC())
                return fail(i, "th type failed to parse because array types are not enabled");

            WASM_FAIL_IF_HELPER_FAILS(parseArrayType(i, signature));
            break;
        }
        case TypeKind::Rec: {
            if (!Options::useWasmGC())
                return fail(i, "th type failed to parse because rec types are not enabled");

            WASM_FAIL_IF_HELPER_FAILS(parseRecursionGroup(i, signature));
            ++recursionGroupCount;
            WASM_PARSER_FAIL_IF(recursionGroupCount > maxNumberOfRecursionGroups, "number of recursion groups exceeded the limit of "_s, maxNumberOfRecursionGroups);
            break;
        }
        case TypeKind::Sub:
        case TypeKind::Subfinal: {
            if (!Options::useWasmGC())
                return fail(i, "th type failed to parse because sub types are not enabled");

            Vector<TypeIndex> empty;
            WASM_FAIL_IF_HELPER_FAILS(parseSubtype(i, signature, empty, static_cast<TypeKind>(typeKind) == TypeKind::Subfinal));
            break;
        }
        default:
            return fail(i, "th Type is non-Func, non-Struct, and non-Array ", typeKind);
        }

        WASM_PARSER_FAIL_IF(!signature, "can't allocate enough memory for Type section's "_s, i, "th signature"_s);

        // When GC is enabled, type definitions that appear on their own are shorthand
        // notations for recursion groups with one type. Here we ensure that if such a
        // shorthand type is actually recursive, it is represented with a recursion group.
        // Subtyping checks are also done here to ensure sub types are subtypes of their parents.
        if (Options::useWasmGC()) {
            // Recursion group parsing will append the entries itself, as there may
            // be multiple entries that need to be added to the type section for
            // each recursion group.
            if (!signature->is<RecursionGroup>()) {
                if (signature->hasRecursiveReference()) {
                    Vector<TypeIndex> types;
                    bool result = types.tryAppend(signature->index());
                    WASM_PARSER_FAIL_IF(!result, "can't allocate enough memory for Type section's "_s, i, "th signature"_s);
                    RefPtr<TypeDefinition> group = TypeInformation::typeDefinitionForRecursionGroup(types);
                    RefPtr<TypeDefinition> projection = TypeInformation::typeDefinitionForProjection(group->index(), 0);
                    TypeInformation::registerCanonicalRTTForType(projection->index());
                    m_info->rtts.append(TypeInformation::getCanonicalRTT(projection->index()));
                    if (signature->is<Subtype>()) {
                        WASM_PARSER_FAIL_IF(m_info->rtts.last()->displaySize() > maxSubtypeDepth, "subtype depth for Type section's "_s, i, "th signature exceeded the limits of "_s, maxSubtypeDepth);
                        WASM_FAIL_IF_HELPER_FAILS(checkSubtypeValidity(projection->unroll()));
                    }
                    m_info->typeSignatures.append(projection.releaseNonNull());
                } else {
                    TypeInformation::registerCanonicalRTTForType(signature->index());
                    m_info->rtts.append(TypeInformation::getCanonicalRTT(signature->index()));
                    if (signature->is<Subtype>()) {
                        WASM_PARSER_FAIL_IF(m_info->rtts.last()->displaySize() > maxSubtypeDepth, "subtype depth for Type section's "_s, i, "th signature exceeded the limits of "_s, maxSubtypeDepth);
                        WASM_FAIL_IF_HELPER_FAILS(checkSubtypeValidity(signature->unroll()));
                    }
                    m_info->typeSignatures.append(signature.releaseNonNull());
                }
            }
        } else
            m_info->typeSignatures.append(signature.releaseNonNull());
    }
    return { };
}

auto SectionParser::parseImport() -> PartialResult
{
    uint32_t importCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(importCount), "can't get Import section's count"_s);
    WASM_PARSER_FAIL_IF(importCount > maxImports, "Import section's count is too big "_s, importCount, " maximum "_s, maxImports);
    RELEASE_ASSERT(!m_info->globals.capacity());
    RELEASE_ASSERT(!m_info->imports.capacity());
    RELEASE_ASSERT(!m_info->importFunctionTypeIndices.capacity());
    RELEASE_ASSERT(!m_info->importExceptionTypeIndices.capacity());
    WASM_PARSER_FAIL_IF(!m_info->globals.tryReserveInitialCapacity(importCount), "can't allocate enough memory for "_s, importCount, " globals"_s); // FIXME this over-allocates when we fix the FIXMEs below.
    WASM_PARSER_FAIL_IF(!m_info->imports.tryReserveInitialCapacity(importCount), "can't allocate enough memory for "_s, importCount, " imports"_s); // FIXME this over-allocates when we fix the FIXMEs below.
    WASM_PARSER_FAIL_IF(!m_info->importFunctionTypeIndices.tryReserveInitialCapacity(importCount), "can't allocate enough memory for "_s, importCount, " import function signatures"_s); // FIXME this over-allocates when we fix the FIXMEs below.
    WASM_PARSER_FAIL_IF(!m_info->importExceptionTypeIndices.tryReserveInitialCapacity(importCount), "can't allocate enough memory for "_s, importCount, " import exception signatures"_s); // FIXME this over-allocates when we fix the FIXMEs below.

    for (uint32_t importNumber = 0; importNumber < importCount; ++importNumber) {
        uint32_t moduleLen;
        uint32_t fieldLen;
        Name moduleString;
        Name fieldString;
        ExternalKind kind;
        unsigned kindIndex { 0 };

        WASM_PARSER_FAIL_IF(!parseVarUInt32(moduleLen), "can't get "_s, importNumber, "th Import's module name length"_s);
        WASM_PARSER_FAIL_IF(!consumeUTF8String(moduleString, moduleLen), "can't get "_s, importNumber, "th Import's module name of length "_s, moduleLen);

        WASM_PARSER_FAIL_IF(!parseVarUInt32(fieldLen), "can't get "_s, importNumber, "th Import's field name length in module '"_s, moduleString, "'"_s);
        WASM_PARSER_FAIL_IF(!consumeUTF8String(fieldString, fieldLen), "can't get "_s, importNumber, "th Import's field name of length "_s, moduleLen, " in module '"_s, moduleString, "'"_s);

        WASM_PARSER_FAIL_IF(!parseExternalKind(kind), "can't get "_s, importNumber, "th Import's kind in module '"_s, moduleString, "' field '"_s, fieldString, "'"_s);
        switch (kind) {
        case ExternalKind::Function: {
            uint32_t functionTypeIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(functionTypeIndex), "can't get "_s, importNumber, "th Import's function signature in module '"_s, moduleString, "' field '"_s, fieldString, "'"_s);
            WASM_PARSER_FAIL_IF(functionTypeIndex >= m_info->typeCount(), "invalid function signature for "_s, importNumber, "th Import, "_s, functionTypeIndex, " is out of range of "_s, m_info->typeCount(), " in module '"_s, moduleString, "' field '"_s, fieldString, "'"_s);
            kindIndex = m_info->importFunctionTypeIndices.size();
            TypeIndex typeIndex = TypeInformation::get(m_info->typeSignatures[functionTypeIndex]);
            m_info->importFunctionTypeIndices.append(typeIndex);
            break;
        }
        case ExternalKind::Table: {
            bool isImport = true;
            kindIndex = m_info->tables.size();
            PartialResult result = parseTableHelper(isImport);
            if (UNLIKELY(!result))
                return makeUnexpected(WTFMove(result.error()));
            break;
        }
        case ExternalKind::Memory: {
            bool isImport = true;
            PartialResult result = parseMemoryHelper(isImport);
            if (UNLIKELY(!result))
                return makeUnexpected(WTFMove(result.error()));
            break;
        }
        case ExternalKind::Global: {
            GlobalInformation global;
            WASM_FAIL_IF_HELPER_FAILS(parseGlobalType(global));
            // Only mutable globals need floating bindings.
            if (global.mutability == Mutability::Mutable)
                global.bindingMode = GlobalInformation::BindingMode::Portable;
            kindIndex = m_info->globals.size();
            m_info->globals.append(WTFMove(global));
            break;
        }
        case ExternalKind::Exception: {
            uint8_t tagType;
            WASM_PARSER_FAIL_IF(!parseUInt8(tagType), "can't get "_s, importNumber, "th Import exception's tag type"_s);
            WASM_PARSER_FAIL_IF(tagType, importNumber, "th Import exception has tag type "_s, tagType, " but the only supported tag type is 0"_s);

            uint32_t exceptionSignatureIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(exceptionSignatureIndex), "can't get "_s, importNumber, "th Import's exception signature in module '"_s, moduleString, "' field '"_s, fieldString, "'"_s);
            WASM_PARSER_FAIL_IF(exceptionSignatureIndex >= m_info->typeCount(), "invalid exception signature for "_s, importNumber, "th Import, "_s, exceptionSignatureIndex, " is out of range of "_s, m_info->typeCount(), " in module '"_s, moduleString, "' field '"_s, fieldString, "'"_s);
            kindIndex = m_info->importExceptionTypeIndices.size();
            TypeIndex typeIndex = TypeInformation::get(m_info->typeSignatures[exceptionSignatureIndex]);
            m_info->importExceptionTypeIndices.append(typeIndex);
            break;
        }
        }

        m_info->imports.append({ WTFMove(moduleString), WTFMove(fieldString), kind, kindIndex });
    }

    m_info->firstInternalGlobal = m_info->globals.size();
    return { };
}

auto SectionParser::parseFunction() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Function section's count"_s);
    WASM_PARSER_FAIL_IF(count > maxFunctions, "Function section's count is too big "_s, count, " maximum "_s, maxFunctions);
    RELEASE_ASSERT(!m_info->internalFunctionTypeIndices.capacity());
    RELEASE_ASSERT(!m_info->functions.capacity());
    WASM_PARSER_FAIL_IF(!m_info->internalFunctionTypeIndices.tryReserveInitialCapacity(count), "can't allocate enough memory for "_s, count, " Function signatures"_s);
    WASM_PARSER_FAIL_IF(!m_info->functions.tryReserveInitialCapacity(count), "can't allocate enough memory for "_s, count, "Function locations"_s);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t typeNumber;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(typeNumber), "can't get "_s, i, "th Function's type number"_s);
        WASM_PARSER_FAIL_IF(typeNumber >= m_info->typeCount(), i, "th Function type number is invalid "_s, typeNumber);

        TypeIndex typeIndex = TypeInformation::get(m_info->typeSignatures[typeNumber]);
        // The Code section fixes up start and end.
        size_t start = 0;
        size_t end = 0;
        m_info->internalFunctionTypeIndices.append(typeIndex);
        m_info->functions.append({ start, end, Vector<uint8_t>() });
    }

    // Note that `initializeFunctionTrackers` should only be used after both parseImport and parseFunction
    // finish updating importFunctionTypeIndices and internalFunctionTypeIndices.
    m_info->initializeFunctionTrackers();
    return { };
}

auto SectionParser::parseResizableLimits(uint32_t& initial, std::optional<uint32_t>& maximum, bool& isShared, LimitsType limitsType) -> PartialResult
{
    ASSERT(!maximum);

    uint8_t flags;
    WASM_PARSER_FAIL_IF(!parseUInt8(flags), "can't parse resizable limits flags"_s);
    WASM_PARSER_FAIL_IF(flags != 0x0 && flags != 0x1 && flags != 0x3, "resizable limits flag should be 0x00, 0x01, or 0x03 but 0x"_s, hex(flags, 2, Lowercase));
    WASM_PARSER_FAIL_IF(flags == 0x3 && limitsType != LimitsType::Memory, "can't use shared limits for non memory"_s);
    WASM_PARSER_FAIL_IF(!parseVarUInt32(initial), "can't parse resizable limits initial page count"_s);

    isShared = flags == 0x3;
    WASM_PARSER_FAIL_IF(isShared && !Options::useWasmFaultSignalHandler(), "shared memory is not enabled"_s);

    if (flags) {
        uint32_t maximumInt;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(maximumInt), "can't parse resizable limits maximum page count"_s);
        WASM_PARSER_FAIL_IF(initial > maximumInt, "resizable limits has an initial page count of "_s, initial, " which is greater than its maximum "_s, maximumInt);
        maximum = maximumInt;
    }

    return { };
}

auto SectionParser::parseTableHelper(bool isImport) -> PartialResult
{
    WASM_PARSER_FAIL_IF(m_info->tableCount() >= maxTables, "Table count of "_s, m_info->tableCount(), " is too big, maximum "_s, maxTables);

    Type type;
    bool hasInitExpr = false;
    TableInformation::InitializationType tableInitType = TableInformation::Default;
    uint64_t initialBitsOrImportNumber = 0;

    int8_t firstByte = 0;
    WASM_PARSER_FAIL_IF(!peekInt7(firstByte), "can't parse Table information"_s);
    if (!isImport && static_cast<TypeKind>(firstByte) == TypeKind::Void) {
        hasInitExpr = true;
        m_offset++;
        uint8_t reservedByte;
        WASM_PARSER_FAIL_IF(!parseUInt8(reservedByte) || reservedByte, "can't parse explicitly initialized Table's reserved byte"_s);
    }

    WASM_PARSER_FAIL_IF(!parseValueType(m_info, type), "can't parse Table type"_s);
    WASM_PARSER_FAIL_IF(!isRefType(type), "Table type should be a ref type, got "_s, type);
    if (!hasInitExpr)
        WASM_PARSER_FAIL_IF(!isDefaultableType(type), "Table's type must be defaultable"_s);

    uint32_t initial;
    std::optional<uint32_t> maximum;
    bool isShared = false;
    PartialResult limits = parseResizableLimits(initial, maximum, isShared, LimitsType::Table);
    if (UNLIKELY(!limits))
        return makeUnexpected(WTFMove(limits.error()));
    WASM_PARSER_FAIL_IF(initial > maxTableEntries, "Table's initial page count of "_s, initial, " is too big, maximum "_s, maxTableEntries);

    ASSERT(!maximum || *maximum >= initial);

    if (hasInitExpr) {
        uint8_t initOpcode;
        Type typeForInitOpcode;
        bool isExtendedConstantExpression;
        v128_t unusedVector { };
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, isExtendedConstantExpression, initialBitsOrImportNumber, unusedVector, type, typeForInitOpcode));
        WASM_PARSER_FAIL_IF(!isSubtype(typeForInitOpcode, type), "Table init_expr opcode of type "_s, typeForInitOpcode.kind, " doesn't match table's type "_s, type.kind);

        if (isExtendedConstantExpression)
            tableInitType = TableInformation::FromExtendedExpression;
        else if (initOpcode == GetGlobal)
            tableInitType = TableInformation::FromGlobalImport;
        else if (initOpcode == RefFunc)
            tableInitType = TableInformation::FromRefFunc;
        else if (initOpcode == RefNull)
            tableInitType = TableInformation::FromRefNull;
        else
            RELEASE_ASSERT_NOT_REACHED();
    }

    TableElementType tableType = isSubtype(type, funcrefType()) ? TableElementType::Funcref : TableElementType::Externref;
    m_info->tables.append(TableInformation(initial, maximum, isImport, tableType, type, tableInitType, initialBitsOrImportNumber));

    return { };
}

auto SectionParser::parseTable() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Table's count"_s);

    for (unsigned i = 0; i < count; ++i) {
        bool isImport = false;
        PartialResult result = parseTableHelper(isImport);
        if (UNLIKELY(!result))
            return makeUnexpected(WTFMove(result.error()));
    }

    return { };
}

auto SectionParser::parseMemoryHelper(bool isImport) -> PartialResult
{
    WASM_PARSER_FAIL_IF(m_info->memoryCount(), "there can at most be one Memory section for now"_s);

    PageCount initialPageCount;
    PageCount maximumPageCount;
    bool isShared = false;
    {
        uint32_t initial;
        std::optional<uint32_t> maximum;
        PartialResult limits = parseResizableLimits(initial, maximum, isShared, LimitsType::Memory);
        if (UNLIKELY(!limits))
            return makeUnexpected(WTFMove(limits.error()));
        ASSERT(!maximum || *maximum >= initial);
        WASM_PARSER_FAIL_IF(!PageCount::isValid(initial), "Memory's initial page count of "_s, initial, " is invalid"_s);

        initialPageCount = PageCount(initial);

        if (maximum) {
            WASM_PARSER_FAIL_IF(!PageCount::isValid(*maximum), "Memory's maximum page count of "_s, *maximum, " is invalid"_s);
            maximumPageCount = PageCount(*maximum);
        }
    }
    ASSERT(initialPageCount);
    ASSERT(!maximumPageCount || maximumPageCount >= initialPageCount);

    m_info->memory = MemoryInformation(initialPageCount, maximumPageCount, isShared, isImport);
    return { };
}

auto SectionParser::parseMemory() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't parse Memory section's count"_s);

    if (!count)
        return { };

    WASM_PARSER_FAIL_IF(count != 1, "Memory section has more than one memory, WebAssembly currently only allows zero or one"_s);

    bool isImport = false;
    return parseMemoryHelper(isImport);
}

auto SectionParser::parseGlobal() -> PartialResult
{
    uint32_t globalCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(globalCount), "can't get Global section's count"_s);
    WASM_PARSER_FAIL_IF(globalCount > maxGlobals, "Global section's count is too big "_s, globalCount, " maximum "_s, maxGlobals);
    size_t totalGlobals = globalCount + m_info->firstInternalGlobal;
    ASSERT(m_info->firstInternalGlobal == m_info->globals.size());
    WASM_PARSER_FAIL_IF(!m_info->globals.tryGrowCapacityBy(globalCount), "can't allocate memory for "_s, totalGlobals, " globals"_s);

    for (uint32_t globalIndex = 0; globalIndex < globalCount; ++globalIndex) {
        GlobalInformation global;
        uint8_t initOpcode;
        v128_t initVector { };
        uint64_t initialBitsOrImportNumber = 0;

        WASM_FAIL_IF_HELPER_FAILS(parseGlobalType(global));
        Type typeForInitOpcode;
        bool isExtendedConstantExpression;
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, isExtendedConstantExpression, initialBitsOrImportNumber, initVector, global.type, typeForInitOpcode));
        if (typeForInitOpcode.isV128())
            global.initialBits.initialVector = initVector;
        else
            global.initialBits.initialBitsOrImportNumber = initialBitsOrImportNumber;

        if (isExtendedConstantExpression)
            global.initializationType = GlobalInformation::FromExtendedExpression;
        else if (initOpcode == GetGlobal)
            global.initializationType = GlobalInformation::FromGlobalImport;
        else if (initOpcode == RefFunc)
            global.initializationType = GlobalInformation::FromRefFunc;
        else
            global.initializationType = GlobalInformation::FromExpression;
        WASM_PARSER_FAIL_IF(!isSubtype(typeForInitOpcode, global.type), "Global init_expr opcode of type "_s, typeForInitOpcode.kind, " doesn't match global's type "_s, global.type.kind);

        if (initOpcode == RefFunc) {
            ASSERT(global.initializationType != GlobalInformation::FromVector);
            m_info->addDeclaredFunction(global.initialBits.initialBitsOrImportNumber);
        }

        m_info->globals.append(WTFMove(global));
    }

    return { };
}

auto SectionParser::parseExport() -> PartialResult
{
    uint32_t exportCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(exportCount), "can't get Export section's count"_s);
    WASM_PARSER_FAIL_IF(exportCount > maxExports, "Export section's count is too big "_s, exportCount, " maximum "_s, maxExports);
    RELEASE_ASSERT(!m_info->exports.capacity());
    WASM_PARSER_FAIL_IF(!m_info->exports.tryReserveInitialCapacity(exportCount), "can't allocate enough memory for "_s, exportCount, " exports"_s);

    HashSet<String> exportNames;
    for (uint32_t exportNumber = 0; exportNumber < exportCount; ++exportNumber) {
        uint32_t fieldLen;
        Name fieldString;
        ExternalKind kind;
        unsigned kindIndex;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(fieldLen), "can't get "_s, exportNumber, "th Export's field name length"_s);
        WASM_PARSER_FAIL_IF(!consumeUTF8String(fieldString, fieldLen), "can't get "_s, exportNumber, "th Export's field name of length "_s, fieldLen);
        String fieldName = WTF::makeString(fieldString);
        WASM_PARSER_FAIL_IF(exportNames.contains(fieldName), "duplicate export: '"_s, fieldString, "'"_s);
        exportNames.add(fieldName);

        WASM_PARSER_FAIL_IF(!parseExternalKind(kind), "can't get "_s, exportNumber, "th Export's kind, named '"_s, fieldString, "'"_s);
        WASM_PARSER_FAIL_IF(!parseVarUInt32(kindIndex), "can't get "_s, exportNumber, "th Export's kind index, named '"_s, fieldString, "'"_s);
        switch (kind) {
        case ExternalKind::Function: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->functionIndexSpaceSize(), exportNumber, "th Export has invalid function number "_s, kindIndex, " it exceeds the function index space "_s, m_info->functionIndexSpaceSize(), ", named '"_s, fieldString, "'"_s);
            m_info->addDeclaredFunction(kindIndex);
            break;
        }
        case ExternalKind::Table: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->tableCount(), "can't export Table "_s, kindIndex, " there are "_s, m_info->tableCount(), " Tables"_s);
            break;
        }
        case ExternalKind::Memory: {
            WASM_PARSER_FAIL_IF(!m_info->memory, "can't export a non-existent Memory"_s);
            WASM_PARSER_FAIL_IF(kindIndex, "can't export Memory "_s, kindIndex, " only one Table is currently supported"_s);
            break;
        }
        case ExternalKind::Global: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->globals.size(), exportNumber, "th Export has invalid global number "_s, kindIndex, " it exceeds the globals count "_s, m_info->globals.size(), ", named '"_s, fieldString, "'"_s);
            // Only mutable globals need floating bindings.
            GlobalInformation& global = m_info->globals[kindIndex];
            if (global.mutability == Mutability::Mutable)
                global.bindingMode = GlobalInformation::BindingMode::Portable;
            break;
        }
        case ExternalKind::Exception: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->exceptionIndexSpaceSize(), exportNumber, "th Export has invalid exception number "_s, kindIndex, " it exceeds the exception index space "_s, m_info->exceptionIndexSpaceSize(), ", named '"_s, fieldString, "'"_s);
            m_info->addDeclaredException(kindIndex);
            break;
        }
        }

        m_info->exports.append({ WTFMove(fieldString), kind, kindIndex });
    }

    return { };
}

auto SectionParser::parseStart() -> PartialResult
{
    uint32_t startFunctionIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(startFunctionIndex), "can't get Start index"_s);
    WASM_PARSER_FAIL_IF(startFunctionIndex >= m_info->functionIndexSpaceSize(), "Start index "_s, startFunctionIndex, " exceeds function index space "_s, m_info->functionIndexSpaceSize());
    TypeIndex typeIndex = m_info->typeIndexFromFunctionIndexSpace(startFunctionIndex);
    const auto& signature = TypeInformation::getFunctionSignature(typeIndex);
    WASM_PARSER_FAIL_IF(signature.argumentCount(), "Start function can't have arguments"_s);
    WASM_PARSER_FAIL_IF(!signature.returnsVoid(), "Start function can't return a value"_s);
    m_info->startFunctionIndexSpace = startFunctionIndex;
    return { };
}

auto SectionParser::parseElement() -> PartialResult
{
    uint32_t elementCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(elementCount), "can't get Element section's count"_s);
    WASM_PARSER_FAIL_IF(elementCount > maxTableEntries, "Element section's count is too big "_s, elementCount, " maximum "_s, maxTableEntries);
    RELEASE_ASSERT(!m_info->elements.capacity());
    WASM_PARSER_FAIL_IF(!m_info->elements.tryReserveInitialCapacity(elementCount), "can't allocate memory for "_s, elementCount, " Elements"_s);
    for (unsigned elementNum = 0; elementNum < elementCount; ++elementNum) {
        uint32_t elementFlags;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(elementFlags), "can't get "_s, elementNum, "th Element reserved byte, which should be element flags"_s);

        switch (elementFlags) {
        case 0x00: {
            constexpr uint32_t tableIndex = 0;
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex, funcrefType()));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, funcrefType(), tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x01: {
            uint8_t elementKind;
            WASM_FAIL_IF_HELPER_FAILS(parseElementKind(elementKind));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            Element element(Element::Kind::Passive, funcrefType());
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x02: {
            uint32_t tableIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get "_s, elementNum, "th Element table index"_s);
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex, funcrefType()));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            uint8_t elementKind;
            WASM_FAIL_IF_HELPER_FAILS(parseElementKind(elementKind));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, funcrefType(), tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x03: {
            uint8_t elementKind;
            WASM_FAIL_IF_HELPER_FAILS(parseElementKind(elementKind));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            Element element(Element::Kind::Declared, funcrefType());
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x04: {
            constexpr uint32_t tableIndex = 0;
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex, funcrefType()));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, funcrefType(), tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(funcrefType(), element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x05: {
            Type refType;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, refType), "can't parse reftype in elem section"_s);

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));

            Element element(Element::Kind::Passive, refType);
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(refType, element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x06: {
            uint32_t tableIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get "_s, elementNum, "th Element table index"_s);

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            Type refType;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, refType), "can't parse reftype in elem section"_s);
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex, refType));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, refType, tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(refType, element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        case 0x07: {
            Type refType;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, refType), "can't parse reftype in elem section"_s);

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));

            Element element(Element::Kind::Declared, refType);
            WASM_PARSER_FAIL_IF(!element.initTypes.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);
            WASM_PARSER_FAIL_IF(!element.initialBitsOrIndices.tryReserveInitialCapacity(indexCount), "can't allocate memory for "_s, indexCount, " Element init_exprs"_s);

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(refType, element.initTypes, element.initialBitsOrIndices, indexCount, elementNum));
            m_info->elements.append(WTFMove(element));
            break;
        }
        default:
            WASM_PARSER_FAIL_IF(true, "can't get "_s, elementNum, "th Element reserved byte"_s);
        }
    }

    return { };
}

auto SectionParser::parseCode() -> PartialResult
{
    // The Code section is handled specially in StreamingParser.
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

auto SectionParser::parseInitExpr(uint8_t& opcode, bool& isExtendedConstantExpression, uint64_t& bitsOrImportNumber, v128_t& vectorBits, Type expectedType, Type& resultType) -> PartialResult
{
    size_t initialOffset = m_offset;
    WASM_PARSER_FAIL_IF(!parseUInt8(opcode), "can't get init_expr's opcode"_s);

    switch (opcode) {
    case I32Const: {
        int32_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt32(constant), "can't get constant value for init_expr's i32.const"_s);
        bitsOrImportNumber = static_cast<uint64_t>(constant);
        resultType = Types::I32;
        break;
    }

    case I64Const: {
        int64_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt64(constant), "can't get constant value for init_expr's i64.const"_s);
        bitsOrImportNumber = constant;
        resultType = Types::I64;
        break;
    }

    case F32Const: {
        uint32_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt32(constant), "can't get constant value for init_expr's f32.const"_s);
        bitsOrImportNumber = constant;
        resultType = Types::F32;
        break;
    }

    case F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't get constant value for init_expr's f64.const"_s);
        bitsOrImportNumber = constant;
        resultType = Types::F64;
        break;
    }

#if ENABLE(B3_JIT)
    case ExtSIMD: {
        WASM_PARSER_FAIL_IF(!Options::useWasmSIMD(), "SIMD must be enabled"_s);
        WASM_PARSER_FAIL_IF(!parseUInt8(opcode), "can't get init_expr's simd opcode"_s);
        WASM_PARSER_FAIL_IF(static_cast<ExtSIMDOpType>(opcode) != ExtSIMDOpType::V128Const, "unknown init_expr simd opcode "_s, opcode);
        v128_t constant;
        WASM_PARSER_FAIL_IF(!parseImmByteArray16(constant), "get constant value for init_expr's v128.const"_s);

        vectorBits = constant;
        resultType = Types::V128;
        break;
    }
#else
    case ExtSIMD:
        WASM_PARSER_FAIL_IF(true, "wasm-simd is not supported"_s);
        (void) vectorBits;
        break;
#endif

    case GetGlobal: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get get_global's index"_s);

        WASM_PARSER_FAIL_IF(index >= m_info->globals.size(), "get_global's index "_s, index, " exceeds the number of globals "_s, m_info->globals.size());
        if (!Options::useWasmGC())
            WASM_PARSER_FAIL_IF(index >= m_info->firstInternalGlobal, "get_global import kind index "_s, index, " exceeds the first internal global "_s, m_info->firstInternalGlobal);
        WASM_PARSER_FAIL_IF(m_info->globals[index].mutability != Mutability::Immutable, "get_global import kind index "_s, index, " is mutable "_s);

        resultType = m_info->globals[index].type;
        bitsOrImportNumber = index;
        break;
    }

    case RefNull: {
        Type typeOfNull;
        int32_t heapType;
        WASM_PARSER_FAIL_IF(!parseHeapType(m_info, heapType), "ref.null heaptype must be funcref, externref or type_idx"_s);
        if (isTypeIndexHeapType(heapType)) {
            TypeIndex typeIndex = TypeInformation::get(m_info->typeSignatures[heapType].get());
            typeOfNull = Type { TypeKind::RefNull, typeIndex };
        } else
            typeOfNull = Type { TypeKind::RefNull, static_cast<TypeIndex>(heapType) };
        resultType = typeOfNull;
        bitsOrImportNumber = JSValue::encode(jsNull());
        break;
    }

    case RefFunc: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get ref.func index"_s);
        WASM_PARSER_FAIL_IF(index >= m_info->functionIndexSpaceSize(), "ref.func index "_s, index, " exceeds the number of functions "_s, m_info->functionIndexSpaceSize());
        m_info->addReferencedFunction(index);
        TypeIndex typeIndex = m_info->typeIndexFromFunctionIndexSpace(index);
        resultType = { TypeKind::Ref, typeIndex };
        bitsOrImportNumber = index;
        break;
    }

    case ExtGC:
        WASM_PARSER_FAIL_IF(!Options::useWasmGC(), "Wasm GC is not enabled"_s);
        break;

    default:
        WASM_PARSER_FAIL_IF(true, "unknown init_expr opcode "_s, opcode);
    }

    uint8_t endOpcode;
    // Don't consume the opcode byte unless it's an End so that the extended
    // parsing mode below can consume it if needed.
    WASM_PARSER_FAIL_IF(offset() >= source().size(), "can't get init_expr's end opcode"_s);
    endOpcode = source()[offset()];

    if (endOpcode == OpType::End && opcode != OpType::ExtGC) {
        m_offset++;
        isExtendedConstantExpression = false;
        return { };
    }

    // If an End doesn't appear, we have to assume it's an extended constant expression
    // and use the full Wasm expression parser to validate.
    size_t initExprOffset;
    WASM_FAIL_IF_HELPER_FAILS(parseExtendedConstExpr(source().subspan(initialOffset), initialOffset + m_offsetInSource, initExprOffset, m_info, expectedType));
    m_offset += (initExprOffset - (m_offset - initialOffset));
    WASM_PARSER_FAIL_IF(!m_info->constantExpressions.tryConstructAndAppend(source().subspan(initialOffset, initExprOffset)), "could not allocate memory for init expr"_s);
    bitsOrImportNumber = m_info->constantExpressions.size() - 1;
    isExtendedConstantExpression = true;
    resultType = expectedType;

    return { };
}

auto SectionParser::validateElementTableIdx(uint32_t tableIndex, Type type) -> PartialResult
{
    WASM_PARSER_FAIL_IF(tableIndex >= m_info->tableCount(), "Element section for Table "_s, tableIndex, " exceeds available Table "_s, m_info->tableCount());
    WASM_PARSER_FAIL_IF(!isSubtype(type, m_info->tables[tableIndex].wasmType()), "Table "_s, tableIndex, " must have type '"_s, type, "' to have an element section"_s);

    return { };
}

auto SectionParser::parseI32InitExpr(std::optional<I32InitExpr>& initExpr, ASCIILiteral failMessage) -> PartialResult
{
    uint8_t initOpcode;
    bool isExtendedConstantExpression;
    uint64_t initExprBits;
    Type initExprType;
    v128_t unused;
    WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, isExtendedConstantExpression, initExprBits, unused, Types::I32, initExprType));
    WASM_PARSER_FAIL_IF(!initExprType.isI32(), failMessage);
    initExpr = makeI32InitExpr(initOpcode, isExtendedConstantExpression, initExprBits);

    return { };
}

auto SectionParser::parseFunctionType(uint32_t position, RefPtr<TypeDefinition>& functionSignature) -> PartialResult
{
    uint32_t argumentCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(argumentCount), "can't get Type's argument count at index "_s, position);
    WASM_PARSER_FAIL_IF(argumentCount > maxFunctionParams, "argument count of Type at index "_s, position, " is too big "_s, argumentCount, " maximum "_s, maxFunctionParams);
    Vector<Type, 16> argumentTypes;
    WASM_PARSER_FAIL_IF(!argumentTypes.tryReserveInitialCapacity(argumentCount), "can't allocate enough memory for Type section's "_s, position, "th signature"_s);

    argumentTypes.resize(argumentCount);
    for (unsigned i = 0; i < argumentCount; ++i) {
        Type argumentType;
        WASM_PARSER_FAIL_IF(!parseValueType(m_info, argumentType), "can't get "_s, i, "th argument Type"_s);
        argumentTypes[i] = argumentType;
    }

    uint32_t returnCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(returnCount), "can't get Type's return count at index "_s, position);
    WASM_PARSER_FAIL_IF(returnCount > maxFunctionReturns, "return count of Type at index "_s, position, " is too big "_s, returnCount, " maximum "_s, maxFunctionReturns);

    Vector<Type, 16> returnTypes;
    WASM_PARSER_FAIL_IF(!returnTypes.tryReserveInitialCapacity(returnCount), "can't allocate enough memory for Type section's "_s, position, "th signature"_s);
    returnTypes.resize(returnCount);
    for (unsigned i = 0; i < returnCount; ++i) {
        Type value;
        WASM_PARSER_FAIL_IF(!parseValueType(m_info, value), "can't get "_s, i, "th Type's return value"_s);
        returnTypes[i] = value;
    }

    functionSignature = TypeInformation::typeDefinitionForFunction(returnTypes, argumentTypes);
    return { };
}

auto SectionParser::parsePackedType(PackedType& packedType) -> PartialResult
{
    int8_t kind;
    WASM_PARSER_FAIL_IF(!parseInt7(kind), "invalid type in struct field or array element"_s);
    if (isValidPackedType(kind)) {
        packedType = static_cast<PackedType>(kind);
        return { };
    }
    return fail("expected a packed type but got "_s, kind);
}

auto SectionParser::parseStorageType(StorageType& storageType) -> PartialResult
{
    ASSERT(Options::useWasmGC());

    int8_t kind;
    WASM_PARSER_FAIL_IF(!peekInt7(kind), "invalid type in struct field or array element"_s);
    if (isValidTypeKind(kind)) {
        Type elementType;
        WASM_PARSER_FAIL_IF(!parseValueType(m_info, elementType), "invalid type in struct field or array element"_s);
        storageType = StorageType { elementType };
        return { };
    }

    PackedType elementType;
    WASM_PARSER_FAIL_IF(!parsePackedType(elementType), "invalid type in struct field or array element"_s);
    storageType = StorageType { elementType };
    return { };
}

auto SectionParser::parseStructType(uint32_t position, RefPtr<TypeDefinition>& structType) -> PartialResult
{
    ASSERT(Options::useWasmGC());

    uint32_t fieldCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(fieldCount), "can't get "_s, position, "th struct type's field count"_s);
    WASM_PARSER_FAIL_IF(fieldCount > maxStructFieldCount, "number of fields for struct type at position "_s, position, " is too big "_s, fieldCount, " maximum "_s, maxStructFieldCount);
    Vector<FieldType> fields;
    WASM_PARSER_FAIL_IF(!fields.tryReserveInitialCapacity(fieldCount), "can't allocate enough memory for struct fields "_s, fieldCount, " entries"_s);
    fields.resize(fieldCount);

    Checked<unsigned, RecordOverflow> structInstancePayloadSize { 0 };
    for (uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
        StorageType fieldType;
        WASM_PARSER_FAIL_IF(!parseStorageType(fieldType), "can't get "_s, fieldIndex, "th field Type"_s);

        uint8_t mutability;
        WASM_PARSER_FAIL_IF(!parseUInt8(mutability), position, "can't get "_s, fieldIndex, "th field mutability"_s);
        WASM_PARSER_FAIL_IF(mutability != 0x0 && mutability != 0x1, "invalid Field's mutability: 0x"_s, hex(mutability, 2, Lowercase));

        fields[fieldIndex] = FieldType { fieldType, static_cast<Mutability>(mutability) };
        structInstancePayloadSize += typeSizeInBytes(fieldType);
        WASM_PARSER_FAIL_IF(structInstancePayloadSize.hasOverflowed(), "struct layout is too big"_s);
    }

    structType = TypeInformation::typeDefinitionForStruct(fields);
    return { };
}

auto SectionParser::parseArrayType(uint32_t position, RefPtr<TypeDefinition>& arrayType) -> PartialResult
{
    ASSERT(Options::useWasmGC());

    StorageType elementType;
    WASM_PARSER_FAIL_IF(!parseStorageType(elementType), "can't get array's element Type"_s);

    uint8_t mutability;
    WASM_PARSER_FAIL_IF(!parseUInt8(mutability), position, "can't get array's mutability"_s);
    WASM_PARSER_FAIL_IF(mutability != 0x0 && mutability != 0x1, "invalid array mutability: 0x"_s, hex(mutability, 2, Lowercase));

    arrayType = TypeInformation::typeDefinitionForArray(FieldType { elementType, static_cast<Mutability>(mutability) });
    return { };
}

auto SectionParser::parseRecursionGroup(uint32_t position, RefPtr<TypeDefinition>& recursionGroup) -> PartialResult
{
    ASSERT(Options::useWasmGC());

    uint32_t typeCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(typeCount), "can't get "_s, position, "th recursion group's type count"_s);
    WASM_PARSER_FAIL_IF(typeCount > maxRecursionGroupCount, "number of types for recursion group at position "_s, position, " is too big "_s, typeCount, " maximum "_s, maxRecursionGroupCount);
    Vector<TypeIndex> types;
    WASM_PARSER_FAIL_IF(!types.tryReserveInitialCapacity(typeCount), "can't allocate enough memory for recursion group "_s, typeCount, " entries"_s);
    RefPtr<TypeDefinition> firstSignature;

    SetForScope<RecursionGroupInformation> recursionGroupInfo(m_recursionGroupInformation, RecursionGroupInformation { true, m_info->typeCount(), m_info->typeCount() + typeCount });

    for (uint32_t i = 0; i < typeCount; ++i) {
        int8_t typeKind;
        WASM_PARSER_FAIL_IF(!parseInt7(typeKind), "can't get recursion group's "_s, i, "th Type's type"_s);
        RefPtr<TypeDefinition> signature;
        switch (static_cast<TypeKind>(typeKind)) {
        case TypeKind::Func: {
            WASM_FAIL_IF_HELPER_FAILS(parseFunctionType(i, signature));
            break;
        }
        case TypeKind::Struct: {
            WASM_FAIL_IF_HELPER_FAILS(parseStructType(i, signature));
            break;
        }
        case TypeKind::Array: {
            WASM_FAIL_IF_HELPER_FAILS(parseArrayType(i, signature));
            break;
        }
        case TypeKind::Sub:
        case TypeKind::Subfinal: {
            WASM_FAIL_IF_HELPER_FAILS(parseSubtype(i, signature, types, static_cast<TypeKind>(typeKind) == TypeKind::Subfinal));
            break;
        }
        default:
            return fail(i, "th Type is non-Func, non-Struct, and non-Array "_s, typeKind);
        }

        WASM_PARSER_FAIL_IF(!signature, "can't allocate enough memory for recursion group's "_s, i, "th signature"_s);
        types.append(signature->index());

        if (!i)
            firstSignature = signature;
    }

    recursionGroup = TypeInformation::typeDefinitionForRecursionGroup(types);

    // Type definitions are normalized such that non-recursive, singleton recursion groups
    // are stored as the underlying concrete type without a projection. Otherwise we will
    // store projections for each recursion group index in the type section.
    WASM_PARSER_FAIL_IF(!m_info->typeSignatures.tryGrowCapacityBy(typeCount), "can't allocate enough memory for recursion group's "_s, typeCount, " type "_s, typeCount > 1 ? "indices"_s : "index"_s);
    WASM_PARSER_FAIL_IF(!m_info->rtts.tryGrowCapacityBy(typeCount), "can't allocate enough memory for recursion group's "_s, typeCount, " RTT"_s, typeCount > 1 ? "s"_s : ""_s);
    if (typeCount > 1) {
        for (uint32_t i = 0; i < typeCount; ++i) {
            RefPtr<TypeDefinition> projection = TypeInformation::typeDefinitionForProjection(recursionGroup->index(), i);
            WASM_PARSER_FAIL_IF(!projection, "can't allocate enough memory for recursion group's "_s, i, "th projection"_s);
            TypeInformation::registerCanonicalRTTForType(projection->index());
            m_info->rtts.append(TypeInformation::getCanonicalRTT(projection->index()));
            m_info->typeSignatures.append(projection.releaseNonNull());
        }
        // Checking subtyping requirements has to be deferred until we construct projections in case recursive references show up in the type.
        for (uint32_t i = 0; i < typeCount; ++i) {
            const TypeDefinition& def = m_info->typeSignatures[i].get().unroll();
            if (def.is<Subtype>())
                WASM_FAIL_IF_HELPER_FAILS(checkSubtypeValidity(def));
        }
    } else if (typeCount) {
        if (!firstSignature->hasRecursiveReference()) {
            TypeInformation::registerCanonicalRTTForType(firstSignature->index());
            m_info->rtts.append(TypeInformation::getCanonicalRTT(firstSignature->index()));
            if (firstSignature->is<Subtype>())
                WASM_FAIL_IF_HELPER_FAILS(checkSubtypeValidity(*firstSignature));
            m_info->typeSignatures.append(firstSignature.releaseNonNull());
        } else {
            RefPtr<TypeDefinition> projection = TypeInformation::typeDefinitionForProjection(recursionGroup->index(), 0);
            WASM_PARSER_FAIL_IF(!projection, "can't allocate enough memory for recursion group's 0th projection"_s);
            TypeInformation::registerCanonicalRTTForType(projection->index());
            m_info->rtts.append(TypeInformation::getCanonicalRTT(projection->index()));
            if (firstSignature->is<Subtype>())
                WASM_FAIL_IF_HELPER_FAILS(checkSubtypeValidity(projection->unroll()));
            m_info->typeSignatures.append(projection.releaseNonNull());
        }
    }

    return { };
}

// Checks subtyping between a concrete structural type and a parent type definition,
// following the subtyping relation described in the GC proposal specification:
//
//   https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#structural-types
//
// The subtype argument should be unrolled before it is passed here, if needed.
bool SectionParser::checkStructuralSubtype(const TypeDefinition& subtype, const TypeDefinition& supertype)
{
    ASSERT(subtype.is<FunctionSignature>() || subtype.is<StructType>() || subtype.is<ArrayType>());
    const TypeDefinition& expanded = supertype.expand();
    ASSERT(expanded.is<FunctionSignature>() || expanded.is<StructType>() || expanded.is<ArrayType>());

    if (subtype.is<FunctionSignature>() && expanded.is<FunctionSignature>()) {
        const FunctionSignature& subFunc = *subtype.as<FunctionSignature>();
        const FunctionSignature& superFunc = *expanded.as<FunctionSignature>();
        if (subFunc.argumentCount() == superFunc.argumentCount() && subFunc.returnCount() == superFunc.returnCount()) {
            for (FunctionArgCount i = 0; i < subFunc.argumentCount(); ++i)  {
                if (!isSubtype(superFunc.argumentType(i), subFunc.argumentType(i)))
                    return false;
            }
            for (FunctionArgCount i = 0; i < subFunc.returnCount(); ++i)  {
                if (!isSubtype(subFunc.returnType(i), superFunc.returnType(i)))
                    return false;
            }
            return true;
        }
    } else if (subtype.is<StructType>() && expanded.is<StructType>()) {
        const StructType& subStruct = *subtype.as<StructType>();
        const StructType& superStruct = *expanded.as<StructType>();
        if (subStruct.fieldCount() >= superStruct.fieldCount()) {
            for (StructFieldCount i = 0; i < superStruct.fieldCount(); ++i)  {
                FieldType subField = subStruct.field(i);
                FieldType superField = superStruct.field(i);
                if (subField.mutability != superField.mutability)
                    return false;
                if (subField.mutability == Mutability::Mutable && subField.type != superField.type)
                    return false;
                if (subField.mutability == Mutability::Immutable && !isSubtype(subField.type, superField.type))
                    return false;
            }
            return true;
        }
    } else if (subtype.is<ArrayType>() && expanded.is<ArrayType>()) {
        FieldType subField = subtype.as<ArrayType>()->elementType();
        FieldType superField = expanded.as<ArrayType>()->elementType();
        if (subField.mutability != superField.mutability)
            return false;
        if (subField.mutability == Mutability::Mutable && subField.type != superField.type)
            return false;
        if (subField.mutability == Mutability::Immutable && !isSubtype(subField.type, superField.type))
            return false;
        return true;
    }

    return false;
}

auto SectionParser::checkSubtypeValidity(const TypeDefinition& subtype) -> PartialResult
{
    ASSERT(subtype.is<Subtype>());

    if (subtype.as<Subtype>()->supertypeCount() < 1)
        return { };
    TypeIndex superIndex = subtype.as<Subtype>()->firstSuperType();

    const auto& superSignature = TypeInformation::get(superIndex);
    ASSERT(!superSignature.is<Projection>() || !superSignature.as<Projection>()->isPlaceholder());

    const TypeDefinition& supertype = superSignature.unroll();
    WASM_PARSER_FAIL_IF(!supertype.is<Subtype>() || supertype.as<Subtype>()->isFinal(), "cannot declare subtype of final supertype"_s);
    WASM_PARSER_FAIL_IF(!checkStructuralSubtype(TypeInformation::get(subtype.as<Subtype>()->underlyingType()), supertype), "structural type is not a subtype of the specified supertype"_s);

    return { };
}

auto SectionParser::parseSubtype(uint32_t position, RefPtr<TypeDefinition>& subtype, Vector<TypeIndex>& recursionGroupTypes, bool isFinal) -> PartialResult
{
    ASSERT(Options::useWasmGC());

    uint32_t supertypeCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(supertypeCount), "can't get "_s, position, "th subtype's supertype count"_s);
    WASM_PARSER_FAIL_IF(supertypeCount > maxSubtypeSupertypeCount, "number of supertypes for subtype at position "_s, position, " is too big "_s, supertypeCount, " maximum "_s, maxSubtypeSupertypeCount);

    // The following assumes the MVP restriction that only up to a single supertype is allowed.
    TypeIndex supertypeIndex = TypeDefinition::invalidIndex;
    if (supertypeCount > 0) {
        uint32_t typeIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get subtype's supertype index"_s);
        WASM_PARSER_FAIL_IF(typeIndex >= m_info->typeCount() + recursionGroupTypes.size(), "supertype index is a forward reference"_s);
        if (typeIndex < m_info->typeCount())
            supertypeIndex = TypeInformation::get(m_info->typeSignatures[typeIndex]);
        // If a parent type is in the same recursion group, the index needs to refer to the projection instead.
        else {
            RefPtr<TypeDefinition> projection = TypeInformation::getPlaceholderProjection(typeIndex - m_info->typeCount());
            supertypeIndex = projection->index();
        }
    }

    int8_t typeKind;
    WASM_PARSER_FAIL_IF(!parseInt7(typeKind), "can't get subtype's underlying Type's type"_s);
    RefPtr<TypeDefinition> underlyingType;
    switch (static_cast<TypeKind>(typeKind)) {
    case TypeKind::Func: {
        WASM_FAIL_IF_HELPER_FAILS(parseFunctionType(position, underlyingType));
        break;
    }
    case TypeKind::Struct: {
        WASM_FAIL_IF_HELPER_FAILS(parseStructType(position, underlyingType));
        break;
    }
    case TypeKind::Array: {
        WASM_FAIL_IF_HELPER_FAILS(parseArrayType(position, underlyingType));
        break;
    }
    default:
        return fail("invalid structural type definition for subtype "_s, typeKind);
    }

    // When no supertypes are specified and the type is final, we will normalize
    // type definitions to not have the subtype. This ensures that type shorthands
    // and the full subtype form are represented in the same way.
    if (!supertypeCount && isFinal) {
        subtype = underlyingType;
        return { };
    }

    if (supertypeCount > 0)
        subtype = TypeInformation::typeDefinitionForSubtype({ supertypeIndex }, TypeInformation::get(*underlyingType), isFinal);
    else
        subtype = TypeInformation::typeDefinitionForSubtype({ }, TypeInformation::get(*underlyingType), isFinal);

    return { };
}

auto SectionParser::parseI32InitExprForElementSection(std::optional<I32InitExpr>& initExpr) -> PartialResult
{
    return parseI32InitExpr(initExpr, "Element init_expr must produce an i32"_s);
}

auto SectionParser::parseElementKind(uint8_t& resultElementKind) -> PartialResult
{
    uint8_t elementKind;
    WASM_PARSER_FAIL_IF(!parseUInt8(elementKind), "can't get element kind"_s);
    WASM_PARSER_FAIL_IF(!!elementKind, "element kind must be zero"_s);
    resultElementKind = elementKind;

    return { };
}

auto SectionParser::parseIndexCountForElementSection(uint32_t& resultIndexCount, const unsigned elementNum) -> PartialResult
{
    uint32_t indexCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(indexCount), "can't get "_s, elementNum, "th index count for Element section"_s);
    WASM_PARSER_FAIL_IF(indexCount == std::numeric_limits<uint32_t>::max(), "Element section's "_s, elementNum, "th index count is too big "_s, indexCount);
    resultIndexCount = indexCount;

    return { };
}

auto SectionParser::parseElementSegmentVectorOfExpressions(Type elementType, Vector<Element::InitializationType>& initTypes, Vector<uint64_t>& initialBitsOrIndices, const unsigned indexCount, const unsigned elementNum) -> PartialResult
{
    for (uint32_t index = 0; index < indexCount; ++index) {
        Element::InitializationType initType;
        uint64_t initialBitsOrIndex;

        uint8_t initOpcode;
        Type typeForInitOpcode;
        bool isExtendedConstantExpression;
        v128_t unusedVector { };
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, isExtendedConstantExpression, initialBitsOrIndex, unusedVector, elementType, typeForInitOpcode));
        WASM_PARSER_FAIL_IF(!isSubtype(typeForInitOpcode, elementType), "Element section's "_s, elementNum, "th element's init_expr opcode of type "_s, typeForInitOpcode.kind, " doesn't match element's type "_s, elementType.kind);

        if (isExtendedConstantExpression)
            initType = Element::InitializationType::FromExtendedExpression;
        else if (initOpcode == GetGlobal)
            initType = Element::InitializationType::FromGlobal;
        else if (initOpcode == RefFunc) {
            initType = Element::InitializationType::FromRefFunc;
            m_info->addDeclaredFunction(initialBitsOrIndex);
        } else if (initOpcode == RefNull)
            initType = Element::InitializationType::FromRefNull;
        else
            RELEASE_ASSERT_NOT_REACHED();

        initTypes.append(initType);
        initialBitsOrIndices.append(initialBitsOrIndex);
    }

    return { };
}

auto SectionParser::parseElementSegmentVectorOfIndexes(Vector<Element::InitializationType>& initTypes, Vector<uint64_t>& initialBitsOrIndices, const unsigned indexCount, const unsigned elementNum) -> PartialResult
{
    for (uint32_t index = 0; index < indexCount; ++index) {
        uint32_t functionIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't get Element section's "_s, elementNum, "th element's "_s, index, "th index"_s);
        WASM_PARSER_FAIL_IF(functionIndex >= m_info->functionIndexSpaceSize(), "Element section's "_s, elementNum, "th element's "_s, index, "th index is "_s, functionIndex, " which exceeds the function index space size of "_s, m_info->functionIndexSpaceSize());

        m_info->addDeclaredFunction(functionIndex);
        initTypes.append(Element::InitializationType::FromRefFunc);
        initialBitsOrIndices.append(functionIndex);
    }

    return { };
}

auto SectionParser::parseI32InitExprForDataSection(std::optional<I32InitExpr>& initExpr) -> PartialResult
{
    return parseI32InitExpr(initExpr, "Data init_expr must produce an i32"_s);
}

auto SectionParser::parseGlobalType(GlobalInformation& global) -> PartialResult
{
    uint8_t mutability;
    WASM_PARSER_FAIL_IF(!parseValueType(m_info, global.type), "can't get Global's value type"_s);
    WASM_PARSER_FAIL_IF(!parseUInt8(mutability), "can't get Global type's mutability"_s);
    WASM_PARSER_FAIL_IF(mutability != 0x0 && mutability != 0x1, "invalid Global's mutability: 0x"_s, hex(mutability, 2, Lowercase));
    global.mutability = static_cast<Mutability>(mutability);
    return { };
}

auto SectionParser::parseData() -> PartialResult
{
    uint32_t segmentCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(segmentCount), "can't get Data section's count"_s);
    WASM_PARSER_FAIL_IF(segmentCount > maxDataSegments, "Data section's count is too big "_s, segmentCount, " maximum "_s, maxDataSegments);
    if (m_info->numberOfDataSegments)
        WASM_PARSER_FAIL_IF(segmentCount != m_info->numberOfDataSegments.value(), "Data section's count "_s, segmentCount, " is different from Data Count section's count "_s, m_info->numberOfDataSegments.value());
    RELEASE_ASSERT(!m_info->data.capacity());
    WASM_PARSER_FAIL_IF(!m_info->data.tryReserveInitialCapacity(segmentCount), "can't allocate enough memory for Data section's "_s, segmentCount, " segments"_s);

    for (uint32_t segmentNumber = 0; segmentNumber < segmentCount; ++segmentNumber) {
        uint32_t memoryIndexOrDataFlag = UINT32_MAX;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(memoryIndexOrDataFlag), "can't get "_s, segmentNumber, "th Data segment's flag"_s);

        if (!memoryIndexOrDataFlag) {
            const uint32_t memoryIndex = memoryIndexOrDataFlag;
            WASM_PARSER_FAIL_IF(memoryIndex >= m_info->memoryCount(), segmentNumber, "th Data segment has index "_s, memoryIndex, " which exceeds the number of Memories "_s, m_info->memoryCount());

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForDataSection(initExpr));

            uint32_t dataByteLength;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get "_s, segmentNumber, "th Data segment's data byte length"_s);
            WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big "_s, dataByteLength, " maximum "_s, maxModuleSize);

            auto segment = Segment::create(*initExpr, dataByteLength, Segment::Kind::Active);
            WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for "_s, segmentNumber, "th Data segment of size "_s, dataByteLength);
            for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
                uint8_t byte;
                WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get "_s, dataByte, "th data byte from "_s, segmentNumber, "th Data segment"_s);
                segment->byte(dataByte) = byte;
            }
            m_info->data.append(WTFMove(segment));
            continue;
        }

        const uint32_t dataFlag = memoryIndexOrDataFlag;
        if (dataFlag == 0x01) {
            uint32_t dataByteLength;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get "_s, segmentNumber, "th Data segment's data byte length"_s);
            WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big "_s, dataByteLength, " maximum "_s, maxModuleSize);

            auto segment = Segment::create(std::nullopt, dataByteLength, Segment::Kind::Passive);
            WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for "_s, segmentNumber, "th Data segment of size "_s, dataByteLength);
            for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
                uint8_t byte;
                WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get "_s, dataByte, "th data byte from "_s, segmentNumber, "th Data segment"_s);
                segment->byte(dataByte) = byte;
            }
            m_info->data.append(WTFMove(segment));
            continue;

        }

        if (dataFlag == 0x02) {
            uint32_t memoryIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(memoryIndex), "can't get "_s, segmentNumber, "th Data segment's index"_s);
            WASM_PARSER_FAIL_IF(memoryIndex >= m_info->memoryCount(), segmentNumber, "th Data segment has index "_s, memoryIndex, " which exceeds the number of Memories "_s, m_info->memoryCount());

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForDataSection(initExpr));

            uint32_t dataByteLength;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get "_s, segmentNumber, "th Data segment's data byte length"_s);
            WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big "_s, dataByteLength, " maximum "_s, maxModuleSize);

            auto segment = Segment::create(*initExpr, dataByteLength, Segment::Kind::Active);
            WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for "_s, segmentNumber, "th Data segment of size "_s, dataByteLength);
            for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
                uint8_t byte;
                WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get "_s, dataByte, "th data byte from "_s, segmentNumber, "th Data segment"_s);
                segment->byte(dataByte) = byte;
            }
            m_info->data.append(WTFMove(segment));
            continue;
        }

        WASM_PARSER_FAIL_IF(true, "unknown "_s, segmentNumber, "th Data segment's flag"_s);
    }

    return { };
}

auto SectionParser::parseDataCount() -> PartialResult
{
    uint32_t numberOfDataSegments;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfDataSegments), "can't get Data Count section's count"_s);
    WASM_PARSER_FAIL_IF(numberOfDataSegments > maxDataSegments, "Data Count section's count is too big "_s, numberOfDataSegments , " maximum "_s, maxDataSegments);

    m_info->numberOfDataSegments = numberOfDataSegments;
    return { };
}

auto SectionParser::parseException() -> PartialResult
{
    uint32_t exceptionCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(exceptionCount), "can't get Exception section's count"_s);
    WASM_PARSER_FAIL_IF(exceptionCount > maxExceptions, "Export section's count is too big "_s, exceptionCount, " maximum "_s, maxExceptions);
    RELEASE_ASSERT(!m_info->internalExceptionTypeIndices.capacity());
    WASM_PARSER_FAIL_IF(!m_info->internalExceptionTypeIndices.tryReserveInitialCapacity(exceptionCount), "can't allocate enough memory for "_s, exceptionCount, " exceptions"_s);

    for (uint32_t exceptionNumber = 0; exceptionNumber < exceptionCount; ++exceptionNumber) {
        uint8_t tagType;
        WASM_PARSER_FAIL_IF(!parseUInt8(tagType), "can't get "_s, exceptionNumber, "th Exception tag type"_s);
        WASM_PARSER_FAIL_IF(tagType, exceptionNumber, "th Exception has tag type "_s, tagType, " but the only supported tag type is 0"_s);

        uint32_t typeNumber;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(typeNumber), "can't get "_s, exceptionNumber, "th Exception's type number"_s);
        WASM_PARSER_FAIL_IF(typeNumber >= m_info->typeCount(), exceptionNumber, "th Exception type number is invalid "_s, typeNumber);
        TypeIndex typeIndex = TypeInformation::get(m_info->typeSignatures[typeNumber]);
        auto signature = TypeInformation::getFunctionSignature(typeIndex);
        WASM_PARSER_FAIL_IF(!signature.returnsVoid(), exceptionNumber, "th Exception type cannot have a non-void return type "_s, typeNumber);
        m_info->internalExceptionTypeIndices.append(typeIndex);
    }

    return { };
}
auto SectionParser::parseCustom() -> PartialResult
{
    CustomSection section;
    uint32_t customSectionNumber = m_info->customSections.size() + 1;
    uint32_t nameLen;
    WASM_PARSER_FAIL_IF(!m_info->customSections.tryReserveCapacity(customSectionNumber), "can't allocate enough memory for "_s, customSectionNumber, "th custom section"_s);
    WASM_PARSER_FAIL_IF(!parseVarUInt32(nameLen), "can't get "_s, customSectionNumber, "th custom section's name length"_s);
    WASM_PARSER_FAIL_IF(!consumeUTF8String(section.name, nameLen), "nameLen get "_s, customSectionNumber, "th custom section's name of length "_s, nameLen);

    uint32_t payloadBytes = source().size() - m_offset;
    WASM_PARSER_FAIL_IF(!section.payload.tryReserveInitialCapacity(payloadBytes), "can't allocate enough memory for "_s, customSectionNumber, "th custom section's "_s, payloadBytes, " bytes"_s);
    section.payload.resize(payloadBytes);
    for (uint32_t byteNumber = 0; byteNumber < payloadBytes; ++byteNumber) {
        uint8_t byte;
        WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get "_s, byteNumber, "th data byte from "_s, customSectionNumber, "th custom section"_s);
        section.payload[byteNumber] = byte;
    }

    if (WTF::Unicode::equal("name"_span, section.name.span())) {
        NameSectionParser nameSectionParser(section.payload, m_info);
        auto nameSection = nameSectionParser.parse();
        if (nameSection)
            m_info->nameSection = WTFMove(*nameSection);
        else
            dataLogLnIf(Options::dumpWasmWarnings(), "Could not parse name section: ", nameSection.error());
    } else if (WTF::Unicode::equal("metadata.code.branch_hint"_span, section.name.span())) {
        BranchHintsSectionParser branchHintsSectionParser(section.payload, m_info);
        branchHintsSectionParser.parse();
    } else if (WTF::Unicode::equal("sourceMappingURL"_span, section.name.span())) {
        SourceMappingURLSectionParser sourceMappingURLSectionParser(section.payload, m_info);
        sourceMappingURLSectionParser.parse();
    }

    m_info->customSections.append(WTFMove(section));

    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
