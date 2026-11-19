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
#include "CorpseSymbolTest.h"

#if ENABLE(LIB_JSC_TOOLS)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/CorpseSymbol.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <spawn.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefPtr.h>
#include <wtf/WTFConfig.h>

// Exists in this binary but is not exported, so an exports trie cannot see it.
// A look up must report that rather than find it some other way.
extern "C" __attribute__((visibility("hidden"))) int jscToolsTestHiddenGlobal;
int jscToolsTestHiddenGlobal = 42;

namespace JSCToolsTest {

using JSC::Corpse::Address;
using JSC::Corpse::Process;
using JSC::Corpse::Snapshot;
using JSC::Corpse::Symbol;

static std::string helperPath()
{
    char buffer[PATH_MAX];
    uint32_t size = sizeof buffer;
    if (_NSGetExecutablePath(buffer, &size))
        return { };
    std::string path(buffer);
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return { };
    return path.substr(0, slash + 1) + "testLibJSCToolsHelper";
}

// By design testLibJSCToolsHelper does NOT link WTF and JSC so that a symbol lookup
// in it will not find g_config. We want to wait for it to reach its main() before commencing
// with taking a snapshot for the test. A snapshot taken too early could miss images dyld
// has yet to load, and hence, may pass the test with a false negative on the symbol load.
static pid_t spawnChildWithoutJavaScriptCore()
{
    std::string helper = helperPath();
    if (helper.empty())
        return 0;

    int readyPipe[2];
    if (pipe(readyPipe))
        return 0;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, readyPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, readyPipe[0]);
    posix_spawn_file_actions_addclose(&actions, readyPipe[1]);

    char* const arguments[] = { const_cast<char*>(helper.c_str()), nullptr };
    pid_t child = 0;
    int spawnError = posix_spawn(&child, helper.c_str(), &actions, nullptr, arguments, nullptr);
    posix_spawn_file_actions_destroy(&actions);
    // Closed here so a child that dies before notifying reads as EOF rather than blocking.
    close(readyPipe[1]);
    if (spawnError) {
        close(readyPipe[0]);
        return 0;
    }

    char ready = 0;
    ssize_t bytesRead;
    while ((bytesRead = read(readyPipe[0], &ready, 1)) < 0 && errno == EINTR) { }
    close(readyPipe[0]);
    if (bytesRead != 1) {
        killChild(child);
        return 0;
    }
    return child;
}

