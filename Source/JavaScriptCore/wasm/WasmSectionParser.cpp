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

#include "IdentifierInlines.h"
#include "WasmMemoryInformation.h"
#include "WasmNameSectionParser.h"
#include "WasmOps.h"
#include "WasmSections.h"
#include "WasmSignatureInlines.h"
#include <wtf/Optional.h>

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
        WASM_PARSER_FAIL_IF(type != Func, i, "th Type is non-Func ", type);
        WASM_PARSER_FAIL_IF(!parseVarUInt32(argumentCount), "can't get ", i, "th Type's argument count");
        WASM_PARSER_FAIL_IF(argumentCount > maxFunctionParams, i, "th argument count is too big ", argumentCount, " maximum ", maxFunctionParams);
        RefPtr<Signature> maybeSignature = Signature::tryCreate(argumentCount);
        WASM_PARSER_FAIL_IF(!maybeSignature, "can't allocate enough memory for Type section's ", i, "th signature");
        Ref<Signature> signature = maybeSignature.releaseNonNull();

        for (unsigned i = 0; i < argumentCount; ++i) {
            Type argumentType;
            WASM_PARSER_FAIL_IF(!parseValueType(argumentType), "can't get ", i, "th argument Type");
            signature->argument(i) = argumentType;
        }

        uint8_t returnCount;
        WASM_PARSER_FAIL_IF(!parseVarUInt1(returnCount), "can't get ", i, "th Type's return count");
        Type returnType;
        if (returnCount) {
            Type value;
            WASM_PARSER_FAIL_IF(!parseValueType(value), "can't get ", i, "th Type's return value");
            returnType = static_cast<Type>(value);
        } else
            returnType = Type::Void;
        signature->returnType() = returnType;

        m_info->usedSignatures.uncheckedAppend(SignatureInformation::adopt(WTFMove(signature)));
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
            Global global;
            WASM_FAIL_IF_HELPER_FAILS(parseGlobalType(global));
            WASM_PARSER_FAIL_IF(global.mutability == Global::Mutable, "Mutable Globals aren't supported");

            kindIndex = m_info->globals.size();
            m_info->globals.uncheckedAppend(WTFMove(global));
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

auto SectionParser::parseResizableLimits(uint32_t& initial, Optional<uint32_t>& maximum) -> PartialResult
{
    ASSERT(!maximum);

    uint8_t flags;
    WASM_PARSER_FAIL_IF(!parseVarUInt1(flags), "can't parse resizable limits flags");
    WASM_PARSER_FAIL_IF(!parseVarUInt32(initial), "can't parse resizable limits initial page count");

    if (flags) {
        uint32_t maximumInt;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(maximumInt), "can't parse resizable limits maximum page count");
        WASM_PARSER_FAIL_IF(initial > maximumInt, "resizable limits has a initial page count of ", initial, " which is greater than its maximum ", maximumInt);
        maximum = maximumInt;
    }

    return { };
}

auto SectionParser::parseTableHelper(bool isImport) -> PartialResult
{
    WASM_PARSER_FAIL_IF(m_info->tableCount() > 0, "Cannot have more than one Table for now");

    int8_t type;
    WASM_PARSER_FAIL_IF(!parseInt7(type), "can't parse Table type");
    WASM_PARSER_FAIL_IF(type != Wasm::Anyfunc, "Table type should be anyfunc, got ", type);

    uint32_t initial;
    Optional<uint32_t> maximum;
    PartialResult limits = parseResizableLimits(initial, maximum);
    if (UNLIKELY(!limits))
        return makeUnexpected(WTFMove(limits.error()));
    WASM_PARSER_FAIL_IF(initial > maxTableEntries, "Table's initial page count of ", initial, " is too big, maximum ", maxTableEntries);

    ASSERT(!maximum || *maximum >= initial);

    m_info->tableInformation = TableInformation(initial, maximum, isImport);

    return { };
}

auto SectionParser::parseTable() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Table's count");
    WASM_PARSER_FAIL_IF(count > 1, "Table count of ", count, " is invalid, at most 1 is allowed for now");

    if (!count)
        return { };

    bool isImport = false;
    PartialResult result = parseTableHelper(isImport);
    if (UNLIKELY(!result))
        return makeUnexpected(WTFMove(result.error()));

    return { };
}

