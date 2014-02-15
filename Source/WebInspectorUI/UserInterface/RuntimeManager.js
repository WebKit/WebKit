/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.RuntimeManager = function()
{
    WebInspector.Object.call(this);

    // Enable the RuntimeAgent to receive notification of execution contexts.
    if (RuntimeAgent.enable)
        RuntimeAgent.enable();
};

WebInspector.RuntimeManager.Event = {
    DidEvaluate: "runtime-manager-did-evaluate"
};

WebInspector.RuntimeManager.prototype = {
    constructor: WebInspector.RuntimeManager,

    // Public

    evaluateInInspectedWindow: function(expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, callback)
    {
        if (!expression) {
            // There is no expression, so the completion should happen against global properties.
            expression = "this";
        }

        function evalCallback(error, result, wasThrown)
        {
            this.dispatchEventToListeners(WebInspector.RuntimeManager.Event.DidEvaluate);
            
            if (error) {
                console.error(error);
                callback(null, false);
                return;
            }

            if (returnByValue)
                callback(null, wasThrown, wasThrown ? null : result);
            else
                callback(WebInspector.RemoteObject.fromPayload(result), wasThrown);
        }

        if (WebInspector.debuggerManager.activeCallFrame) {
            DebuggerAgent.evaluateOnCallFrame(WebInspector.debuggerManager.activeCallFrame.id, expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, evalCallback.bind(this));
            return;
        }

        // COMPATIBILITY (iOS 6): Execution context identifiers (contextId) did not exist
        // in iOS 6. Fallback to including the frame identifier (frameId).
        var contextId = WebInspector.quickConsole.executionContextIdentifier;
        RuntimeAgent.evaluate.invoke({expression: expression, objectGroup: objectGroup, includeCommandLineAPI: includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole: doNotPauseOnExceptionsAndMuteConsole, contextId: contextId, frameId: contextId, returnByValue: returnByValue}, evalCallback.bind(this));
    },

    getPropertiesForRemoteObject: function(objectId, callback)
    {
        RuntimeAgent.getProperties(objectId, function(error, result) {
            if (error) {
                callback(error);
                return;
            }

            var properties = new Map;
            for (var property of result)
                properties.set(property.name, property);

            callback(null, properties);
        });
    }
};

WebInspector.RuntimeManager.prototype.__proto__ = WebInspector.Object.prototype;
