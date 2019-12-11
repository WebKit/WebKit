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
static inline IntType getBits(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index)
{
    ASSERT_UNUSED(bytecodeLength, index + sizeof(IntType) <= bytecodeLength);
    return *reinterpret_cast<const IntType*>(&bytecode[index]);
}

static inline DFABytecodeInstruction getInstruction(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index)
{
    return static_cast<DFABytecodeInstruction>(getBits<uint8_t>(bytecode, bytecodeLength, index) & DFABytecodeInstructionMask);
}

static inline size_t jumpSizeInBytes(DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case Int8:
        return sizeof(int8_t);
    case Int16:
        return sizeof(int16_t);
    case Int24:
        return sizeof(uint16_t) + sizeof(int8_t);
    case Int32:
        return sizeof(int32_t);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static inline DFABytecodeJumpSize getJumpSize(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index)
{
    DFABytecodeJumpSize jumpSize = static_cast<DFABytecodeJumpSize>(getBits<uint8_t>(bytecode, bytecodeLength, index) & DFABytecodeJumpSizeMask);
    ASSERT(jumpSize == DFABytecodeJumpSize::Int32 || jumpSize == DFABytecodeJumpSize::Int24 || jumpSize == DFABytecodeJumpSize::Int16 || jumpSize == DFABytecodeJumpSize::Int8);
    return jumpSize;
}

static inline int32_t getJumpDistance(const DFABytecode* bytecode, uint32_t bytecodeLength, uint32_t index, DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case Int8:
        return getBits<int8_t>(bytecode, bytecodeLength, index);
    case Int16:
        return getBits<int16_t>(bytecode, bytecodeLength, index);
    case Int24:
        return getBits<uint16_t>(bytecode, bytecodeLength, index) | (static_cast<int32_t>(getBits<int8_t>(bytecode, bytecodeLength, index + sizeof(uint16_t))) << 16);
    case Int32:
        return getBits<int32_t>(bytecode, bytecodeLength, index);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static inline bool matchesCondition(uint64_t actionAndFlags, const DFABytecodeInterpreter::Actions& conditionActions)
{
    bool ifCondition = actionAndFlags & IfConditionFlag;
    bool condition = conditionActions.contains(actionAndFlags);
    return ifCondition == condition;
}

void DFABytecodeInterpreter::interpretAppendAction(uint32_t& programCounter, Actions& actions, bool ifCondition)
{
    ASSERT(getInstruction(m_bytecode, m_bytecodeLength, programCounter) == DFABytecodeInstruction::AppendAction
        || getInstruction(m_bytecode, m_bytecodeLength, programCounter) == DFABytecodeInstruction::AppendActionWithIfCondition);
    uint64_t action = (ifCondition ? IfConditionFlag : 0) | static_cast<uint64_t>(getBits<uint32_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction)));
    if (!m_topURLActions || matchesCondition(action, *m_topURLActions))
        actions.add(action);
    
    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::AppendAction) == instructionSizeWithArguments(DFABytecodeInstruction::AppendActionWithIfCondition));
}

void DFABytecodeInterpreter::interpretTestFlagsAndAppendAction(uint32_t& programCounter, uint16_t flags, Actions& actions, bool ifCondition)
{
    ASSERT(getInstruction(m_bytecode, m_bytecodeLength, programCounter) == DFABytecodeInstruction::TestFlagsAndAppendAction
        || getInstruction(m_bytecode, m_bytecodeLength, programCounter) == DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition);
    uint16_t flagsToCheck = getBits<uint16_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction));

    uint16_t loadTypeFlags = flagsToCheck & LoadTypeMask;
    uint16_t resourceTypeFlags = flagsToCheck & ResourceTypeMask;
    
    bool loadTypeMatches = loadTypeFlags ? (loadTypeFlags & flags) : true;
    bool resourceTypeMatches = resourceTypeFlags ? (resourceTypeFlags & flags) : true;
    
    if (loadTypeMatches && resourceTypeMatches) {
        uint64_t actionAndFlags = (ifCondition ? IfConditionFlag : 0) | (static_cast<uint64_t>(flagsToCheck) << 32) | static_cast<uint64_t>(getBits<uint32_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint16_t)));
        if (!m_topURLActions || matchesCondition(actionAndFlags, *m_topURLActions))
            actions.add(actionAndFlags);
    }
    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction);
    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction) == instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition));
}

template<bool caseSensitive>
inline void DFABytecodeInterpreter::interpetJumpTable(const char* url, uint32_t& urlIndex, uint32_t& programCounter, bool& urlIndexIsAfterEndOfString)
{
    DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter);

    char character = caseSensitive ? url[urlIndex] : toASCIILower(url[urlIndex]);
    uint8_t firstCharacter = getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction));
    uint8_t lastCharacter = getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t));
    if (character >= firstCharacter && character <= lastCharacter) {
        uint32_t startOffset = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
        uint32_t jumpLocation = startOffset + (character - firstCharacter) * jumpSizeInBytes(jumpSize);
        programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, jumpSize);
        if (!character)
            urlIndexIsAfterEndOfString = true;
        urlIndex++; // This represents an edge in the DFA.
    } else
        programCounter += sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t) + jumpSizeInBytes(jumpSize) * (lastCharacter - firstCharacter + 1);
}

