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
#include "WasmFormat.h"
#include "WasmMemory.h"
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
    if (!parseVarUInt32(count))
        return false;
    if (verbose)
        dataLogLn("count: ", count);
    if (!m_module->signatures.tryReserveCapacity(count))
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        int8_t type;
        if (!parseInt7(type))
            return false;
        if (type != Func)
            return false;

        if (verbose)
            dataLogLn("Got function type.");

        uint32_t argumentCount;
        if (!parseVarUInt32(argumentCount))
            return false;

        if (verbose)
            dataLogLn("argumentCount: ", argumentCount);

        Vector<Type> argumentTypes;
        if (!argumentTypes.tryReserveCapacity(argumentCount))
            return false;

        for (unsigned i = 0; i != argumentCount; ++i) {
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
    if (!parseVarUInt32(importCount))
        return false;
    if (!m_module->imports.tryReserveCapacity(importCount))
        return false;

    for (uint32_t importNumber = 0; importNumber != importCount; ++importNumber) {
        Import imp;
        uint32_t moduleLen;
        uint32_t fieldLen;
        if (!parseVarUInt32(moduleLen))
            return false;
        String moduleString;
        if (!consumeUTF8String(moduleString, moduleLen))
            return false;
        imp.module = Identifier::fromString(m_vm, moduleString);
        if (!parseVarUInt32(fieldLen))
            return false;
        String fieldString;
        if (!consumeUTF8String(fieldString, fieldLen))
            return false;
        imp.field = Identifier::fromString(m_vm, fieldString);
        if (!parseExternalKind(imp.kind))
            return false;
        switch (imp.kind) {
        case External::Function: {
            uint32_t functionSignatureIndex;
            if (!parseVarUInt32(functionSignatureIndex))
                return false;
            if (functionSignatureIndex >= m_module->signatures.size())
                return false;
            imp.functionSignature = &m_module->signatures[functionSignatureIndex];
            break;
        }
        case External::Table: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164135
            break;
        }
        case External::Memory: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164134
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
    if (!parseVarUInt32(count))
        return false;
    if (!m_module->functions.tryReserveCapacity(count))
        return false;

    for (uint32_t i = 0; i != count; ++i) {
        uint32_t typeNumber;
        if (!parseVarUInt32(typeNumber))
            return false;

        if (typeNumber >= m_module->signatures.size())
            return false;

        // The Code section fixes up start and end.
        size_t start = 0;
        size_t end = 0;
        m_module->functions.uncheckedAppend({ &m_module->signatures[typeNumber], start, end });
    }

    return true;
}

bool ModuleParser::parseTable()
{
    // FIXME
    return true;
}

bool ModuleParser::parseMemory()
{
    uint8_t count;
    if (!parseVarUInt1(count))
        return false;

    if (!count)
        return true;

    uint8_t flags;
    if (!parseVarUInt1(flags))
        return false;

    uint32_t size;
    if (!parseVarUInt32(size))
        return false;
    if (size > maxPageCount)
        return false;

    uint32_t capacity = maxPageCount;
    if (flags) {
        if (!parseVarUInt32(capacity))
            return false;
        if (size > capacity || capacity > maxPageCount)
            return false;
    }

    capacity *= pageSize;
    size *= pageSize;

    Vector<unsigned> pinnedSizes = { 0 };
    m_module->memory = std::make_unique<Memory>(size, capacity, pinnedSizes);
    return m_module->memory->memory();
}

bool ModuleParser::parseGlobal()
{
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
    return true;
}

bool ModuleParser::parseExport()
{
    uint32_t exportCount;
    if (!parseVarUInt32(exportCount))
        return false;
    if (!m_module->exports.tryReserveCapacity(exportCount))
        return false;

    for (uint32_t exportNumber = 0; exportNumber != exportCount; ++exportNumber) {
        Export exp;
        uint32_t fieldLen;
        if (!parseVarUInt32(fieldLen))
            return false;
        String fieldString;
        if (!consumeUTF8String(fieldString, fieldLen))
            return false;
        exp.field = Identifier::fromString(m_vm, fieldString);
        if (!parseExternalKind(exp.kind))
            return false;
        switch (exp.kind) {
        case External::Function: {
            if (!parseVarUInt32(exp.functionIndex))
                return false;
            if (exp.functionIndex >= m_module->functions.size())
                return false;
            break;
        }
        case External::Table: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164135
            break;
        }
        case External::Memory: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164134
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
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=161709
    return true;
}

bool ModuleParser::parseElement()
{
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=161709
    return true;
}

bool ModuleParser::parseCode()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    if (count != m_module->functions.size())
        return false;

    for (uint32_t i = 0; i != count; ++i) {
        uint32_t functionSize;
        if (!parseVarUInt32(functionSize))
            return false;
        if (functionSize > length() || functionSize > length() - m_offset)
            return false;

        FunctionInformation& info = m_module->functions[i];
        info.start = m_offset;
        info.end = m_offset + functionSize;
        m_offset = info.end;
    }

    return true;
}

bool ModuleParser::parseData()
{
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=161709
    return true;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
