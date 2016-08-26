/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "InspectorIndexedDBAgent.h"

#include "DOMStringList.h"
#include "DOMWindow.h"
#include "DOMWindowIndexedDatabase.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "IDBBindingUtilities.h"
#include "IDBCursor.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBOpenDBRequest.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "ScriptState.h"
#include "SecurityOrigin.h"
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <inspector/InspectorFrontendRouter.h>
#include <inspector/InspectorValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

using Inspector::Protocol::Array;
using Inspector::Protocol::IndexedDB::DatabaseWithObjectStores;
using Inspector::Protocol::IndexedDB::DataEntry;
using Inspector::Protocol::IndexedDB::Key;
using Inspector::Protocol::IndexedDB::KeyPath;
using Inspector::Protocol::IndexedDB::KeyRange;
using Inspector::Protocol::IndexedDB::ObjectStore;
using Inspector::Protocol::IndexedDB::ObjectStoreIndex;

typedef Inspector::BackendDispatcher::CallbackBase RequestCallback;
typedef Inspector::IndexedDBBackendDispatcherHandler::RequestDatabaseNamesCallback RequestDatabaseNamesCallback;
typedef Inspector::IndexedDBBackendDispatcherHandler::RequestDatabaseCallback RequestDatabaseCallback;
typedef Inspector::IndexedDBBackendDispatcherHandler::RequestDataCallback RequestDataCallback;
typedef Inspector::IndexedDBBackendDispatcherHandler::ClearObjectStoreCallback ClearObjectStoreCallback;

using namespace Inspector;

namespace WebCore {

namespace {

class ExecutableWithDatabase : public RefCounted<ExecutableWithDatabase> {
public:
    ExecutableWithDatabase(ScriptExecutionContext* context)
        : m_context(context) { }
    virtual ~ExecutableWithDatabase() { }
    void start(IDBFactory*, SecurityOrigin*, const String& databaseName);
    virtual void execute(IDBDatabase&) = 0;
    virtual RequestCallback& requestCallback() = 0;
    ScriptExecutionContext* context() const { return m_context; }
private:
    ScriptExecutionContext* m_context;
};

class OpenDatabaseCallback final : public EventListener {
public:
    static Ref<OpenDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRef(*new OpenDatabaseCallback(executableWithDatabase));
    }

