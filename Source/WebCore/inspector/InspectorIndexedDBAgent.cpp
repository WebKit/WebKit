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

#include "config.h"

#include "InspectorIndexedDBAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GroupSettings.h"
#include "IDBCallbacks.h"
#include "IDBFactoryBackendInterface.h"
#include "InspectorFrontend.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"

#include <wtf/Vector.h>

using WebCore::TypeBuilder::IndexedDB::FrameWithDatabaseNames;

namespace WebCore {

namespace IndexedDBAgentState {
static const char indexedDBAgentEnabled[] = "indexedDBAgentEnabled";
};

class InspectorIndexedDBAgent::FrontendProvider : public RefCounted<InspectorIndexedDBAgent::FrontendProvider> {
public:
    static PassRefPtr<FrontendProvider> create(InspectorFrontend* inspectorFrontend)
    {
        return adoptRef(new FrontendProvider(inspectorFrontend));
    }

    virtual ~FrontendProvider() { }

    InspectorFrontend::IndexedDB* frontend() { return m_inspectorFrontend; }
    void clearFrontend() { m_inspectorFrontend = 0; }
private:
    FrontendProvider(InspectorFrontend* inspectorFrontend) : m_inspectorFrontend(inspectorFrontend->indexeddb()) { }
    InspectorFrontend::IndexedDB* m_inspectorFrontend;
};

namespace {

class InspectorIDBCallback : public IDBCallbacks {
public:
    virtual ~InspectorIDBCallback() { }

    virtual void onError(PassRefPtr<IDBDatabaseError>) { }
    virtual void onSuccess(PassRefPtr<DOMStringList>) { }
    virtual void onSuccess(PassRefPtr<IDBCursorBackendInterface>) { }
    virtual void onSuccess(PassRefPtr<IDBDatabaseBackendInterface>) { }
    virtual void onSuccess(PassRefPtr<IDBKey>) { }
    virtual void onSuccess(PassRefPtr<IDBTransactionBackendInterface>) { }
    virtual void onSuccess(PassRefPtr<SerializedScriptValue>) { }
    virtual void onSuccessWithContinuation() { }
    virtual void onSuccessWithPrefetch(const Vector<RefPtr<IDBKey> >& keys, const Vector<RefPtr<IDBKey> >& primaryKeys, const Vector<RefPtr<SerializedScriptValue> >& values) { }
    virtual void onBlocked() { }
};

class GetDatabaseNamesCallback : public InspectorIDBCallback {
public:
    static PassRefPtr<GetDatabaseNamesCallback> create(PassRefPtr<InspectorIndexedDBAgent::FrontendProvider> frontendProvider, const String& frameId, const String& securityOrigin)
    {
        return adoptRef(new GetDatabaseNamesCallback(frontendProvider, frameId, securityOrigin));
    }

    virtual ~GetDatabaseNamesCallback() { }

    virtual void onSuccess(PassRefPtr<DOMStringList> databaseNamesList)
    {
        if (!m_frontendProvider->frontend())
            return;

        RefPtr<InspectorArray> databaseNames = InspectorArray::create();
        for (size_t i = 0; i < databaseNamesList->length(); ++i)
            databaseNames->pushString(databaseNamesList->item(i));

        RefPtr<FrameWithDatabaseNames> result = FrameWithDatabaseNames::create()
            .setFrameId(m_frameId)
            .setSecurityOrigin(m_securityOrigin)
            .setDatabaseNames(databaseNames);

        m_frontendProvider->frontend()->databaseNamesLoaded(result);
    }

private:
    GetDatabaseNamesCallback(PassRefPtr<InspectorIndexedDBAgent::FrontendProvider> frontendProvider, const String& frameId, const String& securityOrigin)
        : m_frontendProvider(frontendProvider)
        , m_frameId(frameId)
        , m_securityOrigin(securityOrigin) { }
    RefPtr<InspectorIndexedDBAgent::FrontendProvider> m_frontendProvider;
    String m_frameId;
    String m_securityOrigin;
};

} // namespace

InspectorIndexedDBAgent::InspectorIndexedDBAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, InspectorPageAgent* pageAgent)
    : InspectorBaseAgent<InspectorIndexedDBAgent>("IndexedDB", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
{
}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent()
{
}

void InspectorIndexedDBAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontendProvider = FrontendProvider::create(frontend);
}

void InspectorIndexedDBAgent::clearFrontend()
{
    m_frontendProvider->clearFrontend();
    m_frontendProvider.clear();
    disable(0);
}

void InspectorIndexedDBAgent::restore()
{
    if (m_state->getBoolean(IndexedDBAgentState::indexedDBAgentEnabled)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorIndexedDBAgent::enable(ErrorString*)
{
    m_state->setBoolean(IndexedDBAgentState::indexedDBAgentEnabled, true);
}

void InspectorIndexedDBAgent::disable(ErrorString*)
{
    m_state->setBoolean(IndexedDBAgentState::indexedDBAgentEnabled, false);
}

void InspectorIndexedDBAgent::requestDatabaseNamesForFrame(ErrorString* error, const String& frameId)
{
    Frame* frame = m_pageAgent->frameForId(frameId);
    Document* document = frame ? frame->document() : 0;
    Page* page = document ? document->page() : 0;
    IDBFactoryBackendInterface* idbBackend = page ? page->group().idbFactory() : 0;

    if (!idbBackend) {
        *error = "No IndexedDB factory for given frame found";
        return;
    }

    RefPtr<GetDatabaseNamesCallback> callback = GetDatabaseNamesCallback::create(m_frontendProvider, frameId, document->securityOrigin()->toString());
    GroupSettings* groupSettings = document->page()->group().groupSettings();
    idbBackend->getDatabaseNames(callback.get(), document->securityOrigin(), document->frame(), groupSettings->indexedDBDatabasePath());
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)
