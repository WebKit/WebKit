//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// test_blob_cache_unittest.cpp:
//   Checks that TestBlobCache behaves as Android's MultifileBlobCache does.
//

#include "util/test_blob_cache.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
// Android's entry header: uint32_t magic, uint32_t CRC, then the key and value sizes as
// EGLsizeiANDROID.  24 bytes on a 64-bit device.
constexpr size_t kHeaderSize = 8 + 2 * sizeof(EGLsizeiANDROID);

uint64_t ReadLittleEndian(const std::string &bytes, size_t offset, size_t size)
{
    uint64_t value = 0;
    for (size_t i = 0; i < size; ++i)
    {
        value |= static_cast<uint64_t>(static_cast<uint8_t>(bytes[offset + i])) << (8 * i);
    }
    return value;
}

EGLSetBlobFuncANDROID gSet = nullptr;
EGLGetBlobFuncANDROID gGet = nullptr;

void EGLAPIENTRY CaptureFuncs(EGLDisplay, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get)
{
    gSet = set;
    gGet = get;
}

class TestBlobCacheTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        mDir = std::filesystem::path(testing::TempDir()) /
               (std::string("TestBlobCacheTest.") +
                testing::UnitTest::GetInstance()->current_test_info()->name());
        std::error_code ec;
        std::filesystem::remove_all(mDir, ec);
        ResetTestBlobCache();
        InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), TestBlobCacheLimits());
    }

    void TearDown() override
    {
        ResetTestBlobCache();
        std::error_code ec;
        std::filesystem::remove_all(mDir, ec);
    }

    void put(const std::string &key, size_t size, char fill = 'v')
    {
        const std::vector<char> value(size, fill);
        gSet(key.data(), static_cast<EGLsizeiANDROID>(key.size()), value.data(),
             static_cast<EGLsizeiANDROID>(value.size()));
    }

    // Looks up as ANGLE does: the size first, then the value.
    bool get(const std::string &key, std::vector<char> *valueOut = nullptr)
    {
        const EGLsizeiANDROID size =
            gGet(key.data(), static_cast<EGLsizeiANDROID>(key.size()), nullptr, 0);
        if (size <= 0)
        {
            return false;
        }
        std::vector<char> value(static_cast<size_t>(size));
        if (gGet(key.data(), static_cast<EGLsizeiANDROID>(key.size()), value.data(), size) != size)
        {
            return false;
        }
        if (valueOut)
        {
            *valueOut = std::move(value);
        }
        return true;
    }

    std::string readEntryFile(const std::string &name)
    {
        std::ifstream file(mDir / name, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::filesystem::path mDir;
};

// A stored value reads back, from disk after the cache is reopened, and a lookup counts one hit.
TEST_F(TestBlobCacheTest, StoresAcrossReopen)
{
    put("key", 100, 'x');
    // Reopen the directory, as a new process would
    InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), TestBlobCacheLimits());

    std::vector<char> value;
    ASSERT_TRUE(get("key", &value));
    EXPECT_EQ(value, std::vector<char>(100, 'x'));
    EXPECT_FALSE(get("other"));

    const TestBlobCacheStats stats = GetTestBlobCacheStats();
    EXPECT_EQ(stats.hits, 1u);
    EXPECT_EQ(stats.misses, 1u);
    EXPECT_EQ(stats.entries, 1u);
    // Charged as Android charges its file: the header, the key and the value.
    EXPECT_EQ(stats.totalSize, kHeaderSize + 3u + 100u);
}

// The entry file is the one Android would write: named by the Jenkins hash of the key, with the
// magic, zlib's CRC32 of the key and value, both sizes, the key and the value.
TEST_F(TestBlobCacheTest, WritesAndroidEntryFile)
{
    const std::string value = "hello";
    gSet("key", 3, value.data(), static_cast<EGLsizeiANDROID>(value.size()));

    const std::string file = readEntryFile("3852932664");
    ASSERT_EQ(file.size(), kHeaderSize + 3u + 5u);
    EXPECT_EQ(file.substr(0, 4), "$BFM");
    EXPECT_EQ(ReadLittleEndian(file, 4, 4), 0xED320D54u);
    EXPECT_EQ(ReadLittleEndian(file, 8, sizeof(EGLsizeiANDROID)), 3u);
    EXPECT_EQ(ReadLittleEndian(file, 8 + sizeof(EGLsizeiANDROID), sizeof(EGLsizeiANDROID)), 5u);
    EXPECT_EQ(file.substr(kHeaderSize), "keyhello");
}

