/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "Test.h"

#include "Completion.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectInlines.h"
#include "VM.h"
#include "VMInspector.h"

#include <wtf/text/WTFString.h>

namespace JSC {

JSC_DECLARE_HOST_FUNCTION(functionProbe);

ExceptionExpectedScope::ExceptionExpectedScope(JSGlobalObject* globalObject, ThrowScope& throwScope, bool& isExceptionExpected)
    : m_globalObject(globalObject)
    , m_throwScope(throwScope)
    , m_isExceptionExpected(isExceptionExpected)
{
    ASSERT(!m_isExceptionExpected); // do not nest the scopes because that would disturb their behavior
    m_isExceptionExpected = true;
}

ExceptionExpectedScope::~ExceptionExpectedScope()
{
    ASSERT(m_isExceptionExpected);
    m_isExceptionExpected = false;
    if (m_throwScope.exception())
        m_throwScope.clearException();
    else {
        JSObject* error = createError(m_globalObject, "expected exception not thrown"_s);
        throwException(m_globalObject, m_throwScope, error);
    }
}

TestProbe currentTestProbe = nullptr;

JSC_DEFINE_HOST_FUNCTION(functionProbe, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    Vector<JSValue> arguments;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        arguments.append(callFrame->argument(i));

    RELEASE_ASSERT(currentTestProbe);
    JSValue value = currentTestProbe(globalObject, arguments);
    return JSValue::encode(value);
}

} // namespace JSC
