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
#include "MediaSourcePrivateAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "CDMInstance.h"
#import "CDMSessionMediaSourceAVFObjC.h"
#import "ContentType.h"
#import "Logging.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "SourceBufferPrivateAVFObjC.h"
#import <objc/runtime.h>
#import <wtf/Algorithms.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/AtomicString.h>

namespace WebCore {

#pragma mark -
#pragma mark MediaSourcePrivateAVFObjC

Ref<MediaSourcePrivateAVFObjC> MediaSourcePrivateAVFObjC::create(MediaPlayerPrivateMediaSourceAVFObjC* parent, MediaSourcePrivateClient* client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateAVFObjC(parent, client));
    client->setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateAVFObjC::MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC* parent, MediaSourcePrivateClient* client)
    : m_player(parent)
    , m_client(client)
    , m_isEnded(false)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_player->mediaPlayerLogger())
    , m_logIdentifier(m_player->mediaPlayerLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
#if !RELEASE_LOG_DISABLED
    m_client->setLogIdentifier(m_logIdentifier);
#endif
}

MediaSourcePrivateAVFObjC::~MediaSourcePrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto it = m_sourceBuffers.begin(), end = m_sourceBuffers.end(); it != end; ++it)
        (*it)->clearMediaSource();
}

MediaSourcePrivate::AddStatus MediaSourcePrivateAVFObjC::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& outPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) == MediaPlayer::IsNotSupported)
        return NotSupported;

    auto newSourceBuffer = SourceBufferPrivateAVFObjC::create(this);
#if ENABLE(ENCRYPTED_MEDIA)
    newSourceBuffer->setCDMInstance(m_cdmInstance.get());
#endif
    outPrivate = newSourceBuffer.copyRef();
    m_sourceBuffers.append(WTFMove(newSourceBuffer));

    return Ok;
}

void MediaSourcePrivateAVFObjC::removeSourceBuffer(SourceBufferPrivate* buffer)
{
    ASSERT(m_sourceBuffers.contains(buffer));

    if (buffer == m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo = nullptr;

    size_t pos = m_activeSourceBuffers.find(buffer);
    if (pos != notFound) {
        m_activeSourceBuffers.remove(pos);
        m_player->notifyActiveSourceBuffersChanged();
    }

    pos = m_sourceBuffers.find(buffer);
    m_sourceBuffers[pos]->clearMediaSource();
    m_sourceBuffers.remove(pos);
}

MediaTime MediaSourcePrivateAVFObjC::duration()
{
    return m_client->duration();
}

std::unique_ptr<PlatformTimeRanges> MediaSourcePrivateAVFObjC::buffered()
{
    return m_client->buffered();
}

void MediaSourcePrivateAVFObjC::durationChanged()
{
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

void MediaSourcePrivateAVFObjC::waitForSeekCompleted()
{
    m_player->waitForSeekCompleted();
}

void MediaSourcePrivateAVFObjC::seekCompleted()
{
    m_player->seekCompleted();
}

void MediaSourcePrivateAVFObjC::sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateAVFObjC* buffer, bool active)
{
    if (active && !m_activeSourceBuffers.contains(buffer)) {
        m_activeSourceBuffers.append(buffer);
        m_player->notifyActiveSourceBuffersChanged();
    }

    if (!active) {
        size_t position = m_activeSourceBuffers.find(buffer);
        if (position != notFound) {
            m_activeSourceBuffers.remove(position);
            m_player->notifyActiveSourceBuffersChanged();
        }
    }
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void MediaSourcePrivateAVFObjC::sourceBufferKeyNeeded(SourceBufferPrivateAVFObjC* buffer, Uint8Array* initData)
{
    m_sourceBuffersNeedingSessions.append(buffer);
    player()->keyNeeded(initData);
}
#endif

static bool MediaSourcePrivateAVFObjCHasAudio(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    return sourceBuffer->hasAudio();
}

bool MediaSourcePrivateAVFObjC::hasAudio() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MediaSourcePrivateAVFObjCHasAudio);
}

bool MediaSourcePrivateAVFObjC::hasVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (SourceBufferPrivateAVFObjC* sourceBuffer) {
        return sourceBuffer->hasVideo();
    });
}

