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

#import "config.h"
#import "MediaSourcePrivateAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "CDMInstance.h"
#import "CDMSessionMediaSourceAVFObjC.h"
#import "ContentType.h"
#import "Logging.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "SourceBufferParserAVFObjC.h"
#import "SourceBufferPrivateAVFObjC.h"
#import <objc/runtime.h>
#import <wtf/Algorithms.h>
#import <wtf/NativePromise.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/AtomString.h>

namespace WebCore {

#pragma mark -
#pragma mark MediaSourcePrivateAVFObjC

Ref<MediaSourcePrivateAVFObjC> MediaSourcePrivateAVFObjC::create(MediaPlayerPrivateMediaSourceAVFObjC& parent, MediaSourcePrivateClient& client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateAVFObjC(parent, client));
    client.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateAVFObjC::MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC& parent, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
    , m_player(parent)
#if !RELEASE_LOG_DISABLED
    , m_logger(parent.mediaPlayerLogger())
    , m_logIdentifier(parent.mediaPlayerLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
#if !RELEASE_LOG_DISABLED
    client.setLogIdentifier(m_logIdentifier);
#endif
}

MediaSourcePrivateAVFObjC::~MediaSourcePrivateAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

void MediaSourcePrivateAVFObjC::setPlayer(MediaPlayerPrivateInterface* player)
{
    m_player = downcast<MediaPlayerPrivateMediaSourceAVFObjC>(player);
}

MediaSourcePrivate::AddStatus MediaSourcePrivateAVFObjC::addSourceBuffer(const ContentType& contentType, bool webMParserEnabled, RefPtr<SourceBufferPrivate>& outPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported)
        return AddStatus::NotSupported;

    auto parser = SourceBufferParser::create(contentType, webMParserEnabled);
    if (!parser)
        return AddStatus::NotSupported;
#if !RELEASE_LOG_DISABLED
    parser->setLogger(m_logger, m_logIdentifier);
#endif

    auto newSourceBuffer = SourceBufferPrivateAVFObjC::create(*this, parser.releaseNonNull());
#if ENABLE(ENCRYPTED_MEDIA)
    newSourceBuffer->setCDMInstance(m_cdmInstance.get());
#endif
    newSourceBuffer->setResourceOwner(m_resourceOwner);
    outPrivate = newSourceBuffer.copyRef();
    newSourceBuffer->setMediaSourceDuration(duration());
    m_sourceBuffers.append(WTFMove(newSourceBuffer));

    return AddStatus::Ok;
}

void MediaSourcePrivateAVFObjC::removeSourceBuffer(SourceBufferPrivate& sourceBuffer)
{
    if (downcast<SourceBufferPrivateAVFObjC>(&sourceBuffer) == m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo = nullptr;
    if (m_bufferedRanges.contains(&sourceBuffer))
        m_bufferedRanges.remove(&sourceBuffer);

    MediaSourcePrivate::removeSourceBuffer(sourceBuffer);
}

void MediaSourcePrivateAVFObjC::notifyActiveSourceBuffersChanged()
{
    if (auto player = this->player())
        player->notifyActiveSourceBuffersChanged();
}

RefPtr<MediaPlayerPrivateInterface> MediaSourcePrivateAVFObjC::player() const
{
    return m_player.get();
}

void MediaSourcePrivateAVFObjC::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    if (auto player = platformPlayer())
        player->durationChanged();
}

void MediaSourcePrivateAVFObjC::markEndOfStream(EndOfStreamStatus status)
{
    if (auto player = platformPlayer(); status == EndOfStreamStatus::NoError && player)
        player->setNetworkState(MediaPlayer::NetworkState::Loaded);
    MediaSourcePrivate::markEndOfStream(status);
}

MediaPlayer::ReadyState MediaSourcePrivateAVFObjC::mediaPlayerReadyState() const
{
    if (auto player = this->player())
        return player->readyState();
    return MediaPlayer::ReadyState::HaveNothing;
}

