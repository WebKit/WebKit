/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "WasmMemoryInformation.h"
#include "WasmNameSectionParser.h"
#include "WasmOps.h"
#include "WasmSignatureInlines.h"
#include <wtf/HexNumber.h>

namespace JSC { namespace Wasm {

auto SectionParser::parseType() -> PartialResult
{
    uint32_t count;

    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Type section's count");
    WASM_PARSER_FAIL_IF(count > maxTypes, "Type section's count is too big ", count, " maximum ", maxTypes);
    WASM_PARSER_FAIL_IF(!m_info->usedSignatures.tryReserveCapacity(count), "can't allocate enough memory for Type section's ", count, " entries");

    for (uint32_t i = 0; i < count; ++i) {
        int8_t type;
        uint32_t argumentCount;
        Vector<Type> argumentTypes;

        WASM_PARSER_FAIL_IF(!parseInt7(type), "can't get ", i, "th Type's type");
        WASM_PARSER_FAIL_IF(type != static_cast<int8_t>(TypeKind::Func), i, "th Type is non-Func ", type);
        WASM_PARSER_FAIL_IF(!parseVarUInt32(argumentCount), "can't get ", i, "th Type's argument count");
        WASM_PARSER_FAIL_IF(argumentCount > maxFunctionParams, i, "th argument count is too big ", argumentCount, " maximum ", maxFunctionParams);
        Vector<Type> arguments;
        WASM_PARSER_FAIL_IF(!arguments.tryReserveCapacity(argumentCount), "can't allocate enough memory for Type section's ", i, "th signature");

        for (unsigned i = 0; i < argumentCount; ++i) {
            Type argumentType;
            WASM_PARSER_FAIL_IF(!parseValueType(m_info, argumentType), "can't get ", i, "th argument Type");
            arguments.append(argumentType);
        }

        uint32_t returnCount;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(returnCount), "can't get ", i, "th Type's return count");

        Vector<Type, 1> returnTypes;
        WASM_PARSER_FAIL_IF(!returnTypes.tryReserveCapacity(argumentCount), "can't allocate enough memory for Type section's ", i, "th signature");
        for (unsigned i = 0; i < returnCount; ++i) {
            Type value;
            WASM_PARSER_FAIL_IF(!parseValueType(m_info, value), "can't get ", i, "th Type's return value");
            returnTypes.append(value);
        }

        RefPtr<Signature> signature = SignatureInformation::signatureFor(returnTypes, arguments);
        WASM_PARSER_FAIL_IF(!signature, "can't allocate enough memory for Type section's ", i, "th signature");

        m_info->usedSignatures.uncheckedAppend(signature.releaseNonNull());
    }
    return { };
}

auto SectionParser::parseImport() -> PartialResult
{
    uint32_t importCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(importCount), "can't get Import section's count");
    WASM_PARSER_FAIL_IF(importCount > maxImports, "Import section's count is too big ", importCount, " maximum ", maxImports);
    WASM_PARSER_FAIL_IF(!m_info->globals.tryReserveCapacity(importCount), "can't allocate enough memory for ", importCount, " globals"); // FIXME this over-allocates when we fix the FIXMEs below.
    WASM_PARSER_FAIL_IF(!m_info->imports.tryReserveCapacity(importCount), "can't allocate enough memory for ", importCount, " imports"); // FIXME this over-allocates when we fix the FIXMEs below.
    WASM_PARSER_FAIL_IF(!m_info->importFunctionSignatureIndices.tryReserveCapacity(importCount), "can't allocate enough memory for ", importCount, " import function signatures"); // FIXME this over-allocates when we fix the FIXMEs below.
    if (Options::useWebAssemblyExceptions())
        WASM_PARSER_FAIL_IF(!m_info->importExceptionSignatureIndices.tryReserveCapacity(importCount), "can't allocate enough memory for ", importCount, " import exception signatures"); // FIXME this over-allocates when we fix the FIXMEs below.