auto SectionParser::parseMemoryHelper(bool isImport) -> PartialResult
{
    WASM_PARSER_FAIL_IF(m_info->memoryCount(), "there can at most be one Memory section for now");

    PageCount initialPageCount;
    PageCount maximumPageCount;
    {
        uint32_t initial;
        Optional<uint32_t> maximum;
        PartialResult limits = parseResizableLimits(initial, maximum);
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

    m_info->memory = MemoryInformation(initialPageCount, maximumPageCount, isImport);
    return { };
}

auto SectionParser::parseMemory() -> PartialResult
{
    uint8_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt1(count), "can't parse Memory section's count");

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
        Global global;
        uint8_t initOpcode;

        WASM_FAIL_IF_HELPER_FAILS(parseGlobalType(global));
        Type typeForInitOpcode;
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, global.initialBitsOrImportNumber, typeForInitOpcode));
        if (initOpcode == GetGlobal)
            global.initializationType = Global::FromGlobalImport;
        else
            global.initializationType = Global::FromExpression;
        WASM_PARSER_FAIL_IF(typeForInitOpcode != global.type, "Global init_expr opcode of type ", typeForInitOpcode, " doesn't match global's type ", global.type);

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
            WASM_PARSER_FAIL_IF(m_info->globals[kindIndex].mutability != Global::Immutable, exportNumber, "th Export isn't immutable, named '", fieldString, "'");
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
    WASM_PARSER_FAIL_IF(signature.returnType() != Void, "Start function can't return a value");
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
        uint32_t tableIndex;
        uint64_t initExprBits;
        uint8_t initOpcode;
        uint32_t indexCount;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get ", elementNum, "th Element table index");
        WASM_PARSER_FAIL_IF(tableIndex >= m_info->tableCount(), "Element section for Table ", tableIndex, " exceeds available Table ", m_info->tableCount());
        Type initExprType;
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, initExprBits, initExprType));
        WASM_PARSER_FAIL_IF(initExprType != I32, "Element init_expr must produce an i32");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(indexCount), "can't get ", elementNum, "th index count for Element section");
        WASM_PARSER_FAIL_IF(indexCount == std::numeric_limits<uint32_t>::max(), "Element section's ", elementNum, "th index count is too big ", indexCount);

        ASSERT(!!m_info->tableInformation);

        Element element(makeI32InitExpr(initOpcode, initExprBits));
        WASM_PARSER_FAIL_IF(!element.functionIndices.tryReserveCapacity(indexCount), "can't allocate memory for ", indexCount, " Element indices");

        for (unsigned index = 0; index < indexCount; ++index) {
            uint32_t functionIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't get Element section's ", elementNum, "th element's ", index, "th index");
            WASM_PARSER_FAIL_IF(functionIndex >= m_info->functionIndexSpaceSize(), "Element section's ", elementNum, "th element's ", index, "th index is ", functionIndex, " which exceeds the function index space size of ", m_info->functionIndexSpaceSize());

            element.functionIndices.uncheckedAppend(functionIndex);
        }

        m_info->elements.uncheckedAppend(WTFMove(element));
    }

    return { };
}

