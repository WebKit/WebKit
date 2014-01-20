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

#if ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)

#include "InspectorIndexedDBAgent.h"

#include "DOMStringList.h"
#include "DOMWindow.h"
#include "DOMWindowIndexedDatabase.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "IDBCursor.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseMetadata.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBOpenDBRequest.h"
#include "IDBPendingTransactionMonitor.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "InspectorPageAgent.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "SecurityOrigin.h"
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/InspectorValues.h>
#include <wtf/Vector.h>

using Inspector::TypeBuilder::Array;
using Inspector::TypeBuilder::IndexedDB::DatabaseWithObjectStores;
using Inspector::TypeBuilder::IndexedDB::DataEntry;
using Inspector::TypeBuilder::IndexedDB::Key;
using Inspector::TypeBuilder::IndexedDB::KeyPath;
using Inspector::TypeBuilder::IndexedDB::KeyRange;
using Inspector::TypeBuilder::IndexedDB::ObjectStore;
using Inspector::TypeBuilder::IndexedDB::ObjectStoreIndex;

typedef Inspector::InspectorBackendDispatcher::CallbackBase RequestCallback;
typedef Inspector::InspectorIndexedDBBackendDispatcherHandler::RequestDatabaseNamesCallback RequestDatabaseNamesCallback;
typedef Inspector::InspectorIndexedDBBackendDispatcherHandler::RequestDatabaseCallback RequestDatabaseCallback;
typedef Inspector::InspectorIndexedDBBackendDispatcherHandler::RequestDataCallback RequestDataCallback;
typedef Inspector::InspectorIndexedDBBackendDispatcherHandler::ClearObjectStoreCallback ClearObjectStoreCallback;

using namespace Inspector;

namespace WebCore {

namespace {

class GetDatabaseNamesCallback : public EventListener {
    WTF_MAKE_NONCOPYABLE(GetDatabaseNamesCallback);
public:
    static PassRefPtr<GetDatabaseNamesCallback> create(PassRefPtr<RequestDatabaseNamesCallback> requestCallback, const String& securityOrigin)
    {
        return adoptRef(new GetDatabaseNamesCallback(requestCallback, securityOrigin));
    }

    virtual ~GetDatabaseNamesCallback() { }

    virtual bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    virtual void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != eventNames().successEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        ExceptionCode ec = 0;
        RefPtr<IDBAny> requestResult = idbRequest->result(ec);
        if (ec) {
            m_requestCallback->sendFailure("Could not get result in callback.");
            return;
        }
        if (requestResult->type() != IDBAny::DOMStringListType) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<DOMStringList> databaseNamesList = requestResult->domStringList();
        RefPtr<Inspector::TypeBuilder::Array<String>> databaseNames = Inspector::TypeBuilder::Array<String>::create();
        for (size_t i = 0; i < databaseNamesList->length(); ++i)
            databaseNames->addItem(databaseNamesList->item(i));
        m_requestCallback->sendSuccess(databaseNames.release());
    }

private:
    GetDatabaseNamesCallback(PassRefPtr<RequestDatabaseNamesCallback> requestCallback, const String& securityOrigin)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(requestCallback)
        , m_securityOrigin(securityOrigin) { }
    RefPtr<RequestDatabaseNamesCallback> m_requestCallback;
    String m_securityOrigin;
};

class ExecutableWithDatabase : public RefCounted<ExecutableWithDatabase> {
public:
    ExecutableWithDatabase(ScriptExecutionContext* context)
        : m_context(context) { }
    virtual ~ExecutableWithDatabase() { };
    void start(IDBFactory*, SecurityOrigin*, const String& databaseName);
    virtual void execute(PassRefPtr<IDBDatabase>) = 0;
    virtual RequestCallback* requestCallback() = 0;
    ScriptExecutionContext* context() { return m_context; };
private:
    ScriptExecutionContext* m_context;
};

class OpenDatabaseCallback : public EventListener {
public:
    static PassRefPtr<OpenDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRef(new OpenDatabaseCallback(executableWithDatabase));
    }

    virtual ~OpenDatabaseCallback() { }