    for (uint32_t importNumber = 0; importNumber < importCount; ++importNumber) {
        uint32_t moduleLen;
        uint32_t fieldLen;
        Name moduleString;
        Name fieldString;
        ExternalKind kind;
        unsigned kindIndex { 0 };

        WASM_PARSER_FAIL_IF(!parseVarUInt32(moduleLen), "can't get ", importNumber, "th Import's module name length");
        WASM_PARSER_FAIL_IF(!consumeUTF8String(moduleString, moduleLen), "can't get ", importNumber, "th Import's module name of length ", moduleLen);

        WASM_PARSER_FAIL_IF(!parseVarUInt32(fieldLen), "can't get ", importNumber, "th Import's field name length in module '", moduleString, "'");
        WASM_PARSER_FAIL_IF(!consumeUTF8String(fieldString, fieldLen), "can't get ", importNumber, "th Import's field name of length ", moduleLen, " in module '", moduleString, "'");

        WASM_PARSER_FAIL_IF(!parseExternalKind(kind), "can't get ", importNumber, "th Import's kind in module '", moduleString, "' field '", fieldString, "'");
        switch (kind) {
        case ExternalKind::Function: {
            uint32_t functionSignatureIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(functionSignatureIndex), "can't get ", importNumber, "th Import's function signature in module '", moduleString, "' field '", fieldString, "'");
            WASM_PARSER_FAIL_IF(functionSignatureIndex >= m_info->usedSignatures.size(), "invalid function signature for ", importNumber, "th Import, ", functionSignatureIndex, " is out of range of ", m_info->usedSignatures.size(), " in module '", moduleString, "' field '", fieldString, "'");
            kindIndex = m_info->importFunctionSignatureIndices.size();
            SignatureIndex signatureIndex = SignatureInformation::get(m_info->usedSignatures[functionSignatureIndex]);
            m_info->importFunctionSignatureIndices.uncheckedAppend(signatureIndex);
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
            if (global.mutability == GlobalInformation::Mutability::Mutable)
                global.bindingMode = GlobalInformation::BindingMode::Portable;
            kindIndex = m_info->globals.size();
            m_info->globals.uncheckedAppend(WTFMove(global));
            break;
        }
        case ExternalKind::Exception: {
            WASM_PARSER_FAIL_IF(!Options::useWebAssemblyExceptions(), "wasm exceptions are not enabled");

            uint8_t tagType;
            WASM_PARSER_FAIL_IF(!parseUInt8(tagType), "can't get ", importNumber, "th Import exception's tag type");
            WASM_PARSER_FAIL_IF(tagType, importNumber, "th Import exception has tag type ", tagType, " but the only supported tag type is 0");

            uint32_t exceptionSignatureIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(exceptionSignatureIndex), "can't get ", importNumber, "th Import's exception signature in module '", moduleString, "' field '", fieldString, "'");
            WASM_PARSER_FAIL_IF(exceptionSignatureIndex >= m_info->usedSignatures.size(), "invalid exception signature for ", importNumber, "th Import, ", exceptionSignatureIndex, " is out of range of ", m_info->usedSignatures.size(), " in module '", moduleString, "' field '", fieldString, "'");
            kindIndex = m_info->importExceptionSignatureIndices.size();
            SignatureIndex signatureIndex = SignatureInformation::get(m_info->usedSignatures[exceptionSignatureIndex]);
            m_info->importExceptionSignatureIndices.uncheckedAppend(signatureIndex);
            break;
        }
        }

        m_info->imports.uncheckedAppend({ WTFMove(moduleString), WTFMove(fieldString), kind, kindIndex });
    }

    m_info->firstInternalGlobal = m_info->globals.size();
    return { };
}

auto SectionParser::parseFunction() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Function section's count");
    WASM_PARSER_FAIL_IF(count > maxFunctions, "Function section's count is too big ", count, " maximum ", maxFunctions);
    WASM_PARSER_FAIL_IF(!m_info->internalFunctionSignatureIndices.tryReserveCapacity(count), "can't allocate enough memory for ", count, " Function signatures");
    WASM_PARSER_FAIL_IF(!m_info->functions.tryReserveCapacity(count), "can't allocate enough memory for ", count, "Function locations");

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t typeNumber;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(typeNumber), "can't get ", i, "th Function's type number");
        WASM_PARSER_FAIL_IF(typeNumber >= m_info->usedSignatures.size(), i, "th Function type number is invalid ", typeNumber);

        SignatureIndex signatureIndex = SignatureInformation::get(m_info->usedSignatures[typeNumber]);
        // The Code section fixes up start and end.
        size_t start = 0;
        size_t end = 0;
        m_info->internalFunctionSignatureIndices.uncheckedAppend(signatureIndex);
        m_info->functions.uncheckedAppend({ start, end, Vector<uint8_t>() });
    }

    return { };
}

