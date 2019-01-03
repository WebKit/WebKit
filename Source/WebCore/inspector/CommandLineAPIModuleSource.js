/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//# sourceURL=__InjectedScript_CommandLineAPIModuleSource.js

(function (InjectedScriptHost, inspectedWindow, injectedScriptId, injectedScript, RemoteObject, CommandLineAPIHost) {

// FIXME: <https://webkit.org/b/152294> Web Inspector: Parse InjectedScriptSource as a built-in to get guaranteed non-user-overridden built-ins

function bind(func, thisObject, ...outerArgs)
{
    return function(...innerArgs) {
        return func.apply(thisObject, outerArgs.concat(innerArgs));
    };
}

/**
 * @constructor
 * @param {CommandLineAPIImpl} commandLineAPIImpl
 * @param {Object} callFrame
 */
function CommandLineAPI(commandLineAPIImpl, callFrame)
{
    this.$_ = injectedScript._lastResult;
    this.$exception = injectedScript._exceptionValue;

    // $0
    this.__defineGetter__("$0", bind(commandLineAPIImpl._inspectedObject, commandLineAPIImpl));

    // $1-$99
    for (let i = 1; i <= injectedScript._savedResults.length; ++i)
        this.__defineGetter__("$" + i, bind(injectedScript._savedResult, injectedScript, i));

    // Command Line API methods.
    for (let i = 0; i < CommandLineAPI.methods.length; ++i) {
        let method = CommandLineAPI.methods[i];
        this[method] = bind(commandLineAPIImpl[method], commandLineAPIImpl);
        this[method].toString = function() { return "function " + method + "() { [Command Line API] }" };
    }
}

/**
 * @type {Array.<string>}
 * @const
 */
CommandLineAPI.methods = [
    "$",
    "$$",
    "$x",
    "clear",
    "copy",
    "dir",
    "dirxml",
    "getEventListeners",
    "inspect",
    "keys",
    "monitorEvents",
    "profile",
    "profileEnd",
    "queryObjects",
    "table",
    "unmonitorEvents",
    "values",
];

/**
 * @constructor
 */
function CommandLineAPIImpl()
{
}

CommandLineAPIImpl.prototype = {
    /**
     * @param {string} selector
     * @param {Node=} start
     */
    $: function (selector, start)
    {
        if (this._canQuerySelectorOnNode(start))
            return start.querySelector(selector);

        var result = inspectedWindow.document.querySelector(selector);
        if (result)
            return result;
        if (selector && selector[0] !== "#") {
            result = inspectedWindow.document.getElementById(selector);
            if (result) {
                inspectedWindow.console.warn("The console function $() has changed from $=getElementById(id) to $=querySelector(selector). You might try $(\"#%s\")", selector);
                return null;
            }
        }
        return result;
    },

    /**
     * @param {string} selector
     * @param {Node=} start
     */
    $$: function (selector, start)
    {
        if (this._canQuerySelectorOnNode(start))
            return Array.from(start.querySelectorAll(selector));
        return Array.from(inspectedWindow.document.querySelectorAll(selector));
    },

    /**
     * @param {Node=} node
     * @return {boolean}
     */
    _canQuerySelectorOnNode: function(node)
    {
        return !!node && InjectedScriptHost.subtype(node) === "node" && (node.nodeType === Node.ELEMENT_NODE || node.nodeType === Node.DOCUMENT_NODE || node.nodeType === Node.DOCUMENT_FRAGMENT_NODE);
    },

    /**
     * @param {string} xpath
     * @param {Node=} context
     */
    $x: function(xpath, context)
    {
        var doc = (context && context.ownerDocument) || inspectedWindow.document;
        var result = doc.evaluate(xpath, context || doc, null, XPathResult.ANY_TYPE, null);
        switch (result.resultType) {
        case XPathResult.NUMBER_TYPE:
            return result.numberValue;
        case XPathResult.STRING_TYPE:
            return result.stringValue;
        case XPathResult.BOOLEAN_TYPE:
            return result.booleanValue;
        default:
            var nodes = [];
            var node;
            while (node = result.iterateNext())
                nodes.push(node);
            return nodes;
        }
    },

    dir: function()
    {
        return inspectedWindow.console.dir.apply(inspectedWindow.console, arguments)
    },

    dirxml: function()
    {
        return inspectedWindow.console.dirxml.apply(inspectedWindow.console, arguments)
    },

    keys: function(object)
    {
        return Object.keys(object);
    },

    values: function(object)
    {
        var result = [];
        for (var key in object)
            result.push(object[key]);
        return result;
    },

    profile: function()
    {
        return inspectedWindow.console.profile.apply(inspectedWindow.console, arguments)
    },

    profileEnd: function()
    {
        return inspectedWindow.console.profileEnd.apply(inspectedWindow.console, arguments)
    },

    table: function()
    {
        return inspectedWindow.console.table.apply(inspectedWindow.console, arguments)
    },

    /**
     * @param {Object} object
     * @param {Array.<string>|string=} types
     */
    monitorEvents: function(object, types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        types = this._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i) {
            object.removeEventListener(types[i], this._logEvent, false);
            object.addEventListener(types[i], this._logEvent, false);
        }
    },

    /**
     * @param {Object} object
     * @param {Array.<string>|string=} types
     */
    unmonitorEvents: function(object, types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        types = this._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i)
            object.removeEventListener(types[i], this._logEvent, false);
    },

    /**
     * @param {*} object
     * @return {*}
     */
    inspect: function(object)
    {
        return this._inspect(object);
    },

    queryObjects()
    {
        return InjectedScriptHost.queryObjects(...arguments);
    },

    copy: function(object)
    {
        var string;
        var subtype = RemoteObject.subtype(object);
        if (subtype === "node")
            string = object.outerHTML;
        else if (subtype === "regexp")
            string = "" + object;
        else if (injectedScript.isPrimitiveValue(object))
            string = "" + object;
        else if (typeof object === "symbol")
            string = String(object);
        else if (typeof object === "function")
            string = "" + object;
        else {
            try {
                string = JSON.stringify(object, null, "  ");
            } catch (e) {
                string = "" + object;
            }
        }

        CommandLineAPIHost.copyText(string);
    },

    clear: function()
    {
        CommandLineAPIHost.clearConsoleMessages();
    },

    /**
     * @param {Node} node
     */
    getEventListeners: function(node)
    {
        return CommandLineAPIHost.getEventListeners(node);
    },

    _inspectedObject: function()
    {
        return CommandLineAPIHost.inspectedObject();
    },

    /**
     * @param {Array.<string>|string=} types
     * @return {Array.<string>}
     */
    _normalizeEventTypes: function(types)
    {
        if (typeof types === "undefined")
            types = [ "mouse", "key", "touch", "control", "load", "unload", "abort", "error", "select", "change", "submit", "reset", "focus", "blur", "resize", "scroll", "search", "devicemotion", "deviceorientation" ];
        else if (typeof types === "string")
            types = [ types ];

        var result = [];
        for (var i = 0; i < types.length; i++) {
            if (types[i] === "mouse")
                result.splice(0, 0, "mousedown", "mouseup", "click", "dblclick", "mousemove", "mouseover", "mouseout", "mousewheel");
            else if (types[i] === "key")
                result.splice(0, 0, "keydown", "keyup", "keypress", "textInput");
            else if (types[i] === "touch")
                result.splice(0, 0, "touchstart", "touchmove", "touchend", "touchcancel");
            else if (types[i] === "control")
                result.splice(0, 0, "resize", "scroll", "zoom", "focus", "blur", "select", "change", "submit", "reset");
            else
                result.push(types[i]);
        }
        return result;
    },

    /**
     * @param {Event} event
     */
    _logEvent: function(event)
    {
        inspectedWindow.console.log(event.type, event);
    },

    /**
     * @param {*} object
     * @return {*}
     */
    _inspect: function(object)
    {
        if (arguments.length === 0)
            return;

        var objectId = RemoteObject.create(object, "");
        var hints = {};

        switch (RemoteObject.describe(object)) {
        case "Database":
            var databaseId = CommandLineAPIHost.databaseId(object)
            if (databaseId)
                hints.databaseId = databaseId;
            break;
        case "Storage":
            var storageId = CommandLineAPIHost.storageId(object)
            if (storageId)
                hints.domStorageId = InjectedScriptHost.evaluate("(" + storageId + ")");
            break;
        }

        CommandLineAPIHost.inspect(objectId, hints);
        return object;
    }
}

injectedScript.CommandLineAPI = CommandLineAPI;
injectedScript._commandLineAPIImpl = new CommandLineAPIImpl();

// This Module doesn't expose an object, it just adds an extension that InjectedScript uses.
// However, we return an empty object, so that InjectedScript knows this module has been loaded.
return {};

})
