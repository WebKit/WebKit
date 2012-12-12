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
#include "RTCDataChannelHandlerChromium.h"
#include "RTCDataChannelHandlerClient.h"
#include "RTCIceCandidateDescriptor.h"
#include "RTCPeerConnectionHandlerClient.h"
#include "RTCSessionDescriptionDescriptor.h"
#include "RTCSessionDescriptionRequest.h"
#include "RTCStatsRequest.h"
#include "RTCVoidRequest.h"
#include <public/Platform.h>
#include <public/WebMediaConstraints.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebRTCConfiguration.h>
#include <public/WebRTCDataChannelHandler.h>
#include <public/WebRTCICECandidate.h>
#include <public/WebRTCSessionDescription.h>
#include <public/WebRTCSessionDescriptionRequest.h>
#include <public/WebRTCStatsRequest.h>
#include <public/WebRTCVoidRequest.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

WebKit::WebRTCPeerConnectionHandler* RTCPeerConnectionHandlerChromium::toWebRTCPeerConnectionHandler(RTCPeerConnectionHandler* handler)
{
    return static_cast<RTCPeerConnectionHandlerChromium*>(handler)->m_webHandler.get();
}

PassOwnPtr<RTCPeerConnectionHandler> RTCPeerConnectionHandler::create(RTCPeerConnectionHandlerClient* client)
{
    ASSERT(client);
    OwnPtr<RTCPeerConnectionHandlerChromium> handler = adoptPtr(new RTCPeerConnectionHandlerChromium(client));

    if (!handler->createWebHandler())
        return nullptr;

    return handler.release();
}

RTCPeerConnectionHandlerChromium::RTCPeerConnectionHandlerChromium(RTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
}

RTCPeerConnectionHandlerChromium::~RTCPeerConnectionHandlerChromium()
{
}

bool RTCPeerConnectionHandlerChromium::createWebHandler()
{
    m_webHandler = adoptPtr(WebKit::Platform::current()->createRTCPeerConnectionHandler(this));
    return m_webHandler;
}

bool RTCPeerConnectionHandlerChromium::initialize(PassRefPtr<RTCConfiguration> configuration, PassRefPtr<MediaConstraints> constraints)
{
    return m_webHandler->initialize(configuration, constraints);
}

void RTCPeerConnectionHandlerChromium::createOffer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<MediaConstraints> constraints)
{
    m_webHandler->createOffer(request, constraints);
}

void RTCPeerConnectionHandlerChromium::createAnswer(PassRefPtr<RTCSessionDescriptionRequest> request, PassRefPtr<MediaConstraints> constraints)
{
    m_webHandler->createAnswer(request, constraints);
}

void RTCPeerConnectionHandlerChromium::setLocalDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> sessionDescription)
{
    m_webHandler->setLocalDescription(request, sessionDescription);
}

void RTCPeerConnectionHandlerChromium::setRemoteDescription(PassRefPtr<RTCVoidRequest> request, PassRefPtr<RTCSessionDescriptionDescriptor> sessionDescription)
{
    m_webHandler->setRemoteDescription(request, sessionDescription);
}

bool RTCPeerConnectionHandlerChromium::updateIce(PassRefPtr<RTCConfiguration> configuration, PassRefPtr<MediaConstraints> constraints)
{
    return m_webHandler->updateICE(configuration, constraints);
}

bool RTCPeerConnectionHandlerChromium::addIceCandidate(PassRefPtr<RTCIceCandidateDescriptor> iceCandidate)
{
    return m_webHandler->addICECandidate(iceCandidate);
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerChromium::localDescription()
{
    return m_webHandler->localDescription();
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerChromium::remoteDescription()
{
    return m_webHandler->remoteDescription();
}

bool RTCPeerConnectionHandlerChromium::addStream(PassRefPtr<MediaStreamDescriptor> mediaStream, PassRefPtr<MediaConstraints> constraints)
{
    return m_webHandler->addStream(mediaStream, constraints);
}

void RTCPeerConnectionHandlerChromium::removeStream(PassRefPtr<MediaStreamDescriptor> mediaStream)
{
    m_webHandler->removeStream(mediaStream);
}

void RTCPeerConnectionHandlerChromium::getStats(PassRefPtr<RTCStatsRequest> request)
{
    m_webHandler->getStats(request);
}

PassOwnPtr<RTCDataChannelHandler> RTCPeerConnectionHandlerChromium::createDataChannel(const String& label, bool reliable)
{
    WebKit::WebRTCDataChannelHandler* webHandler = m_webHandler->createDataChannel(label, reliable);
    if (!webHandler)
        return nullptr;

    return RTCDataChannelHandlerChromium::create(webHandler);
}

void RTCPeerConnectionHandlerChromium::stop()
{
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

void RTCPeerConnectionHandlerChromium::didChangeICEGatheringState(WebKit::WebRTCPeerConnectionHandlerClient::ICEGatheringState state)
{
    m_client->didChangeIceGatheringState(static_cast<RTCPeerConnectionHandlerClient::IceGatheringState>(state));
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

void RTCPeerConnectionHandlerChromium::didAddRemoteDataChannel(WebKit::WebRTCDataChannelHandler* webHandler)
{
    ASSERT(webHandler);
    m_client->didAddRemoteDataChannel(RTCDataChannelHandlerChromium::create(webHandler));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
