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

#ifndef IDBDatabase_h
#define IDBDatabase_h

#include "ActiveDOMObject.h"
#include "DOMStringList.h"
#include "Dictionary.h"
#include "Event.h"
#include "EventTarget.h"
#include "IDBDatabaseBackendInterface.h"
#include "IDBDatabaseCallbacksImpl.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBVersionChangeRequest;
class ScriptExecutionContext;

typedef int ExceptionCode;

class IDBDatabase : public RefCounted<IDBDatabase>, public EventTarget, public ActiveDOMObject {
public:
    static PassRefPtr<IDBDatabase> create(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendInterface>);
    ~IDBDatabase();

    void setVersionChangeTransaction(IDBTransaction*);
    void clearVersionChangeTransaction(IDBTransaction*);

    // Implement the IDL
    String name() const { return m_backend->name(); }
    String version() const { return m_backend->version(); }
    PassRefPtr<DOMStringList> objectStoreNames() const { return m_backend->objectStoreNames(); }

    // FIXME: Try to modify the code generator so this is unneeded.
    PassRefPtr<IDBObjectStore> createObjectStore(const String& name, ExceptionCode& ec) { return createObjectStore(name, Dictionary(), ec); }
    PassRefPtr<IDBObjectStore> createObjectStore(const String& name, const Dictionary&, ExceptionCode&);
    PassRefPtr<IDBTransaction> transaction(ScriptExecutionContext*, PassRefPtr<DOMStringList>, const String& mode, ExceptionCode&);
    PassRefPtr<IDBTransaction> transaction(ScriptExecutionContext*, const String&, const String& mode, ExceptionCode&);
    PassRefPtr<IDBTransaction> transaction(ScriptExecutionContext*, PassRefPtr<DOMStringList>, unsigned short mode, ExceptionCode&);
    PassRefPtr<IDBTransaction> transaction(ScriptExecutionContext*, const String&, unsigned short mode, ExceptionCode&);
    void deleteObjectStore(const String& name, ExceptionCode&);
    PassRefPtr<IDBVersionChangeRequest> setVersion(ScriptExecutionContext*, const String& version, ExceptionCode&);
    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(versionchange);

    // IDBDatabaseCallbacks
    virtual void onVersionChange(const String& requestedVersion);

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const;
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    void open();
    void enqueueEvent(PassRefPtr<Event>);
    bool dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec) { return EventTarget::dispatchEvent(event, ec); }
    virtual bool dispatchEvent(PassRefPtr<Event>);

    using RefCounted<IDBDatabase>::ref;
    using RefCounted<IDBDatabase>::deref;

private:
    IDBDatabase(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendInterface>);

    // EventTarget
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

    RefPtr<IDBDatabaseBackendInterface> m_backend;
    RefPtr<IDBTransaction> m_versionChangeTransaction;

    bool m_closePending;
    bool m_contextStopped;

    EventTargetData m_eventTargetData;

    // Keep track of the versionchange events waiting to be fired on this
    // database so that we can cancel them if the database closes.
    Vector<RefPtr<Event> > m_enqueuedEvents;

    RefPtr<IDBDatabaseCallbacksImpl> m_databaseCallbacks;
};

} // namespace WebCore

#endif

#endif // IDBDatabase_h
