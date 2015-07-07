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

//# sourceURL=__WebInspectorCommandLineAPIModuleSource__

/**
 * @param {InjectedScriptHost} InjectedScriptHost
 * @param {Window} inspectedWindow
 * @param {number} injectedScriptId
 * @param {InjectedScript} injectedScript
 * @param {CommandLineAPIHost} CommandLineAPIHost
 */
(function (InjectedScriptHost, inspectedWindow, injectedScriptId, injectedScript, CommandLineAPIHost) {

/**
 * @param {Arguments} array
 * @param {number=} index
 * @return {Array.<*>}
 */
function slice(array, index)
{
    var result = [];
    for (var i = index || 0; i < array.length; ++i)
        result.push(array[i]);
    return result;
}

/**
 * Please use this bind, not the one from Function.prototype
 * @param {function(...)} func
 * @param {Object} thisObject
 * @param {...number} var_args
 */
function bind(func, thisObject, var_args)
{
    var args = slice(arguments, 2);

    /**
     * @param {...number} var_args
     */
    function bound(var_args)
    {
        return func.apply(thisObject, args.concat(slice(arguments)));
    }
    bound.toString = function() {
        return "bound: " + func;
    };
    return bound;
}

/**
 * @constructor
 * @param {CommandLineAPIImpl} commandLineAPIImpl
 * @param {Object} callFrame
 */
function CommandLineAPI(commandLineAPIImpl, callFrame)
{
    /**
     * @param {string} member
     * @return {boolean}
     */
    function inScopeVariables(member)
    {
        if (!callFrame)
            return false;

        var scopeChain = callFrame.scopeChain;
        for (var i = 0; i < scopeChain.length; ++i) {
            if (member in scopeChain[i])
                return true;
        }
        return false;
    }

    /**
     * @param {string} name The name of the method for which a toString method should be generated.
     * @return {function():string}
     */
    function customToStringMethod(name)
    {
        return function () { return "function " + name + "() { [Command Line API] }"; };
    }

    for (var i = 0; i < CommandLineAPI.members_.length; ++i) {
        var member = CommandLineAPI.members_[i];
        if (member in inspectedWindow || inScopeVariables(member))
            continue;

        this[member] = bind(commandLineAPIImpl[member], commandLineAPIImpl);
        this[member].toString = customToStringMethod(member);
    }

    for (var i = 0; i < 5; ++i) {
        var member = "$" + i;
        if (member in inspectedWindow || inScopeVariables(member))
            continue;

        this.__defineGetter__("$" + i, bind(commandLineAPIImpl._inspectedObject, commandLineAPIImpl, i));
    }

    this.$_ = injectedScript._lastResult;
}

/**
 * @type {Array.<string>}
 * @const
 */
CommandLineAPI.members_ = [
    "$", "$$", "$x", "dir", "dirxml", "keys", "values", "profile", "profileEnd",
    "monitorEvents", "unmonitorEvents", "inspect", "copy", "clear", "getEventListeners"
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
                inspectedWindow.console.warn("The console function $() has changed from $=getElementById(id) to $=querySelector(selector). You might try $(\"#%s\")", selector );
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
            return start.querySelectorAll(selector);
        return inspectedWindow.document.querySelectorAll(selector);
    },

    /**
     * @param {Node=} node
     * @return {boolean}
     */
    _canQuerySelectorOnNode: function(node)
    {
        return !!node && InjectedScriptHost.type(node) === "node" && (node.nodeType === Node.ELEMENT_NODE || node.nodeType === Node.DOCUMENT_NODE || node.nodeType === Node.DOCUMENT_FRAGMENT_NODE);
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

    copy: function(object)
    {
        if (injectedScript._subtype(object) === "node")
            object = object.outerHTML;
        CommandLineAPIHost.copyText(object);
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

    /**
     * @param {number} num
     */
    _inspectedObject: function(num)
    {
        return CommandLineAPIHost.inspectedObject(num);
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

        var objectId = injectedScript._wrapObject(object, "");
        var hints = {};

        switch (injectedScript._describe(object)) {
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
