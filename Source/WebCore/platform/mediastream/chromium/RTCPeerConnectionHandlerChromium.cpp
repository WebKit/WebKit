/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "RTCPeerConnectionHandlerChromium.h"

#include "MediaConstraints.h"
#include "RTCConfiguration.h"
#include "RTCIceCandidateDescriptor.h"
#include "RTCPeerConnectionHandlerClient.h"
#include "RTCSessionDescriptionDescriptor.h"
#include "RTCSessionDescriptionRequest.h"
#include "RTCVoidRequest.h"
#include <public/Platform.h>
#include <public/WebMediaConstraints.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebRTCConfiguration.h>
#include <public/WebRTCICECandidate.h>
#include <public/WebRTCSessionDescription.h>
#include <public/WebRTCSessionDescriptionRequest.h>
#include <public/WebRTCVoidRequest.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

PassOwnPtr<RTCPeerConnectionHandler> RTCPeerConnectionHandler::create(RTCPeerConnectionHandlerClient* client)
{
    return adoptPtr(new RTCPeerConnectionHandlerChromium(client));
}

RTCPeerConnectionHandlerChromium::RTCPeerConnectionHandlerChromium(RTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
    ASSERT(m_client);
}

RTCPeerConnectionHandlerChromium::~RTCPeerConnectionHandlerChromium()
{
}

bool RTCPeerConnectionHandlerChromium::initialize(PassRefPtr<RTCConfiguration> configuration, PassRefPtr<MediaConstraints> constraints)
{
    m_webHandler = adoptPtr(WebKit::Platform::current()->createRTCPeerConnectionHandler(this));
    return m_webHandler ? m_webHandler->initialize(configuration, constraints) : false;
}

void RTCPeerConnectionHandlerChromium::createOffer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<MediaConstraints> constraints)
{
    // FIXME: Should the error callback be triggered here?
    if (!m_webHandler)
        return;

    m_webHandler->createOffer(request, constraints);
}

void RTCPeerConnectionHandlerChromium::createAnswer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<MediaConstraints> constraints)
{
    // FIXME: Should the error callback be triggered here?
    if (!m_webHandler)
        return;

    m_webHandler->createAnswer(request, constraints);
}

void RTCPeerConnectionHandlerChromium::setLocalDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> sessionDescription)
{
    if (!m_webHandler)
        return;

    m_webHandler->setLocalDescription(request, sessionDescription);
}

void RTCPeerConnectionHandlerChromium::setRemoteDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> sessionDescription)
{
    if (!m_webHandler)
        return;

    m_webHandler->setRemoteDescription(request, sessionDescription);
}

bool RTCPeerConnectionHandlerChromium::updateIce(PassRefPtr<RTCConfiguration> configuration, PassRefPtr<MediaConstraints> constraints)
{
    if (!m_webHandler)
        return false;

    return m_webHandler->updateICE(configuration, constraints);
}

bool RTCPeerConnectionHandlerChromium::addIceCandidate(PassRefPtr<RTCIceCandidateDescriptor> iceCandidate)
{
    if (!m_webHandler)
        return false;

    return m_webHandler->addICECandidate(iceCandidate);
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerChromium::localDescription()
{
    if (!m_webHandler)
        return 0;

    return m_webHandler->localDescription();
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerChromium::remoteDescription()
{
    if (!m_webHandler)
        return 0;

    return m_webHandler->remoteDescription();
}

bool RTCPeerConnectionHandlerChromium::addStream(PassRefPtr<MediaStreamDescriptor> mediaStream, PassRefPtr<MediaConstraints> constraints)
{
    if (!m_webHandler)
        return false;

    return m_webHandler->addStream(mediaStream, constraints);
}

void RTCPeerConnectionHandlerChromium::removeStream(PassRefPtr<MediaStreamDescriptor> mediaStream)
{
    if (!m_webHandler)
        return;

    m_webHandler->removeStream(mediaStream);
}

void RTCPeerConnectionHandlerChromium::stop()
{
    if (!m_webHandler)
        return;

    m_webHandler->stop();
}

void RTCPeerConnectionHandlerChromium::negotiationNeeded()
{
    m_client->negotiationNeeded();
}

void RTCPeerConnectionHandlerChromium::didGenerateICECandidate(const WebKit::WebRTCICECandidate& iceCandidate)
{
    m_client->didGenerateIceCandidate(iceCandidate);
}

void RTCPeerConnectionHandlerChromium::didChangeReadyState(WebKit::WebRTCPeerConnectionHandlerClient::ReadyState state)
{
    m_client->didChangeReadyState(static_cast<RTCPeerConnectionHandlerClient::ReadyState>(state));
}

void RTCPeerConnectionHandlerChromium::didChangeICEState(WebKit::WebRTCPeerConnectionHandlerClient::ICEState state)
{
    m_client->didChangeIceState(static_cast<RTCPeerConnectionHandlerClient::IceState>(state));
}

void RTCPeerConnectionHandlerChromium::didAddRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    m_client->didAddRemoteStream(webMediaStreamDescriptor);
}

void RTCPeerConnectionHandlerChromium::didRemoveRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    m_client->didRemoveRemoteStream(webMediaStreamDescriptor);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
