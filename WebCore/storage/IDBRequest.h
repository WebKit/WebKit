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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "IDBAny.h"
#include "IDBCallbacks.h"
#include "Timer.h"
#include <wtf/Vector.h>

namespace WebCore {

class IDBTransactionBackendInterface;

class IDBRequest : public IDBCallbacks, public EventTarget, public ActiveDOMObject {
public:
    static PassRefPtr<IDBRequest> create(ScriptExecutionContext* context, PassRefPtr<IDBAny> source, IDBTransactionBackendInterface* transaction = 0) { return adoptRef(new IDBRequest(context, source, transaction)); }
    virtual ~IDBRequest();

    // Defined in the IDL
    enum ReadyState {
        LOADING = 1,
        DONE = 2
    };
    unsigned short readyState() const { return m_readyState; }
    DEFINE_ATTRIBUTE_EVENT_LISTENER(success);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    bool resetReadyState();

    // IDBCallbacks
    virtual void onError(PassRefPtr<IDBDatabaseError>);
    virtual void onSuccess(); // For "null".
    virtual void onSuccess(PassRefPtr<IDBDatabaseBackendInterface>);
    virtual void onSuccess(PassRefPtr<IDBCursorBackendInterface>);
    virtual void onSuccess(PassRefPtr<IDBIndexBackendInterface>);
    virtual void onSuccess(PassRefPtr<IDBKey>);
    virtual void onSuccess(PassRefPtr<IDBObjectStoreBackendInterface>);
    virtual void onSuccess(PassRefPtr<SerializedScriptValue>);

    // EventTarget
    virtual IDBRequest* toIDBRequest() { return this; }

    // ActiveDOMObject
    virtual ScriptExecutionContext* scriptExecutionContext() const;
    virtual bool canSuspend() const;

    using RefCounted<IDBCallbacks>::ref;
    using RefCounted<IDBCallbacks>::deref;

private:
    IDBRequest(ScriptExecutionContext*, PassRefPtr<IDBAny> source, IDBTransactionBackendInterface* transaction);

    void timerFired(Timer<IDBRequest>*);
    void scheduleEvent(PassRefPtr<IDBAny> result, PassRefPtr<IDBDatabaseError>);

    // EventTarget
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

    RefPtr<IDBAny> m_source;
    RefPtr<IDBTransactionBackendInterface> m_transaction;

    struct PendingEvent {
        RefPtr<IDBAny> m_result;
        RefPtr<IDBDatabaseError> m_error;
    };
    Vector<PendingEvent> m_pendingEvents;

    // Used to fire events asynchronously.
    Timer<IDBRequest> m_timer;
    RefPtr<IDBRequest> m_selfRef; // This is set to us iff there's an event pending.

    ReadyState m_readyState;
    EventTargetData m_eventTargetData;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBRequest_h