    virtual bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    virtual void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (event->type() != eventNames().successEvent) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected event type.");
            return;
        }

        IDBOpenDBRequest* idbOpenDBRequest = static_cast<IDBOpenDBRequest*>(event->target());
        ExceptionCode ec = 0;
        RefPtr<IDBAny> requestResult = idbOpenDBRequest->result(ec);
        if (ec) {
            m_executableWithDatabase->requestCallback()->sendFailure("Could not get result in callback.");
            return;
        }
        if (requestResult->type() != IDBAny::IDBDatabaseType) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<IDBDatabase> idbDatabase = requestResult->idbDatabase();
        m_executableWithDatabase->execute(idbDatabase);
        IDBPendingTransactionMonitor::deactivateNewTransactions();
        idbDatabase->close();
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactory* idbFactory, SecurityOrigin*, const String& databaseName)
{
    RefPtr<OpenDatabaseCallback> callback = OpenDatabaseCallback::create(this);
    ExceptionCode ec = 0;
    RefPtr<IDBOpenDBRequest> idbOpenDBRequest = idbFactory->open(context(), databaseName, ec);
    if (ec) {
        requestCallback()->sendFailure("Could not open database.");
        return;
    }
    idbOpenDBRequest->addEventListener(eventNames().successEvent, callback, false);
}

static PassRefPtr<IDBTransaction> transactionForDatabase(ScriptExecutionContext* scriptExecutionContext, IDBDatabase* idbDatabase, const String& objectStoreName, const String& mode = IDBTransaction::modeReadOnly())
{
    ExceptionCode ec = 0;
    RefPtr<IDBTransaction> idbTransaction = idbDatabase->transaction(scriptExecutionContext, objectStoreName, mode, ec);
    if (ec)
        return nullptr;
    return idbTransaction;
}

static PassRefPtr<IDBObjectStore> objectStoreForTransaction(IDBTransaction* idbTransaction, const String& objectStoreName)
{
    ExceptionCode ec = 0;
    RefPtr<IDBObjectStore> idbObjectStore = idbTransaction->objectStore(objectStoreName, ec);
    if (ec)
        return nullptr;
    return idbObjectStore;
}

static PassRefPtr<IDBIndex> indexForObjectStore(IDBObjectStore* idbObjectStore, const String& indexName)
{
    ExceptionCode ec = 0;
    RefPtr<IDBIndex> idbIndex = idbObjectStore->index(indexName, ec);
    if (ec)
        return nullptr;
    return idbIndex;
}

