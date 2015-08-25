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

inline bool CallFrame::callSiteBitsAreBytecodeOffset() const
{
    ASSERT(codeBlock());
    switch (codeBlock()->jitType()) {
    case JITCode::InterpreterThunk:
    case JITCode::BaselineJIT:
        return true;
    case JITCode::None:
    case JITCode::HostCallThunk:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

inline bool CallFrame::callSiteBitsAreCodeOriginIndex() const
{
    ASSERT(codeBlock());
    switch (codeBlock()->jitType()) {
    case JITCode::DFGJIT:
    case JITCode::FTLJIT:
        return true;
    case JITCode::None:
    case JITCode::HostCallThunk:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

inline unsigned CallFrame::callSiteAsRawBits() const
{
    return this[JSStack::ArgumentCount].tag();
}

inline CallSiteIndex CallFrame::callSiteIndex() const
{
    return CallSiteIndex(callSiteAsRawBits());
}

inline bool CallFrame::hasActivation() const
{
    JSValue activation = uncheckedActivation();
    return !!activation && activation.isCell();
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
