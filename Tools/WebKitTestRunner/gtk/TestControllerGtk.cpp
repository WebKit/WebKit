/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L.
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
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestController.h"

#include <gtk/gtk.h>
#include <wtf/Platform.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WTR {

static guint gTimeoutSourceId = 0;

static void cancelTimeout()
{
    if (!gTimeoutSourceId)
        return;
    g_source_remove(gTimeoutSourceId);
    gTimeoutSourceId = 0;
}

void TestController::notifyDone()
{
    gtk_main_quit();
    cancelTimeout();
}

void TestController::platformInitialize()
{
}

static gboolean timeoutCallback(gpointer)
{
    fprintf(stderr, "FAIL: TestControllerRunLoop timed out.\n");
    gtk_main_quit();
    return FALSE;
}

void TestController::platformRunUntil(bool&, double timeout)
{
    cancelTimeout();
    gTimeoutSourceId = g_timeout_add(timeout * 1000, timeoutCallback, 0);
    gtk_main();
}

void TestController::initializeInjectedBundlePath()
{
    const char* bundlePath = g_getenv("TEST_RUNNER_INJECTED_BUNDLE_FILENAME");
    if (!bundlePath) {
        fprintf(stderr, "TEST_RUNNER_INJECTED_BUNDLE_FILENAME environment variable not found\n");
        exit(0);
    }

    gsize bytesWritten;
    GOwnPtr<char> utf8BundlePath(g_filename_to_utf8(bundlePath, -1, 0, &bytesWritten, 0));
    m_injectedBundlePath.adopt(WKStringCreateWithUTF8CString(utf8BundlePath.get()));
}

void TestController::initializeTestPluginDirectory()
{
    // This is called after initializeInjectedBundlePath.
    ASSERT(m_injectedBundlePath);
    m_testPluginDirectory = m_injectedBundlePath;
}

void TestController::platformInitializeContext()
{
}

void TestController::runModal(PlatformWebView*)
{
    // FIXME: Need to implement this to test showModalDialog.
}

const char* TestController::platformLibraryPathForTesting()
{
    return 0;
}

} // namespace WTR
