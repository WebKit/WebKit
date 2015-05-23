/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington.
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

InspectorBackendClass = class InspectorBackendClass
{
    constructor()
    {
        this._lastSequenceId = 1;
        this._pendingResponsesCount = 0;
        this._callbackData = new Map;
        this._agents = {};
        this._deferredScripts = [];

        this.dumpInspectorTimeStats = false;
        this.dumpInspectorProtocolMessages = false;
        this.warnForLongMessageHandling = false;
        this.longMessageHandlingThreshold = 10; // milliseconds.
    }

    // Public

    registerCommand(qualifiedName, callSignature, replySignature)
    {
        var [domainName, commandName] = qualifiedName.split(".");
        var agent = this._agentForDomain(domainName);
        agent.addCommand(InspectorBackend.Command.create(this, qualifiedName, callSignature, replySignature));
    }

    registerEnum(qualifiedName, enumValues)
    {
        var [domainName, enumName] = qualifiedName.split(".");
        var agent = this._agentForDomain(domainName);
        agent.addEnum(enumName, enumValues);
    }

    registerEvent(qualifiedName, signature)
    {
        var [domainName, eventName] = qualifiedName.split(".");
        var agent = this._agentForDomain(domainName);
        agent.addEvent(new InspectorBackend.Event(eventName, signature));
    }

    registerDomainDispatcher(domainName, dispatcher)
    {
        var agent = this._agentForDomain(domainName);
        agent.dispatcher = dispatcher;
    }

    dispatch(message)
    {
        if (this.dumpInspectorProtocolMessages)
            console.log("backend: " + ((typeof message === "string") ? message : JSON.stringify(message)));

        var messageObject = (typeof message === "string") ? JSON.parse(message) : message;

        if ("id" in messageObject)
            this._dispatchCallback(messageObject);
        else
            this._dispatchEvent(messageObject);
    }

    runAfterPendingDispatches(script)
    {
        console.assert(script);
        console.assert(typeof script === "function");

        if (!this._pendingResponsesCount)
            script.call(this);
        else
            this._deferredScripts.push(script);
    }

    activateDomain(domainName, activationDebuggableType)
    {
        if (!activationDebuggableType || InspectorFrontendHost.debuggableType() === activationDebuggableType) {
            var agent = this._agents[domainName];
            agent.activate();
            return agent;
        }

        return null;
    }

    // Private

    _agentForDomain(domainName)
    {
        if (this._agents[domainName])
            return this._agents[domainName];

        var agent = new InspectorBackend.Agent(domainName);
        this._agents[domainName] = agent;
        return agent;
    }

    _willSendMessageToBackend(command, callback)
    {
        ++this._pendingResponsesCount;
        var sequenceId = this._lastSequenceId++;

        if (callback && typeof callback === "function") {
            var callbackData =  {
                "callback": callback,
                "command": command,
            };

            if (this.dumpInspectorTimeStats)
                callbackData.sendRequestTime = Date.now();

            this._callbackData.set(sequenceId, callbackData);
        }

        return sequenceId;
    }

    _dispatchCallback(messageObject)
    {
        --this._pendingResponsesCount;
        console.assert(this._pendingResponsesCount >= 0);

        if (messageObject["error"]) {
            if (messageObject["error"].code !== -32000)
                this._reportProtocolError(messageObject);
        }

        var callbackData = this._callbackData.get(messageObject["id"]);
        if (callbackData && typeof callbackData.callback === "function") {
            var command = callbackData.command;
            var callback = callbackData.callback;
            var callbackArguments = [];

            callbackArguments.push(messageObject["error"] ? messageObject["error"].message : null);

            if (messageObject["result"]) {
                // FIXME: this should be indicated by invoking the command differently, rather
                // than by setting a magical property on the callback. <webkit.org/b/132386>
                if (callback.expectsResultObject) {
                    // The callback expects results as an object with properties, this is useful
                    // for backwards compatibility with renamed or different parameters.
                    callbackArguments.push(messageObject["result"]);
                } else {
                    for (var parameterName of command.replySignature)
                        callbackArguments.push(messageObject["result"][parameterName]);
                }
            }

            var processingStartTime;
            if (this.dumpInspectorTimeStats)
                processingStartTime = Date.now();

            try {
                callback.apply(null, callbackArguments);
            } catch (e) {
                console.error("Uncaught exception in inspector page while dispatching callback for command " + command.qualifiedName + ": ", e, e.stack);
            }

            var processingDuration = Date.now() - processingStartTime;
            if (this.warnForLongMessageHandling && processingDuration > this.longMessageHandlingThreshold)
                console.warn("InspectorBackend: took " + processingDuration + "ms to handle response for command: " + command.qualifiedName);

            if (this.dumpInspectorTimeStats)
                console.log("time-stats: Handling: " + processingDuration + "ms; RTT: " + (processingStartTime - callbackData.sendRequestTime) + "ms; (command " + command.qualifiedName + ")");

            this._callbackData.delete(messageObject["id"]);
        }

        if (this._deferredScripts.length && !this._pendingResponsesCount)
            this._flushPendingScripts();
    }

    _dispatchEvent(messageObject)
    {
        var qualifiedName = messageObject["method"];
        var [domainName, eventName] = qualifiedName.split(".");
        if (!(domainName in this._agents)) {
            console.error("Protocol Error: Attempted to dispatch method '" + eventName + "' for non-existing domain '" + domainName + "'");
            return;
        }

        var agent = this._agentForDomain(domainName);
        if (!agent.active) {
            console.error("Protocol Error: Attempted to dispatch method for domain '" + domainName + "' which exists but is not active.");
            return;
        }

        var event = agent.getEvent(eventName);
        if (!event) {
            console.error("Protocol Error: Attempted to dispatch an unspecified method '" + qualifiedName + "'");
            return;
        }

        var eventArguments = [];
        if (messageObject["params"]) {
            var parameterNames = event.parameterNames;
            for (var i = 0; i < parameterNames.length; ++i)
                eventArguments.push(messageObject["params"][parameterNames[i]]);
        }

        var processingStartTime;
        if (this.dumpInspectorTimeStats)
            processingStartTime = Date.now();

        try {
            agent.dispatchEvent(eventName, eventArguments);
        } catch (e) {
            console.error("Uncaught exception in inspector page while handling event " + qualifiedName + ": ", e, e.stack);
        }

        var processingDuration = Date.now() - processingStartTime;
        if (this.warnForLongMessageHandling && processingDuration > this.longMessageHandlingThreshold)
            console.warn("InspectorBackend: took " + processingDuration + "ms to handle event: " + messageObject["method"]);

        if (this.dumpInspectorTimeStats)
            console.log("time-stats: Handling: " + processingDuration + "ms (event " + messageObject["method"] + ")");
    }

    _invokeCommand(command, parameters, callback)
    {
        var messageObject = {};
        messageObject["method"] = command.qualifiedName;

        if (parameters)
            messageObject["params"] = parameters;

        // We always assign an id as a sequence identifier.
        // Callback data is saved only if a callback is actually passed.
        messageObject["id"] = this._willSendMessageToBackend(command, callback);

        var stringifiedMessage = JSON.stringify(messageObject);
        if (this.dumpInspectorProtocolMessages)
            console.log("frontend: " + stringifiedMessage);

        InspectorFrontendHost.sendMessageToBackend(stringifiedMessage);
    }

    _reportProtocolError(messageObject)
    {
        console.error("Request with id = " + messageObject["id"] + " failed. " + JSON.stringify(messageObject["error"]));
    }

    _flushPendingScripts()
    {
        console.assert(!this._pendingResponsesCount);

        var scriptsToRun = this._deferredScripts;
        this._deferredScripts = [];
        for (var script of scriptsToRun)
            script.call(this);
    }
};

