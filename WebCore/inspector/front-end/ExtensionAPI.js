/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

WebInspector.injectedExtensionAPI = function(InjectedScriptHost, inspectedWindow, injectedScriptId)
{

// Here and below, all constructors are private to API implementation.
// For a public type Foo, if internal fields are present, these are on
// a private FooImpl type, an instance of FooImpl is used in a closure
// by Foo consutrctor to re-bind publicly exported members to an instance
// of Foo.

function EventSinkImpl(type, customDispatch)
{
    this._type = type;
    this._listeners = [];
    this._customDispatch = customDispatch;
}

EventSinkImpl.prototype = {
    addListener: function(callback)
    {
        if (typeof callback != "function")
            throw new "addListener: callback is not a function";
        if (this._listeners.length === 0)
            extensionServer.sendRequest({ command: "subscribe", type: this._type });
        this._listeners.push(callback);
        extensionServer.registerHandler("notify-" + this._type, bind(this._dispatch, this));
    },

    removeListener: function(callback)
    {
        var listeners = this._listeners;

        for (var i = 0; i < listeners.length; ++i) {
            if (listeners[i] === callback) {
                listeners.splice(i, 1);
                break;
            }
        }
        if (this._listeners.length === 0)
            extensionServer.sendRequest({ command: "unsubscribe", type: this._type });
    },

    _fire: function()
    {
        var listeners = this._listeners.slice();
        for (var i = 0; i < listeners.length; ++i)
            listeners[i].apply(null, arguments);
    },

    _dispatch: function(request)
    {
         if (this._customDispatch)
             this._customDispatch.call(this, request);
         else
             this._fire.apply(this, request.arguments);
    }
}

function InspectorExtensionAPI()
{
    this.audits = new Audits();
    this.inspectedWindow = new InspectedWindow();
    this.panels = new Panels();
    this.resources = new Resources();

    this.onReset = new EventSink("reset");
}

InspectorExtensionAPI.prototype = {
    log: function(message)
    {
        extensionServer.sendRequest({ command: "log", message: message });
    }
}

function Resources()
{
    this.onFinished = new EventSink("resource-finished");
}

Resources.prototype = {
    getAll: function(callback)
    {
        return extensionServer.sendRequest({ command: "getResources" }, callback);
    },

    get: function(id, callback)
    {
        return extensionServer.sendRequest({ command: "getResources", id: id }, callback);
    },

    getPageTimings: function(callback)
    {
        return extensionServer.sendRequest({ command: "getPageTimings" }, callback);
    },

    getContent: function(ids, callback)
    {
        return extensionServer.sendRequest({ command: "getResourceContent", ids: ids }, callback);
    }
}

var wellKnownPanelNames = [
    "elements",
    "scripts"
];

function Panels()
{
    var panels = [];
    function panelGetter(name)
    {
        return panels[name];
    }

    for (var i = 0; i < wellKnownPanelNames.length; ++i) {
        var name = wellKnownPanelNames[i];
        panels[name] = new Panel(name);
        this.__defineGetter__(name, bind(panelGetter, null, name));
    }
}

Panels.prototype = {
    create: function(label, pageURL, iconURL, callback)
    {
        var id = "extension-panel-" + extensionServer.nextObjectId();
        function callbackWrapper(result)
        {
            if (result.isError)
                callback(result);
            else {
                panel = new ExtensionPanel(id);
                callback(panel);
            }
        }
        var request = {
            command: "createPanel",
            id: id,
            label: label,
            url: expandURL(pageURL),
            icon: expandURL(iconURL)
        };
        extensionServer.sendRequest(request, callback && bind(callbackWrapper, this));
    }
}

function PanelImpl(id)
{
    this._id = id;
    this.onSelectionChanged = new EventSink("panel-objectSelected-" + id);
}

PanelImpl.prototype = {
    createSidebarPane: function(title, url, callback)
    {
        var id = "extension-sidebar-" + extensionServer.nextObjectId();
        function callbackWrapper(result)
        {
            if (result.isError)
                callback(result);
            else
                callback(new ExtensionSidebarPane(id));
        }
        extensionServer.sendRequest({ command: "createSidebarPane", panel: this._id, id: id, title: title, url: expandURL(url) }, callback && callbackWrapper);
    },

    createWatchExpressionSidebarPane: function(title, callback)
    {
        var id = "watch-sidebar-" + extensionServer.nextObjectId();
        function callbackWrapper(result)
        {
            if (result.isError)
                callback(result);
            else
                callback(new WatchExpressionSidebarPane(id));
        }
        extensionServer.sendRequest({ command: "createWatchExpressionSidebarPane", panel: this._id, id: id, title: title }, callback && callbackWrapper);
    }
}

function ExtensionPanel(id)
{
    Panel.call(this, id);
    this.onSearch = new EventSink("panel-search-" + id);
}

function ExtensionSidebarPaneImpl(id)
{
    this._id = id;
}

ExtensionSidebarPaneImpl.prototype = {
    setHeight: function(height)
    {
        extensionServer.sendRequest({ command: "setSidebarHeight", id: this._id, height: height });
    },

    setExpanded: function(expanded)
    {
        extensionServer.sendRequest({ command: "setSidebarExpanded", id: this._id, expanded: expanded });
    }
}

function WatchExpressionSidebarPaneImpl(id)
{
    ExtensionSidebarPaneImpl.call(this, id);
    this.onUpdated = new EventSink("watch-sidebar-updated-" + id);
}

WatchExpressionSidebarPaneImpl.prototype = {
    setExpression: function(expression, rootTitle)
    {
        extensionServer.sendRequest({ command: "setWatchSidebarContent", id: this._id, expression: expression, rootTitle: rootTitle, evaluateOnPage: true });
    },

    setObject: function(jsonObject, rootTitle)
    {
        extensionServer.sendRequest({ command: "setWatchSidebarContent", id: this._id, expression: jsonObject, rootTitle: rootTitle });
    }
}

WatchExpressionSidebarPaneImpl.prototype.__proto__ = ExtensionSidebarPaneImpl.prototype;

function WatchExpressionSidebarPane(id)
{
    var impl = new WatchExpressionSidebarPaneImpl(id);
    ExtensionSidebarPane.call(this, id, impl);
}

function Audits()
{
}

Audits.prototype = {
    addCategory: function(displayName, ruleCount)
    {
        var id = "extension-audit-category-" + extensionServer.nextObjectId();
        extensionServer.sendRequest({ command: "addAuditCategory", id: id, displayName: displayName, ruleCount: ruleCount });
        return new AuditCategory(id);
    }
}

function AuditCategoryImpl(id)
{
    function customDispatch(request)
    {
        var auditResult = new AuditResult(request.arguments[0]);
        try {
            this._fire(auditResult);
        } catch (e) {
            console.error("Uncaught exception in extension audit event handler: " + e);
            auditResult.done();
        }
    }
    this._id = id;
    this.onAuditStarted = new EventSink("audit-started-" + id, customDispatch);
}

function AuditResultImpl(id)
{
    this._id = id;

    var formatterTypes = [
        "url",
        "snippet",
        "text"
    ];
    for (var i = 0; i < formatterTypes.length; ++i)
        this[formatterTypes[i]] = bind(this._nodeFactory, null, formatterTypes[i]);
}

AuditResultImpl.prototype = {
    addResult: function(displayName, description, severity, details)
    {
        // shorthand for specifying details directly in addResult().
        if (details && !(details instanceof AuditResultNode))
            details = details instanceof Array ? this.createNode.apply(this, details) : this.createNode(details);

        var request = {
            command: "addAuditResult",
            resultId: this._id,
            displayName: displayName,
            description: description,
            severity: severity,
            details: details
        };
        extensionServer.sendRequest(request);
    },

    createResult: function()
    {
        var node = new AuditResultNode();
        node.contents = Array.prototype.slice.call(arguments);
        return node;
    },

    done: function()
    {
        extensionServer.sendRequest({ command: "stopAuditCategoryRun", resultId: this._id });
    },

    get Severity()
    {
        return private.audits.Severity;
    },

    _nodeFactory: function(type)
    {
        return {
            type: type,
            arguments: Array.prototype.slice.call(arguments, 1)
        };
    }
}

function AuditResultNode(contents)
{
    this.contents = contents;
    this.children = [];
    this.expanded = false;
}

AuditResultNode.prototype = {
    addChild: function()
    {
        var node = AuditResultImpl.prototype.createResult.apply(null, arguments);
        this.children.push(node);
        return node;
    }
};

function InspectedWindow()
{
    this.onLoaded = new EventSink("inspectedPageLoaded");
    this.onNavigated = new EventSink("inspectedURLChanged");
    this.onDOMContentLoaded = new EventSink("DOMContentLoaded");
}

InspectedWindow.prototype = {
    reload: function()
    {
        return extensionServer.sendRequest({ command: "reload" });
    },

    evaluate: function(expression, callback)
    {
        function callbackWrapper(result)
        {
            if (result && !result.isException)
                result.value = result.value === "undefined" ? undefined : JSON.parse(result.value);
            callback(result);
        }
        return extensionServer.sendRequest({ command: "evaluateOnInspectedPage", expression: expression }, callback && callbackWrapper);
    }
}

function ExtensionServerClient()
{
    this._callbacks = {};
    this._handlers = {};
    this._lastRequestId = 0;
    this._lastObjectId = 0;

    this.registerHandler("callback", bind(this._onCallback, this));

    var channel = new MessageChannel();
    this._port = channel.port1;
    this._port.addEventListener("message", bind(this._onMessage, this), false);
    this._port.start();

    top.postMessage("registerExtension", [ channel.port2 ], "*");
}

ExtensionServerClient.prototype = {
    sendRequest: function(message, callback)
    {
        if (typeof callback === "function")
            message.requestId = this._registerCallback(callback);
        return this._port.postMessage(message);
    },

    registerHandler: function(command, handler)
    {
        this._handlers[command] = handler;
    },

    nextObjectId: function()
    {
        return injectedScriptId + "_" + ++this._lastObjectId;
    },

    _registerCallback: function(callback)
    {
        var id = ++this._lastRequestId;
        this._callbacks[id] = callback;
        return id;
    },

    _onCallback: function(request)
    {
        if (request.requestId in this._callbacks) {
            this._callbacks[request.requestId](request.result);
            delete this._callbacks[request.requestId];
        }
    },

    _onMessage: function(event)
    {
        var request = event.data;
        var handler = this._handlers[request.command];
        if (handler)
            handler.call(this, request);
    }
}

function expandURL(url)
{
    if (!url)
        return url;
    if (/^[^/]+:/.exec(url)) // See if url has schema.
        return url;
    var baseURL = location.protocol + "//" + location.hostname + location.port;
    if (/^\//.exec(url))
        return baseURL + url;
    return baseURL + location.pathname.replace(/\/[^/]*$/,"/") + url;
}

function bind(func, thisObject)
{
    var args = Array.prototype.slice.call(arguments, 2);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))); };
}

function populateInterfaceClass(interface, implementation)
{
    for (var member in implementation) {
        if (member.charAt(0) === "_")
            continue;
        var value = implementation[member];
        interface[member] = typeof value === "function" ? bind(value, implementation)
            : interface[member] = implementation[member];
    }
}

function declareInterfaceClass(implConstructor)
{
    return function()
    {
        var impl = { __proto__: implConstructor.prototype };
        implConstructor.apply(impl, arguments);
        populateInterfaceClass(this, impl);
    }
}

var EventSink = declareInterfaceClass(EventSinkImpl);
var Panel = declareInterfaceClass(PanelImpl);
var ExtensionSidebarPane = declareInterfaceClass(ExtensionSidebarPaneImpl);
var WatchExpressionSidebarPane = declareInterfaceClass(WatchExpressionSidebarPaneImpl);
var AuditCategory = declareInterfaceClass(AuditCategoryImpl);
var AuditResult = declareInterfaceClass(AuditResultImpl);

var extensionServer = new ExtensionServerClient();

webInspector = new InspectorExtensionAPI();

}