DFABytecodeInterpreter::Actions DFABytecodeInterpreter::actionsMatchingEverything()
{
    Actions actions;

    // DFA header.
    uint32_t dfaBytecodeLength = getBits<uint32_t>(m_bytecode, m_bytecodeLength, 0);
    uint32_t programCounter = sizeof(uint32_t);

    while (programCounter < dfaBytecodeLength) {
        DFABytecodeInstruction instruction = getInstruction(m_bytecode, m_bytecodeLength, programCounter);
        if (instruction == DFABytecodeInstruction::AppendAction)
            interpretAppendAction(programCounter, actions, false);
        else if (instruction == DFABytecodeInstruction::AppendActionWithIfCondition)
            interpretAppendAction(programCounter, actions, true);
        else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendAction)
            programCounter += instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction);
        else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition)
            programCounter += instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition);
        else
            break;
    }
    return actions;
}
    
DFABytecodeInterpreter::Actions DFABytecodeInterpreter::interpretWithConditions(const CString& urlCString, uint16_t flags, const DFABytecodeInterpreter::Actions& topURLActions)
{
    ASSERT(!m_topURLActions);
    m_topURLActions = &topURLActions;
    DFABytecodeInterpreter::Actions actions = interpret(urlCString, flags);
    m_topURLActions = nullptr;
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
        uint32_t dfaBytecodeLength = getBits<uint32_t>(m_bytecode, m_bytecodeLength, programCounter);
        programCounter += sizeof(uint32_t);

        // Skip the actions without flags on the DFA root. These are accessed via actionsMatchingEverything.
        if (!dfaStart) {
            while (programCounter < dfaBytecodeLength) {
                DFABytecodeInstruction instruction = getInstruction(m_bytecode, m_bytecodeLength, programCounter);
                if (instruction == DFABytecodeInstruction::AppendAction)
                    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
                else if (instruction == DFABytecodeInstruction::AppendActionWithIfCondition)
                    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendActionWithIfCondition);
                else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendAction)
                    interpretTestFlagsAndAppendAction(programCounter, flags, actions, false);
                else if (instruction == DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition)
                    interpretTestFlagsAndAppendAction(programCounter, flags, actions, true);
                else
                    break;
            }
            if (programCounter >= m_bytecodeLength)
                return actions;
        } else {
            ASSERT_WITH_MESSAGE(getInstruction(m_bytecode, m_bytecodeLength, programCounter) != DFABytecodeInstruction::AppendAction
                && getInstruction(m_bytecode, m_bytecodeLength, programCounter) != DFABytecodeInstruction::AppendActionWithIfCondition
                && getInstruction(m_bytecode, m_bytecodeLength, programCounter) != DFABytecodeInstruction::TestFlagsAndAppendAction
                && getInstruction(m_bytecode, m_bytecodeLength, programCounter) != DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition,
                "Triggers that match everything should only be in the first DFA.");
        }
        
        // Interpret the bytecode from this DFA.
        // This should always terminate if interpreting correctly compiled bytecode.
        uint32_t urlIndex = 0;
        bool urlIndexIsAfterEndOfString = false;
        while (true) {
            ASSERT(programCounter <= m_bytecodeLength);
            switch (getInstruction(m_bytecode, m_bytecodeLength, programCounter)) {

            case DFABytecodeInstruction::Terminate:
                goto nextDFA;
                    
            case DFABytecodeInstruction::CheckValueCaseSensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;

                // Check to see if the next character in the url is the value stored with the bytecode.
                char character = url[urlIndex];
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter);
                if (character == getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, jumpSize);
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
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter);
                if (character == getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, jumpSize);
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else
                    programCounter += sizeof(DFABytecodeInstruction) + sizeof(uint8_t) + jumpSizeInBytes(jumpSize);
                break;
            }

            case DFABytecodeInstruction::JumpTableCaseInsensitive:
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;

                interpetJumpTable<false>(url, urlIndex, programCounter, urlIndexIsAfterEndOfString);
                break;
            case DFABytecodeInstruction::JumpTableCaseSensitive:
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;

                interpetJumpTable<true>(url, urlIndex, programCounter, urlIndexIsAfterEndOfString);
                break;
                    
            case DFABytecodeInstruction::CheckValueRangeCaseSensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;
                
                char character = url[urlIndex];
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter);
                if (character >= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction))
                    && character <= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, jumpSize);
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
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter);
                if (character >= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction))
                    && character <= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecodeInstruction) + sizeof(uint8_t))) {
                    uint32_t jumpLocation = programCounter + sizeof(DFABytecodeInstruction) + 2 * sizeof(uint8_t);
                    programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, jumpSize);
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
                DFABytecodeJumpSize jumpSize = getJumpSize(m_bytecode, m_bytecodeLength, programCounter);
                programCounter += getJumpDistance(m_bytecode, m_bytecodeLength, jumpLocation, jumpSize);
                urlIndex++; // This represents an edge in the DFA.
                break;
            }
                    
            case DFABytecodeInstruction::AppendAction:
                interpretAppendAction(programCounter, actions, false);
                break;
                    
            case DFABytecodeInstruction::AppendActionWithIfCondition:
                interpretAppendAction(programCounter, actions, true);
                break;
                    
            case DFABytecodeInstruction::TestFlagsAndAppendAction:
                interpretTestFlagsAndAppendAction(programCounter, flags, actions, false);
                break;
            
            case DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition:
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
