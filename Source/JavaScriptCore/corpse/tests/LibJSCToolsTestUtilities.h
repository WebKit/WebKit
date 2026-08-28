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

#pragma once

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <mach/mach.h>
#include <memory>
#include <span>
#include <stdint.h>
#include <string_view>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {
namespace Corpse {
class Process;
class Snapshot;
}
}

namespace JSCToolsTest {

// Assertions report against these, and main reports the totals.
extern unsigned assertionsRun;
extern unsigned assertionsFailed;
extern unsigned suitesSkipped;

// Only suites whose name contains this substring run. Null runs all of them.
extern const char* suiteFilter;

bool beginSuite(const char* name);
void skipSuite(const char* name, const char* why);

// Nothing is printed for a passing assertion: a passing run should be quiet, and
// a failing one should say only what failed.
#define TEST_ASSERT(condition, message) \
    do { \
        ++JSCToolsTest::assertionsRun; \
        if (!(condition)) { \
            ++JSCToolsTest::assertionsFailed; \
            dataLogLn("FAIL: ", message, " (", #condition, ") at ", __FILE__, ":", __LINE__); \
        } \
    } while (0)

// For values dataLog can print. Reports both sides, which is what makes a
// failure diagnosable without a debugger.
#define TEST_ASSERT_EQ(actual, expected, message) \
    do { \
        ++JSCToolsTest::assertionsRun; \
        auto testActual = (actual); \
        auto testExpected = (expected); \
        if (!(testActual == testExpected)) { \
            ++JSCToolsTest::assertionsFailed; \
            dataLogLn("FAIL: ", message, ": got ", testActual, ", expected ", testExpected, \
                " at ", __FILE__, ":", __LINE__); \
        } \
    } while (0)

#define TEST_ASSERT_HEX_EQ(actual, expected, message) \
    do { \
        ++JSCToolsTest::assertionsRun; \
        uint64_t testActual = (actual); \
        uint64_t testExpected = (expected); \
        if (testActual != testExpected) { \
            ++JSCToolsTest::assertionsFailed; \
            dataLogLn("FAIL: ", message, ": got 0x", hex(testActual), ", expected 0x", hex(testExpected), \
                " at ", __FILE__, ":", __LINE__); \
        } \
    } while (0)

// The number of names in this task's Mach port name space. Used to show that a
// sequence of operations leaves no port behind.
unsigned machPortNameCount();

// The number of send rights this task holds for `port`, or 0 if it holds no name for
// it. A task that acquires a port it already has a name for gets another reference
// under that same name rather than a new name, so a right that is taken and never
// given back shows up here and not in machPortNameCount().
unsigned machPortSendRightCount(mach_port_t);

// Attaches to this process and takes a corpse of it. That is what lets a test
// check what a corpse reports against what this process already knows about
// itself, and it needs no privilege: a task may always snapshot itself.
//
// Reports the failure if either step does not work, so a caller only has to
// check isValid() and return.
class SelfSnapshot {
public:
    SelfSnapshot();
    ~SelfSnapshot();

    SelfSnapshot(const SelfSnapshot&) = delete;
    SelfSnapshot& operator=(const SelfSnapshot&) = delete;

    bool isValid() const;
    JSC::Corpse::Snapshot& snapshot() const;
    RefPtr<JSC::Corpse::Process> process() const;

private:
    RefPtr<JSC::Corpse::Process> m_process;
    std::unique_ptr<JSC::Corpse::Snapshot> m_snapshot;
};

// Threads that park themselves until stopped, each under a name of our choosing,
// so that a corpse of this process contains threads whose properties are known.
class ParkedThreads {
public:
    struct Thread;

    // pthread keeps a thread name in a fixed buffer, so a longer name arrives cut
    // to this length.
    static constexpr size_t maximumNameLength = 63;

    ~ParkedThreads();

    // Returns false if the thread could not be created.
    bool spawn(const char* name);

    // Blocks until every spawned thread is parked, so that a snapshot taken
    // afterwards sees them with their names set and their stacks in use.
    bool waitUntilAllParked();

    void stopAndJoin();

    size_t count() const { return m_threads.size(); }

private:
    Vector<Thread*> m_threads;
};

} // namespace JSCToolsTest

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
