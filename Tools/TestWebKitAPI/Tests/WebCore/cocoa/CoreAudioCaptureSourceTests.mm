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

#if PLATFORM(MAC) && ENABLE(MEDIA_STREAM)

#include "Helpers/PlatformUtilities.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/CoreAudioCaptureUnit.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceCenter.h>

namespace TestWebKitAPI {

// Mock microphones 1 and 4 leave echo cancellation unset, so both capture through the shared unit.
static constexpr auto firstMicrophoneID = "239c24b0-2b15-11e3-8224-0800200c9a66"_s;
static constexpr auto secondMicrophoneID = "239c24b1-3b15-21e3-8224-0800200c9a66"_s;

static RefPtr<WebCore::RealtimeMediaSource> createMicrophoneSource(const String& persistentID)
{
    WebCore::CaptureDevice device { persistentID, WebCore::CaptureDevice::DeviceType::Microphone, emptyString(), emptyString(), true };
    return WebCore::RealtimeMediaSourceCenter::singleton().audioCaptureFactory().createAudioCaptureSource(device, { }, nullptr, { }).source();
}

TEST(CoreAudioCaptureSource, ResumedSourceCapturesFromItsOwnMicrophone)
{
#if ASSERT_ENABLED
    // Starting the shared unit asserts unless the process opted in, as the GPU process does.
    WebCore::CoreAudioCaptureUnit::allowStarting();
#endif
    WebCore::MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(true);

    RefPtr first = createMicrophoneSource(firstMicrophoneID);
    RefPtr second = createMicrophoneSource(secondMicrophoneID);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    auto& unit = WebCore::CoreAudioCaptureUnit::defaultSingleton();

    first->start();
    EXPECT_WK_STREQ(firstMicrophoneID, unit.persistentIDForTesting());
    first->stop();

    second->start();
    EXPECT_WK_STREQ(secondMicrophoneID, unit.persistentIDForTesting());
    second->stop();

    first->start();
    EXPECT_WK_STREQ(firstMicrophoneID, unit.persistentIDForTesting());

    first->stop();
    first->endImmediatly();
    second->endImmediatly();
    WebCore::MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(false);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(MEDIA_STREAM)
