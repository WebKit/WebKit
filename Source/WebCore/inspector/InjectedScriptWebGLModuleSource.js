/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

/**
 * @param {InjectedScriptHost} InjectedScriptHost
 */
(function (InjectedScriptHost, inspectedWindow, injectedScriptId) {

/**
 * @constructor
 */
var InjectedScript = function()
{
    this._lastBoundObjectId = 0;
    this._idToWrapperProxy = {};
    this._idToRealWebGLContext = {};
    this._capturingFrameInfo = null;
}

InjectedScript.prototype = {
    wrapWebGLContext: function(glContext)
    {
        for (var id in this._idToRealWebGLContext) {
            if (this._idToRealWebGLContext[id] === glContext)
                return this._idToWrapperProxy[id];
        }

        var proxy = {};
        var nameProcessed = {};
        nameProcessed.__proto__ = null;
        nameProcessed.constructor = true;

        function processName(name) {
            if (nameProcessed[name])
                return;
            nameProcessed[name] = true;
            if (typeof glContext[name] === "function")
                proxy[name] = injectedScript._wrappedFunction.bind(injectedScript, glContext, name);
            else
                Object.defineProperty(proxy, name, {
                    get: function()
                    {
                        return glContext[name];
                    },
                    set: function(value)
                    {
                        glContext[name] = value;
                    }
                });
        }

        for (var o = glContext; o; o = o.__proto__)
            Object.getOwnPropertyNames(o).forEach(processName);

        // In order to emulate "instanceof".
        proxy.__proto__ = glContext.__proto__;
        proxy.constructor = glContext.constructor;

        var contextId = this._generateObjectId();
        this._idToWrapperProxy[contextId] = proxy;
        this._idToRealWebGLContext[contextId] = glContext;
        InjectedScriptHost.webGLContextCreated(contextId);

        return proxy;
    },

    _generateObjectId: function()
    {
        var id = ++this._lastBoundObjectId;
        var objectId = "{\"injectedScriptId\":" + injectedScriptId + ",\"webGLId\":" + id + "}";
        return objectId;
    },

    captureFrame: function(contextId)
    {
        this._capturingFrameInfo = {
            contextId: contextId,
            capturedCallsNum: 0
        };
    },

    _stopCapturing: function(info)
    {
        if (this._capturingFrameInfo === info)
            this._capturingFrameInfo = null;
    },

    _wrappedFunction: function(glContext, functionName)
    {
        // Call real WebGL function.
        var args = Array.prototype.slice.call(arguments, 2);
        var result = glContext[functionName].apply(glContext, args);

        if (this._capturingFrameInfo && this._idToRealWebGLContext[this._capturingFrameInfo.contextId] === glContext) {
            var capturedCallsNum = ++this._capturingFrameInfo.capturedCallsNum;
            if (capturedCallsNum === 1)
                this._setZeroTimeouts(this._stopCapturing.bind(this, this._capturingFrameInfo));
            InjectedScriptHost.webGLReportFunctionCall(this._capturingFrameInfo.contextId, functionName, "[" + args.join(", ") + "]", result + "");
        }

        return result;
    },

    _setZeroTimeouts: function(callback)
    {
        // We need a fastest async callback, whatever fires first.
        // Usually a postMessage should be faster than a setTimeout(0).
        var channel = new MessageChannel();
        channel.port1.onmessage = callback;
        channel.port2.postMessage("");
        inspectedWindow.setTimeout(callback, 0);
    }
};

var injectedScript = new InjectedScript();
return injectedScript;

})
