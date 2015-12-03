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
        this._pendingResponses = new Map;
        this._agents = {};
        this._deferredScripts = [];

        this.dumpInspectorTimeStats = false;
        this.dumpInspectorProtocolMessages = false;
        this.warnForLongMessageHandling = false;
        this.longMessageHandlingThreshold = 10; // milliseconds.

        this._log = window.InspectorTest ? InspectorFrontendHost.unbufferedLog.bind(InspectorFrontendHost) : console.log.bind(console);
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
            this._log("backend: " + ((typeof message === "string") ? message : JSON.stringify(message)));

        var messageObject = (typeof message === "string") ? JSON.parse(message) : message;

        if ("id" in messageObject)
            this._dispatchResponse(messageObject);
        else
            this._dispatchEvent(messageObject);
    }

    runAfterPendingDispatches(script)
    {
        console.assert(script);
        console.assert(typeof script === "function");

        if (!this._pendingResponses.size)
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

    _sendCommandToBackendWithCallback(command, parameters, callback)
    {
        var sequenceId = this._lastSequenceId++;

        var messageObject = {
            "id": sequenceId,
            "method": command.qualifiedName,
        };

        if (Object.keys(parameters).length)
            messageObject["params"] = parameters;

        var responseData = {command, callback};

        if (this.dumpInspectorTimeStats)
            responseData.sendRequestTimestamp = timestamp();

        this._pendingResponses.set(sequenceId, responseData);
        this._sendMessageToBackend(messageObject);
    }

    _sendCommandToBackendExpectingPromise(command, parameters)
    {
        var sequenceId = this._lastSequenceId++;

        var messageObject = {
            "id": sequenceId,
            "method": command.qualifiedName,
        };

        if (Object.keys(parameters).length)
            messageObject["params"] = parameters;

        var responseData = {command};

        if (this.dumpInspectorTimeStats)
            responseData.sendRequestTimestamp = timestamp();

        var responsePromise = new Promise(function(resolve, reject) {
            responseData.promise = {resolve, reject};
        });

        this._pendingResponses.set(sequenceId, responseData);
        this._sendMessageToBackend(messageObject);

        return responsePromise;
    }

    _sendMessageToBackend(messageObject)
    {
        var stringifiedMessage = JSON.stringify(messageObject);
        if (this.dumpInspectorProtocolMessages)
            this._log("frontend: " + stringifiedMessage);

        InspectorFrontendHost.sendMessageToBackend(stringifiedMessage);
    }

    _dispatchResponse(messageObject)
    {
        console.assert(this._pendingResponses.size >= 0);

        if (messageObject["error"]) {
            if (messageObject["error"].code !== -32000)
                this._reportProtocolError(messageObject);
        }

        var sequenceId = messageObject["id"];
        console.assert(this._pendingResponses.has(sequenceId), sequenceId, this._pendingResponses);

        var responseData = this._pendingResponses.take(sequenceId);
        var {command, callback, promise} = responseData;

        var processingStartTimestamp;
        if (this.dumpInspectorTimeStats)
            processingStartTimestamp = timestamp();

        if (typeof callback === "function")
            this._dispatchResponseToCallback(command, messageObject, callback);
        else if (typeof promise === "object")
            this._dispatchResponseToPromise(command, messageObject, promise);
        else
            console.error("Received a command response without a corresponding callback or promise.", messageObject, command);

        var processingDuration = (timestamp() - processingStartTimestamp).toFixed(3);
        if (this.warnForLongMessageHandling && processingDuration > this.longMessageHandlingThreshold)
            console.warn(`InspectorBackend: took ${processingDuration}ms to handle response for command: ${command.qualifiedName}`);

        if (this.dumpInspectorTimeStats) {
            var roundTripDuration = (processingStartTimestamp - responseData.sendRequestTimestamp).toFixed(3);
            console.log(`time-stats: Handling: ${processingDuration}ms; RTT: ${roundTripDuration}ms; (command ${command.qualifiedName})`);
        }

        if (this._deferredScripts.length && !this._pendingResponses.size)
            this._flushPendingScripts();
    }

    _dispatchResponseToCallback(command, messageObject, callback)
    {
        var callbackArguments = [];
        callbackArguments.push(messageObject["error"] ? messageObject["error"].message : null);

        if (messageObject["result"]) {
            for (var parameterName of command.replySignature)
                callbackArguments.push(messageObject["result"][parameterName]);
        }

        try {
            callback.apply(null, callbackArguments);
        } catch (e) {
            console.error("Uncaught exception in inspector page while dispatching callback for command " + command.qualifiedName, e);
        }
    }

    _dispatchResponseToPromise(command, messageObject, promise)
    {
        var {resolve, reject} = promise;
        if (messageObject["error"])
            reject(new Error(messageObject["error"].message));
        else
            resolve(messageObject["result"]);
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

        var processingStartTimestamp;
        if (this.dumpInspectorTimeStats)
            processingStartTimestamp = timestamp();

        try {
            agent.dispatchEvent(eventName, eventArguments);
        } catch (e) {
            console.error("Uncaught exception in inspector page while handling event " + qualifiedName, e);
        }

        var processingDuration = (timestamp() - processingStartTimestamp).toFixed(3);
        if (this.warnForLongMessageHandling && processingDuration > this.longMessageHandlingThreshold)
            console.warn(`InspectorBackend: took ${processingDuration}ms to handle event: ${messageObject.method}`);

        if (this.dumpInspectorTimeStats)
            console.log(`time-stats: Handling: ${processingDuration}ms (event ${messageObject.method})`);
    }

    _reportProtocolError(messageObject)
    {
        console.error("Request with id = " + messageObject["id"] + " failed. " + JSON.stringify(messageObject["error"]));
    }

    _flushPendingScripts()
    {
        console.assert(this._pendingResponses.size === 0);

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

        if (typeof callback === "function")
            instance._backend._sendCommandToBackendWithCallback(instance, commandArguments, callback);
        else
            return instance._backend._sendCommandToBackendExpectingPromise(instance, commandArguments);
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

        function deliverFailure(message) {
            console.error(`Protocol Error: ${message}`);
            if (callback)
                setTimeout(callback.bind(null, message), 0);
            else
                return Promise.reject(new Error(message));
        }

        var parameters = {};
        for (var parameter of instance.callSignature) {
            var parameterName = parameter["name"];
            var typeName = parameter["type"];
            var optionalFlag = parameter["optional"];

            if (!commandArguments.length && !optionalFlag)
                return deliverFailure(`Invalid number of arguments for command '${instance.qualifiedName}'.`);

            var value = commandArguments.shift();
            if (optionalFlag && value === undefined)
                continue;

            if (typeof value !== typeName)
                return deliverFailure(`Invalid type of argument '${parameterName}' for command '${instance.qualifiedName}' call. It must be '${typeName}' but it is '${typeof value}'.`);

            parameters[parameterName] = value;
        }

        if (!callback && commandArguments.length === 1 && commandArguments[0] !== undefined)
            return deliverFailure(`Protocol Error: Optional callback argument for command '${instance.qualifiedName}' call must be a function but its type is '${typeof args[0]}'.`);

        if (callback)
            instance._backend._sendCommandToBackendWithCallback(instance, parameters, callback);
        else
            return instance._backend._sendCommandToBackendExpectingPromise(instance, parameters);
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
