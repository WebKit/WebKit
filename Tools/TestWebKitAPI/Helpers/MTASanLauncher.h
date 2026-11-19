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

#include <wtf/Platform.h>

#if !PLATFORM(COCOA) || !__has_feature(modules)

#include "Helpers/Test.h"

#if CPU(ARM64) && OS(DARWIN)

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <wtf/text/MakeString.h>

extern char** environ;

namespace TestWebKitAPI {

constexpr auto mtaSanChildEnvironmentVariable = "TESTWEBKITAPI_MTASAN_CHILD";

// Whether this process was spawned as MT-ASan by runCurrentTestInMTASanChild().
inline bool isMTASanChild()
{
    return getenv(mtaSanChildEnvironmentVariable);
}

// Whether a process is MT-ASan is decided when it is spawned. A test that needs one calls this when it
// finds itself in an ordinary process: it runs the current test again in a child spawned as MT-ASan,
// and fails if the child fails. The caller should return afterwards. Skips without MTE hardware.
inline void runCurrentTestInMTASanChild()
{
    constexpr uint32_t sanitizersConfigMTASan = 0x1; // SANITIZERS_CONFIG_MTASAN

    int hasMTE = 0;
    size_t hasMTESize = sizeof(hasMTE);
    if (sysctlbyname("hw.optional.arm.FEAT_MTE", &hasMTE, &hasMTESize, nullptr, 0) || !hasMTE)
        GTEST_SKIP() << "Requires hardware with MTE";

    // The SPI is only on newer OSes, so look it up instead of linking against it.
    using SetUseSanitizersFunction = int (*)(posix_spawnattr_t*, uint32_t);
    auto setUseSanitizers = std::bit_cast<SetUseSanitizersFunction>(dlsym(RTLD_DEFAULT, "posix_spawnattr_set_use_sanitizers_np"));
    if (!setUseSanitizers)
        GTEST_SKIP() << "Requires an OS with posix_spawnattr_set_use_sanitizers_np";

    auto* testInfo = testing::UnitTest::GetInstance()->current_test_info();
    auto filter = makeString("--gtest_filter="_s, unsafeSpan(testInfo->test_suite_name()), '.', unsafeSpan(testInfo->name())).ascii();

    char path[PATH_MAX];
    uint32_t pathSize = sizeof(path);
    ASSERT_EQ(_NSGetExecutablePath(path, &pathSize), 0);
    char* arguments[] = { path, const_cast<char*>(filter.data()), nullptr };

    posix_spawnattr_t attributes;
    ASSERT_EQ(posix_spawnattr_init(&attributes), 0);
    int error = setUseSanitizers(&attributes, sanitizersConfigMTASan);
    pid_t pid = 0;
    if (!error) {
        setenv(mtaSanChildEnvironmentVariable, "1", 1);
        error = posix_spawn(&pid, path, nullptr, &attributes, arguments, environ);
        unsetenv(mtaSanChildEnvironmentVariable);
    }
    posix_spawnattr_destroy(&attributes);
    ASSERT_EQ(error, 0);

    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    EXPECT_TRUE(WIFEXITED(status) && !WEXITSTATUS(status)) << "The MT-ASan child failed with wait status " << status;
}

} // namespace TestWebKitAPI

#endif // CPU(ARM64) && OS(DARWIN)

#endif // !PLATFORM(COCOA) || !__has_feature(modules)
