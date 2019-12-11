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
#include <wtf/DataMutex.h>

using namespace WTF;

namespace TestWebKitAPI {

struct MyStructure {
    int number;
};
DataMutex<MyStructure> myDataMutex;

TEST(WTF_DataMutex, TakingTheMutex)
{
    Lock* mutex;
    {
        DataMutex<MyStructure>::LockedWrapper wrapper(myDataMutex);
        mutex = &wrapper.mutex();
        ASSERT_TRUE(mutex->isHeld());
        wrapper->number = 5;
    }
    ASSERT_FALSE(mutex->isHeld());

    {
        DataMutex<MyStructure>::LockedWrapper wrapper(myDataMutex);
        EXPECT_EQ(wrapper->number, 5);
    }
}

} // namespace TestWebKitAPI
