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
#include "InitializationSegmentInfo.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SourceBufferPrivateRemoteMessages.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteSourceBufferProxy> RemoteSourceBufferProxy::create(GPUConnectionToWebProcess& connectionToWebProcess, RemoteSourceBufferIdentifier identifier, Ref<SourceBufferPrivate>&& sourceBufferPrivate, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
{
    auto remoteSourceBufferProxy = adoptRef(*new RemoteSourceBufferProxy(connectionToWebProcess, identifier, WTFMove(sourceBufferPrivate), remoteMediaPlayerProxy));
    return remoteSourceBufferProxy;
}

RemoteSourceBufferProxy::RemoteSourceBufferProxy(GPUConnectionToWebProcess& connectionToWebProcess, RemoteSourceBufferIdentifier identifier, Ref<SourceBufferPrivate>&& sourceBufferPrivate, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_identifier(identifier)
    , m_sourceBufferPrivate(WTFMove(sourceBufferPrivate))
    , m_remoteMediaPlayerProxy(makeWeakPtr(remoteMediaPlayerProxy))
{
    m_connectionToWebProcess.messageReceiverMap().addMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
    m_sourceBufferPrivate->setClient(this);
}

RemoteSourceBufferProxy::~RemoteSourceBufferProxy()
{
    m_connectionToWebProcess.messageReceiverMap().removeMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveInitializationSegment(const InitializationSegment& segment)
{
    if (!m_remoteMediaPlayerProxy)
        return;

    InitializationSegmentInfo segmentInfo;
    segmentInfo.duration = segment.duration;
    for (auto& audioTrackInfo : segment.audioTracks) {
        auto identifier = m_remoteMediaPlayerProxy->addRemoteAudioTrackProxy(*audioTrackInfo.track);
        segmentInfo.audioTracks.append({ MediaDescriptionInfo(*audioTrackInfo.description), identifier });
    }

    for (auto& videoTrackInfo : segment.videoTracks) {
        auto identifier = m_remoteMediaPlayerProxy->addRemoteVideoTrackProxy(*videoTrackInfo.track);
        segmentInfo.videoTracks.append({ MediaDescriptionInfo(*videoTrackInfo.description), identifier });
    }

    for (auto& textTrackInfo : segment.textTracks) {
        auto identifier = m_remoteMediaPlayerProxy->addRemoteTextTrackProxy(*textTrackInfo.track);
        segmentInfo.textTracks.append({ MediaDescriptionInfo(*textTrackInfo.description), identifier });
    }

    m_connectionToWebProcess.connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidReceiveInitializationSegment(segmentInfo), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateAppendError(bool)
{
    notImplemented();
}

void RemoteSourceBufferProxy::sourceBufferPrivateDurationChanged(const MediaTime&)
{
    notImplemented();
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidParseSample(double)
{
    notImplemented();
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidDropSample()
{
    notImplemented();
}

void RemoteSourceBufferProxy::sourceBufferPrivateStreamEndedWithDecodeError()
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
