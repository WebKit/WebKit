/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * @constructor
 */
function InspectorBackendClass()
{
    this._lastCallbackId = 1;
    this._pendingResponsesCount = 0;
    this._callbacks = {};
    this._domainDispatchers = {};
    this._eventArgs = {};
    this._replyArgs = {};
    this._deferredScripts = [];

    this.dumpInspectorTimeStats = false;
    this.dumpInspectorProtocolMessages = false;
}

InspectorBackendClass.prototype = {
    _registerPendingResponse: function(callback, methodName)
    {
        var callbackId = this._lastCallbackId++;
        if (!callback)
            callback = function() {};

        this._callbacks[callbackId] = callback;
        callback.methodName = methodName;
        if (this.dumpInspectorTimeStats)
            callback.sendRequestTime = Date.now();

        ++this._pendingResponsesCount;

        return callbackId;
    },

    _invokeMethod: function(command, parameters, callback)
    {
        var messageObject = {};
        messageObject["method"] = command.methodName;
        if (parameters)
            messageObject["params"] = parameters;
        messageObject["id"] = this._registerPendingResponse(callback, command.methodName);

        var stringifiedMessage = JSON.stringify(messageObject);
        if (this.dumpInspectorProtocolMessages)
            console.log("frontend: " + stringifiedMessage);

        InspectorFrontendHost.sendMessageToBackend(stringifiedMessage);
    },

    _getAgent: function(domain)
    {
        var agentName = domain + "Agent";
        if (!window[agentName])
            window[agentName] = {};
        return window[agentName];
    },

    registerCommand: function(method, signature, replyArgs)
    {
        var domainAndMethod = method.split(".");
        var agent = this._getAgent(domainAndMethod[0]);
        agent[domainAndMethod[1]] = InspectorBackendCommand.create(this, method, signature);

        this._replyArgs[method] = replyArgs;
    },

    registerEnum: function(type, values)
    {
        var domainAndMethod = type.split(".");
        var agent = this._getAgent(domainAndMethod[0]);

        agent[domainAndMethod[1]] = values;
    },

    registerEvent: function(eventName, parameters)
    {
        this._eventArgs[eventName] = parameters;
    },

    registerDomainDispatcher: function(domain, dispatcher)
    {
        this._domainDispatchers[domain] = dispatcher;
    },

    dispatch: function(message)
    {
        if (this.dumpInspectorProtocolMessages)
            console.log("backend: " + ((typeof message === "string") ? message : JSON.stringify(message)));

        var messageObject = (typeof message === "string") ? JSON.parse(message) : message;

        if ("id" in messageObject) { // just a response for some request
            if (messageObject.error) {
                if (messageObject.error.code !== -32000)
                    this.reportProtocolError(messageObject);
            }

            var callback = this._callbacks[messageObject.id];
            if (callback) {
                var argumentsArray = [];
                if (messageObject.result) {
                    if (callback.expectsResultObject) {
                        // The callback expects results as an object with properties, this is useful
                        // for backwards compatibility with renamed or different parameters.
                        argumentsArray.push(messageObject.result);
                    } else {
                        var paramNames = this._replyArgs[callback.methodName];
                        if (paramNames) {
                            for (var i = 0; i < paramNames.length; ++i)
                                argumentsArray.push(messageObject.result[paramNames[i]]);
                        }
                    }
                }

                var processingStartTime;
                if (this.dumpInspectorTimeStats && callback.methodName)
                    processingStartTime = Date.now();

                argumentsArray.unshift(messageObject.error ? messageObject.error.message : null);
                callback.apply(null, argumentsArray);
                --this._pendingResponsesCount;
                delete this._callbacks[messageObject.id];

                if (this.dumpInspectorTimeStats && callback.methodName)
                    console.log("time-stats: " + callback.methodName + " = " + (processingStartTime - callback.sendRequestTime) + " + " + (Date.now() - processingStartTime));
            }

            if (this._deferredScripts.length && !this._pendingResponsesCount)
                this._flushPendingScripts();

            return;
        } else {
            var method = messageObject.method.split(".");
            var domainName = method[0];
            var functionName = method[1];
            if (!(domainName in this._domainDispatchers)) {
                console.error("Protocol Error: Attempted to dispatch method '" + functionName + "' for non-existing domain '" + domainName + "'");
                return;
            }
            var dispatcher = this._domainDispatchers[domainName];
            if (!(functionName in dispatcher)) {
                console.error("Protocol Error: Attempted to dispatch an unimplemented method '" + messageObject.method + "'");
                return;
            }

            if (!this._eventArgs[messageObject.method]) {
                console.error("Protocol Error: Attempted to dispatch an unspecified method '" + messageObject.method + "'");
                return;
            }

            var params = [];
            if (messageObject.params) {
                var paramNames = this._eventArgs[messageObject.method];
                for (var i = 0; i < paramNames.length; ++i)
                    params.push(messageObject.params[paramNames[i]]);
            }

            var processingStartTime;
            if (this.dumpInspectorTimeStats)
                processingStartTime = Date.now();

            try {
                dispatcher[functionName].apply(dispatcher, params);
            } catch (e) {
                console.error("Uncaught exception in inspector page: ", e);
            }

            if (this.dumpInspectorTimeStats)
                console.log("time-stats: " + messageObject.method + " = " + (Date.now() - processingStartTime));
        }
    },

    reportProtocolError: function(messageObject)
    {
        console.error("Request with id = " + messageObject.id + " failed. " + JSON.stringify(messageObject.error));
    },

    runAfterPendingDispatches: function(script)
    {
        console.assert(script);
        console.assert(typeof script === "function");

        if (!this._pendingResponsesCount)
            script.call(this);
        else
            this._deferredScripts.push(script);
    },

    // Private

    _flushPendingScripts: function()
    {
        console.assert(!this._pendingResponsesCount);

        var scriptsToRun = this._deferredScripts;
        this._deferredScripts = [];
        for (var script of scriptsToRun)
            script.call(this);
    }
}

