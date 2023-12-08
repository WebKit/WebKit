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
    return WorkQueue::main();
}

Ref<MediaSourcePrivateRemote> MediaSourcePrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& mediaPlayerPrivate, MediaSourcePrivateClient& client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateRemote(gpuProcessConnection, identifier, mimeTypeCache, mediaPlayerPrivate, client));
    client.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

void MediaSourcePrivateRemote::ensureOnDispatcherSync(Function<void()>&& function) const
{
    if (m_dispatcher->isCurrent())
        function();
    else
        queue().dispatchSync(WTFMove(function));
}

void MediaSourcePrivateRemote::ensureOnDispatcher(Function<void()>&& function) const
{
    if (m_dispatcher->isCurrent())
        function();
    else
        m_dispatcher->dispatch(WTFMove(function));
}

MediaSourcePrivateRemote::MediaSourcePrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& mediaPlayerPrivate, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_receiver(MessageReceiver::create(*this))
    , m_dispatcher(RunLoop::current())
    , m_identifier(identifier)
    , m_mimeTypeCache(mimeTypeCache)
    , m_mediaPlayerPrivate(mediaPlayerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_mediaPlayerPrivate->mediaPlayerLogger())
    , m_logIdentifier(m_mediaPlayerPrivate->mediaPlayerLogIdentifier())
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
    assertIsCurrent(clientDispatcher());

    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->connection().removeWorkQueueMessageReceiver(Messages::MediaSourcePrivateRemoteMessageReceiver::messageReceiverName(), m_identifier.toUInt64());
}

MediaSourcePrivate::AddStatus MediaSourcePrivateRemote::addSourceBuffer(const ContentType& contentType, bool, RefPtr<SourceBufferPrivate>& outPrivate)
{
    assertIsCurrent(clientDispatcher());

    AddStatus returnedStatus;
    DEBUG_LOG(LOGIDENTIFIER, contentType);
    ensureOnDispatcherSync([protectedThis = Ref { *this }, this, &returnedStatus, contentTypeString = contentType.raw().isolatedCopy(), &outPrivate] {
        ContentType contentType { contentTypeString };
        MediaEngineSupportParameters parameters;
        parameters.isMediaSource = true;
        parameters.type = contentType;
        if (m_mimeTypeCache.supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported) {
            returnedStatus = AddStatus::NotSupported;
            return;
        }

        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !m_mediaPlayerPrivate) {
            returnedStatus = AddStatus::NotSupported;
            return;
        }

        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteMediaSourceProxy::AddSourceBuffer(WTFMove(contentType)), m_identifier);
        auto [status, remoteSourceBufferIdentifier] = sendResult.takeReplyOr(AddStatus::NotSupported, std::nullopt);

        if (status == AddStatus::Ok) {
            ASSERT(remoteSourceBufferIdentifier.has_value());
            auto newSourceBuffer = SourceBufferPrivateRemote::create(*gpuProcessConnection, *remoteSourceBufferIdentifier, *this, *m_mediaPlayerPrivate);
            outPrivate = newSourceBuffer.copyRef();
            m_sourceBuffers.append(WTFMove(newSourceBuffer));
        }
        returnedStatus = status;
    });
    return returnedStatus;
}

void MediaSourcePrivateRemote::durationChanged(const MediaTime& duration)
{
    assertIsCurrent(clientDispatcher());

    ensureOnDispatcher([protectedThis = Ref { *this }, this, duration] {
        MediaSourcePrivate::durationChanged(duration);
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning())
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::DurationChanged(duration), m_identifier);
    });
}

void MediaSourcePrivateRemote::bufferedChanged(const PlatformTimeRanges& buffered)
{
    // Called from SourceBufferPrivateRemote
    assertIsCurrent(m_dispatcher);

    ensureOnDispatcher([protectedThis = Ref { *this }, this, buffered] {
        MediaSourcePrivate::bufferedChanged(buffered);
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning())
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::BufferedChanged(buffered), m_identifier);
    });
}

void MediaSourcePrivateRemote::markEndOfStream(EndOfStreamStatus status)
{
    assertIsCurrent(clientDispatcher());

    ensureOnDispatcher([protectedThis = Ref { *this }, this, status] {
        if (auto gpuProcessConnection = m_gpuProcessConnection.get())
            gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::MarkEndOfStream(status), m_identifier);
        MediaSourcePrivate::markEndOfStream(status);
    });
}

void MediaSourcePrivateRemote::unmarkEndOfStream()
{
    assertIsCurrent(clientDispatcher());

    ensureOnDispatcher([protectedThis = Ref { *this }, this] {
        // FIXME(125159): implement unmarkEndOfStream()
        if (auto gpuProcessConnection = m_gpuProcessConnection.get())
            gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::UnmarkEndOfStream(), m_identifier);
        MediaSourcePrivate::unmarkEndOfStream();
    });
}

MediaPlayer::ReadyState MediaSourcePrivateRemote::mediaPlayerReadyState() const
{
    assertIsCurrent(m_dispatcher);

    return m_readyState;
}

void MediaSourcePrivateRemote::setMediaPlayerReadyState(MediaPlayer::ReadyState readyState)
{
    assertIsCurrent(clientDispatcher());

    m_readyState = readyState;
    ensureOnDispatcher([protectedThis = Ref { *this }, this, readyState] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning())
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::SetMediaPlayerReadyState(readyState), m_identifier);
    });
}

void MediaSourcePrivateRemote::setTimeFudgeFactor(const MediaTime& fudgeFactor)
{
    assertIsCurrent(clientDispatcher());

    ensureOnDispatcher([protectedThis = Ref { *this }, this, fudgeFactor] {
        auto gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning())
            return;

        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::SetTimeFudgeFactor(fudgeFactor), m_identifier);
        MediaSourcePrivate::setTimeFudgeFactor(fudgeFactor);
    });
}

void MediaSourcePrivateRemote::MessageReceiver::proxyWaitForTarget(const WebCore::SeekTarget& target, CompletionHandler<void(MediaTimePromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (RefPtr parent = m_parent.get()) {
        invokeAsync(parent->clientDispatcher(), [client = parent->client(), target] {
            return client->waitForTarget(target);
        })->whenSettled(parent->m_dispatcher, WTFMove(completionHandler));
        return;
    }
    completionHandler(makeUnexpected(PlatformMediaError::ClientDisconnected));
}

void MediaSourcePrivateRemote::MessageReceiver::proxySeekToTime(const MediaTime& time, CompletionHandler<void(MediaPromise::Result&&)>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (RefPtr parent = m_parent.get()) {
        invokeAsync(parent->clientDispatcher(), [client = parent->client(), time] {
            return client->seekToTime(time);
        })->whenSettled(parent->m_dispatcher, WTFMove(completionHandler));
        return;
    }
    completionHandler(makeUnexpected(PlatformMediaError::SourceRemoved));
}

void MediaSourcePrivateRemote::MessageReceiver::mediaSourcePrivateShuttingDown(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(MediaSourcePrivateRemote::queue());

    if (RefPtr parent = m_parent.get()) {
        parent->m_shutdown = true;
        parent->m_readyState = MediaPlayer::ReadyState::HaveNothing;
        for (auto& sourceBuffer : parent->m_sourceBuffers)
            sourceBuffer->disconnect();
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
