/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEPlatformTest.h"

#include <wtf/FileSystem.h>

namespace TestWebKitAPI {
void beforeAll();
void afterAll();
}

int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, nullptr);

    g_set_prgname(FileSystem::currentExecutableName().data());
    g_setenv("LC_ALL", "C", TRUE);
    g_setenv("GIO_USE_VFS", "local", TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    g_setenv("G_ENABLE_DIAGNOSTIC", "0", TRUE);
    g_setenv("TZ", "America/Los_Angeles", TRUE);
    g_setenv("WPE_PLATFORMS_PATH", WPE_MOCK_PLATFORM_DIR, TRUE);
    g_test_bug_base("https://bugs.webkit.org/");

    TestWebKitAPI::beforeAll();
    int returnValue = g_test_run();
    TestWebKitAPI::afterAll();

    return returnValue;
}
