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

#ifndef IDBRequestImpl_h
#define IDBRequestImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBAnyImpl.h"
#include "IDBOpenDBRequest.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransactionImpl.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Event;
class IDBKeyData;
class IDBResultData;
class ThreadSafeDataBuffer;

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBClient {

class IDBConnectionToServer;

class IDBRequest : public WebCore::IDBOpenDBRequest, public RefCounted<IDBRequest> {
public:
    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    static Ref<IDBRequest> create(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    static Ref<IDBRequest> createCount(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    static Ref<IDBRequest> createGet(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    const IDBResourceIdentifier& resourceIdentifier() const { return m_resourceIdentifier; }

    virtual ~IDBRequest() override;

    virtual RefPtr<WebCore::IDBAny> result(ExceptionCodeWithMessage&) const override;
    virtual unsigned short errorCode(ExceptionCode&) const override;
    virtual RefPtr<DOMError> error(ExceptionCodeWithMessage&) const override;
    virtual RefPtr<WebCore::IDBAny> source() const override;
    virtual RefPtr<WebCore::IDBTransaction> transaction() const override;
    virtual const String& readyState() const override;

    uint64_t sourceObjectStoreIdentifier() const;
    uint64_t sourceIndexIdentifier() const;
    IndexedDB::IndexRecordType requestedIndexRecordType() const;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override;
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<IDBRequest>::ref;
    using RefCounted<IDBRequest>::deref;

    void enqueueEvent(Ref<Event>&&);
    virtual bool dispatchEvent(Event&) override final;

    IDBConnectionToServer& connection() { return m_connection; }

    void requestCompleted(const IDBResultData&);

    void setResult(const IDBKeyData*);
    void setResult(uint64_t);
    void setResultToStructuredClone(const ThreadSafeDataBuffer&);
    void setResultToUndefined();

    IDBAny* modernResult() { return m_result.get(); }

    void willIterateCursor(IDBCursor&);
    void didOpenOrIterateCursor(const IDBResultData&);

    const IDBCursor* pendingCursor() const { return m_pendingCursor.get(); }

    void setSource(IDBCursor&);
    void setVersionChangeTransaction(IDBTransaction&);

protected:
    IDBRequest(IDBConnectionToServer&, ScriptExecutionContext*);
    IDBRequest(ScriptExecutionContext&, IDBObjectStore&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBCursor&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IDBTransaction&);
    IDBRequest(ScriptExecutionContext&, IDBIndex&, IndexedDB::IndexRecordType, IDBTransaction&);

    // ActiveDOMObject.
    virtual const char* activeDOMObjectName() const override final;
    virtual bool canSuspendForDocumentSuspension() const override final;
    virtual bool hasPendingActivity() const override final;
    virtual void stop() override final;

    // EventTarget.
    virtual void refEventTarget() override final { RefCounted<IDBRequest>::ref(); }
    virtual void derefEventTarget() override final { RefCounted<IDBRequest>::deref(); }
    virtual void uncaughtExceptionInEventHandler() override final;

    virtual bool isOpenDBRequest() const { return false; }

    IDBRequestReadyState m_readyState { IDBRequestReadyState::Pending };
    RefPtr<IDBAny> m_result;
    RefPtr<IDBTransaction> m_transaction;
    bool m_shouldExposeTransactionToDOM { true };
    RefPtr<DOMError> m_domError;
    IDBError m_idbError;

private:
    void onError();
    void onSuccess();

    IDBCursor* resultCursor();

    IDBConnectionToServer& m_connection;
    IDBResourceIdentifier m_resourceIdentifier;
    RefPtr<IDBAny> m_source;
    bool m_hasPendingActivity { true };
    IndexedDB::IndexRecordType m_requestedIndexRecordType;

    RefPtr<IDBCursor> m_pendingCursor;

    bool m_contextStopped { false };
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBRequestImpl_h
