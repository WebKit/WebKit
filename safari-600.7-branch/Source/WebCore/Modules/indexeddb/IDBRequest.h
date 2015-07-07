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

#ifndef IDBRequest_h
#define IDBRequest_h

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "DOMError.h"
#include "DOMRequestState.h"
#include "DOMStringList.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "IDBAny.h"
#include "IDBCallbacks.h"
#include "IDBCursor.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "ScriptWrappable.h"

namespace WebCore {

class IDBTransaction;

typedef int ExceptionCode;

class IDBRequest : public ScriptWrappable, public IDBCallbacks, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static PassRefPtr<IDBRequest> create(ScriptExecutionContext*, PassRefPtr<IDBAny> source, IDBTransaction*);
    static PassRefPtr<IDBRequest> create(ScriptExecutionContext*, PassRefPtr<IDBAny> source, IDBDatabaseBackend::TaskType, IDBTransaction*);
    virtual ~IDBRequest();

    PassRefPtr<IDBAny> result(ExceptionCode&) const;
    unsigned short errorCode(ExceptionCode&) const;
    PassRefPtr<DOMError> error(ExceptionCode&) const;
    PassRefPtr<IDBAny> source() const;
    PassRefPtr<IDBTransaction> transaction() const;
    void preventPropagation() { m_preventPropagation = true; }

    // Defined in the IDL
    enum ReadyState {
        PENDING = 1,
        DONE = 2,
        EarlyDeath = 3
    };

    const String& readyState() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(success);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void markEarlyDeath();
    void setCursorDetails(IndexedDB::CursorType, IndexedDB::CursorDirection);
    void setPendingCursor(PassRefPtr<IDBCursor>);
    void finishCursor();
    void abort();

    // IDBCallbacks
    virtual void onError(PassRefPtr<IDBDatabaseError>);
    virtual void onSuccess(PassRefPtr<DOMStringList>);
    virtual void onSuccess(PassRefPtr<IDBCursorBackend>);
    virtual void onSuccess(PassRefPtr<IDBKey>);
    virtual void onSuccess(PassRefPtr<SharedBuffer>);
    virtual void onSuccess(PassRefPtr<SharedBuffer>, PassRefPtr<IDBKey>, const IDBKeyPath&);
    virtual void onSuccess(int64_t);
    virtual void onSuccess();
    virtual void onSuccess(PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer>);

    // ActiveDOMObject
    virtual bool hasPendingActivity() const override;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override;
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }
    virtual void uncaughtExceptionInEventHandler() override final;

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>) override;

    void transactionDidFinishAndDispatch();

    using RefCounted<IDBCallbacks>::ref;
    using RefCounted<IDBCallbacks>::deref;

    IDBDatabaseBackend::TaskType taskType() { return m_taskType; }

    DOMRequestState* requestState() { return &m_requestState; }

protected:
    IDBRequest(ScriptExecutionContext*, PassRefPtr<IDBAny> source, IDBDatabaseBackend::TaskType, IDBTransaction*);
    void enqueueEvent(PassRefPtr<Event>);
    virtual bool shouldEnqueueEvent() const;
    void onSuccessInternal(PassRefPtr<SerializedScriptValue>);
    void onSuccessInternal(const Deprecated::ScriptValue&);

    RefPtr<IDBAny> m_result;
    unsigned short m_errorCode;
    String m_errorMessage;
    RefPtr<DOMError> m_error;
    bool m_contextStopped;
    RefPtr<IDBTransaction> m_transaction;
    ReadyState m_readyState;
    bool m_requestAborted; // May be aborted by transaction then receive async onsuccess; ignore vs. assert.

private:
    // ActiveDOMObject
    virtual void stop() override;

    // EventTarget
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    PassRefPtr<IDBCursor> getResultCursor();
    void setResultCursor(PassRefPtr<IDBCursor>, PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, const Deprecated::ScriptValue&);

    RefPtr<IDBAny> m_source;
    const IDBDatabaseBackend::TaskType m_taskType;

    bool m_hasPendingActivity;
    Vector<RefPtr<Event>> m_enqueuedEvents;

    // Only used if the result type will be a cursor.
    IndexedDB::CursorType m_cursorType;
    IndexedDB::CursorDirection m_cursorDirection;
    bool m_cursorFinished;
    RefPtr<IDBCursor> m_pendingCursor;
    RefPtr<IDBKey> m_cursorKey;
    RefPtr<IDBKey> m_cursorPrimaryKey;
    Deprecated::ScriptValue m_cursorValue;
    bool m_didFireUpgradeNeededEvent;
    bool m_preventPropagation;

    DOMRequestState m_requestState;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBRequest_h
