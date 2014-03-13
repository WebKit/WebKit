/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorAgent_h
#define InspectorAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorJSBackendDispatchers.h"
#include "InspectorJSFrontendDispatchers.h"
#include "inspector/InspectorAgentBase.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace Inspector {

class InspectorObject;
class InstrumentingAgents;
class InspectorInspectorBackendDispatcher;
class InspectorInspectorFrontendDispatchers;

typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorAgent final : public InspectorAgentBase, public InspectorInspectorBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAgent);
public:
    InspectorAgent();
    virtual ~InspectorAgent();

    virtual void didCreateFrontendAndBackend(InspectorFrontendChannel*, InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(InspectorDisconnectReason reason) override;

    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;

    void inspect(PassRefPtr<TypeBuilder::Runtime::RemoteObject> objectToInspect, PassRefPtr<InspectorObject> hints);
    void evaluateForTestInFrontend(const String& script);

private:
    std::unique_ptr<InspectorInspectorFrontendDispatcher> m_frontendDispatcher;
    RefPtr<InspectorInspectorBackendDispatcher> m_backendDispatcher;
    Vector<String> m_pendingEvaluateTestCommands;
    std::pair<RefPtr<TypeBuilder::Runtime::RemoteObject>, RefPtr<InspectorObject>> m_pendingInspectData;
    bool m_enabled;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorAgent_h)
