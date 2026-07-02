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
#include <pthread.h>
#include <thread>

#include "pas_scavenger.h"

using namespace std;

extern "C" inline bool incrementCounter(void* counter)
{
    auto* atomic_counter = reinterpret_cast<std::atomic<int>*>(counter);
    (*atomic_counter)++;
    return false;
}

extern "C" inline bool noopForeignWorkCallback(void*)
{
    return false;
}

inline void testForeignWorkCallbackInstallReleasesLockOnSaturation()
{
    // Saturate the descriptor table by installing until one fails. The last
    // (failing) install exercises the saturated path that previously returned
    // without releasing foreign_work.lock, leaving the mutex permanently held.
    while (pas_scavenger_try_install_foreign_work_callback(noopForeignWorkCallback, 1, nullptr)) { }

    // pthread_mutex_trylock would return EBUSY if the mutex were still held.
    // Avoid calling try_install_foreign_work_callback again here, since under
    // the unfixed code that call would block forever on the leaked lock.
    int rc = pthread_mutex_trylock(&pas_scavenger_data_instance->foreign_work.lock);
    CHECK_EQUAL(rc, 0);
    pthread_mutex_unlock(&pas_scavenger_data_instance->foreign_work.lock);
}

inline void testCallbacksAreCalledWhenExpected(int scavenger_on_ms, int scavenger_off_ms)
{
    std::atomic<int> counter { 0 };
    CHECK(pas_scavenger_try_install_foreign_work_callback(incrementCounter, 1, &counter));

    pas_scavenger_did_create_eligible();
    pas_scavenger_notify_eligibility_if_needed();

    int prevCounterValue;
    {
        pas_scavenger_suspend();
        prevCounterValue = counter.load();

        std::this_thread::sleep_for(std::chrono::milliseconds(scavenger_off_ms));
        CHECK_EQUAL(counter.load(), prevCounterValue);

        pas_scavenger_resume();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(scavenger_on_ms));
    CHECK_GREATER(counter.load(), prevCounterValue);
}

void addScavengerExternalWorkTests()
{
    {
        TestScope frequentScavenging(
            "frequent-scavenging",
            [] () {
                pas_scavenger_period_in_milliseconds = 1.;
                pas_scavenger_max_epoch_delta = -1ll * 1000ll * 1000ll;
            });
        ADD_TEST(testCallbacksAreCalledWhenExpected(50, 50));
    }
    ADD_TEST(testForeignWorkCallbackInstallReleasesLockOnSaturation());
}