#if !PLATFORM(MAC)
static PassRefPtr<KeyPath> keyPathFromIDBKeyPath(const IDBKeyPath& idbKeyPath)
{
    RefPtr<KeyPath> keyPath;
    switch (idbKeyPath.type()) {
    case IDBKeyPath::NullType:
        keyPath = KeyPath::create().setType(KeyPath::Type::Null);
        break;
    case IDBKeyPath::StringType:
        keyPath = KeyPath::create().setType(KeyPath::Type::String);
        keyPath->setString(idbKeyPath.string());
        break;
    case IDBKeyPath::ArrayType: {
        keyPath = KeyPath::create().setType(KeyPath::Type::Array);
        RefPtr<Inspector::TypeBuilder::Array<String>> array = Inspector::TypeBuilder::Array<String>::create();
        const Vector<String>& stringArray = idbKeyPath.array();
        for (size_t i = 0; i < stringArray.size(); ++i)
            array->addItem(stringArray[i]);
        keyPath->setArray(array);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }

    return keyPath.release();
}
#endif // !PLATFORM(MAC)

class DatabaseLoader : public ExecutableWithDatabase {
public:
    static PassRefPtr<DatabaseLoader> create(ScriptExecutionContext* context, PassRefPtr<RequestDatabaseCallback> requestCallback)
    {
        return adoptRef(new DatabaseLoader(context, requestCallback));
    }

    virtual ~DatabaseLoader() { }

    virtual void execute(PassRefPtr<IDBDatabase> prpDatabase) override
    {
#if PLATFORM(MAC)
        ASSERT_UNUSED(prpDatabase, prpDatabase);
#else
        RefPtr<IDBDatabase> idbDatabase = prpDatabase;
        if (!requestCallback()->isActive())
            return;

        const IDBDatabaseMetadata databaseMetadata = idbDatabase->metadata();

        RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::IndexedDB::ObjectStore>> objectStores = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::IndexedDB::ObjectStore>::create();

        for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = databaseMetadata.objectStores.begin(); it != databaseMetadata.objectStores.end(); ++it) {
            const IDBObjectStoreMetadata& objectStoreMetadata = it->value;

            RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::IndexedDB::ObjectStoreIndex>> indexes = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::IndexedDB::ObjectStoreIndex>::create();

            for (IDBObjectStoreMetadata::IndexMap::const_iterator it = objectStoreMetadata.indexes.begin(); it != objectStoreMetadata.indexes.end(); ++it) {
                const IDBIndexMetadata& indexMetadata = it->value;

                RefPtr<ObjectStoreIndex> objectStoreIndex = ObjectStoreIndex::create()
                    .setName(indexMetadata.name)
                    .setKeyPath(keyPathFromIDBKeyPath(indexMetadata.keyPath))
                    .setUnique(indexMetadata.unique)
                    .setMultiEntry(indexMetadata.multiEntry);
                indexes->addItem(objectStoreIndex);
            }

            RefPtr<ObjectStore> objectStore = ObjectStore::create()
                .setName(objectStoreMetadata.name)
                .setKeyPath(keyPathFromIDBKeyPath(objectStoreMetadata.keyPath))
                .setAutoIncrement(objectStoreMetadata.autoIncrement)
                .setIndexes(indexes);
            objectStores->addItem(objectStore);
        }
        RefPtr<DatabaseWithObjectStores> result = DatabaseWithObjectStores::create()
            .setName(databaseMetadata.name)
            .setIntVersion(databaseMetadata.version)
            .setVersion(String::number(databaseMetadata.version))
            .setObjectStores(objectStores);

        m_requestCallback->sendSuccess(result);
#endif // PLATFORM(MAC)
    }

    virtual RequestCallback* requestCallback() override { return m_requestCallback.get(); }
private:
    DatabaseLoader(ScriptExecutionContext* context, PassRefPtr<RequestDatabaseCallback> requestCallback)
        : ExecutableWithDatabase(context)
        , m_requestCallback(requestCallback) { }
    RefPtr<RequestDatabaseCallback> m_requestCallback;
};

static PassRefPtr<IDBKey> idbKeyFromInspectorObject(InspectorObject* key)
{
    RefPtr<IDBKey> idbKey;

    String type;
    if (!key->getString("type", &type))
        return nullptr;

    DEFINE_STATIC_LOCAL(String, number, (ASCIILiteral("number")));
    DEFINE_STATIC_LOCAL(String, string, (ASCIILiteral("string")));
    DEFINE_STATIC_LOCAL(String, date, (ASCIILiteral("date")));
    DEFINE_STATIC_LOCAL(String, array, (ASCIILiteral("array")));

    if (type == number) {
        double number;
        if (!key->getNumber("number", &number))
            return nullptr;
        idbKey = IDBKey::createNumber(number);
    } else if (type == string) {
        String string;
        if (!key->getString("string", &string))
            return nullptr;
        idbKey = IDBKey::createString(string);
    } else if (type == date) {
        double date;
        if (!key->getNumber("date", &date))
            return nullptr;
        idbKey = IDBKey::createDate(date);
    } else if (type == array) {
        IDBKey::KeyArray keyArray;
        RefPtr<InspectorArray> array = key->getArray("array");
        for (size_t i = 0; i < array->length(); ++i) {
            RefPtr<InspectorValue> value = array->get(i);
            RefPtr<InspectorObject> object;
            if (!value->asObject(&object))
                return nullptr;
            keyArray.append(idbKeyFromInspectorObject(object.get()));
        }
        idbKey = IDBKey::createArray(keyArray);
    } else
        return nullptr;

    return idbKey.release();
}

