/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

namespace WebCore {
    
namespace ContentExtensions {

template <typename IntType>
static inline IntType getBits(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index, Vector<bool>& pagesUsed)
{
#if CONTENT_EXTENSIONS_MEMORY_REPORTING
    uint32_t page = index / CONTENT_EXTENSIONS_PAGE_SIZE;
    while (pagesUsed.size() < (bytecodeLength + CONTENT_EXTENSIONS_PAGE_SIZE - 1) / CONTENT_EXTENSIONS_PAGE_SIZE)
        pagesUsed.append(false);
    pagesUsed[page] = true;
#else
    UNUSED_PARAM(pagesUsed);
#endif
    ASSERT_UNUSED(bytecodeLength, index + sizeof(IntType) <= bytecodeLength);
    return *reinterpret_cast<const IntType*>(&bytecode[index]);
}

// FIXME: Get rid of pagesUsed. We don't need to keep track of the pages used.
static inline DFABytecodeInstruction getInstruction(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index, Vector<bool>& pagesUsed)
{
    return static_cast<DFABytecodeInstruction>(getBits<uint8_t>(bytecode, bytecodeLength, index, pagesUsed) & DFABytecodeInstructionMask);
}

static inline size_t jumpSizeInBytes(DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case Int8:
        return sizeof(int8_t);
    case Int16:
        return sizeof(int16_t);
    case Int32:
        return sizeof(int32_t);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static inline DFABytecodeJumpSize getJumpSize(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index, Vector<bool>& pagesUsed)
{
    DFABytecodeJumpSize jumpSize = static_cast<DFABytecodeJumpSize>(getBits<uint8_t>(bytecode, bytecodeLength, index, pagesUsed) & DFABytecodeJumpSizeMask);
    ASSERT(jumpSize == DFABytecodeJumpSize::Int32 || jumpSize == DFABytecodeJumpSize::Int16 || jumpSize == DFABytecodeJumpSize::Int8);
    return jumpSize;
}

static inline int32_t getJumpDistance(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index, Vector<bool>& pagesUsed, DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case Int8:
        return getBits<int8_t>(bytecode, bytecodeLength, index, pagesUsed);
    case Int16:
        return getBits<int16_t>(bytecode, bytecodeLength, index, pagesUsed);
    case Int32:
        return getBits<int32_t>(bytecode, bytecodeLength, index, pagesUsed);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void DFABytecodeInterpreter::interpretAppendAction(uint32_t& programCounter, Actions& actions, bool ifDomain)
{
    ASSERT(getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) == DFABytecodeInstruction::AppendAction
        || getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) == DFABytecodeInstruction::AppendActionWithIfDomain
        || getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) == DFABytecodeInstruction::AppendActionDefaultStylesheet);
    actions.add((ifDomain ? IfDomainFlag : 0) | static_cast<uint64_t>(getBits<uint32_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction), m_pagesUsed)));
    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::AppendAction) == instructionSizeWithArguments(DFABytecodeInstruction::AppendActionWithIfDomain));
    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::AppendAction) == instructionSizeWithArguments(DFABytecodeInstruction::AppendActionDefaultStylesheet));
}

void DFABytecodeInterpreter::interpretTestFlagsAndAppendAction(uint32_t& programCounter, uint16_t flags, Actions& actions, bool ifDomain)
{
    ASSERT(getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) == DFABytecodeInstruction::TestFlagsAndAppendAction
        || getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) == DFABytecodeInstruction::TestFlagsAndAppendActionWithIfDomain);
    uint16_t flagsToCheck = getBits<uint16_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction), m_pagesUsed);

    uint16_t loadTypeFlags = flagsToCheck & LoadTypeMask;
    uint16_t ressourceTypeFlags = flagsToCheck & ResourceTypeMask;
    
    bool loadTypeMatches = loadTypeFlags ? (loadTypeFlags & flags) : true;
    bool ressourceTypeMatches = ressourceTypeFlags ? (ressourceTypeFlags & flags) : true;
    
    if (loadTypeMatches && ressourceTypeMatches)
        actions.add((ifDomain ? IfDomainFlag : 0) | static_cast<uint64_t>(getBits<uint32_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint16_t), m_pagesUsed)));
    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction);
    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction) == instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendActionWithIfDomain));
}

DFABytecodeInterpreter::Actions DFABytecodeInterpreter::actionsForDefaultStylesheetFromDFARoot()
{
    Actions actions;

    // DFA header.
    uint32_t dfaBytecodeLength = getBits<uint32_t>(m_bytecode, m_bytecodeLength, 0, m_pagesUsed);
    uint32_t programCounter = sizeof(uint32_t);

    while (programCounter < dfaBytecodeLength) {
        DFABytecodeInstruction instruction = getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
        if (instruction == DFABytecodeInstruction::AppendActionDefaultStylesheet)
            interpretAppendAction(programCounter, actions, false);
        else if (instruction == DFABytecodeInstruction::AppendAction)
            programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
        else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendAction)
            programCounter += instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction);
        else {
            // actionsForDefaultStylesheetFromDFARoot should only be called on the DFA without domains,
            // which should never have any actions with if-domain.
            ASSERT(instruction != DFABytecodeInstruction::TestFlagsAndAppendActionWithIfDomain);
            ASSERT(instruction != DFABytecodeInstruction::AppendActionWithIfDomain);
            break;
        }
    }
    return actions;
}
    
