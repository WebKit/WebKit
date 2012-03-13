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

#include "DeprecatedPeerConnectionHandler.h"

#include "DeprecatedPeerConnectionHandlerClient.h"
#include "DeprecatedPeerConnectionHandlerInternal.h"

namespace WebCore {

PassOwnPtr<DeprecatedPeerConnectionHandler> DeprecatedPeerConnectionHandler::create(DeprecatedPeerConnectionHandlerClient* client, const String& serverConfiguration, const String& username)
{
    return adoptPtr(new DeprecatedPeerConnectionHandler(client, serverConfiguration, username));
}

DeprecatedPeerConnectionHandler::DeprecatedPeerConnectionHandler(DeprecatedPeerConnectionHandlerClient* client, const String& serverConfiguration, const String& username)
    : m_private(adoptPtr(new DeprecatedPeerConnectionHandlerInternal(client, serverConfiguration, username)))
{
}

DeprecatedPeerConnectionHandler::~DeprecatedPeerConnectionHandler()
{
}

void DeprecatedPeerConnectionHandler::produceInitialOffer(const MediaStreamDescriptorVector& pendingAddStreams)
{
    m_private->produceInitialOffer(pendingAddStreams);
}

void DeprecatedPeerConnectionHandler::handleInitialOffer(const String& sdp)
{
    m_private->handleInitialOffer(sdp);
}

void DeprecatedPeerConnectionHandler::processSDP(const String& sdp)
{
    m_private->processSDP(sdp);
}

void DeprecatedPeerConnectionHandler::processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams)
{
    m_private->processPendingStreams(pendingAddStreams, pendingRemoveStreams);
}

void DeprecatedPeerConnectionHandler::sendDataStreamMessage(const char* data, size_t length)
{
    m_private->sendDataStreamMessage(data, length);
}

void DeprecatedPeerConnectionHandler::stop()
{
    m_private->stop();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
