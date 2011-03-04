/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "InspectorRuntimeAgent.h"

#if ENABLE(INSPECTOR)

#include "InjectedScriptHost.h"
#include "InspectorValues.h"

namespace WebCore {

InspectorRuntimeAgent::InspectorRuntimeAgent(InjectedScriptHost* injectedScriptHost)
    : m_injectedScriptHost(injectedScriptHost)
{
}

InspectorRuntimeAgent::~InspectorRuntimeAgent() { }

void InspectorRuntimeAgent::evaluate(ErrorString*, const String& expression, const String& objectGroup, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForMainFrame();
    if (!injectedScript.hasNoValue())
        injectedScript.evaluate(expression, objectGroup, includeCommandLineAPI, result);
}

void InspectorRuntimeAgent::evaluateOn(ErrorString*, PassRefPtr<InspectorObject> objectId, const String& expression, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForObjectId(objectId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.evaluateOn(objectId, expression, result);
}

void InspectorRuntimeAgent::getProperties(ErrorString*, PassRefPtr<InspectorObject> objectId, bool ignoreHasOwnProperty, bool abbreviate, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForObjectId(objectId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.getProperties(objectId, ignoreHasOwnProperty, abbreviate, result);
}

void InspectorRuntimeAgent::setPropertyValue(ErrorString*, PassRefPtr<InspectorObject> objectId, const String& propertyName, const String& expression, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForObjectId(objectId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.setPropertyValue(objectId, propertyName, expression, result);
}

void InspectorRuntimeAgent::releaseObject(ErrorString*, PassRefPtr<InspectorObject> objectId)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForObjectId(objectId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.releaseObject(objectId);
}

void InspectorRuntimeAgent::releaseObjectGroup(ErrorString*, long injectedScriptId, const String& objectGroup)
{
    m_injectedScriptHost->releaseObjectGroup(injectedScriptId, objectGroup);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
