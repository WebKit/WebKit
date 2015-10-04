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

    static _parseStackTrace(stack)
    {
        var lines = stack.split(/\n/g);
        var result = [];

        for (var line of lines) {
            var functionName = "";
            var url = "";
            var lineNumber = 0;
            var columnNumber = 0;

            var index = line.indexOf("@");
            if (index !== -1) {
                functionName = line.slice(0, index);
                url = line.slice(index + 1);

                var columnIndex = url.lastIndexOf(":");
                if (columnIndex !== -1) {
                    columnNumber = parseInt(url.slice(columnIndex + 1));

                    url = url.slice(0, columnIndex);
                    var lineIndex = url.lastIndexOf(":", columnIndex);
                    if (lineIndex !== -1) {
                        lineNumber = parseInt(url.slice(lineIndex + 1, columnIndex));
                        url = url.slice(0, lineIndex);
                    }
                }
            } else
                functionName = line;

            result.push({functionName, url, lineNumber, columnNumber});
        }

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
