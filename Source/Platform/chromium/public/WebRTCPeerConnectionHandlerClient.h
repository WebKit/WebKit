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
        ICEConnectionStateStarting = 1,
        ICEConnectionStateChecking = 2,
        ICEConnectionStateConnected = 3,
        ICEConnectionStateCompleted = 4,
        ICEConnectionStateFailed = 5,
        ICEConnectionStateDisconnected = 6,
        ICEConnectionStateClosed = 7,
    };

    enum ICEGatheringState {
        ICEGatheringStateNew = 1,
        ICEGatheringStateGathering = 2,
        ICEGatheringStateComplete = 3
    };

    virtual ~WebRTCPeerConnectionHandlerClient() { }

    virtual void negotiationNeeded() = 0;
    virtual void didGenerateICECandidate(const WebRTCICECandidate&) = 0;
    virtual void didChangeSignalingState(SignalingState) { }
    virtual void didChangeICEGatheringState(ICEGatheringState) { }
    virtual void didChangeICEConnectionState(ICEConnectionState) { }
    virtual void didAddRemoteStream(const WebMediaStream&) = 0;
    virtual void didRemoveRemoteStream(const WebMediaStream&) = 0;
    virtual void didAddRemoteDataChannel(WebRTCDataChannelHandler*) { }

    // DEPRECATED
    enum ReadyState {
        ReadyStateStable = 1,
        ReadyStateHaveLocalOffer = 2,
        ReadyStateHaveRemoteOffer = 3,
        ReadyStateHaveLocalPrAnswer = 4,
        ReadyStateHaveRemotePrAnswer = 5,
        ReadyStateClosed = 6,
        ReadyStateNew = 7,
        ReadyStateActive = 8,
        ReadyStateClosing = 9,
        ReadyStateOpening = 10
    };

    // DEPRECATED
    enum ICEState {
        ICEStateStarting = 1,
        ICEStateChecking = 2,
        ICEStateConnected = 3,
        ICEStateCompleted = 4,
        ICEStateFailed = 5,
        ICEStateDisconnected = 6,
        ICEStateClosed = 7,
        ICEStateNew = 8,
        ICEStateGathering = 9,
        ICEStateWaiting = 10
    };

    // DEPRECATED
    virtual void didChangeReadyState(ReadyState) { }
    virtual void didChangeICEState(ICEState) { }
};

} // namespace WebKit

#endif // WebRTCPeerConnectionHandlerClient_h