static PassRefPtr<IDBKeyRange> idbKeyRangeFromKeyRange(InspectorObject* keyRange)
{
    RefPtr<InspectorObject> lower = keyRange->getObject("lower");
    RefPtr<IDBKey> idbLower = lower ? idbKeyFromInspectorObject(lower.get()) : nullptr;
    if (lower && !idbLower)
        return nullptr;

    RefPtr<InspectorObject> upper = keyRange->getObject("upper");
    RefPtr<IDBKey> idbUpper = upper ? idbKeyFromInspectorObject(upper.get()) : nullptr;
    if (upper && !idbUpper)
        return nullptr;

    bool lowerOpen;
    if (!keyRange->getBoolean("lowerOpen", &lowerOpen))
        return nullptr;
    IDBKeyRange::LowerBoundType lowerBoundType = lowerOpen ? IDBKeyRange::LowerBoundOpen : IDBKeyRange::LowerBoundClosed;

    bool upperOpen;
    if (!keyRange->getBoolean("upperOpen", &upperOpen))
        return nullptr;
    IDBKeyRange::UpperBoundType upperBoundType = upperOpen ? IDBKeyRange::UpperBoundOpen : IDBKeyRange::UpperBoundClosed;

    RefPtr<IDBKeyRange> idbKeyRange = IDBKeyRange::create(idbLower, idbUpper, lowerBoundType, upperBoundType);
    return idbKeyRange.release();
}

class DataLoader;

class OpenCursorCallback : public EventListener {
public:
    static PassRefPtr<OpenCursorCallback> create(InjectedScript injectedScript, PassRefPtr<RequestDataCallback> requestCallback, int skipCount, unsigned pageSize)
    {
        return adoptRef(new OpenCursorCallback(injectedScript, requestCallback, skipCount, pageSize));
    }

    virtual ~OpenCursorCallback() { }

    virtual bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    virtual void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (event->type() != eventNames().successEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        ExceptionCode ec = 0;
        RefPtr<IDBAny> requestResult = idbRequest->result(ec);
        if (ec) {
            m_requestCallback->sendFailure("Could not get result in callback.");
            return;
        }
        if (requestResult->type() == IDBAny::ScriptValueType) {
            end(false);
            return;
        }
        if (requestResult->type() != IDBAny::IDBCursorWithValueType) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<IDBCursorWithValue> idbCursor = requestResult->idbCursorWithValue();

        if (m_skipCount) {
            ExceptionCode ec = 0;
            idbCursor->advance(m_skipCount, ec);
            if (ec)
                m_requestCallback->sendFailure("Could not advance cursor.");
            m_skipCount = 0;
            return;
        }

        if (m_result->length() == m_pageSize) {
            end(true);
            return;
        }

        // Continue cursor before making injected script calls, otherwise transaction might be finished.
        idbCursor->continueFunction(nullptr, ec);
        if (ec) {
            m_requestCallback->sendFailure("Could not continue cursor.");
            return;
        }

        RefPtr<DataEntry> dataEntry = DataEntry::create()
            .setKey(m_injectedScript.wrapObject(idbCursor->key(), String()))
            .setPrimaryKey(m_injectedScript.wrapObject(idbCursor->primaryKey(), String()))
            .setValue(m_injectedScript.wrapObject(idbCursor->value(), String()));
        m_result->addItem(dataEntry);

    }

    void end(bool hasMore)
    {
        if (!m_requestCallback->isActive())
            return;
        m_requestCallback->sendSuccess(m_result.release(), hasMore);
    }

private:
    OpenCursorCallback(InjectedScript injectedScript, PassRefPtr<RequestDataCallback> requestCallback, int skipCount, unsigned pageSize)
        : EventListener(EventListener::CPPEventListenerType)
        , m_injectedScript(injectedScript)
        , m_requestCallback(requestCallback)
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
    {
        m_result = Array<DataEntry>::create();
    }
    InjectedScript m_injectedScript;
    RefPtr<RequestDataCallback> m_requestCallback;
    int m_skipCount;
    unsigned m_pageSize;
    RefPtr<Array<DataEntry>> m_result;
};

