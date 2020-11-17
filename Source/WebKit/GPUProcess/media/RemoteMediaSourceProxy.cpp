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
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferProxy.h"
#include <WebCore/ContentType.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SourceBufferPrivate.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaSourceProxy::RemoteMediaSourceProxy(RemoteMediaSourceIdentifier identifier, GPUConnectionToWebProcess& connectionToWebProcess)
    : m_identifier(identifier)
    , m_connectionToWebProcess(connectionToWebProcess)
{
    m_connectionToWebProcess.messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
}

RemoteMediaSourceProxy::~RemoteMediaSourceProxy()
{
    m_connectionToWebProcess.messageReceiverMap().removeMessageReceiver(Messages::RemoteMediaSourceProxy::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteMediaSourceProxy::setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate)
{
    ASSERT(!m_private);
    m_private = WTFMove(mediaSourcePrivate);
}

MediaTime RemoteMediaSourceProxy::duration() const
{
    notImplemented();
    return { };
}

std::unique_ptr<PlatformTimeRanges> RemoteMediaSourceProxy::buffered() const
{
    notImplemented();
    return makeUnique<PlatformTimeRanges>();
}

void RemoteMediaSourceProxy::seekToTime(const MediaTime&)
{
    notImplemented();
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
    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    MediaSourcePrivate::AddStatus status = m_private->addSourceBuffer(contentType, sourceBufferPrivate);

    Optional<RemoteSourceBufferIdentifier> remoteSourceIdentifier;
    if (status == MediaSourcePrivate::AddStatus::Ok) {
        auto identifier = RemoteSourceBufferIdentifier::generate();
        auto remoteSourceBufferProxy = RemoteSourceBufferProxy::create(identifier, m_connectionToWebProcess, sourceBufferPrivate.releaseNonNull());
        m_sourceBuffers.append(WTFMove(remoteSourceBufferProxy));
        remoteSourceIdentifier = identifier;
    }

    callback(status, remoteSourceIdentifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
