/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include <JavaScriptCore/CorpsePlatform.h>
#include <memory>
#include <span>
#include <stdint.h>
#include <string_view>
#include <sys/types.h>
#include <wtf/DataLog.h>
#include <wtf/Function.h>
#include <wtf/HexNumber.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/Vector.h>

#if OS(DARWIN)
#include <mach/mach.h>
#endif

#if ENABLE(MYA)
#include <JavaScriptCore/CorpseAddress.h>

namespace JSC {
class VM;
namespace Corpse {
class Process;
class Snapshot;
}
}
#endif

namespace JSCToolsTest {

// Assertions report against these, and main reports the totals.
extern unsigned assertionsRun;
extern unsigned assertionsFailed;
extern unsigned suitesSkipped;

// Only suites whose name contains this substring run. Null runs all of them.
extern const char* suiteFilter;

extern bool verbose;

void skipSuite(const char* name, const char* why);

bool linuxSkip(const char* name, const char* why);

void testAddress();
void testByteParser();
void testDiagnostics();
void testExportsTrie();
void testProcess();
void testSymbol();
void testRegion();
void testMemory();
void testSnapshot();
void testThreads();
void fuzzExportsTrie(uint64_t seed, unsigned iterations);
void testTypeinfo();

#if ENABLE(MYA_HEAP)
// Each compile unit that includes this header has its own copy, so a heap walk
// must find each copy's type in the compile unit that defines it.
constinit const ASCIILiteral copiedHeaderConstant = "copied header constant"_s;
const ASCIILiteral* copiedHeaderConstantInUtilities();
#endif

void testVM();

// Announces a suite, times it, and reports on the way out. Destroyed on every path out
// of a suite, including the early returns a suite takes when it cannot set itself up.
class SuiteTracer {
public:
    explicit SuiteTracer(const char* name);
    ~SuiteTracer();

    SuiteTracer(const SuiteTracer&) = delete;
    SuiteTracer& operator=(const SuiteTracer&) = delete;
    SuiteTracer(SuiteTracer&&) = delete;
    SuiteTracer& operator=(SuiteTracer&&) = delete;

    bool shouldRun() const { return m_shouldRun; }

private:
    const char* m_name;
    MonotonicTime m_start;
    unsigned m_reportsAtStart { 0 };
    bool m_shouldRun;
};

class ExpectedErrors {
public:
    explicit ExpectedErrors(unsigned count = 1);
    ~ExpectedErrors();

    ExpectedErrors(const ExpectedErrors&) = delete;
    ExpectedErrors& operator=(const ExpectedErrors&) = delete;

private:
    unsigned m_count;
    unsigned m_reportsAtStart;
};

Seconds totalSuiteTime();

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

#if ENABLE(MYA) && OS(DARWIN)

// The number of names in this task's Mach port name space. Used to show that a
// sequence of operations leaves no port behind, so it fails the test unless this
// is the task's only thread.
unsigned machPortNameCount();

// The number of send rights this task holds for `port`, or 0 if it holds no name for
// it. A task that acquires a port it already has a name for gets another reference
// under that same name rather than a new name, so a right that is taken and never
// given back shows up here and not in machPortNameCount().
unsigned machPortSendRightCount(mach_port_t);

#endif // ENABLE(MYA) && OS(DARWIN)

#if ENABLE(MYA)

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
#if OS(DARWIN)
    static constexpr size_t maximumNameLength = 63;
#else
    static constexpr size_t maximumNameLength = 15;
#endif

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

// A snapshot of `pid`, or null, having failed the test.
std::unique_ptr<JSC::Corpse::Snapshot> takeSnapshot(pid_t);

// create() an object, then analyze() it in and out of process.
void analyzeInAndOutOfProcess(JSC::Corpse::Address (*create)(), NOESCAPE const Function<void(JSC::Corpse::Snapshot&, JSC::Corpse::Address object)>& analyze);

// create() an object in a separate process only, then analyze() it there. Returns
// false, having logged why, if the target could not be snapshotted.
[[nodiscard]] bool analyzeInSeparateProcess(JSC::Corpse::Address (*create)(), NOESCAPE const Function<void(JSC::Corpse::Snapshot&, JSC::Corpse::Address object)>& analyze);

// analyzeInSeparateProcess, with the target started by a path relative to its own
// working directory, while this process is in another, and on Darwin exited before analyze().
void analyzeInSeparateProcessStartedByRelativePath(JSC::Corpse::Address (*create)(), NOESCAPE const Function<void(JSC::Corpse::Snapshot&, JSC::Corpse::Address object)>& analyze);

// create() an object in a separate process, snapshot it, and analyze() the
// snapshot only after that process has exited. A Linux snapshot ends with its target.
void analyzeAfterTargetExits(JSC::Corpse::Address (*create)(), NOESCAPE const Function<void(JSC::Corpse::Snapshot&, JSC::Corpse::Address object)>& analyze);

// create() an object in a separate process run from a copy of this executable,
// rename another executable over that copy once the object is reported, then
// snapshot the target and analyze() it.
void analyzeAfterExecutableReplaced(JSC::Corpse::Address (*create)(), NOESCAPE const Function<void(JSC::Corpse::Snapshot&, JSC::Corpse::Address object)>& analyze);

#if OS(LINUX)
// create() an object in a separate process, analyze() its snapshot, release the
// snapshot, then call afterRelease() while the target still runs.
void analyzeOutOfProcessThenRelease(JSC::Corpse::Address (*create)(), NOESCAPE const Function<void(JSC::Corpse::Snapshot&, JSC::Corpse::Address object)>& analyze, Function<void(pid_t target, pid_t copy)>&& afterRelease);

#endif

// The main function for a copy of this process launched as `--target <offset>`: runs the
// create function at `offset` into this executable and holds its object.
int runCorpseTarget(const char* offsetText);

// What a target does with its object: reports it to the analysis, and offers
// the analysis a snapshot of itself, at a safepoint of the target's VM if it set one.
void reportTargetObject(JSC::Corpse::Address);
void reportTargetAddress(JSC::Corpse::Address);
void offerTargetSnapshot();
void setTargetVM(JSC::VM&);

#endif // ENABLE(MYA)

} // namespace JSCToolsTest
