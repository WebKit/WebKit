/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

#include <atomic>
#include <bmalloc/ThreadSuspend.h>
#include <chrono>
#include <thread>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

#if !OS(DARWIN) && !OS(WINDOWS)
#include <JavaScriptCore/JSBasePrivate.h>
#include <cstdlib>
#include <signal.h>
#include <span>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#endif

namespace TestWebKitAPI {

#if !OS(DARWIN) && !OS(WINDOWS)

// JSConfigureSignalForGC() and the JSC_SIGNAL_FOR_GC environment variable must
// both take effect *before* WTF is initialized. We can't satisfy that from inside
// an EXPECT_EXIT body: the "threadsafe" death-test style re-execs this binary, but
// TestsController initializes WTF (WTF::initializeMainThread()) during main(),
// before any test body runs -- so configuration done in the body is too late.
//
// Instead we hand the configuration to the re-exec'd child through the
// environment, where it is applied before WTF initialization:
//   - JSC_SIGNAL_FOR_GC is read by WTF initialization itself.
//   - The programmatic JSConfigureSignalForGC() path is applied from a process
//     constructor (which runs before main(), hence before WTF init), keyed on the
//     JSC_TEST_CONFIGURE_GC_SIGNAL variable below.
// The parent sets the relevant variable before EXPECT_EXIT; the child inherits and
// applies it. WebConfig::g_config is plain zero-initialized storage, so touching
// g_wtfConfig from JSConfigureSignalForGC this early -- exactly as a real embedder
// would, before JSC initialization -- is safe.

static constexpr auto programmaticConfigureEnvVar = "JSC_TEST_CONFIGURE_GC_SIGNAL";
static constexpr auto productionSignalEnvVar = "JSC_SIGNAL_FOR_GC";

__attribute__((constructor)) static void configureSignalForGCBeforeInitialization()
{
    if (const char* signalString = getenv(programmaticConfigureEnvVar))
        JSConfigureSignalForGC(parseInteger<int>(StringView::fromLatin1(signalString)).value_or(0));
}

static void setSignalEnvironmentVariable(const char* name, int signal)
{
    char buffer[16];
    SAFE_SPRINTF(std::span { buffer }, "%d", signal);
    setenv(name, buffer, /* overwrite */ 1);
}

TEST(JavaScriptCore_ConfigureSignalForGC, SignalReachesLibpas)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    // Ask the re-exec'd child's constructor to call JSConfigureSignalForGC(SIGUSR2)
    // before WTF is initialized.
    unsetenv(productionSignalEnvVar);
    setSignalEnvironmentVariable(programmaticConfigureEnvVar, SIGUSR2);
    EXPECT_EXIT({
        WTF::initialize();
        std::exit(bmalloc::api::threadSuspendSignalNumber() == SIGUSR2 ? 0 : 2);
    }, ::testing::ExitedWithCode(0), ".*");
    unsetenv(programmaticConfigureEnvVar);
}

TEST(JavaScriptCore_ConfigureSignalForGC, FailsAfterInitialize)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    unsetenv(programmaticConfigureEnvVar);
    unsetenv(productionSignalEnvVar);
    EXPECT_EXIT({
        WTF::initialize();
        // Initialization has already locked the signal in; further configuration must fail.
        std::exit(JSConfigureSignalForGC(SIGUSR2) ? 1 : 0);
    }, ::testing::ExitedWithCode(0), ".*");
}

TEST(JavaScriptCore_ConfigureSignalForGC, EnvVarHonored)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    // The child inherits JSC_SIGNAL_FOR_GC and WTF initialization reads it.
    unsetenv(programmaticConfigureEnvVar);
    setSignalEnvironmentVariable(productionSignalEnvVar, SIGUSR2);
    EXPECT_EXIT({
        WTF::initialize();
        std::exit(bmalloc::api::threadSuspendSignalNumber() == SIGUSR2 ? 0 : 2);
    }, ::testing::ExitedWithCode(0), ".*");
    unsetenv(productionSignalEnvVar);
}

TEST(JavaScriptCore_ConfigureSignalForGC, DefaultIsSIGUSR1)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    unsetenv(programmaticConfigureEnvVar);
    unsetenv(productionSignalEnvVar);
    EXPECT_EXIT({
        WTF::initialize();
        std::exit(bmalloc::api::threadSuspendSignalNumber() == SIGUSR1 ? 0 : 2);
    }, ::testing::ExitedWithCode(0), ".*");
}

#endif // !OS(DARWIN) && !OS(WINDOWS)

} // namespace TestWebKitAPI
