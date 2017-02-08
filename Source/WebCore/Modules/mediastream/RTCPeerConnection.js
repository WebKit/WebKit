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

function initializeRTCPeerConnection(configuration)
{
    "use strict";

    if (configuration == null)
        configuration = {};
    else if (!@isObject(configuration))
        @throwTypeError("RTCPeerConnection argument must be a valid dictionary");

    // FIXME: Handle errors in a better way than catching and re-throwing (http://webkit.org/b/158936)
    try {
        this.@initializeWith(configuration);
    } catch (e) {
        const message = e.name === "TypeMismatchError" ? "Invalid RTCPeerConnection constructor arguments"
            : "Error creating RTCPeerConnection";
        @throwTypeError(message);
    }
    this.@operations = [];
    this.@localStreams = [];

    return this;
}

function getLocalStreams()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        throw @makeThisTypeError("RTCPeerConnection", "getLocalStreams");

    return this.@localStreams.slice();
}

function getStreamById(streamIdArg)
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        throw @makeThisTypeError("RTCPeerConnection", "getStreamById");

    if (arguments.length < 1)
        @throwTypeError("Not enough arguments");

    const streamId = @String(streamIdArg);

    return this.@localStreams.find(stream => stream.id === streamId)
        || this.@getRemoteStreams().find(stream => stream.id === streamId)
        || null;
}

function addStream(stream)
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        throw @makeThisTypeError("RTCPeerConnection", "addStream");

    if (arguments.length < 1)
        @throwTypeError("Not enough arguments");

    if (!(stream instanceof @MediaStream))
        @throwTypeError("Argument 1 ('stream') to RTCPeerConnection.addStream must be an instance of MediaStream");

    if (this.@localStreams.find(localStream => localStream.id === stream.id))
        return;

    this.@localStreams.@push(stream);
    stream.@getTracks().forEach(track => this.@addTrack(track, stream));
}

function removeStream(stream)
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        throw @makeThisTypeError("RTCPeerConnection", "removeStream");

    if (arguments.length < 1)
        @throwTypeError("Not enough arguments");

    if (!(stream instanceof @MediaStream))
        @throwTypeError("Argument 1 ('stream') to RTCPeerConnection.removeStream must be an instance of MediaStream");

    const indexOfStreamToRemove = this.@localStreams.findIndex(localStream => localStream.id === stream.id);
    if (indexOfStreamToRemove === -1)
        return;

    const senders = this.@getSenders();
    this.@localStreams[indexOfStreamToRemove].@getTracks().forEach(track => {
        const senderForTrack = senders.find(sender => sender.track && sender.track.id === track.id);
        if (senderForTrack)
            this.@removeTrack(senderForTrack);
    });

    this.@localStreams.splice(indexOfStreamToRemove, 1);
}

function createOffer()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(@makeThisTypeError("RTCPeerConnection", "createOffer"));

    const peerConnection = this;

    return @callbacksAndDictionaryOverload(arguments, "createOffer", function (options) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateOffer(options);
        });
    }, function (successCallback, errorCallback, options) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateOffer(options).@then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function createAnswer()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(@makeThisTypeError("RTCPeerConnection", "createAnswer"));

    const peerConnection = this;

    return @callbacksAndDictionaryOverload(arguments, "createAnswer", function (options) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateAnswer(options);
        });
    }, function (successCallback, errorCallback, options) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedCreateAnswer(options).@then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function setLocalDescription()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(@makeThisTypeError("RTCPeerConnection", "setLocalDescription"));

    const peerConnection = this;

    // FIXME: According the spec, we should throw when receiving a RTCSessionDescription.
    const objectInfo = {
        "constructor": @RTCSessionDescription,
        "argName": "description",
        "argType": "RTCSessionDescription",
        "maybeDictionary": "true"
    };
    return @objectAndCallbacksOverload(arguments, "setLocalDescription", objectInfo, function (description) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetLocalDescription(description);
        });
    }, function (description, successCallback, errorCallback) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetLocalDescription(description).@then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function setRemoteDescription()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(@makeThisTypeError("RTCPeerConnection", "setRemoteDescription"));

    const peerConnection = this;

    // FIXME: According the spec, we should throw when receiving a RTCSessionDescription.
    const objectInfo = {
        "constructor": @RTCSessionDescription,
        "argName": "description",
        "argType": "RTCSessionDescription",
        "maybeDictionary": "true"
    };
    return @objectAndCallbacksOverload(arguments, "setRemoteDescription", objectInfo, function (description) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetRemoteDescription(description);
        });
    }, function (description, successCallback, errorCallback) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedSetRemoteDescription(description).@then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}

function addIceCandidate()
{
    "use strict";

    if (!@isRTCPeerConnection(this))
        return @Promise.@reject(@makeThisTypeError("RTCPeerConnection", "addIceCandidate"));

    const peerConnection = this;

    const objectInfo = {
        "constructor": @RTCIceCandidate,
        "argName": "candidate",
        "argType": "RTCIceCandidate",
        "maybeDictionary": "true"
    };
    return @objectAndCallbacksOverload(arguments, "addIceCandidate", objectInfo, function (candidate) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedAddIceCandidate(candidate);
        });
    }, function (candidate, successCallback, errorCallback) {
        // Legacy callbacks mode
        @enqueueOperation(peerConnection, function () {
            return peerConnection.@queuedAddIceCandidate(candidate).@then(successCallback, errorCallback);
        });

        return @Promise.@resolve(@undefined);
    });
}
