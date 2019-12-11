/*
 * Copyright (C) 2012, 2016 Apple Inc. All rights reserved.
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
#include "RegExpCachedResult.h"

#include "JSCInlines.h"
#include "RegExpCache.h"
#include "RegExpMatchesArray.h"

namespace JSC {

void RegExpCachedResult::visitAggregate(SlotVisitor& visitor)
{
    visitor.append(m_lastInput);
    visitor.append(m_lastRegExp);
    if (m_reified) {
        visitor.append(m_reifiedInput);
        visitor.append(m_reifiedResult);
        visitor.append(m_reifiedLeftContext);
        visitor.append(m_reifiedRightContext);
    }
}

JSArray* RegExpCachedResult::lastResult(JSGlobalObject* globalObject, JSObject* owner)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!m_reified) {
        m_reifiedInput.set(vm, owner, m_lastInput.get());
        if (!m_lastRegExp)
            m_lastRegExp.set(vm, owner, vm.regExpCache()->ensureEmptyRegExp(vm));

        JSArray* result = nullptr;
        if (m_result)
            result = createRegExpMatchesArray(globalObject, m_lastInput.get(), m_lastRegExp.get(), m_result.start);
        else
            result = createEmptyRegExpMatchesArray(globalObject, m_lastInput.get(), m_lastRegExp.get());
        RETURN_IF_EXCEPTION(scope, nullptr);

        m_reifiedResult.setWithoutWriteBarrier(result);
        m_reifiedLeftContext.clear();
        m_reifiedRightContext.clear();
        m_reified = true;
        vm.heap.writeBarrier(owner);
    }
    return m_reifiedResult.get();
}

JSString* RegExpCachedResult::leftContext(JSGlobalObject* globalObject, JSObject* owner)
{
    // Make sure we're reified.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    lastResult(globalObject, owner);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (!m_reifiedLeftContext) {
        JSString* leftContext = jsSubstring(globalObject, m_reifiedInput.get(), 0, m_result.start);
        RETURN_IF_EXCEPTION(scope, nullptr);
        m_reifiedLeftContext.set(vm, owner, leftContext);
    }
    return m_reifiedLeftContext.get();
}

JSString* RegExpCachedResult::rightContext(JSGlobalObject* globalObject, JSObject* owner)
{
    // Make sure we're reified.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    lastResult(globalObject, owner);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (!m_reifiedRightContext) {
        unsigned length = m_reifiedInput->length();
        JSString* rightContext = jsSubstring(globalObject, m_reifiedInput.get(), m_result.end, length - m_result.end);
        RETURN_IF_EXCEPTION(scope, nullptr);
        m_reifiedRightContext.set(vm, owner, rightContext);
    }
    return m_reifiedRightContext.get();
}

void RegExpCachedResult::setInput(JSGlobalObject* globalObject, JSObject* owner, JSString* input)
{
    // Make sure we're reified, otherwise m_reifiedInput will be ignored.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    lastResult(globalObject, owner);
    RETURN_IF_EXCEPTION(scope, void());
    leftContext(globalObject, owner);
    RETURN_IF_EXCEPTION(scope, void());
    rightContext(globalObject, owner);
    RETURN_IF_EXCEPTION(scope, void());
    ASSERT(m_reified);
    m_reifiedInput.set(vm, owner, input);
}

} // namespace JSC
