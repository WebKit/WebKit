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

#include "JSInjectedScriptHost.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"

namespace Inspector {

using namespace JSC;

InjectedScriptHost::~InjectedScriptHost()
{
}

JSValue InjectedScriptHost::wrapper(JSGlobalObject* globalObject)
{
    JSValue value = m_wrappers.getWrapper(globalObject);
    if (value)
        return value;

    VM& vm = globalObject->vm();
    JSObject* prototype = JSInjectedScriptHost::createPrototype(vm, globalObject);
    Structure* structure = JSInjectedScriptHost::createStructure(vm, globalObject, prototype);
    JSInjectedScriptHost* injectedScriptHost = JSInjectedScriptHost::create(vm, structure, makeRef(*this));
    m_wrappers.addWrapper(globalObject, injectedScriptHost);

    return injectedScriptHost;
}

void InjectedScriptHost::clearAllWrappers()
{
    m_wrappers.clearAllWrappers();
}

} // namespace Inspector
