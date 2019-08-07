/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2015, 2016 Apple Inc. All rights reserved.
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
        this._agents = {};

        this._customTracer = null;
        this._defaultTracer = new WI.LoggingProtocolTracer;
        this._activeTracers = [this._defaultTracer];

        this._supportedDomainsForDebuggableType = new Map;

        for (let debuggableType of Object.values(WI.DebuggableType))
            this._supportedDomainsForDebuggableType.set(debuggableType, []);

        WI.settings.protocolAutoLogMessages.addEventListener(WI.Setting.Event.Changed, this._startOrStopAutomaticTracing, this);
        WI.settings.protocolAutoLogTimeStats.addEventListener(WI.Setting.Event.Changed, this._startOrStopAutomaticTracing, this);
        this._startOrStopAutomaticTracing();

        this.currentDispatchState = {
            event: null,
            request: null,
            response: null,
        };
    }

    // Public

    // This should be used for feature checking if something exists in the protocol
    // regardless of whether or not the domain is active for a specific target.
    get domains()
    {
        return this._agents;
    }

    // It's still possible to set this flag on InspectorBackend to just
    // dump protocol traffic as it happens. For more complex uses of
    // protocol data, install a subclass of WI.ProtocolTracer.
    set dumpInspectorProtocolMessages(value)
    {
        // Implicitly cause automatic logging to start if it's allowed.
        WI.settings.protocolAutoLogMessages.value = value;

        this._defaultTracer.dumpMessagesToConsole = value;
    }

    get dumpInspectorProtocolMessages()
    {
        return WI.settings.protocolAutoLogMessages.value;
    }

    set dumpInspectorTimeStats(value)
    {
        WI.settings.protocolAutoLogTimeStats.value = value;

        if (!this.dumpInspectorProtocolMessages)
            this.dumpInspectorProtocolMessages = true;

        this._defaultTracer.dumpTimingDataToConsole = value;
    }

    get dumpInspectorTimeStats()
    {
        return WI.settings.protocolAutoLogTimeStats.value;
    }

    set filterMultiplexingBackendInspectorProtocolMessages(value)
    {
        WI.settings.protocolFilterMultiplexingBackendMessages.value = value;

        this._defaultTracer.filterMultiplexingBackend = value;
    }

    get filterMultiplexingBackendInspectorProtocolMessages()
    {
        return WI.settings.protocolFilterMultiplexingBackendMessages.value;
    }

    set customTracer(tracer)
    {
        console.assert(!tracer || tracer instanceof WI.ProtocolTracer, tracer);
        console.assert(!tracer || tracer !== this._defaultTracer, tracer);

        // Bail early if no state change is to be made.
        if (!tracer && !this._customTracer)
            return;

        if (tracer === this._customTracer)
            return;

        if (tracer === this._defaultTracer)
            return;

        if (this._customTracer)
            this._customTracer.logFinished();

        this._customTracer = tracer;
        this._activeTracers = [this._defaultTracer];

        if (this._customTracer) {
            this._customTracer.logStarted();
            this._activeTracers.push(this._customTracer);
        }
    }

    get activeTracers()
    {
        return this._activeTracers;
    }

    registerVersion(domainName, version)
    {
        let agent = this._agentForDomain(domainName);
        agent.VERSION = version;
    }

    registerCommand(qualifiedName, callSignature, replySignature)
    {
        var [domainName, commandName] = qualifiedName.split(".");
        var agent = this._agentForDomain(domainName);
        agent.addCommand(InspectorBackend.Command.create(agent, qualifiedName, callSignature, replySignature));
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
        InspectorBackend.backendConnection.dispatch(message);
    }

    runAfterPendingDispatches(callback)
    {
        if (!WI.mainTarget) {
            callback();
            return;
        }

        // FIXME: Should this respect pending dispatches in all connections?
        WI.mainTarget.connection.runAfterPendingDispatches(callback);
    }

    activateDomain(domainName, activationDebuggableTypes)
    {
        let supportedDebuggableTypes = activationDebuggableTypes || Object.values(WI.DebuggableType);
        for (let debuggableType of supportedDebuggableTypes)
            this._supportedDomainsForDebuggableType.get(debuggableType).push(domainName);

        // FIXME: For proper multi-target support we should eliminate all uses of
        // `window.FooAgent` and `unprefixed FooAgent` in favor of either:
        //   - Per-target: `target.FooAgent`
        //   - Global feature check: `InspectorBackend.domains.Foo`
        if (!activationDebuggableTypes || activationDebuggableTypes.includes(InspectorFrontendHost.debuggableType())) {
            let agent = this._agents[domainName];
            agent.activate();
            return agent;
        }

        return null;
    }

    supportedDomainsForDebuggableType(type)
    {
        console.assert(Object.values(WI.DebuggableType).includes(type), "Unknown debuggable type", type);

        return this._supportedDomainsForDebuggableType.get(type);
    }

    // Private

    _startOrStopAutomaticTracing()
    {
        this._defaultTracer.dumpMessagesToConsole = this.dumpInspectorProtocolMessages;
        this._defaultTracer.dumpTimingDataToConsole = this.dumpTimingDataToConsole;
        this._defaultTracer.filterMultiplexingBackend = this.filterMultiplexingBackendInspectorProtocolMessages;
    }

    _agentForDomain(domainName)
    {
        if (this._agents[domainName])
            return this._agents[domainName];

        var agent = new InspectorBackend.Agent(domainName);
        this._agents[domainName] = agent;
        return agent;
    }
};

