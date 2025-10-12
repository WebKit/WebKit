/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include <WebCore/EventNames.h>
#include <WebCore/EventTarget.h>
#include <WebCore/EventTargetInterfaces.h>
#include <WebCore/IDBActiveDOMObject.h>
#include <WebCore/IDBConnectionProxy.h>
#include <WebCore/IDBDatabaseConnectionIdentifier.h>
#include <WebCore/IDBDatabaseInfo.h>
#include <WebCore/IDBKeyPath.h>
#include <WebCore/IDBTransactionMode.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

class DOMStringList;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBResultData;
class IDBTransaction;
class IDBTransactionInfo;

struct EventNames;

class IDBDatabase final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<IDBDatabase>, public EventTarget, public IDBActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(IDBDatabase);
public:
    static Ref<IDBDatabase> create(ScriptExecutionContext&, IDBClient::IDBConnectionProxy&, const IDBResultData&);

    virtual ~IDBDatabase();

    // IDBDatabase IDL
    const String name() const;
    uint64_t version() const;
    Ref<DOMStringList> objectStoreNames() const;

    struct ObjectStoreParameters {
        std::optional<IDBKeyPath> keyPath;
        bool autoIncrement;
    };

    ExceptionOr<Ref<IDBObjectStore>> createObjectStore(const String& name, ObjectStoreParameters&&);

    using StringOrVectorOfStrings = Variant<String, Vector<String>>;
    struct TransactionOptions {
        std::optional<IDBTransactionDurability> durability;
    };
    ExceptionOr<Ref<IDBTransaction>> transaction(StringOrVectorOfStrings&& storeNames, IDBTransactionMode, TransactionOptions = { });
    ExceptionOr<void> deleteObjectStore(const String& name);
    void close();

    void renameObjectStore(IDBObjectStore&, const String& newName);
    void renameIndex(IDBIndex&, const String& newName);

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::IDBDatabase; }
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void derefEventTarget() final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    // ActiveDOMObject.
    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    IDBDatabaseInfo& info() { return m_info; }
    IDBDatabaseConnectionIdentifier databaseConnectionIdentifier() const { return m_databaseConnectionIdentifier; }
    std::optional<ScriptExecutionContextIdentifier> scriptExecutionContextIdentifier() const;

    Ref<IDBTransaction> startVersionChangeTransaction(const IDBTransactionInfo&, IDBOpenDBRequest&);
    void didStartTransaction(IDBTransaction&);

    void willCommitTransaction(IDBTransaction&);
    void didCommitTransaction(IDBTransaction&);
    void willAbortTransaction(IDBTransaction&);
    void didAbortTransaction(IDBTransaction&);

    void fireVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);
    void didCloseFromServer(const IDBError&);
    void connectionToServerLost(const IDBError&);

    IDBClient::IDBConnectionProxy& connectionProxy() { return m_connectionProxy.get(); }

    void didCreateIndexInfo(const IDBIndexInfo&);
    void didDeleteIndexInfo(const IDBIndexInfo&);

    bool isClosingOrClosed() const { return m_closePending || m_closedInServer; }

    void dispatchEvent(Event&) final;

    void setIsContextSuspended(bool isContextSuspended) { m_isContextSuspended = isContextSuspended; }
    bool isContextSuspended() const { return m_isContextSuspended; }

private:
    IDBDatabase(ScriptExecutionContext&, IDBClient::IDBConnectionProxy&, const IDBResultData&);

    void didCommitOrAbortTransaction(IDBTransaction&);

    // ActiveDOMObject.
    bool virtualHasPendingActivity() const final;
    void stop() final;

    void maybeCloseInServer();

    RefPtr<IDBTransaction> protectedVersionChangeTransaction() const;

    const Ref<IDBClient::IDBConnectionProxy> m_connectionProxy;
    IDBDatabaseInfo m_info;
    IDBDatabaseConnectionIdentifier m_databaseConnectionIdentifier;

    bool m_closePending { false };
    bool m_closedInServer { false };

    RefPtr<IDBTransaction> m_versionChangeTransaction;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_activeTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_committingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_abortingTransactions;
    
    const EventNames& m_eventNames; // Need to cache this so we can use it from GC threads.
    std::atomic<bool> m_isContextSuspended { false };
};

} // namespace WebCore