InspectorBackend = new InspectorBackendClass;

InspectorBackend.Agent = class InspectorBackendAgent
{
    constructor(domainName)
    {
        this._domainName = domainName;

        // Agents are always created, but are only useable after they are activated.
        this._active = false;

        // Commands are stored directly on the Agent instance using their unqualified
        // method name as the property. Thus, callers can write: FooAgent.methodName().
        // Enums are stored similarly based on the unqualified type name.
        this._events = {};
    }

    // Public

    get domainName()
    {
        return this._domainName;
    }

    get active()
    {
        return this._active;
    }

    set dispatcher(value)
    {
        this._dispatcher = value;
    }

    addEnum(enumName, enumValues)
    {
        this[enumName] = enumValues;
    }

    addCommand(command)
    {
        this[command.commandName] = command;
    }

    addEvent(event)
    {
        this._events[event.eventName] = event;
    }

    getEvent(eventName)
    {
        return this._events[eventName];
    }

    hasEvent(eventName)
    {
        return eventName in this._events;
    }

    activate()
    {
        this._active = true;
        window[this._domainName + "Agent"] = this;
    }

    dispatchEvent(eventName, eventArguments)
    {
        if (!(eventName in this._dispatcher)) {
            console.error("Protocol Error: Attempted to dispatch an unimplemented method '" + this._domainName + "." + eventName + "'");
            return false;
        }

        this._dispatcher[eventName].apply(this._dispatcher, eventArguments);
        return true;
    }
};

