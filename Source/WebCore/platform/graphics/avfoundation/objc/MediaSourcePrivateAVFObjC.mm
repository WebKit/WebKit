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
#import "CDMSessionAVContentKeySession.h"
#import "ContentType.h"
#import "Logging.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "MediaStrategy.h"
#import "PlatformStrategies.h"
#import "SourceBufferParserAVFObjC.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "VideoMediaSampleRenderer.h"
#import <algorithm>
#import <objc/runtime.h>
#import <ranges>
#import <wtf/NativePromise.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/AtomString.h>

namespace WebCore {

#pragma mark -
#pragma mark MediaSourcePrivateAVFObjC

WorkQueue& MediaSourcePrivateAVFObjC::queueSingleton()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(hasPlatformStrategies() && platformStrategies()->mediaStrategy()->hasRemoteRendererFor(MediaPlayerMediaEngineIdentifier::AVFoundationMSE) ? WorkQueue::create("MediaSourcePrivateAVFObjC"_s) : Ref { WorkQueue::mainSingleton() });
    });
    return workQueue.get();
}

Ref<MediaSourcePrivateAVFObjC> MediaSourcePrivateAVFObjC::create(MediaPlayerPrivateMediaSourceAVFObjC& parent, MediaSourcePrivateClient& client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateAVFObjC(parent, client));
    client.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateAVFObjC::MediaSourcePrivateAVFObjC(MediaPlayerPrivateMediaSourceAVFObjC& parent, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client, queueSingleton())
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
    ASSERT(player);
    m_player = downcast<MediaPlayerPrivateMediaSourceAVFObjC>(player);
    ensureOnDispatcher([protectedThis = Ref { *this }, renderer = m_player.get()->audioVideoRenderer()] {
        for (Ref sourceBuffer : protectedThis->sourceBuffers())
            downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->setAudioVideoRenderer(renderer);
    });
}

MediaSourcePrivate::AddStatus MediaSourcePrivateAVFObjC::addSourceBuffer(const ContentType& contentType, const MediaSourceConfiguration& configuration, RefPtr<SourceBufferPrivate>& outPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    RefPtr player = platformPlayer();
    if (!player)
        return AddStatus::InvalidState;

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;

    AddStatus returnedStatus;

    callOnMainRunLoopAndWait([&] {
        if (MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported) {
            returnedStatus = AddStatus::NotSupported;
            return;
        }
        returnedStatus = AddStatus::Ok;
    });

    if (returnedStatus != AddStatus::Ok)
        return returnedStatus;

    RefPtr parser = SourceBufferParser::create(contentType, configuration);
    if (!parser)
        return AddStatus::NotSupported;
#if !RELEASE_LOG_DISABLED
    parser->setLogger(m_logger, m_logIdentifier);
#endif

    Ref newSourceBuffer = SourceBufferPrivateAVFObjC::create(*this, parser.releaseNonNull(), player->audioVideoRenderer());
    newSourceBuffer->setResourceOwner(m_resourceOwner);
    outPrivate = newSourceBuffer.copyRef();
    newSourceBuffer->setMediaSourceDuration(duration());
    {
        Locker locker { m_lock };
        m_sourceBuffers.append(WTFMove(newSourceBuffer));
    }
    return AddStatus::Ok;
}

void MediaSourcePrivateAVFObjC::removeSourceBuffer(SourceBufferPrivate& sourceBuffer)
{
    assertIsCurrent(m_dispatcher.get());
    if (downcast<SourceBufferPrivateAVFObjC>(&sourceBuffer) == m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo = nullptr;
    MediaSourcePrivate::removeSourceBuffer(sourceBuffer);
}

void MediaSourcePrivateAVFObjC::notifyActiveSourceBuffersChanged()
{
    if (RefPtr player = this->player()) {
        ensureOnMainThread([player] {
            player->notifyActiveSourceBuffersChanged();
        });
    }
}

RefPtr<MediaPlayerPrivateInterface> MediaSourcePrivateAVFObjC::player() const
{
    return m_player.get();
}

void MediaSourcePrivateAVFObjC::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    if (RefPtr player = platformPlayer()) {
        ensureOnMainThread([player] {
            player->durationChanged();
        });
    }
}

void MediaSourcePrivateAVFObjC::markEndOfStream(EndOfStreamStatus status)
{
    if (RefPtr player = platformPlayer(); status == EndOfStreamStatus::NoError && player) {
        ensureOnMainThread([player] {
            player->setNetworkState(MediaPlayer::NetworkState::Loaded);
        });
    }
    MediaSourcePrivate::markEndOfStream(status);
}

FloatSize MediaSourcePrivateAVFObjC::naturalSize() const
{
    assertIsCurrent(m_dispatcher.get());
    FloatSize result;

    for (auto* sourceBuffer : m_activeSourceBuffers)
        result = result.expandedTo(downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->naturalSize());

    return result;
}

void MediaSourcePrivateAVFObjC::hasSelectedVideoChanged(SourceBufferPrivateAVFObjC& sourceBuffer)
{
    assertIsCurrent(m_dispatcher.get());
    bool hasSelectedVideo = sourceBuffer.hasSelectedVideo();
    if (m_sourceBufferWithSelectedVideo == &sourceBuffer && !hasSelectedVideo)
        setSourceBufferWithSelectedVideo(nullptr);
    else if (m_sourceBufferWithSelectedVideo != &sourceBuffer && hasSelectedVideo)
        setSourceBufferWithSelectedVideo(&sourceBuffer);
}

void MediaSourcePrivateAVFObjC::flushAndReenqueueActiveVideoSourceBuffers()
{
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->m_dispatcher.get());
        for (auto* sourceBuffer : protectedThis->m_activeSourceBuffers)
            downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->flushAndReenqueueVideo();
    });
}

#if ENABLE(ENCRYPTED_MEDIA)
bool MediaSourcePrivateAVFObjC::waitingForKey() const
{
    return std::ranges::any_of(sourceBuffers(), [](auto& sourceBuffer) {
        return sourceBuffer->waitingForKey();
    });
}
#endif

void MediaSourcePrivateAVFObjC::setSourceBufferWithSelectedVideo(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    assertIsCurrent(m_dispatcher.get());
    if (m_sourceBufferWithSelectedVideo)
        m_sourceBufferWithSelectedVideo->setVideoRenderer(false);

    m_sourceBufferWithSelectedVideo = sourceBuffer;

    if (auto player = platformPlayer(); m_sourceBufferWithSelectedVideo && player)
        m_sourceBufferWithSelectedVideo->setVideoRenderer(true);
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
    assertIsMainThread();
    return std::ranges::any_of(sourceBuffers(), [](auto& sourceBuffer) {
        return downcast<SourceBufferPrivateAVFObjC>(sourceBuffer)->needsVideoLayer();
    });
}

void MediaSourcePrivateAVFObjC::bufferedChanged(const PlatformTimeRanges& buffered)
{
    MediaSourcePrivate::bufferedChanged(buffered);
    ensureOnMainThread([player = m_player] {
        if (RefPtr protectedPlayer = player.get())
            protectedPlayer->bufferedChanged();
    });
}

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
