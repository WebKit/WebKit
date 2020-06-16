/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "DOMWindow.h"
#include "DOMWindowIndexedDatabase.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
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
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/text/StringConcatenateNumbers.h>

using JSON::ArrayOf;
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

typedef String ErrorString;

template <typename T>
using ErrorStringOr = Expected<T, ErrorString>;

namespace WebCore {

using namespace Inspector;

namespace {

class ExecutableWithDatabase : public RefCounted<ExecutableWithDatabase> {
public:
    ExecutableWithDatabase(ScriptExecutionContext* context)
        : m_context(context) { }
    virtual ~ExecutableWithDatabase() = default;
    void start(IDBFactory*, SecurityOrigin*, const String& databaseName);
    virtual void execute(IDBDatabase&) = 0;
    virtual RequestCallback& requestCallback() = 0;
    ScriptExecutionContext* context() const { return m_context; }
private:
    ScriptExecutionContext* m_context;
};

class OpenDatabaseCallback final : public EventListener {
public:
    static Ref<OpenDatabaseCallback> create(ExecutableWithDatabase& executableWithDatabase)
    {
        return adoptRef(*new OpenDatabaseCallback(executableWithDatabase));
    }

    bool operator==(const EventListener& other) const final
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        if (event.type() != eventNames().successEvent) {
            m_executableWithDatabase->requestCallback().sendFailure("Unexpected event type.");
            return;
        }

        auto& request = static_cast<IDBOpenDBRequest&>(*event.target());

        auto result = request.result();
        if (result.hasException()) {
            m_executableWithDatabase->requestCallback().sendFailure("Could not get result in callback.");
            return;
        }

        auto resultValue = result.releaseReturnValue();
        if (!WTF::holds_alternative<RefPtr<IDBDatabase>>(resultValue)) {
            m_executableWithDatabase->requestCallback().sendFailure("Unexpected result type.");
            return;
        }

        auto databaseResult = WTF::get<RefPtr<IDBDatabase>>(resultValue);
        m_executableWithDatabase->execute(*databaseResult);
        databaseResult->close();
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase& executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    Ref<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactory* idbFactory, SecurityOrigin*, const String& databaseName)
{
    if (!context()) {
        requestCallback().sendFailure("Could not open database.");
        return;
    }

    auto result = idbFactory->open(*context(), databaseName, WTF::nullopt);
    if (result.hasException()) {
        requestCallback().sendFailure("Could not open database.");
        return;
    }

    result.releaseReturnValue()->addEventListener(eventNames().successEvent, OpenDatabaseCallback::create(*this), false);
}


static RefPtr<KeyPath> keyPathFromIDBKeyPath(const Optional<IDBKeyPath>& idbKeyPath)
{
    if (!idbKeyPath)
        return KeyPath::create().setType(KeyPath::Type::Null).release();

    auto visitor = WTF::makeVisitor([](const String& string) {
        auto keyPath = KeyPath::create().setType(KeyPath::Type::String).release();
        keyPath->setString(string);
        return keyPath;
    }, [](const Vector<String>& vector) {
        auto array = JSON::ArrayOf<String>::create();
        for (auto& string : vector)
            array->addItem(string);
        auto keyPath = KeyPath::create().setType(KeyPath::Type::Array).release();
        keyPath->setArray(WTFMove(array));
        return keyPath;
    });
    return WTF::visit(visitor, idbKeyPath.value());
}

static RefPtr<IDBTransaction> transactionForDatabase(IDBDatabase* idbDatabase, const String& objectStoreName, IDBTransactionMode mode = IDBTransactionMode::Readonly)
{
    auto result = idbDatabase->transaction(objectStoreName, mode);
    if (result.hasException())
        return nullptr;
    return result.releaseReturnValue();
}

static RefPtr<IDBObjectStore> objectStoreForTransaction(IDBTransaction* idbTransaction, const String& objectStoreName)
{
    auto result = idbTransaction->objectStore(objectStoreName);
    if (result.hasException())
        return nullptr;
    return result.releaseReturnValue();
}

static RefPtr<IDBIndex> indexForObjectStore(IDBObjectStore* idbObjectStore, const String& indexName)
{
    auto index = idbObjectStore->index(indexName);
    if (index.hasException())
        return nullptr;
    return index.releaseReturnValue();
}

class DatabaseLoader final : public ExecutableWithDatabase {
public:
    static Ref<DatabaseLoader> create(ScriptExecutionContext* context, Ref<RequestDatabaseCallback>&& requestCallback)
    {
        return adoptRef(*new DatabaseLoader(context, WTFMove(requestCallback)));
    }

