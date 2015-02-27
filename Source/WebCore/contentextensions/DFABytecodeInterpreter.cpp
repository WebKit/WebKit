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
static inline IntType getBits(const Vector<DFABytecode>& bytecode, unsigned index)
{
    return *reinterpret_cast<const IntType*>(&bytecode[index]);
}

DFABytecodeInterpreter::Actions DFABytecodeInterpreter::interpret(const CString& urlCString)
{
    const char* url = urlCString.data();
    ASSERT(url);
    
    unsigned programCounter = 0;
    unsigned urlIndex = 0;
    Actions actions;
    
    // This should always terminate if interpreting correctly compiled bytecode.
    while (true) {
        switch (static_cast<DFABytecodeInstruction>(bytecode[programCounter])) {

        case DFABytecodeInstruction::Terminate:
            return actions;

        case DFABytecodeInstruction::CheckValue:
            // Check to see if the next character in the url is the value stored with the bytecode.
            if (!url[urlIndex])
                return actions; // Reached null character at end.
            if (url[urlIndex] == getBits<uint8_t>(bytecode, programCounter + sizeof(DFABytecode))) {
                programCounter = getBits<unsigned>(bytecode, programCounter + sizeof(DFABytecode) + sizeof(uint8_t));
                urlIndex++; // This represents an edge in the DFA.
            } else
                programCounter += instructionSizeWithArguments(DFABytecodeInstruction::CheckValue);
            break;

        case DFABytecodeInstruction::Jump:
            programCounter = getBits<unsigned>(bytecode, programCounter + sizeof(DFABytecode));
            urlIndex++; // This represents an edge in the DFA.
            break;

        case DFABytecodeInstruction::AppendAction:
            actions.add(static_cast<uint64_t>(getBits<unsigned>(bytecode, programCounter + sizeof(DFABytecode))));
            programCounter += instructionSizeWithArguments(DFABytecodeInstruction::AppendAction);
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED(); // Invalid bytecode.
        }
        ASSERT(urlIndex <= urlCString.length()); // We should always terminate at a null character at the end of a String.
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace ContentExtensions
    
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
