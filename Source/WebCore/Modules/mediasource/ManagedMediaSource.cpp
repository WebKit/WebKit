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

#if ENABLE(MANAGED_MEDIA_SOURCE) && ENABLE(MEDIA_SOURCE)

#include "Event.h"
#include "EventNames.h"
#include "MediaSourcePrivate.h"
#include "SourceBufferList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ManagedMediaSource);

Ref<ManagedMediaSource> ManagedMediaSource::create(ScriptExecutionContext& context)
{
    auto mediaSource = adoptRef(*new ManagedMediaSource(context));
    mediaSource->suspendIfNeeded();
    return mediaSource;
}

ManagedMediaSource::ManagedMediaSource(ScriptExecutionContext& context)
    : MediaSource(context)
{
}

ManagedMediaSource::~ManagedMediaSource() = default;

ExceptionOr<ManagedMediaSource::BufferingPolicy> ManagedMediaSource::buffering() const
{
    return BufferingPolicy::Medium;
}

ExceptionOr<ManagedMediaSource::PreferredQuality> ManagedMediaSource::quality() const
{
    return PreferredQuality::High;
}

bool ManagedMediaSource::isTypeSupported(ScriptExecutionContext& context, const String& type)
{
    return MediaSource::isTypeSupported(context, type);
}

void ManagedMediaSource::setStreaming(bool streaming)
{
    if (m_streaming == streaming)
        return;
    m_streaming = streaming;
    if (streaming)
        scheduleEvent(eventNames().startstreamingEvent);
    else
        scheduleEvent(eventNames().endstreamingEvent);
    notifyElementUpdateMediaState();
}

bool ManagedMediaSource::isBuffered(const PlatformTimeRanges& ranges) const
{
    if (ranges.length() < 1 || isClosed())
        return true;

    ASSERT(ranges.length() == 1);

    auto bufferedRanges = buffered();
    if (!bufferedRanges->length())
        return false;
    bufferedRanges->intersectWith(ranges);

    if (!bufferedRanges->length())
        return false;

    auto hasBufferedTime = [&] (const MediaTime& time) {
        return abs(bufferedRanges->nearest(time) - time) <= m_private->timeFudgeFactor();
    };

    if (!hasBufferedTime(ranges.minimumBufferedTime()) || !hasBufferedTime(ranges.maximumBufferedTime()))
        return false;

    if (bufferedRanges->length() == 1)
        return true;

    // Ensure that if we have a gap in the buffered range, it is smaller than the fudge factor;
    for (unsigned i = 1; i < bufferedRanges->length(); i++) {
        if (bufferedRanges->end(i) - bufferedRanges->start(i-1) > m_private->timeFudgeFactor())
            return false;
    }

    return true;
}

void ManagedMediaSource::monitorSourceBuffers()
{
    if (isClosed()) {
        setStreaming(false);
        return;
    }

    MediaSource::monitorSourceBuffers();

    if (!activeSourceBuffers() || !activeSourceBuffers()->length()) {
        setStreaming(true);
        return;
    }
    auto currentTime = this->currentTime();

    if (!m_lowThreshold || !m_highThreshold) {
        m_lowThreshold = mediaElement()->document().settings().managedMediaSourceLowThreshold();
        m_highThreshold = mediaElement()->document().settings().managedMediaSourceHighThreshold();
    }

    if (!m_streaming) {
        MediaTime aheadTime = std::min(duration(), currentTime + MediaTime::createWithDouble(*m_lowThreshold));
        PlatformTimeRanges neededBufferedRange { currentTime, std::max(currentTime, aheadTime) };
        if (!isBuffered(neededBufferedRange))
            setStreaming(true);
        return;
    }

    MediaTime aheadTime = std::min(duration(), currentTime + MediaTime::createWithDouble(*m_highThreshold));
    PlatformTimeRanges neededBufferedRange { currentTime, aheadTime };
    if (isBuffered(neededBufferedRange))
        setStreaming(false);
}

}
#endif
