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

#ifndef PeerConnection00HandlerInternal_h
#define PeerConnection00HandlerInternal_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamDescriptor.h"
#include <public/WebPeerConnection00HandlerClient.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebICECandidateDescriptor;
class WebPeerConnection00Handler;
class WebMediaStreamDescriptor;
class WebString;
}

namespace WebCore {

class IceCandidateDescriptor;
class IceOptions;
class MediaHints;
class PeerConnection00HandlerClient;
class SessionDescriptionDescriptor;

class PeerConnection00HandlerInternal : public WebKit::WebPeerConnection00HandlerClient {
public:
    PeerConnection00HandlerInternal(PeerConnection00HandlerClient*, const String& serverConfiguration, const String& username);
    ~PeerConnection00HandlerInternal();

    PassRefPtr<SessionDescriptionDescriptor> createOffer(PassRefPtr<MediaHints>);
    PassRefPtr<SessionDescriptionDescriptor> createAnswer(const String& offer, PassRefPtr<MediaHints>);
    bool setLocalDescription(int action, PassRefPtr<SessionDescriptionDescriptor>);
    bool setRemoteDescription(int action, PassRefPtr<SessionDescriptionDescriptor>);
    PassRefPtr<SessionDescriptionDescriptor> localDescription();
    PassRefPtr<SessionDescriptionDescriptor> remoteDescription();
    bool startIce(PassRefPtr<IceOptions>);
    bool processIceMessage(PassRefPtr<IceCandidateDescriptor>);
    void addStream(PassRefPtr<MediaStreamDescriptor>);
    void removeStream(PassRefPtr<MediaStreamDescriptor>);
    void stop();

    // WebKit::WebJSEPPeerConnectionHandlerClient implementation.
    virtual void didGenerateICECandidate(const WebKit::WebICECandidateDescriptor&, bool moreToFollow);
    virtual void didChangeReadyState(ReadyState);
    virtual void didChangeICEState(ICEState);
    virtual void didAddRemoteStream(const WebKit::WebMediaStreamDescriptor&);
    virtual void didRemoveRemoteStream(const WebKit::WebMediaStreamDescriptor&);

private:
    OwnPtr<WebKit::WebPeerConnection00Handler> m_webHandler;
    PeerConnection00HandlerClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // PeerConnection00HandlerInternal_h
