/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WI.Target = class Target extends WI.Object
{
    constructor(identifier, name, type, connection)
    {
        super();

        this._identifier = identifier;
        this._name = name;
        this._type = type;
        this._connection = connection;
        this._executionContext = null;
        this._mainResource = null;
        this._resourceCollection = new WI.ResourceCollection;
        this._extraScriptCollection = new WI.ScriptCollection;

        // Restrict the agents to the list of supported agents for this target type.
        // This makes it so `target.FooAgent` only exists if the "Foo" domain is
        // supported by the target.
        this._agents = {};
        const supportedDomains = this._supportedDomainsForTargetType(this._type);
        for (let domain of supportedDomains)
            this._agents[domain] = this._connection._agents[domain];

        this._connection.target = this;

        // Agents we always expect in every target.
        console.assert(this.RuntimeAgent);
        console.assert(this.DebuggerAgent);
        console.assert(this.ConsoleAgent);
    }

    // Target

    initialize()
    {
        // Intentionally initialize InspectorAgent first if it is available.
        // This may not be strictly necessary anymore, but is historical.
        if (this.InspectorAgent)
            this.InspectorAgent.enable();

        // Initialize agents.
        for (let manager of WI.managers) {
            if (manager.initializeTarget)
                manager.initializeTarget(this);
        }

        // Non-manager specific initialization.
        // COMPATIBILITY (iOS 8): Page.setShowPaintRects did not exist.
        if (this.PageAgent) {
            if (this.PageAgent.setShowPaintRects && WI.settings.showPaintRects.value)
                this.PageAgent.setShowPaintRects(true);
        }

        // Intentionally defer ConsoleAgent initialization to the end. We do this so that any
        // previous initialization messages will have their responses arrive before a stream
        // of console message added events come in after enabling Console.
        this.ConsoleAgent.enable();

        setTimeout(() => {
            // Use this opportunity to run any one time frontend initialization
            // now that we have a target with potentially new capabilities.
            WI.performOneTimeFrontendInitializationsUsingTarget(this);
        });

        setTimeout(() => {
            // Tell the backend we are initialized after all our initialization messages have been sent.
            // This allows an automatically paused backend to resume execution, but we want to ensure
            // our breakpoints were already sent to that backend.
            // COMPATIBILITY (iOS 8): Inspector.initialized did not exist yet.
            if (this.InspectorAgent && this.InspectorAgent.initialized)
                this.InspectorAgent.initialized();
        });
    }

    // Agents

    get ApplicationCacheAgent() { return this._agents.ApplicationCache; }
    get CSSAgent() { return this._agents.CSS; }
    get CanvasAgent() { return this._agents.Canvas; }
    get ConsoleAgent() { return this._agents.Console; }
    get DOMAgent() { return this._agents.DOM; }
    get DOMDebuggerAgent() { return this._agents.DOMDebugger; }
    get DOMStorageAgent() { return this._agents.DOMStorage; }
    get DatabaseAgent() { return this._agents.Database; }
    get DebuggerAgent() { return this._agents.Debugger; }
    get HeapAgent() { return this._agents.Heap; }
    get IndexedDBAgent() { return this._agents.IndexedDB; }
    get InspectorAgent() { return this._agents.Inspector; }
    get LayerTreeAgent() { return this._agents.LayerTree; }
    get MemoryAgent() { return this._agents.Memory; }
    get NetworkAgent() { return this._agents.Network; }
    get PageAgent() { return this._agents.Page; }
    get RecordingAgent() { return this._agents.Recording; }
    get RuntimeAgent() { return this._agents.Runtime; }
    get ScriptProfilerAgent() { return this._agents.ScriptProfiler; }
    get ServiceWorkerAgent() { return this._agents.ServiceWorker; }
    get TargetAgent() { return this._agents.Target; }
    get TimelineAgent() { return this._agents.Timeline; }
    get WorkerAgent() { return this._agents.Worker; }

    // Public

    get identifier() { return this._identifier; }
    set identifier(identifier) { this._identifier = identifier; }

    get name() { return this._name; }
    set name(name) { this._name = name; }

    get type() { return this._type; }
    get connection() { return this._connection; }
    get executionContext() { return this._executionContext; }

    get resourceCollection() { return this._resourceCollection; }
    get extraScriptCollection() { return this._extraScriptCollection; }

    get displayName() { return this._name; }

    get mainResource()
    {
        return this._mainResource;
    }

    set mainResource(resource)
    {
        console.assert(!this._mainResource);

        this._mainResource = resource;

        this.dispatchEventToListeners(WI.Target.Event.MainResourceAdded, {resource});
    }

    addResource(resource)
    {
        this._resourceCollection.add(resource);

        this.dispatchEventToListeners(WI.Target.Event.ResourceAdded, {resource});
    }

    adoptResource(resource)
    {
        resource._target = this;

        this.addResource(resource);
    }

    addScript(script)
    {
        this._extraScriptCollection.add(script);

        this.dispatchEventToListeners(WI.Target.Event.ScriptAdded, {script});
    }

    // Private

    _supportedDomainsForTargetType(type)
    {
        switch (type) {
        case WI.Target.Type.JSContext:
            return InspectorBackend.supportedDomainsForDebuggableType(WI.DebuggableType.JavaScript);
        case WI.Target.Type.Worker:
            return InspectorBackend.supportedDomainsForDebuggableType(WI.DebuggableType.Worker);
        case WI.Target.Type.ServiceWorker:
            return InspectorBackend.supportedDomainsForDebuggableType(WI.DebuggableType.ServiceWorker);
        case WI.Target.Type.Page:
            return InspectorBackend.supportedDomainsForDebuggableType(WI.DebuggableType.Web);
        default:
            console.assert(false, "Unexpected target type", type);
            return InspectorBackend.supportedDomainsForDebuggableType(WI.DebuggableType.Web);
        }
    }
};

WI.Target.Type = {
    Page: Symbol("page"),
    JSContext: Symbol("jscontext"),
    ServiceWorker: Symbol("service-worker"),
    Worker: Symbol("worker"),
};

WI.Target.Event = {
    MainResourceAdded: "target-main-resource-added",
    ResourceAdded: "target-resource-added",
    ScriptAdded: "target-script-added",
};
