/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivate.h"
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>

namespace WebCore {

class MockMediaPlayerMediaSource;
class MockSourceBufferPrivate;

class MockMediaSourcePrivate final
    : public MediaSourcePrivate
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MockMediaSourcePrivate> create(MockMediaPlayerMediaSource&, MediaSourcePrivateClient&);
    virtual ~MockMediaSourcePrivate();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::Mock; }

    WeakPtr<MockMediaPlayerMediaSource> player() const { return m_player; }

    MediaTime currentMediaTime() const final;

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics();

    void incrementTotalVideoFrames() { ++m_totalVideoFrames; }
    void incrementDroppedFrames() { ++m_droppedVideoFrames; }
    void incrementCorruptedFrames() { ++m_corruptedVideoFrames; }
    void incrementTotalFrameDelayBy(const MediaTime& delay) { m_totalFrameDelay += delay; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "MockMediaSourcePrivate"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    const void* nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

private:
    MockMediaSourcePrivate(MockMediaPlayerMediaSource&, MediaSourcePrivateClient&);

    // MediaSourcePrivate Overrides
    AddStatus addSourceBuffer(const ContentType&, bool webMParserEnabled, RefPtr<SourceBufferPrivate>&) override;
    void durationChanged(const MediaTime&) override;
    void markEndOfStream(EndOfStreamStatus) override;

    MediaPlayer::ReadyState mediaPlayerReadyState() const override;
    void setMediaPlayerReadyState(MediaPlayer::ReadyState) override;

    void notifyActiveSourceBuffersChanged() final;

    friend class MockSourceBufferPrivate;

    WeakPtr<MockMediaPlayerMediaSource> m_player;

    unsigned m_totalVideoFrames { 0 };
    unsigned m_droppedVideoFrames { 0 };
    unsigned m_corruptedVideoFrames { 0 };
    MediaTime m_totalFrameDelay;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextSourceBufferID { 0 };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MockMediaSourcePrivate)
static bool isType(const WebCore::MediaSourcePrivate& mediaSource) { return mediaSource.platformType() == WebCore::MediaPlatformType::Mock; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE)
