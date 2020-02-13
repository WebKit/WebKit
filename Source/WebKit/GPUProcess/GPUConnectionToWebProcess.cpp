/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "GPUConnectionToWebProcess.h"

#if ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcess.h"
#include "GPUProcessMessages.h"
#include "GPUProcessProxyMessages.h"
#include "LibWebRTCCodecsProxy.h"
#include "LibWebRTCCodecsProxyMessages.h"
#include "Logging.h"
#include "RemoteAudioMediaStreamTrackRendererManager.h"
#include "RemoteAudioMediaStreamTrackRendererManagerMessages.h"
#include "RemoteAudioMediaStreamTrackRendererMessages.h"
#include "RemoteLayerTreeDrawingAreaProxyMessages.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "RemoteMediaRecorderManager.h"
#include "RemoteMediaRecorderManagerMessages.h"
#include "RemoteMediaRecorderMessages.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteMediaResourceManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerManager.h"
#include "RemoteSampleBufferDisplayLayerManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerMessages.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include "UserMediaCaptureManagerProxy.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebProcessMessages.h"

#include <WebCore/MockRealtimeMediaSourceCenter.h>

namespace WebKit {
using namespace WebCore;

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
class GPUProxyForCapture final : public UserMediaCaptureManagerProxy::ConnectionProxy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GPUProxyForCapture(GPUConnectionToWebProcess& process)
        : m_process(process)
    {
    }

private:
    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& receiver) final { }
    void removeMessageReceiver(IPC::StringReference messageReceiverName) final { }
    IPC::Connection& connection() final { return m_process.connection(); }

    GPUConnectionToWebProcess& m_process;
};
#endif

Ref<GPUConnectionToWebProcess> GPUConnectionToWebProcess::create(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier connectionIdentifier, PAL::SessionID sessionID)
{
    return adoptRef(*new GPUConnectionToWebProcess(gpuProcess, webProcessIdentifier, connectionIdentifier, sessionID));
}

GPUConnectionToWebProcess::GPUConnectionToWebProcess(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier connectionIdentifier, PAL::SessionID sessionID)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier, *this))
    , m_gpuProcess(gpuProcess)
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_sessionID(sessionID)
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_connection->open();
}

GPUConnectionToWebProcess::~GPUConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();
}

void GPUConnectionToWebProcess::didClose(IPC::Connection&)
{
}

Logger& GPUConnectionToWebProcess::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        m_logger->setEnabled(this, m_sessionID.isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}

void GPUConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection& connection, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
    WTFLogAlways("Received an invalid message \"%s.%s\" from the web process.\n", messageReceiverName.toString().data(), messageName.toString().data());
    CRASH();
}

RemoteMediaResourceManager& GPUConnectionToWebProcess::remoteMediaResourceManager()
{
    if (!m_remoteMediaResourceManager)
        m_remoteMediaResourceManager = makeUnique<RemoteMediaResourceManager>();

    return *m_remoteMediaResourceManager;
}

RemoteMediaPlayerManagerProxy& GPUConnectionToWebProcess::remoteMediaPlayerManagerProxy()
{
    if (!m_remoteMediaPlayerManagerProxy)
        m_remoteMediaPlayerManagerProxy = makeUnique<RemoteMediaPlayerManagerProxy>(*this);

    return *m_remoteMediaPlayerManagerProxy;
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
UserMediaCaptureManagerProxy& GPUConnectionToWebProcess::userMediaCaptureManagerProxy()
{
    if (!m_userMediaCaptureManagerProxy)
        m_userMediaCaptureManagerProxy = makeUnique<UserMediaCaptureManagerProxy>(makeUniqueRef<GPUProxyForCapture>(*this));

    return *m_userMediaCaptureManagerProxy;
}

RemoteMediaRecorderManager& GPUConnectionToWebProcess::mediaRecorderManager()
{
    if (!m_remoteMediaRecorderManager)
        m_remoteMediaRecorderManager = makeUnique<RemoteMediaRecorderManager>(*this);

    return *m_remoteMediaRecorderManager;
}

#if ENABLE(VIDEO_TRACK)
RemoteAudioMediaStreamTrackRendererManager& GPUConnectionToWebProcess::audioTrackRendererManager()
{
    if (!m_audioTrackRendererManager)
        m_audioTrackRendererManager = makeUnique<RemoteAudioMediaStreamTrackRendererManager>();

    return *m_audioTrackRendererManager;
}

RemoteSampleBufferDisplayLayerManager& GPUConnectionToWebProcess::sampleBufferDisplayLayerManager()
{
    if (!m_sampleBufferDisplayLayerManager)
        m_sampleBufferDisplayLayerManager = makeUnique<RemoteSampleBufferDisplayLayerManager>(m_connection.copyRef());

    return *m_sampleBufferDisplayLayerManager;
}
#endif
#endif

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
LibWebRTCCodecsProxy& GPUConnectionToWebProcess::libWebRTCCodecsProxy()
{
    if (!m_libWebRTCCodecsProxy)
        m_libWebRTCCodecsProxy = makeUnique<LibWebRTCCodecsProxy>(*this);

    return *m_libWebRTCCodecsProxy;
}
#endif

void GPUConnectionToWebProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerManagerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return;
    } else if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceivePlayerMessage(connection, decoder);
        return;
    } else if (decoder.messageReceiverName() == Messages::RemoteMediaResourceManager::messageReceiverName()) {
        remoteMediaResourceManager().didReceiveMessage(connection, decoder);
        return;
    }
#if ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::UserMediaCaptureManagerProxy::messageReceiverName()) {
        userMediaCaptureManagerProxy().didReceiveMessageFromGPUProcess(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaRecorderManager::messageReceiverName()) {
        mediaRecorderManager().didReceiveMessageFromWebProcess(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaRecorder::messageReceiverName()) {
        mediaRecorderManager().didReceiveRemoteMediaRecorderMessage(connection, decoder);
        return;
    }
#if PLATFORM(COCOA) && ENABLE(VIDEO_TRACK)
    if (decoder.messageReceiverName() == Messages::RemoteAudioMediaStreamTrackRendererManager::messageReceiverName()) {
        audioTrackRendererManager().didReceiveMessageFromWebProcess(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::RemoteAudioMediaStreamTrackRenderer::messageReceiverName()) {
        audioTrackRendererManager().didReceiveRendererMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveMessageFromWebProcess(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::RemoteSampleBufferDisplayLayer::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveLayerMessage(connection, decoder);
        return;
    }
#endif
#endif
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::LibWebRTCCodecsProxy::messageReceiverName()) {
        libWebRTCCodecsProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return;
    }
#endif
}

void GPUConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerManagerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceiveSyncMessageFromWebProcess(connection, decoder, replyEncoder);
        return;
    }

#if ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::UserMediaCaptureManagerProxy::messageReceiverName()) {
        userMediaCaptureManagerProxy().didReceiveSyncMessageFromGPUProcess(connection, decoder, replyEncoder);
        return;
    }
#if PLATFORM(COCOA) && ENABLE(VIDEO_TRACK)
    if (decoder.messageReceiverName() == Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveSyncMessageFromWebProcess(connection, decoder, replyEncoder);
        return;
    }
#endif
#endif

    ASSERT_NOT_REACHED();
}

const String& GPUConnectionToWebProcess::mediaCacheDirectory() const
{
    return m_gpuProcess->mediaCacheDirectory(m_sessionID);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
const String& GPUConnectionToWebProcess::mediaKeysStorageDirectory() const
{
    return m_gpuProcess->mediaKeysStorageDirectory(m_sessionID);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
