//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// test_blob_cache.h:
//   A disk-backed EGL_ANDROID_blob_cache for tests, modelled on Android's MultifileBlobCache.  See
//   test_blob_cache.cpp for how it mirrors Android's behavior.
//

#ifndef ANGLE_UTIL_TEST_BLOB_CACHE_H_
#define ANGLE_UTIL_TEST_BLOB_CACHE_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstddef>
#include <cstdint>
#include <string>

// The defaults are AOSP's (frameworks/native/opengl/libs/EGL/egl_cache.cpp).
struct TestBlobCacheLimits
{
    size_t maxKeySize      = 1024 * 1024;
    size_t maxValueSize    = 8 * 1024 * 1024;
    size_t maxTotalSize    = 32 * 1024 * 1024;
    size_t maxTotalEntries = 4 * 1024;
};

struct TestBlobCacheStats
{
    // Lookups that found an entry and returned it, and lookups that found none.
    uint64_t hits   = 0;
    uint64_t misses = 0;
    // Values stored, and their bytes.  Android has no append, so each store writes its whole value.
    uint64_t stores      = 0;
    uint64_t storedBytes = 0;
    // Stores refused for a key or value over the per-entry limit.
    uint64_t rejectedStores = 0;
    // Entries removed to get back under the total limits.
    uint64_t evictions = 0;
    // What the cache holds now, charged as Android charges it.
    size_t entries   = 0;
    size_t totalSize = 0;
};

std::string CreateTestBlobCacheDirectory();
void RemoveTestBlobCacheDirectory(const std::string &dir);
void InitTestBlobCache(EGLDisplay display,
                       PFNEGLSETBLOBCACHEFUNCSANDROIDPROC setFuncs,
                       const std::string &dir,
                       const TestBlobCacheLimits &limits);
TestBlobCacheStats GetTestBlobCacheStats();
void ResetTestBlobCache();

#endif  // ANGLE_UTIL_TEST_BLOB_CACHE_H_
