/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef CallFrameInlines_h
#define CallFrameInlines_h

#include "CallFrame.h"
#include "CodeBlock.h"

namespace JSC  {

inline uint32_t CallFrame::Location::encode(CallFrame::Location::TypeTag tag, uint32_t bits)
{
#if USE(JSVALUE64)
    ASSERT(!(bits & s_shiftedMask));
    ASSERT(!(tag & ~s_mask));
    return bits | (tag << s_shift);
#else
    ASSERT(!(tag & ~s_mask));
    if (tag & CodeOriginIndexTag)
        bits = (bits << s_shift);
    ASSERT(!(bits & s_mask));
    bits |= tag;
    return bits;
#endif
}

inline uint32_t CallFrame::Location::decode(uint32_t bits)
{
#if USE(JSVALUE64)
    return bits & ~s_shiftedMask;
#else
    if (isCodeOriginIndex(bits))
        return bits >> s_shift;
    return bits & ~s_mask;
#endif
}

#if USE(JSVALUE64)
inline uint32_t CallFrame::Location::encodeAsBytecodeOffset(uint32_t bits)
{
    uint32_t encodedBits = encode(BytecodeLocationTag, bits);
    ASSERT(isBytecodeLocation(encodedBits));
    return encodedBits;
}
#else
inline uint32_t CallFrame::Location::encodeAsBytecodeInstruction(Instruction* instruction)
{
    uint32_t encodedBits = encode(BytecodeLocationTag, reinterpret_cast<uint32_t>(instruction));
    ASSERT(isBytecodeLocation(encodedBits));
    return encodedBits;
}
#endif

inline uint32_t CallFrame::Location::encodeAsCodeOriginIndex(uint32_t bits)
{
    uint32_t encodedBits = encode(CodeOriginIndexTag, bits);
    ASSERT(isCodeOriginIndex(encodedBits));
    return encodedBits;
}

inline bool CallFrame::Location::isBytecodeLocation(uint32_t bits)
{
    return !isCodeOriginIndex(bits);
}

inline bool CallFrame::Location::isCodeOriginIndex(uint32_t bits)
{
#if USE(JSVALUE64)
    TypeTag tag = static_cast<TypeTag>(bits >> s_shift);
    return !!(tag & CodeOriginIndexTag);
#else
    return !!(bits & CodeOriginIndexTag);
#endif
}

inline bool CallFrame::hasLocationAsBytecodeOffset() const
{
    return Location::isBytecodeLocation(locationAsRawBits());
}

inline bool CallFrame::hasLocationAsCodeOriginIndex() const
{
    return Location::isCodeOriginIndex(locationAsRawBits());
}

inline unsigned CallFrame::locationAsRawBits() const
{
    return this[JSStack::ArgumentCount].tag();
}

inline void CallFrame::setLocationAsRawBits(unsigned bits)
{
    this[JSStack::ArgumentCount].tag() = static_cast<int32_t>(bits);
}

#if USE(JSVALUE64)
inline unsigned CallFrame::locationAsBytecodeOffset() const
{
    ASSERT(hasLocationAsBytecodeOffset());
    ASSERT(codeBlock());
    return Location::decode(locationAsRawBits());
}

inline void CallFrame::setLocationAsBytecodeOffset(unsigned offset)
{
    ASSERT(codeBlock());
    setLocationAsRawBits(Location::encodeAsBytecodeOffset(offset));
    ASSERT(hasLocationAsBytecodeOffset());
}
#endif // USE(JSVALUE64)

inline unsigned CallFrame::locationAsCodeOriginIndex() const
{
    ASSERT(hasLocationAsCodeOriginIndex());
    ASSERT(codeBlock());
    return Location::decode(locationAsRawBits());
}

inline JSValue CallFrame::uncheckedActivation() const
{
    CodeBlock* codeBlock = this->codeBlock();
    RELEASE_ASSERT(codeBlock->needsActivation());
    VirtualRegister activationRegister = codeBlock->activationRegister();
    return registers()[activationRegister.offset()].jsValue();
}

} // namespace JSC

#endif // CallFrameInlines_h
