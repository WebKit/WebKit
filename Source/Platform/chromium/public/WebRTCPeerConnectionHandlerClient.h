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

#ifndef WebRTCPeerConnectionHandlerClient_h
#define WebRTCPeerConnectionHandlerClient_h

namespace WebKit {
class WebMediaStream;
class WebRTCDataChannelHandler;
class WebRTCICECandidate;

class WebRTCPeerConnectionHandlerClient {
public:
    enum SignalingState {
        SignalingStateStable = 1,
        SignalingStateHaveLocalOffer = 2,
        SignalingStateHaveRemoteOffer = 3,
        SignalingStateHaveLocalPrAnswer = 4,
        SignalingStateHaveRemotePrAnswer = 5,
        SignalingStateClosed = 6,
    };

    enum ICEConnectionState {
        ICEConnectionStateNew = 1,
        ICEConnectionStateChecking = 2,
        ICEConnectionStateConnected = 3,
        ICEConnectionStateCompleted = 4,
        ICEConnectionStateFailed = 5,
        ICEConnectionStateDisconnected = 6,
        ICEConnectionStateClosed = 7,

        // DEPRECATED
        ICEConnectionStateStarting = 1,
    };

    enum ICEGatheringState {
        ICEGatheringStateNew = 1,
        ICEGatheringStateGathering = 2,
        ICEGatheringStateComplete = 3
    };

    virtual ~WebRTCPeerConnectionHandlerClient() { }

    virtual void negotiationNeeded() = 0;
    virtual void didGenerateICECandidate(const WebRTCICECandidate&) = 0;
    virtual void didChangeSignalingState(SignalingState) = 0;
    virtual void didChangeICEGatheringState(ICEGatheringState) = 0;
    virtual void didChangeICEConnectionState(ICEConnectionState) = 0;
    virtual void didAddRemoteStream(const WebMediaStream&) = 0;
    virtual void didRemoveRemoteStream(const WebMediaStream&) = 0;
    virtual void didAddRemoteDataChannel(WebRTCDataChannelHandler*) = 0;
};

} // namespace WebKit

#endif // WebRTCPeerConnectionHandlerClient_h
