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
#include "RemoteMediaSourceProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "GPUConnectionToWebProcess.h"
#include "MediaSourcePrivateRemoteMessageReceiverMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferProxy.h"
#include <WebCore/ContentType.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SourceBufferPrivate.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSourceProxy);

RemoteMediaSourceProxy::RemoteMediaSourceProxy(GPUConnectionToWebProcess& connectionToWebProcess, RemoteMediaSourceIdentifier identifier, bool webMParserEnabled, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_identifier(identifier)
    , m_webMParserEnabled(webMParserEnabled)
    , m_remoteMediaPlayerProxy(remoteMediaPlayerProxy)
{
    connectionToWebProcess.messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
}

RemoteMediaSourceProxy::~RemoteMediaSourceProxy()
{
    disconnect();
}

void RemoteMediaSourceProxy::disconnect()
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;

    connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64());
    m_connectionToWebProcess = nullptr;
}

void RemoteMediaSourceProxy::setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate)
{
    ASSERT(!m_private);
    m_private = WTFMove(mediaSourcePrivate);
}

Ref<MediaTimePromise> RemoteMediaSourceProxy::waitForTarget(const SeekTarget& target)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return MediaTimePromise::createAndReject(PlatformMediaError::IPCError);

    return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::MediaSourcePrivateRemoteMessageReceiver::ProxyWaitForTarget(target), m_identifier);
}

Ref<MediaPromise> RemoteMediaSourceProxy::seekToTime(const MediaTime& time)
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return MediaPromise::createAndReject(PlatformMediaError::IPCError);

    return connection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::MediaSourcePrivateRemoteMessageReceiver::ProxySeekToTime(time), m_identifier);
}

#if !RELEASE_LOG_DISABLED
void RemoteMediaSourceProxy::setLogIdentifier(const void*)
{
    notImplemented();
}
#endif

void RemoteMediaSourceProxy::failedToCreateRenderer(RendererType)
{
    notImplemented();
}

void RemoteMediaSourceProxy::addSourceBuffer(const WebCore::ContentType& contentType, AddSourceBufferCallback&& callback)
{
    auto connection = m_connectionToWebProcess.get();
    if (!m_remoteMediaPlayerProxy || !connection)
        return;

    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    MediaSourcePrivate::AddStatus status = m_private->addSourceBuffer(contentType, m_webMParserEnabled, sourceBufferPrivate);

    std::optional<RemoteSourceBufferIdentifier> remoteSourceIdentifier;
    if (status == MediaSourcePrivate::AddStatus::Ok) {
        auto identifier = RemoteSourceBufferIdentifier::generate();
        auto remoteSourceBufferProxy = RemoteSourceBufferProxy::create(*connection, identifier, sourceBufferPrivate.releaseNonNull(), *m_remoteMediaPlayerProxy);
        m_sourceBuffers.append(WTFMove(remoteSourceBufferProxy));
        remoteSourceIdentifier = identifier;
    }

    callback(status, remoteSourceIdentifier);
}

void RemoteMediaSourceProxy::durationChanged(const MediaTime& duration)
{
    if (m_private)
        m_private->durationChanged(duration);
}

void RemoteMediaSourceProxy::bufferedChanged(WebCore::PlatformTimeRanges&& buffered)
{
    if (m_private)
        m_private->bufferedChanged(WTFMove(buffered));
}

void RemoteMediaSourceProxy::markEndOfStream(WebCore::MediaSourcePrivate::EndOfStreamStatus status )
{
    if (m_private)
        m_private->markEndOfStream(status);
}

void RemoteMediaSourceProxy::unmarkEndOfStream()
{
    if (m_private)
        m_private->unmarkEndOfStream();
}


void RemoteMediaSourceProxy::setMediaPlayerReadyState(WebCore::MediaPlayerEnums::ReadyState readyState)
{
    if (m_private)
        m_private->setMediaPlayerReadyState(readyState);
}

void RemoteMediaSourceProxy::setTimeFudgeFactor(const MediaTime& fudgeFactor)
{
    if (m_private)
        m_private->setTimeFudgeFactor(fudgeFactor);
}

void RemoteMediaSourceProxy::shutdown()
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;

    for (auto sourceBuffer : m_sourceBuffers)
        sourceBuffer->shutdown();

    connection->connection().sendWithAsyncReply(Messages::MediaSourcePrivateRemoteMessageReceiver::MediaSourcePrivateShuttingDown(), [this, protectedThis = Ref { *this }, protectedConnection = Ref { *connection }] {
        disconnect();
    }, m_identifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
