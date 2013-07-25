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
#include "CodeOrigin.h"

namespace JSC  {

inline bool CallFrame::hasLocationAsBytecodeOffset() const
{
    return !CodeOrigin::isHandle(locationAsRawBits());
}

inline bool CallFrame::hasLocationAsCodeOriginIndex() const
{
    return CodeOrigin::isHandle(locationAsRawBits());
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
    return locationAsRawBits();
}

inline void CallFrame::setLocationAsBytecodeOffset(unsigned offset)
{
    ASSERT(codeBlock());
    setLocationAsRawBits(offset);
    ASSERT(hasLocationAsBytecodeOffset());
}
#endif // USE(JSVALUE64)

inline unsigned CallFrame::locationAsCodeOriginIndex() const
{
    ASSERT(hasLocationAsCodeOriginIndex());
    ASSERT(codeBlock());
    return CodeOrigin::decodeHandle(locationAsRawBits());
}

} // namespace JSC

#endif // CallFrameInlines_h