class DataLoader : public ExecutableWithDatabase {
public:
    static PassRefPtr<DataLoader> create(ScriptExecutionContext* context, PassRefPtr<RequestDataCallback> requestCallback, const InjectedScript& injectedScript, const String& objectStoreName, const String& indexName, PassRefPtr<IDBKeyRange> idbKeyRange, int skipCount, unsigned pageSize)
    {
        return adoptRef(new DataLoader(context, requestCallback, injectedScript, objectStoreName, indexName, idbKeyRange, skipCount, pageSize));
    }

    virtual ~DataLoader() { }

    virtual void execute(PassRefPtr<IDBDatabase> prpDatabase) override
    {
        RefPtr<IDBDatabase> idbDatabase = prpDatabase;
        if (!requestCallback()->isActive())
            return;
        RefPtr<IDBTransaction> idbTransaction = transactionForDatabase(context(), idbDatabase.get(), m_objectStoreName);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }
        RefPtr<IDBObjectStore> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        RefPtr<OpenCursorCallback> openCursorCallback = OpenCursorCallback::create(m_injectedScript, m_requestCallback, m_skipCount, m_pageSize);

        ExceptionCode ec = 0;
        RefPtr<IDBRequest> idbRequest;
        if (!m_indexName.isEmpty()) {
            RefPtr<IDBIndex> idbIndex = indexForObjectStore(idbObjectStore.get(), m_indexName);
            if (!idbIndex) {
                m_requestCallback->sendFailure("Could not get index");
                return;
            }

            idbRequest = idbIndex->openCursor(context(), PassRefPtr<IDBKeyRange>(m_idbKeyRange), ec);
        } else
            idbRequest = idbObjectStore->openCursor(context(), PassRefPtr<IDBKeyRange>(m_idbKeyRange), ec);
        idbRequest->addEventListener(eventNames().successEvent, openCursorCallback, false);
    }

    virtual RequestCallback* requestCallback() override { return m_requestCallback.get(); }
    DataLoader(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<RequestDataCallback> requestCallback, const InjectedScript& injectedScript, const String& objectStoreName, const String& indexName, PassRefPtr<IDBKeyRange> idbKeyRange, int skipCount, unsigned pageSize)
        : ExecutableWithDatabase(scriptExecutionContext)
        , m_requestCallback(requestCallback)
        , m_injectedScript(injectedScript)
        , m_objectStoreName(objectStoreName)
        , m_indexName(indexName)
        , m_idbKeyRange(idbKeyRange)
        , m_skipCount(skipCount)
        , m_pageSize(pageSize) { }
    RefPtr<RequestDataCallback> m_requestCallback;
    InjectedScript m_injectedScript;
    String m_objectStoreName;
    String m_indexName;
    RefPtr<IDBKeyRange> m_idbKeyRange;
    int m_skipCount;
    unsigned m_pageSize;
};

} // namespace

InspectorIndexedDBAgent::InspectorIndexedDBAgent(InstrumentingAgents* instrumentingAgents, InjectedScriptManager* injectedScriptManager, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("IndexedDB"), instrumentingAgents)
    , m_injectedScriptManager(injectedScriptManager)
    , m_pageAgent(pageAgent)
{
}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent()
{
}

void InspectorIndexedDBAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, InspectorBackendDispatcher* backendDispatcher)
{
    m_backendDispatcher = InspectorIndexedDBBackendDispatcher::create(backendDispatcher, this);
}

void InspectorIndexedDBAgent::willDestroyFrontendAndBackend()
{
    m_backendDispatcher.clear();

    disable(nullptr);
}

void InspectorIndexedDBAgent::enable(ErrorString*)
{
}

void InspectorIndexedDBAgent::disable(ErrorString*)
{
}

static Document* assertDocument(ErrorString* errorString, Frame* frame)
{
    Document* document = frame ? frame->document() : nullptr;

    if (!document)
        *errorString = "No document for given frame found";

    return document;
}

static IDBFactory* assertIDBFactory(ErrorString* errorString, Document* document)
{
    DOMWindow* domWindow = document->domWindow();
    if (!domWindow) {
        *errorString = "No IndexedDB factory for given frame found";
        return nullptr;
    }
    IDBFactory* idbFactory = DOMWindowIndexedDatabase::indexedDB(domWindow);

    if (!idbFactory)
        *errorString = "No IndexedDB factory for given frame found";

    return idbFactory;
}