auto SectionParser::parseResizableLimits(uint32_t& initial, std::optional<uint32_t>& maximum, bool& isShared, LimitsType limitsType) -> PartialResult
{
    ASSERT(!maximum);

    uint8_t flags;
    WASM_PARSER_FAIL_IF(!parseUInt8(flags), "can't parse resizable limits flags");
    WASM_PARSER_FAIL_IF(flags != 0x0 && flags != 0x1 && flags != 0x3, "resizable limits flag should be 0x00, 0x01, or 0x03 but 0x", hex(flags, 2, Lowercase));
    WASM_PARSER_FAIL_IF(flags == 0x3 && limitsType != LimitsType::Memory, "can't use shared limits for non memory");
    WASM_PARSER_FAIL_IF(!parseVarUInt32(initial), "can't parse resizable limits initial page count");

    isShared = flags == 0x3;
    WASM_PARSER_FAIL_IF(isShared && !Options::useSharedArrayBuffer(), "shared memory is not enabled");

    if (flags) {
        uint32_t maximumInt;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(maximumInt), "can't parse resizable limits maximum page count");
        WASM_PARSER_FAIL_IF(initial > maximumInt, "resizable limits has an initial page count of ", initial, " which is greater than its maximum ", maximumInt);
        maximum = maximumInt;
    }

    return { };
}

auto SectionParser::parseTableHelper(bool isImport) -> PartialResult
{
    WASM_PARSER_FAIL_IF(m_info->tableCount() >= maxTables, "Table count of ", m_info->tableCount(), " is too big, maximum ", maxTables);

    int8_t type;
    WASM_PARSER_FAIL_IF(!parseInt7(type), "can't parse Table type");
    WASM_PARSER_FAIL_IF(type != static_cast<int8_t>(TypeKind::Funcref) && type != static_cast<int8_t>(TypeKind::Externref), "Table type should be funcref or anyref, got ", type);

    uint32_t initial;
    std::optional<uint32_t> maximum;
    bool isShared = false;
    PartialResult limits = parseResizableLimits(initial, maximum, isShared, LimitsType::Table);
    if (UNLIKELY(!limits))
        return makeUnexpected(WTFMove(limits.error()));
    WASM_PARSER_FAIL_IF(initial > maxTableEntries, "Table's initial page count of ", initial, " is too big, maximum ", maxTableEntries);

    ASSERT(!maximum || *maximum >= initial);

    TableElementType tableType = type == static_cast<int8_t>(TypeKind::Funcref) ? TableElementType::Funcref : TableElementType::Externref;
    m_info->tables.append(TableInformation(initial, maximum, isImport, tableType));

    return { };
}

auto SectionParser::parseTable() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Table's count");

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
    WASM_PARSER_FAIL_IF(m_info->memoryCount(), "there can at most be one Memory section for now");

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
        WASM_PARSER_FAIL_IF(!PageCount::isValid(initial), "Memory's initial page count of ", initial, " is invalid");

        initialPageCount = PageCount(initial);

        if (maximum) {
            WASM_PARSER_FAIL_IF(!PageCount::isValid(*maximum), "Memory's maximum page count of ", *maximum, " is invalid");
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
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't parse Memory section's count");

    if (!count)
        return { };

    WASM_PARSER_FAIL_IF(count != 1, "Memory section has more than one memory, WebAssembly currently only allows zero or one");

    bool isImport = false;
    return parseMemoryHelper(isImport);
}

auto SectionParser::parseGlobal() -> PartialResult
{
    uint32_t globalCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(globalCount), "can't get Global section's count");
    WASM_PARSER_FAIL_IF(globalCount > maxGlobals, "Global section's count is too big ", globalCount, " maximum ", maxGlobals);
    size_t totalBytes = globalCount + m_info->firstInternalGlobal;
    WASM_PARSER_FAIL_IF((static_cast<uint32_t>(totalBytes) < globalCount) || !m_info->globals.tryReserveCapacity(totalBytes), "can't allocate memory for ", totalBytes, " globals");

    for (uint32_t globalIndex = 0; globalIndex < globalCount; ++globalIndex) {
        GlobalInformation global;
        uint8_t initOpcode;

        WASM_FAIL_IF_HELPER_FAILS(parseGlobalType(global));
        Type typeForInitOpcode;
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, global.initialBitsOrImportNumber, typeForInitOpcode));
        if (initOpcode == GetGlobal)
            global.initializationType = GlobalInformation::FromGlobalImport;
        else if (initOpcode == RefFunc)
            global.initializationType = GlobalInformation::FromRefFunc;
        else
            global.initializationType = GlobalInformation::FromExpression;
        WASM_PARSER_FAIL_IF(!isSubtype(typeForInitOpcode, global.type), "Global init_expr opcode of type ", typeForInitOpcode.kind, " doesn't match global's type ", global.type.kind);

        if (initOpcode == RefFunc)
            m_info->addDeclaredFunction(global.initialBitsOrImportNumber);

        m_info->globals.uncheckedAppend(WTFMove(global));
    }

    return { };
}

