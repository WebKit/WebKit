/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "RemoteMediaSourceProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "GPUConnectionToWebProcess.h"
#include "MediaSourcePrivateRemoteMessageReceiverMessages.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferProxy.h"
#include <WebCore/ContentType.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SourceBufferPrivate.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSourceProxy);

RemoteMediaSourceProxy::RemoteMediaSourceProxy(RemoteMediaPlayerManagerProxy& manager, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
    : m_manager(manager)
    , m_identifier(identifier)
    , m_remoteMediaPlayerProxy(remoteMediaPlayerProxy)
{
    assertIsMainRunLoop();

    connectionToWebProcess()->messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
    manager.registerMediaSource(m_identifier, *this);
}

RemoteMediaSourceProxy::~RemoteMediaSourceProxy()
{
    disconnect();
}

void RemoteMediaSourceProxy::setMediaPlayers(RemoteMediaPlayerProxy& remoteMediaPlayerProxy, WebCore::MediaPlayerPrivateInterface* mediaPlayerPrivate)
{
    m_remoteMediaPlayerProxy = remoteMediaPlayerProxy;
    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->setMediaPlayer(remoteMediaPlayerProxy);
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->setPlayer(mediaPlayerPrivate);
}

void RemoteMediaSourceProxy::disconnect()
{
    RefPtr connection = connectionToWebProcess();
    if (!connection)
        return;

    connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64());
    m_manager = nullptr;
}

void RemoteMediaSourceProxy::setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate)
{
    ASSERT(!m_private);
    m_private = WTFMove(mediaSourcePrivate);
}

void RemoteMediaSourceProxy::reOpen()
{
    ASSERT(m_private);
}

Ref<MediaTimePromise> RemoteMediaSourceProxy::waitForTarget(const SeekTarget& target)
{
    if (RefPtr connection = connectionToWebProcess())
        return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::MediaSourcePrivateRemoteMessageReceiver::ProxyWaitForTarget(target), m_identifier);

    return MediaTimePromise::createAndReject(PlatformMediaError::IPCError);
}

#if !RELEASE_LOG_DISABLED
void RemoteMediaSourceProxy::setLogIdentifier(uint64_t)
{
    notImplemented();
}
#endif

void RemoteMediaSourceProxy::failedToCreateRenderer(RendererType)
{
    notImplemented();
}

void RemoteMediaSourceProxy::addSourceBuffer(const WebCore::ContentType& contentType, const WebCore::MediaSourceConfiguration& configuration, AddSourceBufferCallback&& callback)
{
    RefPtr connection = connectionToWebProcess();
    RefPtr mediaSourcePrivate = this->mediaSourcePrivate();
    if (!m_remoteMediaPlayerProxy || !connection || !mediaSourcePrivate) {
        callback(MediaSourcePrivate::AddStatus::NotSupported, { });
        return;
    }

    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    MediaSourcePrivate::AddStatus status = mediaSourcePrivate->addSourceBuffer(contentType, configuration, sourceBufferPrivate);

    std::optional<RemoteSourceBufferIdentifier> remoteSourceIdentifier;
    if (status == MediaSourcePrivate::AddStatus::Ok) {
        auto identifier = RemoteSourceBufferIdentifier::generate();
        Ref remoteMediaPlayerProxy { *m_remoteMediaPlayerProxy };
        auto remoteSourceBufferProxy = RemoteSourceBufferProxy::create(*connection, identifier, sourceBufferPrivate.releaseNonNull(), remoteMediaPlayerProxy);
        m_sourceBuffers.append(WTFMove(remoteSourceBufferProxy));
        remoteSourceIdentifier = identifier;
    }

    callback(status, remoteSourceIdentifier);
}

void RemoteMediaSourceProxy::durationChanged(const MediaTime& duration)
{
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->durationChanged(duration);
}

void RemoteMediaSourceProxy::bufferedChanged(WebCore::PlatformTimeRanges&& buffered)
{
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->bufferedChanged(WTFMove(buffered));
}

void RemoteMediaSourceProxy::markEndOfStream(WebCore::MediaSourcePrivate::EndOfStreamStatus status )
{
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->markEndOfStream(status);
}

void RemoteMediaSourceProxy::unmarkEndOfStream()
{
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->unmarkEndOfStream();
}


void RemoteMediaSourceProxy::setMediaPlayerReadyState(WebCore::MediaPlayerEnums::ReadyState readyState)
{
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->setMediaPlayerReadyState(readyState);
}

void RemoteMediaSourceProxy::setTimeFudgeFactor(const MediaTime& fudgeFactor)
{
    if (RefPtr protectedPrivate = m_private)
        protectedPrivate->setTimeFudgeFactor(fudgeFactor);
}

void RemoteMediaSourceProxy::attached()
{
}

void RemoteMediaSourceProxy::shutdown()
{
    assertIsMainRunLoop();

    disconnect();

    if (RefPtr manager = m_manager.get())
        manager->invalidateMediaSource(m_identifier);
}

RefPtr<GPUConnectionToWebProcess> RemoteMediaSourceProxy::connectionToWebProcess() const
{
    assertIsMainRunLoop();

    RefPtr manager = m_manager.get();
    return manager ? manager->gpuConnectionToWebProcess() : nullptr;
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaSourceProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr connection = connectionToWebProcess())
        return connection->sharedPreferencesForWebProcess();

    return std::nullopt;
}

void RemoteMediaSourceProxy::connectionToWebProcessClosed()
{
    assertIsMainRunLoop();
    for (RefPtr sourceBuffer : std::exchange(m_sourceBuffers, { }))
        sourceBuffer->connectionToWebProcessClosed();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