bool MediaSourcePrivateAVFObjC::hasSelectedVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (SourceBufferPrivateAVFObjC* sourceBuffer) {
        return sourceBuffer->hasSelectedVideo();
    });
}

void MediaSourcePrivateAVFObjC::willSeek()
{
    for (auto* sourceBuffer : m_activeSourceBuffers)
        sourceBuffer->willSeek();
}

void MediaSourcePrivateAVFObjC::seekToTime(const MediaTime& time)
{
    m_client->seekToTime(time);
}

MediaTime MediaSourcePrivateAVFObjC::fastSeekTimeForMediaTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    MediaTime seekTime = targetTime;

    for (auto it = m_activeSourceBuffers.begin(), end = m_activeSourceBuffers.end(); it != end; ++it) {
        MediaTime sourceSeekTime = (*it)->fastSeekTimeForMediaTime(targetTime, negativeThreshold, positiveThreshold);
        if (abs(targetTime - sourceSeekTime) > abs(targetTime - seekTime))
            seekTime = sourceSeekTime;
    }

    return seekTime;
}

FloatSize MediaSourcePrivateAVFObjC::naturalSize() const
{
    FloatSize result;

    for (auto* sourceBuffer : m_activeSourceBuffers)
        result = result.expandedTo(sourceBuffer->naturalSize());

    return result;
}

void MediaSourcePrivateAVFObjC::hasSelectedVideoChanged(SourceBufferPrivateAVFObjC& sourceBuffer)
{
    bool hasSelectedVideo = sourceBuffer.hasSelectedVideo();
    if (m_sourceBufferWithSelectedVideo == &sourceBuffer && !hasSelectedVideo)
        setSourceBufferWithSelectedVideo(nullptr);
    else if (m_sourceBufferWithSelectedVideo != &sourceBuffer && hasSelectedVideo)
        setSourceBufferWithSelectedVideo(&sourceBuffer);
}

void MediaSourcePrivateAVFObjC::setVideoLayer(AVSampleBufferDisplayLayer* layer)
{
    if (m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo->setVideoLayer(layer);
}

void MediaSourcePrivateAVFObjC::setDecompressionSession(WebCoreDecompressionSession* decompressionSession)
{
    if (m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo->setDecompressionSession(decompressionSession);
}

#if ENABLE(ENCRYPTED_MEDIA)
void MediaSourcePrivateAVFObjC::cdmInstanceAttached(CDMInstance& instance)
{
    ASSERT(!m_cdmInstance);
    m_cdmInstance = &instance;
    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->setCDMInstance(&instance);
}

void MediaSourcePrivateAVFObjC::cdmInstanceDetached(CDMInstance& instance)
{
    ASSERT_UNUSED(instance, m_cdmInstance && m_cdmInstance == &instance);
    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->setCDMInstance(nullptr);

    m_cdmInstance = nullptr;
}

void MediaSourcePrivateAVFObjC::attemptToDecryptWithInstance(CDMInstance& instance)
{
    ASSERT_UNUSED(instance, m_cdmInstance && m_cdmInstance == &instance);
    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->attemptToDecrypt();
}

bool MediaSourcePrivateAVFObjC::waitingForKey() const
{
    return anyOf(m_sourceBuffers, [] (auto& sourceBuffer) {
        return sourceBuffer->waitingForKey();
    });
}

void MediaSourcePrivateAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
    if (m_cdmInstance)
        m_cdmInstance->setHDCPStatus(obscured ? CDMInstance::HDCPStatus::OutputRestricted : CDMInstance::HDCPStatus::Valid);
}
#endif

void MediaSourcePrivateAVFObjC::setSourceBufferWithSelectedVideo(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    if (m_sourceBufferWithSelectedVideo) {
        m_sourceBufferWithSelectedVideo->setVideoLayer(nullptr);
        m_sourceBufferWithSelectedVideo->setDecompressionSession(nullptr);
    }

    m_sourceBufferWithSelectedVideo = sourceBuffer;

    if (m_sourceBufferWithSelectedVideo) {
        m_sourceBufferWithSelectedVideo->setVideoLayer(m_player->sampleBufferDisplayLayer());
        m_sourceBufferWithSelectedVideo->setDecompressionSession(m_player->decompressionSession());
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