// InspectorBackend.Command can't use ES6 classes because of its trampoline nature.
// But we can use strict mode to get stricter handling of the code inside its functions.
InspectorBackend.Command = function(backend, qualifiedName, callSignature, replySignature)
{
    'use strict';

    this._backend = backend;
    this._instance = this;

    var [domainName, commandName] = qualifiedName.split(".");
    this._qualifiedName = qualifiedName;
    this._commandName = commandName;
    this._callSignature = callSignature || [];
    this._replySignature = replySignature || [];
};

InspectorBackend.Command.create = function(backend, commandName, callSignature, replySignature)
{
    'use strict';

    var instance = new InspectorBackend.Command(backend, commandName, callSignature, replySignature);

    function callable() {
        // If the last argument to the command is not a function, return a result promise.
        if (!arguments.length || typeof arguments[arguments.length - 1] !== "function")
            return instance.promise.apply(instance, arguments);
        return instance._invokeWithArguments.apply(instance, arguments);
    }

    callable._instance = instance;
    Object.setPrototypeOf(callable, InspectorBackend.Command.prototype);

    return callable;
};

// As part of the workaround to make commands callable, these functions use |this._instance|.
// |this| could refer to the callable trampoline, or the InspectorBackend.Command instance.
InspectorBackend.Command.prototype = {
    __proto__: Function.prototype,

    // Public

    get qualifiedName()
    {
        return this._instance._qualifiedName;
    },

    get commandName()
    {
        return this._instance._commandName;
    },

    get callSignature()
    {
        return this._instance._callSignature;
    },

    get replySignature()
    {
        return this._instance._replySignature;
    },

    invoke: function(commandArguments, callback)
    {
        'use strict';

        var instance = this._instance;
        instance._backend._invokeCommand(instance, commandArguments, callback);
    },

    promise: function()
    {
        'use strict';

        var instance = this._instance;
        var promiseArguments = Array.from(arguments);
        return new Promise(function(resolve, reject) {
            function convertToPromiseCallback(error, payload) {
                return error ? reject(new Error(error)) : resolve(payload);
            }

            // FIXME: this should be indicated by invoking the command differently, rather
            // than by setting a magical property on the callback. <webkit.org/b/132386>
            convertToPromiseCallback.expectsResultObject = true;

            promiseArguments.push(convertToPromiseCallback);
            instance._invokeWithArguments.apply(instance, promiseArguments);
        });
    },

    supports: function(parameterName)
    {
        'use strict';

        var instance = this._instance;
        return instance.callSignature.some(function(parameter) {
            return parameter["name"] === parameterName;
        });
    },

    // Private

    _invokeWithArguments: function()
    {
        'use strict';

        var instance = this._instance;
        var commandArguments = Array.from(arguments);
        var callback = typeof commandArguments.lastValue === "function" ? commandArguments.pop() : null;

        var parameters = {};
        for (var i = 0; i < instance.callSignature.length; ++i) {
            var parameter = instance.callSignature[i];
            var parameterName = parameter["name"];
            var typeName = parameter["type"];
            var optionalFlag = parameter["optional"];

            if (!commandArguments.length && !optionalFlag) {
                console.error("Protocol Error: Invalid number of arguments for method '" + instance.qualifiedName + "' call. It must have the following arguments '" + JSON.stringify(instance.callSignature) + "'.");
                return;
            }

            var value = commandArguments.shift();
            if (optionalFlag && value === undefined)
                continue;

            if (typeof value !== typeName) {
                console.error("Protocol Error: Invalid type of argument '" + parameterName + "' for method '" + instance.qualifiedName + "' call. It must be '" + typeName + "' but it is '" + typeof value + "'.");
                return;
            }

            parameters[parameterName] = value;
        }

        if (commandArguments.length === 1 && !callback) {
            if (commandArguments[0] !== undefined) {
                console.error("Protocol Error: Optional callback argument for method '" + instance.qualifiedName + "' call must be a function but its type is '" + typeof args[0] + "'.");
                return;
            }
        }

        instance._backend._invokeCommand(instance, Object.keys(parameters).length ? parameters : null, callback);
    }
};

InspectorBackend.Event = class Event
{
    constructor(eventName, parameterNames)
    {
        this.eventName = eventName;
        this.parameterNames = parameterNames;
    }
};
