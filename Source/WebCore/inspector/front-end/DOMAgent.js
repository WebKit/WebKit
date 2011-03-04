/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

WebInspector.DOMNode = function(doc, payload) {
    this.ownerDocument = doc;

    this.id = payload.id;
    this.nodeType = payload.nodeType;
    this.nodeName = payload.nodeName;
    this.localName = payload.localName;
    this._nodeValue = payload.nodeValue;
    this.textContent = this.nodeValue;

    this.attributes = [];
    this._attributesMap = {};
    if (payload.attributes)
        this._setAttributesPayload(payload.attributes);

    this._childNodeCount = payload.childNodeCount;
    this.children = null;

    this.nextSibling = null;
    this.prevSibling = null;
    this.firstChild = null;
    this.lastChild = null;
    this.parentNode = null;

    if (payload.children)
        this._setChildrenPayload(payload.children);

    this._computedStyle = null;
    this.style = null;
    this._matchedCSSRules = [];

    this.breakpoints = {};

    if (this.nodeType === Node.ELEMENT_NODE) {
        // HTML and BODY from internal iframes should not overwrite top-level ones.
        if (!this.ownerDocument.documentElement && this.nodeName === "HTML")
            this.ownerDocument.documentElement = this;
        if (!this.ownerDocument.body && this.nodeName === "BODY")
            this.ownerDocument.body = this;
        if (payload.documentURL)
            this.documentURL = payload.documentURL;
    } else if (this.nodeType === Node.DOCUMENT_TYPE_NODE) {
        this.publicId = payload.publicId;
        this.systemId = payload.systemId;
        this.internalSubset = payload.internalSubset;
    } else if (this.nodeType === Node.DOCUMENT_NODE) {
        this.documentURL = payload.documentURL;
    } else if (this.nodeType === Node.ATTRIBUTE_NODE) {
        this.name = payload.name;
        this.value = payload.value;
    }
}

WebInspector.DOMNode.prototype = {
    hasAttributes: function()
    {
        return this.attributes.length > 0;
    },

    hasChildNodes: function()
    {
        return this._childNodeCount > 0;
    },

    get nodeValue() {
        return this._nodeValue;
    },

    set nodeValue(value) {
        if (this.nodeType != Node.TEXT_NODE)
            return;
        this.ownerDocument._domAgent.setTextNodeValueAsync(this, value, function() {});
    },

    getAttribute: function(name)
    {
        var attr = this._attributesMap[name];
        return attr ? attr.value : undefined;
    },

    setAttribute: function(name, value)
    {
        var self = this;
        var callback = function()
        {
            var attr = self._attributesMap[name];
            if (attr)
                attr.value = value;
            else
                attr = self._addAttribute(name, value);
        };
        this.ownerDocument._domAgent.setAttributeAsync(this, name, value, callback);
    },

    removeAttribute: function(name)
    {
        var self = this;
        var callback = function()
        {
            delete self._attributesMap[name];
            for (var i = 0;  i < self.attributes.length; ++i) {
                if (self.attributes[i].name == name) {
                    self.attributes.splice(i, 1);
                    break;
                }
            }
        };
        this.ownerDocument._domAgent.removeAttributeAsync(this, name, callback);
    },

    path: function()
    {
        var path = [];
        var node = this;
        while (node && "index" in node && node.nodeName.length) {
            path.push([node.index, node.nodeName]);
            node = node.parentNode;
        }
        path.reverse();
        return path.join(",");
    },

    _setAttributesPayload: function(attrs)
    {
        this.attributes = [];
        this._attributesMap = {};
        for (var i = 0; i < attrs.length; i += 2)
            this._addAttribute(attrs[i], attrs[i + 1]);
    },

    _insertChild: function(prev, payload)
    {
        var node = new WebInspector.DOMNode(this.ownerDocument, payload);
        if (!prev) {
            if (!this.children) {
                // First node
                this.children = [ node ];
            } else
                this.children.unshift(node);
        } else
            this.children.splice(this.children.indexOf(prev) + 1, 0, node);
        this._renumber();
        return node;
    },

    removeChild_: function(node)
    {
        this.children.splice(this.children.indexOf(node), 1);
        node.parentNode = null;
        this._renumber();
    },

    _setChildrenPayload: function(payloads)
    {
        this.children = [];
        for (var i = 0; i < payloads.length; ++i) {
            var payload = payloads[i];
            var node = new WebInspector.DOMNode(this.ownerDocument, payload);
            this.children.push(node);
        }
        this._renumber();
    },

    _renumber: function()
    {
        this._childNodeCount = this.children.length;
        if (this._childNodeCount == 0) {
            this.firstChild = null;
            this.lastChild = null;
            return;
        }
        this.firstChild = this.children[0];
        this.lastChild = this.children[this._childNodeCount - 1];
        for (var i = 0; i < this._childNodeCount; ++i) {
            var child = this.children[i];
            child.index = i;
            child.nextSibling = i + 1 < this._childNodeCount ? this.children[i + 1] : null;
            child.prevSibling = i - 1 >= 0 ? this.children[i - 1] : null;
            child.parentNode = this;
        }
    },

    _addAttribute: function(name, value)
    {
        var attr = {
            "name": name,
            "value": value,
            "_node": this
        };
        this._attributesMap[name] = attr;
        this.attributes.push(attr);
    }
}

