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

#pragma once

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
    
namespace ContentExtensions {

typedef uint8_t DFABytecode;

// Increment ContentExtensionStore::CurrentContentExtensionFileVersion
// when making any non-backwards-compatible changes to the bytecode.
// FIXME: Changes here should not require changes in WebKit2.  Move all versioning to WebCore.
enum class DFABytecodeInstruction : uint8_t {

    // CheckValue has two arguments:
    // The value to check (1 byte),
    // The distance to jump if the values are equal (1-4 bytes, signed).
    CheckValueCaseInsensitive = 0x0,
    CheckValueCaseSensitive = 0x1,

    // Jump table if the input value is within a certain range.
    // The lower value (1 byte).
    // The higher value (1 byte).
    // The distance to jump if the value is in the range
    // for every character in the range (1-4 bytes, signed).
    JumpTableCaseInsensitive = 0x2,
    JumpTableCaseSensitive = 0x3,

    // Jump to an offset if the input value is within a certain range.
    // The lower value (1 byte).
    // The higher value (1 byte).
    // The distance to jump if the value is in the range (1-4 bytes, signed).
    CheckValueRangeCaseInsensitive = 0x4,
    CheckValueRangeCaseSensitive = 0x5,

    // AppendAction has one argument:
    // The action to append (4 bytes).
    AppendAction = 0x6,
    AppendActionWithIfCondition = 0x7,
    
    // TestFlagsAndAppendAction has two arguments:
    // The flags to check before appending (2 bytes).
    // The action to append (4 bytes).
    TestFlagsAndAppendAction = 0x8,
    TestFlagsAndAppendActionWithIfCondition = 0x9,

    // Terminate has no arguments.
    Terminate = 0xA,

    // Jump has one argument:
    // The distance to jump (1-4 bytes, signed).
    Jump = 0xB,
};

// The last four bits contain the instruction type.
const uint8_t DFABytecodeInstructionMask = 0x0F;
const uint8_t DFABytecodeJumpSizeMask = 0xF0;

// DFA bytecode starts with a 4 byte header which contains the size of this DFA.
typedef uint32_t DFAHeader;

// A DFABytecodeJumpSize is stored in the top four bits of the DFABytecodeInstructions that have a jump.
enum DFABytecodeJumpSize {
    Int8 = 0x10,
    Int16 = 0x20,
    Int24 = 0x30,
    Int32 = 0x40,
};
const int32_t Int24Max = (1 << 23) - 1;
const int32_t Int24Min = -(1 << 23);

static inline DFABytecodeJumpSize smallestPossibleJumpSize(int32_t longestPossibleJump)
{
    if (longestPossibleJump <= std::numeric_limits<int8_t>::max() && longestPossibleJump >= std::numeric_limits<int8_t>::min())
        return Int8;
    if (longestPossibleJump <= std::numeric_limits<int16_t>::max() && longestPossibleJump >= std::numeric_limits<int16_t>::min())
        return Int16;
    if (longestPossibleJump <= Int24Max && longestPossibleJump >= Int24Min)
        return Int24;
    return Int32;
}
    
static inline size_t instructionSizeWithArguments(DFABytecodeInstruction instruction)
{
    switch (instruction) {
    case DFABytecodeInstruction::CheckValueCaseSensitive:
    case DFABytecodeInstruction::CheckValueCaseInsensitive:
    case DFABytecodeInstruction::JumpTableCaseInsensitive:
    case DFABytecodeInstruction::JumpTableCaseSensitive:
    case DFABytecodeInstruction::CheckValueRangeCaseSensitive:
    case DFABytecodeInstruction::CheckValueRangeCaseInsensitive:
    case DFABytecodeInstruction::Jump:
        RELEASE_ASSERT_NOT_REACHED(); // Variable instruction size.
    case DFABytecodeInstruction::AppendAction:
    case DFABytecodeInstruction::AppendActionWithIfCondition:
        return sizeof(DFABytecodeInstruction) + sizeof(uint32_t);
    case DFABytecodeInstruction::TestFlagsAndAppendAction:
    case DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition:
        return sizeof(DFABytecodeInstruction) + sizeof(uint16_t) + sizeof(uint32_t);
    case DFABytecodeInstruction::Terminate:
        return sizeof(DFABytecodeInstruction);
    }
    RELEASE_ASSERT_NOT_REACHED();
}
    
} // namespace ContentExtensions    
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