auto SectionParser::parseExport() -> PartialResult
{
    uint32_t exportCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(exportCount), "can't get Export section's count");
    WASM_PARSER_FAIL_IF(exportCount > maxExports, "Export section's count is too big ", exportCount, " maximum ", maxExports);
    WASM_PARSER_FAIL_IF(!m_info->exports.tryReserveCapacity(exportCount), "can't allocate enough memory for ", exportCount, " exports");

    HashSet<String> exportNames;
    for (uint32_t exportNumber = 0; exportNumber < exportCount; ++exportNumber) {
        uint32_t fieldLen;
        Name fieldString;
        ExternalKind kind;
        unsigned kindIndex;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(fieldLen), "can't get ", exportNumber, "th Export's field name length");
        WASM_PARSER_FAIL_IF(!consumeUTF8String(fieldString, fieldLen), "can't get ", exportNumber, "th Export's field name of length ", fieldLen);
        String fieldName = String::fromUTF8(fieldString);
        WASM_PARSER_FAIL_IF(exportNames.contains(fieldName), "duplicate export: '", fieldString, "'");
        exportNames.add(fieldName);

        WASM_PARSER_FAIL_IF(!parseExternalKind(kind), "can't get ", exportNumber, "th Export's kind, named '", fieldString, "'");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(kindIndex), "can't get ", exportNumber, "th Export's kind index, named '", fieldString, "'");
        switch (kind) {
        case ExternalKind::Function: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->functionIndexSpaceSize(), exportNumber, "th Export has invalid function number ", kindIndex, " it exceeds the function index space ", m_info->functionIndexSpaceSize(), ", named '", fieldString, "'");
            m_info->addDeclaredFunction(kindIndex);
            break;
        }
        case ExternalKind::Table: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->tableCount(), "can't export Table ", kindIndex, " there are ", m_info->tableCount(), " Tables");
            break;
        }
        case ExternalKind::Memory: {
            WASM_PARSER_FAIL_IF(!m_info->memory, "can't export a non-existent Memory");
            WASM_PARSER_FAIL_IF(kindIndex, "can't export Memory ", kindIndex, " only one Table is currently supported");
            break;
        }
        case ExternalKind::Global: {
            WASM_PARSER_FAIL_IF(kindIndex >= m_info->globals.size(), exportNumber, "th Export has invalid global number ", kindIndex, " it exceeds the globals count ", m_info->globals.size(), ", named '", fieldString, "'");
            // Only mutable globals need floating bindings.
            GlobalInformation& global = m_info->globals[kindIndex];
            if (global.mutability == GlobalInformation::Mutability::Mutable)
                global.bindingMode = GlobalInformation::BindingMode::Portable;
            break;
        }
        case ExternalKind::Exception: {
            WASM_PARSER_FAIL_IF(!Options::useWebAssemblyExceptions(), "wasm exceptions are not enabled");

            WASM_PARSER_FAIL_IF(kindIndex >= m_info->exceptionIndexSpaceSize(), exportNumber, "th Export has invalid exception number ", kindIndex, " it exceeds the exception index space ", m_info->exceptionIndexSpaceSize(), ", named '", fieldString, "'");
            m_info->addDeclaredException(kindIndex);
            break;
        }
        }

        m_info->exports.uncheckedAppend({ WTFMove(fieldString), kind, kindIndex });
    }

    return { };
}

auto SectionParser::parseStart() -> PartialResult
{
    uint32_t startFunctionIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(startFunctionIndex), "can't get Start index");
    WASM_PARSER_FAIL_IF(startFunctionIndex >= m_info->functionIndexSpaceSize(), "Start index ", startFunctionIndex, " exceeds function index space ", m_info->functionIndexSpaceSize());
    SignatureIndex signatureIndex = m_info->signatureIndexFromFunctionIndexSpace(startFunctionIndex);
    const Signature& signature = SignatureInformation::get(signatureIndex);
    WASM_PARSER_FAIL_IF(signature.argumentCount(), "Start function can't have arguments");
    WASM_PARSER_FAIL_IF(!signature.returnsVoid(), "Start function can't return a value");
    m_info->startFunctionIndexSpace = startFunctionIndex;
    return { };
}

