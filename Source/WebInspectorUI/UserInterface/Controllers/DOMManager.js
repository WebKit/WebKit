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
        if (target.DOMAgent) {
            setTimeout(() => {
                this.ensureDocument();
            });
        }
    }

    transitionPageTarget()
    {
        this._documentUpdated();
    }

    // Static

    static supportsDisablingEventListeners()
    {
        return !!(InspectorBackend.domains.DOM && InspectorBackend.domains.DOM.setEventListenerDisabled);
    }

    static supportsEventListenerBreakpoints()
    {
        return !!(InspectorBackend.domains.DOM && InspectorBackend.domains.DOM.setBreakpointForEventListener && InspectorBackend.domains.DOM.removeBreakpointForEventListener);
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

        if (!WI.pageTarget.DOMAgent)
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
        this._dispatchWhenDocumentAvailable(DOMAgent.requestNode.bind(DOMAgent, objectId), callback);
    }

    pushNodeByPathToFrontend(path, callback)
    {
        this._dispatchWhenDocumentAvailable(DOMAgent.pushNodeByPathToFrontend.bind(DOMAgent, path), callback);
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

    _wrapClientCallback(callback)
    {
        if (!callback)
            return null;

        return function(error, result) {
            if (error)
                console.error("Error during DOMAgent operation: " + error);
            callback(error ? null : result);
        };
    }

    _dispatchWhenDocumentAvailable(func, callback)
    {
        var callbackWrapper = this._wrapClientCallback(callback);

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

        for (var nodeId in this._attributeLoadNodeIds) {
            var nodeIdAsNumber = parseInt(nodeId);
            DOMAgent.getAttributes(nodeIdAsNumber, callback.bind(this, nodeIdAsNumber));
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
        return this._idToDOMNode[nodeId];
    }

    _documentUpdated()
    {
        this._setDocument(null);
    }

    _setDocument(payload)
    {
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

    inspectElement(nodeId)
    {
        var node = this._idToDOMNode[nodeId];
        if (!node || !node.ownerDocument)
            return;

        this.dispatchEventToListeners(WI.DOMManager.Event.DOMNodeWasInspected, {node});

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

    querySelector(nodeOrNodeId, selector, callback)
    {
        let nodeId = nodeOrNodeId instanceof WI.DOMNode ? nodeOrNodeId.id : nodeOrNodeId;
        console.assert(typeof nodeId === "number");

        if (typeof callback === "function")
            DOMAgent.querySelector(nodeId, selector, this._wrapClientCallback(callback));
        else
            return DOMAgent.querySelector(nodeId, selector).then(({nodeId}) => nodeId);
    }

    querySelectorAll(nodeOrNodeId, selector, callback)
    {
        let nodeId = nodeOrNodeId instanceof WI.DOMNode ? nodeOrNodeId.id : nodeOrNodeId;
        console.assert(typeof nodeId === "number");

        if (typeof callback === "function")
            DOMAgent.querySelectorAll(nodeId, selector, this._wrapClientCallback(callback));
        else
            return DOMAgent.querySelectorAll(nodeId, selector).then(({nodeIds}) => nodeIds);
    }

    highlightDOMNode(nodeId, mode)
    {
        if (this._hideDOMNodeHighlightTimeout) {
            clearTimeout(this._hideDOMNodeHighlightTimeout);
            this._hideDOMNodeHighlightTimeout = undefined;
        }

        this._highlightedDOMNodeId = nodeId;
        if (nodeId)
            DOMAgent.highlightNode.invoke({nodeId, highlightConfig: this._buildHighlightConfig(mode)});
        else
            DOMAgent.hideHighlight();
    }

    highlightDOMNodeList(nodeIds, mode)
    {
        // COMPATIBILITY (iOS 11): DOM.highlightNodeList did not exist.
        if (!DOMAgent.highlightNodeList)
            return;

        if (this._hideDOMNodeHighlightTimeout) {
            clearTimeout(this._hideDOMNodeHighlightTimeout);
            this._hideDOMNodeHighlightTimeout = undefined;
        }

        DOMAgent.highlightNodeList(nodeIds, this._buildHighlightConfig(mode));
    }

    highlightSelector(selectorText, frameId, mode)
    {
        // COMPATIBILITY (iOS 8): DOM.highlightSelector did not exist.
        if (!DOMAgent.highlightSelector)
            return;

        if (this._hideDOMNodeHighlightTimeout) {
            clearTimeout(this._hideDOMNodeHighlightTimeout);
            this._hideDOMNodeHighlightTimeout = undefined;
        }

        DOMAgent.highlightSelector(this._buildHighlightConfig(mode), selectorText, frameId);
    }

    highlightRect(rect, usePageCoordinates)
    {
        DOMAgent.highlightRect.invoke({
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
        this.highlightDOMNode(0);
    }

    highlightDOMNodeForTwoSeconds(nodeId)
    {
        this.highlightDOMNode(nodeId);
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

        let commandArguments = {
            enabled,
            highlightConfig: this._buildHighlightConfig(),
            showRulers: WI.settings.showRulersDuringElementSelection.value,
        };
        DOMAgent.setInspectModeEnabled.invoke(commandArguments, (error) => {
            this._inspectModeEnabled = error ? false : enabled;
            this.dispatchEventToListeners(WI.DOMManager.Event.InspectModeStateChanged);
        });
    }

    setInspectedNode(node)
    {
        console.assert(node instanceof WI.DOMNode);
        if (node === this._inspectedNode)
            return;

        let callback = (error) => {
            console.assert(!error, error);
            if (error)
                return;

            this._inspectedNode = node;

            this.dispatchEventToListeners(WI.DOMManager.Event.InspectedNodeChanged);
        };

        // COMPATIBILITY (iOS 11): DOM.setInspectedNode did not exist.
        if (!DOMAgent.setInspectedNode) {
            ConsoleAgent.addInspectedNode(node.id, callback);
            return;
        }

        DOMAgent.setInspectedNode(node.id, callback);
    }

    getSupportedEventNames(callback)
    {
        if (!DOMAgent.getSupportedEventNames)
            return Promise.resolve(new Set);

        if (!this._getSupportedEventNamesPromise) {
            this._getSupportedEventNamesPromise = DOMAgent.getSupportedEventNames()
            .then(({eventNames}) => new Set(eventNames));
        }

        return this._getSupportedEventNamesPromise;
    }

    setEventListenerDisabled(eventListener, disabled)
    {
        DOMAgent.setEventListenerDisabled(eventListener.eventListenerId, disabled, (error) => {
            if (error)
                console.error(error);
        });
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
            if (target.DOMAgent)
                this._updateEventBreakpoint(breakpoint, target);
        }

        WI.domDebuggerManager.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointAdded, {breakpoint});
    }

    removeBreakpointForEventListener(eventListener)
    {
        let breakpoint = this._breakpointsForEventListeners.take(eventListener.eventListenerId);
        console.assert(breakpoint);

        for (let target of WI.targets) {
            if (target.DOMAgent)
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

    _buildHighlightConfig(mode = "all")
    {
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
            if (target.DOMAgent)
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