    virtual ~OpenDatabaseCallback() { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (event->type() != eventNames().successEvent) {
            m_executableWithDatabase->requestCallback().sendFailure("Unexpected event type.");
            return;
        }

        auto& request = static_cast<IDBOpenDBRequest&>(*event->target());
        if (!request.isDone()) {
            m_executableWithDatabase->requestCallback().sendFailure("Could not get result in callback.");
            return;
        }
        auto databaseResult = request.databaseResult();
        if (!databaseResult) {
            m_executableWithDatabase->requestCallback().sendFailure("Unexpected result type.");
            return;
        }

        m_executableWithDatabase->execute(*databaseResult);
        databaseResult->close();
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactory* idbFactory, SecurityOrigin*, const String& databaseName)
{
    if (!context()) {
        requestCallback().sendFailure("Could not open database.");
        return;
    }

    ExceptionCodeWithMessage ec;
    RefPtr<IDBOpenDBRequest> idbOpenDBRequest = idbFactory->open(*context(), databaseName, Nullopt, ec);
    if (ec.code) {
        requestCallback().sendFailure("Could not open database.");
        return;
    }

    Ref<OpenDatabaseCallback> callback = OpenDatabaseCallback::create(this);
    idbOpenDBRequest->addEventListener(eventNames().successEvent, WTFMove(callback), false);
}


static RefPtr<KeyPath> keyPathFromIDBKeyPath(const IDBKeyPath& idbKeyPath)
{
    RefPtr<KeyPath> keyPath;
    switch (idbKeyPath.type()) {
    case IDBKeyPath::Type::Null:
        keyPath = KeyPath::create()
            .setType(KeyPath::Type::Null)
            .release();
        break;

    case IDBKeyPath::Type::String:
        keyPath = KeyPath::create()
            .setType(KeyPath::Type::String)
            .release();
        keyPath->setString(idbKeyPath.string());
        break;

    case IDBKeyPath::Type::Array: {
        auto array = Inspector::Protocol::Array<String>::create();
        for (auto& string : idbKeyPath.array())
            array->addItem(string);
        keyPath = KeyPath::create()
            .setType(KeyPath::Type::Array)
            .release();
        keyPath->setArray(WTFMove(array));
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }

    return keyPath;
}

static RefPtr<IDBTransaction> transactionForDatabase(IDBDatabase* idbDatabase, const String& objectStoreName, const String& mode = IDBTransaction::modeReadOnly())
{
    ExceptionCodeWithMessage ec;
    RefPtr<IDBTransaction> idbTransaction = idbDatabase->transaction(objectStoreName, mode, ec);
    if (ec.code)
        return nullptr;
    return idbTransaction;
}

static RefPtr<IDBObjectStore> objectStoreForTransaction(IDBTransaction* idbTransaction, const String& objectStoreName)
{
    ExceptionCodeWithMessage ec;
    RefPtr<IDBObjectStore> idbObjectStore = idbTransaction->objectStore(objectStoreName, ec);
    if (ec.code)
        return nullptr;
    return idbObjectStore;
}

static RefPtr<IDBIndex> indexForObjectStore(IDBObjectStore* idbObjectStore, const String& indexName)
{
    ExceptionCodeWithMessage ec;
    RefPtr<IDBIndex> idbIndex = idbObjectStore->index(indexName, ec);
    if (ec.code)
        return nullptr;
    return idbIndex;
}

class DatabaseLoader final : public ExecutableWithDatabase {
public:
    static Ref<DatabaseLoader> create(ScriptExecutionContext* context, Ref<RequestDatabaseCallback>&& requestCallback)
    {
        return adoptRef(*new DatabaseLoader(context, WTFMove(requestCallback)));
    }

    virtual ~DatabaseLoader() { }

    void execute(IDBDatabase& database) override
    {
        if (!requestCallback().isActive())
            return;
    
        auto& databaseInfo = database.info();
        auto objectStores = Inspector::Protocol::Array<Inspector::Protocol::IndexedDB::ObjectStore>::create();
        auto objectStoreNames = databaseInfo.objectStoreNames();
        for (auto& name : objectStoreNames) {
            auto* objectStoreInfo = databaseInfo.infoForExistingObjectStore(name);
            if (!objectStoreInfo)
                continue;

            auto indexes = Inspector::Protocol::Array<Inspector::Protocol::IndexedDB::ObjectStoreIndex>::create();
    
            for (auto& indexInfo : objectStoreInfo->indexMap().values()) {
                Ref<ObjectStoreIndex> objectStoreIndex = ObjectStoreIndex::create()
                    .setName(indexInfo.name())
                    .setKeyPath(keyPathFromIDBKeyPath(indexInfo.keyPath()))
                    .setUnique(indexInfo.unique())
                    .setMultiEntry(indexInfo.multiEntry())
                    .release();
                indexes->addItem(WTFMove(objectStoreIndex));
            }
    
            Ref<ObjectStore> objectStore = ObjectStore::create()
                .setName(objectStoreInfo->name())
                .setKeyPath(keyPathFromIDBKeyPath(objectStoreInfo->keyPath()))
                .setAutoIncrement(objectStoreInfo->autoIncrement())
                .setIndexes(WTFMove(indexes))
                .release();
            objectStores->addItem(WTFMove(objectStore));
        }
    
        Ref<DatabaseWithObjectStores> result = DatabaseWithObjectStores::create()
            .setName(databaseInfo.name())
            .setVersion(databaseInfo.version())
            .setObjectStores(WTFMove(objectStores))
            .release();
        m_requestCallback->sendSuccess(WTFMove(result));
    }

    RequestCallback& requestCallback() override { return m_requestCallback.get(); }
private:
    DatabaseLoader(ScriptExecutionContext* context, Ref<RequestDatabaseCallback>&& requestCallback)
        : ExecutableWithDatabase(context)
        , m_requestCallback(WTFMove(requestCallback)) { }
    Ref<RequestDatabaseCallback> m_requestCallback;
};

static RefPtr<IDBKey> idbKeyFromInspectorObject(InspectorObject* key)
{
    String type;
    if (!key->getString("type", type))
        return nullptr;

    static NeverDestroyed<const String> numberType(ASCIILiteral("number"));
    static NeverDestroyed<const String> stringType(ASCIILiteral("string"));
    static NeverDestroyed<const String> dateType(ASCIILiteral("date"));
    static NeverDestroyed<const String> arrayType(ASCIILiteral("array"));

    RefPtr<IDBKey> idbKey;
    if (type == numberType) {
        double number;
        if (!key->getDouble("number", number))
            return nullptr;
        idbKey = IDBKey::createNumber(number);
    } else if (type == stringType) {
        String string;
        if (!key->getString("string", string))
            return nullptr;
        idbKey = IDBKey::createString(string);
    } else if (type == dateType) {
        double date;
        if (!key->getDouble("date", date))
            return nullptr;
        idbKey = IDBKey::createDate(date);
    } else if (type == arrayType) {
        Vector<RefPtr<IDBKey>> keyArray;
        RefPtr<InspectorArray> array;
        if (!key->getArray("array", array))
            return nullptr;
        for (size_t i = 0; i < array->length(); ++i) {
            RefPtr<InspectorValue> value = array->get(i);
            RefPtr<InspectorObject> object;
            if (!value->asObject(object))
                return nullptr;
            keyArray.append(idbKeyFromInspectorObject(object.get()));
        }
        idbKey = IDBKey::createArray(keyArray);
    } else
        return nullptr;

    return idbKey;
}

static RefPtr<IDBKeyRange> idbKeyRangeFromKeyRange(const InspectorObject* keyRange)
{
    RefPtr<IDBKey> idbLower;
    RefPtr<InspectorObject> lower;
    if (keyRange->getObject(ASCIILiteral("lower"), lower)) {
        idbLower = idbKeyFromInspectorObject(lower.get());
        if (!idbLower)
            return nullptr;
    }

    RefPtr<IDBKey> idbUpper;
    RefPtr<InspectorObject> upper;
    if (keyRange->getObject(ASCIILiteral("upper"), upper)) {
        idbUpper = idbKeyFromInspectorObject(upper.get());
        if (!idbUpper)
            return nullptr;
    }

    bool lowerOpen;
    if (!keyRange->getBoolean(ASCIILiteral("lowerOpen"), lowerOpen))
        return nullptr;

    bool upperOpen;
    if (!keyRange->getBoolean(ASCIILiteral("upperOpen"), upperOpen))
        return nullptr;

    return IDBKeyRange::create(WTFMove(idbLower), WTFMove(idbUpper), lowerOpen, upperOpen);
}

class DataLoader;

class OpenCursorCallback : public EventListener {
public:
    static Ref<OpenCursorCallback> create(InjectedScript injectedScript, Ref<RequestDataCallback>&& requestCallback, int skipCount, unsigned pageSize)
    {
        return adoptRef(*new OpenCursorCallback(injectedScript, WTFMove(requestCallback), skipCount, pageSize));
    }

    virtual ~OpenCursorCallback() { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext* context, Event* event) override
    {
        if (event->type() != eventNames().successEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        auto& request = static_cast<IDBRequest&>(*event->target());
        if (!request.isDone()) {
            m_requestCallback->sendFailure("Could not get result in callback.");
            return;
        }
        if (request.scriptResult()) {
            end(false);
            return;
        }
        auto* cursorResult = request.cursorResult();
        if (!cursorResult) {
            end(false);
            return;
        }

        auto& cursor = *cursorResult;

        if (m_skipCount) {
            ExceptionCodeWithMessage ec;
            cursor.advance(m_skipCount, ec);
            if (ec.code)
                m_requestCallback->sendFailure("Could not advance cursor.");
            m_skipCount = 0;
            return;
        }

        if (m_result->length() == m_pageSize) {
            end(true);
            return;
        }

        // Continue cursor before making injected script calls, otherwise transaction might be finished.
        ExceptionCodeWithMessage ec;
        cursor.continueFunction(nullptr, ec);
        if (ec.code) {
            m_requestCallback->sendFailure("Could not continue cursor.");
            return;
        }

        JSC::ExecState* state = context ? context->execState() : nullptr;
        if (!state)
            return;

        RefPtr<DataEntry> dataEntry = DataEntry::create()
            .setKey(m_injectedScript.wrapObject(cursor.key(), String(), true))
            .setPrimaryKey(m_injectedScript.wrapObject(cursor.primaryKey(), String(), true))
            .setValue(m_injectedScript.wrapObject(cursor.value(), String(), true))
            .release();
        m_result->addItem(WTFMove(dataEntry));
    }

    void end(bool hasMore)
    {
        if (!m_requestCallback->isActive())
            return;
        m_requestCallback->sendSuccess(WTFMove(m_result), hasMore);
    }

private:
    OpenCursorCallback(InjectedScript injectedScript, Ref<RequestDataCallback>&& requestCallback, int skipCount, unsigned pageSize)
        : EventListener(EventListener::CPPEventListenerType)
        , m_injectedScript(injectedScript)
        , m_requestCallback(WTFMove(requestCallback))
        , m_result(Array<DataEntry>::create())
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
    {
    }
    InjectedScript m_injectedScript;
    Ref<RequestDataCallback> m_requestCallback;
    Ref<Array<DataEntry>> m_result;
    int m_skipCount;
    unsigned m_pageSize;
};

class DataLoader : public ExecutableWithDatabase {
public:
    static Ref<DataLoader> create(ScriptExecutionContext* context, Ref<RequestDataCallback>&& requestCallback, const InjectedScript& injectedScript, const String& objectStoreName, const String& indexName, RefPtr<IDBKeyRange>&& idbKeyRange, int skipCount, unsigned pageSize)
    {
        return adoptRef(*new DataLoader(context, WTFMove(requestCallback), injectedScript, objectStoreName, indexName, WTFMove(idbKeyRange), skipCount, pageSize));
    }

    virtual ~DataLoader() { }

    void execute(IDBDatabase& database) override
    {
        if (!requestCallback().isActive())
            return;

        RefPtr<IDBTransaction> idbTransaction = transactionForDatabase(&database, m_objectStoreName);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }

        RefPtr<IDBObjectStore> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        TransactionActivator activator(idbTransaction.get());
        ExceptionCodeWithMessage ec;
        RefPtr<IDBRequest> idbRequest;
        JSC::ExecState* exec = context() ? context()->execState() : nullptr;
        if (!m_indexName.isEmpty()) {
            RefPtr<IDBIndex> idbIndex = indexForObjectStore(idbObjectStore.get(), m_indexName);
            if (!idbIndex) {
                m_requestCallback->sendFailure("Could not get index");
                return;
            }

            idbRequest = exec ? idbIndex->openCursor(*exec, m_idbKeyRange.get(), IDBCursor::directionNext(), ec) : nullptr;
        } else
            idbRequest = exec ? idbObjectStore->openCursor(*exec, m_idbKeyRange.get(), IDBCursor::directionNext(), ec) : nullptr;

        if (!idbRequest) {
            m_requestCallback->sendFailure("Could not open cursor to populate database data");
            return;
        }

        Ref<OpenCursorCallback> openCursorCallback = OpenCursorCallback::create(m_injectedScript, m_requestCallback.copyRef(), m_skipCount, m_pageSize);
        idbRequest->addEventListener(eventNames().successEvent, WTFMove(openCursorCallback), false);
    }

    RequestCallback& requestCallback() override { return m_requestCallback.get(); }
    DataLoader(ScriptExecutionContext* scriptExecutionContext, Ref<RequestDataCallback>&& requestCallback, const InjectedScript& injectedScript, const String& objectStoreName, const String& indexName, RefPtr<IDBKeyRange> idbKeyRange, int skipCount, unsigned pageSize)
        : ExecutableWithDatabase(scriptExecutionContext)
        , m_requestCallback(WTFMove(requestCallback))
        , m_injectedScript(injectedScript)
        , m_objectStoreName(objectStoreName)
        , m_indexName(indexName)
        , m_idbKeyRange(WTFMove(idbKeyRange))
        , m_skipCount(skipCount)
        , m_pageSize(pageSize) { }
    Ref<RequestDataCallback> m_requestCallback;
    InjectedScript m_injectedScript;
    String m_objectStoreName;
    String m_indexName;
    RefPtr<IDBKeyRange> m_idbKeyRange;
    int m_skipCount;
    unsigned m_pageSize;
};

} // namespace

InspectorIndexedDBAgent::InspectorIndexedDBAgent(WebAgentContext& context, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("IndexedDB"), context)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_backendDispatcher(Inspector::IndexedDBBackendDispatcher::create(context.backendDispatcher, this))
    , m_pageAgent(pageAgent)
{
}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent()
{
}

void InspectorIndexedDBAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorIndexedDBAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString unused;
    disable(unused);
}

void InspectorIndexedDBAgent::enable(ErrorString&)
{
}

void InspectorIndexedDBAgent::disable(ErrorString&)
{
}

static Document* assertDocument(ErrorString& errorString, Frame* frame)
{
    Document* document = frame ? frame->document() : nullptr;
    if (!document)
        errorString = ASCIILiteral("No document for given frame found");
    return document;
}

static IDBFactory* assertIDBFactory(ErrorString& errorString, Document* document)
{
    DOMWindow* domWindow = document->domWindow();
    if (!domWindow) {
        errorString = ASCIILiteral("No IndexedDB factory for given frame found");
        return nullptr;
    }

    IDBFactory* idbFactory = DOMWindowIndexedDatabase::indexedDB(*domWindow);
    if (!idbFactory)
        errorString = ASCIILiteral("No IndexedDB factory for given frame found");

    return idbFactory;
}

void InspectorIndexedDBAgent::requestDatabaseNames(ErrorString& errorString, const String& securityOrigin, Ref<RequestDatabaseNamesCallback>&& requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;

    auto* openingOrigin = document->securityOrigin();
    if (!openingOrigin)
        return;

    auto* topOrigin = document->topOrigin();
    if (!topOrigin)
        return;

    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    RefPtr<RequestDatabaseNamesCallback> callback = WTFMove(requestCallback);
    idbFactory->getAllDatabaseNames(*topOrigin, *openingOrigin, [callback](auto& databaseNames) {
        if (!callback->isActive())
            return;

        Ref<Inspector::Protocol::Array<String>> databaseNameArray = Inspector::Protocol::Array<String>::create();
        for (auto& databaseName : databaseNames)
            databaseNameArray->addItem(databaseName);

        callback->sendSuccess(WTFMove(databaseNameArray));
    });
}

void InspectorIndexedDBAgent::requestDatabase(ErrorString& errorString, const String& securityOrigin, const String& databaseName, Ref<RequestDatabaseCallback>&& requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;

    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    Ref<DatabaseLoader> databaseLoader = DatabaseLoader::create(document, WTFMove(requestCallback));
    databaseLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

void InspectorIndexedDBAgent::requestData(ErrorString& errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, const String& indexName, int skipCount, int pageSize, const InspectorObject* keyRange, Ref<RequestDataCallback>&& requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;

    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(mainWorldExecState(frame));

    RefPtr<IDBKeyRange> idbKeyRange = keyRange ? idbKeyRangeFromKeyRange(keyRange) : nullptr;
    if (keyRange && !idbKeyRange) {
        errorString = ASCIILiteral("Can not parse key range.");
        return;
    }

    Ref<DataLoader> dataLoader = DataLoader::create(document, WTFMove(requestCallback), injectedScript, objectStoreName, indexName, WTFMove(idbKeyRange), skipCount, pageSize);
    dataLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

class ClearObjectStoreListener : public EventListener {
    WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);
public:
    static Ref<ClearObjectStoreListener> create(Ref<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(*new ClearObjectStoreListener(WTFMove(requestCallback)));
    }

    virtual ~ClearObjectStoreListener() { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext*, Event* event) override
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
    ClearObjectStoreListener(Ref<ClearObjectStoreCallback>&& requestCallback)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(WTFMove(requestCallback))
    {
    }

    Ref<ClearObjectStoreCallback> m_requestCallback;
};

class ClearObjectStore : public ExecutableWithDatabase {
public:
    static Ref<ClearObjectStore> create(ScriptExecutionContext* context, const String& objectStoreName, Ref<ClearObjectStoreCallback>&& requestCallback)
    {
        return adoptRef(*new ClearObjectStore(context, objectStoreName, WTFMove(requestCallback)));
    }

    ClearObjectStore(ScriptExecutionContext* context, const String& objectStoreName, Ref<ClearObjectStoreCallback>&& requestCallback)
        : ExecutableWithDatabase(context)
        , m_objectStoreName(objectStoreName)
        , m_requestCallback(WTFMove(requestCallback))
    {
    }

    void execute(IDBDatabase& database) override
    {
        if (!requestCallback().isActive())
            return;

        RefPtr<IDBTransaction> idbTransaction = transactionForDatabase(&database, m_objectStoreName, IDBTransaction::modeReadWrite());
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }

        RefPtr<IDBObjectStore> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        TransactionActivator activator(idbTransaction.get());
        ExceptionCodeWithMessage ec;
        JSC::ExecState* exec = context() ? context()->execState() : nullptr;
        RefPtr<IDBRequest> idbRequest = exec ? idbObjectStore->clear(*exec, ec) : nullptr;
        ASSERT(!ec.code);
        if (ec.code) {
            m_requestCallback->sendFailure(String::format("Could not clear object store '%s': %d", m_objectStoreName.utf8().data(), ec.code));
            return;
        }

        idbTransaction->addEventListener(eventNames().completeEvent, ClearObjectStoreListener::create(m_requestCallback.copyRef()), false);
    }

    RequestCallback& requestCallback() override { return m_requestCallback.get(); }
private:
    const String m_objectStoreName;
    Ref<ClearObjectStoreCallback> m_requestCallback;
};

void InspectorIndexedDBAgent::clearObjectStore(ErrorString& errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, Ref<ClearObjectStoreCallback>&& requestCallback)
{
    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    Ref<ClearObjectStore> clearObjectStore = ClearObjectStore::create(document, objectStoreName, WTFMove(requestCallback));
    clearObjectStore->start(idbFactory, document->securityOrigin(), databaseName);
}

} // namespace WebCore
#endif // ENABLE(INDEXED_DATABASE)