auto SectionParser::parseElement() -> PartialResult
{
    uint32_t elementCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(elementCount), "can't get Element section's count");
    WASM_PARSER_FAIL_IF(elementCount > maxTableEntries, "Element section's count is too big ", elementCount, " maximum ", maxTableEntries);
    WASM_PARSER_FAIL_IF(!m_info->elements.tryReserveCapacity(elementCount), "can't allocate memory for ", elementCount, " Elements");
    for (unsigned elementNum = 0; elementNum < elementCount; ++elementNum) {
        uint8_t elementFlags;
        WASM_PARSER_FAIL_IF(!parseUInt8(elementFlags), "can't get ", elementNum, "th Element reserved byte, which should be element flags");

        switch (elementFlags) {
        case 0x00: {
            constexpr uint32_t tableIndex = 0;
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, TableElementType::Funcref, tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x01: {
            uint8_t elementKind;
            WASM_FAIL_IF_HELPER_FAILS(parseElementKind(elementKind));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            Element element(Element::Kind::Passive, TableElementType::Funcref);
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x02: {
            uint32_t tableIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get ", elementNum, "th Element table index");
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            uint8_t elementKind;
            WASM_FAIL_IF_HELPER_FAILS(parseElementKind(elementKind));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, TableElementType::Funcref, tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x03: {
            uint8_t elementKind;
            WASM_FAIL_IF_HELPER_FAILS(parseElementKind(elementKind));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            Element element(Element::Kind::Declared, TableElementType::Funcref);
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfIndexes(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x04: {
            constexpr uint32_t tableIndex = 0;
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, TableElementType::Funcref, tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x05: {
            Type refType;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, refType), "can't parse reftype in elem section");
            WASM_PARSER_FAIL_IF(!refType.isFuncref(), "reftype in element section should be funcref");

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));

            Element element(Element::Kind::Passive, TableElementType::Funcref);
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x06: {
            uint32_t tableIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get ", elementNum, "th Element table index");
            WASM_FAIL_IF_HELPER_FAILS(validateElementTableIdx(tableIndex));

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForElementSection(initExpr));

            Type refType;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, refType), "can't parse reftype in elem section");
            WASM_PARSER_FAIL_IF(!refType.isFuncref(), "reftype in element section should be funcref");

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));
            ASSERT(!!m_info->tables[tableIndex]);

            Element element(Element::Kind::Active, TableElementType::Funcref, tableIndex, WTFMove(initExpr));
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        case 0x07: {
            Type refType;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, refType), "can't parse reftype in elem section");
            WASM_PARSER_FAIL_IF(!refType.isFuncref(), "reftype in element section should be funcref");

            uint32_t indexCount;
            WASM_FAIL_IF_HELPER_FAILS(parseIndexCountForElementSection(indexCount, elementNum));

            Element element(Element::Kind::Declared, TableElementType::Funcref);
            WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

            WASM_FAIL_IF_HELPER_FAILS(parseElementSegmentVectorOfExpressions(element.functionIndices, indexCount, elementNum));
            m_info->elements.uncheckedAppend(WTFMove(element));
            break;
        }
        default:
            WASM_PARSER_FAIL_IF(true, "can't get ", elementNum, "th Element reserved byte");
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

auto SectionParser::parseInitExpr(uint8_t& opcode, uint64_t& bitsOrImportNumber, Type& resultType) -> PartialResult
{
    WASM_PARSER_FAIL_IF(!parseUInt8(opcode), "can't get init_expr's opcode");

    switch (opcode) {
    case I32Const: {
        int32_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt32(constant), "can't get constant value for init_expr's i32.const");
        bitsOrImportNumber = static_cast<uint64_t>(constant);
        resultType = Types::I32;
        break;
    }

    case I64Const: {
        int64_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt64(constant), "can't get constant value for init_expr's i64.const");
        bitsOrImportNumber = constant;
        resultType = Types::I64;
        break;
    }

    case F32Const: {
        uint32_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt32(constant), "can't get constant value for init_expr's f32.const");
        bitsOrImportNumber = constant;
        resultType = Types::F32;
        break;
    }

    case F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't get constant value for init_expr's f64.const");
        bitsOrImportNumber = constant;
        resultType = Types::F64;
        break;
    }

    case GetGlobal: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get get_global's index");

        WASM_PARSER_FAIL_IF(index >= m_info->globals.size(), "get_global's index ", index, " exceeds the number of globals ", m_info->globals.size());
        WASM_PARSER_FAIL_IF(index >= m_info->firstInternalGlobal, "get_global import kind index ", index, " exceeds the first internal global ", m_info->firstInternalGlobal);
        WASM_PARSER_FAIL_IF(m_info->globals[index].mutability != GlobalInformation::Immutable, "get_global import kind index ", index, " is mutable ");

        resultType = m_info->globals[index].type;
        bitsOrImportNumber = index;
        break;
    }

    case RefNull: {
        Type typeOfNull;
        if (Options::useWebAssemblyTypedFunctionReferences()) {
            int32_t heapType;
            WASM_PARSER_FAIL_IF(!parseHeapType(m_info, heapType), "ref.null heaptype must be funcref, externref or type_idx");
            if (isTypeIndexHeapType(heapType)) {
                SignatureIndex signatureIndex = SignatureInformation::get(m_info->usedSignatures[heapType].get());
                typeOfNull = Type { TypeKind::RefNull, Nullable::Yes, signatureIndex };
            } else
                typeOfNull = Type { TypeKind::RefNull, Nullable::Yes, static_cast<SignatureIndex>(heapType) };
        } else
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, typeOfNull), "ref.null type must be a reference type");

        resultType = typeOfNull;
        bitsOrImportNumber = JSValue::encode(jsNull());
        break;
    }

    case RefFunc: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get ref.func index");
        WASM_PARSER_FAIL_IF(index >= m_info->functions.size(), "ref.func index", index, " exceeds the number of functions ", m_info->functions.size());

        if (Options::useWebAssemblyTypedFunctionReferences()) {
            SignatureIndex signatureIndex = m_info->signatureIndexFromFunctionIndexSpace(index);
            resultType = { TypeKind::Ref, Nullable::No, signatureIndex };
        } else
            resultType = Types::Funcref;

        bitsOrImportNumber = index;
        break;
    }

    default:
        WASM_PARSER_FAIL_IF(true, "unknown init_expr opcode ", opcode);
    }

    uint8_t endOpcode;
    WASM_PARSER_FAIL_IF(!parseUInt8(endOpcode), "can't get init_expr's end opcode");
    WASM_PARSER_FAIL_IF(endOpcode != OpType::End, "init_expr should end with end, ended with ", endOpcode);

    return { };
}