WebInspector.DOMDocument = function(domAgent, payload)
{
    WebInspector.DOMNode.call(this, this, payload);
    this._listeners = {};
    this._domAgent = domAgent;
}

WebInspector.DOMDocument.prototype = {

    addEventListener: function(name, callback)
    {
        var listeners = this._listeners[name];
        if (!listeners) {
            listeners = [];
            this._listeners[name] = listeners;
        }
        listeners.push(callback);
    },

    removeEventListener: function(name, callback)
    {
        var listeners = this._listeners[name];
        if (!listeners)
            return;

        var index = listeners.indexOf(callback);
        if (index != -1)
            listeners.splice(index, 1);
    },

    _fireDomEvent: function(name, event)
    {
        var listeners = this._listeners[name];
        if (!listeners)
            return;

        for (var i = 0; i < listeners.length; ++i) {
            var listener = listeners[i];
            listener.call(this, event);
        }
    }
}

WebInspector.DOMDocument.prototype.__proto__ = WebInspector.DOMNode.prototype;

WebInspector.DOMAgent = function() {
    this._idToDOMNode = null;
    this._document = null;
    InspectorBackend.registerDomainDispatcher("DOM", new WebInspector.DOMDispatcher(this));
}

WebInspector.DOMAgent.prototype = {
    requestDocument: function(callback)
    {
        if (this._document) {
            if (callback)
                callback(this._document);
        } else
            this._documentUpdated(callback);
    },

    pushNodeToFrontend: function(objectId, callback)
    {
        function mycallback()
        {
            if (this._document)
                DOMAgent.pushNodeToFrontend(objectId, callback);
            else {
                if (callback)
                    callback(0);
            }
        }
        this.requestDocument(mycallback.bind(this));
    },

    pushNodeByPathToFrontend: function(path, callback)
    {
        function mycallback()
        {
            if (this._document)
                DOMAgent.pushNodeByPathToFrontend(path, callback);
            else {
                if (callback)
                    callback(0);
            }
        }
        this.requestDocument(mycallback.bind(this));
    },

    getChildNodesAsync: function(parent, callback)
    {
        var children = parent.children;
        if (children) {
            callback(children);
            return;
        }
        function mycallback() {
            callback(parent.children);
        }
        DOMAgent.getChildNodes(parent.id, mycallback);
    },

    setAttributeAsync: function(node, name, value, callback)
    {
        var mycallback = this._didApplyDomChange.bind(this, node, callback);
        DOMAgent.setAttribute(node.id, name, value, mycallback);
    },

    removeAttributeAsync: function(node, name, callback)
    {
        var mycallback = this._didApplyDomChange.bind(this, node, callback);
        DOMAgent.removeAttribute(node.id, name, mycallback);
    },

    setTextNodeValueAsync: function(node, text, callback)
    {
        var mycallback = this._didApplyDomChange.bind(this, node, callback);
        DOMAgent.setTextNodeValue(node.id, text, mycallback);
    },

    _didApplyDomChange: function(node, callback, success)
    {
        if (!success)
            return;
        callback();
        // TODO(pfeldman): Fix this hack.
        var elem = WebInspector.panels.elements.treeOutline.findTreeElement(node);
        if (elem)
            elem.updateTitle();
    },

    _attributesUpdated: function(nodeId, attrsArray)
    {
        var node = this._idToDOMNode[nodeId];
        node._setAttributesPayload(attrsArray);
        var event = {target: node};
        this._document._fireDomEvent("DOMAttrModified", event);
    },

    _characterDataModified: function(nodeId, newValue)
    {
        var node = this._idToDOMNode[nodeId];
        node._nodeValue = newValue;
        node.textContent = newValue;
        var event = { target : node };
        this._document._fireDomEvent("DOMCharacterDataModified", event);
    },

    nodeForId: function(nodeId)
    {
        return this._idToDOMNode[nodeId];
    },

    _documentUpdated: function(callback)
    {
        function mycallback(root)
        {
            this._setDocument(root);
            if (callback)
                callback(this._document);
        }
        DOMAgent.getDocument(mycallback.bind(this));
    },

    _setDocument: function(payload)
    {
        this._idToDOMNode = {};
        if (payload && "id" in payload) {
            this._document = new WebInspector.DOMDocument(this, payload);
            this._idToDOMNode[payload.id] = this._document;
            this._bindNodes(this._document.children);
            WebInspector.breakpointManager.restoreDOMBreakpoints();
        } else
            this._document = null;
        WebInspector.panels.elements.setDocument(this._document);
    },

    _setDetachedRoot: function(payload)
    {
        var root = new WebInspector.DOMNode(this._document, payload);
        this._idToDOMNode[payload.id] = root;
    },

    _setChildNodes: function(parentId, payloads)
    {
        if (!parentId && payloads.length) {
            this._setDetachedRoot(payloads[0]);
            return;
        }

        var parent = this._idToDOMNode[parentId];
        parent._setChildrenPayload(payloads);
        this._bindNodes(parent.children);
    },

    _bindNodes: function(children)
    {
        for (var i = 0; i < children.length; ++i) {
            var child = children[i];
            this._idToDOMNode[child.id] = child;
            if (child.children)
                this._bindNodes(child.children);
        }
    },

    _childNodeCountUpdated: function(nodeId, newValue)
    {
        var node = this._idToDOMNode[nodeId];
        node._childNodeCount = newValue;
        var outline = WebInspector.panels.elements.treeOutline;
        var treeElement = outline.findTreeElement(node);
        if (treeElement)
            treeElement.hasChildren = newValue;
    },

    _childNodeInserted: function(parentId, prevId, payload)
    {
        var parent = this._idToDOMNode[parentId];
        var prev = this._idToDOMNode[prevId];
        var node = parent._insertChild(prev, payload);
        this._idToDOMNode[node.id] = node;
        var event = { target : node, relatedNode : parent };
        this._document._fireDomEvent("DOMNodeInserted", event);
    },

    _childNodeRemoved: function(parentId, nodeId)
    {
        var parent = this._idToDOMNode[parentId];
        var node = this._idToDOMNode[nodeId];
        parent.removeChild_(node);
        var event = { target : node, relatedNode : parent };
        this._document._fireDomEvent("DOMNodeRemoved", event);
        delete this._idToDOMNode[nodeId];
        this._removeBreakpoints(node);
    },

    _removeBreakpoints: function(node)
    {
        for (var type in node.breakpoints)
            node.breakpoints[type].remove();
        if (!node.children)
            return;
        for (var i = 0; i < node.children.length; ++i)
            this._removeBreakpoints(node.children[i]);
    }
}

