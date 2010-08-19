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

var injectedExtensionAPI = function(InjectedScriptHost, inspectedWindow, injectedScriptId)
{

// Here and below, all constructors are private to API implementation.
// For a public type Foo, if internal fields are present, these are on
// a private FooImpl type, an instance of FooImpl is used in a closure
// by Foo consutrctor to re-bind publicly exported members to an instance
// of Foo.

function EventSinkImpl(type)
{
    this._type = type;
    this._listeners = [];
}

EventSinkImpl.prototype = {
    addListener: function(callback)
    {
        if (this._listeners.length === 0)
            extensionServer.sendRequest({ command: "subscribe", type: this._type });
        this._listeners.push(callback);
        extensionServer.registerHandler("notify-" + this._type, bind(this._fire, this));
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

    _fire: function(request)
    {
        var listeners = this._listeners.slice();
        for (var i = 0; i < listeners.length; ++i)
            listeners[i].apply(null, request.arguments);
    }
}

function EventSink(type)
{
    var impl = new EventSinkImpl(type);
    this.addListener = bind(impl.addListener, impl);
    this.removeListener = bind(impl.removeListener, impl);
}

function InspectorExtensionAPI()
{
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
    }
}

function Panel(id)
{
    var impl = new PanelImpl(id);
    this.createSidebarPane = bind(impl.createSidebarPane, impl);
    this.onSelectionChanged = new EventSink("panel-objectSelected-" + id);
}

function ExtensionPanel(id)
{
    Panel.call(this, id);
    this.onSearch = new EventSink("panel-search-" + id);
}

ExtensionPanel.prototype = {
}

ExtensionPanel.prototype.__proto__ = Panel.prototype;

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

function ExtensionSidebarPane(id)
{
    var impl = new ExtensionSidebarPaneImpl(id);
    this.setHeight = bind(impl.setHeight, impl);
    this.setExpanded = bind(impl.setExpanded, impl);
}

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
        return extensionServer.sendRequest({ command: "evaluateOnInspectedPage", expression: expression }, callback);
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

var extensionServer = new ExtensionServerClient();

webInspector = new InspectorExtensionAPI();

}
