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

#ifndef InspectorIndexedDBAgent_h
#define InspectorIndexedDBAgent_h

#if ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class InspectorPageAgent;

typedef String ErrorString;

class InspectorIndexedDBAgent : public InspectorAgentBase, public Inspector::InspectorIndexedDBBackendDispatcherHandler {
public:
    InspectorIndexedDBAgent(InstrumentingAgents*, Inspector::InjectedScriptManager*, InspectorPageAgent*);
    ~InspectorIndexedDBAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend() override;

    // Called from the front-end.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void requestDatabaseNames(ErrorString*, const String& securityOrigin, PassRefPtr<RequestDatabaseNamesCallback>);
    virtual void requestDatabase(ErrorString*, const String& securityOrigin, const String& databaseName, PassRefPtr<RequestDatabaseCallback>);
    virtual void requestData(ErrorString*, const String& securityOrigin, const String& databaseName, const String& objectStoreName, const String& indexName, int skipCount, int pageSize, const RefPtr<Inspector::InspectorObject>* keyRange, PassRefPtr<RequestDataCallback>);
    virtual void clearObjectStore(ErrorString*, const String& in_securityOrigin, const String& in_databaseName, const String& in_objectStoreName, PassRefPtr<ClearObjectStoreCallback>);

private:
    Inspector::InjectedScriptManager* m_injectedScriptManager;
    InspectorPageAgent* m_pageAgent;
    RefPtr<Inspector::InspectorIndexedDBBackendDispatcher> m_backendDispatcher;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)
#endif // !defined(InspectorIndexedDBAgent_h)
