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
#include "RemoteSourceBufferProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "Connection.h"
#include "DataReference.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SourceBufferPrivateRemoteMessages.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteSourceBufferProxy> RemoteSourceBufferProxy::create(RemoteSourceBufferIdentifier identifier, GPUConnectionToWebProcess& connectionToWebProcess, Ref<SourceBufferPrivate>&& sourceBufferPrivate)
{
    auto remoteSourceBufferProxy = adoptRef(*new RemoteSourceBufferProxy(identifier, connectionToWebProcess, WTFMove(sourceBufferPrivate)));
    return remoteSourceBufferProxy;
}

RemoteSourceBufferProxy::RemoteSourceBufferProxy(RemoteSourceBufferIdentifier identifier, GPUConnectionToWebProcess& connectionToWebProcess, Ref<SourceBufferPrivate>&& sourceBufferPrivate)
    : m_identifier(identifier)
    , m_connectionToWebProcess(connectionToWebProcess)
    , m_sourceBufferPrivate(WTFMove(sourceBufferPrivate))
{
    m_connectionToWebProcess.messageReceiverMap().addMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
    m_sourceBufferPrivate->setClient(this);
}

RemoteSourceBufferProxy::~RemoteSourceBufferProxy()
{
    m_connectionToWebProcess.messageReceiverMap().removeMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveInitializationSegment(const InitializationSegment&)
{
    notImplemented();
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveSample(WebCore::MediaSample&)
{
    notImplemented();
}

bool RemoteSourceBufferProxy::sourceBufferPrivateHasAudio() const
{
    notImplemented();
    return false;
}

bool RemoteSourceBufferProxy::sourceBufferPrivateHasVideo() const
{
    notImplemented();
    return false;
}

void RemoteSourceBufferProxy::sourceBufferPrivateReenqueSamples(const AtomString& trackID)
{
    notImplemented();
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidBecomeReadyForMoreSamples(const AtomString& trackID)
{
    notImplemented();
}

MediaTime RemoteSourceBufferProxy::sourceBufferPrivateFastSeekTimeForMediaTime(const MediaTime&, const MediaTime&, const MediaTime&)
{
    notImplemented();
    return { };
}

void RemoteSourceBufferProxy::sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::AppendResult appendResult)
{
    m_connectionToWebProcess.connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateAppendComplete(appendResult), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveRenderingError(int errorCode)
{
    notImplemented();
}

void RemoteSourceBufferProxy::append(const IPC::DataReference& data)
{
    m_sourceBufferPrivate->append(data.vector());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
