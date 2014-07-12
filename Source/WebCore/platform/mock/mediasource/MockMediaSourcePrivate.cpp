/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MockMediaSourcePrivate.h"

#if ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "ExceptionCodePlaceholder.h"
#include "MediaSourcePrivateClient.h"
#include "MockMediaPlayerMediaSource.h"
#include "MockSourceBufferPrivate.h"

namespace WebCore {

RefPtr<MockMediaSourcePrivate> MockMediaSourcePrivate::create(MockMediaPlayerMediaSource* parent, MediaSourcePrivateClient* client)
{
    RefPtr<MockMediaSourcePrivate> mediaSourcePrivate = adoptRef(new MockMediaSourcePrivate(parent, client));
    client->setPrivateAndOpen(*mediaSourcePrivate);
    return mediaSourcePrivate;
}

MockMediaSourcePrivate::MockMediaSourcePrivate(MockMediaPlayerMediaSource* parent, MediaSourcePrivateClient* client)
    : m_player(parent)
    , m_client(client)
    , m_isEnded(false)
    , m_totalVideoFrames(0)
    , m_droppedVideoFrames(0)
    , m_corruptedVideoFrames(0)
    , m_totalFrameDelay(0)
{
}

MockMediaSourcePrivate::~MockMediaSourcePrivate()
{
    for (auto it = m_sourceBuffers.begin(), end = m_sourceBuffers.end(); it != end; ++it)
        (*it)->clearMediaSource();
}

MediaSourcePrivate::AddStatus MockMediaSourcePrivate::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& outPrivate)
{
    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType.type();
    parameters.codecs = contentType.parameter(ASCIILiteral("codecs"));
    if (MockMediaPlayerMediaSource::supportsType(parameters) == MediaPlayer::IsNotSupported)
        return NotSupported;

    m_sourceBuffers.append(MockSourceBufferPrivate::create(this));
    outPrivate = m_sourceBuffers.last();

    return Ok;
}

void MockMediaSourcePrivate::removeSourceBuffer(SourceBufferPrivate* buffer)
{
    ASSERT(m_sourceBuffers.contains(buffer));

    size_t pos = m_activeSourceBuffers.find(buffer);
    if (pos != notFound)
        m_activeSourceBuffers.remove(pos);

    pos = m_sourceBuffers.find(buffer);
    m_sourceBuffers.remove(pos);
}

double MockMediaSourcePrivate::duration()
{
    return m_client->duration();
}

std::unique_ptr<PlatformTimeRanges> MockMediaSourcePrivate::buffered()
{
    return m_client->buffered();
}

void MockMediaSourcePrivate::durationChanged()
{
    m_player->updateDuration(MediaTime::createWithDouble(duration()));
}

void MockMediaSourcePrivate::markEndOfStream(EndOfStreamStatus status)
{
    if (status == EosNoError)
        m_player->setNetworkState(MediaPlayer::Loaded);
    m_isEnded = true;
}

void MockMediaSourcePrivate::unmarkEndOfStream()
{
    m_isEnded = false;
}

MediaPlayer::ReadyState MockMediaSourcePrivate::readyState() const
{
    return m_player->readyState();
}

void MockMediaSourcePrivate::setReadyState(MediaPlayer::ReadyState readyState)
{
    m_player->setReadyState(readyState);
}

void MockMediaSourcePrivate::waitForSeekCompleted()
{
    m_player->waitForSeekCompleted();
}

void MockMediaSourcePrivate::seekCompleted()
{
    m_player->seekCompleted();
}

void MockMediaSourcePrivate::sourceBufferPrivateDidChangeActiveState(MockSourceBufferPrivate* buffer, bool active)
{
    if (active && !m_activeSourceBuffers.contains(buffer))
        m_activeSourceBuffers.append(buffer);

    if (!active) {
        size_t position = m_activeSourceBuffers.find(buffer);
        if (position != notFound)
            m_activeSourceBuffers.remove(position);
    }
}

static bool MockSourceBufferPrivateHasAudio(PassRefPtr<MockSourceBufferPrivate> prpSourceBuffer)
{
    return prpSourceBuffer->hasAudio();
}

bool MockMediaSourcePrivate::hasAudio() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MockSourceBufferPrivateHasAudio);
}

static bool MockSourceBufferPrivateHasVideo(PassRefPtr<MockSourceBufferPrivate> prpSourceBuffer)
{
    return prpSourceBuffer->hasVideo();
}

bool MockMediaSourcePrivate::hasVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MockSourceBufferPrivateHasVideo);
}

void MockMediaSourcePrivate::seekToTime(const MediaTime& time)
{
    for (auto it = m_activeSourceBuffers.begin(), end = m_activeSourceBuffers.end(); it != end; ++it)
        (*it)->seekToTime(time);
}

MediaTime MockMediaSourcePrivate::seekToTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    MediaTime seekTime = targetTime;
    for (auto it = m_activeSourceBuffers.begin(), end = m_activeSourceBuffers.end(); it != end; ++it) {
        MediaTime sourceSeekTime = (*it)->fastSeekTimeForMediaTime(targetTime, negativeThreshold, positiveThreshold);
        if (abs(targetTime - sourceSeekTime) > abs(targetTime - seekTime))
            seekTime = sourceSeekTime;
    }

    for (auto it = m_activeSourceBuffers.begin(), end = m_activeSourceBuffers.end(); it != end; ++it)
        (*it)->seekToTime(seekTime);
    
    return seekTime;
}

};

#endif

