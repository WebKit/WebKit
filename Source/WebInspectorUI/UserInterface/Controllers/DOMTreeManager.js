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

/**
 * @extends {WebInspector.Object}
 * @constructor
 */
WebInspector.DOMTreeManager = function() {
    /** @type {Object|undefined} */
    this._idToDOMNode = {};
    this._document = null;
    this._attributeLoadNodeIds = {};
    this._flows = new Map;
    this._contentNodesToFlowsMap = new Map;
}

WebInspector.Object.addConstructorFunctions(WebInspector.DOMTreeManager);

WebInspector.DOMTreeManager.Event = {
    AttributeModified: "dom-tree-manager-attribute-modified",
    AttributeRemoved: "dom-tree-manager-attribute-removed",
    CharacterDataModified: "dom-tree-manager-character-data-modified",
    NodeInserted: "dom-tree-manager-node-inserted",
    NodeRemoved: "dom-tree-manager-node-removed",
    DocumentUpdated: "dom-tree-manager-document-updated",
    ChildNodeCountUpdated: "dom-tree-manager-child-node-count-updated",
    DOMNodeWasInspected: "dom-tree-manager-dom-node-was-inspected",
    InspectModeStateChanged: "dom-tree-manager-inspect-mode-state-changed",
    ContentFlowListWasUpdated: "dom-tree-manager-content-flow-list-was-updated",
    ContentFlowWasAdded: "dom-tree-manager-content-flow-was-added",
    ContentFlowWasRemoved: "dom-tree-manager-content-flow-was-removed",
    RegionLayoutUpdated: "dom-tree-manager-region-layout-updated",
    RegionOversetChanged: "dom-tree-manager-region-overset-changed"
};

WebInspector.DOMTreeManager._flowPayloadHashKey = function(flowPayload)
{
    // Use the flow node id, to avoid collisions when we change main document id.
    return flowPayload.documentNodeId + ":" + flowPayload.name;
};

