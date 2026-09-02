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
#include "CorpseSnapshotTest.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <mach/mach.h>
#include <unistd.h>

namespace JSCToolsTest {

using JSC::Corpse::Process;
using JSC::Corpse::Snapshot;

void testSnapshot()
{
    SuiteTracer tracer("Snapshot");
    if (!tracer.shouldRun())
        return;

    RefPtr<Process> process = Process::create(getpid());
    if (!process->attach()) {
        TEST_ASSERT(false, "attaching to this process succeeds");
        return;
    }

    unsigned firstId = 0;
    {
        Snapshot snapshot(process);
        TEST_ASSERT(snapshot.isValid(), "a snapshot of this process is valid");
        TEST_ASSERT(MACH_PORT_VALID(snapshot.corpsePort()), "a valid snapshot holds a corpse port");
        TEST_ASSERT(snapshot.process() == process.get(), "a snapshot keeps the process it came from");
        firstId = snapshot.id();
        TEST_ASSERT(firstId, "a snapshot has an identifier");

        Snapshot second(process);
        TEST_ASSERT(second.isValid(), "a second snapshot of the same process is valid");
        TEST_ASSERT(second.id() > firstId, "identifiers increase");
        TEST_ASSERT(second.corpsePort() != snapshot.corpsePort(),
            "two snapshots hold two different corpses");
    }
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

    {
        // A corpse and the threads read out of it are Mach ports. Taking many
        // snapshots must not leave any of them behind.
        static constexpr unsigned rounds = 100;
        unsigned before = machPortNameCount();
        for (unsigned round = 0; round < rounds; ++round) {
            Snapshot snapshot(process);
            if (!snapshot.isValid())
                continue;
            snapshot.threads();
        }
        unsigned after = machPortNameCount();
        // A handful of names may come and go for reasons of their own; a leak of
        // one port per round would be a hundred.
        static constexpr unsigned allowedDrift = 8;
        TEST_ASSERT(after <= before + allowedDrift, "taking and dropping snapshots leaks no Mach port");
        if (after > before + allowedDrift)
            dataLogLn("    port names before ", before, ", after ", after, ", over ", rounds, " snapshots");
    }
}

} // namespace JSCToolsTest

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