void MediaSourcePrivateAVFObjC::setMediaPlayerReadyState(MediaPlayer::ReadyState readyState)
{
    if (auto player = platformPlayer())
        player->setReadyState(readyState);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void MediaSourcePrivateAVFObjC::sourceBufferKeyNeeded(SourceBufferPrivateAVFObjC* buffer, const SharedBuffer& initData)
{
    m_sourceBuffersNeedingSessions.append(buffer);

    if (auto player = platformPlayer())
        player->keyNeeded(initData);
}
#endif

bool MediaSourcePrivateAVFObjC::hasSelectedVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (auto* sourceBuffer) {
        return downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->hasSelectedVideo();
    });
}

void MediaSourcePrivateAVFObjC::willSeek()
{
    for (auto* sourceBuffer : m_activeSourceBuffers)
        downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->willSeek();
}

FloatSize MediaSourcePrivateAVFObjC::naturalSize() const
{
    FloatSize result;

    for (auto* sourceBuffer : m_activeSourceBuffers)
        result = result.expandedTo(downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->naturalSize());

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

void MediaSourcePrivateAVFObjC::setVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    if (m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo->setVideoRenderer(renderer);
}

void MediaSourcePrivateAVFObjC::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    if (m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo->stageVideoRenderer(renderer);
}

void MediaSourcePrivateAVFObjC::setDecompressionSession(WebCoreDecompressionSession* decompressionSession)
{
    if (m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo->setDecompressionSession(decompressionSession);
}

void MediaSourcePrivateAVFObjC::flushActiveSourceBuffersIfNeeded()
{
    for (auto* sourceBuffer : m_activeSourceBuffers)
        downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->flushIfNeeded();
}

#if ENABLE(ENCRYPTED_MEDIA)
void MediaSourcePrivateAVFObjC::cdmInstanceAttached(CDMInstance& instance)
{
    if (m_cdmInstance.get() == &instance)
        return;

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
        m_sourceBufferWithSelectedVideo->setVideoRenderer(nullptr);
        m_sourceBufferWithSelectedVideo->setDecompressionSession(nullptr);
    }

    m_sourceBufferWithSelectedVideo = sourceBuffer;

    if (auto player = platformPlayer(); m_sourceBufferWithSelectedVideo && player) {
        m_sourceBufferWithSelectedVideo->setVideoRenderer(player->layerOrVideoRenderer());
        m_sourceBufferWithSelectedVideo->setDecompressionSession(player->decompressionSession());
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateAVFObjC::logChannel() const
{
    return LogMediaSource;
}
#endif

void MediaSourcePrivateAVFObjC::failedToCreateRenderer(RendererType type)
{
    if (RefPtr client = this->client())
        client->failedToCreateRenderer(type);
}

bool MediaSourcePrivateAVFObjC::needsVideoLayer() const
{
    return anyOf(m_sourceBuffers, [] (auto& sourceBuffer) {
        return downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->needsVideoLayer();
    });
}

void MediaSourcePrivateAVFObjC::bufferedChanged(const PlatformTimeRanges& buffered)
{
    MediaSourcePrivate::bufferedChanged(buffered);
    if (RefPtr player = m_player.get())
        player->bufferedChanged();
}

void MediaSourcePrivateAVFObjC::trackBufferedChanged(SourceBufferPrivate& sourceBuffer, Vector<PlatformTimeRanges>&& ranges)
{
    auto it = m_bufferedRanges.find(&sourceBuffer);
    if (it == m_bufferedRanges.end())
        m_bufferedRanges.add(&sourceBuffer, WTFMove(ranges));
    else
        it->value = WTFMove(ranges);

    PlatformTimeRanges intersectionRange { MediaTime::zeroTime(), MediaTime::positiveInfiniteTime() };
    for (auto& ranges : m_bufferedRanges.values()) {
        for (auto& range : ranges)
            intersectionRange.intersectWith(range);
    }
    bufferedChanged(intersectionRange);
}

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
