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

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this.ensureDocument();
    }

    // Public

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

        if (this._pendingDocumentRequestCallbacks) {
            this._pendingDocumentRequestCallbacks.push(callback);
            return;
        }

        this._pendingDocumentRequestCallbacks = [callback];

        function onDocumentAvailable(error, root)
        {
            if (!error)
                this._setDocument(root);

            for (let callback of this._pendingDocumentRequestCallbacks)
                callback(this._document);

            this._pendingDocumentRequestCallbacks = null;
        }

        if (window.DOMAgent)
            DOMAgent.getDocument(onDocumentAvailable.bind(this));
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

    didAddEventListener(nodeId)
    {
        // Called from WI.DOMObserver.

        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.dispatchEventToListeners(WI.DOMNode.Event.EventListenersChanged);
    }

    willRemoveEventListener(nodeId)
    {
        // Called from WI.DOMObserver.

        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.dispatchEventToListeners(WI.DOMNode.Event.EventListenersChanged);
    }

    didFireEvent(nodeId, eventName, timestamp, data)
    {
        // Called from WI.DOMObserver.

        let node = this._idToDOMNode[nodeId];
        if (!node)
            return;

        node.didFireEvent(eventName, timestamp, data);
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
        this._breakpointsForEventListeners.clear();

        let newDocument = null;
        if (payload && "nodeId" in payload)
            newDocument = new WI.DOMNode(this, null, false, payload);

        if (this._document === newDocument)
            return;

        this._document = newDocument;
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

    performSearch(query, searchCallback)
    {
        this.cancelSearch();

        function callback(error, searchId, resultsCount)
        {
            this._searchId = searchId;
            searchCallback(resultsCount);
        }
        DOMAgent.performSearch(query, callback.bind(this));
    }

    searchResult(index, callback)
    {
        function mycallback(error, nodeIds)
        {
            if (error) {
                console.error(error);
                callback(null);
                return;
            }
            if (nodeIds.length !== 1)
                return;

            callback(this._idToDOMNode[nodeIds[0]]);
        }

        if (this._searchId)
            DOMAgent.getSearchResults(this._searchId, index, index + 1, mycallback.bind(this));
        else
            callback(null);
    }

    cancelSearch()
    {
        if (this._searchId) {
            DOMAgent.discardSearchResults(this._searchId);
            this._searchId = undefined;
        }
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

        DOMAgent.setInspectModeEnabled(enabled, this._buildHighlightConfig(), (error) => {
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
        let breakpoint = new WI.EventBreakpoint(WI.EventBreakpoint.Type.Listener, eventListener.type, {eventListener});
        this._breakpointsForEventListeners.set(eventListener.eventListenerId, breakpoint);

        DOMAgent.setBreakpointForEventListener(eventListener.eventListenerId, (error) => {
            if (error) {
                console.error(error);
                return;
            }

            WI.domDebuggerManager.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointAdded, {breakpoint});
        });
    }

    removeBreakpointForEventListener(eventListener)
    {
        let breakpoint = this._breakpointsForEventListeners.get(eventListener.eventListenerId);
        console.assert(breakpoint);

        this._breakpointsForEventListeners.delete(eventListener.eventListenerId);

        DOMAgent.removeBreakpointForEventListener(eventListener.eventListenerId, (error) => {
            if (error) {
                console.error(error);
                return;
            }

            if (breakpoint)
                WI.domDebuggerManager.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, {breakpoint});
        });
    }

    breakpointForEventListenerId(eventListenerId)
    {
        return this._breakpointsForEventListeners.get(eventListenerId) || null;
    }

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

    // Private

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
};
