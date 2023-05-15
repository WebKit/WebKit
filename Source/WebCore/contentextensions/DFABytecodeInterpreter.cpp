/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DFABytecodeInterpreter.h"

#include "ContentExtensionsDebugging.h"
#include <wtf/text/CString.h>

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore::ContentExtensions {

template <typename IntType>
static IntType getBits(std::span<const uint8_t> bytecode, uint32_t index)
{
    ASSERT(index + sizeof(IntType) <= bytecode.size());
    return *reinterpret_cast<const IntType*>(bytecode.data() + index);
}

static uint32_t get24BitsUnsigned(std::span<const uint8_t> bytecode, uint32_t index)
{
    ASSERT(index + UInt24Size <= bytecode.size());
    uint32_t highBits = getBits<uint8_t>(bytecode, index + sizeof(uint16_t));
    uint32_t lowBits = getBits<uint16_t>(bytecode, index);
    return (highBits << 16) | lowBits;
}

static DFABytecodeInstruction getInstruction(std::span<const uint8_t> bytecode, uint32_t index)
{
    return static_cast<DFABytecodeInstruction>(getBits<uint8_t>(bytecode, index) & DFABytecodeInstructionMask);
}

static size_t jumpSizeInBytes(DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case DFABytecodeJumpSize::Int8:
        return sizeof(int8_t);
    case DFABytecodeJumpSize::Int16:
        return sizeof(int16_t);
    case DFABytecodeJumpSize::Int24:
        return Int24Size;
    case DFABytecodeJumpSize::Int32:
        return sizeof(int32_t);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename T> uint32_t consumeInteger(std::span<const uint8_t> bytecode, uint32_t& programCounter)
{
    programCounter += sizeof(T);
    return getBits<T>(bytecode, programCounter - sizeof(T));
}
static uint32_t consume24BitUnsignedInteger(std::span<const uint8_t> bytecode, uint32_t& programCounter)
{
    programCounter += UInt24Size;
    return get24BitsUnsigned(bytecode, programCounter - UInt24Size);
}

static constexpr bool hasFlags(DFABytecodeInstruction instruction)
{
    switch (instruction) {
    case DFABytecodeInstruction::TestFlagsAndAppendAction:
        return true;
    case DFABytecodeInstruction::CheckValueCaseSensitive:
    case DFABytecodeInstruction::CheckValueCaseInsensitive:
    case DFABytecodeInstruction::JumpTableCaseInsensitive:
    case DFABytecodeInstruction::JumpTableCaseSensitive:
    case DFABytecodeInstruction::CheckValueRangeCaseSensitive:
    case DFABytecodeInstruction::CheckValueRangeCaseInsensitive:
    case DFABytecodeInstruction::Jump:
    case DFABytecodeInstruction::AppendAction:
    case DFABytecodeInstruction::Terminate:
        break;
    }
    return false;
}

static constexpr bool hasAction(DFABytecodeInstruction instruction)
{
    switch (instruction) {
    case DFABytecodeInstruction::TestFlagsAndAppendAction:
    case DFABytecodeInstruction::AppendAction:
        return true;
    case DFABytecodeInstruction::CheckValueCaseSensitive:
    case DFABytecodeInstruction::CheckValueCaseInsensitive:
    case DFABytecodeInstruction::JumpTableCaseInsensitive:
    case DFABytecodeInstruction::JumpTableCaseSensitive:
    case DFABytecodeInstruction::CheckValueRangeCaseSensitive:
    case DFABytecodeInstruction::CheckValueRangeCaseInsensitive:
    case DFABytecodeInstruction::Jump:
    case DFABytecodeInstruction::Terminate:
        break;
    }
    return false;
}

static ResourceFlags consumeResourceFlagsAndInstruction(std::span<const uint8_t> bytecode, uint32_t& programCounter)
{
    ASSERT_UNUSED(hasFlags, hasFlags(getInstruction(bytecode, programCounter)));
    switch (static_cast<DFABytecodeFlagsSize>(bytecode[programCounter++] & DFABytecodeFlagsSizeMask)) {
    case DFABytecodeFlagsSize::UInt8:
        return consumeInteger<uint8_t>(bytecode, programCounter);
    case DFABytecodeFlagsSize::UInt16:
        return consumeInteger<uint16_t>(bytecode, programCounter);
    case DFABytecodeFlagsSize::UInt24:
        return consume24BitUnsignedInteger(bytecode, programCounter);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static uint32_t consumeAction(std::span<const uint8_t> bytecode, uint32_t& programCounter, uint32_t instructionLocation)
{
    ASSERT_UNUSED(hasAction, hasAction(getInstruction(bytecode, instructionLocation)));
    ASSERT(programCounter > instructionLocation);
    switch (static_cast<DFABytecodeActionSize>(bytecode[instructionLocation] & DFABytecodeActionSizeMask)) {
    case DFABytecodeActionSize::UInt8:
        return consumeInteger<uint8_t>(bytecode, programCounter);
    case DFABytecodeActionSize::UInt16:
        return consumeInteger<uint16_t>(bytecode, programCounter);
    case DFABytecodeActionSize::UInt24:
        return consume24BitUnsignedInteger(bytecode, programCounter);
    case DFABytecodeActionSize::UInt32:
        return consumeInteger<uint32_t>(bytecode, programCounter);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static DFABytecodeJumpSize getJumpSize(std::span<const uint8_t> bytecode, uint32_t index)
{
    auto jumpSize = static_cast<DFABytecodeJumpSize>(getBits<uint8_t>(bytecode, index) & DFABytecodeJumpSizeMask);
    ASSERT(jumpSize == DFABytecodeJumpSize::Int32
        || jumpSize == DFABytecodeJumpSize::Int24
        || jumpSize == DFABytecodeJumpSize::Int16
        || jumpSize == DFABytecodeJumpSize::Int8);
    return jumpSize;
}

static int32_t getJumpDistance(std::span<const uint8_t> bytecode, uint32_t index, DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case DFABytecodeJumpSize::Int8:
        return getBits<int8_t>(bytecode, index);
    case DFABytecodeJumpSize::Int16:
        return getBits<int16_t>(bytecode, index);
    case DFABytecodeJumpSize::Int24:
        return getBits<uint16_t>(bytecode, index) | (static_cast<int32_t>(getBits<int8_t>(bytecode, index + sizeof(uint16_t))) << 16);
    case DFABytecodeJumpSize::Int32:
        return getBits<int32_t>(bytecode, index);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void DFABytecodeInterpreter::interpretAppendAction(uint32_t& programCounter, Actions& actions)
{
    ASSERT(getInstruction(m_bytecode, programCounter) == DFABytecodeInstruction::AppendAction);
    auto instructionLocation = programCounter++;
    actions.add(consumeAction(m_bytecode, programCounter, instructionLocation));
}

void DFABytecodeInterpreter::interpretTestFlagsAndAppendAction(uint32_t& programCounter, ResourceFlags flags, Actions& actions)
{
    auto instructionLocation = programCounter;
    auto flagsToCheck = consumeResourceFlagsAndInstruction(m_bytecode, programCounter);

    ResourceFlags loadTypeFlags = flagsToCheck & LoadTypeMask;
    ResourceFlags loadContextFlags = flagsToCheck & LoadContextMask;
    ResourceFlags resourceTypeFlags = flagsToCheck & ResourceTypeMask;

    bool loadTypeMatches = loadTypeFlags ? (loadTypeFlags & flags) : true;
    bool loadContextMatches = loadContextFlags ? (loadContextFlags & flags) : true;
    bool resourceTypeMatches = resourceTypeFlags ? (resourceTypeFlags & flags) : true;
    
    auto actionWithoutFlags = consumeAction(m_bytecode, programCounter, instructionLocation);
    if (loadTypeMatches && loadContextMatches && resourceTypeMatches) {
        uint64_t actionAndFlags = (static_cast<uint64_t>(flagsToCheck) << 32) | static_cast<uint64_t>(actionWithoutFlags);
        actions.add(actionAndFlags);
    }
}

template<bool caseSensitive>
inline void DFABytecodeInterpreter::interpetJumpTable(std::span<const char> url, uint32_t& urlIndex, uint32_t& programCounter)
{
    DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, programCounter);

    char c = urlIndex < url.size() ? url[urlIndex] : 0;
    char character = caseSensitive ? c : toASCIILower(c);
    uint8_t firstCharacter = getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction));
    uint8_t lastCharacter = getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t));
    if (character >= firstCharacter && character <= lastCharacter) {
        uint32_t startOffset = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
        uint32_t jumpLocation = startOffset + (character - firstCharacter) * jumpSizeInBytes(jumpSize);
        programCounter += getJumpDistance(m_bytecode, jumpLocation, jumpSize);
        urlIndex++; // This represents an edge in the DFA.
    } else
        programCounter += sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t) + jumpSizeInBytes(jumpSize) * (lastCharacter - firstCharacter + 1);
}

auto DFABytecodeInterpreter::actionsMatchingEverything() -> Actions
{
    Actions actions;

    // DFA header.
    uint32_t dfaBytecodeLength = getBits<uint32_t>(m_bytecode, 0);
    uint32_t programCounter = sizeof(DFAHeader);

    while (programCounter < dfaBytecodeLength) {
        DFABytecodeInstruction instruction = getInstruction(m_bytecode, programCounter);
        if (instruction == DFABytecodeInstruction::AppendAction)
            interpretAppendAction(programCounter, actions);
        else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendAction) {
            auto programCounterAtInstruction = programCounter;
            consumeResourceFlagsAndInstruction(m_bytecode, programCounter);
            consumeAction(m_bytecode, programCounter, programCounterAtInstruction);
        } else
            break;
    }
    return actions;
}

