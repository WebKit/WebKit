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

WebInspector.StackTrace = class StackTrace extends WebInspector.Object
{
    constructor(callFrames)
    {
        super();

        console.assert(callFrames && callFrames.every(function(callFrame) { return callFrame instanceof WebInspector.CallFrame; }));

        this._callFrames = callFrames;
    }

    // Static

    static fromPayload(payload)
    {
        var callFrames = payload.map(WebInspector.CallFrame.fromPayload);
        return new WebInspector.StackTrace(callFrames);
    }

    static fromString(stack)
    {
        var payload = WebInspector.StackTrace._parseStackTrace(stack);
        return WebInspector.StackTrace.fromPayload(payload);
    }

    // May produce false negatives; must not produce any false positives.
    // It may return false on a valid stack trace, but it will never return true on an invalid stack trace.
    static isLikelyStackTrace(stack)
    {
        // This function runs for every logged string. It penalizes the performance.
        // As most logged strings are not stack traces, exit as early as possible.
        const smallestPossibleStackTraceLength = "http://a.bc/:9:1".length;
        if (stack.length < smallestPossibleStackTraceLength.length * 2)
            return false;

        const approximateStackLengthOf50Items = 5000;
        if (stack.length > approximateStackLengthOf50Items)
            return false;

        if (/^[^a-z$_]/i.test(stack[0]))
            return false;

        const reasonablyLongLineLength = 500;
        const reasonablyLongNativeMethodLength = 120;
        const stackTraceLine = `(.{1,${reasonablyLongLineLength}}:\\d+:\\d+|eval code|.{1,${reasonablyLongNativeMethodLength}}@\\[native code\\])`;
        const stackTrace = new RegExp(`^${stackTraceLine}(\\n${stackTraceLine})*$`, "g");

        return stackTrace.test(stack);
    }

    static _parseStackTrace(stack)
    {
        var lines = stack.split(/\n/g);
        var result = [];

        for (var line of lines) {
            var functionName = "";
            var url = "";
            var lineNumber = 0;
            var columnNumber = 0;
            var atIndex = line.indexOf("@");

            if (atIndex !== -1) {
                functionName = line.slice(0, atIndex);
                ({url, lineNumber, columnNumber} = WebInspector.StackTrace._parseLocation(line.slice(atIndex + 1)));
            } else if (line.includes("/"))
                ({url, lineNumber, columnNumber} = WebInspector.StackTrace._parseLocation(line));
            else
                functionName = line;

            result.push({functionName, url, lineNumber, columnNumber});
        }

        return result;
    }

    static _parseLocation(locationString)
    {
        var result = {url: "", lineNumber: 0, columnNumber: 0};
        var locationRegEx = /(.+?)(?::(\d+)(?::(\d+))?)?$/;
        var matched = locationString.match(locationRegEx);

        if (!matched)
            return result;

        result.url = matched[1];

        if (matched[2])
            result.lineNumber = parseInt(matched[2]);

        if (matched[3])
            result.columnNumber = parseInt(matched[3]);

        return result;
    }

    // Public

    get callFrames()
    {
        return this._callFrames;
    }

    get firstNonNativeCallFrame()
    {
        for (var frame of this._callFrames) {
            if (!frame.nativeCode)
                return frame;
        }

        return null;
    }
};
