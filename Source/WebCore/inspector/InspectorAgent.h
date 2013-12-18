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

#include "InspectorWebAgentBase.h"
#include <inspector/InspectorJSBackendDispatchers.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace Inspector {
class InspectorObject;
class InspectorInspectorFrontendDispatcher;
}

namespace WebCore {

class DOMWrapperWorld;
class DocumentLoader;
class Frame;
class InstrumentingAgents;
class Page;

typedef String ErrorString;

class InspectorAgent : public InspectorAgentBase, public Inspector::InspectorInspectorBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAgent);
public:
    static PassOwnPtr<InspectorAgent> create(Page* page, InstrumentingAgents* instrumentingAgents)
    {
        return adoptPtr(new InspectorAgent(page, instrumentingAgents));
    }

    virtual ~InspectorAgent();

    bool developerExtrasEnabled() const;

    // Inspector front-end API.
    void enable(ErrorString*);
    void disable(ErrorString*);

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) OVERRIDE;
    virtual void willDestroyFrontendAndBackend() OVERRIDE;

    // Generic code called from custom implementations.
    void evaluateForTestInFrontend(long testCallId, const String& script);

    void inspect(PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> objectToInspect, PassRefPtr<Inspector::InspectorObject> hints);

private:
    InspectorAgent(Page*, InstrumentingAgents*);

    Page* m_inspectedPage;
    std::unique_ptr<Inspector::InspectorInspectorFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorInspectorBackendDispatcher> m_backendDispatcher;

    Vector<pair<long, String>> m_pendingEvaluateTestCommands;
    pair<RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>, RefPtr<Inspector::InspectorObject>> m_pendingInspectData;

    bool m_enabled;
};

} // namespace WebCore

#endif // !defined(InspectorAgent_h)
