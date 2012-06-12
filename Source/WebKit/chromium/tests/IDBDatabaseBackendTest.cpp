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
#include "IDBDatabaseBackendImpl.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBFakeBackingStore.h"
#include "IDBIndexBackendImpl.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBTransactionCoordinator.h"

#include <gtest/gtest.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

TEST(IDBDatabaseBackendTest, BackingStoreRetention)
{
    RefPtr<IDBFakeBackingStore> backingStore = adoptRef(new IDBFakeBackingStore());
    EXPECT_TRUE(backingStore->hasOneRef());

    IDBTransactionCoordinator* coordinator = 0;
    IDBFactoryBackendImpl* factory = 0;
    RefPtr<IDBDatabaseBackendImpl> db = IDBDatabaseBackendImpl::create("db", backingStore.get(), coordinator, factory, "uniqueid");
    EXPECT_GT(backingStore->refCount(), 1);

    const bool autoIncrement = false;
    RefPtr<IDBObjectStoreBackendImpl> store = IDBObjectStoreBackendImpl::create(db.get(), "store", String("keyPath"), autoIncrement);
    EXPECT_GT(backingStore->refCount(), 1);

    const bool unique = false;
    const bool multiEntry = false;
    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(db.get(), store.get(), "index", String("keyPath"), unique, multiEntry);
    EXPECT_GT(backingStore->refCount(), 1);

    db.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
    store.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
    index.clear();
    EXPECT_TRUE(backingStore->hasOneRef());
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
