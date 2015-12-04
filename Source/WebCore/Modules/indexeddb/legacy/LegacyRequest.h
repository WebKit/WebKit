/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#ifndef LegacyRequest_h
#define LegacyRequest_h

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "DOMError.h"
#include "DOMRequestState.h"
#include "DOMStringList.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "IDBCallbacks.h"
#include "IDBCursor.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBOpenDBRequest.h"
#include "LegacyAny.h"
#include "ScriptWrappable.h"

namespace WebCore {

class LegacyTransaction;

typedef int ExceptionCode;

class LegacyRequest : public IDBOpenDBRequest, public IDBCallbacks {
public:
    static Ref<LegacyRequest> create(ScriptExecutionContext*, PassRefPtr<LegacyAny> source, LegacyTransaction*);
    static Ref<LegacyRequest> create(ScriptExecutionContext*, PassRefPtr<LegacyAny> source, IDBDatabaseBackend::TaskType, LegacyTransaction*);
    virtual ~LegacyRequest();

    virtual RefPtr<IDBAny> result(ExceptionCodeWithMessage&) const override final;
    PassRefPtr<LegacyAny> legacyResult(ExceptionCode&);
    virtual unsigned short errorCode(ExceptionCode&) const override final;
    virtual RefPtr<DOMError> error(ExceptionCodeWithMessage&) const override final;
    virtual RefPtr<IDBAny> source() const override final;
    virtual RefPtr<IDBTransaction> transaction() const override final;
    virtual const String& readyState() const override final;

    void preventPropagation() { m_preventPropagation = true; }

    void markEarlyDeath();
    void setCursorDetails(IndexedDB::CursorType, IndexedDB::CursorDirection);
    void setPendingCursor(PassRefPtr<LegacyCursor>);
    void finishCursor();
    void abort();

    // IDBCallbacks
    virtual void onError(PassRefPtr<IDBDatabaseError>) override final;
    virtual void onSuccess(PassRefPtr<DOMStringList>) override final;
    virtual void onSuccess(PassRefPtr<IDBCursorBackend>) override final;
    virtual void onSuccess(PassRefPtr<IDBKey>) override final;
    virtual void onSuccess(PassRefPtr<SharedBuffer>) override final;
    virtual void onSuccess(PassRefPtr<SharedBuffer>, PassRefPtr<IDBKey>, const IDBKeyPath&) override final;
    virtual void onSuccess(int64_t) override final;
    virtual void onSuccess() override final;
    virtual void onSuccess(PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer>) override final;
    virtual void onSuccess(PassRefPtr<IDBDatabaseBackend>, const IDBDatabaseMetadata&) override;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override;
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }
    virtual void uncaughtExceptionInEventHandler() override final;

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(Event&) override;

    void transactionDidFinishAndDispatch();

    IDBDatabaseBackend::TaskType taskType() { return m_taskType; }

    DOMRequestState* requestState() { return &m_requestState; }

    // ActiveDOMObject API.
    bool hasPendingActivity() const override;

    using IDBCallbacks::ref;
    using IDBCallbacks::deref;

protected:
    LegacyRequest(ScriptExecutionContext*, PassRefPtr<LegacyAny> source, IDBDatabaseBackend::TaskType, LegacyTransaction*);
    void enqueueEvent(Ref<Event>&&);
    virtual bool shouldEnqueueEvent() const;
    void onSuccessInternal(PassRefPtr<SerializedScriptValue>);
    void onSuccessInternal(const Deprecated::ScriptValue&);

    RefPtr<LegacyAny> m_result;
    unsigned short m_errorCode;
    String m_errorMessage;
    RefPtr<DOMError> m_error;
    bool m_contextStopped;
    RefPtr<LegacyTransaction> m_transaction;
    IDBRequestReadyState m_readyState;
    bool m_requestAborted; // May be aborted by transaction then receive async onsuccess; ignore vs. assert.

private:
    // ActiveDOMObject API.
    void stop() override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;

    // EventTarget API.
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    RefPtr<LegacyCursor> getResultCursor();
    void setResultCursor(PassRefPtr<LegacyCursor>, PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, const Deprecated::ScriptValue&);

    RefPtr<LegacyAny> m_source;
    const IDBDatabaseBackend::TaskType m_taskType;

    bool m_hasPendingActivity;
    Vector<Ref<Event>> m_enqueuedEvents;

    // Only used if the result type will be a cursor.
    IndexedDB::CursorType m_cursorType;
    IndexedDB::CursorDirection m_cursorDirection;
    bool m_cursorFinished;
    RefPtr<LegacyCursor> m_pendingCursor;
    RefPtr<IDBKey> m_cursorKey;
    RefPtr<IDBKey> m_cursorPrimaryKey;
    Deprecated::ScriptValue m_cursorValue;
    bool m_didFireUpgradeNeededEvent;
    bool m_preventPropagation;

    DOMRequestState m_requestState;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // LegacyRequest_h
