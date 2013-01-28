/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "IDBBackingStore.h"
#include "IDBCursorBackendInterface.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseCallbacksProxy.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBFakeBackingStore.h"
#include "IDBTransactionBackendImpl.h"
#include "WebIDBDatabaseCallbacksImpl.h"
#include "WebIDBDatabaseImpl.h"

#include <gtest/gtest.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;
using WebKit::IDBDatabaseCallbacksProxy;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseImpl;
using WebKit::WebIDBDatabaseCallbacksImpl;

namespace {

TEST(IDBDatabaseBackendTest, BackingStoreRetention)
{
    RefPtr<IDBFakeBackingStore> backingStore = adoptRef(new IDBFakeBackingStore());
    EXPECT_TRUE(backingStore->hasOneRef());

    IDBFactoryBackendImpl* factory = 0;
    RefPtr<IDBDatabaseBackendImpl> db = IDBDatabaseBackendImpl::create("db", backingStore.get(), factory, "uniqueid");
    EXPECT_GT(backingStore->refCount(), 1);

    db.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
}

class MockIDBCallbacks : public IDBCallbacks {
public:
    static PassRefPtr<MockIDBCallbacks> create() { return adoptRef(new MockIDBCallbacks()); }
    virtual ~MockIDBCallbacks()
    {
        EXPECT_TRUE(m_wasSuccessDBCalled);
    }
    virtual void onError(PassRefPtr<IDBDatabaseError>) OVERRIDE { }
    virtual void onSuccess(PassRefPtr<DOMStringList>) OVERRIDE { }
    virtual void onSuccess(PassRefPtr<IDBCursorBackendInterface>, PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SerializedScriptValue>) OVERRIDE { }
    virtual void onSuccess(PassRefPtr<IDBDatabaseBackendInterface>) OVERRIDE
    {
        m_wasSuccessDBCalled = true;
    }
    virtual void onSuccess(PassRefPtr<IDBKey>) OVERRIDE { }
    virtual void onSuccess(PassRefPtr<SerializedScriptValue>) OVERRIDE { }
    virtual void onSuccess(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBKey>, const IDBKeyPath&) OVERRIDE { };
    virtual void onSuccess(int64_t) OVERRIDE { }
    virtual void onSuccess() OVERRIDE { }
    virtual void onSuccess(PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SerializedScriptValue>) OVERRIDE { };
    virtual void onSuccessWithPrefetch(const Vector<RefPtr<IDBKey> >&, const Vector<RefPtr<IDBKey> >&, const Vector<RefPtr<SerializedScriptValue> >&) OVERRIDE { }
    virtual void onBlocked() OVERRIDE { }
private:
    MockIDBCallbacks()
        : m_wasSuccessDBCalled(false) { }
    bool m_wasSuccessDBCalled;
};

class FakeIDBDatabaseCallbacks : public IDBDatabaseCallbacks {
public:
    static PassRefPtr<FakeIDBDatabaseCallbacks> create() { return adoptRef(new FakeIDBDatabaseCallbacks()); }
    virtual ~FakeIDBDatabaseCallbacks() { }
    virtual void onVersionChange(const String& version) OVERRIDE { }
    virtual void onVersionChange(int64_t oldVersion, int64_t newVersion) OVERRIDE { }
    virtual void onForcedClose() OVERRIDE { }
    virtual void onAbort(int64_t transactionId, PassRefPtr<IDBDatabaseError> error) OVERRIDE { }
    virtual void onComplete(int64_t transactionId) OVERRIDE { }
private:
    FakeIDBDatabaseCallbacks() { }
};

TEST(IDBDatabaseBackendTest, ConnectionLifecycle)
{
    RefPtr<IDBFakeBackingStore> backingStore = adoptRef(new IDBFakeBackingStore());
    EXPECT_TRUE(backingStore->hasOneRef());

    IDBFactoryBackendImpl* factory = 0;
    RefPtr<IDBDatabaseBackendImpl> db = IDBDatabaseBackendImpl::create("db", backingStore.get(), factory, "uniqueid");
    EXPECT_GT(backingStore->refCount(), 1);

    RefPtr<MockIDBCallbacks> request1 = MockIDBCallbacks::create();
    RefPtr<FakeIDBDatabaseCallbacks> connection1 = FakeIDBDatabaseCallbacks::create();
    db->openConnection(request1, connection1, 1, IDBDatabaseMetadata::DefaultIntVersion);

    RefPtr<MockIDBCallbacks> request2 = MockIDBCallbacks::create();
    RefPtr<FakeIDBDatabaseCallbacks> connection2 = FakeIDBDatabaseCallbacks::create();
    db->openConnection(request2, connection2, 2, IDBDatabaseMetadata::DefaultIntVersion);

    db->close(connection1);
    EXPECT_GT(backingStore->refCount(), 1);

    db->close(connection2);
    EXPECT_TRUE(backingStore->hasOneRef());
}

class MockIDBDatabaseBackendProxy : public IDBDatabaseBackendInterface {
public:
    static PassRefPtr<MockIDBDatabaseBackendProxy> create(WebIDBDatabaseImpl& database)
    {
        return adoptRef(new MockIDBDatabaseBackendProxy(database));
    }

