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

#include "RTCPeerConnectionHandler.h"

#include "RTCPeerConnectionHandlerClient.h"
#include "RTCSessionDescriptionDescriptor.h"
#include "RTCVoidRequest.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

// Dummy implementations below for ports that build with MEDIA_STREAM enabled by default.

class RTCPeerConnectionHandlerDummy : public RTCPeerConnectionHandler {
public:
    RTCPeerConnectionHandlerDummy(RTCPeerConnectionHandlerClient*);
    virtual ~RTCPeerConnectionHandlerDummy();

    virtual bool initialize(PassRefPtr<RTCConfiguration>, PassRefPtr<MediaConstraints>) OVERRIDE;

    virtual void createOffer(PassRefPtr<RTCSessionDescriptionRequest>, PassRefPtr<MediaConstraints>) OVERRIDE;
    virtual void setLocalDescription(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCSessionDescriptionDescriptor>) OVERRIDE;
    virtual void setRemoteDescription(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCSessionDescriptionDescriptor>) OVERRIDE;
    virtual PassRefPtr<RTCSessionDescriptionDescriptor> localDescription() OVERRIDE;
    virtual PassRefPtr<RTCSessionDescriptionDescriptor> remoteDescription() OVERRIDE;
    virtual bool updateIce(PassRefPtr<RTCConfiguration>, PassRefPtr<MediaConstraints>) OVERRIDE;
    virtual bool addIceCandidate(PassRefPtr<RTCIceCandidateDescriptor>) OVERRIDE;
    virtual bool addStream(PassRefPtr<MediaStreamDescriptor>, PassRefPtr<MediaConstraints>) OVERRIDE;
    virtual void removeStream(PassRefPtr<MediaStreamDescriptor>) OVERRIDE;
    virtual void stop() OVERRIDE;

private:
    RTCPeerConnectionHandlerClient* m_client;
};

PassOwnPtr<RTCPeerConnectionHandler> RTCPeerConnectionHandler::create(RTCPeerConnectionHandlerClient* client)
{
    return adoptPtr(new RTCPeerConnectionHandlerDummy(client));
}

RTCPeerConnectionHandler::RTCPeerConnectionHandler()
{
}

RTCPeerConnectionHandler::~RTCPeerConnectionHandler()
{
}

RTCPeerConnectionHandlerDummy::RTCPeerConnectionHandlerDummy(RTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
    ASSERT(m_client);
}

RTCPeerConnectionHandlerDummy::~RTCPeerConnectionHandlerDummy()
{
}

bool RTCPeerConnectionHandlerDummy::initialize(PassRefPtr<RTCConfiguration>, PassRefPtr<MediaConstraints>)
{
    return false;
}

void RTCPeerConnectionHandlerDummy::createOffer(PassRefPtr<RTCSessionDescriptionRequest>, PassRefPtr<MediaConstraints>)
{
}

void RTCPeerConnectionHandlerDummy::setLocalDescription(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCSessionDescriptionDescriptor>)
{
}

void RTCPeerConnectionHandlerDummy::setRemoteDescription(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCSessionDescriptionDescriptor>)
{
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerDummy::localDescription()
{
    return 0;
}

PassRefPtr<RTCSessionDescriptionDescriptor> RTCPeerConnectionHandlerDummy::remoteDescription()
{
    return 0;
}

bool RTCPeerConnectionHandlerDummy::addStream(PassRefPtr<MediaStreamDescriptor>, PassRefPtr<MediaConstraints>)
{
    return false;
}

void RTCPeerConnectionHandlerDummy::removeStream(PassRefPtr<MediaStreamDescriptor>)
{
}

bool RTCPeerConnectionHandlerDummy::updateIce(PassRefPtr<RTCConfiguration>, PassRefPtr<MediaConstraints>)
{
    return false;
}

bool RTCPeerConnectionHandlerDummy::addIceCandidate(PassRefPtr<RTCIceCandidateDescriptor>)
{
    return false;
}

void RTCPeerConnectionHandlerDummy::stop()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