// This function will be changed to be RELEASE_ASSERT_NOT_REACHED once we switch our parsing infrastructure to the streaming parser.
auto SectionParser::parseCode() -> PartialResult
{
    uint32_t count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(count), "can't get Code section's count");
    WASM_PARSER_FAIL_IF(count == std::numeric_limits<uint32_t>::max(), "Code section's count is too big ", count);
    WASM_PARSER_FAIL_IF(count != m_info->functions.size(), "Code section count ", count, " exceeds the declared number of functions ", m_info->functions.size());

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t functionSize;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(functionSize), "can't get ", i, "th Code function's size");
        WASM_PARSER_FAIL_IF(functionSize > length(), "Code function's size ", functionSize, " exceeds the module's size ", length());
        WASM_PARSER_FAIL_IF(functionSize > length() - m_offset, "Code function's size ", functionSize, " exceeds the module's remaining size", length() - m_offset);
        WASM_PARSER_FAIL_IF(functionSize > maxFunctionSize, "Code function's size ", functionSize, " is too big");

        Vector<uint8_t> data(functionSize);
        std::memcpy(data.data(), source() + m_offset, functionSize);
        m_info->functions[i].start = m_offsetInSource + m_offset;
        m_info->functions[i].end = m_offsetInSource + m_offset + functionSize;
        m_info->functions[i].data = WTFMove(data);
        m_offset += functionSize;
    }

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
        resultType = I32;
        break;
    }

    case I64Const: {
        int64_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt64(constant), "can't get constant value for init_expr's i64.const");
        bitsOrImportNumber = constant;
        resultType = I64;
        break;
    }

    case F32Const: {
        uint32_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt32(constant), "can't get constant value for init_expr's f32.const");
        bitsOrImportNumber = constant;
        resultType = F32;
        break;
    }

    case F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't get constant value for init_expr's f64.const");
        bitsOrImportNumber = constant;
        resultType = F64;
        break;
    }

    case GetGlobal: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get get_global's index");

        WASM_PARSER_FAIL_IF(index >= m_info->globals.size(), "get_global's index ", index, " exceeds the number of globals ", m_info->globals.size());
        WASM_PARSER_FAIL_IF(index >= m_info->firstInternalGlobal, "get_global import kind index ", index, " exceeds the first internal global ", m_info->firstInternalGlobal);

        ASSERT(m_info->globals[index].mutability == Global::Immutable);
        resultType = m_info->globals[index].type;
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

auto SectionParser::parseGlobalType(Global& global) -> PartialResult
{
    uint8_t mutability;
    WASM_PARSER_FAIL_IF(!parseValueType(global.type), "can't get Global's value type");
    WASM_PARSER_FAIL_IF(!parseVarUInt1(mutability), "can't get Global type's mutability");
    global.mutability = static_cast<Global::Mutability>(mutability);
    return { };
}

auto SectionParser::parseData() -> PartialResult
{
    uint32_t segmentCount;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(segmentCount), "can't get Data section's count");
    WASM_PARSER_FAIL_IF(segmentCount > maxDataSegments, "Data section's count is too big ", segmentCount, " maximum ", maxDataSegments);
    WASM_PARSER_FAIL_IF(!m_info->data.tryReserveCapacity(segmentCount), "can't allocate enough memory for Data section's ", segmentCount, " segments");

    for (uint32_t segmentNumber = 0; segmentNumber < segmentCount; ++segmentNumber) {
        uint32_t memoryIndex;
        uint64_t initExprBits;
        uint8_t initOpcode;
        uint32_t dataByteLength;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(memoryIndex), "can't get ", segmentNumber, "th Data segment's index");
        WASM_PARSER_FAIL_IF(memoryIndex >= m_info->memoryCount(), segmentNumber, "th Data segment has index ", memoryIndex, " which exceeds the number of Memories ", m_info->memoryCount());
        Type initExprType;
        WASM_FAIL_IF_HELPER_FAILS(parseInitExpr(initOpcode, initExprBits, initExprType));
        WASM_PARSER_FAIL_IF(initExprType != I32, segmentNumber, "th Data segment's init_expr must produce an i32");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(dataByteLength), "can't get ", segmentNumber, "th Data segment's data byte length");
        WASM_PARSER_FAIL_IF(dataByteLength > maxModuleSize, segmentNumber, "th Data segment's data byte length is too big ", dataByteLength, " maximum ", maxModuleSize);

        Segment* segment = Segment::create(makeI32InitExpr(initOpcode, initExprBits), dataByteLength);
        WASM_PARSER_FAIL_IF(!segment, "can't allocate enough memory for ", segmentNumber, "th Data segment of size ", dataByteLength);
        m_info->data.uncheckedAppend(Segment::adoptPtr(segment));
        for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
            uint8_t byte;
            WASM_PARSER_FAIL_IF(!parseUInt8(byte), "can't get ", dataByte, "th data byte from ", segmentNumber, "th Data segment");
            segment->byte(dataByte) = byte;
        }
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
    }

    m_info->customSections.uncheckedAppend(WTFMove(section));

    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
