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

#ifndef WebPeerConnection00HandlerClient_h
#define WebPeerConnection00HandlerClient_h

namespace WebKit {
class WebICECandidateDescriptor;
class WebMediaStreamDescriptor;
class WebString;

class WebPeerConnection00HandlerClient {
public:
    enum ReadyState {
        ReadyStateNew = 0,
        ReadyStateOpening = 1,
        ReadyStateActive = 2,
        ReadyStateClosed = 3,

        // DEPRECATED
        ReadyStateNegotiating = 1,
    };

    enum ICEState {
        ICEStateGathering = 0x100,
        ICEStateWaiting = 0x200,
        ICEStateChecking = 0x300,
        ICEStateConnected = 0x400,
        ICEStateCompleted = 0x500,
        ICEStateFailed = 0x600,
        ICEStateClosed = 0x700
    };

    virtual ~WebPeerConnection00HandlerClient() { }

    virtual void didGenerateICECandidate(const WebICECandidateDescriptor&, bool moreToFollow) = 0;
    virtual void didChangeReadyState(ReadyState) = 0;
    virtual void didChangeICEState(ICEState) = 0;
    virtual void didAddRemoteStream(const WebMediaStreamDescriptor&) = 0;
    virtual void didRemoveRemoteStream(const WebMediaStreamDescriptor&) = 0;
};

} // namespace WebKit

#endif // WebPeerConnection00HandlerClient_h
