/*
 * Copyright (C) 2012 Google AB. All rights reserved.
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#include "PeerConnection00Handler.h"

#include "IceCandidateDescriptor.h"
#include "IceOptions.h"
#include "MediaHints.h"
#include "MediaStreamDescriptor.h"
#include "PeerConnection00HandlerClient.h"
#include "SessionDescriptionDescriptor.h"

namespace WebCore {

// Dummy implementations below for ports that build with MEDIA_STREAM enabled by default.

PassOwnPtr<PeerConnection00Handler> PeerConnection00Handler::create(PeerConnection00HandlerClient* client, const String& serverConfiguration, const String& username)
{
    return adoptPtr(new PeerConnection00Handler(client, serverConfiguration, username));
}

PeerConnection00Handler::PeerConnection00Handler(PeerConnection00HandlerClient* client, const String&, const String&)
    : m_client(client)
{
}

PeerConnection00Handler::~PeerConnection00Handler()
{
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00Handler::createOffer(PassRefPtr<MediaHints>)
{
    return 0;
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00Handler::createAnswer(const String&, PassRefPtr<MediaHints>)
{
    return 0;
}

bool PeerConnection00Handler::setLocalDescription(int, PassRefPtr<SessionDescriptionDescriptor>)
{
    return false;
}

bool PeerConnection00Handler::setRemoteDescription(int, PassRefPtr<SessionDescriptionDescriptor>)
{
    return false;
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00Handler::localDescription()
{
    return 0;
}

PassRefPtr<SessionDescriptionDescriptor> PeerConnection00Handler::remoteDescription()
{
    return 0;
}

bool PeerConnection00Handler::startIce(PassRefPtr<IceOptions>)
{
    return false;
}

bool PeerConnection00Handler::processIceMessage(PassRefPtr<IceCandidateDescriptor>)
{
    return false;
}

void PeerConnection00Handler::addStream(PassRefPtr<MediaStreamDescriptor>)
{
}

void PeerConnection00Handler::removeStream(PassRefPtr<MediaStreamDescriptor>)
{
}

void PeerConnection00Handler::stop()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