auto DFABytecodeInterpreter::interpret(const String& urlString, ResourceFlags flags) -> Actions
{
    CString urlCString;
    std::span<const char> url;
    if (LIKELY(urlString.is8Bit()))
        url = { reinterpret_cast<const char*>(urlString.characters8()), urlString.length() };
    else {
        urlCString = urlString.utf8();
        url = { urlCString.data(), urlCString.length() };
    }
    ASSERT(url.data());
    
    Actions actions;
    
    uint32_t programCounter = 0;
    while (programCounter < m_bytecode.size()) {

        // DFA header.
        uint32_t dfaStart = programCounter;
        uint32_t dfaBytecodeLength = getBits<uint32_t>(m_bytecode, programCounter);
        programCounter += sizeof(uint32_t);

        // Skip the actions without flags on the DFA root. These are accessed via actionsMatchingEverything.
        if (!dfaStart) {
            while (programCounter < dfaBytecodeLength) {
                DFABytecodeInstruction instruction = getInstruction(m_bytecode, programCounter);
                if (instruction == DFABytecodeInstruction::AppendAction) {
                    auto instructionLocation = programCounter++;
                    consumeAction(m_bytecode, programCounter, instructionLocation);
                } else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendAction)
                    interpretTestFlagsAndAppendAction(programCounter, flags, actions);
                else
                    break;
            }
            if (programCounter >= m_bytecode.size())
                return actions;
        } else {
            ASSERT_WITH_MESSAGE(getInstruction(m_bytecode, programCounter) != DFABytecodeInstruction::AppendAction
                && getInstruction(m_bytecode, programCounter) != DFABytecodeInstruction::TestFlagsAndAppendAction,
                "Triggers that match everything should only be in the first DFA.");
        }
        
        // Interpret the bytecode from this DFA.
        // This should always terminate if interpreting correctly compiled bytecode.
        uint32_t urlIndex = 0;
        while (true) {
            ASSERT(programCounter <= m_bytecode.size());
            switch (getInstruction(m_bytecode, programCounter)) {

            case DFABytecodeInstruction::Terminate:
                goto nextDFA;
                    
            case DFABytecodeInstruction::CheckValueCaseSensitive: {
                if (urlIndex > url.size())
                    goto nextDFA;

                // Check to see if the next character in the url is the value stored with the bytecode.
                char character = urlIndex < url.size() ? url[urlIndex] : 0;
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, programCounter);
                if (character == getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, jumpLocation, jumpSize);
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::CheckValueCaseInsensitive: {
                if (urlIndex > url.size())
                    goto nextDFA;

                // Check to see if the next character in the url is the value stored with the bytecode.
                char character = urlIndex < url.size() ? toASCIILower(url[urlIndex]) : 0;
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, programCounter);
                if (character == getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, jumpLocation, jumpSize);
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::JumpTableCaseInsensitive:
                if (urlIndex > url.size())
                    goto nextDFA;

                interpetJumpTable<false>(url, urlIndex, programCounter);
                break;
            case DFABytecodeInstruction::JumpTableCaseSensitive:
                if (urlIndex > url.size())
                    goto nextDFA;

                interpetJumpTable<true>(url, urlIndex, programCounter);
                break;
                    
            case DFABytecodeInstruction::CheckValueRangeCaseSensitive: {
                if (urlIndex > url.size())
                    goto nextDFA;
                
                char character = urlIndex < url.size() ? url[urlIndex] : 0;
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, programCounter);
                if (character >= getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction))
                    && character <= getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, jumpLocation, jumpSize);
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::CheckValueRangeCaseInsensitive: {
                if (urlIndex > url.size())
                    goto nextDFA;
                
                char character = urlIndex < url.size() ? toASCIILower(url[urlIndex]) : 0;
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, programCounter);
                if (character >= getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction))
                    && character <= getBits<uint8_t>(m_bytecode, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, jumpLocation, jumpSize);
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::Jump: {
                if (urlIndex >= url.size())
                    goto nextDFA;
                
                uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction);
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, programCounter);
                programCounter += getJumpDistance(m_bytecode, jumpLocation, jumpSize);
                urlIndex++; // This represents an edge in the DFA.
                break;
            }
                    
            case DFABytecodeInstruction::AppendAction:
                interpretAppendAction(programCounter, actions);
                break;
                    
            case DFABytecodeInstruction::TestFlagsAndAppendAction:
                interpretTestFlagsAndAppendAction(programCounter, flags, actions);
                break;
                    
            default:
                RELEASE_ASSERT_NOT_REACHED(); // Invalid bytecode.
            }
            // We should always terminate before or at an imaginary null character at the end of a String.
            ASSERT(urlIndex <= url.size() + 1);
        }
        RELEASE_ASSERT_NOT_REACHED(); // The while loop can only be exited using goto nextDFA.
        nextDFA:
        ASSERT(dfaBytecodeLength);
        programCounter = dfaStart + dfaBytecodeLength;
    }
    return actions;
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
