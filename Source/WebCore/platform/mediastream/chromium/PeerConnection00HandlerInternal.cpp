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

#include "PeerConnection00HandlerInternal.h"

#include "IceCandidateDescriptor.h"
#include "IceOptions.h"
#include "MediaHints.h"
#include "PeerConnection00.h"
#include "PeerConnection00HandlerClient.h"
#include "SessionDescriptionDescriptor.h"
#include <public/Platform.h>
#include <public/WebICECandidateDescriptor.h>
#include <public/WebICEOptions.h>
#include <public/WebMediaHints.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebPeerConnection00Handler.h>
#include <public/WebPeerConnection00HandlerClient.h>
#include <public/WebSessionDescriptionDescriptor.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

PeerConnection00HandlerInternal::PeerConnection00HandlerInternal(PeerConnection00HandlerClient* client, const String& serverConfiguration, const String& username)
    : m_client(client)
{
    ASSERT(m_client);
    m_webHandler = adoptPtr(WebKit::Platform::current()->createPeerConnection00Handler(this));
    // FIXME: When there is some error reporting avaliable in the PeerConnection object report
    // if we didn't get a WebPeerConnection00Handler instance.

    if (m_webHandler)
        m_webHandler->initialize(serverConfiguration, username);
}

PeerConnection00HandlerInternal::~PeerConnection00HandlerInternal()
{
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00HandlerInternal::createOffer(PassRefPtr<MediaHints> mediaHints)
{
    if (!m_webHandler)
        return 0;

    return m_webHandler->createOffer(mediaHints);
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00HandlerInternal::createAnswer(const String& offer, PassRefPtr<MediaHints> mediaHints)
{
    if (!m_webHandler)
        return 0;

    return m_webHandler->createAnswer(offer, mediaHints);
}

bool PeerConnection00HandlerInternal::setLocalDescription(int action, PassRefPtr<SessionDescriptionDescriptor> sessionDescription)
{
    if (!m_webHandler)
        return false;

    return m_webHandler->setLocalDescription(static_cast<WebKit::WebPeerConnection00Handler::Action>(action), sessionDescription);
}

bool PeerConnection00HandlerInternal::setRemoteDescription(int action, PassRefPtr<SessionDescriptionDescriptor> sessionDescription)
{
    if (!m_webHandler)
        return false;

    return m_webHandler->setRemoteDescription(static_cast<WebKit::WebPeerConnection00Handler::Action>(action), sessionDescription);
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00HandlerInternal::localDescription()
{
    if (!m_webHandler)
        return 0;

    return m_webHandler->localDescription();
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00HandlerInternal::remoteDescription()
{
    if (!m_webHandler)
        return 0;

    return m_webHandler->remoteDescription();
}

bool PeerConnection00HandlerInternal::startIce(PassRefPtr<IceOptions> iceOptions)
{
    if (!m_webHandler)
        return true;

    return m_webHandler->startIce(iceOptions);
}

bool PeerConnection00HandlerInternal::processIceMessage(PassRefPtr<IceCandidateDescriptor> iceCandidate)
{
    if (!m_webHandler)
        return false;

    return m_webHandler->processIceMessage(iceCandidate);
}

void PeerConnection00HandlerInternal::addStream(PassRefPtr<MediaStreamDescriptor> mediaStream)
{
    if (!m_webHandler)
        return;

    m_webHandler->addStream(mediaStream);
}

void PeerConnection00HandlerInternal::removeStream(PassRefPtr<MediaStreamDescriptor> mediaStream)
{
    if (!m_webHandler)
        return;

    m_webHandler->removeStream(mediaStream);
}

void PeerConnection00HandlerInternal::stop()
{
    if (!m_webHandler)
        return;

    m_webHandler->stop();
}

void PeerConnection00HandlerInternal::didGenerateICECandidate(const WebKit::WebICECandidateDescriptor& iceCandidate, bool moreToFollow)
{
    m_client->didGenerateIceCandidate(iceCandidate, moreToFollow);
}

void PeerConnection00HandlerInternal::didChangeReadyState(ReadyState state)
{
    m_client->didChangeReadyState(state);
}

void PeerConnection00HandlerInternal::didChangeICEState(ICEState state)
{
    m_client->didChangeIceState(state);
}

void PeerConnection00HandlerInternal::didAddRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    m_client->didAddRemoteStream(webMediaStreamDescriptor);
}

void PeerConnection00HandlerInternal::didRemoveRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    m_client->didRemoveRemoteStream(webMediaStreamDescriptor);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
