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
#include "GPUProcessConnection.h"

#if ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "LibWebRTCCodecs.h"
#include "LibWebRTCCodecsMessages.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteCDMFactory.h"
#include "RemoteCDMProxy.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteMediaPlayerManager.h"
#include "SampleBufferDisplayLayerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageMessages.h"
#include "WebProcess.h"
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/SharedBuffer.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMInstanceSessionMessages.h"
#endif

#if USE(AUDIO_SESSION)
#include "RemoteAudioSession.h"
#include "RemoteAudioSessionMessages.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "RemoteMediaSessionHelper.h"
#include "RemoteMediaSessionHelperMessages.h"
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

GPUProcessConnection::GPUProcessConnection(IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createClientConnection(connectionIdentifier, *this))
{
    m_connection->open();
}

GPUProcessConnection::~GPUProcessConnection()
{
    m_connection->invalidate();
}

void GPUProcessConnection::didClose(IPC::Connection&)
{
}

void GPUProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
SampleBufferDisplayLayerManager& GPUProcessConnection::sampleBufferDisplayLayerManager()
{
    if (!m_sampleBufferDisplayLayerManager)
        m_sampleBufferDisplayLayerManager = makeUnique<SampleBufferDisplayLayerManager>();
    return *m_sampleBufferDisplayLayerManager;
}
#endif

RemoteMediaPlayerManager& GPUProcessConnection::mediaPlayerManager()
{
    return *WebProcess::singleton().supplement<RemoteMediaPlayerManager>();
}

#if ENABLE(ENCRYPTED_MEDIA)
RemoteCDMFactory& GPUProcessConnection::cdmFactory()
{
    return *WebProcess::singleton().supplement<RemoteCDMFactory>();
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RemoteLegacyCDMFactory& GPUProcessConnection::legacyCDMFactory()
{
    return *WebProcess::singleton().supplement<RemoteLegacyCDMFactory>();
}
#endif

bool GPUProcessConnection::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::MediaPlayerPrivateRemote::messageReceiverName()) {
        WebProcess::singleton().supplement<RemoteMediaPlayerManager>()->didReceivePlayerMessage(connection, decoder);
        return true;
    }

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::UserMediaCaptureManager::messageReceiverName()) {
        if (auto* captureManager = WebProcess::singleton().supplement<UserMediaCaptureManager>())
            captureManager->didReceiveMessageFromGPUProcess(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::SampleBufferDisplayLayer::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveLayerMessage(connection, decoder);
        return true;
    }
#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#if USE(LIBWEBRTC) && PLATFORM(COCOA)
    if (decoder.messageReceiverName() == Messages::LibWebRTCCodecs::messageReceiverName()) {
        WebProcess::singleton().libWebRTCCodecs().didReceiveMessage(connection, decoder);
        return true;
    }
#endif
#if USE(AUDIO_SESSION)
    if (decoder.messageReceiverName() == Messages::RemoteAudioSession::messageReceiverName()) {
        // FIXME
        return true;
    }
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceSession::messageReceiverName()) {
        WebProcess::singleton().supplement<RemoteCDMFactory>()->didReceiveSessionMessage(connection, decoder);
        return true;
    }
#endif
    return messageReceiverMap().dispatchMessage(connection, decoder);
}

bool GPUProcessConnection::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    return messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder);
}

void GPUProcessConnection::didReceiveRemoteCommand(PlatformMediaSession::RemoteControlCommandType type, Optional<double> argument)
{
    const PlatformMediaSession::RemoteCommandArgument value { argument ? *argument : 0 };
    PlatformMediaSessionManager::sharedManager().processDidReceiveRemoteControlCommand(type, argument ? &value : nullptr);
}

void GPUProcessConnection::updateParameters(const WebPageCreationParameters& parameters)
{
#if ENABLE(VP9)
    if (m_enableVP9Decoder == parameters.shouldEnableVP9Decoder && m_enableVP9SWDecoder == parameters.shouldEnableVP9SWDecoder)
        return;

    m_enableVP9Decoder = parameters.shouldEnableVP9Decoder;
    m_enableVP9SWDecoder = parameters.shouldEnableVP9SWDecoder;
    connection().send(Messages::GPUConnectionToWebProcess::EnableVP9Decoders(parameters.shouldEnableVP9Decoder, parameters.shouldEnableVP9SWDecoder), { });
#endif
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
