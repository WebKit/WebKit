/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "IDBAny.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransaction.h"
#include "ScopeGuard.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Event;
class IDBAny;
class IDBCursor;
class IDBIndex;
class IDBKeyData;
class IDBObjectStore;
class IDBResultData;
class IDBValue;
class ThreadSafeDataBuffer;

namespace IDBClient {
class IDBConnectionToServer;
}

namespace IndexedDB {
enum class IndexRecordType;
}

// Defined in the IDL
enum class IDBRequestReadyState {
    Pending = 1,
    Done = 2,
};

class IDBRequest : public EventTargetWithInlineData, public ActiveDOMObject, public RefCounted<IDBRequest> {
public:
    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    static Ref<IDBRequest> createCount(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    static Ref<IDBRequest> createGet(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    const IDBResourceIdentifier& resourceIdentifier() const { return m_resourceIdentifier; }

    virtual ~IDBRequest();

    RefPtr<IDBAny> result(ExceptionCodeWithMessage&) const;
    unsigned short errorCode(ExceptionCode&) const;
    RefPtr<DOMError> error(ExceptionCodeWithMessage&) const;
    RefPtr<IDBAny> source() const;
    RefPtr<IDBTransaction> transaction() const;
    const String& readyState() const;

    uint64_t sourceObjectStoreIdentifier() const;
    uint64_t sourceIndexIdentifier() const;
    IndexedDB::IndexRecordType requestedIndexRecordType() const;

    // EventTarget
    EventTargetInterface eventTargetInterface() const override;
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<IDBRequest>::ref;
    using RefCounted<IDBRequest>::deref;

    void enqueueEvent(Ref<Event>&&);
    bool dispatchEvent(Event&) override;

    IDBClient::IDBConnectionToServer& connection() { return m_connection; }

    void requestCompleted(const IDBResultData&);

    void setResult(const IDBKeyData*);
    void setResult(uint64_t);
    void setResultToStructuredClone(const IDBValue&);
    void setResultToUndefined();

    IDBAny* modernResult() { return m_result.get(); }

    void willIterateCursor(IDBCursor&);
    void didOpenOrIterateCursor(const IDBResultData&);

    const IDBCursor* pendingCursor() const { return m_pendingCursor.get(); }

    void setSource(IDBCursor&);
    void setVersionChangeTransaction(IDBTransaction&);

    IndexedDB::RequestType requestType() const { return m_requestType; }

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    bool hasPendingActivity() const final;
    void stop() final;

protected:
    IDBRequest(IDBClient::IDBConnectionToServer&, ScriptExecutionContext&);
    IDBRequest(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    // EventTarget.
    void refEventTarget() final { RefCounted<IDBRequest>::ref(); }
    void derefEventTarget() final { RefCounted<IDBRequest>::deref(); }
    void uncaughtExceptionInEventHandler() final;

    virtual bool isOpenDBRequest() const { return false; }

    IDBRequestReadyState m_readyState { IDBRequestReadyState::Pending };
    RefPtr<IDBAny> m_result;
    RefPtr<IDBTransaction> m_transaction;
    bool m_shouldExposeTransactionToDOM { true };
    RefPtr<DOMError> m_domError;
    IDBError m_idbError;
    IndexedDB::RequestType m_requestType = { IndexedDB::RequestType::Other };
    bool m_contextStopped { false };

    Event* m_openDatabaseSuccessEvent { nullptr };

private:
    void onError();
    void onSuccess();

    IDBCursor* resultCursor();

    IDBClient::IDBConnectionToServer& m_connection;
    IDBResourceIdentifier m_resourceIdentifier;
    RefPtr<IDBAny> m_source;
    bool m_hasPendingActivity { true };
    IndexedDB::IndexRecordType m_requestedIndexRecordType;

    RefPtr<IDBCursor> m_pendingCursor;

    std::unique_ptr<ScopeGuard> m_cursorRequestNotifier;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
