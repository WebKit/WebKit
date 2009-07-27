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
#include "ScriptArray.h"

#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

ScriptArray::ScriptArray(JSArray* object)
    : ScriptObject(object)
{
}

static bool handleException(ScriptState* scriptState)
{
    if (!scriptState->hadException())
        return true;

    reportException(scriptState, scriptState->exception());
    return false;
}

bool ScriptArray::set(ScriptState* scriptState, unsigned index, const ScriptObject& value)
{
    JSLock lock(SilenceAssertionsOnly);
    jsArray()->put(scriptState, index, value.jsObject());
    return handleException(scriptState);
}

bool ScriptArray::set(ScriptState* scriptState, unsigned index, const String& value)
{
    JSLock lock(SilenceAssertionsOnly);
    jsArray()->put(scriptState, index, jsString(scriptState, value));
    return handleException(scriptState);
}

bool ScriptArray::set(ScriptState* scriptState, unsigned index, double value)
{
    JSLock lock(SilenceAssertionsOnly);
    jsArray()->put(scriptState, index, jsNumber(scriptState, value));
    return handleException(scriptState);
}

bool ScriptArray::set(ScriptState* scriptState, unsigned index, long long value)
{
    JSLock lock(SilenceAssertionsOnly);
    jsArray()->put(scriptState, index, jsNumber(scriptState, value));
    return handleException(scriptState);
}

bool ScriptArray::set(ScriptState* scriptState, unsigned index, int value)
{
    JSLock lock(SilenceAssertionsOnly);
    jsArray()->put(scriptState, index, jsNumber(scriptState, value));
    return handleException(scriptState);
}

bool ScriptArray::set(ScriptState* scriptState, unsigned index, bool value)
{
    JSLock lock(SilenceAssertionsOnly);
    jsArray()->put(scriptState, index, jsBoolean(value));
    return handleException(scriptState);
}

unsigned ScriptArray::length(ScriptState*)
{
    return jsArray()->length();
}

ScriptArray ScriptArray::createNew(ScriptState* scriptState)
{
    JSLock lock(SilenceAssertionsOnly);
    return ScriptArray(constructEmptyArray(scriptState));
}

} // namespace WebCore