    ~MockIDBDatabaseBackendProxy()
    {
        EXPECT_TRUE(m_wasCloseCalled);
    }

    virtual IDBDatabaseMetadata metadata() const { return IDBDatabaseMetadata(); }
    virtual void createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) OVERRIDE { };
    virtual void deleteObjectStore(int64_t transactionId, int64_t objectStoreId) OVERRIDE { }
    // FIXME: Remove this method in https://bugs.webkit.org/show_bug.cgi?id=103923.
    virtual PassRefPtr<IDBTransactionBackendInterface> createTransaction(int64_t, const Vector<int64_t>&, IDBTransaction::Mode) OVERRIDE { return 0; }
    virtual void createTransaction(int64_t, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>&, unsigned short mode) OVERRIDE { }

    virtual void close(PassRefPtr<IDBDatabaseCallbacks>) OVERRIDE
    {
        m_wasCloseCalled = true;
        m_webDatabase.close();
    }

    virtual void abort(int64_t transactionId) OVERRIDE { }
    virtual void commit(int64_t transactionId) OVERRIDE { }

    virtual void openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, unsigned short direction, bool keyOnly, TaskType, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void count(int64_t objectStoreId, int64_t indexId, int64_t transactionId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void get(int64_t objectStoreId, int64_t indexId, int64_t transactionId, PassRefPtr<IDBKeyRange>, bool keyOnly, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void put(int64_t transactionId, int64_t objectStoreId, Vector<uint8_t>*, PassRefPtr<IDBKey>, PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE { }
    virtual void setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE { }
    virtual void setIndexesReady(int64_t transactionId, int64_t objectStoreId, const Vector<int64_t>& indexIds) OVERRIDE { }
    virtual void deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks>) OVERRIDE { }

    virtual void createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool unique, bool multiEntry) OVERRIDE { ASSERT_NOT_REACHED(); }
    virtual void deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId) OVERRIDE { ASSERT_NOT_REACHED(); }

private:
    MockIDBDatabaseBackendProxy(WebIDBDatabaseImpl& webDatabase)
        : m_wasCloseCalled(false)
        , m_webDatabase(webDatabase) { }

    bool m_wasCloseCalled;

    WebIDBDatabaseImpl& m_webDatabase;
};

TEST(IDBDatabaseBackendTest, ForcedClose)
{
    RefPtr<IDBFakeBackingStore> backingStore = adoptRef(new IDBFakeBackingStore());
    EXPECT_TRUE(backingStore->hasOneRef());

    IDBFactoryBackendImpl* factory = 0;
    RefPtr<IDBDatabaseBackendImpl> backend = IDBDatabaseBackendImpl::create("db", backingStore.get(), factory, "uniqueid");
    EXPECT_GT(backingStore->refCount(), 1);

    RefPtr<FakeIDBDatabaseCallbacks> connection = FakeIDBDatabaseCallbacks::create();
    RefPtr<IDBDatabaseCallbacksProxy> connectionProxy = IDBDatabaseCallbacksProxy::create(adoptPtr(new WebIDBDatabaseCallbacksImpl(connection)));
    WebIDBDatabaseImpl webDatabase(backend, connectionProxy);

    RefPtr<MockIDBDatabaseBackendProxy> proxy = MockIDBDatabaseBackendProxy::create(webDatabase);
    RefPtr<MockIDBCallbacks> request = MockIDBCallbacks::create();
    backend->openConnection(request, connectionProxy, 3, IDBDatabaseMetadata::DefaultIntVersion);

    ScriptExecutionContext* context = 0;
    RefPtr<IDBDatabase> idbDatabase = IDBDatabase::create(context, proxy, connection);

    webDatabase.forceClose();

    EXPECT_TRUE(backingStore->hasOneRef());
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
