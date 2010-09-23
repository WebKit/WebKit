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

#ifndef IDBTransactionBackendImpl_h
#define IDBTransactionBackendImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBTransactionBackendInterface.h"
#include "IDBTransactionCallbacks.h"
#include "SQLiteTransaction.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBDatabaseBackendImpl;

class IDBTransactionBackendImpl : public IDBTransactionBackendInterface {
public:
    static PassRefPtr<IDBTransactionBackendImpl> create(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, int id, IDBDatabaseBackendImpl*);
    virtual ~IDBTransactionBackendImpl() { abort(); }

    virtual PassRefPtr<IDBObjectStoreBackendInterface> objectStore(const String& name);
    virtual unsigned short mode() const { return m_mode; }
    virtual bool scheduleTask(PassOwnPtr<ScriptExecutionContext::Task>);
    virtual void didCompleteTaskEvents();
    virtual void abort();
    virtual int id() const { return m_id; }
    virtual void setCallbacks(IDBTransactionCallbacks* callbacks) { m_callbacks = callbacks; }

    void run();
    bool isFinished() const { return m_state == Finished; }

private:
    IDBTransactionBackendImpl(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, int id, IDBDatabaseBackendImpl*);

    enum State {
        NotStarted,
        Started,
        Finished,
    };

    void start();
    void commit();

    void timerFired(Timer<IDBTransactionBackendImpl>*);

    RefPtr<DOMStringList> m_objectStoreNames;
    unsigned short m_mode;
    unsigned long m_timeout;
    int m_id;

    State m_state;
    RefPtr<IDBTransactionCallbacks> m_callbacks;
    RefPtr<IDBDatabaseBackendImpl> m_database;

    typedef Deque<OwnPtr<ScriptExecutionContext::Task> > TaskQueue;
    TaskQueue m_taskQueue;

    OwnPtr<SQLiteTransaction> m_transaction;

    // FIXME: delete the timer once we have threads instead.
    Timer<IDBTransactionBackendImpl> m_timer;
    int m_pendingEvents;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBTransactionBackendImpl_h
