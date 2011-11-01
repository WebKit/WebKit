/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "PeerConnectionHandler.h"

#include "PeerConnectionHandlerClient.h"
#include "SecurityOrigin.h"

namespace WebCore {

PassOwnPtr<PeerConnectionHandler> PeerConnectionHandler::create(PeerConnectionHandlerClient* client, const String& serverConfiguration, PassRefPtr<SecurityOrigin> securityOrigin)
{
    return adoptPtr(new PeerConnectionHandler(client, serverConfiguration, securityOrigin));
}

// FIXME: remove when real implementations are available
// Empty implementations for ports that build with MEDIA_STREAM enabled by default.
PeerConnectionHandler::PeerConnectionHandler(PeerConnectionHandlerClient*, const String&, PassRefPtr<SecurityOrigin>)
{
}

PeerConnectionHandler::~PeerConnectionHandler()
{
}

void PeerConnectionHandler::produceInitialOffer(const MediaStreamDescriptorVector&)
{
}

void PeerConnectionHandler::handleInitialOffer(const String&)
{
}

void PeerConnectionHandler::processSDP(const String&)
{
}

void PeerConnectionHandler::processPendingStreams(const MediaStreamDescriptorVector&, const MediaStreamDescriptorVector&)
{
}

void PeerConnectionHandler::sendDataStreamMessage(const char*, size_t)
{
}

void PeerConnectionHandler::stop()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
