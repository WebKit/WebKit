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

#if USE(LEVELDB)

#include "FileSystem.h"
#include "LevelDBComparator.h"
#include "LevelDBDatabase.h"
#include "LevelDBIterator.h"
#include "LevelDBSlice.h"
#include "LevelDBTransaction.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace {

class SimpleComparator : public LevelDBComparator {
public:
    virtual int compare(const LevelDBSlice& a, const LevelDBSlice& b) const OVERRIDE
    {
        size_t len = std::min(a.end() - a.begin(), b.end() - b.begin());
        return memcmp(a.begin(), b.begin(), len);
    }
    virtual const char* name() const OVERRIDE { return "temp_comparator"; }
};

Vector<char> encodeString(const std::string& s)
{
    Vector<char> ret(s.size());
    for (size_t i = 0; i < s.size(); ++i)
        ret[i] = s[i];
    return ret;
}

TEST(LevelDBDatabaseTest, CorruptionTest)
{
    OwnPtr<webkit_support::ScopedTempDirectory> tempDirectory = adoptPtr(webkit_support::CreateScopedTempDirectory());
    tempDirectory->CreateUniqueTempDir();
    const String path = String::fromUTF8(tempDirectory->path().c_str());

    const Vector<char> key = encodeString("key");
    const Vector<char> putValue = encodeString("value");
    Vector<char> gotValue;
    SimpleComparator comparator;

    OwnPtr<LevelDBDatabase> leveldb = LevelDBDatabase::open(path, &comparator);
    EXPECT_TRUE(leveldb);
    bool success = leveldb->put(key, putValue);
    EXPECT_TRUE(success);
    leveldb.release();
    EXPECT_FALSE(leveldb);

    leveldb = LevelDBDatabase::open(path, &comparator);
    EXPECT_TRUE(leveldb);
    bool found = false;
    success = leveldb->safeGet(key, gotValue, found);
    EXPECT_TRUE(success);
    EXPECT_TRUE(found);
    EXPECT_EQ(putValue, gotValue);
    leveldb.release();
    EXPECT_FALSE(leveldb);

    const String filepath = pathByAppendingComponent(path, "CURRENT");
    PlatformFileHandle handle = openFile(filepath, OpenForWrite);
    truncateFile(handle, 0);
    closeFile(handle);

    leveldb = LevelDBDatabase::open(path, &comparator);
    EXPECT_FALSE(leveldb);

    bool destroyed = LevelDBDatabase::destroy(path);
    EXPECT_TRUE(destroyed);

    leveldb = LevelDBDatabase::open(path, &comparator);
    EXPECT_TRUE(leveldb);
    success = leveldb->safeGet(key, gotValue, found);
    EXPECT_TRUE(success);
    EXPECT_FALSE(found);
}

TEST(LevelDBDatabaseTest, Transaction)
{
    OwnPtr<webkit_support::ScopedTempDirectory> tempDirectory = adoptPtr(webkit_support::CreateScopedTempDirectory());
    tempDirectory->CreateUniqueTempDir();
    const String path = String::fromUTF8(tempDirectory->path().c_str());

    const Vector<char> key = encodeString("key");
    Vector<char> gotValue;
    SimpleComparator comparator;

    OwnPtr<LevelDBDatabase> leveldb = LevelDBDatabase::open(path, &comparator);
    EXPECT_TRUE(leveldb);

    const Vector<char> oldValue = encodeString("value");
    bool success = leveldb->put(key, oldValue);
    EXPECT_TRUE(success);

    RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(leveldb.get());

    const Vector<char> newValue = encodeString("new value");
    success = leveldb->put(key, newValue);
    EXPECT_TRUE(success);

    bool found = false;
    success = transaction->safeGet(key, gotValue, found);
    EXPECT_TRUE(success);
    EXPECT_TRUE(found);
    EXPECT_EQ(comparator.compare(gotValue, oldValue), 0);

    found = false;
    success = leveldb->safeGet(key, gotValue, found);
    EXPECT_TRUE(success);
    EXPECT_TRUE(found);
    EXPECT_EQ(comparator.compare(gotValue, newValue), 0);

    const Vector<char> addedKey = encodeString("added key");
    const Vector<char> addedValue = encodeString("added value");
    success = leveldb->put(addedKey, addedValue);
    EXPECT_TRUE(success);

    success = leveldb->safeGet(addedKey, gotValue, found);
    EXPECT_TRUE(success);
    EXPECT_TRUE(found);
    EXPECT_EQ(comparator.compare(gotValue, addedValue), 0);

    success = transaction->safeGet(addedKey, gotValue, found);
    EXPECT_TRUE(success);
    EXPECT_FALSE(found);
}

TEST(LevelDBDatabaseTest, TransactionIterator)
{
    OwnPtr<webkit_support::ScopedTempDirectory> tempDirectory = adoptPtr(webkit_support::CreateScopedTempDirectory());
    tempDirectory->CreateUniqueTempDir();
    const String path = String::fromUTF8(tempDirectory->path().c_str());

    const Vector<char> start = encodeString("");
    const Vector<char> key1 = encodeString("key1");
    const Vector<char> value1 = encodeString("value1");
    const Vector<char> key2 = encodeString("key2");
    const Vector<char> value2 = encodeString("value2");

    SimpleComparator comparator;
    bool success;

    OwnPtr<LevelDBDatabase> leveldb = LevelDBDatabase::open(path, &comparator);
    EXPECT_TRUE(leveldb);

    success = leveldb->put(key1, value1);
    EXPECT_TRUE(success);
    success = leveldb->put(key2, value2);
    EXPECT_TRUE(success);

    RefPtr<LevelDBTransaction> transaction = LevelDBTransaction::create(leveldb.get());

    success = leveldb->remove(key2);
    EXPECT_TRUE(success);

    OwnPtr<LevelDBIterator> it = transaction->createIterator();

    it->seek(start);

    EXPECT_TRUE(it->isValid());
    EXPECT_EQ(comparator.compare(it->key(), key1), 0);
    EXPECT_EQ(comparator.compare(it->value(), value1), 0);

    it->next();

    EXPECT_TRUE(it->isValid());
    EXPECT_EQ(comparator.compare(it->key(), key2), 0);
    EXPECT_EQ(comparator.compare(it->value(), value2), 0);

    it->next();

    EXPECT_FALSE(it->isValid());
}

} // namespace

#endif // USE(LEVELDB)