WebInspector.DOMTreeManager.prototype = {
    /**
     * @param {function(WebInspector.DOMDocument)=} callback
     */
    requestDocument: function(callback)
    {
        if (this._document) {
            if (callback)
                callback(this._document);
            return;
        }

        if (this._pendingDocumentRequestCallbacks) {
            this._pendingDocumentRequestCallbacks.push(callback);
            return;
        }

        this._pendingDocumentRequestCallbacks = [callback];

        /**
         * @this {WebInspector.DOMTreeManager}
         * @param {?Protocol.Error} error
         * @param {DOMAgent.Node} root
         */
        function onDocumentAvailable(error, root)
        {
            if (!error)
                this._setDocument(root);

            for (var i = 0; i < this._pendingDocumentRequestCallbacks.length; ++i) {
                var callback = this._pendingDocumentRequestCallbacks[i];
                if (callback)
                    callback(this._document);
            }
            delete this._pendingDocumentRequestCallbacks;
        }

        DOMAgent.getDocument(onDocumentAvailable.bind(this));
    },

    /**
     * @param {RuntimeAgent.RemoteObjectId} objectId
     * @param {function()=} callback
     */
    pushNodeToFrontend: function(objectId, callback)
    {
        this._dispatchWhenDocumentAvailable(DOMAgent.requestNode.bind(DOMAgent, objectId), callback);
    },

    /**
     * @param {string} path
     * @param {function(?WebInspector.DOMNode)=} callback
     */
    pushNodeByPathToFrontend: function(path, callback)
    {
        var callbackCast = /** @type {function(*)} */ callback;
        this._dispatchWhenDocumentAvailable(DOMAgent.pushNodeByPathToFrontend.bind(DOMAgent, path), callbackCast);
    },

    /**
     * @param {function(*)=} callback
     * @return {function(?Protocol.Error,*=)|undefined}
     */
    _wrapClientCallback: function(callback)
    {
        if (!callback)
            return;
        return function(error, result) {
            if (error)
                console.error("Error during DOMAgent operation: " + error);
            callback(error ? null : result);
        };
    },

    /**
     * @param {function(function()=)} func
     * @param {function(*)=} callback
     */
    _dispatchWhenDocumentAvailable: function(func, callback)
    {
        var callbackWrapper = /** @type {function(?Protocol.Error, *=)} */ this._wrapClientCallback(callback);

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
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} name
     * @param {string} value
     */
    _attributeModified: function(nodeId, name, value)
    {
        var node = this._idToDOMNode[nodeId];
        if (!node)
            return;
        node._setAttribute(name, value);
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.AttributeModified, { node: node, name: name });
        node.dispatchEventToListeners(WebInspector.DOMNode.Event.AttributeModified, {name: name});
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} name
     */
    _attributeRemoved: function(nodeId, name)
    {
        var node = this._idToDOMNode[nodeId];
        if (!node)
            return;
        node._removeAttribute(name);
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.AttributeRemoved, { node: node, name: name });
        node.dispatchEventToListeners(WebInspector.DOMNode.Event.AttributeRemoved, {name: name});
    },

    /**
     * @param {Array.<DOMAgent.NodeId>} nodeIds
     */
    _inlineStyleInvalidated: function(nodeIds)
    {
        for (var i = 0; i < nodeIds.length; ++i)
            this._attributeLoadNodeIds[nodeIds[i]] = true;
        if ("_loadNodeAttributesTimeout" in this)
            return;
        this._loadNodeAttributesTimeout = setTimeout(this._loadNodeAttributes.bind(this), 0);
    },

    _loadNodeAttributes: function()
    {
        /**
         * @this {WebInspector.DOMTreeManager}
         * @param {DOMAgent.NodeId} nodeId
         * @param {?Protocol.Error} error
         * @param {Array.<string>} attributes
         */
        function callback(nodeId, error, attributes)
        {
            if (error) {
                console.error("Error during DOMAgent operation: " + error);
                return;
            }
            var node = this._idToDOMNode[nodeId];
            if (node) {
                node._setAttributesPayload(attributes);
                this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.AttributeModified, { node: node, name: "style" });
                node.dispatchEventToListeners(WebInspector.DOMNode.Event.AttributeModified, {name: "style"});
            }
        }

        delete this._loadNodeAttributesTimeout;

        for (var nodeId in this._attributeLoadNodeIds) {
            var nodeIdAsNumber = parseInt(nodeId, 10);
            DOMAgent.getAttributes(nodeIdAsNumber, callback.bind(this, nodeIdAsNumber));
        }
        this._attributeLoadNodeIds = {};
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} newValue
     */
    _characterDataModified: function(nodeId, newValue)
    {
        var node = this._idToDOMNode[nodeId];
        node._nodeValue = newValue;
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.CharacterDataModified, {node: node});
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @return {WebInspector.DOMNode|undefined}
     */
    nodeForId: function(nodeId)
    {
        return this._idToDOMNode[nodeId];
    },

    _documentUpdated: function()
    {
        this._setDocument(null);
    },

    /**
     * @param {DOMAgent.Node} payload
     */
    _setDocument: function(payload)
    {
        this._idToDOMNode = {};
        if (payload && "nodeId" in payload)
            this._document = new WebInspector.DOMNode(this, null, false, payload);
        else
            this._document = null;
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.DocumentUpdated, this._document);
    },

    /**
     * @param {DOMAgent.Node} payload
     */
    _setDetachedRoot: function(payload)
    {
        new WebInspector.DOMNode(this, null, false, payload);
    },

    /**
     * @param {DOMAgent.NodeId} parentId
     * @param {Array.<DOMAgent.Node>} payloads
     */
    _setChildNodes: function(parentId, payloads)
    {
        if (!parentId && payloads.length) {
            this._setDetachedRoot(payloads[0]);
            return;
        }

        var parent = this._idToDOMNode[parentId];
        parent._setChildrenPayload(payloads);
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {number} newValue
     */
    _childNodeCountUpdated: function(nodeId, newValue)
    {
        var node = this._idToDOMNode[nodeId];
        node.childNodeCount = newValue;
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.ChildNodeCountUpdated, node);
    },

    /**
     * @param {DOMAgent.NodeId} parentId
     * @param {DOMAgent.NodeId} prevId
     * @param {DOMAgent.Node} payload
     */
    _childNodeInserted: function(parentId, prevId, payload)
    {
        var parent = this._idToDOMNode[parentId];
        var prev = this._idToDOMNode[prevId];
        var node = parent._insertChild(prev, payload);
        this._idToDOMNode[node.id] = node;
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.NodeInserted, {node: node, parent: parent});
    },

    /**
     * @param {DOMAgent.NodeId} parentId
     * @param {DOMAgent.NodeId} nodeId
     */
    _childNodeRemoved: function(parentId, nodeId)
    {
        var parent = this._idToDOMNode[parentId];
        var node = this._idToDOMNode[nodeId];
        parent._removeChild(node);
        this._unbind(node);
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.NodeRemoved, {node:node, parent: parent});
    },

    /**
     * @param {DOMAgent.Node} node
     */
    _unbind: function(node)
    {
        this._removeContentNodeFromFlowIfNeeded(node);

        delete this._idToDOMNode[node.id];
        for (var i = 0; node.children && i < node.children.length; ++i)
            this._unbind(node.children[i]);
    },

    /**
     * @param {number} nodeId
     */
    inspectElement: function(nodeId)
    {
        var node = this._idToDOMNode[nodeId];
        if (node)
            this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.DOMNodeWasInspected, {node: node});

        this._inspectModeEnabled = false;
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.InspectModeStateChanged);
    },

    inspectNodeObject: function(remoteObject)
    {
        function nodeAvailable(nodeId)
        {
            remoteObject.release();

            console.assert(nodeId);
            if (!nodeId)
                return;

            this.inspectElement(nodeId);
        }

        remoteObject.pushNodeToFrontend(nodeAvailable.bind(this));
    },

    /**
     * @param {string} query
     * @param {function(number)} searchCallback
     */
    performSearch: function(query, searchCallback)
    {
        this.cancelSearch();

        /**
         * @param {?Protocol.Error} error
         * @param {string} searchId
         * @param {number} resultsCount
         */
        function callback(error, searchId, resultsCount)
        {
            this._searchId = searchId;
            searchCallback(resultsCount);
        }
        DOMAgent.performSearch(query, callback.bind(this));
    },

    /**
     * @param {number} index
     * @param {?function(DOMAgent.Node)} callback
     */
    searchResult: function(index, callback)
    {
        if (this._searchId) {
            /**
             * @param {?Protocol.Error} error
             * @param {Array.<number>} nodeIds
             */
            function mycallback(error, nodeIds)
            {
                if (error) {
                    console.error(error);
                    callback(null);
                    return;
                }
                if (nodeIds.length != 1)
                    return;

                callback(this._idToDOMNode[nodeIds[0]]);
            }
            DOMAgent.getSearchResults(this._searchId, index, index + 1, mycallback.bind(this));
        } else
            callback(null);
    },

    cancelSearch: function()
    {
        if (this._searchId) {
            DOMAgent.discardSearchResults(this._searchId);
            delete this._searchId;
        }
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} selectors
     * @param {function(?DOMAgent.NodeId)=} callback
     */
    querySelector: function(nodeId, selectors, callback)
    {
        var callbackCast = /** @type {function(*)|undefined} */callback;
        DOMAgent.querySelector(nodeId, selectors, this._wrapClientCallback(callbackCast));
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} selectors
     * @param {function(?Array.<DOMAgent.NodeId>)=} callback
     */
    querySelectorAll: function(nodeId, selectors, callback)
    {
        var callbackCast = /** @type {function(*)|undefined} */callback;
        DOMAgent.querySelectorAll(nodeId, selectors, this._wrapClientCallback(callbackCast));
    },

    /**
     * @param {?number} nodeId
     * @param {string=} mode
     */
    highlightDOMNode: function(nodeId, mode)
    {
        if (this._hideDOMNodeHighlightTimeout) {
            clearTimeout(this._hideDOMNodeHighlightTimeout);
            delete this._hideDOMNodeHighlightTimeout;
        }

        this._highlightedDOMNodeId = nodeId;
        if (nodeId)
            DOMAgent.highlightNode.invoke({nodeId: nodeId, highlightConfig: this._buildHighlightConfig(mode)});
        else
            DOMAgent.hideHighlight();
    },

    highlightRect: function(rect, usePageCoordinates)
    {
        DOMAgent.highlightRect.invoke({
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height,
            color: {r: 111, g: 168, b: 220, a: 0.66},
            outlineColor: {r: 255, g: 229, b: 153, a: 0.66},
            usePageCoordinates: usePageCoordinates
        });
    },

    hideDOMNodeHighlight: function()
    {
        this.highlightDOMNode(0);
    },

    /**
     * @param {?DOMAgent.NodeId} nodeId
     */
    highlightDOMNodeForTwoSeconds: function(nodeId)
    {
        this.highlightDOMNode(nodeId);
        this._hideDOMNodeHighlightTimeout = setTimeout(this.hideDOMNodeHighlight.bind(this), 2000);
    },

    get inspectModeEnabled()
    {
        return this._inspectModeEnabled;
    },

    set inspectModeEnabled(enabled)
    {
        function callback(error)
        {
            this._inspectModeEnabled = error ? false : enabled;
            this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.InspectModeStateChanged);
        }

        DOMAgent.setInspectModeEnabled(enabled, this._buildHighlightConfig(), callback.bind(this));
    },

    /**
     * @param {string=} mode
     */
    _buildHighlightConfig: function(mode)
    {
        mode = mode || "all";
        var highlightConfig = { showInfo: mode === "all" };
        if (mode === "all" || mode === "content")
            highlightConfig.contentColor = {r: 111, g: 168, b: 220, a: 0.66};

        if (mode === "all" || mode === "padding")
            highlightConfig.paddingColor = {r: 147, g: 196, b: 125, a: 0.66};

        if (mode === "all" || mode === "border")
            highlightConfig.borderColor = {r: 255, g: 229, b: 153, a: 0.66};

        if (mode === "all" || mode === "margin")
            highlightConfig.marginColor = {r: 246, g: 178, b: 107, a: 0.66};

        return highlightConfig;
    },

    _createContentFlowFromPayload: function(flowPayload)
    {
        // FIXME: Collect the regions from the payload.
        var flow = new WebInspector.ContentFlow(flowPayload.documentNodeId, flowPayload.name, flowPayload.overset, flowPayload.content.map(this.nodeForId.bind(this)));

        for (var contentNode of flow.contentNodes) {
            console.assert(!this._contentNodesToFlowsMap.has(contentNode.id));
            this._contentNodesToFlowsMap.set(contentNode.id, flow);
        }

        return flow;
    },

    _updateContentFlowFromPayload: function(contentFlow, flowPayload)
    {
        console.assert(contentFlow.contentNodes.length === flowPayload.content.length);
        console.assert(contentFlow.contentNodes.every(function(node, i) { node.id === flowPayload.content[i]; }));

        // FIXME: Collect the regions from the payload.
        contentFlow.overset = flowPayload.overset;
    },

    getNamedFlowCollection: function(documentNodeIdentifier)
    {
        function onNamedFlowCollectionAvailable(error, flows)
        {
            if (error) {
                console.error("Error while getting the named flows for document " + documentNodeIdentifier + ": " + error);
                return;
            }
            this._contentNodesToFlowsMap.clear();
            var contentFlows = [];
            for (var i = 0; i < flows.length; ++i) {
                var flowPayload = flows[i];
                var flowKey = WebInspector.DOMTreeManager._flowPayloadHashKey(flowPayload);
                var contentFlow = this._flows.get(flowKey);
                if (contentFlow)
                    this._updateContentFlowFromPayload(contentFlow, flowPayload);
                else {
                    contentFlow = this._createContentFlowFromPayload(flowPayload);
                    this._flows.set(flowKey, contentFlow);
                }
                contentFlows.push(contentFlow);
            }
            this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.ContentFlowListWasUpdated, {documentNodeIdentifier: documentNodeIdentifier, flows: contentFlows});
        }

        if (CSSAgent.getNamedFlowCollection)
            CSSAgent.getNamedFlowCollection(documentNodeIdentifier, onNamedFlowCollectionAvailable.bind(this));
    },

    namedFlowCreated: function(flowPayload)
    {
        var flowKey = WebInspector.DOMTreeManager._flowPayloadHashKey(flowPayload);
        console.assert(!this._flows.has(flowKey));
        var contentFlow = this._createContentFlowFromPayload(flowPayload);
        this._flows.set(flowKey, contentFlow);
        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.ContentFlowWasAdded, {flow: contentFlow});
    },

    namedFlowRemoved: function(documentNodeIdentifier, flowName)
    {
        var flowKey = WebInspector.DOMTreeManager._flowPayloadHashKey({documentNodeId: documentNodeIdentifier, name: flowName});
        var contentFlow = this._flows.get(flowKey);
        console.assert(contentFlow);
        this._flows.delete(flowKey);

        // Remove any back links to this flow from the content nodes.
        for (var contentNode of contentFlow.contentNodes)
            this._contentNodesToFlowsMap.delete(contentNode.id);

        this.dispatchEventToListeners(WebInspector.DOMTreeManager.Event.ContentFlowWasRemoved, {flow: contentFlow});
    },

    _sendNamedFlowUpdateEvents: function(flowPayload)
    {
        var flowKey = WebInspector.DOMTreeManager._flowPayloadHashKey(flowPayload);
        console.assert(this._flows.has(flowKey));
        this._updateContentFlowFromPayload(this._flows.get(flowKey), flowPayload);
    },

    regionLayoutUpdated: function(flowPayload)
    {
        this._sendNamedFlowUpdateEvents(flowPayload);
    },

    regionOversetChanged: function(flowPayload)
    {
        this._sendNamedFlowUpdateEvents(flowPayload);
    },

    registeredNamedFlowContentElement: function(documentNodeIdentifier, flowName, contentNodeId, nextContentElementNodeId)
    {
        var flowKey = WebInspector.DOMTreeManager._flowPayloadHashKey({documentNodeId: documentNodeIdentifier, name: flowName});
        console.assert(this._flows.has(flowKey));
        console.assert(!this._contentNodesToFlowsMap.has(contentNodeId));

        var flow = this._flows.get(flowKey);
        var contentNode = this.nodeForId(contentNodeId);

        this._contentNodesToFlowsMap.set(contentNode.id, flow);

        if (nextContentElementNodeId)
            flow.insertContentNodeBefore(contentNode, this.nodeForId(nextContentElementNodeId));
        else
            flow.appendContentNode(contentNode);
    },

    _removeContentNodeFromFlowIfNeeded: function(node)
    {
        if (!this._contentNodesToFlowsMap.has(node.id))
            return;
        var flow = this._contentNodesToFlowsMap.get(node.id);
        this._contentNodesToFlowsMap.delete(node.id);
        flow.removeContentNode(node);
    },

    unregisteredNamedFlowContentElement: function(documentNodeIdentifier, flowName, contentNodeId)
    {
        console.assert(this._contentNodesToFlowsMap.has(contentNodeId));

        var flow = this._contentNodesToFlowsMap.get(contentNodeId);
        console.assert(flow.id === WebInspector.DOMTreeManager._flowPayloadHashKey({documentNodeId: documentNodeIdentifier, name: flowName}));

        this._contentNodesToFlowsMap.delete(contentNodeId);
        flow.removeContentNode(this.nodeForId(contentNodeId));
    },

    _coerceRemoteArrayOfDOMNodes: function(objectId, callback)
    {
        var length, nodes, received = 0, lastError = null, domTreeManager = this;

        function nodeRequested(index, error, nodeId)
        {
            if (error)
                lastError = error;
            else
                nodes[index] = domTreeManager._idToDOMNode[nodeId];
            if (++received === length)
                callback(lastError, nodes);
        }

        WebInspector.runtimeManager.getPropertiesForRemoteObject(objectId, function(error, properties) {
            if (error) {
                callback(error);
                return;
            }

            var lengthProperty = properties.get("length");
            if (!lengthProperty || lengthProperty.value.type !== "number") {
                callback(null);
                return;
            }

            length = lengthProperty.value.value;
            if (!length) {
                callback(null, []);
                return;
            }

            nodes = new Array(length);
            for (var i = 0; i < length; ++i) {
                var nodeProperty = properties.get(String(i));
                console.assert(nodeProperty.value.type === "object");
                DOMAgent.requestNode(nodeProperty.value.objectId, nodeRequested.bind(null, i));
            }
        });
    },

    getNodeContentFlowInfo: function(domNode, resultReadyCallback)
    {
        DOMAgent.resolveNode(domNode.id, domNodeResolved.bind(this));

        function domNodeResolved(error, remoteObject)
        {
            if (error) {
                resultReadyCallback(error);
                return;
            }
            // Serialize "backendFunction" and execute it in the context of the page
            // passing the DOMNode as the "this" reference.
            var evalParameters = {
                objectId: remoteObject.objectId,
                functionDeclaration: backendFunction.toString(),
                doNotPauseOnExceptionsAndMuteConsole: true,
                returnByValue: false,
                generatePreview: false
            };
            RuntimeAgent.callFunctionOn.invoke(evalParameters, regionNodesAvailable.bind(this));
        }

        function regionNodesAvailable(error, remoteObject, wasThrown)
        {
            if (error) {
                resultReadyCallback(error);
                return;
            }

            if (wasThrown) {
                // We should never get here, but having the error is useful for debugging.
                console.error("Error while executing backend function:", JSON.stringify(remoteObject));
                resultReadyCallback(null);
                return;
            }

            // The backend function can never return null.
            console.assert(remoteObject.type === "object");
            console.assert(remoteObject.objectId);
            WebInspector.runtimeManager.getPropertiesForRemoteObject(remoteObject.objectId, remoteObjectPropertiesAvailable.bind(this));
        }

        function remoteObjectPropertiesAvailable(error, properties) {
            if (error) {
                resultReadyCallback(error);
                return;
            }

            var result = {
                regionFlow: null,
                contentFlow: null,
                regions: null
            };

            var regionFlowNameProperty = properties.get("regionFlowName");
            if (regionFlowNameProperty && regionFlowNameProperty.value && regionFlowNameProperty.value.value) {
                console.assert(regionFlowNameProperty.value.type === "string");
                var regionFlowKey = WebInspector.DOMTreeManager._flowPayloadHashKey({documentNodeId: domNode.ownerDocument.id, name: regionFlowNameProperty.value.value});
                result.regionFlow = this._flows.get(regionFlowKey);
            }

            var contentFlowNameProperty = properties.get("contentFlowName");
            if (contentFlowNameProperty && contentFlowNameProperty.value && contentFlowNameProperty.value.value) {
                console.assert(contentFlowNameProperty.value.type === "string");
                var contentFlowKey = WebInspector.DOMTreeManager._flowPayloadHashKey({documentNodeId: domNode.ownerDocument.id, name: contentFlowNameProperty.value.value});
                result.contentFlow = this._flows.get(contentFlowKey);
            }

            var regionsProperty = properties.get("regions");
            if (!regionsProperty || !regionsProperty.value.objectId) {
                // The list of regions is null.
                resultReadyCallback(null, result);
                return;
            }

            console.assert(regionsProperty.value.type === "object");
            console.assert(regionsProperty.value.subtype === "array");
            this._coerceRemoteArrayOfDOMNodes(regionsProperty.value.objectId, function(error, nodes) {
                result.regions = nodes;
                resultReadyCallback(error, result);
            });
        }

        // Note that "backendFunction" is serialized and executed in the context of the page.
        function backendFunction()
        {
            function getComputedProperty(node, propertyName)
            {
                if (!node.ownerDocument || !node.ownerDocument.defaultView)
                    return null;
                var computedStyle = node.ownerDocument.defaultView.getComputedStyle(node);
                return computedStyle ? computedStyle[propertyName] : null;
            }

            function getContentFlowName(node)
            {
                for (; node; node = node.parentNode) {
                    var flowName = getComputedProperty(node, "webkitFlowInto");
                    if (flowName && flowName !== "none")
                        return flowName;
                }
                return null;
            }

            var node = this;

            // Even detached nodes have an ownerDocument.
            console.assert(node.ownerDocument);

            var result = {
                regionFlowName: getComputedProperty(node, "webkitFlowFrom"),
                contentFlowName: getContentFlowName(node),
                regions: null
            };

            if (result.contentFlowName) {
                var flowThread = node.ownerDocument.webkitGetNamedFlows().namedItem(result.contentFlowName);
                if (flowThread)
                    result.regions = flowThread.getRegionsByContent(node);
            }

            return result;
        }
    }
}

WebInspector.DOMTreeManager.prototype.__proto__ = WebInspector.Object.prototype;
