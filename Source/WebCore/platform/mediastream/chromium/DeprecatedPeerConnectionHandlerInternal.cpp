/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "DeprecatedPeerConnectionHandlerInternal.h"

#include "DeprecatedPeerConnectionHandlerClient.h"
#include <public/Platform.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebPeerConnectionHandler.h>
#include <public/WebPeerConnectionHandlerClient.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

DeprecatedPeerConnectionHandlerInternal::DeprecatedPeerConnectionHandlerInternal(DeprecatedPeerConnectionHandlerClient* client, const String& serverConfiguration, const String& username)
    : m_client(client)
{
    ASSERT(m_client);
    m_webHandler = adoptPtr(WebKit::Platform::current()->createPeerConnectionHandler(this));
    // FIXME: When there is some error reporting avaliable in the PeerConnection object report
    // if we didn't get a WebPeerConnectionHandler instance.

    if (m_webHandler)
        m_webHandler->initialize(serverConfiguration, username);
}

DeprecatedPeerConnectionHandlerInternal::~DeprecatedPeerConnectionHandlerInternal()
{
}

void DeprecatedPeerConnectionHandlerInternal::produceInitialOffer(const MediaStreamDescriptorVector& pendingAddStreams)
{
    if (m_webHandler)
        m_webHandler->produceInitialOffer(pendingAddStreams);
}

void DeprecatedPeerConnectionHandlerInternal::handleInitialOffer(const String& sdp)
{
    if (m_webHandler)
        m_webHandler->handleInitialOffer(sdp);
}

void DeprecatedPeerConnectionHandlerInternal::processSDP(const String& sdp)
{
    if (m_webHandler)
        m_webHandler->processSDP(sdp);
}

void DeprecatedPeerConnectionHandlerInternal::processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams)
{
    if (m_webHandler)
        m_webHandler->processPendingStreams(pendingAddStreams, pendingRemoveStreams);
}

void DeprecatedPeerConnectionHandlerInternal::sendDataStreamMessage(const char* data, size_t length)
{
    if (m_webHandler)
        m_webHandler->sendDataStreamMessage(data, length);
}

void DeprecatedPeerConnectionHandlerInternal::stop()
{
    if (m_webHandler)
        m_webHandler->stop();
}

void DeprecatedPeerConnectionHandlerInternal::didCompleteICEProcessing()
{
    if (m_webHandler)
        m_client->didCompleteICEProcessing();
}

void DeprecatedPeerConnectionHandlerInternal::didGenerateSDP(const WebKit::WebString& sdp)
{
    if (m_webHandler)
        m_client->didGenerateSDP(sdp);
}

void DeprecatedPeerConnectionHandlerInternal::didReceiveDataStreamMessage(const char* data, size_t length)
{
    if (m_webHandler)
        m_client->didReceiveDataStreamMessage(data, length);
}

void DeprecatedPeerConnectionHandlerInternal::didAddRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    if (m_webHandler)
        m_client->didAddRemoteStream(webMediaStreamDescriptor);
}

void DeprecatedPeerConnectionHandlerInternal::didRemoveRemoteStream(const WebKit::WebMediaStreamDescriptor& webMediaStreamDescriptor)
{
    if (m_webHandler)
        m_client->didRemoveRemoteStream(webMediaStreamDescriptor);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
