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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "MediaSourcePrivateAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "CDMSessionMediaSourceAVFObjC.h"
#import "ContentType.h"
#import "ExceptionCodePlaceholder.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "NotImplemented.h"
#import "SoftLinking.h"
#import <objc/runtime.h>
#import <wtf/text/AtomicString.h>
#import <wtf/text/StringBuilder.h>

namespace WebCore {

#pragma mark -
#pragma mark MediaSourcePrivateAVFObjC

RefPtr<MediaSourcePrivateAVFObjC> MediaSourcePrivateAVFObjC::create(MediaPlayerPrivateMediaSourceAVFObjC* parent)
{
    return adoptRef(new MediaSourcePrivateAVFObjC(parent));
}

MediaSourcePrivateAVFObjC::MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC* parent)
    : m_player(parent)
    , m_duration(std::numeric_limits<double>::quiet_NaN())
    , m_isEnded(false)
{
}

MediaSourcePrivateAVFObjC::~MediaSourcePrivateAVFObjC()
{
    for (auto it = m_sourceBuffers.begin(), end = m_sourceBuffers.end(); it != end; ++it)
        (*it)->clearMediaSource();
}

MediaSourcePrivate::AddStatus MediaSourcePrivateAVFObjC::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& outPrivate)
{
    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType.type();
    parameters.codecs = contentType.parameter(ASCIILiteral("codecs"));
    if (MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) == MediaPlayer::IsNotSupported)
        return NotSupported;

    m_sourceBuffers.append(SourceBufferPrivateAVFObjC::create(this));
    outPrivate = m_sourceBuffers.last();

    return Ok;
}

void MediaSourcePrivateAVFObjC::removeSourceBuffer(SourceBufferPrivate* buffer)
{
    ASSERT(m_sourceBuffers.contains(buffer));

    size_t pos = m_activeSourceBuffers.find(buffer);
    if (pos != notFound)
        m_activeSourceBuffers.remove(pos);

    pos = m_sourceBuffers.find(buffer);
    m_sourceBuffers.remove(pos);
}

double MediaSourcePrivateAVFObjC::duration()
{
    return m_duration;
}

void MediaSourcePrivateAVFObjC::setDuration(double duration)
{
    if (duration == m_duration)
        return;

    m_duration = duration;
    m_player->durationChanged();
}

void MediaSourcePrivateAVFObjC::markEndOfStream(EndOfStreamStatus status)
{
    if (status == EosNoError)
        m_player->setNetworkState(MediaPlayer::Loaded);
    m_isEnded = true;
}

void MediaSourcePrivateAVFObjC::unmarkEndOfStream() 
{
    // FIXME(125159): implement unmarkEndOfStream()
    m_isEnded = false;
}

MediaPlayer::ReadyState MediaSourcePrivateAVFObjC::readyState() const
{
    return m_player->readyState();
}

void MediaSourcePrivateAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    m_player->setReadyState(readyState);
}

void MediaSourcePrivateAVFObjC::sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateAVFObjC* buffer, bool active)
{
    if (active && !m_activeSourceBuffers.contains(buffer))
        m_activeSourceBuffers.append(buffer);

    if (!active) {
        size_t position = m_activeSourceBuffers.find(buffer);
        if (position != notFound)
            m_activeSourceBuffers.remove(position);
    }
}

#if ENABLE(ENCRYPTED_MEDIA_V2)
std::unique_ptr<CDMSession> MediaSourcePrivateAVFObjC::createSession(const String&)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    if (m_sourceBuffersNeedingSessions.isEmpty())
        return nullptr;
    return std::make_unique<CDMSessionMediaSourceAVFObjC>(m_sourceBuffersNeedingSessions.takeFirst());
#endif
    return nullptr;
}
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
void MediaSourcePrivateAVFObjC::sourceBufferKeyNeeded(SourceBufferPrivateAVFObjC* buffer, Uint8Array* initData)
{
    m_sourceBuffersNeedingSessions.append(buffer);
    player()->keyNeeded(initData);
}
#endif

static bool MediaSourcePrivateAVFObjCHasAudio(PassRefPtr<SourceBufferPrivateAVFObjC> prpSourceBuffer)
{
    RefPtr<SourceBufferPrivateAVFObjC> sourceBuffer = prpSourceBuffer;
    return sourceBuffer->hasAudio();
}

bool MediaSourcePrivateAVFObjC::hasAudio() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MediaSourcePrivateAVFObjCHasAudio);
}

static bool MediaSourcePrivateAVFObjCHasVideo(PassRefPtr<SourceBufferPrivateAVFObjC> prpSourceBuffer)
{
    RefPtr<SourceBufferPrivateAVFObjC> sourceBuffer = prpSourceBuffer;
    return sourceBuffer->hasVideo();
}

bool MediaSourcePrivateAVFObjC::hasVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MediaSourcePrivateAVFObjCHasVideo);
}

MediaTime MediaSourcePrivateAVFObjC::seekToTime(MediaTime targetTime, MediaTime negativeThreshold, MediaTime positiveThreshold)
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

IntSize MediaSourcePrivateAVFObjC::naturalSize() const
{
    IntSize result;

    for (auto* sourceBuffer : m_activeSourceBuffers)
        result = result.expandedTo(sourceBuffer->naturalSize());

    return result;
}

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
