/*
 * Copyright (C) 2017-2019 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeOutgoingVideoSource.h"

#if USE(LIBWEBRTC)

#include "Logging.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/MainThread.h>

namespace WebCore {

RealtimeOutgoingVideoSource::RealtimeOutgoingVideoSource(Ref<MediaStreamTrackPrivate>&& videoSource)
    : m_videoSource(WTFMove(videoSource))
    , m_blackFrameTimer(*this, &RealtimeOutgoingVideoSource::sendOneBlackFrame)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_videoSource->logger())
    , m_logIdentifier(m_videoSource->logIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

RealtimeOutgoingVideoSource::~RealtimeOutgoingVideoSource()
{
ASSERT(!m_videoSource->hasObserver(*this));
#if ASSERT_ENABLED
    Locker locker { m_sinksLock };
#endif
    ASSERT(m_sinks.isEmpty());
    stop();
}

void RealtimeOutgoingVideoSource::observeSource()
{
    ASSERT(!m_videoSource->hasObserver(*this));
    m_videoSource->addObserver(*this);
    initializeFromSource();
}

void RealtimeOutgoingVideoSource::unobserveSource()
{
    m_videoSource->removeObserver(*this);
    m_videoSource->source().removeVideoFrameObserver(*this);
}

void RealtimeOutgoingVideoSource::startObservingVideoFrames()
{
    if (m_maxFrameRate) {
        m_videoSource->source().addVideoFrameObserver(*this, { }, *m_maxFrameRate);
        return;
    }
    m_videoSource->source().addVideoFrameObserver(*this);
}

void RealtimeOutgoingVideoSource::setSource(Ref<MediaStreamTrackPrivate>&& newSource)
{
    ASSERT(isMainThread());
    ASSERT(!m_videoSource->hasObserver(*this));
    m_videoSource = WTFMove(newSource);

    ALWAYS_LOG(LOGIDENTIFIER, "track ", m_videoSource->logIdentifier());

    if (!m_areSinksAskingToApplyRotation)
        return;
    if (!m_videoSource->source().setShouldApplyRotation(true))
        m_shouldApplyRotation = true;
}

void RealtimeOutgoingVideoSource::applyRotation()
{
    ensureOnMainThread([this, protectedThis = Ref { *this }] {
        if (m_areSinksAskingToApplyRotation)
            return;

        m_areSinksAskingToApplyRotation = true;
        if (!m_videoSource->source().setShouldApplyRotation(true))
            m_shouldApplyRotation = true;
    });
}

void RealtimeOutgoingVideoSource::stop()
{
    ASSERT(isMainThread());
    unobserveSource();
    m_blackFrameTimer.stop();
}

void RealtimeOutgoingVideoSource::updateFramesSending()
{
    double videoFrameScaling = 1.0;
    if (m_maxPixelCount && *m_maxPixelCount > 0) {
        int counter = 0;
        while (videoFrameScaling * m_width * m_height > *m_maxPixelCount) {
            if (++counter % 2)
                videoFrameScaling *= 3.0 / 4.0;
            else
                videoFrameScaling *= 2.0 / 3.0;
        }
        if (videoFrameScaling != 1)
            videoFrameScaling = std::sqrt(videoFrameScaling);
    }
    m_videoFrameScaling = videoFrameScaling;

    if (!m_muted && m_enabled) {
        if (!m_isObservingVideoFrames) {
            m_isObservingVideoFrames = true;
            startObservingVideoFrames();
        }
        if (m_blackFrameTimer.isActive())
            m_blackFrameTimer.stop();
        return;
    }

    if (m_isObservingVideoFrames) {
        m_isObservingVideoFrames = false;
        m_videoSource->source().removeVideoFrameObserver(*this);
    }
    sendBlackFramesIfNeeded();
}

void RealtimeOutgoingVideoSource::sourceMutedChanged()
{
    ASSERT(m_muted != m_videoSource->muted());

    m_muted = m_videoSource->muted();

    updateFramesSending();
}

void RealtimeOutgoingVideoSource::sourceEnabledChanged()
{
    ASSERT(m_enabled != m_videoSource->enabled());

    m_enabled = m_videoSource->enabled();

    updateFramesSending();
}

void RealtimeOutgoingVideoSource::initializeFromSource()
{
    const auto& settings = m_videoSource->source().settings();
    m_width = settings.width();
    m_height = settings.height();

    m_muted = m_videoSource->muted();
    m_enabled = m_videoSource->enabled();

    updateFramesSending();
}

void RealtimeOutgoingVideoSource::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& sinkWants)
{
    ASSERT(!sinkWants.black_frames);

    if (sinkWants.rotation_applied)
        applyRotation();

    std::optional<double> maxFrameRate;
    if (sinkWants.max_framerate_fps != std::numeric_limits<int>::max())
        maxFrameRate = sinkWants.max_framerate_fps;
    std::optional<double> maxPixelCount;
    if (sinkWants.max_pixel_count != std::numeric_limits<int>::max())
        maxPixelCount = sinkWants.max_pixel_count;
    ensureOnMainThread([this, protectedThis = Ref { *this }, maxFrameRate, maxPixelCount] {
        if (m_maxFrameRate == maxFrameRate && m_maxPixelCount == maxPixelCount)
            return;
        m_maxFrameRate = maxFrameRate;
        m_maxPixelCount = maxPixelCount;
        if (!m_isObservingVideoFrames)
            return;
        m_videoSource->source().removeVideoFrameObserver(*this);
        m_isObservingVideoFrames = false;
        updateFramesSending();
    });

    Locker locker { m_sinksLock };
    m_sinks.add(sink);
}

void RealtimeOutgoingVideoSource::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    Locker locker { m_sinksLock };
    m_sinks.remove(sink);
}

void RealtimeOutgoingVideoSource::sendBlackFramesIfNeeded()
{
    if (m_blackFrameTimer.isActive())
        return;

    if (!m_muted && m_enabled)
        return;

    if (!m_width || !m_height)
        return;

    if (!m_blackFrame) {
        auto width = m_width;
        auto height = m_height;
        if (!m_shouldApplyRotation && (m_currentRotation == webrtc::kVideoRotation_270 || m_currentRotation == webrtc::kVideoRotation_90))
            std::swap(width, height);

        m_blackFrame = createBlackFrame(width, height);
        ASSERT(m_blackFrame);
        if (!m_blackFrame) {
            ALWAYS_LOG(LOGIDENTIFIER, "Unable to send black frames");
            return;
        }
    }
    sendOneBlackFrame();
    m_blackFrameTimer.startRepeating(1_s);
}

void RealtimeOutgoingVideoSource::sendOneBlackFrame()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>(m_blackFrame));
}

void RealtimeOutgoingVideoSource::sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer)
{
    MonotonicTime timestamp = MonotonicTime::now();
    webrtc::VideoFrame frame(buffer, m_shouldApplyRotation ? webrtc::kVideoRotation_0 : m_currentRotation, static_cast<int64_t>(timestamp.secondsSinceEpoch().microseconds()));

#if !RELEASE_LOG_DISABLED
    ++m_frameCount;

    auto delta = timestamp - m_lastFrameLogTime;
    if (!m_lastFrameLogTime || delta >= 1_s) {
        if (m_lastFrameLogTime) {
            INFO_LOG(LOGIDENTIFIER, m_frameCount, " frames sent in ", delta.value(), " seconds");
            m_frameCount = 0;
        }
        m_lastFrameLogTime = timestamp;
    }
#endif

    Locker locker { m_sinksLock };
    for (auto* sink : m_sinks)
        sink->OnFrame(frame);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RealtimeOutgoingVideoSource::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // USE(LIBWEBRTC)
