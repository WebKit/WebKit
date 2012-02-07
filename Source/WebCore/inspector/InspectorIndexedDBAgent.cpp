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
#include "IDBDatabaseBackendInterface.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBIndexBackendInterface.h"
#include "IDBObjectStoreBackendInterface.h"
#include "IDBTransaction.h"
#include "IDBTransactionBackendInterface.h"
#include "InspectorFrontend.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"

#include <wtf/Vector.h>

using WebCore::TypeBuilder::IndexedDB::SecurityOriginWithDatabaseNames;
using WebCore::TypeBuilder::IndexedDB::DatabaseWithObjectStores;
using WebCore::TypeBuilder::IndexedDB::ObjectStore;
using WebCore::TypeBuilder::IndexedDB::ObjectStoreIndex;

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
    static PassRefPtr<GetDatabaseNamesCallback> create(InspectorIndexedDBAgent::FrontendProvider* frontendProvider, int requestId, const String& securityOrigin)
    {
        return adoptRef(new GetDatabaseNamesCallback(frontendProvider, requestId, securityOrigin));
    }

    virtual ~GetDatabaseNamesCallback() { }

    virtual void onSuccess(PassRefPtr<DOMStringList> databaseNamesList)
    {
        if (!m_frontendProvider->frontend())
            return;

        RefPtr<InspectorArray> databaseNames = InspectorArray::create();
        for (size_t i = 0; i < databaseNamesList->length(); ++i)
            databaseNames->pushString(databaseNamesList->item(i));

        RefPtr<SecurityOriginWithDatabaseNames> result = SecurityOriginWithDatabaseNames::create()
            .setSecurityOrigin(m_securityOrigin)
            .setDatabaseNames(databaseNames);

        m_frontendProvider->frontend()->databaseNamesLoaded(m_requestId, result);
    }

private:
    GetDatabaseNamesCallback(InspectorIndexedDBAgent::FrontendProvider* frontendProvider, int requestId, const String& securityOrigin)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_securityOrigin(securityOrigin) { }
    RefPtr<InspectorIndexedDBAgent::FrontendProvider> m_frontendProvider;
    int m_requestId;
    String m_securityOrigin;
};

class ExecutableWithDatabase : public RefCounted<ExecutableWithDatabase> {
public:
    virtual ~ExecutableWithDatabase() { };
    void start(IDBFactoryBackendInterface*, SecurityOrigin*, Frame*, const String& databaseName);
    virtual void execute(PassRefPtr<IDBDatabaseBackendInterface>) = 0;
};

class OpenDatabaseCallback : public InspectorIDBCallback {
public:
    static PassRefPtr<OpenDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRef(new OpenDatabaseCallback(executableWithDatabase));
    }

    virtual ~OpenDatabaseCallback() { }

    virtual void onSuccess(PassRefPtr<IDBDatabaseBackendInterface> idbDatabase)
    {
        m_executableWithDatabase->execute(idbDatabase);
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactoryBackendInterface* idbFactory, SecurityOrigin* securityOrigin, Frame* frame, const String& databaseName)
{
    RefPtr<OpenDatabaseCallback> callback = OpenDatabaseCallback::create(this);
    idbFactory->open(databaseName, callback.get(), securityOrigin, frame, String());
}

static PassRefPtr<IDBTransactionBackendInterface> transactionForDatabase(IDBDatabaseBackendInterface* idbDatabase, const String& objectStoreName)
{
    ExceptionCode ec = 0;
    RefPtr<DOMStringList> transactionObjectStoreNamesList = DOMStringList::create();
    transactionObjectStoreNamesList->append(objectStoreName);
    RefPtr<IDBTransactionBackendInterface> idbTransaction = idbDatabase->transaction(transactionObjectStoreNamesList.get(), IDBTransaction::READ_ONLY, ec);
    if (ec)
        return 0;
    return idbTransaction;
}

static PassRefPtr<IDBObjectStoreBackendInterface> objectStoreForTransaction(IDBTransactionBackendInterface* idbTransaction, const String& objectStoreName)
{
    ExceptionCode ec = 0;
    RefPtr<IDBObjectStoreBackendInterface> idbObjectStore = idbTransaction->objectStore(objectStoreName, ec);
    if (ec)
        return 0;
    return idbObjectStore;
}

static PassRefPtr<IDBIndexBackendInterface> indexForObjectStore(IDBObjectStoreBackendInterface* idbObjectStore, const String& indexName)
{
    ExceptionCode ec = 0;
    RefPtr<IDBIndexBackendInterface> idbIndex = idbObjectStore->index(indexName, ec);
    if (ec)
        return 0;
    return idbIndex;
}

