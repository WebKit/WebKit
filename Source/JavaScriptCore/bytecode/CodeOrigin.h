/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CodeOrigin_h
#define CodeOrigin_h

#include <wtf/StdLibExtras.h>

namespace JSC {

struct InlineCallFrame;
class ExecutableBase;

struct CodeOrigin {
    uint32_t bytecodeIndex;
    InlineCallFrame* inlineCallFrame;
    
    CodeOrigin()
        : bytecodeIndex(std::numeric_limits<uint32_t>::max())
        , inlineCallFrame(0)
    {
    }
    
    explicit CodeOrigin(uint32_t bytecodeIndex)
        : bytecodeIndex(bytecodeIndex)
        , inlineCallFrame(0)
    {
    }
    
    explicit CodeOrigin(uint32_t bytecodeIndex, InlineCallFrame* inlineCallFrame)
        : bytecodeIndex(bytecodeIndex)
        , inlineCallFrame(inlineCallFrame)
    {
    }
    
    bool isSet() const { return bytecodeIndex != std::numeric_limits<uint32_t>::max(); }
};

struct InlineCallFrame {
    ExecutableBase* executable;
    unsigned stackOffset;
    unsigned calleeVR;
    CodeOrigin caller;
    unsigned numArgumentsIncludingThis;
};

struct CodeOriginAtCallReturnOffset {
    CodeOrigin codeOrigin;
    unsigned callReturnOffset;
};

inline unsigned getCallReturnOffsetForCodeOrigin(CodeOriginAtCallReturnOffset* data)
{
    return data->callReturnOffset;
}

} // namespace JSC

#endif // CodeOrigin_h