auto SectionParser::validateElementTableIdx(uint32_t tableIndex) -> PartialResult
{
    WASM_PARSER_FAIL_IF(tableIndex >= m_info->tableCount(), "Element section for Table ", tableIndex, " exceeds available Table ", m_info->tableCount());
    WASM_PARSER_FAIL_IF(m_info->tables[tableIndex].type() != TableElementType::Funcref, "Table ", tableIndex, " must have type 'funcref' to have an element section");

    return { };
}

auto SectionParser::parseI32InitExpr(std::optional<I32InitExpr>& initExpr, ASCIILiteral failMessage) -> PartialResult
{
    uint8_t initOpcode;
    uint64_t initExprBits;
    Type initExprType;
    WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, initExprBits, initExprType));
    WASM_PARSER_FAIL_IF(!initExprType.isI32(), failMessage);
    initExpr = makeI32InitExpr(initOpcode, initExprBits);

    return { };
}

auto SectionParser::parseI32InitExprForElementSection(std::optional<I32InitExpr>& initExpr) -> PartialResult
{
    return parseI32InitExpr(initExpr, "Element init_expr must produce an i32"_s);
}

auto SectionParser::parseElementKind(uint8_t& resultElementKind) -> PartialResult
{
    uint8_t elementKind;
    WASM_PARSER_FAIL_IF(!parseUInt8(elementKind), "can't get element kind");
    WASM_PARSER_FAIL_IF(!!elementKind, "element kind must be zero");
    resultElementKind = elementKind;

    return { };
}

auto SectionParser::parseIndexCountForElementSection(uint32_t& resultIndexCount, const unsigned elementNum) -> PartialResult
{
    uint32_t indexCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(indexCount), "can't get ", elementNum, "th index count for Element section");
    WASM_PARSER_FAIL_IF(indexCount == std::numeric_limits<uint32_t>::max(), "Element section's ", elementNum, "th index count is too big ", indexCount);
    resultIndexCount = indexCount;

    return { };
}

auto SectionParser::parseElementSegmentVectorOfExpressions(Vector<uint32_t>& result, const unsigned indexCount, const unsigned elementNum) -> PartialResult
{
    for (uint32_t index = 0; index < indexCount; ++index) {
        uint8_t opcode;
        WASM_PARSER_FAIL_IF(!parseUInt8(opcode), "can't get opcode for exp in element section's ", elementNum, "th element's ", index, "th index");
        WASM_PARSER_FAIL_IF((opcode != RefFunc) && (opcode != RefNull), "opcode for exp in element section's should be either ref.func or ref.null ", elementNum, "th element's ", index, "th index");

        uint32_t functionIndex;
        if (opcode == RefFunc) {
            WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't get Element section's ", elementNum, "th element's ", index, "th index");
            WASM_PARSER_FAIL_IF(functionIndex >= m_info->functionIndexSpaceSize(), "Element section's ", elementNum, "th element's ", index, "th index is ", functionIndex, " which exceeds the function index space size of ", m_info->functionIndexSpaceSize());
            m_info->addDeclaredFunction(functionIndex);
        } else {
            Type typeOfNull;
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, typeOfNull), "ref.null type must be a func type in elem section");
            WASM_PARSER_FAIL_IF(!typeOfNull.isFuncref(), "ref.null extern is forbidden in element section's, ", elementNum, "th element's ", index, "th index");
            functionIndex = Element::nullFuncIndex;
        }

        WASM_PARSER_FAIL_IF(!parseUInt8(opcode), "can't get opcode for exp end in element section's ", elementNum, "th element's ", index, "th index");
        WASM_PARSER_FAIL_IF(opcode != End, "malformed expr in element section's", elementNum, "th element's ", index, "th index");

        result.uncheckedAppend(functionIndex);
    }

    return { };
}

