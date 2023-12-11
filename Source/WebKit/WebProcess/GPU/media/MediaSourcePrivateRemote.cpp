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
#include "MediaSourcePrivateRemoteMessages.h"
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferIdentifier.h"
#include "SourceBufferPrivateRemote.h"
#include <WebCore/NotImplemented.h>
#include <wtf/NativePromise.h>
#include <wtf/RunLoop.h>

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {

using namespace WebCore;

Ref<MediaSourcePrivateRemote> MediaSourcePrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& mediaPlayerPrivate, MediaSourcePrivateClient& client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateRemote(gpuProcessConnection, identifier, mimeTypeCache, mediaPlayerPrivate, client));
    client.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateRemote::MediaSourcePrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& mediaPlayerPrivate, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
    , m_gpuProcessConnection(gpuProcessConnection)
    , m_identifier(identifier)
    , m_mimeTypeCache(mimeTypeCache)
    , m_mediaPlayerPrivate(mediaPlayerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_mediaPlayerPrivate->mediaPlayerLogger())
    , m_logIdentifier(m_mediaPlayerPrivate->mediaPlayerLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

    gpuProcessConnection.messageReceiverMap().addMessageReceiver(Messages::MediaSourcePrivateRemote::messageReceiverName(), m_identifier.toUInt64(), *this);

#if !RELEASE_LOG_DISABLED
    client.setLogIdentifier(m_logIdentifier);
#endif
}

MediaSourcePrivateRemote::~MediaSourcePrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::MediaSourcePrivateRemote::messageReceiverName(), m_identifier.toUInt64());
}

MediaSourcePrivate::AddStatus MediaSourcePrivateRemote::addSourceBuffer(const ContentType& contentType, bool, RefPtr<SourceBufferPrivate>& outPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (m_mimeTypeCache.supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported)
        return AddStatus::NotSupported;

    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !m_mediaPlayerPrivate)
        return AddStatus::NotSupported;

    auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteMediaSourceProxy::AddSourceBuffer(contentType), m_identifier);
    auto [status, remoteSourceBufferIdentifier] = sendResult.takeReplyOr(AddStatus::NotSupported, std::nullopt);

    if (status == AddStatus::Ok) {
        ASSERT(remoteSourceBufferIdentifier.has_value());
        auto newSourceBuffer = SourceBufferPrivateRemote::create(*gpuProcessConnection, *remoteSourceBufferIdentifier, *this, *m_mediaPlayerPrivate);
        outPrivate = newSourceBuffer.copyRef();
        m_sourceBuffers.append(WTFMove(newSourceBuffer));
    }

    return status;
}

void MediaSourcePrivateRemote::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::DurationChanged(duration), m_identifier);
}

void MediaSourcePrivateRemote::bufferedChanged(const PlatformTimeRanges& buffered)
{
    MediaSourcePrivate::bufferedChanged(buffered);
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::BufferedChanged(buffered), m_identifier);
}

void MediaSourcePrivateRemote::markEndOfStream(EndOfStreamStatus status)
{
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::MarkEndOfStream(status), m_identifier);
    MediaSourcePrivate::markEndOfStream(status);
}

void MediaSourcePrivateRemote::unmarkEndOfStream()
{
    // FIXME(125159): implement unmarkEndOfStream()
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::UnmarkEndOfStream(), m_identifier);
    MediaSourcePrivate::unmarkEndOfStream();
}

MediaPlayer::ReadyState MediaSourcePrivateRemote::mediaPlayerReadyState() const
{
    return m_readyState;
}

void MediaSourcePrivateRemote::setMediaPlayerReadyState(MediaPlayer::ReadyState readyState)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    m_readyState = readyState;
    gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::SetMediaPlayerReadyState(readyState), m_identifier);
}

void MediaSourcePrivateRemote::setTimeFudgeFactor(const MediaTime& fudgeFactor)
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning())
        return;

    MediaSourcePrivate::setTimeFudgeFactor(fudgeFactor);
    gpuProcessConnection->connection().send(Messages::RemoteMediaSourceProxy::SetTimeFudgeFactor(fudgeFactor), m_identifier);
}

void MediaSourcePrivateRemote::proxyWaitForTarget(const WebCore::SeekTarget& target, CompletionHandler<void(MediaTimePromise::Result&&)>&& completionHandler)
{
    if (RefPtr client = this->client())
        client->waitForTarget(target)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
    else
        completionHandler(makeUnexpected(PlatformMediaError::ClientDisconnected));
}

void MediaSourcePrivateRemote::proxySeekToTime(const MediaTime& time, CompletionHandler<void(MediaPromise::Result&&)>&& completionHandler)
{
    if (RefPtr client = this->client())
        client->seekToTime(time)->whenSettled(RunLoop::current(), WTFMove(completionHandler));
    else
        completionHandler(makeUnexpected(PlatformMediaError::SourceRemoved));
}

void MediaSourcePrivateRemote::mediaSourcePrivateShuttingDown(CompletionHandler<void()>&& completionHandler)
{
    m_shutdown = true;
    m_readyState = MediaPlayer::ReadyState::HaveNothing;

    for (auto& sourceBuffer : m_sourceBuffers)
        downcast<SourceBufferPrivateRemote>(sourceBuffer)->disconnect();
    completionHandler();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