WebInspector.DOMDispatcher = function(domAgent)
{
    this._domAgent = domAgent;
}

WebInspector.DOMDispatcher.prototype = {
    documentUpdated: function()
    {
        this._domAgent._documentUpdated();
    },

    attributesUpdated: function(nodeId, attrsArray)
    {
        this._domAgent._attributesUpdated(nodeId, attrsArray);
    },

    characterDataModified: function(nodeId, newValue)
    {
        this._domAgent._characterDataModified(nodeId, newValue);
    },

    setChildNodes: function(parentId, payloads)
    {
        this._domAgent._setChildNodes(parentId, payloads);
    },

    childNodeCountUpdated: function(nodeId, newValue)
    {
        this._domAgent._childNodeCountUpdated(nodeId, newValue);
    },

    childNodeInserted: function(parentId, prevId, payload)
    {
        this._domAgent._childNodeInserted(parentId, prevId, payload);
    },

    childNodeRemoved: function(parentId, nodeId)
    {
        this._domAgent._childNodeRemoved(parentId, nodeId);
    },

    inspectElementRequested: function(nodeId)
    {
        WebInspector.updateFocusedNode(nodeId);
    },

    addNodesToSearchResult: function(nodeIds)
    {
        WebInspector.panels.elements.addNodesToSearchResult(nodeIds);
    }
}

