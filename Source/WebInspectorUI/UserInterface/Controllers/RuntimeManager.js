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

WebInspector.RuntimeManager = class RuntimeManager extends WebInspector.Object
{
    constructor()
    {
        super();

        // Enable the RuntimeAgent to receive notification of execution contexts.
        RuntimeAgent.enable();
    }

    // Public

    evaluateInInspectedWindow(expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, saveResult, callback)
    {
        if (!expression) {
            // There is no expression, so the completion should happen against global properties.
            expression = "this";
        } else if (/^\s*\{/.test(expression) && /\}\s*$/.test(expression)) {
            // Transform {a:1} to ({a:1}) so it is treated like an object literal instead of a block with a label.
            expression = "(" + expression + ")";
        }

        expression = appendWebInspectorSourceURL(expression);

        function evalCallback(error, result, wasThrown, savedResultIndex)
        {
            this.dispatchEventToListeners(WebInspector.RuntimeManager.Event.DidEvaluate, {objectGroup});

            if (error) {
                console.error(error);
                callback(null, false);
                return;
            }

            if (returnByValue)
                callback(null, wasThrown, wasThrown ? null : result, savedResultIndex);
            else
                callback(WebInspector.RemoteObject.fromPayload(result), wasThrown, savedResultIndex);
        }

        if (WebInspector.debuggerManager.activeCallFrame) {
            // COMPATIBILITY (iOS 8): "saveResult" did not exist.
            DebuggerAgent.evaluateOnCallFrame.invoke({callFrameId: WebInspector.debuggerManager.activeCallFrame.id, expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, saveResult}, evalCallback.bind(this));
            return;
        }

        // COMPATIBILITY (iOS 8): "saveResult" did not exist.
        var contextId = WebInspector.quickConsole.executionContextIdentifier;
        RuntimeAgent.evaluate.invoke({expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, contextId, returnByValue, generatePreview, saveResult}, evalCallback.bind(this));
    }

    saveResult(remoteObject, callback)
    {
        console.assert(remoteObject instanceof WebInspector.RemoteObject);

        // COMPATIBILITY (iOS 8): Runtime.saveResult did not exist.
        if (!RuntimeAgent.saveResult) {
            callback(undefined);
            return;
        }

        function mycallback(error, savedResultIndex)
        {
            callback(savedResultIndex);
        }

        if (remoteObject.objectId)
            RuntimeAgent.saveResult(remoteObject.asCallArgument(), mycallback);
        else
            RuntimeAgent.saveResult(remoteObject.asCallArgument(), WebInspector.quickConsole.executionContextIdentifier, mycallback);
    }

    getPropertiesForRemoteObject(objectId, callback)
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

WebInspector.RuntimeManager.Event = {
    DidEvaluate: "runtime-manager-did-evaluate"
};

WebInspector.RuntimeManager.ConsoleObjectGroup = "console";
