/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CorpseThreadTest.h"

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseRegion.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/CorpseThread.h>
#include <string>
#include <string_view>

namespace JSCToolsTest {

using JSC::Corpse::Thread;

void testThreads()
{
    if (!beginSuite("Thread"))
        return;

    static constexpr const char* alphaName = "jsctools alpha";
    static constexpr const char* betaName = "jsctools beta";
    // Longer than a pthread name can hold, so that truncation is exercised.
    static constexpr const char* longName =
        "jsctools a thread whose name is far too long to fit in the space a pthread name has";

    ParkedThreads parked;
    TEST_ASSERT(parked.spawn(alphaName), "a named thread starts");
    TEST_ASSERT(parked.spawn(betaName), "a second named thread starts");
    TEST_ASSERT(parked.spawn(longName), "a thread with an overlong name starts");
    if (!parked.waitUntilAllParked()) {
        TEST_ASSERT(false, "the spawned threads parked themselves");
        return;
    }

    SelfSnapshot self;
    if (!self.isValid())
        return;

    const Vector<Thread>& threads = self.snapshot().threads();
    TEST_ASSERT(threads.size() >= 1 + parked.count(),
        "the corpse holds at least this process's own threads");

    bool foundAlpha = false;
    bool foundBeta = false;
    bool foundTruncated = false;
    std::string expectedTruncated(std::string_view(longName).substr(0, ParkedThreads::maximumNameLength));

    for (const Thread& thread : threads) {
        if (thread.name() == alphaName)
            foundAlpha = true;
        else if (thread.name() == betaName)
            foundBeta = true;
        else if (thread.name() == expectedTruncated)
            foundTruncated = true;

        TEST_ASSERT(thread.id(), "every thread has an identifier");
        TEST_ASSERT(thread.name().length() <= ParkedThreads::maximumNameLength,
            "no thread name is longer than a pthread name can be");

        // The stack is defined as the region the stack pointer points into, so if
        // both were read they have to agree.
        if (thread.stackPointer()) {
            TEST_ASSERT(thread.hasStack(), "a thread with a stack pointer has a stack region");
            if (thread.hasStack()) {
                TEST_ASSERT(thread.stackRegion().contains(thread.stackPointer()),
                    "a thread's stack pointer lies inside its stack region");
                TEST_ASSERT(thread.stackRegion().pageCount() >= thread.stackRegion().residentPageCount(),
                    "a stack has at least as many pages as it has resident");
            }
        }

        TEST_ASSERT(!std::string_view(thread.runStateDescription()).empty(),
            "a thread's run state has a name");
    }

    TEST_ASSERT(foundAlpha, "a named thread appears in the corpse under its name");
    TEST_ASSERT(foundBeta, "a second named thread appears under its name");
    TEST_ASSERT(foundTruncated, "an overlong thread name appears cut to what a pthread name holds");

    // Reading the threads is the expensive part, so it happens once.
    const Vector<Thread>& again = self.snapshot().threads();
    TEST_ASSERT(&again == &threads, "the threads of a snapshot are read once and kept");

    parked.stopAndJoin();
}

} // namespace JSCToolsTest

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
