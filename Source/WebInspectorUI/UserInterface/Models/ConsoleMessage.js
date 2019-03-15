/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.ConsoleMessage = class ConsoleMessage
{
    constructor(target, source, level, message, type, url, line, column, repeatCount, parameters, callFrames, request)
    {
        console.assert(target instanceof WI.Target);
        console.assert(typeof source === "string");
        console.assert(typeof level === "string");
        console.assert(typeof message === "string");
        console.assert(!type || Object.values(WI.ConsoleMessage.MessageType).includes(type));
        console.assert(!parameters || parameters.every((x) => x instanceof WI.RemoteObject));

        this._target = target;
        this._source = source;
        this._level = level;
        this._messageText = message;
        this._type = type || WI.ConsoleMessage.MessageType.Log;

        this._url = url || null;
        this._line = line || 0;
        this._column = column || 0;
        this._sourceCodeLocation = undefined;

        this._repeatCount = repeatCount || 0;
        this._parameters = parameters;

        callFrames = callFrames || [];
        this._stackTrace = WI.StackTrace.fromPayload(this._target, {callFrames});

        this._request = request;
    }

    // Public

    get target() { return this._target; }
    get source() { return this._source; }
    get level() { return this._level; }
    get messageText() { return this._messageText; }
    get type() { return this._type; }
    get url() { return this._url; }
    get line() { return this._line; }
    get column() { return this._column; }
    get repeatCount() { return this._repeatCount; }
    get parameters() { return this._parameters; }
    get stackTrace() { return this._stackTrace; }
    get request() { return this._request; }

    get sourceCodeLocation()
    {
        if (this._sourceCodeLocation !== undefined)
            return this._sourceCodeLocation;

        // First try to get the location from the top frame of the stack trace.
        let topCallFrame = this._stackTrace.callFrames[0];
        if (topCallFrame && topCallFrame.sourceCodeLocation) {
            this._sourceCodeLocation = topCallFrame.sourceCodeLocation;
            return this._sourceCodeLocation;
        }

        // If that doesn't exist try to get a location from the url/line/column in the ConsoleMessage.
        // FIXME <http://webkit.org/b/76404>: Remove the string equality checks for undefined once we don't get that value anymore.
        if (this._url && this._url !== "undefined") {
            let sourceCode = WI.networkManager.resourceForURL(this._url);
            if (sourceCode) {
                let lineNumber = this._line > 0 ? this._line - 1 : 0;
                let columnNumber = this._column > 0 ? this._column - 1 : 0;
                this._sourceCodeLocation = new WI.SourceCodeLocation(sourceCode, lineNumber, columnNumber);
                return this._sourceCodeLocation;
            }
        }

        this._sourceCodeLocation = null;
        return this._sourceCodeLocation;
    }
};

WI.ConsoleMessage.MessageSource = {
    HTML: "html",
    XML: "xml",
    JS: "javascript",
    Network: "network",
    ConsoleAPI: "console-api",
    Storage: "storage",
    Appcache: "appcache",
    Rendering: "rendering",
    CSS: "css",
    Security: "security",
    Other: "other",
    Media: "media",
    MediaSource: "mediasource",
    WebRTC: "webrtc",
};

WI.ConsoleMessage.MessageType = {
    Log: "log",
    Dir: "dir",
    DirXML: "dirxml",
    Table: "table",
    Trace: "trace",
    StartGroup: "startGroup",
    StartGroupCollapsed: "startGroupCollapsed",
    EndGroup: "endGroup",
    Assert: "assert",
    Timing: "timing",
    Profile: "profile",
    ProfileEnd: "profileEnd",
    Image: "image",
    Result: "result", // Frontend Only.
};

WI.ConsoleMessage.MessageLevel = {
    Log: "log",
    Info: "info",
    Warning: "warning",
    Error: "error",
    Debug: "debug",
};
