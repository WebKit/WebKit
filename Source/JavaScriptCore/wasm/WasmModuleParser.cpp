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

#include "JSWasmModule.h"
#include "WasmFormat.h"
#include "WasmOps.h"
#include "WasmSections.h"

#include <sys/mman.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;

bool ModuleParser::parse()
{
    const size_t minSize = 8;
    if (length() < minSize) {
        m_errorMessage = "Module is " + String::number(length()) + " bytes, expected at least " + String::number(minSize) + " bytes";
        return false;
    }
    if (!consumeCharacter(0) || !consumeString("asm")) {
        m_errorMessage = "Modules doesn't start with '\\0asm'";
        return false;
    }

    // Skip the version number for now since we don't do anything with it.
    uint32_t versionNumber;
    if (!parseUInt32(versionNumber)) {
        // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
        m_errorMessage = "couldn't parse version number";
        return false;
    }

    if (versionNumber != magicNumber) {
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

        case Sections::Memory: {
            if (verbose)
                dataLogLn("Parsing Memory.");
            if (!parseMemory()) {
                // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
                m_errorMessage = "couldn't parse memory";
                return false;
            }
            break;
        }

        case Sections::FunctionTypes: {
            if (verbose)
                dataLogLn("Parsing types.");
            if (!parseFunctionTypes()) {
                // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
                m_errorMessage = "couldn't parse types";
                return false;
            }
            break;
        }

        case Sections::Signatures: {
            if (verbose)
                dataLogLn("Parsing function signatures.");
            if (!parseFunctionSignatures()) {
                // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
                m_errorMessage = "couldn't parse function signatures";
                return false;
            }
            break;
        }

        case Sections::Definitions: {
            if (verbose)
                dataLogLn("Parsing function definitions.");
            if (!parseFunctionDefinitions()) {
                // FIXME improve error message https://bugs.webkit.org/show_bug.cgi?id=163919
                m_errorMessage = "couldn't parse function definitions";
                return false;
            }
            break;
        }

        case Sections::Unknown:
        // FIXME: Delete this when we support all the sections.
        default: {
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

bool ModuleParser::parseMemory()
{
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
    m_memory = std::make_unique<Memory>(size, capacity, pinnedSizes);
    return m_memory->memory();
}

bool ModuleParser::parseFunctionTypes()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    if (verbose)
        dataLogLn("count: ", count);

    m_signatures.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t type;
        if (!parseUInt7(type))
            return false;
        if (type != 0x40) // Function type constant.
            return false;

        if (verbose)
            dataLogLn("Got function type.");

        uint32_t argumentCount;
        if (!parseVarUInt32(argumentCount))
            return false;

        if (verbose)
            dataLogLn("argumentCount: ", argumentCount);

        Vector<Type> argumentTypes;
        argumentTypes.resize(argumentCount);

        for (unsigned i = 0; i < argumentCount; ++i) {
            if (!parseUInt7(type) || !isValueType(static_cast<Type>(type)))
                return false;
            argumentTypes[i] = static_cast<Type>(type);
        }

        if (!parseVarUInt1(type))
            return false;
        Type returnType;

        if (verbose)
            dataLogLn(type);

        if (type) {
            Type value;
            if (!parseValueType(value))
                return false;
            returnType = static_cast<Type>(value);
        } else
            returnType = Type::Void;

        m_signatures[i] = { returnType, WTFMove(argumentTypes) };
    }
    return true;
}

bool ModuleParser::parseFunctionSignatures()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    m_functions.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t typeNumber;
        if (!parseVarUInt32(typeNumber))
            return false;

        if (typeNumber >= m_signatures.size())
            return false;

        m_functions[i].signature = &m_signatures[typeNumber];
    }

    return true;
}

bool ModuleParser::parseFunctionDefinitions()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    if (count != m_functions.size())
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t functionSize;
        if (!parseVarUInt32(functionSize))
            return false;

        FunctionInformation& info = m_functions[i];
        info.start = m_offset;
        info.end = m_offset + functionSize;
        m_offset = info.end;
    }

    return true;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
