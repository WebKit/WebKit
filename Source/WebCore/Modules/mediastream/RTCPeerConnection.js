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

// @conditional=ENABLE(WEB_RTC)

function createOffer()
{
    "use strict";

    return @createOfferOrAnswer(this, this.@queuedCreateOffer, "createOffer", arguments);
}

function createAnswer()
{
    "use strict";

    return @createOfferOrAnswer(this, this.@queuedCreateAnswer, "createAnswer", arguments);
}

function setLocalDescription()
{
    "use strict";

    return @setLocalOrRemoteDescription(this, this.@queuedSetLocalDescription, "setLocalDescription", arguments);
}

function setRemoteDescription()
{
    "use strict";

    return @setLocalOrRemoteDescription(this, this.@queuedSetRemoteDescription, "setRemoteDescription", arguments);
}

function addIceCandidate()
{
    "use strict";

    var peerConnection = this;

    if (arguments.length < 1)
        throw new @TypeError("Not enough arguments");

    var candidate = arguments[0];
    if (!(candidate instanceof @RTCIceCandidate))
        throw new @TypeError("Argument 1 ('candidate') to RTCPeerConnection.addIceCandidate must be an instance of RTCIceCandidate");

    if (arguments.length == 1) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedAddIceCandidate(candidate);
        });
    }

    // Legacy callbacks mode (3 arguments)
    if (arguments.length < 3)
        throw new @TypeError("Not enough arguments");

    var successCallback = @extractCallbackArg(arguments, 1, "successCallback", "addIceCandidate");
    var errorCallback = @extractCallbackArg(arguments, 2, "errorCallback", "addIceCandidate");

    @enqueueOperation(peerConnection, function () {
        return peerConnection.@queuedAddIceCandidate(candidate).then(successCallback, errorCallback);
    });
}

function getStats()
{
    "use strict";

    var peerConnection = this;
    var selector = null;

    if (arguments.length) {
        selector = arguments[0];
        if (selector != null && !(selector instanceof @MediaStreamTrack))
            throw new @TypeError("Argument 1 ('selector') to RTCPeerConnection.getStats must be an instance of MediaStreamTrack");
    }

    if (arguments.length <= 1) {
        // Promise mode
        return peerConnection.@privateGetStats(selector);
    }

    // Legacy callbacks mode (3 arguments)
    if (arguments.length < 3)
        throw new @TypeError("Not enough arguments");

    var successCallback = @extractCallbackArg(arguments, 1, "successCallback", "getStats");
    var errorCallback = @extractCallbackArg(arguments, 2, "errorCallback", "getStats");

    peerConnection.@privateGetStats(selector).then(successCallback, errorCallback);
}
