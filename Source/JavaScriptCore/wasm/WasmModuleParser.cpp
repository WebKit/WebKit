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

#include "config.h"
#include "WasmModuleParser.h"

#if ENABLE(WEBASSEMBLY)

#include "IdentifierInlines.h"
#include "JSWebAssemblyTable.h"
#include "WasmFormat.h"
#include "WasmMemoryInformation.h"
#include "WasmOps.h"
#include "WasmSections.h"

#include <sys/mman.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;

bool ModuleParser::parse()
{
    m_module = std::make_unique<ModuleInformation>();

    const size_t minSize = 8;
    if (length() < minSize) {
        m_errorMessage = "Module is " + String::number(length()) + " bytes, expected at least " + String::number(minSize) + " bytes";
        return false;
    }
    if (!consumeCharacter(0) || !consumeString("asm")) {
        m_errorMessage = "Modules doesn't start with '\\0asm'";
        return false;
    }

    uint32_t versionNumber;
    if (!parseUInt32(versionNumber)) {
        // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
        m_errorMessage = "couldn't parse version number";
        return false;
    }

    if (versionNumber != expectedVersionNumber) {
        // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
        m_errorMessage = "unexpected version number";
        return false;
    }


    if (verbose)
        dataLogLn("Passed processing header.");

    Sections::Section previousSection = Sections::Unknown;
    while (m_offset < length()) {
        if (verbose)
            dataLogLn("Starting to parse next section at offset: ", m_offset);

        uint8_t sectionByte;
        if (!parseUInt7(sectionByte)) {
            // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
            m_errorMessage = "couldn't get section byte";
            return false;
        }

        if (verbose)
            dataLogLn("Section byte: ", sectionByte);

        Sections::Section section = Sections::Unknown;
        if (sectionByte) {
            if (sectionByte < Sections::Unknown)
                section = static_cast<Sections::Section>(sectionByte);
        }

        if (!Sections::validateOrder(previousSection, section)) {
            // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
            m_errorMessage = "invalid section order";
            return false;
        }

        uint32_t sectionLength;
        if (!parseVarUInt32(sectionLength)) {
            // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
            m_errorMessage = "couldn't get section length";
            return false;
        }

        if (sectionLength > length() - m_offset) {
            // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
            m_errorMessage = "section content would overflow Module's size";
            return false;
        }

        auto end = m_offset + sectionLength;

        switch (section) {
        // FIXME improve error message in macro below https://bugs.webkit.org/show_bug.cgi?id=163919
#define WASM_SECTION_PARSE(NAME, ID, DESCRIPTION) \
        case Sections::NAME: { \
            if (verbose) \
                dataLogLn("Parsing " DESCRIPTION); \
            if (!parse ## NAME()) { \
                m_errorMessage = "couldn't parse section " #NAME ": " DESCRIPTION; \
                return false; \
            } \
            break; \
        }
        FOR_EACH_WASM_SECTION(WASM_SECTION_PARSE)
#undef WASM_SECTION_PARSE

        case Sections::Unknown: {
            if (verbose)
                dataLogLn("Unknown section, skipping.");
            // Ignore section's name LEB and bytes: they're already included in sectionLength.
            m_offset += sectionLength;
            break;
        }
        }

        if (verbose)
            dataLogLn("Finished parsing section.");

        if (end != m_offset) {
            // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
            m_errorMessage = "parsing ended before the end of the section";
            return false;
        }

        previousSection = section;
    }

    // TODO
    m_failed = false;
    return true;
}

bool ModuleParser::parseType()
{
    uint32_t count;
    if (!parseVarUInt32(count)
        || count == std::numeric_limits<uint32_t>::max()
        || !m_module->signatures.tryReserveCapacity(count))
        return false;
    if (verbose)
        dataLogLn("  count: ", count);

    for (uint32_t i = 0; i < count; ++i) {
        int8_t type;
        if (!parseInt7(type))
            return false;
        if (type != Func)
            return false;

        if (verbose)
            dataLogLn("Got function type.");

        uint32_t argumentCount;
        Vector<Type> argumentTypes;
        if (!parseVarUInt32(argumentCount)
            || argumentCount == std::numeric_limits<uint32_t>::max()
            || !argumentTypes.tryReserveCapacity(argumentCount))
            return false;
        if (verbose)
            dataLogLn("  argument count: ", argumentCount);

        for (unsigned i = 0; i < argumentCount; ++i) {
            Type argumentType;
            if (!parseResultType(argumentType))
                return false;
            argumentTypes.uncheckedAppend(argumentType);
        }

        uint8_t returnCount;
        if (!parseVarUInt1(returnCount))
            return false;
        Type returnType;

        if (verbose)
            dataLogLn(returnCount);

        if (returnCount) {
            Type value;
            if (!parseValueType(value))
                return false;
            returnType = static_cast<Type>(value);
        } else
            returnType = Type::Void;

        m_module->signatures.uncheckedAppend({ returnType, WTFMove(argumentTypes) });
    }
    return true;
}

