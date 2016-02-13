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

// @conditional=ENABLE(MEDIA_STREAM)
// @internal

// Operation queue as specified in section 4.3.1 (WebRTC 1.0)
function enqueueOperation(peerConnection, operation)
{
    "use strict";

    if (!peerConnection.@operations)
        peerConnection.@operations = [];

    var operations = peerConnection.@operations;

    function runNext() {
        operations.@shift();
        if (operations.length)
            operations[0]();
    };

    return new @Promise(function (resolve, reject) {
        operations.@push(function() {
            operation().then(resolve, reject).then(runNext, runNext);
        });

        if (operations.length == 1)
            operations[0]();
    });
}

function createOfferOrAnswer(peerConnection, targetFunction, functionName, args)
{
    "use strict";

    var options = {};

    if (args.length <= 1) {
        // Promise mode
        if (args.length && @isDictionary(args[0]))
            options = args[0]

        return @enqueueOperation(peerConnection, function () {
            return targetFunction.@call(peerConnection, options);
        });
    }

    // Legacy callbacks mode (2 or 3 arguments)
    var successCallback = @extractCallbackArg(args, 0, "successCallback", functionName);
    var errorCallback = @extractCallbackArg(args, 1, "errorCallback", functionName);

    if (args.length > 2 && @isDictionary(args[2]))
        options = args[2];

    @enqueueOperation(peerConnection, function () {
        return targetFunction.@call(peerConnection, options).then(successCallback, errorCallback);
    });
}

function setLocalOrRemoteDescription(peerConnection, targetFunction, functionName, args)
{
    "use strict";

    if (args.length < 1)
        throw new @TypeError("Not enough arguments");

    var description = args[0];
    if (!(description instanceof @RTCSessionDescription))
        throw new @TypeError("Argument 1 ('description') to RTCPeerConnection." + functionName + " must be an instance of RTCSessionDescription");

    if (args.length == 1) {
        // Promise mode
        return @enqueueOperation(peerConnection, function () {
            return targetFunction.@call(peerConnection, description);
        });
    }

    // Legacy callbacks mode (3 arguments)
    if (args.length < 3)
        throw new @TypeError("Not enough arguments");

    var successCallback = @extractCallbackArg(args, 1, "successCallback", functionName);
    var errorCallback = @extractCallbackArg(args, 2, "errorCallback", functionName);

    @enqueueOperation(peerConnection, function () {
        return targetFunction.@call(peerConnection, description).then(successCallback, errorCallback);
    });
}

function extractCallbackArg(args, index, name, parentFunctionName)
{
    "use strict";

    var callback = args[index];
    if (typeof callback !== "function")
        throw new @TypeError("Argument " + (index + 1) + " ('" + name + "') to RTCPeerConnection." + parentFunctionName + " must be a Function");

    return callback;
}
