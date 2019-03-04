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

    const operations = @getByIdDirectPrivate(peerConnection, "operations");

    function runNext() {
        operations.@shift();
        if (operations.length)
            operations[0]();
    };

    return new @Promise(function (resolve, reject) {
        operations.@push(function() {
            operation().@then(resolve, reject).@then(runNext, runNext);
        });

        if (operations.length === 1)
            operations[0]();
    });
}

function objectOverload(objectArg, functionName, objectInfo, promiseMode)
{
    "use strict";

    let objectArgOk = false;

    const hasMatchingType = objectArg instanceof objectInfo.constructor;
    if (hasMatchingType)
        objectArgOk = true;
    else if (objectArg == null && objectInfo.defaultsToNull) {
        objectArgOk = true;
        objectArg = null;
    } else if (objectInfo.maybeDictionary) {
        try {
            objectArg = new objectInfo.constructor(objectArg);
            objectArgOk = true;
        } catch (e) {
            objectArgOk = false;
        }
    }

    if (!objectArgOk)
        return @Promise.@reject(@makeTypeError(`Argument 1 ('${objectInfo.argName}') to RTCPeerConnection.${functionName} must be an instance of ${objectInfo.argType}`));

    return promiseMode(objectArg);
}

function isRTCPeerConnection(connection)
{
    "use strict";

    return @isObject(connection) && !!@getByIdDirectPrivate(connection, "operations");
}
