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

#include "PlatformWebView.h"
#include <glib.h>
#include <wtf/RunLoop.h>
#include <wtf/WTFProcess.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkData.h>
#include <skia/encode/SkPngEncoder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WTR {

void TestController::notifyDone()
{
    RunLoop::mainSingleton().stop();
}

void TestController::setHidden(bool)
{
}

void TestController::platformInitialize(const Options&)
{
}

void TestController::platformDestroy()
{
}

void TestController::platformInitializeContext()
{
}

void TestController::platformRunUntil(bool& done, WTF::Seconds timeout)
{
    bool timedOut = false;
    class TimeoutTimer {
    public:
        TimeoutTimer(WTF::Seconds timeout, bool& timedOut)
            : m_timer(RunLoop::mainSingleton(), "TestController::TimeoutTimer::Timer"_s, [&timedOut] {
                timedOut = true;
                RunLoop::mainSingleton().stop();
            })
        {
            m_timer.setPriority(G_PRIORITY_DEFAULT_IDLE);
            if (timeout >= 0_s)
                m_timer.startOneShot(timeout);
        }
    private:
        RunLoop::Timer m_timer;
    } timeoutTimer(timeout, timedOut);

    while (!done && !timedOut)
        RunLoop::mainSingleton().run();
}

void TestController::platformDidCommitLoadForFrame(WKPageRef, WKFrameRef)
{
}

static char* getEnvironmentVariableAsUTF8String(const char* variableName)
{
    const char* value = g_getenv(variableName);
    if (!value) {
        fprintf(stderr, "%s environment variable not found\n", variableName);
        exitProcess(0);
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

const char* TestController::platformLibraryPathForTesting()
{
    return nullptr;
}

void TestController::platformConfigureViewForTest(const TestInvocation&)
{
    WKRetainPtr<WKStringRef> appName = adoptWK(WKStringCreateWithUTF8CString("WebKitTestRunnerWPE"));
    WKPageSetApplicationNameForUserAgent(mainWebView()->page(), appName.get());
}

bool TestController::platformResetStateToConsistentValues(const TestOptions&)
{
    return true;
}

TestFeatures TestController::platformSpecificFeatureDefaultsForTest(const TestCommand&) const
{
    TestFeatures features;
    features.boolWebPreferenceFeatures.insert({ "AsyncOverflowScrollingEnabled", true });
    return features;
}

WKRetainPtr<WKStringRef> TestController::takeViewPortSnapshot()
{
    sk_sp<SkImage> image(mainWebView()->windowSnapshotImage());
    auto data = SkPngEncoder::Encode(nullptr, image.get(), { });
    auto uri = makeString("data:image/png;base64,"_s, base64Encoded(std::span { static_cast<const uint8_t*>(data->data()), data->size() }));
    return adoptWK(WKStringCreateWithUTF8CString(uri.utf8().data()));
}

} // namespace WTR