auto SectionParser::parseElementSegmentVectorOfIndexes(Vector<uint32_t>& result, const unsigned indexCount, const unsigned elementNum) -> PartialResult
{
    for (uint32_t index = 0; index < indexCount; ++index) {
        uint32_t functionIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't get Element section's ", elementNum, "th element's ", index, "th index");
        WASM_PARSER_FAIL_IF(functionIndex >= m_info->functionIndexSpaceSize(), "Element section's ", elementNum, "th element's ", index, "th index is ", functionIndex, " which exceeds the function index space size of ", m_info->functionIndexSpaceSize());

        m_info->addDeclaredFunction(functionIndex);
        result.uncheckedAppend(functionIndex);
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
    WASM_PARSER_FAIL_IF(!parseValueType(m_info, global.type), "can't get Global's value type");
    WASM_PARSER_FAIL_IF(!parseUInt8(mutability), "can't get Global type's mutability");
    WASM_PARSER_FAIL_IF(mutability != 0x0 && mutability != 0x1, "invalid Global's mutability: 0x", hex(mutability, 2, Lowercase));
    global.mutability = static_cast<GlobalInformation::Mutability>(mutability);
    return { };
}

auto SectionParser::parseData() -> PartialResult
{
    uint32_t segmentCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(segmentCount), "can't get Data section's count");
    WASM_PARSER_FAIL_IF(segmentCount > maxDataSegments, "Data section's count is too big ", segmentCount, " maximum ", maxDataSegments);
    WASM_PARSER_FAIL_IF(!m_info->data.tryReserveCapacity(segmentCount), "can't allocate enough memory for Data section's ", segmentCount, " segments");

    for (uint32_t segmentNumber = 0; segmentNumber < segmentCount; ++segmentNumber) {
        uint32_t memoryIndexOrDataFlag = UINT32_MAX;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(memoryIndexOrDataFlag), "can't get ", segmentNumber, "th Data segment's flag");

        if (!memoryIndexOrDataFlag) {
            const uint32_t memoryIndex = memoryIndexOrDataFlag;
            WASM_PARSER_FAIL_IF(memoryIndex >= m_info->memoryCount(), segmentNumber, "th Data segment has index ", memoryIndex, " which exceeds the number of Memories ", m_info->memoryCount());

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForDataSection(initExpr));

            uint32_t dataByteLength;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get ", segmentNumber, "th Data segment's data byte length");
            WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big ", dataByteLength, " maximum ", maxModuleSize);

            auto segment = Segment::create(*initExpr, dataByteLength, Segment::Kind::Active);
            WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for ", segmentNumber, "th Data segment of size ", dataByteLength);
            for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
                uint8_t byte;
                WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get ", dataByte, "th data byte from ", segmentNumber, "th Data segment");
                segment->byte(dataByte) = byte;
            }
            m_info->data.uncheckedAppend(WTFMove(segment));
            continue;
        }

        const uint32_t dataFlag = memoryIndexOrDataFlag;
        if (dataFlag == 0x01) {
            uint32_t dataByteLength;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get ", segmentNumber, "th Data segment's data byte length");
            WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big ", dataByteLength, " maximum ", maxModuleSize);

            auto segment = Segment::create(std::nullopt, dataByteLength, Segment::Kind::Passive);
            WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for ", segmentNumber, "th Data segment of size ", dataByteLength);
            for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
                uint8_t byte;
                WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get ", dataByte, "th data byte from ", segmentNumber, "th Data segment");
                segment->byte(dataByte) = byte;
            }
            m_info->data.uncheckedAppend(WTFMove(segment));
            continue;

        }

        if (dataFlag == 0x02) {
            uint32_t memoryIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(memoryIndex), "can't get ", segmentNumber, "th Data segment's index");
            WASM_PARSER_FAIL_IF(memoryIndex >= m_info->memoryCount(), segmentNumber, "th Data segment has index ", memoryIndex, " which exceeds the number of Memories ", m_info->memoryCount());

            std::optional<I32InitExpr> initExpr;
            WASM_FAIL_IF_HELPER_FAILS(parseI32InitExprForDataSection(initExpr));

            uint32_t dataByteLength;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get ", segmentNumber, "th Data segment's data byte length");
            WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big ", dataByteLength, " maximum ", maxModuleSize);

            auto segment = Segment::create(*initExpr, dataByteLength, Segment::Kind::Active);
            WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for ", segmentNumber, "th Data segment of size ", dataByteLength);
            for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
                uint8_t byte;
                WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get ", dataByte, "th data byte from ", segmentNumber, "th Data segment");
                segment->byte(dataByte) = byte;
            }
            m_info->data.uncheckedAppend(WTFMove(segment));
            continue;
        }

        WASM_PARSER_FAIL_IF(true, "unknown ", segmentNumber, "th Data segment's flag");
    }

    return { };
}

