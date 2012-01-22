/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MethodCallLinkStatus.h"

#include "CodeBlock.h"

namespace JSC {

MethodCallLinkStatus MethodCallLinkStatus::computeFor(CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
#if ENABLE(JIT) && ENABLE(VALUE_PROFILER)
    MethodCallLinkInfo& methodCall = profiledBlock->getMethodCallLinkInfo(bytecodeIndex);
    
    if (!methodCall.seen || !methodCall.cachedStructure)
        return MethodCallLinkStatus();
    
    if (methodCall.cachedPrototype.get() == profiledBlock->globalObject()->methodCallDummy()) {
        return MethodCallLinkStatus(
            methodCall.cachedStructure.get(),
            0,
            methodCall.cachedFunction.get(),
            0);
    }
    
    return MethodCallLinkStatus(
        methodCall.cachedStructure.get(),
        methodCall.cachedPrototypeStructure.get(),
        methodCall.cachedFunction.get(),
        methodCall.cachedPrototype.get());
#else // ENABLE(JIT)
    return MethodCallLinkStatus();
#endif // ENABLE(JIT)
}

} // namespace JSC
