/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
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

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(new @TypeError("Function should be called on an RTCPeerConnection"));

    const peerConnection = this;

    return @callbacksAndDictionaryOverload(arguments, "createOffer", function (options) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateOffer(options);
        });
    }, function (successCallback, errorCallback, options) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateOffer(options).then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function createAnswer()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(new @TypeError("Function should be called on an RTCPeerConnection"));

    const peerConnection = this;

    return @callbacksAndDictionaryOverload(arguments, "createAnswer", function (options) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateAnswer(options);
        });
    }, function (successCallback, errorCallback, options) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateAnswer(options).then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function setLocalDescription()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(new @TypeError("Function should be called on an RTCPeerConnection"));

    const peerConnection = this;

    const objectInfo = {
        "constructor": @RTCSessionDescription,
        "argName": "description",
        "argType": "RTCSessionDescription"
    };
    return @objectAndCallbacksOverload(arguments, "setLocalDescription", objectInfo, function (description) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetLocalDescription(description);
        });
    }, function (description, successCallback, errorCallback) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetLocalDescription(description).then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function setRemoteDescription()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(new @TypeError("Function should be called on an RTCPeerConnection"));

    const peerConnection = this;

    const objectInfo = {
        "constructor": @RTCSessionDescription,
        "argName": "description",
        "argType": "RTCSessionDescription"
    };
    return @objectAndCallbacksOverload(arguments, "setRemoteDescription", objectInfo, function (description) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetRemoteDescription(description);
        });
    }, function (description, successCallback, errorCallback) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetRemoteDescription(description).then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function addIceCandidate()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(new @TypeError("Function should be called on an RTCPeerConnection"));

    const peerConnection = this;

    const objectInfo = {
        "constructor": @RTCIceCandidate,
        "argName": "candidate",
        "argType": "RTCIceCandidate"
    };
    return @objectAndCallbacksOverload(arguments, "addIceCandidate", objectInfo, function (candidate) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedAddIceCandidate(candidate);
        });
    }, function (candidate, successCallback, errorCallback) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedAddIceCandidate(candidate).then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function getStats()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(new @TypeError("Function should be called on an RTCPeerConnection"));

    const peerConnection = this;

    const objectInfo = {
        "constructor": @MediaStreamTrack,
        "argName": "selector",
        "argType": "MediaStreamTrack",
        "defaultsToNull": true
    };
    return @objectAndCallbacksOverload(arguments, "getStats", objectInfo, function (selector) {
        // Promise mode
        return peerConnection.@privateGetStats(selector);
    }, function (selector, successCallback, errorCallback) {
        // Legacy callbacks mode
        peerConnection.@privateGetStats(selector).then(successCallback, errorCallback);

        return @Promise.@resolve(@undefined);
    });
}
