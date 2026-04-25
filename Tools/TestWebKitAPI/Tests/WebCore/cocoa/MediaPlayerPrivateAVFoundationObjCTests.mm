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

#include "Helpers/PlatformUtilities.h"
#include "Helpers/cocoa/TestNSBundleExtras.h"
#include "WebCoreTestSupport.h"
#include <WebCore/MediaPlayerPrivateAVFoundationObjC.h>
#include <wtf/Identified.h>

namespace TestWebKitAPI {

class MediaPlayerPrivateAVFoundationObjCTest
    : public testing::Test
    , public WebCore::MediaPlayerClient
    , public Identified<WebCore::MediaPlayerClientIdentifier> {
public:
    // Pretend to be reference counted:
    void ref() const final { }
    void deref() const final { }

    // MediaPlayerPrivateInterface:
    WebCore::MediaPlayerClientIdentifier mediaPlayerClientIdentifier() const { return identifier(); }

    // testing::Test:
    void SetUp() final
    {
        mediaPlayer = WebCore::MediaPlayer::create(*this, WebCore::MediaPlayerMediaEngineIdentifier::AVFoundation);
    }

    void TearDown() final
    {
        mediaPlayer = nullptr;
    }

    // Utility methods:
    RefPtr<WebCore::MediaPlayerPrivateAVFoundationObjC> playerPrivate() const
    {
        if (mediaPlayer)
            return dynamicDowncast<WebCore::MediaPlayerPrivateAVFoundationObjC>(mediaPlayer->playerPrivate());
        return nullptr;
    }

    URL videoWithAudioURL() const
    {
        return [NSBundle.test_resourcesBundle URLForResource:@"video-with-audio" withExtension:@"mp4"];
    }

    URL videoWithoutAudioURL() const
    {
        return [NSBundle.test_resourcesBundle URLForResource:@"video-without-audio" withExtension:@"mp4"];
    }

    void waitForReadyStateGreaterThan(WebCore::MediaPlayerReadyState readyState)
    {
        Util::waitFor([&] { return mediaPlayer->readyState() > readyState; });
        EXPECT_GE(mediaPlayer->readyState(), readyState);
    }

    void waitForCurrentTimeGreaterThan(MediaTime time)
    {
        Util::waitFor([&] { return mediaPlayer->currentTime() > time; });
        EXPECT_GE(mediaPlayer->currentTime(), time);
    }

    void waitForEffectiveRateGreaterThan(double rate)
    {
        Util::waitFor([&] { return mediaPlayer->effectiveRate() > rate; });
        EXPECT_GE(mediaPlayer->effectiveRate(), rate);
    }

    void waitForEffectiveRateEqualTo(double rate)
    {
        Util::waitFor([&] { return mediaPlayer->effectiveRate() == rate; });
        EXPECT_EQ(mediaPlayer->effectiveRate(), rate);
    }

    RefPtr<WebCore::MediaPlayer> mediaPlayer;
};

TEST_F(MediaPlayerPrivateAVFoundationObjCTest, Basic)
{
    mediaPlayer->load(videoWithAudioURL(), { });
    waitForReadyStateGreaterThan(WebCore::MediaPlayerReadyState::HaveCurrentData);
}

TEST_F(MediaPlayerPrivateAVFoundationObjCTest, IsAudible)
{
    mediaPlayer->load(videoWithAudioURL(), { });
    waitForReadyStateGreaterThan(WebCore::MediaPlayerReadyState::HaveNothing);

    RefPtr playerPrivate = this->playerPrivate();
    ASSERT_NE(playerPrivate.get(), nullptr);

    mediaPlayer->setMuted(false);
    mediaPlayer->setVolume(1);
    EXPECT_EQ(playerPrivate->isAudible(), true);

    mediaPlayer->setMuted(true);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setMuted(false);
    EXPECT_EQ(playerPrivate->isAudible(), true);

    mediaPlayer->setVolume(0);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setVolume(1);
    EXPECT_EQ(playerPrivate->isAudible(), true);

    mediaPlayer->setMuted(true);
    mediaPlayer->setVolume(0);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setMuted(false);
    mediaPlayer->setVolume(1);
    EXPECT_EQ(playerPrivate->isAudible(), true);

    mediaPlayer->setVolume(0);
    mediaPlayer->setMuted(true);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setVolume(1);
    mediaPlayer->setMuted(false);
    EXPECT_EQ(playerPrivate->isAudible(), true);
}

TEST_F(MediaPlayerPrivateAVFoundationObjCTest, IsAudibleSilentVideo)
{
    mediaPlayer->load(videoWithoutAudioURL(), { });
    waitForReadyStateGreaterThan(WebCore::MediaPlayerReadyState::HaveNothing);

    RefPtr playerPrivate = this->playerPrivate();
    ASSERT_NE(playerPrivate.get(), nullptr);

    mediaPlayer->setMuted(false);
    mediaPlayer->setVolume(1);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setMuted(true);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setMuted(false);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setVolume(0);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->setVolume(1);
    EXPECT_EQ(playerPrivate->isAudible(), false);
}

TEST_F(MediaPlayerPrivateAVFoundationObjCTest, IsAudibleWhilePlaying)
{
    mediaPlayer->load(videoWithAudioURL(), { });
    waitForReadyStateGreaterThan(WebCore::MediaPlayerReadyState::HaveNothing);

    RefPtr playerPrivate = this->playerPrivate();
    ASSERT_NE(playerPrivate.get(), nullptr);

    // Reduce the possibility of this test flaking when the underlying
    // media reaches the end by reducing the playback rate to 1/10000.
    // Calling setPreservesPitch(false) also has the effect of shifting
    // all generated frequencies to nearly inaudible ranges.
    mediaPlayer->setRate(0.0001);
    mediaPlayer->setPreservesPitch(false);
    mediaPlayer->setMuted(false);
    mediaPlayer->setVolume(1);

    mediaPlayer->play();
    waitForEffectiveRateGreaterThan(0);
    EXPECT_EQ(playerPrivate->isAudible(), true);

    // isAudible() should not become false during playback
    mediaPlayer->setMuted(true);
    EXPECT_EQ(playerPrivate->isAudible(), true);
    mediaPlayer->setVolume(0);
    EXPECT_EQ(playerPrivate->isAudible(), true);

    // isAudible() should subsequently become false once playback stops
    mediaPlayer->pause();
    waitForEffectiveRateEqualTo(0);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    mediaPlayer->play();
    waitForEffectiveRateGreaterThan(0);
    EXPECT_EQ(playerPrivate->isAudible(), false);

    // isAudible() should become true during playback
    mediaPlayer->setMuted(false);
    mediaPlayer->setVolume(1);
    EXPECT_EQ(playerPrivate->isAudible(), true);
}

}
