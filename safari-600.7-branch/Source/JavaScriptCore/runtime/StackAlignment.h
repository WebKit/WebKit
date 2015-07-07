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

#ifndef StackAlignment_h
#define StackAlignment_h

#include "JSCJSValue.h"
#include "JSStack.h"
#include <wtf/MathExtras.h>

namespace JSC {

// NB. Different platforms may have different requirements here. But 16 bytes is very common.
inline unsigned stackAlignmentBytes() { return 16; }

inline unsigned stackAlignmentRegisters()
{
    return stackAlignmentBytes() / sizeof(EncodedJSValue);
}

// Align argument count taking into account the CallFrameHeaderSize may be
// an "unaligned" count of registers.
inline unsigned roundArgumentCountToAlignFrame(unsigned argumentCount)
{
    return WTF::roundUpToMultipleOf(stackAlignmentRegisters(), argumentCount + JSStack::CallFrameHeaderSize) - JSStack::CallFrameHeaderSize;
}

// Align local register count to make the last local end on a stack aligned address given the
// CallFrame is at an address that is stack aligned minus JSStack::CallerFrameAndPCSize
inline unsigned roundLocalRegisterCountForFramePointerOffset(unsigned localRegisterCount)
{
    return WTF::roundUpToMultipleOf(stackAlignmentRegisters(), localRegisterCount + JSStack::CallerFrameAndPCSize) - JSStack::CallerFrameAndPCSize;
}

inline unsigned logStackAlignmentRegisters()
{
    return WTF::fastLog2(stackAlignmentRegisters());
}

} // namespace JSC

#endif // StackAlignment_h

