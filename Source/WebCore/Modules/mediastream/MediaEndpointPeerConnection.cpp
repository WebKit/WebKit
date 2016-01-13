/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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
#include "MediaEndpointPeerConnection.h"

#include "DOMError.h"
#include "JSDOMError.h"

namespace WebCore {

using namespace PeerConnection;

static std::unique_ptr<PeerConnectionBackend> createMediaEndpointPeerConnection(PeerConnectionBackendClient* client)
{
    return std::unique_ptr<PeerConnectionBackend>(new MediaEndpointPeerConnection(client));
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createMediaEndpointPeerConnection;

MediaEndpointPeerConnection::MediaEndpointPeerConnection(PeerConnectionBackendClient* client)
{
    UNUSED_PARAM(client);
}

void MediaEndpointPeerConnection::createOffer(RTCOfferOptions& options, SessionDescriptionPromise&& promise)
{
    UNUSED_PARAM(options);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::createAnswer(RTCAnswerOptions& options, SessionDescriptionPromise&& promise)
{
    UNUSED_PARAM(options);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::setLocalDescription(RTCSessionDescription& description, VoidPromise&& promise)
{
    UNUSED_PARAM(description);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::localDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentLocalDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingLocalDescription() const
{
    notImplemented();

    return nullptr;
}

void MediaEndpointPeerConnection::setRemoteDescription(RTCSessionDescription& description, VoidPromise&& promise)
{
    UNUSED_PARAM(description);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::remoteDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentRemoteDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingRemoteDescription() const
{
    notImplemented();

    return nullptr;
}

void MediaEndpointPeerConnection::setConfiguration(RTCConfiguration& configuration)
{
    UNUSED_PARAM(configuration);

    notImplemented();
}

void MediaEndpointPeerConnection::addIceCandidate(RTCIceCandidate& rtcCandidate, PeerConnection::VoidPromise&& promise)
{
    UNUSED_PARAM(rtcCandidate);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::getStats(MediaStreamTrack*, PeerConnection::StatsPromise&& promise)
{
    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::replaceTrack(RTCRtpSender& sender, MediaStreamTrack& withTrack, PeerConnection::VoidPromise&& promise)
{
    UNUSED_PARAM(sender);
    UNUSED_PARAM(withTrack);
    UNUSED_PARAM(promise);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::stop()
{
    notImplemented();
}

void MediaEndpointPeerConnection::markAsNeedingNegotiation()
{
    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
