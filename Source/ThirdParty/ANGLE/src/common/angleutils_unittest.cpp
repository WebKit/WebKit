//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angleutils_unittest.cpp: Unit tests for ANGLE's common utilities.

#include "gtest/gtest.h"

#include "common/angleutils.h"

#include <array>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace
{

// Test that multiple array indices are written out in the right order.
TEST(ArrayIndexString, MultipleArrayIndices)
{
    std::vector<unsigned int> indices;
    indices.push_back(12);
    indices.push_back(34);
    indices.push_back(56);
    EXPECT_EQ("[56][34][12]", ArrayIndexString(indices));
}

// Tests that MakeStaticString is thread-safe.
TEST(MakeStaticString, ThreadSafety)
{
    constexpr size_t kThreadCount    = 16;
    constexpr size_t kIterationCount = 1'000;

    std::array<std::thread, kThreadCount> threads;

    std::mutex mutex;
    std::condition_variable condVar;
    size_t readyCount = 0;

    std::array<std::array<const char *, kIterationCount>, kThreadCount> results = {};

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i] = std::thread([&, i]() {
            // Wait for all threads to start, so the following loop is as simultaneously
            // executed as possible.
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++readyCount;
                if (readyCount < kThreadCount)
                {
                    condVar.wait(lock, [&]() { return readyCount == kThreadCount; });
                }
                else
                {
                    condVar.notify_all();
                }
            }
            for (size_t j = 0; j < kIterationCount; ++j)
            {
                results[i][j] = MakeStaticString("extension_" + std::to_string(j));
            }
        });
    }

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i].join();
    }

    // Verify all threads got valid, identical pointers for the same input strings.
    for (size_t j = 0; j < kIterationCount; ++j)
    {
        EXPECT_NE(results[0][j], nullptr);
        EXPECT_STREQ(results[0][j], ("extension_" + std::to_string(j)).c_str());
        for (size_t i = 1; i < kThreadCount; ++i)
        {
            // Same input string should return the same interned pointer.
            EXPECT_EQ(results[0][j], results[i][j]);
        }
    }
}

}  // anonymous namespace
