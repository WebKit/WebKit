/*
 * Copyright (C) 2019 Igalia, S.L.
 * Copyright (C) 2019 Metrological Group B.V.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

// For this file, we force checks even if we're running on release.
#define ENABLE_DATA_MUTEX_CHECKS 1
#include <wtf/DataMutex.h>

namespace TestWebKitAPI {

struct MyStructure {
    int number;
};

TEST(WTF_DataMutex, TakingTheMutex)
{
    DataMutex<MyStructure> myDataMutex;

    Lock* mutex;
    {
        DataMutexLocker wrapper { myDataMutex };
        mutex = &wrapper.mutex();
        ASSERT_TRUE(mutex->isLocked());
        wrapper->number = 5;

        wrapper.runUnlocked([mutex]() {
            ASSERT_FALSE(mutex->isLocked());
        });
        ASSERT_TRUE(mutex->isLocked());
    }
    ASSERT_FALSE(mutex->isLocked());

    {
        DataMutexLocker wrapper { myDataMutex };
        EXPECT_EQ(wrapper->number, 5);
    }
}

// FIXME: Tests using ASSERT_DEATH currently panic on playstation
#if !PLATFORM(PLAYSTATION)
TEST(WTF_DataMutex, RunUnlockedIllegalAccessDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    DataMutex<MyStructure> myDataMutex;
    DataMutexLocker wrapper { myDataMutex };
    wrapper->number = 5;

    ASSERT_DEATH(wrapper.runUnlocked([&]() {
        wrapper->number++;
    }), "");
}

TEST(WTF_DataMutex, DoubleLockDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    DataMutex<MyStructure> myDataMutex;
    DataMutexLocker wrapper1 { myDataMutex };
    ASSERT_DEATH(DataMutexLocker wrapper2 { myDataMutex }, "");
}
#endif

} // namespace TestWebKitAPI
