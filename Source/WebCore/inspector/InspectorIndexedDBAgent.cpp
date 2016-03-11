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
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "Frame.h"
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

class GetDatabaseNamesCallback : public EventListener {
    WTF_MAKE_NONCOPYABLE(GetDatabaseNamesCallback);
public:
    static Ref<GetDatabaseNamesCallback> create(Ref<RequestDatabaseNamesCallback>&& requestCallback, const String& securityOrigin)
    {
        return adoptRef(*new GetDatabaseNamesCallback(WTFMove(requestCallback), securityOrigin));
    }

    virtual ~GetDatabaseNamesCallback() { }

    bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != eventNames().successEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        ExceptionCodeWithMessage ec;
        RefPtr<IDBAny> requestResult = idbRequest->result(ec);
        if (ec.code) {
            m_requestCallback->sendFailure("Could not get result in callback.");
            return;
        }
        if (requestResult->type() != IDBAny::Type::DOMStringList) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<DOMStringList> databaseNamesList = requestResult->domStringList();
        Ref<Inspector::Protocol::Array<String>> databaseNames = Inspector::Protocol::Array<String>::create();
        for (size_t i = 0; i < databaseNamesList->length(); ++i)
            databaseNames->addItem(databaseNamesList->item(i));
        m_requestCallback->sendSuccess(WTFMove(databaseNames));
    }

private:
    GetDatabaseNamesCallback(Ref<RequestDatabaseNamesCallback>&& requestCallback, const String& securityOrigin)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(WTFMove(requestCallback))
        , m_securityOrigin(securityOrigin) { }
    Ref<RequestDatabaseNamesCallback> m_requestCallback;
    String m_securityOrigin;
};

class ExecutableWithDatabase : public RefCounted<ExecutableWithDatabase> {
public:
    ExecutableWithDatabase(ScriptExecutionContext* context)
        : m_context(context) { }
    virtual ~ExecutableWithDatabase() { };
    void start(IDBFactory*, SecurityOrigin*, const String& databaseName);
    virtual void execute() = 0;
    virtual RequestCallback& requestCallback() = 0;
    ScriptExecutionContext* context() { return m_context; };
private:
    ScriptExecutionContext* m_context;
};

class OpenDatabaseCallback : public EventListener {
public:
    static Ref<OpenDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRef(*new OpenDatabaseCallback(executableWithDatabase));
    }

    virtual ~OpenDatabaseCallback() { }

    bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (event->type() != eventNames().successEvent) {
            m_executableWithDatabase->requestCallback().sendFailure("Unexpected event type.");
            return;
        }

        IDBOpenDBRequest* idbOpenDBRequest = static_cast<IDBOpenDBRequest*>(event->target());
        ExceptionCodeWithMessage ec;
        RefPtr<IDBAny> requestResult = idbOpenDBRequest->result(ec);
        if (ec.code) {
            m_executableWithDatabase->requestCallback().sendFailure("Could not get result in callback.");
            return;
        }
        if (requestResult->type() != IDBAny::Type::IDBDatabase) {
            m_executableWithDatabase->requestCallback().sendFailure("Unexpected result type.");
            return;
        }
        if (!requestResult->isLegacy()) {
            m_executableWithDatabase->requestCallback().sendFailure("Only Legacy IDB is supported right now.");
            return;
        }

        // FIXME (webkit.org/b/154686) - Reimplement this.
        m_executableWithDatabase->execute();
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactory* idbFactory, SecurityOrigin*, const String& databaseName)
{
    Ref<OpenDatabaseCallback> callback = OpenDatabaseCallback::create(this);
    ExceptionCode ec = 0;

    if (!context()) {
        requestCallback().sendFailure("Could not open database.");
        return;
    }

    RefPtr<IDBOpenDBRequest> idbOpenDBRequest = idbFactory->open(*context(), databaseName, ec);
    if (ec) {
        requestCallback().sendFailure("Could not open database.");
        return;
    }
    idbOpenDBRequest->addEventListener(eventNames().successEvent, WTFMove(callback), false);
}

class DatabaseLoader : public ExecutableWithDatabase {
public:
    static Ref<DatabaseLoader> create(ScriptExecutionContext* context, Ref<RequestDatabaseCallback>&& requestCallback)
    {
        return adoptRef(*new DatabaseLoader(context, WTFMove(requestCallback)));
    }

    virtual ~DatabaseLoader() { }

