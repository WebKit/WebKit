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
#include "LevelDBSlice.h"
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
        ret.append(s[i]);
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
    success = leveldb->get(key, gotValue);
    EXPECT_TRUE(success);
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
    success = leveldb->get(key, gotValue);
    EXPECT_FALSE(success);
}

} // namespace

#endif // USE(LEVELDB)
