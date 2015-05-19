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

WebInspector.ConsoleMessage = class ConsoleMessage extends WebInspector.Object
{
    constructor(source, level, message, type, url, line, column, repeatCount, parameters, stackTrace, request)
    {
        super();

        console.assert(typeof source === "string");
        console.assert(typeof level === "string");
        console.assert(typeof message === "string");
        console.assert(!parameters || parameters.every(function(x) { return x instanceof WebInspector.RemoteObject; }));

        this._source = source;
        this._level = level;
        this._messageText = message;
        this._type = type || WebInspector.ConsoleMessage.MessageType.Log;
        this._url = url || null;
        this._line = line || 0;
        this._column = column || 0;

        this._repeatCount = repeatCount || 0;
        this._parameters = parameters;

        this._stackTrace = WebInspector.StackTrace.fromPayload(stackTrace || []);

        this._request = request;
    }

    // Public

    get source()
    {
        return this._source;
    }

    get level()
    {
        return this._level;
    }

    get messageText()
    {
        return this._messageText;
    }

    get type()
    {
        return this._type;
    }

    get url()
    {
        return this._url;
    }

    get line()
    {
        return this._line;
    }

    get column()
    {
        return this._column;
    }

    get repeatCount()
    {
        return this._repeatCount;
    }

    get parameters()
    {
        return this._parameters;
    }

    get stackTrace()
    {
        return this._stackTrace;
    }

    get request()
    {
        return this._request;
    }
};

WebInspector.ConsoleMessage.MessageSource = {
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
};

WebInspector.ConsoleMessage.MessageType = {
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
    Result: "result", // Frontend Only.
};

WebInspector.ConsoleMessage.MessageLevel = {
    Log: "log",
    Info: "info",
    Warning: "warning",
    Error: "error",
    Debug: "debug",
};
