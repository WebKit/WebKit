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
#include "WASMModuleParser.h"

#include "WASMFormat.h"
#include "WASMOps.h"
#include "WASMSections.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC {

namespace WASM {

static const bool verbose = false;

bool WASMModuleParser::parse()
{
    if (m_sourceLength < 8)
        return false;
    if (!consumeCharacter(0))
        return false;
    if (!consumeString("asm"))
        return false;

    // Skip the version number for now since we don't do anything with it.
    uint32_t versionNumber;
    if (!parseUInt32(versionNumber))
        return false;

    if (versionNumber != magicNumber)
        return false;


    if (verbose)
        dataLogLn("Passed processing header.");

    WASMSections::Section previousSection = WASMSections::Section::Unknown;
    while (m_offset < m_sourceLength) {
        if (verbose)
            dataLogLn("Starting to parse next section at offset: ", m_offset);
        uint32_t sectionNameLength;
        if (!parseVarUInt32(sectionNameLength))
            return false;

        // Make sure we can read up to the section's size.
        if (m_offset + sectionNameLength + maxLEBByteLength >= m_sourceLength)
            return false;

        WASMSections::Section section = WASMSections::lookup(m_source.data() + m_offset, sectionNameLength);
        if (!WASMSections::validateOrder(previousSection, section))
            return false;
        m_offset += sectionNameLength;

        uint32_t sectionLength;
        if (!parseVarUInt32(sectionLength))
            return false;

        unsigned end = m_offset + sectionLength;

        switch (section) {
        case WASMSections::Section::End:
            return true;

        case WASMSections::Section::FunctionTypes: {
            if (verbose)
                dataLogLn("Parsing types.");
            if (!parseFunctionTypes())
                return false;
            break;
        }

        case WASMSections::Section::Signatures: {
            if (verbose)
                dataLogLn("Parsing function signatures.");
            if (!parseFunctionSignatures())
                return false;
            break;
        }

        case WASMSections::Section::Definitions: {
            if (verbose)
                dataLogLn("Parsing function definitions.");
            if (!parseFunctionDefinitions())
                return false;
            break;
        }

        case WASMSections::Section::Unknown: {
            if (verbose)
                dataLogLn("Unknown section, skipping.");
            m_offset += sectionLength;
            break;
        }
        }

        if (verbose)
            dataLogLn("Finished parsing section.");

        if (end != m_offset)
            return false;

        previousSection = section;
    }

    // TODO
    return true;
}

bool WASMModuleParser::parseFunctionTypes()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    if (verbose)
        dataLogLn("count: ", count);

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

        Vector<WASMValueType> argumentTypes;
        for (unsigned i = 0; i < argumentCount; ++i) {
            if (!parseUInt7(type) || type >= static_cast<uint8_t>(WASMValueType::NumberOfTypes))
                return false;
            argumentTypes.append(static_cast<WASMValueType>(type));
        }

        if (!parseVarUInt1(type))
            return false;
        WASMFunctionReturnType returnType;

        if (verbose)
            dataLogLn(type);

        if (type) {
            WASMValueType value;
            if (!parseValueType(value))
                return false;
            returnType = static_cast<WASMFunctionReturnType>(value);
        } else
            returnType = WASMFunctionReturnType::Void;

        // TODO: Actually do something with this data...
    }
    return true;
}

bool WASMModuleParser::parseFunctionSignatures()
{
    uint32_t count;
    if (!parseVarUInt32(count))
        return false;

    m_functions.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t typeNumber;
        if (!parseVarUInt32(typeNumber))
            return false;
    }

    return true;
}

bool WASMModuleParser::parseFunctionDefinitions()
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

        WASMFunctionInformation& info = m_functions[i];
        info.start = m_offset;
        info.end = m_offset + functionSize;
        m_offset = info.end;
    }

    return true;
}

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
