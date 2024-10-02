/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "ManagedMediaSource.h"

#if ENABLE(MEDIA_SOURCE)

#include "Event.h"
#include "EventNames.h"
#include "MediaSourcePrivate.h"
#include "SourceBufferList.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ManagedMediaSource);

Ref<ManagedMediaSource> ManagedMediaSource::create(ScriptExecutionContext& context, MediaSourceInit&& options)
{
    auto mediaSource = adoptRef(*new ManagedMediaSource(context, WTFMove(options)));
    mediaSource->suspendIfNeeded();
    return mediaSource;
}

ManagedMediaSource::ManagedMediaSource(ScriptExecutionContext& context, MediaSourceInit&& options)
    : MediaSource(context, WTFMove(options))
    , m_streamingTimer(*this, &ManagedMediaSource::streamingTimerFired)
{
}

ManagedMediaSource::~ManagedMediaSource()
{
    m_streamingTimer.stop();
}

ExceptionOr<ManagedMediaSource::PreferredQuality> ManagedMediaSource::quality() const
{
    return PreferredQuality::High;
}

bool ManagedMediaSource::isTypeSupported(ScriptExecutionContext& context, const String& type)
{
    return MediaSource::isTypeSupported(context, type);
}

void ManagedMediaSource::elementDetached()
{
    setStreaming(false);
}

void ManagedMediaSource::setStreaming(bool streaming)
{
    if (m_streaming == streaming)
        return;
    ALWAYS_LOG(LOGIDENTIFIER, streaming);
    m_streaming = streaming;
    if (m_private)
        m_private->setStreaming(streaming);
    if (streaming) {
        scheduleEvent(eventNames().startstreamingEvent);
        if (m_streamingAllowed) {
            ensurePrefsRead();
            Seconds delay { *m_highThreshold };
            m_streamingTimer.startOneShot(delay);
        }
    } else {
        if (m_streamingTimer.isActive())
            m_streamingTimer.stop();
        scheduleEvent(eventNames().endstreamingEvent);
    }
    notifyElementUpdateMediaState();
}

void ManagedMediaSource::ensurePrefsRead()
{
    ASSERT(scriptExecutionContext());

    if (m_lowThreshold && m_highThreshold)
        return;
    m_lowThreshold = scriptExecutionContext()->settingsValues().managedMediaSourceLowThreshold;
    m_highThreshold = scriptExecutionContext()->settingsValues().managedMediaSourceHighThreshold;
}

void ManagedMediaSource::monitorSourceBuffers()
{
    MediaSource::monitorSourceBuffers();

    if (!activeSourceBuffers()->length()) {
        setStreaming(true);
        return;
    }

    ensurePrefsRead();

    auto currentTime = this->currentTime();
    ASSERT(currentTime.isValid());

    auto limitAhead = [&] (double upper) {
        MediaTime aheadTime = currentTime + MediaTime::createWithDouble(upper);
        return isEnded() ? std::min(duration(), aheadTime) : aheadTime;
    };
    if (!m_streaming) {
        PlatformTimeRanges neededBufferedRange { currentTime, std::max(currentTime, limitAhead(*m_lowThreshold)) };
        if (!isBuffered(neededBufferedRange))
            setStreaming(true);
        return;
    }

    if (auto ahead = limitAhead(*m_highThreshold); currentTime < ahead) {
        if (isBuffered({ currentTime,  ahead }))
            setStreaming(false);
    } else
        setStreaming(false);
}

void ManagedMediaSource::streamingTimerFired()
{
    ALWAYS_LOG(LOGIDENTIFIER, "Disabling streaming due to policy ", *m_highThreshold);
    m_streamingAllowed = false;
    if (m_private)
        m_private->setStreamingAllowed(false);
    notifyElementUpdateMediaState();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
