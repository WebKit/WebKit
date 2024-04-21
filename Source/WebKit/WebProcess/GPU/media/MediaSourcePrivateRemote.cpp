/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "MediaSourcePrivateRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "Logging.h"
#include "MediaPlayerPrivateRemote.h"
#include "MediaSourcePrivateRemoteMessageReceiverMessages.h"
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferIdentifier.h"
#include "SourceBufferPrivateRemote.h"
#include <WebCore/NotImplemented.h>
#include <mutex>
#include <wtf/NativePromise.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {

using namespace WebCore;

WorkQueue& MediaSourcePrivateRemote::queue()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("MediaSourceRemote"_s));
    });
    return workQueue.get();
}

Ref<MediaSourcePrivateRemote> MediaSourcePrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& mediaPlayerPrivate, MediaSourcePrivateClient& client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateRemote(gpuProcessConnection, identifier, mimeTypeCache, mediaPlayerPrivate, client));
    client.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

void MediaSourcePrivateRemote::ensureOnDispatcherSync(Function<void()>&& function) const
{
    if (queue().isCurrent())
        function();
    else
        queue().dispatchSync(WTFMove(function));
}

MediaSourcePrivateRemote::MediaSourcePrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& mediaPlayerPrivate, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client, queue())
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_receiver(MessageReceiver::create(*this))
    , m_identifier(identifier)
    , m_mimeTypeCache(mimeTypeCache)
    , m_mediaPlayerPrivate(mediaPlayerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(client.logger() ? *client.logger() : mediaPlayerPrivate.mediaPlayerLogger())
    , m_logIdentifier(mediaPlayerPrivate.mediaPlayerLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

    gpuProcessConnection.connection().addWorkQueueMessageReceiver(Messages::MediaSourcePrivateRemoteMessageReceiver::messageReceiverName(), queue(), m_receiver, m_identifier.toUInt64());

#if !RELEASE_LOG_DISABLED
    client.setLogIdentifier(m_logIdentifier);
#endif
}

MediaSourcePrivateRemote::~MediaSourcePrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->connection().removeWorkQueueMessageReceiver(Messages::MediaSourcePrivateRemoteMessageReceiver::messageReceiverName(), m_identifier.toUInt64());
}

MediaSourcePrivate::AddStatus MediaSourcePrivateRemote::addSourceBuffer(const ContentType& contentType, bool, RefPtr<SourceBufferPrivate>& outPrivate)
{
    RefPtr mediaPlayerPrivate = m_mediaPlayerPrivate.get();
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection || !mediaPlayerPrivate)
        return AddStatus::NotSupported;

    AddStatus returnedStatus;
    RemoteSourceBufferIdentifier returnedIdentifier;
    RefPtr<SourceBufferPrivate> returnedSourceBuffer;
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    // the sendSync() call requires us to run on the connection's dispatcher, which is the main thread.
    // FIXME: Uses a new Connection for remote playback, and not the main GPUProcessConnection's one.
    // FIXME: m_mimeTypeCache is a main-thread only object.
    callOnMainRunLoopAndWait([this, &returnedStatus, &returnedIdentifier, contentTypeString = contentType.raw().isolatedCopy(), &returnedSourceBuffer, mediaPlayerPrivate, gpuProcessConnection] {
        ContentType contentType { contentTypeString };
        MediaEngineSupportParameters parameters;
        parameters.isMediaSource = true;
        parameters.type = contentType;
        if (m_mimeTypeCache.supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported) {
            returnedStatus = AddStatus::NotSupported;
            return;
        }

        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteMediaSourceProxy::AddSourceBuffer(WTFMove(contentType)), m_identifier);
        auto [status, remoteSourceBufferIdentifier] = sendResult.takeReplyOr(AddStatus::NotSupported, std::nullopt);

        if (status == AddStatus::Ok) {
            ASSERT(remoteSourceBufferIdentifier.has_value());
            returnedIdentifier = * remoteSourceBufferIdentifier;
            returnedSourceBuffer = SourceBufferPrivateRemote::create(*gpuProcessConnection, *remoteSourceBufferIdentifier, *this, *mediaPlayerPrivate);
        }
        returnedStatus = status;
    });

    if (returnedStatus != AddStatus::Ok)
        return returnedStatus;

    ensureOnDispatcher([protectedThis = Ref { *this }, this, sourceBuffer = returnedSourceBuffer]() mutable {
        m_sourceBuffers.append(WTFMove(sourceBuffer));
    });
    outPrivate = WTFMove(returnedSourceBuffer);
    return returnedStatus;
}

