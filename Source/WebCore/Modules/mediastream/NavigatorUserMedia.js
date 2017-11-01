/*
 * Copyright (C) 2015 Canon Inc.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// @conditional=ENABLE(MEDIA_STREAM)

function getUserMedia(options, successCallback, errorCallback)
{
    "use strict";

    // FIXME: We should raise a DOM unsupported exception if there is no navigator and properly detect whether method is not called on a Navigator object.
    if (!(this.mediaDevices && this.mediaDevices.@getUserMedia))
        throw @makeThisTypeError("Navigator", "getUserMedia");

    if (arguments.length < 3)
        @throwTypeError("Not enough arguments");

    if (!@isObject(options))
        @throwTypeError("Argument 1 (options) to Navigator.getUserMedia must be an object");

    if (typeof successCallback !== "function")
        @throwTypeError("Argument 2 ('successCallback') to Navigator.getUserMedia must be a function");
    if (typeof errorCallback !== "function")
        @throwTypeError("Argument 3 ('errorCallback') to Navigator.getUserMedia must be a function");

    this.mediaDevices.@getUserMedia(options).@then(successCallback, errorCallback);
}