DFABytecodeInterpreter::Actions DFABytecodeInterpreter::interpret(const CString& urlCString, uint16_t flags)
{
    const char* url = urlCString.data();
    ASSERT(url);
    
    Actions actions;
    
    uint32_t programCounter = 0;
    while (programCounter < m_bytecodeLength) {

        // DFA header.
        uint32_t dfaStart = programCounter;
        uint32_t dfaBytecodeLength = getBits<uint32_t>(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
        programCounter += sizeof(uint32_t);

        // Skip the default stylesheet actions on the DFA root. These are accessed via actionsForDefaultStylesheetFromDFARoot.
        if (!dfaStart) {
            while (programCounter < dfaBytecodeLength) {
                DFABytecodeInstruction instruction = getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
                if (instruction == DFABytecodeInstruction::AppendActionDefaultStylesheet)
                    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendActionDefaultStylesheet);
                else if (instruction == DFABytecodeInstruction::AppendAction)
                    interpretAppendAction(programCounter, actions, false);
                else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendAction)
                    interpretTestFlagsAndAppendAction(programCounter, flags, actions, false);
                else
                    break;
            }
            if (programCounter >= m_bytecodeLength)
                return actions;
        } else {
            ASSERT_WITH_MESSAGE(getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) != DFABytecodeInstruction::AppendAction
                && getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed) != DFABytecodeInstruction::TestFlagsAndAppendAction,
                "Triggers that match everything should only be in the first DFA.");
        }
        
        // Interpret the bytecode from this DFA.
        // This should always terminate if interpreting correctly compiled bytecode.
        uint32_t urlIndex = 0;
        bool urlIndexIsAfterEndOfString = false;
        while (true) {
            ASSERT(programCounter <= m_bytecodeLength);
            switch (getInstruction(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed)) {

            case DFABytecodeInstruction::Terminate:
                goto nextDFA;
                    
            case DFABytecodeInstruction::CheckValueCaseSensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;

                // Check to see if the next character in the url is the value stored with the bytecode.
                char character = url[urlIndex];
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
                if (character == getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction), m_pagesUsed)) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, m_pagesUsed, jumpSize);
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::CheckValueCaseInsensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;

                // Check to see if the next character in the url is the value stored with the bytecode.
                char character = toASCIILower(url[urlIndex]);
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
                if (character == getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction), m_pagesUsed)) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, m_pagesUsed, jumpSize);
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }
                    
            case DFABytecodeInstruction::CheckValueRangeCaseSensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;
                
                char character = url[urlIndex];
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
                if (character >= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction), m_pagesUsed)
                    && character <= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t), m_pagesUsed)) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, m_pagesUsed, jumpSize);
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::CheckValueRangeCaseInsensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;
                
                char character = toASCIILower(url[urlIndex]);
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
                if (character >= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction), m_pagesUsed)
                    && character <= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t), m_pagesUsed)) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, m_pagesUsed, jumpSize);
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::Jump: {
                if (!url[urlIndex] || urlIndexIsAfterEndOfString)
                    goto nextDFA;
                
                uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction);
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter, m_pagesUsed);
                programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, m_pagesUsed, jumpSize);
                urlIndex++; // This represents an edge in the DFA.
                break;
            }
                    
            case DFABytecodeInstruction::AppendAction:
                interpretAppendAction(programCounter, actions, false);
                break;
                    
            case DFABytecodeInstruction::AppendActionWithIfDomain:
                interpretAppendAction(programCounter, actions, true);
                break;
                    
            case DFABytecodeInstruction::TestFlagsAndAppendAction:
                interpretTestFlagsAndAppendAction(programCounter, flags, actions, false);
                break;
            
            case DFABytecodeInstruction::TestFlagsAndAppendActionWithIfDomain:
                interpretTestFlagsAndAppendAction(programCounter, flags, actions, true);
                break;
                    
            default:
                RELEASE_ASSERT_NOT_REACHED(); // Invalid bytecode.
            }
            // We should always terminate before or at a null character at the end of a String.
            ASSERT(urlIndex <= urlCString.length() || (urlIndexIsAfterEndOfString && urlIndex <= urlCString.length() + 1));
        }
        RELEASE_ASSERT_NOT_REACHED(); // The while loop can only be exited using goto nextDFA.
        nextDFA:
        ASSERT(dfaBytecodeLength);
        programCounter = dfaStart + dfaBytecodeLength;
    }
    return actions;
}

} // namespace ContentExtensions
    
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
