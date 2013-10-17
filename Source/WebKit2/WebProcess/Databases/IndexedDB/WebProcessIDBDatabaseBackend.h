/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WebProcessIDBDatabaseBackend_h
#define WebProcessIDBDatabaseBackend_h

#include "MessageSender.h"
#include <WebCore/IDBDatabaseBackendInterface.h>

#if ENABLE(INDEXED_DATABASE)
#if ENABLE(DATABASE_PROCESS)

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class WebProcessIDBDatabaseBackend : public WebCore::IDBDatabaseBackendInterface, public CoreIPC::MessageSender {
public:
    static PassRefPtr<WebProcessIDBDatabaseBackend> create(const String& name, WebCore::SecurityOrigin* origin) { return adoptRef(new WebProcessIDBDatabaseBackend(name, origin)); }

    virtual ~WebProcessIDBDatabaseBackend();

    virtual void createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const WebCore::IDBKeyPath&, bool autoIncrement);
    virtual void deleteObjectStore(int64_t transactionId, int64_t objectStoreId);
    virtual void createTransaction(int64_t transactionId, PassRefPtr<WebCore::IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, unsigned short mode);
    virtual void close(PassRefPtr<WebCore::IDBDatabaseCallbacks>);

    // Transaction-specific operations.
    virtual void commit(int64_t transactionId);
    virtual void abort(int64_t transactionId);
    virtual void abort(int64_t transactionId, PassRefPtr<WebCore::IDBDatabaseError>);

    virtual void createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const WebCore::IDBKeyPath&, bool unique, bool multiEntry);
    virtual void deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId);

    virtual void get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<WebCore::IDBKeyRange>, bool keyOnly, PassRefPtr<WebCore::IDBCallbacks>);
    // Note that 'value' may be consumed/adopted by this call.
    virtual void put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<WebCore::SharedBuffer> value, PassRefPtr<WebCore::IDBKey>, PutMode, PassRefPtr<WebCore::IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&);
    virtual void setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<WebCore::IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&);
    virtual void setIndexesReady(int64_t transactionId, int64_t objectStoreId, const Vector<int64_t>& indexIds);
    virtual void openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<WebCore::IDBKeyRange>, WebCore::IndexedDB::CursorDirection, bool keyOnly, TaskType, PassRefPtr<WebCore::IDBCallbacks>);
    virtual void count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<WebCore::IDBKeyRange>, PassRefPtr<WebCore::IDBCallbacks>);
    virtual void deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<WebCore::IDBKeyRange>, PassRefPtr<WebCore::IDBCallbacks>);
    virtual void clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<WebCore::IDBCallbacks>);

    void openConnection(PassRefPtr<WebCore::IDBCallbacks>, PassRefPtr<WebCore::IDBDatabaseCallbacks>, int64_t transactionId, int64_t version);

    void establishDatabaseProcessBackend();

private:
    WebProcessIDBDatabaseBackend(const String& name, WebCore::SecurityOrigin*);

    String m_databaseName;
    RefPtr<WebCore::SecurityOrigin> m_securityOrigin;

    // CoreIPC::MessageSender
    virtual CoreIPC::Connection* messageSenderConnection() OVERRIDE;
    virtual uint64_t messageSenderDestinationID() OVERRIDE { return m_backendIdentifier; }

    uint64_t m_backendIdentifier;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
#endif // ENABLE(INDEXED_DATABASE)
#endif // WebProcessIDBDatabaseBackend_h
