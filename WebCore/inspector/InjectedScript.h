/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InjectedScript_h
#define InjectedScript_h

#include "InjectedScriptHost.h"
#include "ScriptObject.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class InspectorValue;

class InjectedScript {
public:
    InjectedScript() { }
    ~InjectedScript() { }

    bool hasNoValue() const { return m_injectedScriptObject.hasNoValue(); }

    void dispatch(const String& methodName, const String& arguments, RefPtr<InspectorValue>* result, bool* hadException);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    PassRefPtr<InspectorValue> callFrames();
#endif
    PassRefPtr<InspectorValue> wrapForConsole(ScriptValue);
    void releaseWrapperObjectGroup(const String&);
    ScriptState* scriptState() const { return m_injectedScriptObject.scriptState(); }

private:
    friend InjectedScript InjectedScriptHost::injectedScriptFor(ScriptState*);
    explicit InjectedScript(ScriptObject);
    bool canAccessInspectedWindow();
    ScriptObject m_injectedScriptObject;
};

} // namespace WebCore

#endif
