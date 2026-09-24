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

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#if OS(DARWIN)
#include <mach/mach.h>
#endif
#include <unistd.h>

namespace JSCToolsTest {

using JSC::Corpse::Process;
using JSC::Corpse::Snapshot;
using JSC::Corpse::TaskHandle;

void testSnapshot()
{
    SuiteTracer tracer("Snapshot");
    if (!tracer.shouldRun())
        return;
    if (linuxSkip("Snapshot", "corpses are not implemented on Linux yet"))
        return;

    RefPtr<Process> process = Process::create(getpid());
    if (!process->attach()) {
        TEST_ASSERT(false, "attaching to this process succeeds");
        return;
    }

    unsigned firstId = 0;
    TaskHandle firstCorpsePort = JSC::Corpse::invalidTaskHandle;
    TaskHandle secondCorpsePort = JSC::Corpse::invalidTaskHandle;
    {
        Snapshot snapshot(process);
        TEST_ASSERT(snapshot.isValid(), "a snapshot of this process is valid");
        TEST_ASSERT(JSC::Corpse::isValidTaskHandle(snapshot.corpsePort()), "a valid snapshot holds a corpse port");
        TEST_ASSERT(snapshot.process() == process.get(), "a snapshot keeps the process it came from");
        firstId = snapshot.id();
        TEST_ASSERT(firstId, "a snapshot has an identifier");
        firstCorpsePort = snapshot.corpsePort();

        Snapshot second(process);
        TEST_ASSERT(second.isValid(), "a second snapshot of the same process is valid");
        TEST_ASSERT(second.id() > firstId, "identifiers increase");
        TEST_ASSERT(second.corpsePort() != snapshot.corpsePort(),
            "two snapshots hold two different corpses");
        secondCorpsePort = second.corpsePort();
    }
#if OS(DARWIN)
    TEST_ASSERT_EQ(machPortSendRightCount(firstCorpsePort), 0u,
        "destroying a snapshot gives its corpse port back");
    TEST_ASSERT_EQ(machPortSendRightCount(secondCorpsePort), 0u,
        "and so does destroying the second");
#else
    UNUSED_VARIABLE(firstCorpsePort);
    UNUSED_VARIABLE(secondCorpsePort);
#endif
    {
        // The two above are gone; their identifiers must not come back.
        Snapshot later(process);
        TEST_ASSERT(later.id() > firstId + 1, "identifiers are not reused after a snapshot is destroyed");
    }
    {
        RefPtr<Process> unattached = Process::create(getpid());
        Snapshot snapshot(unattached);
        TEST_ASSERT(!snapshot.isValid(), "a snapshot of an unattached process is invalid");
        TEST_ASSERT(snapshot.threads().isEmpty(), "an invalid snapshot reports no threads");
        TEST_ASSERT(!snapshot.symbol("g_config"), "an invalid snapshot resolves no symbol");
    }
    {
        RefPtr<Process> none;
        Snapshot snapshot(none);
        TEST_ASSERT(!snapshot.isValid(), "a snapshot with no process is invalid");
    }
    {
        Snapshot snapshot(process);
        TEST_ASSERT(!snapshot.symbol(nullptr), "an unnamed symbol resolves to nothing");
        TEST_ASSERT(!snapshot.symbol(""), "an empty symbol name resolves to nothing");
    }

#if OS(DARWIN)
    {
        // A corpse and the thread rights read out of it are Mach ports. Taking a snapshot
        // must not leave any of them behind.
        unsigned namesBefore = machPortNameCount();
        mach_port_t corpsePort = MACH_PORT_NULL;
        {
            Snapshot snapshot(process);
            if (!snapshot.isValid()) {
                TEST_ASSERT(false, "a snapshot of this process is valid");
                return;
            }
            corpsePort = snapshot.corpsePort();
            TEST_ASSERT(machPortSendRightCount(corpsePort), "a snapshot holds a right to its corpse");

            unsigned namesBeforeThreads = machPortNameCount();
            snapshot.threads();
            TEST_ASSERT_EQ(machPortNameCount(), namesBeforeThreads,
                "reading the thread list gives back every thread right it took");
        }

        TEST_ASSERT_EQ(machPortSendRightCount(corpsePort), static_cast<unsigned>(0),
            "destroying a snapshot gives back the right to its corpse");
        TEST_ASSERT_EQ(machPortNameCount(), namesBefore,
            "and leaves no port name behind");
    }
#endif // OS(DARWIN)
}

} // namespace JSCToolsTest

#endif // ENABLE(MYA)
