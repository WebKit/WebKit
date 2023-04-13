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
#include "MediaSourcePrivateRemoteMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferProxy.h"
#include <WebCore/ContentType.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SourceBufferPrivate.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaSourceProxy::RemoteMediaSourceProxy(GPUConnectionToWebProcess& connectionToWebProcess, RemoteMediaSourceIdentifier identifier, bool webMParserEnabled, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_identifier(identifier)
    , m_webMParserEnabled(webMParserEnabled)
    , m_remoteMediaPlayerProxy(remoteMediaPlayerProxy)
{
    m_connectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
}

RemoteMediaSourceProxy::~RemoteMediaSourceProxy()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteMediaSourceProxy::setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate)
{
    ASSERT(!m_private);
    m_private = WTFMove(mediaSourcePrivate);
}

MediaTime RemoteMediaSourceProxy::duration() const
{
    return m_duration;
}

const PlatformTimeRanges& RemoteMediaSourceProxy::buffered() const
{
    return m_buffered;
}

void RemoteMediaSourceProxy::seekToTime(const MediaTime& time)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::MediaSourcePrivateRemote::SeekToTime(time), m_identifier);
}

void RemoteMediaSourceProxy::monitorSourceBuffers()
{
    notImplemented();
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
    if (!m_remoteMediaPlayerProxy || !m_connectionToWebProcess)
        return;

    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    MediaSourcePrivate::AddStatus status = m_private->addSourceBuffer(contentType, m_webMParserEnabled, sourceBufferPrivate);

    std::optional<RemoteSourceBufferIdentifier> remoteSourceIdentifier;
    if (status == MediaSourcePrivate::AddStatus::Ok) {
        auto identifier = RemoteSourceBufferIdentifier::generate();
        auto remoteSourceBufferProxy = RemoteSourceBufferProxy::create(*m_connectionToWebProcess, identifier, sourceBufferPrivate.releaseNonNull(), *m_remoteMediaPlayerProxy);
        m_sourceBuffers.append(WTFMove(remoteSourceBufferProxy));
        remoteSourceIdentifier = identifier;
    }

    callback(status, remoteSourceIdentifier);
}

void RemoteMediaSourceProxy::durationChanged(const MediaTime& duration)
{
    if (m_duration == duration)
        return;

    m_duration = duration;
    if (m_private)
        m_private->durationChanged(duration);
}

void RemoteMediaSourceProxy::bufferedChanged(WebCore::PlatformTimeRanges&& buffered)
{
    m_buffered = WTFMove(buffered);
    if (m_private)
        m_private->bufferedChanged(m_buffered);
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


void RemoteMediaSourceProxy::setReadyState(WebCore::MediaPlayerEnums::ReadyState readyState)
{
    if (m_private)
        m_private->setReadyState(readyState);
}

void RemoteMediaSourceProxy::setIsSeeking(bool isSeeking)
{
    if (m_private)
        m_private->setIsSeeking(isSeeking);
}

void RemoteMediaSourceProxy::waitForSeekCompleted()
{
    if (m_private)
        m_private->waitForSeekCompleted();
}

void RemoteMediaSourceProxy::seekCompleted()
{
    if (m_private)
        m_private->seekCompleted();
}

void RemoteMediaSourceProxy::setTimeFudgeFactor(const MediaTime& fudgeFactor)
{
    if (m_private)
        m_private->setTimeFudgeFactor(fudgeFactor);
}

void RemoteMediaSourceProxy::shutdown()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().sendWithAsyncReply(Messages::MediaSourcePrivateRemote::MediaSourcePrivateShuttingDown(), [self = RefPtr { this }] { }, m_identifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