auto SectionParser::parseDataCount() -> PartialResult
{
    uint32_t numberOfDataSegments;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfDataSegments), "can't get Data Count section's count");

    m_info->numberOfDataSegments = numberOfDataSegments;
    return { };
}

auto SectionParser::parseException() -> PartialResult
{
    WASM_PARSER_FAIL_IF(!Options::useWebAssemblyExceptions(), "wasm exceptions are not enabled");

    uint32_t exceptionCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(exceptionCount), "can't get Exception section's count");
    WASM_PARSER_FAIL_IF(exceptionCount > maxExceptions, "Export section's count is too big ", exceptionCount, " maximum ", maxExceptions);
    WASM_PARSER_FAIL_IF(!m_info->internalExceptionSignatureIndices.tryReserveCapacity(exceptionCount), "can't allocate enough memory for ", exceptionCount, " exceptions");

    for (uint32_t exceptionNumber = 0; exceptionNumber < exceptionCount; ++exceptionNumber) {
        uint8_t tagType;
        WASM_PARSER_FAIL_IF(!parseUInt8(tagType), "can't get ", exceptionNumber, "th Exception tag type");
        WASM_PARSER_FAIL_IF(tagType, exceptionNumber, "th Exception has tag type ", tagType, " but the only supported tag type is 0");

        uint32_t typeNumber;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(typeNumber), "can't get ", exceptionNumber, "th Exception's type number");
        WASM_PARSER_FAIL_IF(typeNumber >= m_info->usedSignatures.size(), exceptionNumber, "th Exception type number is invalid ", typeNumber);
        SignatureIndex signatureIndex = SignatureInformation::get(m_info->usedSignatures[typeNumber]);
        m_info->internalExceptionSignatureIndices.uncheckedAppend(signatureIndex);
    }

    return { };
}
auto SectionParser::parseCustom() -> PartialResult
{
    CustomSection section;
    uint32_t customSectionNumber = m_info->customSections.size() + 1;
    uint32_t nameLen;
    WASM_PARSER_FAIL_IF(!m_info->customSections.tryReserveCapacity(customSectionNumber), "can't allocate enough memory for ", customSectionNumber, "th custom section");
    WASM_PARSER_FAIL_IF(!parseVarUInt32(nameLen), "can't get ", customSectionNumber, "th custom section's name length");
    WASM_PARSER_FAIL_IF(!consumeUTF8String(section.name, nameLen), "nameLen get ", customSectionNumber, "th custom section's name of length ", nameLen);

    uint32_t payloadBytes = length() - m_offset;
    WASM_PARSER_FAIL_IF(!section.payload.tryReserveCapacity(payloadBytes), "can't allocate enough memory for ", customSectionNumber, "th custom section's ", payloadBytes, " bytes");
    for (uint32_t byteNumber = 0; byteNumber < payloadBytes; ++byteNumber) {
        uint8_t byte;
        WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get ", byteNumber, "th data byte from ", customSectionNumber, "th custom section");
        section.payload.uncheckedAppend(byte);
    }

    Name nameName = { 'n', 'a', 'm', 'e' };
    if (section.name == nameName) {
        NameSectionParser nameSectionParser(section.payload.begin(), section.payload.size(), m_info);
        if (auto nameSection = nameSectionParser.parse())
            m_info->nameSection = WTFMove(*nameSection);
    } else if (Options::useWebAssemblyBranchHints()) {
        Name branchHintsName = { 'c', 'o', 'd', 'e', '_', 'a', 'n', 'n', 'o', 't', 'a', 't', 'i', 'o', 'n', '.', 'b', 'r', 'a', 'n', 'c', 'h', '_', 'h', 'i', 'n', 't' };
        if (section.name == branchHintsName) {
            BranchHintsSectionParser branchHintsSectionParser(section.payload.begin(), section.payload.size(), m_info);
            branchHintsSectionParser.parse();
        }
    }

    m_info->customSections.uncheckedAppend(WTFMove(section));

    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
