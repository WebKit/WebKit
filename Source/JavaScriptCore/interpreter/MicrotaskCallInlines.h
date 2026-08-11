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

#pragma once

#include <JavaScriptCore/LLIntThunks.h>
#include <JavaScriptCore/MicrotaskCall.h>
#include <JavaScriptCore/VMTrapsInlines.h>
#include <wtf/StackStats.h>

#if ASSERT_ENABLED
#include <JavaScriptCore/IntegrityInlines.h>
#endif

namespace JSC {

template<typename... Args> requires (std::is_convertible_v<Args, JSValue> && ...)
ALWAYS_INLINE JSValue MicrotaskCall::tryCallWithArguments(VM& vm, JSFunction* function, JSValue thisValue, JSCell* context, Args... args)
{
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

    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    ASSERT(vm.currentThreadIsHoldingAPILock());

    constexpr unsigned argumentCountIncludingThis = 1 + sizeof...(args);
#if (CPU(ARM64) || CPU(X86_64)) && CPU(ADDRESS64) && !ENABLE(C_LOOP)
    static_assert(argumentCountIncludingThis <= 7);
    if (m_numParameters <= argumentCountIncludingThis) [[likely]] {
        auto* entry = m_addressForCall;
        if (!entry) [[unlikely]] {
            DeferTraps deferTraps(vm);
            relink(vm, function);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, { });
            entry = m_addressForCall;
        }
        auto* codeBlock = m_codeBlock;
        if constexpr (!sizeof...(args))
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith0Arguments(entry, &vm, codeBlock, function, thisValue, context)));
        else if constexpr (sizeof...(args) == 1)
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith1Arguments(entry, &vm, codeBlock, function, thisValue, context, args...)));
        else if constexpr (sizeof...(args) == 2)
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith2Arguments(entry, &vm, codeBlock, function, thisValue, context, args...)));
        else if constexpr (sizeof...(args) == 3)
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith3Arguments(entry, &vm, codeBlock, function, thisValue, context, args...)));
        else if constexpr (sizeof...(args) == 4)
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith4Arguments(entry, &vm, codeBlock, function, thisValue, context, args...)));
        else if constexpr (sizeof...(args) == 5)
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith5Arguments(entry, &vm, codeBlock, function, thisValue, context, args...)));
        else if constexpr (sizeof...(args) == 6)
            RELEASE_AND_RETURN(scope, JSValue::decode(vmEntryToJavaScriptWith6Arguments(entry, &vm, codeBlock, function, thisValue, context, args...)));
        else
            return { };
    }
#else
    UNUSED_PARAM(function);
    UNUSED_PARAM(thisValue);
    UNUSED_PARAM(context);
    UNUSED_VARIABLE(scope);
    UNUSED_VARIABLE(argumentCountIncludingThis);
#endif

    return { };
}

} // namespace JSC
