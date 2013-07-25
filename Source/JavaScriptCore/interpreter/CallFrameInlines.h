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

namespace JSC  {

inline uint32_t CallFrame::Location::encode(CallFrame::Location::Type type, uint32_t bits)
{
#if USE(JSVALUE64)
    ASSERT(!(bits & s_shiftedMask));
    ASSERT(!(type & ~s_mask));
    return bits | (type << s_shift);
#else
    ASSERT(!(type & ~s_mask));
    if (type & CodeOriginIndex)
        bits = (bits << s_shift);
    ASSERT(!(bits & s_mask));
    bits |= type;
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

inline bool CallFrame::Location::isBytecodeOffset(uint32_t bits)
{
    return !isCodeOriginIndex(bits);
}

inline bool CallFrame::Location::isCodeOriginIndex(uint32_t bits)
{
#if USE(JSVALUE64)
    Type type = static_cast<Type>(bits >> s_shift);
    return !!(type & CodeOriginIndex);
#else
    return !!(bits & CodeOriginIndex);
#endif
}

inline bool CallFrame::Location::isInlinedCode(uint32_t bits)
{
#if USE(JSVALUE64)
    Type type = static_cast<Type>(bits >> s_shift);
    return !!(type & IsInlinedCode);
#else
    return !!(bits & IsInlinedCode);
#endif
}

inline bool CallFrame::isInlinedFrame() const
{
    return Location::isInlinedCode(locationAsRawBits());
}

inline void CallFrame::setIsInlinedFrame()
{
    ASSERT(codeBlock());
    uint32_t bits = Location::encode(Location::IsInlinedCode, locationAsRawBits());
    setLocationAsRawBits(bits);
    ASSERT(isInlinedFrame());
}

inline bool CallFrame::hasLocationAsBytecodeOffset() const
{
    return Location::isBytecodeOffset(locationAsRawBits());
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
    setLocationAsRawBits(Location::encode(Location::BytecodeOffset, offset));
    ASSERT(hasLocationAsBytecodeOffset());
}
#endif // USE(JSVALUE64)

inline unsigned CallFrame::locationAsCodeOriginIndex() const
{
    ASSERT(hasLocationAsCodeOriginIndex());
    ASSERT(codeBlock());
    return Location::decode(locationAsRawBits());
}

} // namespace JSC

#endif // CallFrameInlines_h