// A symbol resolved out of a corpse of this very process must land on the same
// address this process would use, because the corpse is a copy of this address
// space. That makes every look up below checkable against ground truth.
void testSymbol()
{
    SuiteTracer tracer("Symbol");
    if (!tracer.shouldRun())
        return;

    SelfSnapshot self;
    if (!self.isValid())
        return;
    Snapshot& snapshot = self.snapshot();

    {
        // A data symbol exported by the JavaScriptCore framework: an image that is
        // not in the shared cache, so this checks the slide from __TEXT's link-time
        // address to where the image actually landed.
        auto expected = reinterpret_cast<uintptr_t>(WebConfig::g_config);
        Address found = snapshot.symbol("g_config");
        TEST_ASSERT(found, "a symbol exported by JavaScriptCore is found");
        TEST_ASSERT_HEX_EQ(found.toMachVMAddress(), expected,
            "g_config resolves to the address this process uses for it");
    }
    {
        // Process w/o JavaScriptCore should not have a g_config.
        pid_t child = spawnChildWithoutJavaScriptCore();
        TEST_ASSERT(child, "a child without JavaScriptCore could be spawned");
        if (child) {
            RefPtr<Process> process = Process::create(child);
            TEST_ASSERT(process->attach(), "attaching to a process without JavaScriptCore succeeds");
            if (process->isAttached()) {
                Snapshot childSnapshot(process);
                TEST_ASSERT(childSnapshot.isValid(), "a snapshot of that process is valid");
                // Every process loads libSystem. So "tolower" should be in the findable set of symbols.
                TEST_ASSERT(childSnapshot.symbol("tolower"),
                    "a shared cache symbol is found in a process without JavaScriptCore");
                TEST_ASSERT(!childSnapshot.symbol("g_config"),
                    "a process without JavaScriptCore exports no g_config");
            }
            killChild(child);
        }
    }
    {
        // A function in the shared cache, where __LINKEDIT is shared between images
        // and the trie is reached by a different route.
        void* expected = dlsym(RTLD_DEFAULT, "tolower");
        TEST_ASSERT(expected, "tolower can be looked up locally");
        Address found = snapshot.symbol("tolower");
        TEST_ASSERT(found, "a symbol exported by a shared cache image is found");
        // A function pointer arrives signed on arm64e; only the address it names is
        // being compared here.
        TEST_ASSERT_HEX_EQ(found.stripped().toMachVMAddress(),
            Address(expected).stripped().toMachVMAddress(),
            "tolower resolves to the address this process uses for it");
    }
    {
        // A data symbol in the shared cache.
        void* expected = dlsym(RTLD_DEFAULT, "environ");
        if (!expected)
            skipSuite("Symbol environ", "this system does not export environ");
        else {
            Address found = snapshot.symbol("environ");
            TEST_ASSERT(found, "a data symbol in the shared cache is found");
            TEST_ASSERT_HEX_EQ(found.stripped().toMachVMAddress(),
                Address(expected).stripped().toMachVMAddress(),
                "environ resolves to the address this process uses for it");
        }
    }
    {
        TEST_ASSERT(!snapshot.symbol("jscToolsTestNoSuchSymbolAnywhere"),
            "a name that is not exported anywhere is not found");
        TEST_ASSERT(!snapshot.symbol(nullptr), "no name resolves to nothing");
        TEST_ASSERT(!snapshot.symbol(""), "an empty name resolves to nothing");
    }
    {
        // Only exported symbols appear in a trie. This one is in the binary, and
        // still must not be found: saying so is the honest answer.
        TEST_ASSERT(jscToolsTestHiddenGlobal == 42, "the hidden global is in this binary");
        TEST_ASSERT(!snapshot.symbol("jscToolsTestHiddenGlobal"),
            "a symbol hidden from the linker is not found");
    }
    {
        // A look up prepends the underscore that a Mach-O symbol name carries, so a
        // name that already has one is asking for a different symbol.
        TEST_ASSERT(!snapshot.symbol("_malloc"),
            "a name given with its underscore already attached is not found");
    }
    {
        // Resolving is expensive, so a snapshot keeps what it has resolved.
        Address first = snapshot.symbol("g_config");
        Address second = snapshot.symbol("g_config");
        TEST_ASSERT(first == second, "resolving the same name twice gives the same address");
    }
    {
        Symbol symbol(snapshot, "g_config");
        TEST_ASSERT(symbol.name() == "g_config", "a Symbol keeps the name it was asked for");
        TEST_ASSERT(symbol.isValid(), "a Symbol that resolved is valid");
        TEST_ASSERT(symbol.address() == snapshot.symbol("g_config"),
            "a Symbol resolves to what the snapshot reports");

        Symbol missing(snapshot, "jscToolsTestNoSuchSymbolAnywhere");
        TEST_ASSERT(!missing.isValid(), "a Symbol that did not resolve is not valid");
        TEST_ASSERT(!missing.address(), "a Symbol that did not resolve has no address");
        TEST_ASSERT(missing.name() == "jscToolsTestNoSuchSymbolAnywhere",
            "a Symbol that did not resolve still knows its name");

        Symbol unnamed(snapshot, nullptr);
        TEST_ASSERT(unnamed.name().empty(), "a Symbol with no name has an empty name");
        TEST_ASSERT(!unnamed.isValid(), "a Symbol with no name is not valid");
    }
    {
        // A name that is nowhere walks every image in the corpse, which is the most
        // work a look up can be asked to do. It has to stay bounded.
        static constexpr double budgetSeconds = 60;
        MonotonicTime start = MonotonicTime::now();
        TEST_ASSERT(!snapshot.symbol("jscToolsTestAnotherNameThatIsNowhere"),
            "an absent name is reported absent");
        double elapsed = (MonotonicTime::now() - start).seconds();
        TEST_ASSERT(elapsed < budgetSeconds, "a look up that finds nothing still finishes");
        if (elapsed >= budgetSeconds)
            dataLogLn("    the search took ", elapsed, " seconds");
    }
}

} // namespace JSCToolsTest

#endif // ENABLE(LIB_JSC_TOOLS)
