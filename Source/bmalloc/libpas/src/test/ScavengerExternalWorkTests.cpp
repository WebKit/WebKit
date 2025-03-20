/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "TestHarness.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "pas_scavenger.h"

using namespace std;

extern "C" inline bool incrementCounter(void* counter)
{
    auto* atomic_counter = reinterpret_cast<std::atomic<int>*>(counter);
    (*atomic_counter)++;
    return false;
}

inline void testCallbacksAreCalledWhenExpected(int scavenger_on_ms, int scavenger_off_ms)
{
    TestScope frequentScavenging(
        "frequent-scavenging",
        [] () {
            pas_scavenger_period_in_milliseconds = 1.;
            pas_scavenger_max_epoch_delta = -1ll * 1000ll * 1000ll;
        });

    auto counter = 0;
    [[maybe_unused]] void* counter_ptr = reinterpret_cast<void*>(&counter);
    CHECK(pas_scavenger_try_install_foreign_work_callback(incrementCounter, 1, counter_ptr));

    int prevCounterValue;
    {
        SuspendScavengerScope suspendScavenger;
        prevCounterValue = counter;

        std::this_thread::sleep_for(std::chrono::milliseconds(scavenger_off_ms));
        CHECK_EQUAL(counter, prevCounterValue);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(scavenger_on_ms));
    CHECK_GREATER(counter, prevCounterValue);
}

void addScavengerExternalWorkTests()
{
    ADD_TEST(testCallbacksAreCalledWhenExpected(50, 50));
}
