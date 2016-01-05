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

#ifndef IDBDatabaseImpl_h
#define IDBDatabaseImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBDatabase.h"
#include "IDBDatabaseInfo.h"

namespace WebCore {

class IDBResultData;
class IDBTransaction;
class IDBTransactionInfo;

namespace IDBClient {

class IDBTransaction;

class IDBDatabase : public WebCore::IDBDatabase {
public:
    static Ref<IDBDatabase> create(ScriptExecutionContext&, IDBConnectionToServer&, const IDBResultData&);

    virtual ~IDBDatabase();

    // IDBDatabase IDL
    virtual const String name() const override final;
    virtual uint64_t version() const override final;
    virtual RefPtr<DOMStringList> objectStoreNames() const override final;

    virtual RefPtr<WebCore::IDBObjectStore> createObjectStore(const String& name, const Dictionary&, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBObjectStore> createObjectStore(const String& name, const IDBKeyPath&, bool autoIncrement, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBTransaction> transaction(ScriptExecutionContext*, const Vector<String>&, const String& mode, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBTransaction> transaction(ScriptExecutionContext*, const String&, const String& mode, ExceptionCodeWithMessage&) override final;
    virtual void deleteObjectStore(const String& name, ExceptionCodeWithMessage&) override final;
    virtual void close() override final;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override final { return IDBDatabaseEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    virtual const char* activeDOMObjectName() const override final;
    virtual bool canSuspendForDocumentSuspension() const override final;
    virtual void stop() override final;

    const IDBDatabaseInfo& info() const { return m_info; }
    uint64_t databaseConnectionIdentifier() const { return m_databaseConnectionIdentifier; }

    Ref<IDBTransaction> startVersionChangeTransaction(const IDBTransactionInfo&, IDBOpenDBRequest&);
    void didStartTransaction(IDBTransaction&);

    void willCommitTransaction(IDBTransaction&);
    void didCommitTransaction(IDBTransaction&);
    void willAbortTransaction(IDBTransaction&);
    void didAbortTransaction(IDBTransaction&);

    void fireVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);

    IDBConnectionToServer& serverConnection() { return m_serverConnection.get(); }

    void didCreateIndexInfo(const IDBIndexInfo&);
    void didDeleteIndexInfo(const IDBIndexInfo&);

    bool isClosingOrClosed() const { return m_closePending || m_closedInServer; }

    bool dispatchEvent(Event&) override final;

private:
    IDBDatabase(ScriptExecutionContext&, IDBConnectionToServer&, const IDBResultData&);

    void didCommitOrAbortTransaction(IDBTransaction&);

    void maybeCloseInServer();

    virtual bool hasPendingActivity() const override final;

    Ref<IDBConnectionToServer> m_serverConnection;
    IDBDatabaseInfo m_info;
    uint64_t m_databaseConnectionIdentifier { 0 };

    bool m_closePending { false };
    bool m_closedInServer { false };

    RefPtr<IDBTransaction> m_versionChangeTransaction;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_activeTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_committingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_abortingTransactions;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBDatabaseImpl_h