bool ModuleParser::parseImport()
{
    uint32_t importCount;
    if (!parseVarUInt32(importCount)
        || importCount == std::numeric_limits<uint32_t>::max()
        || !m_module->imports.tryReserveCapacity(importCount) // FIXME this over-allocates when we fix the FIXMEs below.
        || !m_module->importFunctions.tryReserveCapacity(importCount) // FIXME this over-allocates when we fix the FIXMEs below.
        || !m_functionIndexSpace.tryReserveCapacity(importCount)) // FIXME this over-allocates when we fix the FIXMEs below. We'll allocate some more here when we know how many functions to expect.
        return false;

    for (uint32_t importNumber = 0; importNumber < importCount; ++importNumber) {
        Import imp;
        uint32_t moduleLen;
        uint32_t fieldLen;
        String moduleString;
        String fieldString;
        if (!parseVarUInt32(moduleLen)
            || !consumeUTF8String(moduleString, moduleLen))
            return false;
        imp.module = Identifier::fromString(m_vm, moduleString);
        if (!parseVarUInt32(fieldLen)
            || !consumeUTF8String(fieldString, fieldLen))
            return false;
        imp.field = Identifier::fromString(m_vm, fieldString);
        if (!parseExternalKind(imp.kind))
            return false;
        switch (imp.kind) {
        case External::Function: {
            uint32_t functionSignatureIndex;
            if (!parseVarUInt32(functionSignatureIndex)
                || functionSignatureIndex >= m_module->signatures.size())
                return false;
            imp.kindIndex = m_module->importFunctions.size();
            Signature* signature = &m_module->signatures[functionSignatureIndex];
            m_module->importFunctions.uncheckedAppend(signature);
            m_functionIndexSpace.uncheckedAppend(signature);
            break;
        }
        case External::Table: {
            bool isImport = true;
            if (!parseTableHelper(isImport))
                return false;
            break;
        }
        case External::Memory: {
            bool isImport = true;
            if (!parseMemoryHelper(isImport))
                return false;
            break;
        }
        case External::Global: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
            // In the MVP, only immutable global variables can be imported.
            break;
        }
        }

        m_module->imports.uncheckedAppend(imp);
    }

    return true;
}

bool ModuleParser::parseFunction()
{
    uint32_t count;
    if (!parseVarUInt32(count)
        || count == std::numeric_limits<uint32_t>::max()
        || !m_module->internalFunctionSignatures.tryReserveCapacity(count)
        || !m_functionLocationInBinary.tryReserveCapacity(count)
        || !m_functionIndexSpace.tryReserveCapacity(m_functionIndexSpace.size() + count))
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t typeNumber;
        if (!parseVarUInt32(typeNumber)
            || typeNumber >= m_module->signatures.size())
            return false;

        Signature* signature = &m_module->signatures[typeNumber];
        // The Code section fixes up start and end.
        size_t start = 0;
        size_t end = 0;
        m_module->internalFunctionSignatures.uncheckedAppend(signature);
        m_functionLocationInBinary.uncheckedAppend({ start, end });
        m_functionIndexSpace.uncheckedAppend(signature);
    }

    return true;
}

bool ModuleParser::parseResizableLimits(uint32_t& initial, std::optional<uint32_t>& maximum)
{
    ASSERT(!maximum);

    uint8_t flags;
    if (!parseVarUInt1(flags))
        return false;

    if (!parseVarUInt32(initial))
        return false;

    if (flags) {
        uint32_t maximumInt;
        if (!parseVarUInt32(maximumInt))
            return false;

        if (initial > maximumInt)
            return false;

        maximum = maximumInt;
    }

    return true;
}

bool ModuleParser::parseTableHelper(bool isImport)
{
    // We're only allowed a total of one Table import or definition.
    if (m_hasTable)
        return false;

    m_hasTable = true;

    int8_t type;
    if (!parseInt7(type))
        return false;
    if (type != Wasm::Anyfunc)
        return false;

    uint32_t initial;
    std::optional<uint32_t> maximum;
    if (!parseResizableLimits(initial, maximum))
        return false;

    if (!JSWebAssemblyTable::isValidSize(initial))
        return false;

    ASSERT(!maximum || *maximum >= initial);

    m_module->tableInformation = TableInformation(initial, maximum, isImport);

    return true;
}

bool ModuleParser::parseTable()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    // We only allow one table for now.
    if (count != 1)
        return false;

    bool isImport = false;
    return parseTableHelper(isImport);
}

bool ModuleParser::parseMemoryHelper(bool isImport)
{
    // We don't allow redeclaring memory. Either via import or definition.
    if (m_module->memory)
        return false;

    PageCount initialPageCount;
    PageCount maximumPageCount;
    {
        uint32_t initial;
        std::optional<uint32_t> maximum;
        if (!parseResizableLimits(initial, maximum))
            return false;
        ASSERT(!maximum || *maximum >= initial);
        if (!PageCount::isValid(initial))
            return false;

        initialPageCount = PageCount(initial);

        if (maximum) {
            if (!PageCount::isValid(*maximum))
                return false;
            maximumPageCount = PageCount(*maximum);
        }
    }
    ASSERT(initialPageCount);
    ASSERT(!maximumPageCount || maximumPageCount >= initialPageCount);

    Vector<unsigned> pinnedSizes = { 0 };
    m_module->memory = MemoryInformation(initialPageCount, maximumPageCount, pinnedSizes, isImport);
    return true;
}