    void execute() override
    {
        if (!requestCallback().isActive())
            return;

        // FIXME (webkit.org/b/154686) - Reimplement this.
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
    RefPtr<IDBKey> idbKey;

    String type;
    if (!key->getString("type", type))
        return nullptr;

    static NeverDestroyed<const String> numberType(ASCIILiteral("number"));
    static NeverDestroyed<const String> stringType(ASCIILiteral("string"));
    static NeverDestroyed<const String> dateType(ASCIILiteral("date"));
    static NeverDestroyed<const String> arrayType(ASCIILiteral("array"));

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

    return idbKey.release();
}

static RefPtr<IDBKeyRange> idbKeyRangeFromKeyRange(const InspectorObject* keyRange)
{
    RefPtr<InspectorObject> lower;
    if (!keyRange->getObject("lower", lower))
        return nullptr;
    RefPtr<IDBKey> idbLower = idbKeyFromInspectorObject(lower.get());
    if (!idbLower)
        return nullptr;

    RefPtr<InspectorObject> upper;
    if (!keyRange->getObject("upper", upper))
        return nullptr;
    RefPtr<IDBKey> idbUpper = idbKeyFromInspectorObject(upper.get());
    if (!idbUpper)
        return nullptr;

    bool lowerOpen;
    if (!keyRange->getBoolean("lowerOpen", lowerOpen))
        return nullptr;
    IDBKeyRange::LowerBoundType lowerBoundType = lowerOpen ? IDBKeyRange::LowerBoundOpen : IDBKeyRange::LowerBoundClosed;

    bool upperOpen;
    if (!keyRange->getBoolean("upperOpen", upperOpen))
        return nullptr;
    IDBKeyRange::UpperBoundType upperBoundType = upperOpen ? IDBKeyRange::UpperBoundOpen : IDBKeyRange::UpperBoundClosed;

    return IDBKeyRange::create(WTFMove(idbLower), WTFMove(idbUpper), lowerBoundType, upperBoundType);
}

class DataLoader;

class OpenCursorCallback : public EventListener {
public:
    static Ref<OpenCursorCallback> create(InjectedScript injectedScript, Ref<RequestDataCallback>&& requestCallback, int skipCount, unsigned pageSize)
    {
        return adoptRef(*new OpenCursorCallback(injectedScript, WTFMove(requestCallback), skipCount, pageSize));
    }

    virtual ~OpenCursorCallback() { }

    bool operator==(const EventListener& other) override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext*, Event* event) override
    {
        if (event->type() != eventNames().successEvent) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        ExceptionCodeWithMessage ecwm;
        RefPtr<IDBAny> requestResult = idbRequest->result(ecwm);
        if (ecwm.code) {
            m_requestCallback->sendFailure("Could not get result in callback.");
            return;
        }
        if (requestResult->type() == IDBAny::Type::ScriptValue) {
            end(false);
            return;
        }
        if (requestResult->type() != IDBAny::Type::IDBCursorWithValue) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<IDBCursorWithValue> idbCursor = requestResult->idbCursorWithValue();

        if (m_skipCount) {
            ExceptionCodeWithMessage ec;
            idbCursor->advance(m_skipCount, ec);
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
        idbCursor->continueFunction(nullptr, ec);
        if (ec.code) {
            m_requestCallback->sendFailure("Could not continue cursor.");
            return;
        }

        RefPtr<DataEntry> dataEntry = DataEntry::create()
            .setKey(m_injectedScript.wrapObject(idbCursor->key(), String(), true))
            .setPrimaryKey(m_injectedScript.wrapObject(idbCursor->primaryKey(), String(), true))
            .setValue(m_injectedScript.wrapObject(idbCursor->value(), String(), true))
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
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
        , m_result(Array<DataEntry>::create())
    {
    }
    InjectedScript m_injectedScript;
    Ref<RequestDataCallback> m_requestCallback;
    int m_skipCount;
    unsigned m_pageSize;
    Ref<Array<DataEntry>> m_result;
};

class DataLoader : public ExecutableWithDatabase {
public:
    static Ref<DataLoader> create(ScriptExecutionContext* context, Ref<RequestDataCallback>&& requestCallback, const InjectedScript& injectedScript, const String& objectStoreName, const String& indexName, RefPtr<IDBKeyRange>&& idbKeyRange, int skipCount, unsigned pageSize)
    {
        return adoptRef(*new DataLoader(context, WTFMove(requestCallback), injectedScript, objectStoreName, indexName, WTFMove(idbKeyRange), skipCount, pageSize));
    }

    virtual ~DataLoader() { }

    void execute() override
    {
        if (!requestCallback().isActive())
            return;

        // FIXME (webkit.org/b/154686) - Reimplement this.
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

    IDBFactory* idbFactory = DOMWindowIndexedDatabase::indexedDB(domWindow);
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

    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    ExceptionCode ec = 0;
    RefPtr<IDBRequest> idbRequest = idbFactory->getDatabaseNames(*document, ec);
    if (!idbRequest || ec) {
        requestCallback->sendFailure("Could not obtain database names.");
        return;
    }

    idbRequest->addEventListener(eventNames().successEvent, GetDatabaseNamesCallback::create(WTFMove(requestCallback), document->securityOrigin()->toRawString()), false);
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

    bool operator==(const EventListener& other) override
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

    void execute() override
    {
        if (!requestCallback().isActive())
            return;

        // FIXME (webkit.org/b/154686) - Reimplement this.
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
