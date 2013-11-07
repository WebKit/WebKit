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

#include "NotImplemented.h"
#include "RTCDTMFSenderHandler.h"
#include "RTCDataChannelHandler.h"
#include "RTCDataChannelHandlerMock.h"
#include "RTCNotifiersMock.h"
#include "RTCSessionDescriptionRequest.h"
#include "RTCVoidRequest.h"
#include <wtf/text/CString.h>

namespace WebCore {

PassOwnPtr<RTCPeerConnectionHandler> RTCPeerConnectionHandlerMock::create(RTCPeerConnectionHandlerClient* client)
{
    return adoptPtr(new RTCPeerConnectionHandlerMock(client));
}

RTCPeerConnectionHandlerMock::RTCPeerConnectionHandlerMock(RTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
}

bool RTCPeerConnectionHandlerMock::initialize(PassRefPtr<RTCConfiguration>, PassRefPtr<MediaConstraints>)
{
    // FIXME: once there is support in RTCPeerConnection + constraints, this should be taken into account here
    // and a LayouTest for that must be added.
    // https://bugs.webkit.org/show_bug.cgi?id=123093
    RefPtr<IceConnectionNotifier> notifier = adoptRef(new IceConnectionNotifier(m_client, RTCPeerConnectionHandlerClient::IceConnectionStateCompleted, RTCPeerConnectionHandlerClient::IceGatheringStateComplete));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    return true;
}

void RTCPeerConnectionHandlerMock::createOffer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<MediaConstraints> constraints)
{
    String succeedValue;
    RefPtr<SessionRequestNotifier> notifier;
    if (constraints->getMandatoryConstraintValue("succeed", succeedValue) && succeedValue == "true")
        notifier = adoptRef(new SessionRequestNotifier(request, RTCSessionDescriptionDescriptor::create("offer", "local")));
    else
        notifier = adoptRef(new SessionRequestNotifier(request, 0));

    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

void RTCPeerConnectionHandlerMock::createAnswer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<MediaConstraints> constraints)
{
    RefPtr<SessionRequestNotifier> notifier;
    // We can only create an answer if we have already had an offer and the remote session description is stored.
    String succeedValue;
    if (!m_remoteSessionDescription.get() || (constraints->getMandatoryConstraintValue("succeed", succeedValue) && succeedValue == "false"))
        notifier = adoptRef(new SessionRequestNotifier(request, 0));
    else
        notifier = adoptRef(new SessionRequestNotifier(request, RTCSessionDescriptionDescriptor::create("answer", "local")));

    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

void RTCPeerConnectionHandlerMock::setLocalDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> descriptor)
{
    RefPtr<VoidRequestNotifier> notifier;
    if (!descriptor.get() || descriptor->sdp() != "local")
        notifier = adoptRef(new VoidRequestNotifier(request, false));
    else {
        m_localSessionDescription = descriptor;
        notifier = adoptRef(new VoidRequestNotifier(request, true));
    }

    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

void RTCPeerConnectionHandlerMock::setRemoteDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> descriptor)
{
    RefPtr<VoidRequestNotifier> notifier;
    if (!descriptor.get() || descriptor->sdp() != "remote")
        notifier = adoptRef(new VoidRequestNotifier(request, false));
    else {
        m_remoteSessionDescription = descriptor;
        notifier = adoptRef(new VoidRequestNotifier(request, true));
    }

    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerMock::localDescription()
{
    return m_localSessionDescription;
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerMock::remoteDescription()
{
    return m_remoteSessionDescription;
}

bool RTCPeerConnectionHandlerMock::updateIce(PassRefPtr<RTCConfiguration>, PassRefPtr<MediaConstraints>)
{
    return true;
}

bool RTCPeerConnectionHandlerMock::addIceCandidate(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCIceCandidateDescriptor>)
{
    RefPtr<VoidRequestNotifier> notifier = adoptRef(new VoidRequestNotifier(request, true));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    return true;
}

bool RTCPeerConnectionHandlerMock::addStream(PassRefPtr<MediaStreamPrivate>, PassRefPtr<MediaConstraints>)
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

PassOwnPtr<RTCDataChannelHandler> RTCPeerConnectionHandlerMock::createDataChannel(const String& label, const RTCDataChannelInit& init)
{
    RefPtr<RemoteDataChannelNotifier> notifier = adoptRef(new RemoteDataChannelNotifier(m_client));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    return adoptPtr(new RTCDataChannelHandlerMock(label, init));
}

PassOwnPtr<RTCDTMFSenderHandler> RTCPeerConnectionHandlerMock::createDTMFSender(PassRefPtr<MediaStreamSource>)
{
    // Requires a mock implementation of RTCDTMFSenderHandler.
    // This must be implemented once the mock implementation of RTCDataChannelHandler is ready.
    notImplemented();
    return 0;
}

void RTCPeerConnectionHandlerMock::stop()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