void InspectorIndexedDBAgent::requestDatabaseNames(ErrorString* errorString, const String& securityOrigin, PassRefPtr<RequestDatabaseNamesCallback> requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    ExceptionCode ec = 0;
    RefPtr<IDBRequest> idbRequest = idbFactory->getDatabaseNames(document, ec);
    if (ec) {
        requestCallback->sendFailure("Could not obtain database names.");
        return;
    }
    idbRequest->addEventListener(eventNames().successEvent, GetDatabaseNamesCallback::create(requestCallback, document->securityOrigin()->toRawString()), false);
}

void InspectorIndexedDBAgent::requestDatabase(ErrorString* errorString, const String& securityOrigin, const String& databaseName, PassRefPtr<RequestDatabaseCallback> requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    RefPtr<DatabaseLoader> databaseLoader = DatabaseLoader::create(document, requestCallback);
    databaseLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

void InspectorIndexedDBAgent::requestData(ErrorString* errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, const String& indexName, int skipCount, int pageSize, const RefPtr<InspectorObject>* keyRange, PassRefPtr<RequestDataCallback> requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(mainWorldExecState(frame));

    RefPtr<IDBKeyRange> idbKeyRange = keyRange ? idbKeyRangeFromKeyRange(keyRange->get()) : nullptr;
    if (keyRange && !idbKeyRange) {
        *errorString = "Can not parse key range.";
        return;
    }

    RefPtr<DataLoader> dataLoader = DataLoader::create(document, requestCallback, injectedScript, objectStoreName, indexName, idbKeyRange, skipCount, pageSize);
    dataLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

class ClearObjectStoreListener : public EventListener {
    WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);
public:
    static PassRefPtr<ClearObjectStoreListener> create(PassRefPtr<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(new ClearObjectStoreListener(requestCallback));
    }

    virtual ~ClearObjectStoreListener() { }

    virtual bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    virtual void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != eventNames().completeEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        m_requestCallback->sendSuccess();
    }
private:
    ClearObjectStoreListener(PassRefPtr<ClearObjectStoreCallback> requestCallback)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(requestCallback)
    {
    }

    RefPtr<ClearObjectStoreCallback> m_requestCallback;
};


class ClearObjectStore : public ExecutableWithDatabase {
public:
    static PassRefPtr<ClearObjectStore> create(ScriptExecutionContext* context, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(new ClearObjectStore(context, objectStoreName, requestCallback));
    }

    ClearObjectStore(ScriptExecutionContext* context, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
        : ExecutableWithDatabase(context)
        , m_objectStoreName(objectStoreName)
        , m_requestCallback(requestCallback)
    {
    }

    virtual void execute(PassRefPtr<IDBDatabase> prpDatabase) override
    {
        RefPtr<IDBDatabase> idbDatabase = prpDatabase;
        if (!requestCallback()->isActive())
            return;
        RefPtr<IDBTransaction> idbTransaction = transactionForDatabase(context(), idbDatabase.get(), m_objectStoreName, IDBTransaction::modeReadWrite());
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }
        RefPtr<IDBObjectStore> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        ExceptionCode ec = 0;
        RefPtr<IDBRequest> idbRequest = idbObjectStore->clear(context(), ec);
        ASSERT(!ec);
        if (ec) {
            m_requestCallback->sendFailure(String::format("Could not clear object store '%s': %d", m_objectStoreName.utf8().data(), ec));
            return;
        }
        idbTransaction->addEventListener(eventNames().completeEvent, ClearObjectStoreListener::create(m_requestCallback), false);
    }

    virtual RequestCallback* requestCallback() override { return m_requestCallback.get(); }
private:
    const String m_objectStoreName;
    RefPtr<ClearObjectStoreCallback> m_requestCallback;
};

void InspectorIndexedDBAgent::clearObjectStore(ErrorString* errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    RefPtr<ClearObjectStore> clearObjectStore = ClearObjectStore::create(document, objectStoreName, requestCallback);
    clearObjectStore->start(idbFactory, document->securityOrigin(), databaseName);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(INDEXED_DATABASE)
