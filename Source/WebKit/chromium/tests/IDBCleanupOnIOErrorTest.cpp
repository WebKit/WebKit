/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "LevelDBDatabase.h"
#include "SecurityOrigin.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

class BustedLevelDBDatabase : public LevelDBDatabase {
public:
    static PassOwnPtr<LevelDBDatabase> open(const String& fileName, const LevelDBComparator*)
    {
        return adoptPtr(new BustedLevelDBDatabase);
    }
    virtual bool safeGet(const LevelDBSlice& key, Vector<char>& value, bool& found, const LevelDBSnapshot* = 0)
    {
        // false means IO error.
        return false;
    }
};

class MockLevelDBFactory : public LevelDBFactory {
public:
    MockLevelDBFactory() : m_destroyCalled(false) { }
    virtual PassOwnPtr<LevelDBDatabase> openLevelDB(const String& fileName, const LevelDBComparator* comparator)
    {
        return BustedLevelDBDatabase::open(fileName, comparator);
    }
    virtual bool destroyLevelDB(const String& fileName)
    {
        EXPECT_FALSE(m_destroyCalled);
        m_destroyCalled = true;
        return false;
    }
    virtual ~MockLevelDBFactory()
    {
        EXPECT_TRUE(m_destroyCalled);
    }
private:
    bool m_destroyCalled;
};

TEST(IDBIOErrorTest, CleanUpTest)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create("http", "localhost", 81);
    OwnPtr<webkit_support::ScopedTempDirectory> tempDirectory = adoptPtr(webkit_support::CreateScopedTempDirectory());
    tempDirectory->CreateUniqueTempDir();
    const String path = String::fromUTF8(tempDirectory->path().c_str());
    String dummyFileIdentifier;
    IDBFactoryBackendImpl* dummyIDBFactory = 0;
    MockLevelDBFactory mockLevelDBFactory;
    RefPtr<IDBBackingStore> backingStore = IDBBackingStore::open(origin.get(), path, dummyFileIdentifier, dummyIDBFactory, &mockLevelDBFactory);
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
