/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

WI.DOMNode = class DOMNode extends WI.Object
{
    constructor(domManager, doc, isInShadowTree, payload)
    {
        super();

        this._domManager = domManager;
        this._isInShadowTree = isInShadowTree;

        this.id = payload.nodeId;
        this._domManager._idToDOMNode[this.id] = this;

        this._nodeType = payload.nodeType;
        this._nodeName = payload.nodeName;
        this._localName = payload.localName;
        this._nodeValue = payload.nodeValue;
        this._pseudoType = payload.pseudoType;
        this._shadowRootType = payload.shadowRootType;
        this._computedRole = null;
        this._contentSecurityPolicyHash = payload.contentSecurityPolicyHash;

        if (this._nodeType === Node.DOCUMENT_NODE)
            this.ownerDocument = this;
        else
            this.ownerDocument = doc;

        this._attributes = [];
        this._attributesMap = new Map;
        if (payload.attributes)
            this._setAttributesPayload(payload.attributes);

        this._childNodeCount = payload.childNodeCount;
        this._children = null;
        this._filteredChildren = null;
        this._filteredChildrenNeedsUpdating = true;

        this._nextSibling = null;
        this._previousSibling = null;
        this.parentNode = null;

        this._enabledPseudoClasses = [];

        // FIXME: The logic around this._shadowRoots and this._children is very confusing.
        // We eventually include shadow roots at the start of _children. However we might
        // not have our actual children yet. So we try to defer initializing _children until
        // we have both shadowRoots and child nodes.
        this._shadowRoots = [];
        if (payload.shadowRoots) {
            for (var i = 0; i < payload.shadowRoots.length; ++i) {
                var root = payload.shadowRoots[i];
                var node = new WI.DOMNode(this._domManager, this.ownerDocument, true, root);
                node.parentNode = this;
                this._shadowRoots.push(node);
            }
        }

        if (payload.children)
            this._setChildrenPayload(payload.children);
        else if (this._shadowRoots.length && !this._childNodeCount)
            this._children = this._shadowRoots.slice();

        if (this._nodeType === Node.ELEMENT_NODE)
            this._customElementState = payload.customElementState || WI.DOMNode.CustomElementState.Builtin;
        else
            this._customElementState = null;

        if (payload.templateContent) {
            this._templateContent = new WI.DOMNode(this._domManager, this.ownerDocument, false, payload.templateContent);
            this._templateContent.parentNode = this;
        }

        this._pseudoElements = new Map;
        if (payload.pseudoElements) {
            for (var i = 0; i < payload.pseudoElements.length; ++i) {
                var node = new WI.DOMNode(this._domManager, this.ownerDocument, this._isInShadowTree, payload.pseudoElements[i]);
                node.parentNode = this;
                this._pseudoElements.set(node.pseudoType(), node);
            }
        }

        if (payload.contentDocument) {
            this._contentDocument = new WI.DOMNode(this._domManager, null, false, payload.contentDocument);
            this._children = [this._contentDocument];
            this._renumber();
        }

        if (payload.frameId)
            this._frameIdentifier = payload.frameId;

        if (this._nodeType === Node.ELEMENT_NODE) {
            // HTML and BODY from internal iframes should not overwrite top-level ones.
            if (this.ownerDocument && !this.ownerDocument.documentElement && this._nodeName === "HTML")
                this.ownerDocument.documentElement = this;
            if (this.ownerDocument && !this.ownerDocument.body && this._nodeName === "BODY")
                this.ownerDocument.body = this;
            if (payload.documentURL)
                this.documentURL = payload.documentURL;
        } else if (this._nodeType === Node.DOCUMENT_TYPE_NODE) {
            this.publicId = payload.publicId;
            this.systemId = payload.systemId;
        } else if (this._nodeType === Node.DOCUMENT_NODE) {
            this.documentURL = payload.documentURL;
            this.xmlVersion = payload.xmlVersion;
        } else if (this._nodeType === Node.ATTRIBUTE_NODE) {
            this.name = payload.name;
            this.value = payload.value;
        }

        this._domEvents = [];
        this._lowPowerRanges = [];

        if (this._shouldListenForEventListeners())
            WI.DOMNode.addEventListener(WI.DOMNode.Event.DidFireEvent, this._handleDOMNodeDidFireEvent, this);
    }

    // Static

    static getFullscreenDOMEvents(domEvents)
    {
        return domEvents.reduce((accumulator, current) => {
            if (current.eventName === "webkitfullscreenchange" && current.data && (!accumulator.length || accumulator.lastValue.data.enabled !== current.data.enabled))
                accumulator.push(current);
            return accumulator;
        }, []);
    }

    static isPlayEvent(eventName)
    {
        return eventName === "play"
            || eventName === "playing";
    }

    static isPauseEvent(eventName)
    {
        return eventName === "pause"
            || eventName === "stall";
    }

    static isStopEvent(eventName)
    {
        return eventName === "emptied"
            || eventName === "ended"
            || eventName === "suspend";
    }


    // Public

    get domEvents() { return this._domEvents; }
    get lowPowerRanges() { return this._lowPowerRanges; }

    get frameIdentifier()
    {
        return this._frameIdentifier || this.ownerDocument.frameIdentifier;
    }

    get frame()
    {
        if (!this._frame)
            this._frame = WI.networkManager.frameForIdentifier(this.frameIdentifier);
        return this._frame;
    }

    get children()
    {
        if (!this._children)
            return null;

        if (WI.settings.showShadowDOM.value)
            return this._children;

        if (this._filteredChildrenNeedsUpdating) {
            this._filteredChildrenNeedsUpdating = false;
            this._filteredChildren = this._children.filter(function(node) {
                return !node._isInShadowTree;
            });
        }

        return this._filteredChildren;
    }

    get firstChild()
    {
        var children = this.children;

        if (children && children.length > 0)
            return children[0];

        return null;
    }

    get lastChild()
    {
        var children = this.children;

        if (children && children.length > 0)
            return children.lastValue;

        return null;
    }

    get nextSibling()
    {
        if (WI.settings.showShadowDOM.value)
            return this._nextSibling;

        var node = this._nextSibling;
        while (node) {
            if (!node._isInShadowTree)
                return node;
            node = node._nextSibling;
        }
        return null;
    }

    get previousSibling()
    {
        if (WI.settings.showShadowDOM.value)
            return this._previousSibling;

        var node = this._previousSibling;
        while (node) {
            if (!node._isInShadowTree)
                return node;
            node = node._previousSibling;
        }
        return null;
    }

    get childNodeCount()
    {
        var children = this.children;
        if (children)
            return children.length;

        if (WI.settings.showShadowDOM.value)
            return this._childNodeCount + this._shadowRoots.length;

        return this._childNodeCount;
    }

    set childNodeCount(count)
    {
        this._childNodeCount = count;
    }

    computedRole()
    {
        return this._computedRole;
    }

    contentSecurityPolicyHash()
    {
        return this._contentSecurityPolicyHash;
    }

    hasAttributes()
    {
        return this._attributes.length > 0;
    }

    hasChildNodes()
    {
        return this.childNodeCount > 0;
    }

    hasShadowRoots()
    {
        return !!this._shadowRoots.length;
    }

    isInShadowTree()
    {
        return this._isInShadowTree;
    }

    isInUserAgentShadowTree()
    {
        return this._isInShadowTree && this.ancestorShadowRoot().isUserAgentShadowRoot();
    }

    isCustomElement()
    {
        return this._customElementState === WI.DOMNode.CustomElementState.Custom;
    }

    customElementState()
    {
        return this._customElementState;
    }

    isShadowRoot()
    {
        return !!this._shadowRootType;
    }

    isUserAgentShadowRoot()
    {
        return this._shadowRootType === WI.DOMNode.ShadowRootType.UserAgent;
    }

    ancestorShadowRoot()
    {
        if (!this._isInShadowTree)
            return null;

        let node = this;
        while (node && !node.isShadowRoot())
            node = node.parentNode;
        return node;
    }

    ancestorShadowHost()
    {
        let shadowRoot = this.ancestorShadowRoot();
        return shadowRoot ? shadowRoot.parentNode : null;
    }

    isPseudoElement()
    {
        return this._pseudoType !== undefined;
    }

    nodeType()
    {
        return this._nodeType;
    }

    nodeName()
    {
        return this._nodeName;
    }

    nodeNameInCorrectCase()
    {
        return this.isXMLNode() ? this.nodeName() : this.nodeName().toLowerCase();
    }

    setNodeName(name, callback)
    {
        DOMAgent.setNodeName(this.id, name, this._makeUndoableCallback(callback));
    }

    localName()
    {
        return this._localName;
    }

    templateContent()
    {
        return this._templateContent || null;
    }

    pseudoType()
    {
        return this._pseudoType;
    }

    hasPseudoElements()
    {
        return this._pseudoElements.size > 0;
    }

    pseudoElements()
    {
        return this._pseudoElements;
    }

    beforePseudoElement()
    {
        return this._pseudoElements.get(WI.DOMNode.PseudoElementType.Before) || null;
    }

    afterPseudoElement()
    {
        return this._pseudoElements.get(WI.DOMNode.PseudoElementType.After) || null;
    }

    shadowRoots()
    {
        return this._shadowRoots;
    }

    shadowRootType()
    {
        return this._shadowRootType;
    }

    nodeValue()
    {
        return this._nodeValue;
    }

    setNodeValue(value, callback)
    {
        DOMAgent.setNodeValue(this.id, value, this._makeUndoableCallback(callback));
    }

    getAttribute(name)
    {
        let attr = this._attributesMap.get(name);
        return attr ? attr.value : undefined;
    }

    setAttribute(name, text, callback)
    {
        DOMAgent.setAttributesAsText(this.id, text, name, this._makeUndoableCallback(callback));
    }

    setAttributeValue(name, value, callback)
    {
        DOMAgent.setAttributeValue(this.id, name, value, this._makeUndoableCallback(callback));
    }

    attributes()
    {
        return this._attributes;
    }

    removeAttribute(name, callback)
    {
        function mycallback(error, success)
        {
            if (!error) {
                this._attributesMap.delete(name);
                for (var i = 0; i < this._attributes.length; ++i) {
                    if (this._attributes[i].name === name) {
                        this._attributes.splice(i, 1);
                        break;
                    }
                }
            }

            this._makeUndoableCallback(callback)(error);
        }
        DOMAgent.removeAttribute(this.id, name, mycallback.bind(this));
    }

    toggleClass(className, flag)
    {
        if (!className || !className.length)
            return;

        if (this.isPseudoElement()) {
            this.parentNode.toggleClass(className, flag);
            return;
        }

        if (this.nodeType() !== Node.ELEMENT_NODE)
            return;

        WI.RemoteObject.resolveNode(this).then((object) => {
            function inspectedPage_node_toggleClass(className, flag) {
                this.classList.toggle(className, flag);
            }

            object.callFunction(inspectedPage_node_toggleClass, [className, flag]);
            object.release();
        });
    }

    scrollIntoView()
    {
        WI.RemoteObject.resolveNode(this).then((object) => {
            function inspectedPage_node_scrollIntoView() {
                this.scrollIntoViewIfNeeded(true);
            }

            object.callFunction(inspectedPage_node_scrollIntoView);
            object.release();
        });
    }

    getChildNodes(callback)
    {
        if (this.children) {
            if (callback)
                callback(this.children);
            return;
        }

        function mycallback(error) {
            if (!error && callback)
                callback(this.children);
        }

        DOMAgent.requestChildNodes(this.id, mycallback.bind(this));
    }

    getSubtree(depth, callback)
    {
        function mycallback(error)
        {
            if (callback)
                callback(error ? null : this.children);
        }

        DOMAgent.requestChildNodes(this.id, depth, mycallback.bind(this));
    }

    getOuterHTML(callback)
    {
        DOMAgent.getOuterHTML(this.id, callback);
    }

    setOuterHTML(html, callback)
    {
        DOMAgent.setOuterHTML(this.id, html, this._makeUndoableCallback(callback));
    }

    insertAdjacentHTML(position, html)
    {
        if (this.nodeType() !== Node.ELEMENT_NODE)
            return;

        // COMPATIBILITY (iOS 11.0): DOM.insertAdjacentHTML did not exist.
        if (!DOMAgent.insertAdjacentHTML) {
            WI.RemoteObject.resolveNode(this).then((object) => {
                function inspectedPage_node_insertAdjacentHTML(position, html) {
                    this.insertAdjacentHTML(position, html);
                }

                object.callFunction(inspectedPage_node_insertAdjacentHTML, [position, html]);
                object.release();
            });
            return;
        }

        DOMAgent.insertAdjacentHTML(this.id, position, html, this._makeUndoableCallback());
    }

    removeNode(callback)
    {
        DOMAgent.removeNode(this.id, this._makeUndoableCallback(callback));
    }

    copyNode()
    {
        function copy(error, text)
        {
            if (!error)
                InspectorFrontendHost.copyText(text);
        }
        DOMAgent.getOuterHTML(this.id, copy);
    }

    getEventListeners(callback)
    {
        DOMAgent.getEventListenersForNode(this.id, callback);
    }

    accessibilityProperties(callback)
    {
        function accessibilityPropertiesCallback(error, accessibilityProperties)
        {
            if (!error && callback && accessibilityProperties) {
                this._computedRole = accessibilityProperties.role;

                callback({
                    activeDescendantNodeId: accessibilityProperties.activeDescendantNodeId,
                    busy: accessibilityProperties.busy,
                    checked: accessibilityProperties.checked,
                    childNodeIds: accessibilityProperties.childNodeIds,
                    controlledNodeIds: accessibilityProperties.controlledNodeIds,
                    current: accessibilityProperties.current,
                    disabled: accessibilityProperties.disabled,
                    exists: accessibilityProperties.exists,
                    expanded: accessibilityProperties.expanded,
                    flowedNodeIds: accessibilityProperties.flowedNodeIds,
                    focused: accessibilityProperties.focused,
                    ignored: accessibilityProperties.ignored,
                    ignoredByDefault: accessibilityProperties.ignoredByDefault,
                    invalid: accessibilityProperties.invalid,
                    isPopupButton: accessibilityProperties.isPopUpButton,
                    headingLevel: accessibilityProperties.headingLevel,
                    hierarchyLevel: accessibilityProperties.hierarchyLevel,
                    hidden: accessibilityProperties.hidden,
                    label: accessibilityProperties.label,
                    liveRegionAtomic: accessibilityProperties.liveRegionAtomic,
                    liveRegionRelevant: accessibilityProperties.liveRegionRelevant,
                    liveRegionStatus: accessibilityProperties.liveRegionStatus,
                    mouseEventNodeId: accessibilityProperties.mouseEventNodeId,
                    nodeId: accessibilityProperties.nodeId,
                    ownedNodeIds: accessibilityProperties.ownedNodeIds,
                    parentNodeId: accessibilityProperties.parentNodeId,
                    pressed: accessibilityProperties.pressed,
                    readonly: accessibilityProperties.readonly,
                    required: accessibilityProperties.required,
                    role: accessibilityProperties.role,
                    selected: accessibilityProperties.selected,
                    selectedChildNodeIds: accessibilityProperties.selectedChildNodeIds
                });
            }
        }
        DOMAgent.getAccessibilityPropertiesForNode(this.id, accessibilityPropertiesCallback.bind(this));
    }

    path()
    {
        var path = [];
        var node = this;
        while (node && "index" in node && node._nodeName.length) {
            path.push([node.index, node._nodeName]);
            node = node.parentNode;
        }
        path.reverse();
        return path.join(",");
    }

    get escapedIdSelector()
    {
        let id = this.getAttribute("id");
        if (!id)
            return "";

        id = id.trim();
        if (!id.length)
            return "";

        id = CSS.escape(id);
        if (/[\s'"]/.test(id))
            return `[id="${id}"]`;

        return `#${id}`;
    }

    get escapedClassSelector()
    {
        let classes = this.getAttribute("class");
        if (!classes)
            return "";

        classes = classes.trim();
        if (!classes.length)
            return "";

        let foundClasses = new Set;
        return classes.split(/\s+/).reduce((selector, className) => {
            if (!className.length || foundClasses.has(className))
                return selector;

            foundClasses.add(className);
            return `${selector}.${CSS.escape(className)}`;
        }, "");
    }

    get displayName()
    {
        if (this.isPseudoElement())
            return "::" + this._pseudoType;
        return this.nodeNameInCorrectCase() + this.escapedIdSelector + this.escapedClassSelector;
    }

    appropriateSelectorFor(justSelector)
    {
        if (this.isPseudoElement())
            return this.parentNode.appropriateSelectorFor() + "::" + this._pseudoType;

        let lowerCaseName = this.localName() || this.nodeName().toLowerCase();

        let id = this.escapedIdSelector;
        if (id.length)
            return justSelector ? id : lowerCaseName + id;

        let classes = this.escapedClassSelector;
        if (classes.length)
            return justSelector ? classes : lowerCaseName + classes;

        if (lowerCaseName === "input" && this.getAttribute("type"))
            return lowerCaseName + "[type=\"" + this.getAttribute("type") + "\"]";

        return lowerCaseName;
    }

    isAncestor(node)
    {
        if (!node)
            return false;

        var currentNode = node.parentNode;
        while (currentNode) {
            if (this === currentNode)
                return true;
            currentNode = currentNode.parentNode;
        }
        return false;
    }

    isDescendant(descendant)
    {
        return descendant !== null && descendant.isAncestor(this);
    }

    get ownerSVGElement()
    {
        if (this._nodeName === "svg")
            return this;

        if (!this.parentNode)
            return null;

        return this.parentNode.ownerSVGElement;
    }

    isSVGElement()
    {
        return !!this.ownerSVGElement;
    }

    didFireEvent(eventName, timestamp, data)
    {
        // Called from WI.DOMManager.

        this._addDOMEvent({
            eventName,
            timestamp: WI.timelineManager.computeElapsedTime(timestamp),
            data,
        });
    }

    videoLowPowerChanged(timestamp, isLowPower)
    {
        // Called from WI.DOMManager.

        console.assert(this.canEnterLowPowerMode());

        let lastValue = this._lowPowerRanges.lastValue;

        if (isLowPower) {
            console.assert(!lastValue || lastValue.endTimestamp);
            if (!lastValue || lastValue.endTimestamp)
                this._lowPowerRanges.push({startTimestamp: timestamp});
        } else {
            console.assert(!lastValue || lastValue.startTimestamp);
            if (!lastValue)
                this._lowPowerRanges.push({endTimestamp: timestamp});
            else if (lastValue.startTimestamp)
                lastValue.endTimestamp = timestamp;
        }

        this.dispatchEventToListeners(WI.DOMNode.Event.LowPowerChanged, {isLowPower, timestamp});
    }

    canEnterLowPowerMode()
    {
        return this.localName() === "video" || this.nodeName().toLowerCase() === "video";
    }

    _handleDOMNodeDidFireEvent(event)
    {
        if (event.target === this || !event.target.isAncestor(this))
            return;

        let domEvent = Object.shallowCopy(event.data.domEvent);
        domEvent.originator = event.target;

        this._addDOMEvent(domEvent);
    }

    _addDOMEvent(domEvent)
    {
        this._domEvents.push(domEvent);

        this.dispatchEventToListeners(WI.DOMNode.Event.DidFireEvent, {domEvent});
    }

    _shouldListenForEventListeners()
    {
        let lowerCaseName = this.localName() || this.nodeName().toLowerCase();
        return lowerCaseName === "video" || lowerCaseName === "audio";
    }

    _setAttributesPayload(attrs)
    {
        this._attributes = [];
        this._attributesMap = new Map;
        for (var i = 0; i < attrs.length; i += 2)
            this._addAttribute(attrs[i], attrs[i + 1]);
    }

    _insertChild(prev, payload)
    {
        var node = new WI.DOMNode(this._domManager, this.ownerDocument, this._isInShadowTree, payload);
        if (!prev) {
            if (!this._children) {
                // First node
                this._children = this._shadowRoots.concat([node]);
            } else
                this._children.unshift(node);
        } else
            this._children.splice(this._children.indexOf(prev) + 1, 0, node);
        this._renumber();
        return node;
    }

    _removeChild(node)
    {
        // FIXME: Handle removal if this is a shadow root.
        if (node.isPseudoElement()) {
            this._pseudoElements.delete(node.pseudoType());
            node.parentNode = null;
        } else {
            this._children.splice(this._children.indexOf(node), 1);
            node.parentNode = null;
            this._renumber();
        }
    }

    _setChildrenPayload(payloads)
    {
        // We set children in the constructor.
        if (this._contentDocument)
            return;

        this._children = this._shadowRoots.slice();
        for (var i = 0; i < payloads.length; ++i) {
            var node = new WI.DOMNode(this._domManager, this.ownerDocument, this._isInShadowTree, payloads[i]);
            this._children.push(node);
        }
        this._renumber();
    }

    _renumber()
    {
        this._filteredChildrenNeedsUpdating = true;

        var childNodeCount = this._children.length;
        if (childNodeCount === 0)
            return;

        for (var i = 0; i < childNodeCount; ++i) {
            var child = this._children[i];
            child.index = i;
            child._nextSibling = i + 1 < childNodeCount ? this._children[i + 1] : null;
            child._previousSibling = i - 1 >= 0 ? this._children[i - 1] : null;
            child.parentNode = this;
        }
    }

    _addAttribute(name, value)
    {
        let attr = {name, value, _node: this};
        this._attributesMap.set(name, attr);
        this._attributes.push(attr);
    }

    _setAttribute(name, value)
    {
        let attr = this._attributesMap.get(name);
        if (attr)
            attr.value = value;
        else
            this._addAttribute(name, value);
    }

    _removeAttribute(name)
    {
        let attr = this._attributesMap.get(name);
        if (attr) {
            this._attributes.remove(attr);
            this._attributesMap.delete(name);
        }
    }

    moveTo(targetNode, anchorNode, callback)
    {
        DOMAgent.moveTo(this.id, targetNode.id, anchorNode ? anchorNode.id : undefined, this._makeUndoableCallback(callback));
    }

    isXMLNode()
    {
        return !!this.ownerDocument && !!this.ownerDocument.xmlVersion;
    }

    get enabledPseudoClasses()
    {
        return this._enabledPseudoClasses;
    }

    setPseudoClassEnabled(pseudoClass, enabled)
    {
        var pseudoClasses = this._enabledPseudoClasses;
        if (enabled) {
            if (pseudoClasses.includes(pseudoClass))
                return;
            pseudoClasses.push(pseudoClass);
        } else {
            if (!pseudoClasses.includes(pseudoClass))
                return;
            pseudoClasses.remove(pseudoClass);
        }

        function changed(error)
        {
            if (!error)
                this.dispatchEventToListeners(WI.DOMNode.Event.EnabledPseudoClassesChanged);
        }

        CSSAgent.forcePseudoState(this.id, pseudoClasses, changed.bind(this));
    }

    _makeUndoableCallback(callback)
    {
        return function(error)
        {
            if (!error)
                DOMAgent.markUndoableState();

            if (callback)
                callback.apply(null, arguments);
        };
    }
};

WI.DOMNode.Event = {
    EnabledPseudoClassesChanged: "dom-node-enabled-pseudo-classes-did-change",
    AttributeModified: "dom-node-attribute-modified",
    AttributeRemoved: "dom-node-attribute-removed",
    EventListenersChanged: "dom-node-event-listeners-changed",
    DidFireEvent: "dom-node-did-fire-event",
    LowPowerChanged: "dom-node-video-low-power-changed",
};

WI.DOMNode.PseudoElementType = {
    Before: "before",
    After: "after",
};

WI.DOMNode.ShadowRootType = {
    UserAgent: "user-agent",
    Closed: "closed",
    Open: "open",
};

WI.DOMNode.CustomElementState = {
    Builtin: "builtin",
    Custom: "custom",
    Waiting: "waiting",
    Failed: "failed",
};
