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

#include "config.h"
#include "InjectedScriptHost.h"

#if ENABLE(INSPECTOR)

#include "JSInjectedScriptHost.h"

using namespace JSC;

namespace Inspector {

InjectedScriptHost::~InjectedScriptHost()
{
}

JSValue InjectedScriptHost::jsWrapper(ExecState* exec, JSGlobalObject* globalObject)
{
    auto key = std::make_pair(exec, globalObject);
    auto it = m_wrappers.find(key);
    if (it != m_wrappers.end())
        return it->value.get();

    JSValue jsValue = toJS(exec, globalObject, this);
    if (!jsValue.isObject())
        return jsValue;

    JSObject* jsObject = jsValue.toObject(exec, globalObject);
    Strong<JSObject> wrapper(exec->vm(), jsObject);
    m_wrappers.add(key, wrapper);

    return jsValue;
}

static void clearWrapperFromValue(JSValue value)
{
    JSInjectedScriptHost* jsInjectedScriptHost = toJSInjectedScriptHost(value);
    ASSERT(jsInjectedScriptHost);
    if (jsInjectedScriptHost)
        jsInjectedScriptHost->releaseImpl();
}

void InjectedScriptHost::clearWrapper(ExecState* exec, JSGlobalObject* globalObject)
{
    auto key = std::make_pair(exec, globalObject);
    clearWrapperFromValue(m_wrappers.take(key).get());
}

void InjectedScriptHost::clearAllWrappers()
{
    for (auto& wrapper : m_wrappers)
        clearWrapperFromValue(wrapper.value.get());

    m_wrappers.clear();
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
