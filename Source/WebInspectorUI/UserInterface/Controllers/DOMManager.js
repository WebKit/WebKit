/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

// FIXME: DOMManager lacks advanced multi-target support. (DOMNodes per-target)

WI.DOMManager = class DOMManager extends WI.Object
{
    constructor()
    {
        super();

        this._idToDOMNode = {};
        this._document = null;
        this._attributeLoadNodeIds = {};
        this._restoreSelectedNodeIsAllowed = true;
        this._loadNodeAttributesTimeout = 0;
        this._inspectedNode = null;

        this._breakpointsForEventListeners = new Map;

        this._hasRequestedDocument = false;
        this._pendingDocumentRequestCallbacks = null;

        WI.EventBreakpoint.addEventListener(WI.EventBreakpoint.Event.DisabledStateChanged, this._handleEventBreakpointDisabledStateChanged, this);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    // Target

    initializeTarget(target)
    {
        // FIXME: This should be improved when adding better DOM multi-target support since it is really per-target.
        // This currently uses a setTimeout since it doesn't need to happen immediately, and DOMManager uses the
        // global DOMAgent to request the document, so we want to make sure we've transitioned the global agents
        // to this target if necessary.
        if (target.hasDomain("DOM")) {
            setTimeout(() => {
                this.ensureDocument();
            });

            if (WI.isEngineeringBuild) {
                if (DOMManager.supportsEditingUserAgentShadowTrees({target}))
                    target.DOMAgent.setAllowEditingUserAgentShadowTrees(WI.settings.engineeringAllowEditingUserAgentShadowTrees.value);
            }
        }
    }

    transitionPageTarget()
    {
        this._documentUpdated();
    }

    // Static

    static buildHighlightConfig(mode)
    {
        mode = mode || "all";

        let highlightConfig = {showInfo: mode === "all"};

        if (mode === "all" || mode === "content")
            highlightConfig.contentColor = {r: 111, g: 168, b: 220, a: 0.66};

        if (mode === "all" || mode === "padding")
            highlightConfig.paddingColor = {r: 147, g: 196, b: 125, a: 0.66};

        if (mode === "all" || mode === "border")
            highlightConfig.borderColor = {r: 255, g: 229, b: 153, a: 0.66};

        if (mode === "all" || mode === "margin")
            highlightConfig.marginColor = {r: 246, g: 178, b: 107, a: 0.66};

        return highlightConfig;
    }

    static wrapClientCallback(callback)
    {
        if (!callback)
            return null;

        return function(error, result) {
            if (error)
                console.error("Error during DOMAgent operation: " + error);
            callback(error ? null : result);
        };
    }

    static supportsDisablingEventListeners()
    {
        return InspectorBackend.hasCommand("DOM.setEventListenerDisabled");
    }

    static supportsEventListenerBreakpoints()
    {
        return InspectorBackend.hasCommand("DOM.setBreakpointForEventListener")
            && InspectorBackend.hasCommand("DOM.removeBreakpointForEventListener");
    }

    static supportsEditingUserAgentShadowTrees({frontendOnly, target} = {})
    {
        target = target || InspectorBackend;
        return WI.settings.engineeringAllowEditingUserAgentShadowTrees.value
            && (frontendOnly || target.hasCommand("DOM.setAllowEditingUserAgentShadowTrees"));

    }

    // Public

    get inspectedNode() { return this._inspectedNode; }

    get eventListenerBreakpoints()
    {
        return Array.from(this._breakpointsForEventListeners.values());
    }

    requestDocument(callback)
    {
        if (this._document) {
            callback(this._document);
            return;
        }

        if (this._pendingDocumentRequestCallbacks)
            this._pendingDocumentRequestCallbacks.push(callback);
        else
            this._pendingDocumentRequestCallbacks = [callback];

        if (this._hasRequestedDocument)
            return;

        if (!WI.pageTarget)
            return;

        if (!WI.pageTarget.hasDomain("DOM"))
            return;

        this._hasRequestedDocument = true;

        WI.pageTarget.DOMAgent.getDocument((error, root) => {
            if (!error)
                this._setDocument(root);

            for (let callback of this._pendingDocumentRequestCallbacks)
                callback(this._document);

            this._pendingDocumentRequestCallbacks = null;
        });
    }

    ensureDocument()
    {
        this.requestDocument(function(){});
    }

    pushNodeToFrontend(objectId, callback)
    {
        let target = WI.assumingMainTarget();
        this._dispatchWhenDocumentAvailable((callbackWrapper) => {
            target.DOMAgent.requestNode(objectId, callbackWrapper);
        }, callback);
    }

    pushNodeByPathToFrontend(path, callback)
    {
        let target = WI.assumingMainTarget();
        this._dispatchWhenDocumentAvailable((callbackWrapper) => {
            target.DOMAgent.pushNodeByPathToFrontend(path, callbackWrapper);
        }, callback);
    }

    // DOMObserver

    didAddEventListener(nodeId)
    {
        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.dispatchEventToListeners(WI.DOMNode.Event.EventListenersChanged);
    }

    willRemoveEventListener(nodeId)
    {
        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.dispatchEventToListeners(WI.DOMNode.Event.EventListenersChanged);
    }

    didFireEvent(nodeId, eventName, timestamp, data)
    {
        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.didFireEvent(eventName, timestamp, data);
    }

    powerEfficientPlaybackStateChanged(nodeId, timestamp, isPowerEfficient)
    {
        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.powerEfficientPlaybackStateChanged(timestamp, isPowerEfficient);
    }

    // Private

    _dispatchWhenDocumentAvailable(func, callback)
    {
        var callbackWrapper = DOMManager.wrapClientCallback(callback);

        function onDocumentAvailable()
        {
            if (this._document)
                func(callbackWrapper);
            else {
                if (callbackWrapper)
                    callbackWrapper("No document");
            }
        }
        this.requestDocument(onDocumentAvailable.bind(this));
    }

    _attributeModified(nodeId, name, value)
    {
        var node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node._setAttribute(name, value);
        this.dispatchEventToListeners(WI.DOMManager.Event.AttributeModified, {node, name});
        node.dispatchEventToListeners(WI.DOMNode.Event.AttributeModified, {name});
    }

    _attributeRemoved(nodeId, name)
    {
        var node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node._removeAttribute(name);
        this.dispatchEventToListeners(WI.DOMManager.Event.AttributeRemoved, {node, name});
        node.dispatchEventToListeners(WI.DOMNode.Event.AttributeRemoved, {name});
    }

    _inlineStyleInvalidated(nodeIds)
    {
        for (var nodeId of nodeIds)
            this._attributeLoadNodeIds[nodeId] = true;
        if (this._loadNodeAttributesTimeout)
            return;
        this._loadNodeAttributesTimeout = setTimeout(this._loadNodeAttributes.bind(this), 0);
    }

    _loadNodeAttributes()
    {
        function callback(nodeId, error, attributes)
        {
            if (error) {
                console.error("Error during DOMAgent operation: " + error);
                return;
            }
            var node = this._idToDOMNode[nodeId];
            if (node) {
                node._setAttributesPayload(attributes);
                this.dispatchEventToListeners(WI.DOMManager.Event.AttributeModified, {node, name: "style"});
                node.dispatchEventToListeners(WI.DOMNode.Event.AttributeModified, {name: "style"});
            }
        }

        this._loadNodeAttributesTimeout = 0;

        let target = WI.assumingMainTarget();

        for (var nodeId in this._attributeLoadNodeIds) {
            if (!(nodeId in this._idToDOMNode))
                continue;
            var nodeIdAsNumber = parseInt(nodeId);
            target.DOMAgent.getAttributes(nodeIdAsNumber, callback.bind(this, nodeIdAsNumber));
        }
        this._attributeLoadNodeIds = {};
    }

    _characterDataModified(nodeId, newValue)
    {
        var node = this._idToDOMNode[nodeId];
        node._nodeValue = newValue;
        this.dispatchEventToListeners(WI.DOMManager.Event.CharacterDataModified, {node});
    }

    nodeForId(nodeId)
    {
        return this._idToDOMNode[nodeId] || null;
    }

    _documentUpdated()
    {
        this._setDocument(null);
    }

    _setDocument(payload)
    {
        for (let node of Object.values(this._idToDOMNode))
            node.markDestroyed();

        this._idToDOMNode = {};

        for (let breakpoint of this._breakpointsForEventListeners.values())
            WI.domDebuggerManager.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, {breakpoint});
        this._breakpointsForEventListeners.clear();

        let newDocument = null;
        if (payload && "nodeId" in payload)
            newDocument = new WI.DOMNode(this, null, false, payload);

        if (this._document === newDocument)
            return;

        this._document = newDocument;

        if (!this._document)
            this._hasRequestedDocument = false;

        this.dispatchEventToListeners(WI.DOMManager.Event.DocumentUpdated, {document: this._document});
    }

    _setDetachedRoot(payload)
    {
        new WI.DOMNode(this, null, false, payload);
    }

    _setChildNodes(parentId, payloads)
    {
        if (!parentId && payloads.length) {
            this._setDetachedRoot(payloads[0]);
            return;
        }

        var parent = this._idToDOMNode[parentId];
        parent._setChildrenPayload(payloads);
    }

    _childNodeCountUpdated(nodeId, newValue)
    {
        var node = this._idToDOMNode[nodeId];
        node.childNodeCount = newValue;
        this.dispatchEventToListeners(WI.DOMManager.Event.ChildNodeCountUpdated, node);
    }

    _childNodeInserted(parentId, prevId, payload)
    {
        var parent = this._idToDOMNode[parentId];
        var prev = this._idToDOMNode[prevId];
        var node = parent._insertChild(prev, payload);
        this._idToDOMNode[node.id] = node;
        this.dispatchEventToListeners(WI.DOMManager.Event.NodeInserted, {node, parent});
    }

    _childNodeRemoved(parentId, nodeId)
    {
        var parent = this._idToDOMNode[parentId];
        var node = this._idToDOMNode[nodeId];
        parent._removeChild(node);
        this._unbind(node);
        this.dispatchEventToListeners(WI.DOMManager.Event.NodeRemoved, {node, parent});
    }

    _customElementStateChanged(elementId, newState)
    {
        const node = this._idToDOMNode[elementId];
        node._customElementState = newState;
        this.dispatchEventToListeners(WI.DOMManager.Event.CustomElementStateChanged, {node});
    }

    _pseudoElementAdded(parentId, pseudoElement)
    {
        var parent = this._idToDOMNode[parentId];
        if (!parent)
            return;

        var node = new WI.DOMNode(this, parent.ownerDocument, false, pseudoElement);
        node.parentNode = parent;
        this._idToDOMNode[node.id] = node;
        console.assert(!parent.pseudoElements().get(node.pseudoType()));
        parent.pseudoElements().set(node.pseudoType(), node);
        this.dispatchEventToListeners(WI.DOMManager.Event.NodeInserted, {node, parent});
    }

    _pseudoElementRemoved(parentId, pseudoElementId)
    {
        var pseudoElement = this._idToDOMNode[pseudoElementId];
        if (!pseudoElement)
            return;

        var parent = pseudoElement.parentNode;
        console.assert(parent);
        console.assert(parent.id === parentId);
        if (!parent)
            return;

        parent._removeChild(pseudoElement);
        this._unbind(pseudoElement);
        this.dispatchEventToListeners(WI.DOMManager.Event.NodeRemoved, {node: pseudoElement, parent});
    }

    _unbind(node)
    {
        node.markDestroyed();

        delete this._idToDOMNode[node.id];

        for (let i = 0; node.children && i < node.children.length; ++i)
            this._unbind(node.children[i]);

        let templateContent = node.templateContent();
        if (templateContent)
            this._unbind(templateContent);

        for (let pseudoElement of node.pseudoElements().values())
            this._unbind(pseudoElement);

        // FIXME: Handle shadow roots.
    }

    get restoreSelectedNodeIsAllowed()
    {
        return this._restoreSelectedNodeIsAllowed;
    }

    inspectElement(nodeId, options = {})
    {
        var node = this._idToDOMNode[nodeId];
        if (!node || !node.ownerDocument)
            return;

        // This code path is hit by "Reveal in DOM Tree" and clicking element links/console widgets.
        // Unless overridden by callers, assume that this is navigation is initiated by a Inspect mode.
        let initiatorHint = options.initiatorHint || WI.TabBrowser.TabNavigationInitiator.Inspect;
        this.dispatchEventToListeners(WI.DOMManager.Event.DOMNodeWasInspected, {node, initiatorHint});

        this._inspectModeEnabled = false;
        this.dispatchEventToListeners(WI.DOMManager.Event.InspectModeStateChanged);
    }

    inspectNodeObject(remoteObject)
    {
        this._restoreSelectedNodeIsAllowed = false;

        function nodeAvailable(nodeId)
        {
            remoteObject.release();

            console.assert(nodeId);
            if (!nodeId)
                return;

            this.inspectElement(nodeId);

            // Re-resolve the node in the console's object group when adding to the console.
            let domNode = this.nodeForId(nodeId);
            WI.RemoteObject.resolveNode(domNode, WI.RuntimeManager.ConsoleObjectGroup).then((remoteObject) => {
                const specialLogStyles = true;
                const shouldRevealConsole = false;
                WI.consoleLogViewController.appendImmediateExecutionWithResult(WI.UIString("Selected Element"), remoteObject, specialLogStyles, shouldRevealConsole);
            });
        }

        remoteObject.pushNodeToFrontend(nodeAvailable.bind(this));
    }

    highlightDOMNodeList(nodes, mode)
    {
        let target = WI.assumingMainTarget();

        // COMPATIBILITY (iOS 11): DOM.highlightNodeList did not exist.
        if (!target.hasCommand("DOM.highlightNodeList"))
            return;

        if (this._hideDOMNodeHighlightTimeout) {
            clearTimeout(this._hideDOMNodeHighlightTimeout);
            this._hideDOMNodeHighlightTimeout = undefined;
        }

        let nodeIds = [];
        for (let node of nodes) {
            console.assert(node instanceof WI.DOMNode, node);
            console.assert(!node.destroyed, node);
            if (node.destroyed)
                continue;
            nodeIds.push(node.id);
        }

        target.DOMAgent.highlightNodeList(nodeIds, DOMManager.buildHighlightConfig(mode));
    }

    highlightSelector(selectorText, frameId, mode)
    {
        if (this._hideDOMNodeHighlightTimeout) {
            clearTimeout(this._hideDOMNodeHighlightTimeout);
            this._hideDOMNodeHighlightTimeout = undefined;
        }

        let target = WI.assumingMainTarget();
        target.DOMAgent.highlightSelector(DOMManager.buildHighlightConfig(mode), selectorText, frameId);
    }

    highlightRect(rect, usePageCoordinates)
    {
        let target = WI.assumingMainTarget();
        target.DOMAgent.highlightRect.invoke({
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height,
            color: {r: 111, g: 168, b: 220, a: 0.66},
            outlineColor: {r: 255, g: 229, b: 153, a: 0.66},
            usePageCoordinates
        });
    }

    hideDOMNodeHighlight()
    {
        let target = WI.assumingMainTarget();
        target.DOMAgent.hideHighlight();
    }

    highlightDOMNodeForTwoSeconds(nodeId)
    {
        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.highlight();

        this._hideDOMNodeHighlightTimeout = setTimeout(this.hideDOMNodeHighlight.bind(this), 2000);
    }

    get inspectModeEnabled()
    {
        return this._inspectModeEnabled;
    }

    set inspectModeEnabled(enabled)
    {
        if (enabled === this._inspectModeEnabled)
            return;

        let target = WI.assumingMainTarget();
        let commandArguments = {
            enabled,
            highlightConfig: DOMManager.buildHighlightConfig(),
            showRulers: WI.settings.showRulersDuringElementSelection.value,
        };
        target.DOMAgent.setInspectModeEnabled.invoke(commandArguments, (error) => {
            if (error) {
                WI.reportInternalError(error);
                return;
            }

            this._inspectModeEnabled = enabled;
            this.dispatchEventToListeners(WI.DOMManager.Event.InspectModeStateChanged);
        });
    }

    setInspectedNode(node)
    {
        console.assert(node instanceof WI.DOMNode);
        if (node === this._inspectedNode)
            return;

        console.assert(!node.destroyed, node);
        if (node.destroyed)
            return;

        let callback = (error) => {
            console.assert(!error, error);
            if (error)
                return;

            let lastInspectedNode = this._inspectedNode;
            this._inspectedNode = node;

            this.dispatchEventToListeners(WI.DOMManager.Event.InspectedNodeChanged, {lastInspectedNode});
        };

        let target = WI.assumingMainTarget();

        // COMPATIBILITY (iOS 11): DOM.setInspectedNode did not exist.
        if (!target.hasCommand("DOM.setInspectedNode")) {
            if (target.hasCommand("Console.addInspectedNode"))
                target.ConsoleAgent.addInspectedNode(node.id, callback);
            return;
        }

        target.DOMAgent.setInspectedNode(node.id, callback);
    }

    getSupportedEventNames(callback)
    {
        let target = WI.assumingMainTarget();
        if (!target.hasCommand("DOM.getSupportedEventNames"))
            return Promise.resolve(new Set);

        if (!this._getSupportedEventNamesPromise) {
            this._getSupportedEventNamesPromise = target.DOMAgent.getSupportedEventNames()
            .then(({eventNames}) => new Set(eventNames));
        }

        return this._getSupportedEventNamesPromise;
    }

    setEventListenerDisabled(eventListener, disabled)
    {
        let target = WI.assumingMainTarget();
        target.DOMAgent.setEventListenerDisabled(eventListener.eventListenerId, disabled);
    }

    setBreakpointForEventListener(eventListener)
    {
        let breakpoint = this._breakpointsForEventListeners.get(eventListener.eventListenerId);
        if (breakpoint) {
            console.assert(breakpoint.disabled);
            breakpoint.disabled = false;
            return;
        }

        breakpoint = new WI.EventBreakpoint(WI.EventBreakpoint.Type.Listener, {eventName: eventListener.type, eventListener});
        console.assert(!breakpoint.disabled);

        this._breakpointsForEventListeners.set(eventListener.eventListenerId, breakpoint);

        for (let target of WI.targets) {
            if (target.hasDomain("DOM"))
                this._updateEventBreakpoint(breakpoint, target);
        }

        WI.domDebuggerManager.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointAdded, {breakpoint});
    }

    removeBreakpointForEventListener(eventListener)
    {
        let breakpoint = this._breakpointsForEventListeners.take(eventListener.eventListenerId);
        console.assert(breakpoint);

        for (let target of WI.targets) {
            if (target.hasDomain("DOM"))
                target.DOMAgent.removeBreakpointForEventListener(eventListener.eventListenerId);
        }

        WI.domDebuggerManager.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, {breakpoint});
    }

    removeEventListenerBreakpointsForNode(domNode)
    {
        for (let breakpoint of Array.from(this._breakpointsForEventListeners.values())) {
            let eventListener = breakpoint.eventListener;
            if (eventListener.nodeId === domNode.id)
                this.removeBreakpointForEventListener(eventListener);
        }
    }

    breakpointForEventListenerId(eventListenerId)
    {
        return this._breakpointsForEventListeners.get(eventListenerId) || null;
    }

    // Private

    _updateEventBreakpoint(breakpoint, target)
    {
        let eventListener = breakpoint.eventListener;
        console.assert(eventListener);

        if (breakpoint.disabled)
            target.DOMAgent.removeBreakpointForEventListener(eventListener.eventListenerId);
        else {
            if (!WI.debuggerManager.breakpointsDisabledTemporarily)
                WI.debuggerManager.breakpointsEnabled = true;

            target.DOMAgent.setBreakpointForEventListener(eventListener.eventListenerId);
        }
    }

    _handleEventBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;

        // Non-specific event listener breakpoints are handled by `DOMDebuggerManager`.
        if (!breakpoint.eventListener)
            return;

        for (let target of WI.targets) {
            if (target.hasDomain("DOM"))
                this._updateEventBreakpoint(breakpoint, target);
        }
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._restoreSelectedNodeIsAllowed = true;

        this.ensureDocument();
    }
};

WI.DOMManager.Event = {
    AttributeModified: "dom-manager-attribute-modified",
    AttributeRemoved: "dom-manager-attribute-removed",
    CharacterDataModified: "dom-manager-character-data-modified",
    NodeInserted: "dom-manager-node-inserted",
    NodeRemoved: "dom-manager-node-removed",
    CustomElementStateChanged: "dom-manager-custom-element-state-changed",
    DocumentUpdated: "dom-manager-document-updated",
    ChildNodeCountUpdated: "dom-manager-child-node-count-updated",
    DOMNodeWasInspected: "dom-manager-dom-node-was-inspected",
    InspectModeStateChanged: "dom-manager-inspect-mode-state-changed",
    InspectedNodeChanged: "dom-manager-inspected-node-changed",
};