// A value over the per-entry limit is dropped without a word, and an empty value removes the
// entry.
TEST_F(TestBlobCacheTest, RejectsOversizedAndRemovesEmpty)
{
    TestBlobCacheLimits limits;
    limits.maxValueSize = 64;
    InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), limits);

    put("big", 65);
    EXPECT_FALSE(get("big"));
    EXPECT_EQ(GetTestBlobCacheStats().rejectedStores, 1u);

    put("small", 64);
    EXPECT_TRUE(get("small"));
    put("small", 0);
    EXPECT_FALSE(get("small"));
    EXPECT_EQ(GetTestBlobCacheStats().entries, 0u);
}

// As on Android, an empty value for a key with no entry is stored like any other.  A lookup finds
// nothing to return.
TEST_F(TestBlobCacheTest, StoresEmptyValueForNewKey)
{
    put("key", 0);
    EXPECT_FALSE(get("key"));

    const TestBlobCacheStats stats = GetTestBlobCacheStats();
    EXPECT_EQ(stats.stores, 1u);
    EXPECT_EQ(stats.entries, 1u);
    EXPECT_EQ(readEntryFile("3852932664").size(), kHeaderSize + 3u);
}

// Crossing the entry limit evicts the least recently used entries down to half the limit.
TEST_F(TestBlobCacheTest, EvictsLeastRecentlyUsedToHalf)
{
    TestBlobCacheLimits limits;
    limits.maxTotalEntries = 4;
    InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), limits);

    put("a", 10);
    put("b", 10);
    put("c", 10);
    put("d", 10);
    // Makes "a" the most recently used.
    EXPECT_TRUE(get("a"));
    put("e", 10);

    EXPECT_TRUE(get("a"));
    EXPECT_FALSE(get("b"));
    EXPECT_FALSE(get("c"));
    EXPECT_TRUE(get("d"));
    EXPECT_TRUE(get("e"));
    EXPECT_EQ(GetTestBlobCacheStats().evictions, 2u);
}

// The total size limit counts each entry's header and key, not just its value.
TEST_F(TestBlobCacheTest, EvictsOnTotalSize)
{
    TestBlobCacheLimits limits;
    limits.maxTotalSize = 4 * (kHeaderSize + 1 + 76);
    InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), limits);

    put("a", 76);
    put("b", 76);
    put("c", 76);
    put("d", 76);
    EXPECT_EQ(GetTestBlobCacheStats().evictions, 0u);

    put("e", 76);
    // Down to half the limit, two entries, before "e" lands.
    EXPECT_EQ(GetTestBlobCacheStats().evictions, 2u);
    EXPECT_EQ(GetTestBlobCacheStats().entries, 3u);
}

// The order entries were used in survives reopening the cache, as Android keeps it in each file's
// access time.
TEST_F(TestBlobCacheTest, KeepsUseOrderAcrossReopen)
{
    TestBlobCacheLimits limits;
    limits.maxTotalEntries = 4;
    InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), limits);

    put("a", 10);
    put("b", 10);
    put("c", 10);
    put("d", 10);
    // Makes "a" the most recently used, then restarts.
    EXPECT_TRUE(get("a"));
    InitTestBlobCache(EGL_NO_DISPLAY, CaptureFuncs, mDir.string(), limits);
    EXPECT_EQ(GetTestBlobCacheStats().entries, 4u);

    // Down to half the limit, evicting two of "b", "c" and "d", before "e" lands.
    put("e", 10);
    EXPECT_EQ(GetTestBlobCacheStats().evictions, 2u);
    EXPECT_TRUE(get("a"));
    EXPECT_TRUE(get("e"));
}

// An entry changed on disk fails its CRC check, and is removed rather than returned.
TEST_F(TestBlobCacheTest, RemovesCorruptedEntry)
{
    put("key", 5, 'x');
    {
        std::fstream file(mDir / "3852932664", std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(static_cast<std::streamoff>(kHeaderSize + 3));
        file.put('y');
    }

    EXPECT_FALSE(get("key"));
    EXPECT_EQ(GetTestBlobCacheStats().entries, 0u);
    EXPECT_FALSE(std::filesystem::exists(mDir / "3852932664"));
}

// The directory is named after the process, so one left behind by an earlier run, such as a dead
// process with the same ID, is emptied and reused.
TEST(TestBlobCacheDirectoryTest, EmptiesLeftOverDirectory)
{
    const std::string dir = CreateTestBlobCacheDirectory();
    ASSERT_FALSE(dir.empty());
    EXPECT_TRUE(std::filesystem::is_empty(dir));

    // Leave something behind, as a run that didn't clean up would
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::create_directories(dir + "/left_over", ec));

    EXPECT_EQ(CreateTestBlobCacheDirectory(), dir);
    EXPECT_TRUE(std::filesystem::is_empty(dir));

    RemoveTestBlobCacheDirectory(dir);
    EXPECT_FALSE(std::filesystem::exists(dir));
}
}  // namespace
