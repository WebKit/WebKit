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
// @internal

// Operation queue as specified in section 4.3.1 (WebRTC 1.0)
function enqueueOperation(peerConnection, operation)
{
    "use strict";

    const operations = peerConnection.@operations;

    function runNext() {
        operations.@shift();
        if (operations.length)
            operations[0]();
    };

    return new @Promise(function (resolve, reject) {
        operations.@push(function() {
            operation().@then(resolve, reject).@then(runNext, runNext);
        });

        if (operations.length == 1)
            operations[0]();
    });
}

function objectAndCallbacksOverload(args, functionName, objectInfo, promiseMode, legacyMode)
{
    "use strict";

    let argsCount = args.length;
    let objectArg = args[0];
    let objectArgOk = false;

    if (!argsCount) {
        if (!objectInfo.defaultsToNull)
            return @Promise.@reject(new @TypeError("Not enough arguments"));

        objectArg = null;
        objectArgOk = true;
        argsCount = 1;
    } else {
        const hasMatchingType = objectArg instanceof objectInfo.constructor;
        objectArgOk = objectInfo.defaultsToNull ? (objectArg === null || typeof objectArg === "undefined" || hasMatchingType) : hasMatchingType;
    }

    if (!objectArgOk)
        return @Promise.@reject(new @TypeError(`Argument 1 ('${objectInfo.argName}') to RTCPeerConnection.${functionName} must be an instance of ${objectInfo.argType}`));

    if (argsCount === 1)
        return promiseMode(objectArg);

    // More than one argument: Legacy mode
    if (argsCount < 3)
        return @Promise.@reject(new @TypeError("Not enough arguments"));

    const successCallback = args[1];
    const errorCallback = args[2];

    if (typeof successCallback !== "function")
        return @Promise.@reject(new @TypeError(`Argument 2 ('successCallback') to RTCPeerConnection.${functionName} must be a function`));

    if (typeof errorCallback !== "function")
        return @Promise.@reject(new @TypeError(`Argument 3 ('errorCallback') to RTCPeerConnection.${functionName} must be a function`));

    return legacyMode(objectArg, successCallback, errorCallback);
}

function callbacksAndDictionaryOverload(args, functionName, promiseMode, legacyMode)
{
    "use strict";

    if (args.length <= 1) {
        // Zero or one arguments: Promise mode
        const options = args[0];
        if (args.length && !@isDictionary(options))
            return @Promise.@reject(new @TypeError(`Argument 1 ('options') to RTCPeerConnection.${functionName} must be a Dictionary`));

        return promiseMode(options);
    }

    // More than one argument: Legacy mode
    const successCallback = args[0];
    const errorCallback = args[1];
    const options = args[2];

    if (typeof successCallback !== "function")
        return @Promise.@reject(new @TypeError(`Argument 1 ('successCallback') to RTCPeerConnection.${functionName} must be a function`));

    if (typeof errorCallback !== "function")
        return @Promise.@reject(new @TypeError(`Argument 2 ('errorCallback') to RTCPeerConnection.${functionName} must be a function`));

    if (args.length > 2 && !@isDictionary(options))
        return @Promise.@reject(new @TypeError(`Argument 3 ('options') to RTCPeerConnection.${functionName} must be a Dictionary`));

    return legacyMode(successCallback, errorCallback, args[2]);
}

function isRTCPeerConnection(connection)
{
    "use strict";

    return @isObject(connection) && !!connection.@operations;
}