class DatabaseLoaderCallback : public ExecutableWithDatabase {
public:
    static PassRefPtr<DatabaseLoaderCallback> create(InspectorIndexedDBAgent::FrontendProvider* frontendProvider, int requestId)
    {
        return adoptRef(new DatabaseLoaderCallback(frontendProvider, requestId));
    }

    virtual ~DatabaseLoaderCallback() { }

    virtual void execute(PassRefPtr<IDBDatabaseBackendInterface> idbDatabase)
    {
        if (!m_frontendProvider->frontend())
            return;

        RefPtr<InspectorArray> objectStores = InspectorArray::create();

        RefPtr<DOMStringList> objectStoreNamesList = idbDatabase->objectStoreNames();
        for (size_t i = 0; i < objectStoreNamesList->length(); ++i) {
            String objectStoreName = objectStoreNamesList->item(i);
            RefPtr<IDBTransactionBackendInterface> idbTransaction = transactionForDatabase(idbDatabase.get(), objectStoreName);
            if (!idbTransaction)
                continue;
            RefPtr<IDBObjectStoreBackendInterface> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), objectStoreName);
            if (!idbObjectStore)
                continue;

            RefPtr<InspectorArray> indexes = InspectorArray::create();
            RefPtr<DOMStringList> indexNamesList = idbObjectStore->indexNames();
            for (size_t j = 0; j < indexNamesList->length(); ++j) {
                RefPtr<IDBIndexBackendInterface> idbIndex = indexForObjectStore(idbObjectStore.get(), indexNamesList->item(j));
                if (!idbIndex)
                    continue;

                RefPtr<ObjectStoreIndex> objectStoreIndex = ObjectStoreIndex::create()
                    .setName(idbIndex->name())
                    .setKeyPath(idbIndex->keyPath())
                    .setUnique(idbIndex->unique())
                    .setMultiEntry(idbIndex->multiEntry());
                indexes->pushObject(objectStoreIndex);
            }

            RefPtr<ObjectStore> objectStore = ObjectStore::create()
                .setName(idbObjectStore->name())
                .setKeyPath(idbObjectStore->keyPath())
                .setIndexes(indexes);
            objectStores->pushObject(objectStore);
        }
        RefPtr<DatabaseWithObjectStores> result = DatabaseWithObjectStores::create()
            .setName(idbDatabase->name())
            .setVersion(idbDatabase->version())
            .setObjectStores(objectStores);

        m_frontendProvider->frontend()->databaseLoaded(m_requestId, result);
    }

private:
    DatabaseLoaderCallback(InspectorIndexedDBAgent::FrontendProvider* frontendProvider, int requestId)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId) { }
    RefPtr<InspectorIndexedDBAgent::FrontendProvider> m_frontendProvider;
    int m_requestId;
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

static Document* assertDocument(const String& frameId, InspectorPageAgent* pageAgent, ErrorString* error)
{
    Frame* frame = pageAgent->frameForId(frameId);
    Document* document = frame ? frame->document() : 0;

    if (!document)
        *error = "No document for given frame found";

    return document;
}

static IDBFactoryBackendInterface* assertIDBFactory(Document* document, ErrorString* error)
{
    Page* page = document ? document->page() : 0;
    IDBFactoryBackendInterface* idbFactory = page ? page->group().idbFactory() : 0;

    if (!idbFactory)
        *error = "No IndexedDB factory for given frame found";

    return idbFactory;
}

void InspectorIndexedDBAgent::requestDatabaseNamesForFrame(ErrorString* error, int requestId, const String& frameId)
{
    Document* document = assertDocument(frameId, m_pageAgent, error);
    if (!document)
        return;
    IDBFactoryBackendInterface* idbFactory = assertIDBFactory(document, error);
    if (!idbFactory)
        return;

    RefPtr<GetDatabaseNamesCallback> callback = GetDatabaseNamesCallback::create(m_frontendProvider.get(), requestId, document->securityOrigin()->toString());
    GroupSettings* groupSettings = document->page()->group().groupSettings();
    idbFactory->getDatabaseNames(callback.get(), document->securityOrigin(), document->frame(), groupSettings->indexedDBDatabasePath());
}

void InspectorIndexedDBAgent::requestDatabase(ErrorString* error, int requestId, const String& frameId, const String& databaseName)
{
    Document* document = assertDocument(frameId, m_pageAgent, error);
    if (!document)
        return;
    IDBFactoryBackendInterface* idbFactory = assertIDBFactory(document, error);
    if (!idbFactory)
        return;

    RefPtr<DatabaseLoaderCallback> databaseLoaderCallback = DatabaseLoaderCallback::create(m_frontendProvider.get(), requestId);
    databaseLoaderCallback->start(idbFactory, document->securityOrigin(), document->frame(), databaseName);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)
