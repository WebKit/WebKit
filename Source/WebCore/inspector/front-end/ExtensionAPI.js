/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

function defineCommonExtensionSymbols(apiPrivate)
{
    if (!apiPrivate.audits)
        apiPrivate.audits = {};

    apiPrivate.audits.Severity = {
        Info: "info",
        Warning: "warning",
        Severe: "severe"
    };

    if (!apiPrivate.console)
        apiPrivate.console = {};
    apiPrivate.console.Severity = {
        Tip: "tip",
        Debug: "debug",
        Log: "log",
        Warning: "warning",
        Error: "error"
    };
}

function injectedExtensionAPI(injectedScriptId)
{

var apiPrivate = {};

defineCommonExtensionSymbols(apiPrivate);

// Here and below, all constructors are private to API implementation.
// For a public type Foo, if internal fields are present, these are on
// a private FooImpl type, an instance of FooImpl is used in a closure
// by Foo consutrctor to re-bind publicly exported members to an instance
// of Foo.

/**
 * @constructor
 */
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
            throw "addListener: callback is not a function";
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

/**
 * @constructor
 */
function InspectorExtensionAPI()
{
    this.audits = new Audits();
    this.inspectedWindow = new InspectedWindow();
    this.panels = new Panels();
    this.network = new Network();
    defineDeprecatedProperty(this, "webInspector", "resources", "network");
    this.timeline = new Timeline();
    this.console = new Console();

    this.onReset = new EventSink("reset");
}

/**
 * @constructor
 */
InspectorExtensionAPI.prototype = {
    log: function(message)
    {
        extensionServer.sendRequest({ command: "log", message: message });
    }
}

/**
 * @constructor
 */
function Console()
{
    this.onMessageAdded = new EventSink("console-message-added");
}

Console.prototype = {
    getMessages: function(callback)
    {
        extensionServer.sendRequest({ command: "getConsoleMessages" }, callback);
    },

    addMessage: function(severity, text, url, line)
    {
        extensionServer.sendRequest({ command: "addConsoleMessage", severity: severity, text: text, url: url, line: line });
    },

    get Severity()
    {
        return apiPrivate.console.Severity;
    }
}

/**
 * @constructor
 */
function Network()
{
    function dispatchRequestEvent(message)
    {
        var request = message.arguments[1];
        request.__proto__ = new Request(message.arguments[0]);
        this._fire(request);
    }
    this.onRequestFinished = new EventSink("network-request-finished", dispatchRequestEvent);
    defineDeprecatedProperty(this, "network", "onFinished", "onRequestFinished");
    this.onNavigated = new EventSink("inspectedURLChanged");
}

Network.prototype = {
    getHAR: function(callback)
    {
        function callbackWrapper(result)
        {
            var entries = (result && result.entries) || [];
            for (var i = 0; i < entries.length; ++i) {
                entries[i].__proto__ = new Request(entries[i]._requestId);
                delete entries[i]._requestId;
            }
            callback(result);
        }
        return extensionServer.sendRequest({ command: "getHAR" }, callback && callbackWrapper);
    },

    addRequestHeaders: function(headers)
    {
        return extensionServer.sendRequest({ command: "addRequestHeaders", headers: headers, extensionId: window.location.hostname });
    }
}

/**
 * @constructor
 */
function RequestImpl(id)
{
    this._id = id;
}

RequestImpl.prototype = {
    getContent: function(callback)
    {
        function callbackWrapper(response)
        {
            callback(response.content, response.encoding);
        }
        extensionServer.sendRequest({ command: "getRequestContent", id: this._id }, callback && callbackWrapper);
    }
}

/**
 * @constructor
 */
function Panels()
{
    var panels = {
        elements: new ElementsPanel()
    };

    function panelGetter(name)
    {
        return panels[name];
    }
    for (var panel in panels)
        this.__defineGetter__(panel, bind(panelGetter, null, panel));
}

Panels.prototype = {
    create: function(title, icon, page, callback)
    {
        var id = "extension-panel-" + extensionServer.nextObjectId();
        var request = {
            command: "createPanel",
            id: id,
            title: title,
            icon: icon,
            page: page
        };
        extensionServer.sendRequest(request, callback && bind(callback, this, new ExtensionPanel(id)));
    },

    setOpenResourceHandler: function(callback)
    {
        var hadHandler = extensionServer.hasHandler("open-resource");

        if (!callback)
            extensionServer.unregisterHandler("open-resource");
        else {
            function callbackWrapper(message)
            {
                callback.call(null, message.resource);
            }
            extensionServer.registerHandler("open-resource", callbackWrapper);
        }
        // Only send command if we either removed an existing handler or added handler and had none before.
        if (hadHandler === !callback)
            extensionServer.sendRequest({ command: "setOpenResourceHandler", "handlerPresent": !!callback });
    }
}

/**
 * @constructor
 */
function ExtensionViewImpl(id)
{
    this._id = id;

    function dispatchShowEvent(message)
    {
        var frameIndex = message.arguments[0];
        this._fire(window.top.frames[frameIndex]);
    }
    this.onShown = new EventSink("view-shown-" + id, dispatchShowEvent);
    this.onHidden = new EventSink("view-hidden-" + id);
}

/**
 * @constructor
 */
function PanelWithSidebarImpl(id)
{
    this._id = id;
}

PanelWithSidebarImpl.prototype = {
    createSidebarPane: function(title, callback)
    {
        var id = "extension-sidebar-" + extensionServer.nextObjectId();
        var request = {
            command: "createSidebarPane",
            panel: this._id,
            id: id,
            title: title
        };
        function callbackWrapper()
        {
            callback(new ExtensionSidebarPane(id));
        }
        extensionServer.sendRequest(request, callback && callbackWrapper);
    }
}

PanelWithSidebarImpl.prototype.__proto__ = ExtensionViewImpl.prototype;

/**
 * @constructor
 * @extends {PanelWithSidebar}
 */
function ElementsPanel()
{
    var id = "elements";
    PanelWithSidebar.call(this, id);
    this.onSelectionChanged = new EventSink("panel-objectSelected-" + id);
}

/**
 * @constructor
 * @extends {ExtensionViewImpl}
 */
function ExtensionPanelImpl(id)
{
    ExtensionViewImpl.call(this, id);
    this.onSearch = new EventSink("panel-search-" + id);
}

ExtensionPanelImpl.prototype.__proto__ = ExtensionViewImpl.prototype;

/**
 * @constructor
 * @extends {ExtensionViewImpl}
 */
function ExtensionSidebarPaneImpl(id)
{
    ExtensionViewImpl.call(this, id);
    this.onUpdated = new EventSink("sidebar-updated-" + id);
}

ExtensionSidebarPaneImpl.prototype = {
    setHeight: function(height)
    {
        extensionServer.sendRequest({ command: "setSidebarHeight", id: this._id, height: height });
    },

    setExpression: function(expression, rootTitle)
    {
        extensionServer.sendRequest({ command: "setSidebarContent", id: this._id, expression: expression, rootTitle: rootTitle, evaluateOnPage: true });
    },

    setObject: function(jsonObject, rootTitle)
    {
        extensionServer.sendRequest({ command: "setSidebarContent", id: this._id, expression: jsonObject, rootTitle: rootTitle });
    },

    setPage: function(page)
    {
        extensionServer.sendRequest({ command: "setSidebarPage", id: this._id, page: page });
    }
}

/**
 * @constructor
 */
function Audits()
{
}

Audits.prototype = {
    addCategory: function(displayName, resultCount)
    {
        var id = "extension-audit-category-" + extensionServer.nextObjectId();
        extensionServer.sendRequest({ command: "addAuditCategory", id: id, displayName: displayName, resultCount: resultCount });
        return new AuditCategory(id);
    }
}

/**
 * @constructor
 */
function AuditCategoryImpl(id)
{
    function dispatchAuditEvent(request)
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
    this.onAuditStarted = new EventSink("audit-started-" + id, dispatchAuditEvent);
}

/**
 * @constructor
 */
function AuditResultImpl(id)
{
    this._id = id;

    this.createURL = bind(this._nodeFactory, null, "url");
    this.createSnippet = bind(this._nodeFactory, null, "snippet");
    this.createText = bind(this._nodeFactory, null, "text");
    this.createResourceLink = bind(this._nodeFactory, null, "resourceLink");
}

AuditResultImpl.prototype = {
    addResult: function(displayName, description, severity, details)
    {
        // shorthand for specifying details directly in addResult().
        if (details && !(details instanceof AuditResultNode))
            details = new AuditResultNode(details instanceof Array ? details : [details]);

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
        return new AuditResultNode(Array.prototype.slice.call(arguments));
    },

    done: function()
    {
        extensionServer.sendRequest({ command: "stopAuditCategoryRun", resultId: this._id });
    },

    get Severity()
    {
        return apiPrivate.audits.Severity;
    },

    _nodeFactory: function(type)
    {
        return {
            type: type,
            arguments: Array.prototype.slice.call(arguments, 1)
        };
    }
}

/**
 * @constructor
 */
function AuditResultNode(contents)
{
    this.contents = contents;
    this.children = [];
    this.expanded = false;
}

AuditResultNode.prototype = {
    addChild: function()
    {
        var node = new AuditResultNode(Array.prototype.slice.call(arguments));
        this.children.push(node);
        return node;
    }
};

/**
 * @constructor
 */
function InspectedWindow()
{
    function dispatchResourceEvent(message)
    {
        this._fire(new Resource(message.arguments[0]));
    }
    function dispatchResourceContentEvent(message)
    {
        this._fire(new Resource(message.arguments[0]), message.arguments[1]);
    }
    this.onResourceAdded = new EventSink("resource-added", dispatchResourceEvent);
    this.onResourceContentCommitted = new EventSink("resource-content-committed", dispatchResourceContentEvent);
}

InspectedWindow.prototype = {
    reload: function(optionsOrUserAgent)
    {
        var options = null;
        if (typeof optionsOrUserAgent === "object")
            options = optionsOrUserAgent;
        else if (typeof optionsOrUserAgent === "string") {
            options = { userAgent: optionsOrUserAgent };
            console.warn("Passing userAgent as string parameter to inspectedWindow.reload() is deprecated. " +
                         "Use inspectedWindow.reload({ userAgent: value}) instead.");
        }
        return extensionServer.sendRequest({ command: "reload", options: options });
    },

    eval: function(expression, callback)
    {
        function callbackWrapper(result)
        {
            callback(result.value, result.isException);
        }
        return extensionServer.sendRequest({ command: "evaluateOnInspectedPage", expression: expression }, callback && callbackWrapper);
    },

    getResources: function(callback)
    {
        function wrapResource(resourceData)
        {
            return new Resource(resourceData);
        }
        function callbackWrapper(resources)
        {
            callback(resources.map(wrapResource));
        }
        return extensionServer.sendRequest({ command: "getPageResources" }, callback && callbackWrapper);
    }
}

/**
 * @constructor
 */
function ResourceImpl(resourceData)
{
    this._url = resourceData.url
    this._type = resourceData.type;
}

ResourceImpl.prototype = {
    get url()
    {
        return this._url;
    },

    get type()
    {
        return this._type;
    },

    getContent: function(callback)
    {
        function callbackWrapper(response)
        {
            callback(response.content, response.encoding);
        }

        return extensionServer.sendRequest({ command: "getResourceContent", url: this._url }, callback && callbackWrapper);
    },

    setContent: function(content, commit, callback)
    {
        return extensionServer.sendRequest({ command: "setResourceContent", url: this._url, content: content, commit: commit }, callback);
    }
}

/**
 * @constructor
 */
function TimelineImpl()
{
    this.onEventRecorded = new EventSink("timeline-event-recorded");
}

/**
 * @constructor
 */
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

    hasHandler: function(command)
    {
        return !!this._handlers[command];
    },

    registerHandler: function(command, handler)
    {
        this._handlers[command] = handler;
    },

    unregisterHandler: function(command)
    {
        delete this._handlers[command];
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
            var callback = this._callbacks[request.requestId];
            delete this._callbacks[request.requestId];
            callback(request.result);
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

/**
 * @param {...*} vararg
 */
function bind(func, thisObject, vararg)
{
    var args = Array.prototype.slice.call(arguments, 2);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))); };
}

