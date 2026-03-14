/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "MicrotaskCall.h"

#include "CodeBlock.h"
#include "Interpreter.h"
#include "JSFunction.h"
#include "ThrowScope.h"

namespace JSC {

void MicrotaskCall::initialize(VM& vm, JSFunction* function)
{
    m_addressForCall = nullptr;
    m_functionExecutable = function->jsExecutable();
    relink(vm, function);
}

void MicrotaskCall::relink(VM& vm, JSFunction* function)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Unlink from any prior CodeBlock before we reset state.
    if (isOnList())
        remove();

    auto* newCodeBlock = vm.interpreter.prepareForMicrotaskCall(*this, function);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());
    m_codeBlock = newCodeBlock;
    m_numParameters = newCodeBlock->numParameters();
}

void MicrotaskCall::unlinkOrUpgradeImpl(VM&, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock)
{
    if (isOnList())
        remove();

    if (newCodeBlock && m_codeBlock == oldCodeBlock) {
        newCodeBlock->m_shouldAlwaysBeInlined = false;
        m_addressForCall = newCodeBlock->jitCode()->addressForCall();
        m_codeBlock = newCodeBlock;
        m_numParameters = newCodeBlock->numParameters();
        newCodeBlock->linkIncomingCall(nullptr, this);
        return;
    }
    m_addressForCall = nullptr;
}

} // namespace JSC
