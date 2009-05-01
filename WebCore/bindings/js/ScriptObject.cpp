/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptObject.h"

#include "JSDOMBinding.h"
#include "JSInspectorController.h"

#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

ScriptObject::ScriptObject(JSObject* object)
    : ScriptValue(object)
{
}

static bool handleException(ScriptState* scriptState)
{
    if (!scriptState->hadException())
        return true;

    reportException(scriptState, scriptState->exception());
    return false;
}

bool ScriptObject::set(ScriptState* scriptState, const String& name, const String& value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), jsString(scriptState, value), slot);
    return handleException(scriptState);
}

bool ScriptObject::set(ScriptState* scriptState, const char* name, const ScriptObject& value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), value.jsObject(), slot);
    return handleException(scriptState);
}

bool ScriptObject::set(ScriptState* scriptState, const char* name, const String& value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), jsString(scriptState, value), slot);
    return handleException(scriptState);
}

bool ScriptObject::set(ScriptState* scriptState, const char* name, double value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), jsNumber(scriptState, value), slot);
    return handleException(scriptState);
}

bool ScriptObject::set(ScriptState* scriptState, const char* name, long long value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), jsNumber(scriptState, value), slot);
    return handleException(scriptState);
}

bool ScriptObject::set(ScriptState* scriptState, const char* name, int value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), jsNumber(scriptState, value), slot);
    return handleException(scriptState);
}

bool ScriptObject::set(ScriptState* scriptState, const char* name, bool value)
{
    JSLock lock(false);
    PutPropertySlot slot;
    jsObject()->put(scriptState, Identifier(scriptState, name), jsBoolean(value), slot);
    return handleException(scriptState);
}

ScriptObject ScriptObject::createNew(ScriptState* scriptState)
{
    JSLock lock(false);
    return ScriptObject(constructEmptyObject(scriptState));
}

bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, const ScriptObject& value)
{
    JSLock lock(false);
    scriptState->lexicalGlobalObject()->putDirect(Identifier(scriptState, name), value.jsObject());
    return handleException(scriptState);
}

bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, InspectorController* value)
{
    JSLock lock(false);
    scriptState->lexicalGlobalObject()->putDirect(Identifier(scriptState, name), toJS(scriptState, value));
    return handleException(scriptState);
}

bool ScriptGlobalObject::get(ScriptState* scriptState, const char* name, ScriptObject& value)
{
    JSLock lock(false);
    JSValue jsValue = scriptState->lexicalGlobalObject()->get(scriptState, Identifier(scriptState, name));
    if (!jsValue)
        return false;

    if (!jsValue.isObject())
        return false;

    value = ScriptObject(asObject(jsValue));
    return true;
}

bool ScriptGlobalObject::remove(ScriptState* scriptState, const char* name)
{
    JSLock lock(false);
    scriptState->lexicalGlobalObject()->deleteProperty(scriptState, Identifier(scriptState, name));
    return handleException(scriptState);
}

} // namespace WebCore