    ~DatabaseLoader() override = default;

    void execute(IDBDatabase& database) override
    {
        if (!requestCallback().isActive())
            return;
    
        auto& databaseInfo = database.info();
        auto objectStores = JSON::ArrayOf<Inspector::Protocol::IndexedDB::ObjectStore>::create();
        auto objectStoreNames = databaseInfo.objectStoreNames();
        for (auto& name : objectStoreNames) {
            auto* objectStoreInfo = databaseInfo.infoForExistingObjectStore(name);
            if (!objectStoreInfo)
                continue;

            auto indexes = JSON::ArrayOf<Inspector::Protocol::IndexedDB::ObjectStoreIndex>::create();
    
            for (auto& indexInfo : objectStoreInfo->indexMap().values()) {
                auto objectStoreIndex = ObjectStoreIndex::create()
                    .setName(indexInfo.name())
                    .setKeyPath(keyPathFromIDBKeyPath(indexInfo.keyPath()))
                    .setUnique(indexInfo.unique())
                    .setMultiEntry(indexInfo.multiEntry())
                    .release();
                indexes->addItem(WTFMove(objectStoreIndex));
            }
    
            auto objectStore = ObjectStore::create()
                .setName(objectStoreInfo->name())
                .setKeyPath(keyPathFromIDBKeyPath(objectStoreInfo->keyPath()))
                .setAutoIncrement(objectStoreInfo->autoIncrement())
                .setIndexes(WTFMove(indexes))
                .release();
            objectStores->addItem(WTFMove(objectStore));
        }
    
        auto result = DatabaseWithObjectStores::create()
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

static RefPtr<IDBKey> idbKeyFromInspectorObject(JSON::Object* key)
{
    String type;
    if (!key->getString("type", type))
        return nullptr;

    static NeverDestroyed<const String> numberType(MAKE_STATIC_STRING_IMPL("number"));
    static NeverDestroyed<const String> stringType(MAKE_STATIC_STRING_IMPL("string"));
    static NeverDestroyed<const String> dateType(MAKE_STATIC_STRING_IMPL("date"));
    static NeverDestroyed<const String> arrayType(MAKE_STATIC_STRING_IMPL("array"));

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
        RefPtr<JSON::Array> array;
        if (!key->getArray("array", array))
            return nullptr;
        for (size_t i = 0; i < array->length(); ++i) {
            RefPtr<JSON::Value> value = array->get(i);
            RefPtr<JSON::Object> object;
            if (!value->asObject(object))
                return nullptr;
            keyArray.append(idbKeyFromInspectorObject(object.get()));
        }
        idbKey = IDBKey::createArray(keyArray);
    } else
        return nullptr;

    return idbKey;
}

static RefPtr<IDBKeyRange> idbKeyRangeFromKeyRange(const JSON::Object* keyRange)
{
    RefPtr<IDBKey> idbLower;
    RefPtr<JSON::Object> lower;
    if (keyRange->getObject("lower"_s, lower)) {
        idbLower = idbKeyFromInspectorObject(lower.get());
        if (!idbLower)
            return nullptr;
    }

    RefPtr<IDBKey> idbUpper;
    RefPtr<JSON::Object> upper;
    if (keyRange->getObject("upper"_s, upper)) {
        idbUpper = idbKeyFromInspectorObject(upper.get());
        if (!idbUpper)
            return nullptr;
    }

    bool lowerOpen;
    if (!keyRange->getBoolean("lowerOpen"_s, lowerOpen))
        return nullptr;

    bool upperOpen;
    if (!keyRange->getBoolean("upperOpen"_s, upperOpen))
        return nullptr;

    return IDBKeyRange::create(WTFMove(idbLower), WTFMove(idbUpper), lowerOpen, upperOpen);
}

class OpenCursorCallback final : public EventListener {
public:
    static Ref<OpenCursorCallback> create(InjectedScript injectedScript, Ref<RequestDataCallback>&& requestCallback, int skipCount, unsigned pageSize)
    {
        return adoptRef(*new OpenCursorCallback(injectedScript, WTFMove(requestCallback), skipCount, pageSize));
    }

