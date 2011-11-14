/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "PeerConnectionHandlerInternal.h"

#include "PeerConnectionHandlerClient.h"
#include "SecurityOrigin.h"
#include "WebKit.h"
#include "WebKitPlatformSupport.h"
#include "WebMediaStreamDescriptor.h"
#include "WebPeerConnectionHandler.h"
#include "WebPeerConnectionHandlerClient.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

PeerConnectionHandlerInternal::PeerConnectionHandlerInternal(PeerConnectionHandlerClient* client, const String& serverConfiguration, PassRefPtr<SecurityOrigin> securityOrigin)
    : m_client(client)
{
    ASSERT(m_client);
    m_webHandler = adoptPtr(WebKit::webKitPlatformSupport()->createPeerConnectionHandler(this));
    // FIXME: When there is some error reporting avaliable in the PeerConnection object report
    // if we didn't get a WebPeerConnectionHandler instance.
    if (m_webHandler)
        m_webHandler->initialize(serverConfiguration, securityOrigin);
}

PeerConnectionHandlerInternal::~PeerConnectionHandlerInternal()
{
}

void PeerConnectionHandlerInternal::produceInitialOffer(const MediaStreamDescriptorVector& pendingAddStreams)
{
    if (m_webHandler)
        m_webHandler->produceInitialOffer(pendingAddStreams);
}

void PeerConnectionHandlerInternal::handleInitialOffer(const String& sdp)
{
    if (m_webHandler)
        m_webHandler->handleInitialOffer(sdp);
}

void PeerConnectionHandlerInternal::processSDP(const String& sdp)
{
    if (m_webHandler)
        m_webHandler->processSDP(sdp);
}

void PeerConnectionHandlerInternal::processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams)
{
    if (m_webHandler)
        m_webHandler->processPendingStreams(pendingAddStreams, pendingRemoveStreams);
}

void PeerConnectionHandlerInternal::sendDataStreamMessage(const char* data, size_t length)
{
    if (m_webHandler)
        m_webHandler->sendDataStreamMessage(data, length);
}

void PeerConnectionHandlerInternal::stop()
{
    if (m_webHandler)
        m_webHandler->stop();
}

void PeerConnectionHandlerInternal::didCompleteICEProcessing()
{
    if (m_webHandler)
        m_client->didCompleteICEProcessing();
}

void PeerConnectionHandlerInternal::didGenerateSDP(const WebKit::WebString& sdp)
{
    if (m_webHandler)
        m_client->didGenerateSDP(sdp);
}

void PeerConnectionHandlerInternal::didReceiveDataStreamMessage(const char* data, size_t length)
{
    if (m_webHandler)
        m_client->didReceiveDataStreamMessage(data, length);
}

void PeerConnectionHandlerInternal::didAddRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    if (m_webHandler)
        m_client->didAddRemoteStream(webMediaStreamDescriptor);
}

void PeerConnectionHandlerInternal::didRemoveRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    if (m_webHandler)
        m_client->didRemoveRemoteStream(webMediaStreamDescriptor);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
