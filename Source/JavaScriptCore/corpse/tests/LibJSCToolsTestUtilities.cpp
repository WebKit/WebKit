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
#include "LibJSCToolsTestUtilities.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>

namespace JSCToolsTest {

unsigned assertionsRun = 0;
unsigned assertionsFailed = 0;
unsigned suitesSkipped = 0;
const char* suiteFilter = nullptr;
bool verbose = false;

static Seconds s_totalSuiteTime;

SuiteTracer::SuiteTracer(const char* name)
    : m_name(name)
    , m_shouldRun(!suiteFilter || std::string_view(name).contains(std::string_view(suiteFilter)))
{
    if (!m_shouldRun)
        return;
    dataLogLn("--- ", m_name);
    m_start = MonotonicTime::now();
}

SuiteTracer::~SuiteTracer()
{
    if (!m_shouldRun)
        return;

    Seconds elapsed = MonotonicTime::now() - m_start;
    s_totalSuiteTime += elapsed;
    if (!verbose)
        return;

    uint64_t microseconds = static_cast<uint64_t>(elapsed.microseconds());
    uint64_t fraction = microseconds % 1000;
    dataLogLn("    ran for ", microseconds / 1000, ".",
        fraction < 100 ? "0" : "", fraction < 10 ? "0" : "", fraction, " ms");
}

Seconds totalSuiteTime()
{
    return s_totalSuiteTime;
}

void skipSuite(const char* name, const char* why)
{
    ++suitesSkipped;
    dataLogLn("SKIP: ", name, ": ", why);
}

unsigned machPortNameCount()
{
    mach_port_name_array_t names = nullptr;
    mach_msg_type_number_t nameCount = 0;
    mach_port_type_array_t types = nullptr;
    mach_msg_type_number_t typeCount = 0;
    auto result = mach_port_names(mach_task_self(), &names, &nameCount, &types, &typeCount);
    RELEASE_ASSERT(result == KERN_SUCCESS);

    mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(names), nameCount * sizeof(mach_port_name_t));
    mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(types), typeCount * sizeof(mach_port_type_t));
    return nameCount;
}

unsigned machPortSendRightCount(mach_port_t port)
{
    mach_port_urefs_t refs = 0;
    auto result = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &refs);
    if (result == KERN_INVALID_NAME) {
        // A name this task does not hold is an answer -- it holds no rights under it --
        // rather than a failure. Hence, has no send right.
        return 0;
    }
    RELEASE_ASSERT(result == KERN_SUCCESS);
    return refs; // Can still be 0 (which still means no send right).
}

SelfSnapshot::SelfSnapshot()
{
    m_process = JSC::Corpse::Process::create(getpid());
    if (!m_process->attach()) {
        TEST_ASSERT(false, "attaching to this process succeeds");
        return;
    }
    m_snapshot = WTF::makeUnique<JSC::Corpse::Snapshot>(m_process);
    if (!m_snapshot->isValid())
        TEST_ASSERT(false, "a snapshot of this process is valid");
}

SelfSnapshot::~SelfSnapshot() = default;

bool SelfSnapshot::isValid() const
{
    return m_snapshot && m_snapshot->isValid();
}

JSC::Corpse::Snapshot& SelfSnapshot::snapshot() const
{
    return *m_snapshot;
}

RefPtr<JSC::Corpse::Process> SelfSnapshot::process() const
{
    return m_process;
}

// One control block for all parked threads, so that they can be told to stop
// together. Only one ParkedThreads is expected to be alive at a time.
static pthread_mutex_t parkMutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned parkedCount = 0;
static bool parkStopping = false;

struct ParkedThreads::Thread {
    pthread_t handle { };
    std::string name;
};

static void* parkThread(void* argument)
{
    auto* thread = static_cast<ParkedThreads::Thread*>(argument);
    pthread_setname_np(thread->name.c_str());

    pthread_mutex_lock(&parkMutex);
    ++parkedCount;
    while (!parkStopping) {
        pthread_mutex_unlock(&parkMutex);
        usleep(1000);
        pthread_mutex_lock(&parkMutex);
    }
    pthread_mutex_unlock(&parkMutex);
    return nullptr;
}

ParkedThreads::~ParkedThreads()
{
    stopAndJoin();
}

bool ParkedThreads::spawn(const char* name)
{
    auto* thread = new Thread;
    // pthread cuts a name that does not fit, and so must this copy, so that the
    // name asked for here is the name a corpse will report.
    thread->name = std::string_view(name).substr(0, maximumNameLength);
    if (pthread_create(&thread->handle, nullptr, parkThread, thread)) {
        delete thread;
        return false;
    }
    m_threads.append(thread);
    return true;
}

bool ParkedThreads::waitUntilAllParked()
{
    // Bounded so that a thread that never starts fails the test rather than
    // hanging it.
    for (unsigned attempt = 0; attempt < 5000; ++attempt) {
        pthread_mutex_lock(&parkMutex);
        bool ready = parkedCount >= m_threads.size();
        pthread_mutex_unlock(&parkMutex);
        if (ready) {
            // A thread counts itself as parked just before it settles into its
            // wait, so give it that moment before anything reads its state.
            usleep(50 * 1000);
            return true;
        }
        usleep(1000);
    }
    return false;
}

void ParkedThreads::stopAndJoin()
{
    if (m_threads.isEmpty())
        return;

    pthread_mutex_lock(&parkMutex);
    parkStopping = true;
    pthread_mutex_unlock(&parkMutex);

    for (Thread* thread : m_threads) {
        pthread_join(thread->handle, nullptr);
        delete thread;
    }
    m_threads.clear();

    pthread_mutex_lock(&parkMutex);
    parkStopping = false;
    parkedCount = 0;
    pthread_mutex_unlock(&parkMutex);
}

} // namespace JSCToolsTest

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
