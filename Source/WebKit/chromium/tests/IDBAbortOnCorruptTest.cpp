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
#include "DOMStringList.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendInterface.h"
#include "IDBDatabaseBackendInterface.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBFakeBackingStore.h"
#include "SecurityOrigin.h"
#include <gtest/gtest.h>
#include <wtf/Vector.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

class MockIDBCallbacks : public IDBCallbacks {
public:
    static PassRefPtr<MockIDBCallbacks> create()
    {
        return adoptRef(new MockIDBCallbacks());
    }
    virtual ~MockIDBCallbacks()
    {
        EXPECT_TRUE(m_wasErrorCalled);
    }
    virtual void onError(PassRefPtr<IDBDatabaseError>)
    {
        m_wasErrorCalled = true;
    }
    virtual void onSuccess(PassRefPtr<DOMStringList>) { }
    virtual void onSuccess(PassRefPtr<IDBCursorBackendInterface>, PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SerializedScriptValue>) { }
    virtual void onSuccess(PassRefPtr<IDBDatabaseBackendInterface>)
    {
        EXPECT_TRUE(false);
    }
    virtual void onSuccess(PassRefPtr<IDBKey>) { }
    virtual void onSuccess(PassRefPtr<IDBTransactionBackendInterface>) { }
    virtual void onSuccess(PassRefPtr<SerializedScriptValue>) { }
    virtual void onSuccess(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBKey>, const IDBKeyPath&) { }
    virtual void onSuccess(int64_t) OVERRIDE { }
    virtual void onSuccess() OVERRIDE { }
    virtual void onSuccess(PassRefPtr<IDBKey>, PassRefPtr<IDBKey>, PassRefPtr<SerializedScriptValue>) { };
    virtual void onSuccessWithPrefetch(const Vector<RefPtr<IDBKey> >&, const Vector<RefPtr<IDBKey> >&, const Vector<RefPtr<SerializedScriptValue> >&) { }
    virtual void onBlocked() { }
private:
    MockIDBCallbacks() : m_wasErrorCalled(false) { }

    bool m_wasErrorCalled;
};

class FailingBackingStore : public IDBFakeBackingStore {
public:
    virtual ~FailingBackingStore() { }
    static PassRefPtr<IDBBackingStore> open()
    {
        return adoptRef(new FailingBackingStore);
    }
    virtual bool createIDBDatabaseMetaData(const String&, const String&, int64_t, int64_t&)
    {
        return false;
    }
};

class FailingIDBFactoryBackendImpl : public IDBFactoryBackendImpl {
public:
    virtual ~FailingIDBFactoryBackendImpl() { }
    static PassRefPtr<FailingIDBFactoryBackendImpl> create()
    {
        return adoptRef(new FailingIDBFactoryBackendImpl);
    }
    virtual void removeIDBDatabaseBackend(const WTF::String &) { }

protected:
    virtual PassRefPtr<IDBBackingStore> openBackingStore(PassRefPtr<SecurityOrigin> prpOrigin, const String& dataDir)
    {
        return FailingBackingStore::open();
    }
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

TEST(IDBAbortTest, TheTest)
{
    RefPtr<IDBFactoryBackendImpl> factory = FailingIDBFactoryBackendImpl::create();
    const String& name = "db name";
    RefPtr<MockIDBCallbacks> callbacks = MockIDBCallbacks::create();
    RefPtr<FakeIDBDatabaseCallbacks> databaseCallbacks = FakeIDBDatabaseCallbacks::create();
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create("http", "localhost", 81);
    const int64_t DummyVersion = 2;
    factory->open(name, DummyVersion, callbacks.get(), databaseCallbacks, origin, 0 /*Frame*/, String() /*path*/);
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
