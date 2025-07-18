/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#include "PlatformWebView.h"
#include <wtf/RunLoop.h>

namespace WTR {

void TestController::notifyDone()
{
    RunLoop::mainSingleton().stop();
}

void TestController::platformInitialize(const Options&)
{
}

void TestController::platformDestroy()
{
}

void TestController::platformRunUntil(bool& done, WTF::Seconds timeout)
{
    bool timedOut = false;
    class TimeoutTimer {
    public:
        TimeoutTimer(WTF::Seconds timeout, bool& timedOut)
            : m_timer(RunLoop::mainSingleton(), [&timedOut] {
                timedOut = true;
                RunLoop::mainSingleton().stop();
            })
        {
            if (timeout >= 0_s)
                m_timer.startOneShot(timeout);
        }
    private:
        RunLoop::Timer m_timer;
    } timeoutTimer(timeout, timedOut);

    while (!done && !timedOut)
        RunLoop::mainSingleton().run();
}

void TestController::initializeInjectedBundlePath()
{
    m_injectedBundlePath.adopt(WKStringCreateWithUTF8CString("/app0/libTestWebKitAPIInjectedBundle.sprx"));
}

void TestController::initializeTestPluginDirectory()
{
    m_testPluginDirectory.adopt(WKStringCreateWithUTF8CString(""));
}

void TestController::platformInitializeContext()
{
}

void TestController::runModal(PlatformWebView*)
{
}

void TestController::abortModal()
{
}

const char* TestController::platformLibraryPathForTesting()
{
    return "/hostapp";
}

void TestController::setHidden(bool hidden)
{
    if (!m_mainWebView)
        return;
    WKViewSetVisible(m_mainWebView->platformView(), !hidden);
}

WKContextRef TestController::platformContext()
{
    return m_context.get();
}

bool TestController::platformResetStateToConsistentValues(const TestOptions&)
{
    return true;
}

TestFeatures TestController::platformSpecificFeatureDefaultsForTest(const TestCommand&) const
{
    return { };
}

void TestController::platformConfigureViewForTest(const TestInvocation&)
{
    WKPageSetApplicationNameForUserAgent(mainWebView()->page(), WKStringCreateWithUTF8CString("WebKitTestRunnerPlayStation"));
}

} // namespace WTR
