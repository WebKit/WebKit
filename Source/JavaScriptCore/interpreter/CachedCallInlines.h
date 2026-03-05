/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Codeblog CORP.
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

#pragma once

#include <JavaScriptCore/CachedCall.h>

#if ASSERT_ENABLED
#include <JavaScriptCore/IntegrityInlines.h>
#endif

namespace JSC {

template<typename... Args> requires (std::is_convertible_v<Args, JSValue> && ...)
ALWAYS_INLINE JSValue CachedCall::callWithArguments(JSGlobalObject* globalObject, JSValue thisValue, Args... args)
{
    VM& vm = m_vm;
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_WITH_MESSAGE(!thisValue.isEmpty(), "Expected thisValue to be non-empty. Use jsUndefined() if you meant to use undefined.");
#if ASSERT_ENABLED
    if constexpr (sizeof...(args) > 0) {
        size_t argIndex = 0;
        auto checkArg = [&argIndex, &vm](JSValue arg) {
            ASSERT_WITH_MESSAGE(!arg.isEmpty(), "arguments[%zu] is JSValue(). Use jsUndefined() if you meant to make it undefined.", argIndex);
            if (arg.isCell())
                Integrity::auditCell(vm, arg.asCell());
            ++argIndex;
        };
        (checkArg(args), ...);
    }
#endif

#if CPU(ARM64) && CPU(ADDRESS64) && !ENABLE(C_LOOP)
    ASSERT(sizeof...(args) == static_cast<size_t>(m_protoCallFrame.argumentCount()));
    constexpr unsigned argumentCountIncludingThis = 1 + sizeof...(args);
    if constexpr (argumentCountIncludingThis <= 7) {
        if (m_numParameters <= argumentCountIncludingThis) [[likely]] {
            JSValue result = m_vm.interpreter.tryCallWithArguments(*this, thisValue, args...);
            RETURN_IF_EXCEPTION(scope, { });
            if (result)
                return result;
        }
    }
#endif

    clearArguments();
    setThis(thisValue);
    (appendArgument(args), ...);

    if (hasOverflowedArguments()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    RELEASE_AND_RETURN(scope, call());
}

}
