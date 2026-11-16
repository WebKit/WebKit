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

#pragma once

#include <glib-object.h>
#include <wpe/wpe-platform.h>
#include <wtf/HashSet.h>
#include <wtf/glib/GUniquePtr.h>

namespace TestWebKitAPI {

#define WPE_PLATFORM_TEST_FIXTURE(ClassName) \
    static void setUp(ClassName* fixture, gconstpointer) \
    { \
        new (fixture) ClassName; \
    } \
    static void tearDown(ClassName* fixture, gconstpointer) \
    { \
        fixture->~ClassName(); \
    } \
    static void add(const char* suiteName, const char* testName, void (*testFunc)(ClassName*, const void*)) \
    { \
        GUniquePtr<gchar> testPath(g_strdup_printf("/wpe-platform/%s/%s", suiteName, testName)); \
        g_test_add(testPath.get(), ClassName, 0, ClassName::setUp, testFunc, ClassName::tearDown); \
    }

class WPEPlatformTest {
public:
    WPE_PLATFORM_TEST_FIXTURE(WPEPlatformTest);

    WPEPlatformTest() = default;
    ~WPEPlatformTest();

    void assertObjectIsDeletedWhenTestFinishes(gpointer);

private:
    void checkWatchedObjects();
    static void objectFinalized(WPEPlatformTest*, GObject*);

    HashSet<GObject*> m_watchedObjects;
};

} // TestWebKitAPI