WebInspector.ApplicationCacheDispatcher = function()
{
}

WebInspector.ApplicationCacheDispatcher.getApplicationCachesAsync = function(callback)
{
    function mycallback(applicationCaches)
    {
        // FIXME: Currently, this list only returns a single application cache.
        if (applicationCaches)
            callback(applicationCaches);
    }

    ApplicationCacheAgent.getApplicationCaches(mycallback);
}

WebInspector.ApplicationCacheDispatcher.prototype = {
    updateApplicationCacheStatus: function(status)
    {
        WebInspector.panels.resources.updateApplicationCacheStatus(status);
    },

    updateNetworkState: function(isNowOnline)
    {
        WebInspector.panels.resources.updateNetworkState(isNowOnline);
    }
}

InspectorBackend.registerDomainDispatcher("ApplicationCache", new WebInspector.ApplicationCacheDispatcher());

WebInspector.Cookies = {}

WebInspector.Cookies.getCookiesAsync = function(callback)
{
    function mycallback(cookies, cookiesString)
    {
        if (cookiesString)
            callback(WebInspector.Cookies.buildCookiesFromString(cookiesString), false);
        else
            callback(cookies, true);
    }

    InspectorAgent.getCookies(mycallback);
}

WebInspector.Cookies.buildCookiesFromString = function(rawCookieString)
{
    var rawCookies = rawCookieString.split(/;\s*/);
    var cookies = [];

    if (!(/^\s*$/.test(rawCookieString))) {
        for (var i = 0; i < rawCookies.length; ++i) {
            var cookie = rawCookies[i];
            var delimIndex = cookie.indexOf("=");
            var name = cookie.substring(0, delimIndex);
            var value = cookie.substring(delimIndex + 1);
            var size = name.length + value.length;
            cookies.push({ name: name, value: value, size: size });
        }
    }

    return cookies;
}

WebInspector.Cookies.cookieMatchesResourceURL = function(cookie, resourceURL)
{
    var url = resourceURL.asParsedURL();
    if (!url || !this.cookieDomainMatchesResourceDomain(cookie.domain, url.host))
        return false;
    return (url.path.indexOf(cookie.path) === 0
        && (!cookie.port || url.port == cookie.port)
        && (!cookie.secure || url.scheme === "https"));
}

WebInspector.Cookies.cookieDomainMatchesResourceDomain = function(cookieDomain, resourceDomain)
{
    if (cookieDomain.charAt(0) !== '.')
        return resourceDomain === cookieDomain;
    return !!resourceDomain.match(new RegExp("^([^\\.]+\\.)?" + cookieDomain.substring(1).escapeForRegExp() + "$"), "i");
}

WebInspector.EventListeners = {}

WebInspector.EventListeners.getEventListenersForNodeAsync = function(node, callback)
{
    if (!node)
        return;
    DOMAgent.getEventListenersForNode(node.id, callback);
}