    ~OpenCursorCallback() override = default;

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext& context, Event& event) override
    {
        if (event.type() != eventNames().successEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        auto& request = static_cast<IDBRequest&>(*event.target());

        auto result = request.result();
        if (result.hasException()) {
            m_requestCallback->sendFailure("Could not get result in callback.");
            return;
        }
        
        auto resultValue = result.releaseReturnValue();
        if (!WTF::holds_alternative<RefPtr<IDBCursor>>(resultValue)) {
            end(false);
            return;
        }

        auto cursor = WTF::get<RefPtr<IDBCursor>>(resultValue);

        if (m_skipCount) {
            if (cursor->advance(m_skipCount).hasException())
                m_requestCallback->sendFailure("Could not advance cursor.");
            m_skipCount = 0;
            return;
        }

        if (m_result->length() == m_pageSize) {
            end(true);
            return;
        }

        // Continue cursor before making injected script calls, otherwise transaction might be finished.
        if (cursor->continueFunction(nullptr).hasException()) {
            m_requestCallback->sendFailure("Could not continue cursor.");
            return;
        }

        auto* lexicalGlobalObject = context.execState();
        auto key =  toJS(*lexicalGlobalObject, *lexicalGlobalObject, cursor->key());
        auto primaryKey = toJS(*lexicalGlobalObject, *lexicalGlobalObject, cursor->primaryKey());
        auto value = deserializeIDBValueToJSValue(*lexicalGlobalObject, cursor->value());
        auto dataEntry = DataEntry::create()
            .setKey(m_injectedScript.wrapObject(key, String(), true))
            .setPrimaryKey(m_injectedScript.wrapObject(primaryKey, String(), true))
            .setValue(m_injectedScript.wrapObject(value, String(), true))
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
        , m_result(JSON::ArrayOf<DataEntry>::create())
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
    {
    }
    InjectedScript m_injectedScript;
    Ref<RequestDataCallback> m_requestCallback;
    Ref<JSON::ArrayOf<DataEntry>> m_result;
    int m_skipCount;
    unsigned m_pageSize;
};

class DataLoader final : public ExecutableWithDatabase {
public:
    static Ref<DataLoader> create(ScriptExecutionContext* context, Ref<RequestDataCallback>&& requestCallback, const InjectedScript& injectedScript, const String& objectStoreName, const String& indexName, RefPtr<IDBKeyRange>&& idbKeyRange, int skipCount, unsigned pageSize)
    {
        return adoptRef(*new DataLoader(context, WTFMove(requestCallback), injectedScript, objectStoreName, indexName, WTFMove(idbKeyRange), skipCount, pageSize));
    }

    ~DataLoader() override = default;

    void execute(IDBDatabase& database) override
    {
        if (!requestCallback().isActive())
            return;

        auto idbTransaction = transactionForDatabase(&database, m_objectStoreName);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }

        auto idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        TransactionActivator activator(idbTransaction.get());
        RefPtr<IDBRequest> idbRequest;
        auto* exec = context() ? context()->execState() : nullptr;
        if (!m_indexName.isEmpty()) {
            auto idbIndex = indexForObjectStore(idbObjectStore.get(), m_indexName);
            if (!idbIndex) {
                m_requestCallback->sendFailure("Could not get index");
                return;
            }

            if (exec) {
                auto result = idbIndex->openCursor(*exec, m_idbKeyRange.get(), IDBCursorDirection::Next);
                if (!result.hasException())
                    idbRequest = result.releaseReturnValue();
            }
        } else {
            if (exec) {
                auto result = idbObjectStore->openCursor(*exec, m_idbKeyRange.get(), IDBCursorDirection::Next);
                if (!result.hasException())
                    idbRequest = result.releaseReturnValue();
            }
        }

        if (!idbRequest) {
            m_requestCallback->sendFailure("Could not open cursor to populate database data");
            return;
        }

        auto openCursorCallback = OpenCursorCallback::create(m_injectedScript, m_requestCallback.copyRef(), m_skipCount, m_pageSize);
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

InspectorIndexedDBAgent::InspectorIndexedDBAgent(PageAgentContext& context)
    : InspectorAgentBase("IndexedDB"_s, context)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_backendDispatcher(Inspector::IndexedDBBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
{
}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent() = default;

void InspectorIndexedDBAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorIndexedDBAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString ignored;
    disable(ignored);
}

void InspectorIndexedDBAgent::enable(ErrorString&)
{
}

void InspectorIndexedDBAgent::disable(ErrorString&)
{
}

static ErrorStringOr<Document*> documentFromFrame(Frame* frame)
{
    Document* document = frame ? frame->document() : nullptr;
    if (!document)
        return makeUnexpected("Missing document for given frame"_s);
    
    return document;
}

static ErrorStringOr<IDBFactory*> IDBFactoryFromDocument(Document* document)
{
    DOMWindow* domWindow = document->domWindow();
    if (!domWindow)
        return makeUnexpected("Missing window for given document"_s);

    IDBFactory* idbFactory = DOMWindowIndexedDatabase::indexedDB(*domWindow);
    if (!idbFactory)
        makeUnexpected("Missing IndexedDB factory of window for given document"_s);
    
    return idbFactory;
}

static bool getDocumentAndIDBFactoryFromFrameOrSendFailure(Frame* frame, Document*& out_document, IDBFactory*& out_idbFactory, BackendDispatcher::CallbackBase& callback)
{
    ErrorStringOr<Document*> document = documentFromFrame(frame);
    if (!document.has_value()) {
        callback.sendFailure(document.error());
        return false;
    }

    ErrorStringOr<IDBFactory*> idbFactory = IDBFactoryFromDocument(document.value());
    if (!idbFactory.has_value()) {
        callback.sendFailure(idbFactory.error());
        return false;
    }
    
    out_document = document.value();
    out_idbFactory = idbFactory.value();
    return true;
}
    
void InspectorIndexedDBAgent::requestDatabaseNames(const String& securityOrigin, Ref<RequestDatabaseNamesCallback>&& callback)
{
    auto* frame = InspectorPageAgent::findFrameWithSecurityOrigin(m_inspectedPage, securityOrigin);
    Document* document;
    IDBFactory* idbFactory;
    if (!getDocumentAndIDBFactoryFromFrameOrSendFailure(frame, document, idbFactory, callback))
        return;

    auto& openingOrigin = document->securityOrigin();
    auto& topOrigin = document->topOrigin();
    idbFactory->getAllDatabaseNames(topOrigin, openingOrigin, [callback = WTFMove(callback)](auto& databaseNames) {
        if (!callback->isActive())
            return;

        Ref<JSON::ArrayOf<String>> databaseNameArray = JSON::ArrayOf<String>::create();
        for (auto& databaseName : databaseNames)
            databaseNameArray->addItem(databaseName);

        callback->sendSuccess(WTFMove(databaseNameArray));
    });
}

void InspectorIndexedDBAgent::requestDatabase(const String& securityOrigin, const String& databaseName, Ref<RequestDatabaseCallback>&& callback)
{
    auto* frame = InspectorPageAgent::findFrameWithSecurityOrigin(m_inspectedPage, securityOrigin);
    Document* document;
    IDBFactory* idbFactory;
    if (!getDocumentAndIDBFactoryFromFrameOrSendFailure(frame, document, idbFactory, callback))
        return;

    Ref<DatabaseLoader> databaseLoader = DatabaseLoader::create(document, WTFMove(callback));
    databaseLoader->start(idbFactory, &document->securityOrigin(), databaseName);
}

void InspectorIndexedDBAgent::requestData(const String& securityOrigin, const String& databaseName, const String& objectStoreName, const String& indexName, int skipCount, int pageSize, const JSON::Object* keyRange, Ref<RequestDataCallback>&& callback)
{
    auto* frame = InspectorPageAgent::findFrameWithSecurityOrigin(m_inspectedPage, securityOrigin);
    Document* document;
    IDBFactory* idbFactory;
    if (!getDocumentAndIDBFactoryFromFrameOrSendFailure(frame, document, idbFactory, callback))
        return;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(mainWorldExecState(frame));
    RefPtr<IDBKeyRange> idbKeyRange = keyRange ? idbKeyRangeFromKeyRange(keyRange) : nullptr;
    if (keyRange && !idbKeyRange) {
        callback->sendFailure("Could not parse key range."_s);
        return;
    }

    Ref<DataLoader> dataLoader = DataLoader::create(document, WTFMove(callback), injectedScript, objectStoreName, indexName, WTFMove(idbKeyRange), skipCount, pageSize);
    dataLoader->start(idbFactory, &document->securityOrigin(), databaseName);
}

namespace {

class ClearObjectStoreListener final : public EventListener {
    WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);
public:
    static Ref<ClearObjectStoreListener> create(Ref<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(*new ClearObjectStoreListener(WTFMove(requestCallback)));
    }

    ~ClearObjectStoreListener() override = default;

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext&, Event& event) override
    {
        if (!m_requestCallback->isActive())
            return;
        if (event.type() != eventNames().completeEvent) {
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

class ClearObjectStore final : public ExecutableWithDatabase {
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

        auto idbTransaction = transactionForDatabase(&database, m_objectStoreName, IDBTransactionMode::Readwrite);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }

        auto idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        TransactionActivator activator(idbTransaction.get());
        RefPtr<IDBRequest> idbRequest;
        if (auto* exec = context() ? context()->execState() : nullptr) {
            auto result = idbObjectStore->clear(*exec);
            ASSERT(!result.hasException());
            if (result.hasException()) {
                m_requestCallback->sendFailure(makeString("Could not clear object store '", m_objectStoreName, "': ", static_cast<int>(result.releaseException().code())));
                return;
            }
            idbRequest = result.releaseReturnValue();
        }

        idbTransaction->addEventListener(eventNames().completeEvent, ClearObjectStoreListener::create(m_requestCallback.copyRef()), false);
    }

    RequestCallback& requestCallback() override { return m_requestCallback.get(); }
private:
    const String m_objectStoreName;
    Ref<ClearObjectStoreCallback> m_requestCallback;
};

} // anonymous namespace

void InspectorIndexedDBAgent::clearObjectStore(const String& securityOrigin, const String& databaseName, const String& objectStoreName, Ref<ClearObjectStoreCallback>&& callback)
{
    auto* frame = InspectorPageAgent::findFrameWithSecurityOrigin(m_inspectedPage, securityOrigin);
    Document* document;
    IDBFactory* idbFactory;
    if (!getDocumentAndIDBFactoryFromFrameOrSendFailure(frame, document, idbFactory, callback))
        return;

    Ref<ClearObjectStore> clearObjectStore = ClearObjectStore::create(document, objectStoreName, WTFMove(callback));
    clearObjectStore->start(idbFactory, &document->securityOrigin(), databaseName);
}

} // namespace WebCore
#endif // ENABLE(INDEXED_DATABASE)
