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

#ifndef WebRTCPeerConnectionHandler_h
#define WebRTCPeerConnectionHandler_h

namespace WebKit {
class WebMediaConstraints;
class WebMediaStreamDescriptor;
class WebRTCConfiguration;
class WebRTCDataChannelHandler;
class WebRTCICECandidate;
class WebRTCPeerConnectionHandlerClient;
class WebRTCSessionDescription;
class WebRTCSessionDescriptionRequest;
class WebRTCStatsRequest;
class WebRTCVoidRequest;
class WebString;

class WebRTCPeerConnectionHandler {
public:
    virtual ~WebRTCPeerConnectionHandler() { }

    virtual bool initialize(const WebRTCConfiguration&, const WebMediaConstraints&) = 0;

    virtual void createOffer(const WebRTCSessionDescriptionRequest&, const WebMediaConstraints&) = 0;
    virtual void createAnswer(const WebRTCSessionDescriptionRequest&, const WebMediaConstraints&) = 0;
    virtual void setLocalDescription(const WebRTCVoidRequest&, const WebRTCSessionDescription&) = 0;
    virtual void setRemoteDescription(const WebRTCVoidRequest&, const WebRTCSessionDescription&) = 0;
    virtual WebRTCSessionDescription localDescription() = 0;
    virtual WebRTCSessionDescription remoteDescription() = 0;
    virtual bool updateICE(const WebRTCConfiguration&, const WebMediaConstraints&) = 0;
    virtual bool addICECandidate(const WebRTCICECandidate&) = 0;
    virtual bool addStream(const WebMediaStreamDescriptor&, const WebMediaConstraints&) = 0;
    virtual void removeStream(const WebMediaStreamDescriptor&) = 0;
    // FIXME: Remove default implementation when clients have changed.
    virtual void getStats(const WebRTCStatsRequest&) { }
    virtual WebRTCDataChannelHandler* createDataChannel(const WebString& label, bool reliable) { return 0; }
    virtual void stop() = 0;
};

} // namespace WebKit

#endif // WebRTCPeerConnectionHandler_h