InspectorBackend = new InspectorBackendClass;

InspectorBackend.Agent = class InspectorBackendAgent
{
    constructor(domainName)
    {
        this._domainName = domainName;

        // Default connection is the main connection.
        this._connection = InspectorBackend.backendConnection;
        this._dispatcher = null;

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

    get connection()
    {
        return this._connection;
    }

    set connection(connection)
    {
        this._connection = connection;
    }

    get dispatcher()
    {
        return this._dispatcher;
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

    hasEventParameter(eventName, eventParameterName)
    {
        let event = this._events[eventName];
        return event && event.parameterNames.includes(eventParameterName);
    }

    activate()
    {
        this._active = true;
        window[this._domainName + "Agent"] = this;
    }

    dispatchEvent(eventName, eventArguments)
    {
        if (!this._dispatcher) {
            console.error(`No domain dispatcher registered for domain '${this._domainName}', for event '${this._domainName}.${eventName}'`);
            return false;
        }

        if (!(eventName in this._dispatcher)) {
            console.error(`Protocol Error: Attempted to dispatch an unimplemented method '${this._domainName}.${eventName}'`);
            return false;
        }

        this._dispatcher[eventName].apply(this._dispatcher, eventArguments);
        return true;
    }
};

// InspectorBackend.Command can't use ES6 classes because of its trampoline nature.
// But we can use strict mode to get stricter handling of the code inside its functions.
InspectorBackend.Command = function(agent, qualifiedName, callSignature, replySignature)
{
    "use strict";

    this._agent = agent;
    this._instance = this;

    let [domainName, commandName] = qualifiedName.split(".");
    this._qualifiedName = qualifiedName;
    this._commandName = commandName;
    this._callSignature = callSignature || [];
    this._replySignature = replySignature || [];
};

InspectorBackend.Command.create = function(agent, commandName, callSignature, replySignature)
{
    "use strict";

    let instance = new InspectorBackend.Command(agent, commandName, callSignature, replySignature);

    function callable() {
        console.assert(this instanceof InspectorBackend.Agent);
        return instance._invokeWithArguments.call(instance, this, Array.from(arguments));
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

    invoke(commandArguments, callback, agent)
    {
        "use strict";

        let instance = this._instance;

        function deliverFailure(message) {
            console.error(`Protocol Error: ${message}`);
            if (callback)
                setTimeout(callback.bind(null, message), 0);
            else
                return Promise.reject(new Error(message));
        }

        if (typeof commandArguments !== "object")
            return deliverFailure(`invoke expects an object for command arguments but its type is '${typeof commandArguments}'.`);

        let parameters = {};
        for (let {name, type, optional} of instance.callSignature) {
            if (!(name in commandArguments) && !optional)
                return deliverFailure(`Missing argument '${name}' for command '${instance.qualifiedName}'.`);

            let value = commandArguments[name];
            if (optional && value === undefined)
                continue;

            if (typeof value !== type)
                return deliverFailure(`Invalid type of argument '${name}' for command '${instance.qualifiedName}' call. It must be '${type}' but it is '${typeof value}'.`);

            parameters[name] = value;
        }

        agent = agent || instance._agent;
        if (typeof callback === "function")
            agent._connection._sendCommandToBackendWithCallback(instance, parameters, callback);
        else
            return agent._connection._sendCommandToBackendExpectingPromise(instance, parameters);
    },

    supports(parameterName)
    {
        "use strict";

        return this._instance.callSignature.some((parameter) => parameter["name"] === parameterName);
    },

    // Private

    _invokeWithArguments(agent, commandArguments)
    {
        "use strict";

        let instance = this._instance;
        let callback = typeof commandArguments.lastValue === "function" ? commandArguments.pop() : null;

        function deliverFailure(message) {
            console.error(`Protocol Error: ${message}`);
            if (callback)
                setTimeout(callback.bind(null, message), 0);
            else
                return Promise.reject(new Error(message));
        }

        let parameters = {};
        for (let {name, type, optional} of instance.callSignature) {
            if (!commandArguments.length && !optional)
                return deliverFailure(`Invalid number of arguments for command '${instance.qualifiedName}'.`);

            let value = commandArguments.shift();
            if (optional && value === undefined)
                continue;

            if (typeof value !== type)
                return deliverFailure(`Invalid type of argument '${name}' for command '${instance.qualifiedName}' call. It must be '${type}' but it is '${typeof value}'.`);

            parameters[name] = value;
        }

        if (!callback && commandArguments.length === 1 && commandArguments[0] !== undefined)
            return deliverFailure(`Protocol Error: Optional callback argument for command '${instance.qualifiedName}' call must be a function but its type is '${typeof commandArguments[0]}'.`);

        if (callback)
            agent._connection._sendCommandToBackendWithCallback(instance, parameters, callback);
        else
            return agent._connection._sendCommandToBackendExpectingPromise(instance, parameters);
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
