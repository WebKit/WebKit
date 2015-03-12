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

#ifndef DFABytecode_h
#define DFABytecode_h

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
    
namespace ContentExtensions {

typedef uint8_t DFABytecode;

enum class DFABytecodeInstruction : uint8_t {

    // CheckValue has two arguments:
    // The value to check (1 byte),
    // The index to jump to if the values are equal (4 bytes).
    CheckValue,

    // AppendAction has one argument:
    // The action to append (4 bytes).
    AppendAction,
    
    // TestFlagsAndAppendAction has two arguments:
    // The flags to check before appending (2 bytes),
    // The action to append (4bytes).
    TestFlagsAndAppendAction,

    // Terminate has no arguments.
    Terminate,

    // Jump has one argument:
    // The index to jump to unconditionally (4 bytes).
    Jump,
};

static inline size_t instructionSizeWithArguments(DFABytecodeInstruction instruction)
{
    switch (instruction) {
    case DFABytecodeInstruction::CheckValue:
        return sizeof(DFABytecodeInstruction) + sizeof(uint8_t) + sizeof(unsigned);
    case DFABytecodeInstruction::AppendAction:
        return sizeof(DFABytecodeInstruction) + sizeof(unsigned);
    case DFABytecodeInstruction::TestFlagsAndAppendAction:
        return sizeof(DFABytecodeInstruction) + sizeof(uint16_t) + sizeof(unsigned);
    case DFABytecodeInstruction::Terminate:
        return sizeof(DFABytecodeInstruction);
    case DFABytecodeInstruction::Jump:
        return sizeof(DFABytecodeInstruction) + sizeof(unsigned);
    }
}
    
} // namespace ContentExtensions
    
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // DFABytecode_h
