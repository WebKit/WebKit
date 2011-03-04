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

#ifndef InspectorRuntimeAgent_h
#define InspectorRuntimeAgent_h

#if ENABLE(INSPECTOR)

#include "InjectedScript.h"
#include "PlatformString.h"

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class InjectedScriptHost;
class InspectorObject;
class InspectorValue;

typedef String ErrorString;

class InspectorRuntimeAgent {
    WTF_MAKE_NONCOPYABLE(InspectorRuntimeAgent);
public:
    static PassOwnPtr<InspectorRuntimeAgent> create(InjectedScriptHost* injectedScriptHost)
    {
        return adoptPtr(new InspectorRuntimeAgent(injectedScriptHost));
    }

    ~InspectorRuntimeAgent();

    // Part of the protocol.
    void evaluate(ErrorString*, const String& expression, const String& objectGroup, bool includeCommandLineAPI, RefPtr<InspectorValue>* result);
    void evaluateOn(ErrorString*, PassRefPtr<InspectorObject> objectId, const String& expression, RefPtr<InspectorValue>* result);
    void releaseObject(ErrorString*, PassRefPtr<InspectorObject> objectId);
    void getProperties(ErrorString*, PassRefPtr<InspectorObject> objectId, bool ignoreHasOwnProperty, bool abbreviate, RefPtr<InspectorValue>* result);
    void setPropertyValue(ErrorString*, PassRefPtr<InspectorObject> objectId, const String& propertyName, const String& expression, RefPtr<InspectorValue>* result);
    void releaseWrapperObjectGroup(ErrorString*, long injectedScriptId, const String& objectGroup);

private:
    InspectorRuntimeAgent(InjectedScriptHost*);

    InjectedScriptHost* m_injectedScriptHost;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
#endif // InspectorRuntimeAgent_h
