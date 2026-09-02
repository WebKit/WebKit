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
#include "CorpseProcessTest.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseProcess.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/text/CString.h>

namespace JSCToolsTest {

using JSC::Corpse::Process;

// A pid that is certainly not in use: a child that has been reaped. Returns 0 if no
// child could be made, reporting why, since the caller only sees the missing pid.
static pid_t reapedChildPid()
{
    pid_t child = fork();
    if (!child)
        _exit(0);
    if (child < 0) {
        dataLogLn("    could not fork: ", safeStrerror(errno));
        return 0;
    }

    // EINTR leaves the child unreaped, so resume rather than report a failure.
    int status = 0;
    pid_t waited = 0;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited != child) {
        dataLogLn("    could not reap child ", child, ": waitpid returned ", waited,
            ", errno ", errno, " (", safeStrerror(errno), ")");
        return 0;
    }
    return child;
}

#if CPU(ARM64)
// Launches /bin/sh as x86_64, which on Apple silicon means under translation.
// Returns 0 if this machine cannot run x86_64 code.
static pid_t spawnTranslatedChild()
{
    posix_spawnattr_t attributes;
    if (posix_spawnattr_init(&attributes))
        return 0;

    cpu_type_t preference = CPU_TYPE_X86_64;
    size_t counted = 0;
    posix_spawnattr_setbinpref_np(&attributes, 1, &preference, &counted);

    char* const arguments[] = {
        const_cast<char*>("/bin/sh"),
        const_cast<char*>("-c"),
        const_cast<char*>("sleep 30"),
        nullptr
    };
    pid_t child = 0;
    int error = posix_spawn(&child, "/bin/sh", nullptr, &attributes, arguments, nullptr);
    posix_spawnattr_destroy(&attributes);

    if (error || counted != 1)
        return 0;
    return child;
}
#endif // CPU(ARM64)

void testProcess()
{
    SuiteTracer tracer("Process");
    if (!tracer.shouldRun())
        return;

    {
        RefPtr<Process> process = Process::create(getpid());
        TEST_ASSERT(!process->isAttached(), "a new Process is not attached");
        TEST_ASSERT_EQ(process->pid(), getpid(), "a Process keeps the pid it was given");

        TEST_ASSERT(process->attach(), "attaching to this process succeeds");
        TEST_ASSERT(process->isAttached(), "attaching leaves the Process attached");
        TEST_ASSERT(process->holdsLiveTask(), "the task port names this very process");
        TEST_ASSERT(process->attach(), "attaching an already attached Process succeeds");

        process->detach();
        TEST_ASSERT(!process->isAttached(), "detaching releases the task port");
        TEST_ASSERT(!process->holdsLiveTask(), "a detached Process holds no task");

        TEST_ASSERT(process->attach(), "a detached Process can attach again");
        process->detach();
        process->detach();
        TEST_ASSERT(!process->isAttached(), "detaching twice is harmless");
    }
    {
        // Attaching takes a send right to the target's task port, and every path out of
        // an attach has to give it back. Attaching to this very process yields the name
        // this task already holds for itself, so the kernel adds a reference to that
        // name rather than handing out a new one: a right that is never given back
        // shows up in the reference count and not in the size of the name space.
        unsigned namesBefore = machPortNameCount();
        mach_port_t port = MACH_PORT_NULL;
        unsigned refsAttached = 0;
        {
            RefPtr<Process> process = Process::create(getpid());
            TEST_ASSERT(process->attach(), "attaching to this process succeeds");
            if (process->isAttached()) {
                port = process->taskPort();
                refsAttached = machPortSendRightCount(port);
                TEST_ASSERT(refsAttached, "an attached Process holds a send right to the task port");

                process->attach();
                TEST_ASSERT_EQ(machPortSendRightCount(port), refsAttached,
                    "attaching an attached Process takes no further send right");

                process->detach();
                TEST_ASSERT_EQ(machPortSendRightCount(port), refsAttached - 1,
                    "detaching gives the send right back");
                process->detach();
                TEST_ASSERT_EQ(machPortSendRightCount(port), refsAttached - 1,
                    "detaching twice gives back only what one attach took");

                process->attach(); // Left attached, so the destructor has to release it.
            }
        }
        if (refsAttached) {
            TEST_ASSERT_EQ(machPortSendRightCount(port), refsAttached - 1,
                "destroying an attached Process gives its send right back");
            TEST_ASSERT_EQ(machPortNameCount(), namesBefore,
                "attaching and detaching leaves no port name behind");
        }
    }
    {
        pid_t gone = reapedChildPid();
        if (!gone)
            TEST_ASSERT(gone, "a child could be forked and reaped");
        else {
            dataLogLn("    (the next line is the failure this test asks for)");
            unsigned namesBefore = machPortNameCount();
            RefPtr<Process> process = Process::create(gone);
            TEST_ASSERT(!process->attach(), "attaching to a process that has exited fails");
            TEST_ASSERT(!process->isAttached(), "a failed attach leaves the Process unattached");
            TEST_ASSERT_EQ(machPortNameCount(), namesBefore, "a failed attach leaves no port name behind");
        }
    }
    {
        RefPtr<Process> process = Process::create(getpid());
        TEST_ASSERT(!process->isTranslated(), "this process does not run under translation");
        // Answering this needs no task port, only the pid.
        TEST_ASSERT(!process->isAttached(), "asking about translation does not attach");
    }
    {
        RefPtr<Process> initProcess = Process::create(1);
        TEST_ASSERT(!initProcess->isTranslated(), "launchd does not run under translation");
    }
    {
        pid_t gone = reapedChildPid();
        if (gone) {
            RefPtr<Process> process = Process::create(gone);
            TEST_ASSERT(!process->isTranslated(), "a process that has exited is not translated");
        }
    }
#if CPU(ARM64)
    {
        pid_t translated = spawnTranslatedChild();
        if (!translated)
            skipSuite("Process translation", "this machine cannot run x86_64 code");
        else {
            RefPtr<Process> process = Process::create(translated);
            TEST_ASSERT(process->isTranslated(), "a process running x86_64 code is translated");
            kill(translated, SIGKILL);
            int status = 0;
            while (waitpid(translated, &status, 0) < 0 && errno == EINTR) { }
        }
    }
#endif // CPU(ARM64)
}

} // namespace JSCToolsTest

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