bool ModuleParser::parseMemory()
{
    uint8_t count;
    if (!parseVarUInt1(count))
        return false;

    if (!count)
        return true;

    // We only allow one memory for now.
    if (count != 1)
        return false;

    bool isImport = false;
    return parseMemoryHelper(isImport);
}

bool ModuleParser::parseGlobal()
{
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

bool ModuleParser::parseExport()
{
    uint32_t exportCount;
    if (!parseVarUInt32(exportCount)
        || exportCount == std::numeric_limits<uint32_t>::max()
        || !m_module->exports.tryReserveCapacity(exportCount))
        return false;

    for (uint32_t exportNumber = 0; exportNumber < exportCount; ++exportNumber) {
        Export exp;
        uint32_t fieldLen;
        String fieldString;
        if (!parseVarUInt32(fieldLen)
            || !consumeUTF8String(fieldString, fieldLen))
            return false;
        exp.field = Identifier::fromString(m_vm, fieldString);

        if (!parseExternalKind(exp.kind))
            return false;

        switch (exp.kind) {
        case External::Function: {
            if (!parseVarUInt32(exp.functionIndex)
                || exp.functionIndex >= m_functionIndexSpace.size())
                return false;
            break;
        }
        case External::Table: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=165782
            break;
        }
        case External::Memory: {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=165671
            break;
        }
        case External::Global: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
            // In the MVP, only immutable global variables can be exported.
            break;
        }
        }

        m_module->exports.uncheckedAppend(exp);
    }

    return true;
}

bool ModuleParser::parseStart()
{
    uint32_t startFunctionIndex;
    if (!parseVarUInt32(startFunctionIndex)
        || startFunctionIndex >= m_functionIndexSpace.size())
        return false;
    Signature* signature = m_functionIndexSpace[startFunctionIndex].signature;
    if (signature->arguments.size() != 0
        || signature->returnType != Void)
        return false;
    m_module->startFunctionIndexSpace = startFunctionIndex;
    return true;
}

bool ModuleParser::parseElement()
{
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=161709
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

bool ModuleParser::parseCode()
{
    uint32_t count;
    if (!parseVarUInt32(count)
        || count == std::numeric_limits<uint32_t>::max()
        || count != m_functionLocationInBinary.size())
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t functionSize;
        if (!parseVarUInt32(functionSize)
            || functionSize > length()
            || functionSize > length() - m_offset)
            return false;

        m_functionLocationInBinary[i].start = m_offset;
        m_functionLocationInBinary[i].end = m_offset + functionSize;
        m_offset = m_functionLocationInBinary[i].end;
    }

    return true;
}

bool ModuleParser::parseData()
{
    uint32_t segmentCount;
    if (!parseVarUInt32(segmentCount)
        || segmentCount == std::numeric_limits<uint32_t>::max()
        || !m_module->data.tryReserveCapacity(segmentCount))
        return false;
    if (verbose)
        dataLogLn("  segments: ", segmentCount);

    for (uint32_t segmentNumber = 0; segmentNumber < segmentCount; ++segmentNumber) {
        if (verbose)
            dataLogLn("  segment #", segmentNumber);
        uint32_t index;
        uint8_t opcode;
        uint32_t offset;
        uint8_t endOpcode;
        uint32_t dataByteLength;
        if (!parseVarUInt32(index)
            || index)
            return false;

        // FIXME allow complex init_expr here. https://bugs.webkit.org/show_bug.cgi?id=165700
        // For now we only handle i32.const as offset.
        if (!parseUInt8(opcode)
            || opcode != Wasm::I32Const
            || !parseVarUInt32(offset)
            || !parseUInt8(endOpcode)
            || endOpcode != Wasm::End)
            return false;
        if (verbose)
            dataLogLn("    offset: ", offset);

        if (!parseVarUInt32(dataByteLength)
            || dataByteLength == std::numeric_limits<uint32_t>::max())
            return false;
        if (verbose)
            dataLogLn("    data bytes: ", dataByteLength);

        Segment* segment = Segment::make(offset, dataByteLength);
        if (!segment)
            return false;
        m_module->data.uncheckedAppend(Segment::makePtr(segment));
        for (uint32_t dataByte = 0; dataByte < dataByteLength; ++dataByte) {
            uint8_t byte;
            if (!parseUInt8(byte))
                return false;
            segment->byte(dataByte) = byte;
            if (verbose)
                dataLogLn("    [", dataByte, "] = ", segment->byte(dataByte));
        }
    }
    return true;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