RefPtr<WebCore::MediaPlayerPrivateInterface> MediaSourcePrivateRemote::player() const
{
    return m_mediaPlayerPrivate.get();
}

void MediaSourcePrivateRemote::durationChanged(const MediaTime& duration)
{
    // Called from the MediaSource's dispatcher.
    MediaSourcePrivate::durationChanged(duration);
    ensureOnDispatcher([protectedThis = Ref { *this }, this, duration] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::DurationChanged(duration), m_identifier);
    });
}

void MediaSourcePrivateRemote::bufferedChanged(const PlatformTimeRanges& buffered)
{
    // Called from the MediaSource's dispatcher.
    MediaSourcePrivate::bufferedChanged(buffered);
    // Called from SourceBufferPrivateRemote
    ensureOnDispatcher([protectedThis = Ref { *this }, this, buffered] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::BufferedChanged(buffered), m_identifier);
    });
}

void MediaSourcePrivateRemote::markEndOfStream(EndOfStreamStatus status)
{
    MediaSourcePrivate::markEndOfStream(status);
    ensureOnDispatcher([protectedThis = Ref { *this }, this, status] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return;
        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::MarkEndOfStream(status), m_identifier);
    });
}

void MediaSourcePrivateRemote::unmarkEndOfStream()
{
    MediaSourcePrivate::unmarkEndOfStream();
    ensureOnDispatcher([protectedThis = Ref { *this }, this] {
        // FIXME(125159): implement unmarkEndOfStream()
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return;
        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::UnmarkEndOfStream(), m_identifier);
    });
}

MediaPlayer::ReadyState MediaSourcePrivateRemote::mediaPlayerReadyState() const
{
    return m_mediaPlayerReadyState;
}

void MediaSourcePrivateRemote::setMediaPlayerReadyState(MediaPlayer::ReadyState readyState)
{
    // Call from MediaSource's dispatcher.
    if (m_mediaPlayerReadyState > MediaPlayer::ReadyState::HaveCurrentData && readyState == MediaPlayer::ReadyState::HaveCurrentData) {
        RefPtr player = m_mediaPlayerPrivate.get();
        auto currentTime = player->currentTime();
        auto buffered = this->buffered();
        auto duration = this->duration();
        ALWAYS_LOG(LOGIDENTIFIER, "stall detected at:", currentTime, " duration:", duration, " buffered:", buffered);
    }
    m_mediaPlayerReadyState = readyState;
    ensureOnDispatcher([protectedThis = Ref { *this }, this, readyState] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return;
        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::SetMediaPlayerReadyState(readyState), m_identifier);
    });
}

void MediaSourcePrivateRemote::setTimeFudgeFactor(const MediaTime& fudgeFactor)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, this, fudgeFactor] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::SetTimeFudgeFactor(fudgeFactor), m_identifier);
        MediaSourcePrivate::setTimeFudgeFactor(fudgeFactor);
    });
}

RefPtr<MediaSourcePrivateClient> MediaSourcePrivateRemote::MessageReceiver::client() const
{
    if (RefPtr parent = m_parent.get()) {
        if (RefPtr client = parent->client())
            return client;
    }
    return nullptr;
}

void MediaSourcePrivateRemote::MessageReceiver::proxyWaitForTarget(const WebCore::SeekTarget& target, CompletionHandler<void(MediaTimePromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (auto client = this->client()) {
        client->waitForTarget(target)->whenSettled(MediaSourcePrivateRemote::queue(), WTFMove(completionHandler));
        return;
    }
    completionHandler(makeUnexpected(PlatformMediaError::ClientDisconnected));
}

void MediaSourcePrivateRemote::MessageReceiver::proxySeekToTime(const MediaTime& time, CompletionHandler<void(MediaPromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (auto client = this->client()) {
        client->seekToTime(time)->whenSettled(MediaSourcePrivateRemote::queue(), WTFMove(completionHandler));
        return;
    }
    completionHandler(makeUnexpected(PlatformMediaError::SourceRemoved));
}

void MediaSourcePrivateRemote::MessageReceiver::mediaSourcePrivateShuttingDown(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (RefPtr parent = m_parent.get()) {
        parent->m_shutdown = true;
        parent->m_mediaPlayerReadyState = MediaPlayer::ReadyState::HaveNothing;
    };
    completionHandler();
}

MediaSourcePrivateRemote::MessageReceiver::MessageReceiver(MediaSourcePrivateRemote& parent)
    : m_parent(parent)
{
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
