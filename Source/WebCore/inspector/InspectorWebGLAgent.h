/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InspectorWebGLAgent_h
#define InspectorWebGLAgent_h

#if ENABLE(INSPECTOR) && ENABLE(WEBGL)

#include "InspectorBaseAgent.h"
#include "InspectorFrontend.h"
#include "PlatformString.h"
#include "ScriptState.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class InjectedScriptManager;
class InspectorState;
class InstrumentingAgents;
class ScriptObject;

typedef String ErrorString;

class InspectorWebGLAgent : public InspectorBaseAgent<InspectorWebGLAgent>, public InspectorBackendDispatcher::WebGLCommandHandler {
public:
    static PassOwnPtr<InspectorWebGLAgent> create(InstrumentingAgents* instrumentingAgents, InspectorState* state, InjectedScriptManager* injectedScriptManager)
    {
        return adoptPtr(new InspectorWebGLAgent(instrumentingAgents, state, injectedScriptManager));
    }
    ~InspectorWebGLAgent();

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    bool enabled() { return m_enabled; }

    ScriptObject wrapWebGLRenderingContextForInstrumentation(const ScriptObject&);

    // Called from the front-end.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);

    // Called from the injected script.

    // Called from InspectorInstrumentation

private:
    InspectorWebGLAgent(InstrumentingAgents*, InspectorState*, InjectedScriptManager*);

    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::WebGL* m_frontend;
    bool m_enabled;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(WEBGL)

#endif // !defined(InspectorWebGLAgent_h)
