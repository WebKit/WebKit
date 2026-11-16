/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include <wtf/Ref.h>
#include <wtf/RefCounter.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static const int IncrementExpected = 0xC0FFEE1;
static const int DecrementExpected = 0xC0FFEE2;
static const int CallbackNotExpected = 0xDECAF;

enum TestCounterType { };
typedef RefCounter<TestCounterType> TestCounter;
typedef TestCounter::Token TokenType;

TEST(WTF, RefCounter)
{
    // RefCounter API is pretty simple, containing the following 4 methods to test:
    //
    // 1) RefCounter(std::function<void()>);
    // 2) ~RefCounter();
    // 3) Ref<Count> token() const;
    // 4) unsigned value() const;
    //
    // We'll test:
    // 1) Construction:
    //   1a) with a callback
    //   1b) without a callback
    // 2) Destruction where the RefCounter::Count has:
    //   2a) a non-zero reference count (Count outlives RefCounter).
    //   2b) a zero reference count (Count is deleted by RefCounter's destructor).
    // 3) Call count to ref/deref the Count object, where:
    //   3a) ref with callback from 0 -> 1.
    //   3b) ref with callback from 1 -> >1.
    //   3c) deref with callback from >1 -> 1.
    //   3d) deref with callback from 1 -> 0.
    //   3d) deref with callback from 1 -> 0.
    //   3e) ref with callback from 1 -> >1 AFTER RefCounter has been destroyed.
    //   3f) deref with callback from >1 -> 1 AFTER RefCounter has been destroyed.
    //   3g) deref with callback from 1 -> 0 AFTER RefCounter has been destroyed.
    //   3h) ref without callback
    //   3i) deref without callback
    //   3j) ref using a Ref rather than a RefPtr (make sure there is no unnecessary reference count churn).
    //   3k) deref using a Ref rather than a RefPtr (make sure there is no unnecessary reference count churn).
    // 4) Test the value of the counter:
    //   4a) at construction.
    //   4b) as read within the callback.
    //   4c) as read after the ref/deref.

    // These values will outlive the following block.
    int callbackValue = CallbackNotExpected;
    TokenType incTo1Again;

    {
        // Testing (1a) - Construction with a callback.
        TestCounter* counterPtr = nullptr;
        TestCounter counter([&](RefCounterEvent event) {
            // Check that the callback is called at the expected times, and the correct number of times.
            if (RefCounterEvent::Increment == event)
                EXPECT_EQ(callbackValue, IncrementExpected);
            if (RefCounterEvent::Decrement == event)
                EXPECT_EQ(callbackValue, DecrementExpected);
            // return the value of the counter in the callback.
            callbackValue = counterPtr->value();
        });
        counterPtr = &counter;
        // Testing (4a) - after construction value() is 0.
        EXPECT_EQ(0, static_cast<int>(counter.value()));

        // Testing (3a) - ref with callback from 0 -> 1.
        callbackValue = IncrementExpected;
        TokenType incTo1(counter.count());
        // Testing (4b) & (4c) - values within & after callback.
        EXPECT_EQ(1, callbackValue);
        EXPECT_EQ(1, static_cast<int>(counter.value()));

        // Testing (3b) - ref with callback from 1 -> 2.
        callbackValue = IncrementExpected;
        TokenType incTo2(incTo1);
        // Testing (4b) & (4c) - values within & after callback.
        EXPECT_EQ(2, callbackValue);
        EXPECT_EQ(2, static_cast<int>(counter.value()));

        // Testing (3c) - deref with callback from >1 -> 1.
        callbackValue = DecrementExpected;
        incTo1 = nullptr;
        // Testing (4b) & (4c) - values within & after callback.
        EXPECT_EQ(1, callbackValue);
        EXPECT_EQ(1, static_cast<int>(counter.value()));

        {
            // Testing (3j) - ref using a Ref rather than a RefPtr.
            callbackValue = IncrementExpected;
            TokenType incTo2Again(counter.count());
            // Testing (4b) & (4c) - values within & after callback.
            EXPECT_EQ(2, callbackValue);
            EXPECT_EQ(2, static_cast<int>(counter.value()));
            // Testing (3k) - deref using a Ref rather than a RefPtr.

            callbackValue = DecrementExpected;
        }
        EXPECT_EQ(1, callbackValue);
        EXPECT_EQ(1, static_cast<int>(counter.value()));
        // Testing (4b) & (4c) - values within & after callback.

        // Testing (3d) - deref with callback from 1 -> 0.
        callbackValue = DecrementExpected;
        incTo2 = nullptr;
        // Testing (4b) & (4c) - values within & after callback.
        EXPECT_EQ(0, callbackValue);
        EXPECT_EQ(0, static_cast<int>(counter.value()));

        // Testing (2a) - Destruction where the TestCounter::Count has a non-zero reference count.
        callbackValue = IncrementExpected;
        incTo1Again = counter.count();
        EXPECT_EQ(1, callbackValue);
        EXPECT_EQ(1, static_cast<int>(counter.value()));
        callbackValue = CallbackNotExpected;
    }

    // Testing (3e) - ref with callback from 1 -> >1 AFTER RefCounter has been destroyed.
    TokenType incTo2Again = incTo1Again;
    // Testing (3f) - deref with callback from >1 -> 1 AFTER RefCounter has been destroyed.
    incTo1Again = nullptr;
    // Testing (3g) - deref with callback from 1 -> 0 AFTER RefCounter has been destroyed.
    incTo2Again = nullptr;

    // Testing (1b) - Construction without a callback.
    TestCounter counter;
    // Testing (4a) - after construction value() is 0.
    EXPECT_EQ(0, static_cast<int>(counter.value()));
    // Testing (3h) - ref without callback
    TokenType incTo1(counter.count());
    // Testing (4c) - value as read after the ref.
    EXPECT_EQ(1, static_cast<int>(counter.value()));
    // Testing (3i) - deref without callback
    incTo1 = nullptr;
    // Testing (4c) - value as read after the deref.
    EXPECT_EQ(0, static_cast<int>(counter.value()));
    // Testing (2b) - Destruction where the RefCounter::Count has a zero reference count.
    // ... not a lot to test here! - we can at least ensure this code path is run & we don't crash!
}

TEST(WTF, RefCounterDeleteCounterWithOutstandingTokens)
{
    {
        std::unique_ptr<TestCounter> counter = makeUnique<TestCounter>([&](RefCounterEvent event) {
            if (!counter->value())
                counter = nullptr;
        });

        TokenType incTo1 = counter->count();
        EXPECT_EQ(1UL, counter->value());
        incTo1 = nullptr;
        EXPECT_EQ(nullptr, counter.get());
    }
}

} // namespace TestWebKitAPI
