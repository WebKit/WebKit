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

#if USE(AVFOUNDATION)

#include "Helpers/PlatformUtilities.h"
#include <WebCore/AudioVideoRendererAVFObjC.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/TrackInfo.h>
#include <wtf/Logger.h>

using namespace WebCore;

namespace TestWebKitAPI {

class AudioVideoRendererAVFObjCTest : public testing::Test {
public:
    void SetUp() final
    {
        Ref logger = Logger::create(this);
        renderer = AudioVideoRendererAVFObjC::create(logger, 0);
        renderer->setPreferences({ VideoRendererPreference::PrefersDecompressionSession });
        renderer->notifyWhenRequiresFlushToResume([this] {
            ++flushToResumeCount;
        });
        renderer->addTrack(TrackInfo::TrackType::Video);
    }

    void TearDown() final
    {
        renderer = nullptr;
    }

    RefPtr<AudioVideoRendererAVFObjC> renderer;
    int flushToResumeCount { 0 };
};

TEST_F(AudioVideoRendererAVFObjCTest, FlushToResumeOnVisibilityRestore)
{
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    renderer->renderingCanBeAcceleratedChanged(false);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    renderer->renderingCanBeAcceleratedChanged(true);
    bool callbackFired = Util::waitFor([this] {
        return flushToResumeCount > 0;
    });
    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(flushToResumeCount, 1);
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION)