function populateInterfaceClass(interface, implementation)
{
    for (var member in implementation) {
        if (member.charAt(0) === "_")
            continue;
        var descriptor = null;
        // Traverse prototype chain until we find the owner.
        for (var owner = implementation; owner && !descriptor; owner = owner.__proto__)
            descriptor = Object.getOwnPropertyDescriptor(owner, member);
        if (!descriptor)
            continue;
        if (typeof descriptor.value === "function")
            interface[member] = bind(descriptor.value, implementation);
        else if (typeof descriptor.get === "function")
            interface.__defineGetter__(member, bind(descriptor.get, implementation));
        else
            Object.defineProperty(interface, member, descriptor);
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

function defineDeprecatedProperty(object, className, oldName, newName)
{
    var warningGiven = false;
    function getter()
    {
        if (!warningGiven) {
            console.warn(className + "." + oldName + " is deprecated. Use " + className + "." + newName + " instead");
            warningGiven = true;
        }
        return object[newName];
    }
    object.__defineGetter__(oldName, getter);
}

var AuditCategory = declareInterfaceClass(AuditCategoryImpl);
var AuditResult = declareInterfaceClass(AuditResultImpl);
var EventSink = declareInterfaceClass(EventSinkImpl);
var ExtensionPanel = declareInterfaceClass(ExtensionPanelImpl);
var ExtensionSidebarPane = declareInterfaceClass(ExtensionSidebarPaneImpl);
var PanelWithSidebar = declareInterfaceClass(PanelWithSidebarImpl);
var Request = declareInterfaceClass(RequestImpl);
var Resource = declareInterfaceClass(ResourceImpl);
var Timeline = declareInterfaceClass(TimelineImpl);

var extensionServer = new ExtensionServerClient();

window.webInspector = new InspectorExtensionAPI();
window.experimental = window.experimental || {};
window.experimental.webInspector = window.webInspector;

}

function buildExtensionAPIInjectedScript(platformAPI)
{
    return "(function(injectedScriptHost, inspectedWindow, injectedScriptId){ " +
        defineCommonExtensionSymbols.toString() + ";" +
        injectedExtensionAPI.toString() + ";" +
        "injectedExtensionAPI(injectedScriptId);" +
        (platformAPI || "") + ";" +
        "return {};" +
        "})";
}
