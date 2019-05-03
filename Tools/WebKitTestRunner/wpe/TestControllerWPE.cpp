/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "TestController.h"

#include <glib.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTR {

void TestController::notifyDone()
{
    RunLoop::main().stop();
}

void TestController::setHidden(bool)
{
}

void TestController::platformInitialize()
{
}

WKPreferencesRef TestController::platformPreferences()
{
    return WKPageGroupGetPreferences(m_pageGroup.get());
}

void TestController::platformDestroy()
{
}

void TestController::platformInitializeContext()
{
}

void TestController::platformRunUntil(bool& done, WTF::Seconds timeout)
{
    struct TimeoutTimer {
        TimeoutTimer()
            : timer(RunLoop::main(), this, &TimeoutTimer::fired)
        { }

        void fired()
        {
            timedOut = true;
            RunLoop::main().stop();
        }

        RunLoop::Timer<TimeoutTimer> timer;
        bool timedOut { false };
    } timeoutTimer;

    timeoutTimer.timer.setPriority(G_PRIORITY_DEFAULT_IDLE);
    if (timeout >= 0_s)
        timeoutTimer.timer.startOneShot(timeout);

    while (!done && !timeoutTimer.timedOut)
        RunLoop::main().run();

    timeoutTimer.timer.stop();
}

void TestController::platformDidCommitLoadForFrame(WKPageRef, WKFrameRef)
{
}

static char* getEnvironmentVariableAsUTF8String(const char* variableName)
{
    const char* value = g_getenv(variableName);
    if (!value) {
        fprintf(stderr, "%s environment variable not found\n", variableName);
        exit(0);
    }
    gsize bytesWritten;
    return g_filename_to_utf8(value, -1, 0, &bytesWritten, 0);
}

void TestController::initializeInjectedBundlePath()
{
    GUniquePtr<char> path(getEnvironmentVariableAsUTF8String("TEST_RUNNER_INJECTED_BUNDLE_FILENAME"));
    m_injectedBundlePath.adopt(WKStringCreateWithUTF8CString(path.get()));
}

void TestController::initializeTestPluginDirectory()
{
}

void TestController::runModal(PlatformWebView*)
{
}

void TestController::abortModal()
{
}

WKContextRef TestController::platformContext()
{
    return m_context.get();
}

const char* TestController::platformLibraryPathForTesting()
{
    return nullptr;
}

void TestController::platformConfigureViewForTest(const TestInvocation&)
{
}

void TestController::platformResetPreferencesToConsistentValues()
{
}

void TestController::updatePlatformSpecificTestOptionsForTest(TestOptions&, const std::string&) const
{
}

} // namespace WTR
