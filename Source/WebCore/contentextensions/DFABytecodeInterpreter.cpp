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

#include <wtf/text/CString.h>

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
    
namespace ContentExtensions {

template <typename IntType>
static inline IntType getBits(const DFABytecode* bytecode, unsigned bytecodeLength, unsigned index)
{
    ASSERT_UNUSED(bytecodeLength, index + sizeof(IntType) <= bytecodeLength);
    return *reinterpret_cast<const IntType*>(&bytecode[index]);
}
    
DFABytecodeInterpreter::Actions DFABytecodeInterpreter::actionsFromDFARoot()
{
    DFABytecodeInterpreter::Actions universalActionLocations;

    // Skip first DFA header. All universal actions are in the first DFA root.
    unsigned programCounter = sizeof(unsigned);
    while (static_cast<DFABytecodeInstruction>(m_bytecode[programCounter]) == DFABytecodeInstruction::AppendAction) {
        universalActionLocations.add(static_cast<uint64_t>(getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode))));
        programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
    }
    return universalActionLocations;
}
    
DFABytecodeInterpreter::Actions DFABytecodeInterpreter::interpret(const CString& urlCString, uint16_t flags)
{
    const char* url = urlCString.data();
    ASSERT(url);
    
    Actions actions;
    
    unsigned programCounter = 0;
    while (programCounter < m_bytecodeLength) {

        // DFA header.
        unsigned dfaStart = programCounter;
        unsigned dfaBytecodeLength = getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter);
        programCounter += sizeof(unsigned);

        // Skip the universal actions.
        // FIXME: Replace AppendAction with AppendActions to make this just one jump and make sure there aren't universal actions with flags.
        while (static_cast<DFABytecodeInstruction>(m_bytecode[programCounter]) == DFABytecodeInstruction::AppendAction)
            programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
        
        // Interpret the bytecode from this DFA.
        // This should always terminate if interpreting correctly compiled bytecode.
        unsigned urlIndex = 0;
        bool urlIndexIsAfterEndOfString = false;
        while (true) {
            ASSERT(programCounter <= m_bytecodeLength);
            switch (static_cast<DFABytecodeInstruction>(m_bytecode[programCounter])) {

            case DFABytecodeInstruction::Terminate:
                goto nextDFA;
                    
            case DFABytecodeInstruction::CheckValueCaseSensitive:
            case DFABytecodeInstruction::CheckValueCaseInsensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;

                char character = url[urlIndex];
                if (static_cast<DFABytecodeInstruction>(m_bytecode[programCounter]) == DFABytecodeInstruction::CheckValueCaseInsensitive)
                    character = toASCIILower(character);

                // Check to see if the next character in the url is the value stored with the bytecode.
                if (character == getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode))) {
                    programCounter = getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode) + sizeof(uint8_t));
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else {
                    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::CheckValueCaseSensitive);
                    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::CheckValueCaseSensitive) == instructionSizeWithArguments(DFABytecodeInstruction::CheckValueCaseInsensitive));
                }
                break;
            }
                    
            case DFABytecodeInstruction::CheckValueRangeCaseSensitive:
            case DFABytecodeInstruction::CheckValueRangeCaseInsensitive: {
                if (urlIndexIsAfterEndOfString)
                    goto nextDFA;
                
                char character = url[urlIndex];
                if (static_cast<DFABytecodeInstruction>(m_bytecode[programCounter]) == DFABytecodeInstruction::CheckValueRangeCaseInsensitive)
                    character = toASCIILower(character);
                if (character >= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode))
                    && character <= getBits<uint8_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode) + sizeof(uint8_t))) {
                    programCounter = getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode) + sizeof(uint8_t) + sizeof(uint8_t));
                    if (!character)
                        urlIndexIsAfterEndOfString = true;
                    urlIndex++; // This represents an edge in the DFA.
                } else {
                    programCounter += instructionSizeWithArguments(DFABytecodeInstruction::CheckValueRangeCaseSensitive);
                    ASSERT(instructionSizeWithArguments(DFABytecodeInstruction::CheckValueRangeCaseSensitive) == instructionSizeWithArguments(DFABytecodeInstruction::CheckValueRangeCaseInsensitive));
                }
                break;
            }

            case DFABytecodeInstruction::Jump:
                if (!url[urlIndex] || urlIndexIsAfterEndOfString)
                    goto nextDFA;
                
                programCounter = getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode));
                urlIndex++; // This represents an edge in the DFA.
                break;
                    
            case DFABytecodeInstruction::AppendAction:
                actions.add(static_cast<uint64_t>(getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode))));
                programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
                break;
                    
            case DFABytecodeInstruction::TestFlagsAndAppendAction:
                if (flags & getBits<uint16_t>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode)))
                    actions.add(static_cast<uint64_t>(getBits<unsigned>(m_bytecode, m_bytecodeLength, programCounter + sizeof(DFABytecode) + sizeof(uint16_t))));
                programCounter += instructionSizeWithArguments(DFABytecodeInstruction::TestFlagsAndAppendAction);
                break;
                    
            default:
                RELEASE_ASSERT_NOT_REACHED(); // Invalid bytecode.
            }
            // We should always terminate before or at a null character at the end of a String.
            ASSERT(urlIndex <= urlCString.length() || (urlIndexIsAfterEndOfString && urlIndex <= urlCString.length() + 1));
        }
        nextDFA:
        ASSERT(dfaBytecodeLength);
        programCounter = dfaStart + dfaBytecodeLength;
    }
    return actions;
}

} // namespace ContentExtensions
    
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
