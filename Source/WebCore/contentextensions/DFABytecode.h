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

namespace WebCore::ContentExtensions {

using DFABytecode = uint8_t;

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
    // The action to append (1-4 bytes).
    AppendAction = 0x6,
    AppendActionWithIfCondition = 0x7,
    
    // TestFlagsAndAppendAction has two arguments:
    // The flags to check before appending (1-3 bytes).
    // The action to append (1-4 bytes).
    TestFlagsAndAppendAction = 0x8,
    TestFlagsAndAppendActionWithIfCondition = 0x9,

    // Terminate has no arguments.
    Terminate = 0xA,

    // Jump has one argument:
    // The distance to jump (1-4 bytes, signed).
    Jump = 0xB,
};

// The last four bits contain the instruction type.
static constexpr uint8_t DFABytecodeInstructionMask = 0x0F;
static constexpr uint8_t DFABytecodeJumpSizeMask = 0xF0;
static constexpr uint8_t DFABytecodeFlagsSizeMask = 0x30;
static constexpr uint8_t DFABytecodeActionSizeMask = 0xC0;

// DFA bytecode starts with a 4 byte header which contains the size of this DFA.
using DFAHeader = uint32_t;

// DFABytecodeFlagsSize and DFABytecodeActionSize are stored in the top four bits of the DFABytecodeInstructions that have flags and actions.
enum class DFABytecodeFlagsSize : uint8_t {
    UInt8 = 0x10,
    UInt16 = 0x00, // Needs to be zero to be binary compatible with bytecode compiled with fixed-size flags.
    UInt24 = 0x20,
};
enum class DFABytecodeActionSize : uint8_t {
    UInt8 = 0x40,
    UInt16 = 0x80,
    UInt24 = 0xC0,
    UInt32 = 0x00, // Needs to be zero to be binary compatible with bytecode compiled with fixed-size actions.
};

// A DFABytecodeJumpSize is stored in the top four bits of the DFABytecodeInstructions that have a jump.
enum class DFABytecodeJumpSize : uint8_t {
    Int8 = 0x10,
    Int16 = 0x20,
    Int24 = 0x30,
    Int32 = 0x40,
};
static constexpr int32_t UInt24Max = (1 << 24) - 1;
static constexpr int32_t Int24Max = (1 << 23) - 1;
static constexpr int32_t Int24Min = -(1 << 23);
static constexpr size_t Int24Size = 3;
static constexpr size_t UInt24Size = 3;

static inline DFABytecodeJumpSize smallestPossibleJumpSize(int32_t longestPossibleJump)
{
    if (longestPossibleJump <= std::numeric_limits<int8_t>::max() && longestPossibleJump >= std::numeric_limits<int8_t>::min())
        return DFABytecodeJumpSize::Int8;
    if (longestPossibleJump <= std::numeric_limits<int16_t>::max() && longestPossibleJump >= std::numeric_limits<int16_t>::min())
        return DFABytecodeJumpSize::Int16;
    if (longestPossibleJump <= Int24Max && longestPossibleJump >= Int24Min)
        return DFABytecodeJumpSize::Int24;
    return DFABytecodeJumpSize::Int32;
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
