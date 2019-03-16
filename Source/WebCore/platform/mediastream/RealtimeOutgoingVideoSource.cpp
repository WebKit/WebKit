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
}

RealtimeOutgoingVideoSource::~RealtimeOutgoingVideoSource()
{
    ASSERT(m_sinks.isEmpty());
    stop();
}

void RealtimeOutgoingVideoSource::observeSource()
{
    m_videoSource->addObserver(*this);
    initializeFromSource();
}

void RealtimeOutgoingVideoSource::unobserveSource()
{
    m_videoSource->removeObserver(*this);
}

bool RealtimeOutgoingVideoSource::setSource(Ref<MediaStreamTrackPrivate>&& newSource)
{
    if (!m_initialSettings)
        m_initialSettings = m_videoSource->source().settings();

    auto locker = holdLock(m_sinksLock);
    bool hasSinks = !m_sinks.isEmpty();

    if (hasSinks)
        unobserveSource();
    m_videoSource = WTFMove(newSource);
    if (hasSinks)
        observeSource();

    return true;
}

void RealtimeOutgoingVideoSource::stop()
{
    ASSERT(isMainThread());
    unobserveSource();
    m_blackFrameTimer.stop();
}

void RealtimeOutgoingVideoSource::updateBlackFramesSending()
{
    if (!m_muted && m_enabled) {
        if (m_blackFrameTimer.isActive())
            m_blackFrameTimer.stop();
        return;
    }

    sendBlackFramesIfNeeded();
}

void RealtimeOutgoingVideoSource::sourceMutedChanged()
{
    ASSERT(m_muted != m_videoSource->muted());

    m_muted = m_videoSource->muted();

    updateBlackFramesSending();
}

void RealtimeOutgoingVideoSource::sourceEnabledChanged()
{
    ASSERT(m_enabled != m_videoSource->enabled());

    m_enabled = m_videoSource->enabled();

    updateBlackFramesSending();
}

void RealtimeOutgoingVideoSource::initializeFromSource()
{
    const auto& settings = m_videoSource->source().settings();
    m_width = settings.width();
    m_height = settings.height();

    m_muted = m_videoSource->muted();
    m_enabled = m_videoSource->enabled();

    updateBlackFramesSending();
}

void RealtimeOutgoingVideoSource::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& sinkWants)
{
    ASSERT(!sinkWants.black_frames);

    if (sinkWants.rotation_applied)
        m_shouldApplyRotation = true;

    {
    auto locker = holdLock(m_sinksLock);
    if (!m_sinks.add(sink) || m_sinks.size() != 1)
        return;
    }

    callOnMainThread([protectedThis = makeRef(*this)]() {
        protectedThis->observeSource();
    });
}

void RealtimeOutgoingVideoSource::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    {
    auto locker = holdLock(m_sinksLock);

    if (!m_sinks.remove(sink) || m_sinks.size())
        return;
    }

    unobserveSource();

    callOnMainThread([protectedThis = makeRef(*this)]() {
        if (protectedThis->m_blackFrameTimer.isActive())
            protectedThis->m_blackFrameTimer.stop();
    });
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
        if (m_shouldApplyRotation && (m_currentRotation == webrtc::kVideoRotation_0 || m_currentRotation == webrtc::kVideoRotation_90))
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

    auto locker = holdLock(m_sinksLock);
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
