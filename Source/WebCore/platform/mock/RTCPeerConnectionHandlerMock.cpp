/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "RTCPeerConnectionHandlerMock.h"

#include "MediaConstraintsMock.h"
#include "NotImplemented.h"
#include "RTCDTMFSenderHandler.h"
#include "RTCDataChannelHandler.h"
#include "RTCDataChannelHandlerMock.h"
#include "RTCNotifiersMock.h"
#include "RTCSessionDescriptionRequest.h"
#include "RTCVoidRequest.h"
#include <wtf/text/CString.h>

namespace WebCore {

std::unique_ptr<RTCPeerConnectionHandler> RTCPeerConnectionHandlerMock::create(RTCPeerConnectionHandlerClient* client)
{
    return std::make_unique<RTCPeerConnectionHandlerMock>(client);
}

RTCPeerConnectionHandlerMock::RTCPeerConnectionHandlerMock(RTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
}

bool RTCPeerConnectionHandlerMock::initialize(PassRefPtr<RTCConfigurationPrivate>)
{
    RefPtr<IceConnectionNotifier> notifier = adoptRef(new IceConnectionNotifier(m_client, RTCPeerConnectionHandlerClient::IceConnectionStateCompleted, RTCPeerConnectionHandlerClient::IceGatheringStateComplete));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    return true;
}

void RTCPeerConnectionHandlerMock::createOffer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<RTCOfferOptions>)
{
    RefPtr<SessionRequestNotifier> notifier = adoptRef(new SessionRequestNotifier(request, RTCSessionDescriptionDescriptor::create("offer", "local")));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

void RTCPeerConnectionHandlerMock::createAnswer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<RTCOfferAnswerOptions>)
{
    RefPtr<SessionRequestNotifier> notifier = adoptRef(new SessionRequestNotifier(request, RTCSessionDescriptionDescriptor::create("answer", "local")));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

RTCPeerConnectionHandlerClient::SignalingState RTCPeerConnectionHandlerMock::signalingStateFromSDP(RTCSessionDescriptionDescriptor* descriptor)
{
    if (descriptor->type() == "offer" && descriptor->sdp() == "local")
        return RTCPeerConnectionHandlerClient::SignalingStateHaveLocalOffer;
    if (descriptor->type() == "offer" && descriptor->sdp() == "remote")
        return RTCPeerConnectionHandlerClient::SignalingStateHaveRemoteOffer;
    if (descriptor->type() == "pranswer" && descriptor->sdp() == "local")
        return RTCPeerConnectionHandlerClient::SignalingStateHaveLocalPrAnswer;
    if (descriptor->type() == "pranswer" && descriptor->sdp() == "remote")
        return RTCPeerConnectionHandlerClient::SignalingStateHaveRemotePrAnswer;

    return RTCPeerConnectionHandlerClient::SignalingStateStable;
}

void RTCPeerConnectionHandlerMock::setLocalDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> descriptor)
{
    RefPtr<VoidRequestNotifier> voidNotifier;
    RefPtr<SignalingStateNotifier> signalingNotifier;
    if (!descriptor.get() || descriptor->sdp() != "local")
        voidNotifier = adoptRef(new VoidRequestNotifier(request, false, RTCPeerConnectionHandler::internalErrorName()));
    else {
        m_localSessionDescription = descriptor;
        signalingNotifier = adoptRef(new SignalingStateNotifier(m_client, signalingStateFromSDP(m_localSessionDescription.get())));
        voidNotifier = adoptRef(new VoidRequestNotifier(request, true));
        m_timerEvents.append(adoptRef(new TimerEvent(this, signalingNotifier)));
    }

    m_timerEvents.append(adoptRef(new TimerEvent(this, voidNotifier)));
}

void RTCPeerConnectionHandlerMock::setRemoteDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> descriptor)
{
    RefPtr<VoidRequestNotifier> voidNotifier;
    RefPtr<SignalingStateNotifier> signalingNotifier;
    if (!descriptor.get() || descriptor->sdp() != "remote")
        voidNotifier = adoptRef(new VoidRequestNotifier(request, false, RTCPeerConnectionHandler::internalErrorName()));
    else {
        m_remoteSessionDescription = descriptor;
        signalingNotifier = adoptRef(new SignalingStateNotifier(m_client, signalingStateFromSDP(m_remoteSessionDescription.get())));
        voidNotifier = adoptRef(new VoidRequestNotifier(request, true));
        m_timerEvents.append(adoptRef(new TimerEvent(this, signalingNotifier)));
    }

    m_timerEvents.append(adoptRef(new TimerEvent(this, voidNotifier)));
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerMock::localDescription()
{
    return m_localSessionDescription;
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerMock::remoteDescription()
{
    return m_remoteSessionDescription;
}

bool RTCPeerConnectionHandlerMock::updateIce(PassRefPtr<RTCConfigurationPrivate>)
{
    return true;
}

bool RTCPeerConnectionHandlerMock::addIceCandidate(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCIceCandidateDescriptor>)
{
    RefPtr<VoidRequestNotifier> notifier = adoptRef(new VoidRequestNotifier(request, true));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    return true;
}

bool RTCPeerConnectionHandlerMock::addStream(PassRefPtr<MediaStreamPrivate>)
{
    // Spec states that every time a stream is added, a negotiationneeded event must be fired.
    m_client->negotiationNeeded();
    return true;
}

void RTCPeerConnectionHandlerMock::removeStream(PassRefPtr<MediaStreamPrivate>)
{
    // Spec states that every time a stream is removed, a negotiationneeded event must be fired.
    m_client->negotiationNeeded();
}

void RTCPeerConnectionHandlerMock::getStats(PassRefPtr<RTCStatsRequest>)
{
    // Current tests does not support getUserMedia and this function requires it.
    // Once WebKit has test support to getUserMedia, this must be fixed.
    notImplemented();
}

std::unique_ptr<RTCDataChannelHandler> RTCPeerConnectionHandlerMock::createDataChannel(const String& label, const RTCDataChannelInit& init)
{
    RefPtr<RemoteDataChannelNotifier> notifier = adoptRef(new RemoteDataChannelNotifier(m_client));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    return std::make_unique<RTCDataChannelHandlerMock>(label, init);
}

std::unique_ptr<RTCDTMFSenderHandler> RTCPeerConnectionHandlerMock::createDTMFSender(PassRefPtr<MediaStreamSource>)
{
    // Requires a mock implementation of RTCDTMFSenderHandler.
    // This must be implemented once the mock implementation of RTCDataChannelHandler is ready.
    notImplemented();
    return nullptr;
}

void RTCPeerConnectionHandlerMock::stop()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