InspectorBackend = new InspectorBackendClass();

InspectorBackendCommand = function(backend, methodName, callSignature)
{
    this._backend = backend;
    this.methodName = methodName;
    this._callSignature = callSignature;
    this._instance = this;
}

InspectorBackendCommand.create = function(backend, methodName, callSignature)
{
    var instance = new InspectorBackendCommand(backend, methodName, callSignature);

    function callable() {
        instance._invokeWithArguments.apply(instance, arguments);
    }
    callable._instance = instance;
    callable.__proto__ = InspectorBackendCommand.prototype;
    return callable;
}

// As part of the workaround to make commands callable, these functions use |this._instance|.
// |this| could refer to the callable trampoline, or the InspectorBackendCommand instance.
InspectorBackendCommand.prototype = {
    __proto__: Function.prototype,

    invoke: function(args, callback)
    {
        var instance = this._instance;
        instance._backend._invokeMethod(instance, args, callback);
    },

    promise: function()
    {
        var instance = this._instance;
        var promiseArguments = Array.prototype.slice.call(arguments);
        return new Promise(function(resolve, reject) {
            function convertToPromiseCallback(error, payload) {
                return error ? reject(error) : resolve(payload);
            }
            promiseArguments.push(convertToPromiseCallback);
            instance._invokeWithArguments.apply(instance, promiseArguments);
        });
    },

    supports: function(parameterName)
    {
        var instance = this._instance;
        return instance._callSignature.any(function(parameter) {
            return parameter["name"] === parameterName
        });
    },

    _invokeWithArguments: function()
    {
        var instance = this._instance;
        var args = Array.prototype.slice.call(arguments);
        var callback = typeof args.lastValue === "function" ? args.pop() : null;

        var parameters = {};
        for (var i = 0; i < instance._callSignature.length; ++i) {
            var parameter = instance._callSignature[i];
            var parameterName = parameter["name"];
            var typeName = parameter["type"];
            var optionalFlag = parameter["optional"];

            if (!args.length && !optionalFlag) {
                console.error("Protocol Error: Invalid number of arguments for method '" + instance.methodName + "' call. It must have the following arguments '" + JSON.stringify(signature) + "'.");
                return;
            }

            var value = args.shift();
            if (optionalFlag && typeof value === "undefined")
                continue;

            if (typeof value !== typeName) {
                console.error("Protocol Error: Invalid type of argument '" + parameterName + "' for method '" + instance.methodName + "' call. It must be '" + typeName + "' but it is '" + typeof value + "'.");
                return;
            }

            parameters[parameterName] = value;
        }

        if (args.length === 1 && !callback) {
            if (typeof args[0] !== "undefined") {
                console.error("Protocol Error: Optional callback argument for method '" + instance.methodName + "' call must be a function but its type is '" + typeof args[0] + "'.");
                return;
            }
        }

        instance._backend._invokeMethod(instance, Object.keys(parameters).length ? parameters : null, callback);
    },
}
