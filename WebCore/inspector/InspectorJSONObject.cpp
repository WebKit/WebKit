/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorJSONObject.h"

#include "PlatformString.h"
#include "ScriptObject.h"
#include "ScriptState.h"

namespace WebCore {

InspectorJSONObject::InspectorJSONObject(ScriptState* scriptState)
    : m_scriptState(scriptState)
{
    m_scriptObject = ScriptObject::createNew(scriptState);
}

bool InspectorJSONObject::set(const String& name, const String& value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

bool InspectorJSONObject::set(const char* name, const ScriptObject& value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

bool InspectorJSONObject::set(const char* name, const InspectorJSONObject& value)
{
    return set(name, value.scriptObject());
}

bool InspectorJSONObject::set(const char* name, const String& value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

bool InspectorJSONObject::set(const char* name, double value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

bool InspectorJSONObject::set(const char* name, long long value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

bool InspectorJSONObject::set(const char* name, int value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

bool InspectorJSONObject::set(const char* name, bool value)
{
    return m_scriptObject.set(m_scriptState, name, value);
}

ScriptObject InspectorJSONObject::scriptObject() const
{
    return m_scriptObject;
}

InspectorJSONObject InspectorJSONObject::createNew(ScriptState* scriptState)
{
    return InspectorJSONObject(scriptState);
}

} // namespace WebCore
