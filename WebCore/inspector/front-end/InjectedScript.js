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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

var injectedScriptConstructor = (function (InjectedScriptHost, inspectedWindow, injectedScriptId, jsEngine) {

var InjectedScript = {};

InjectedScript.lastBoundObjectId = 1;
InjectedScript.idToWrappedObject = {};
InjectedScript.objectGroups = {};

InjectedScript.wrapObjectForConsole = function(object, canAccessInspectedWindow)
{
    if (canAccessInspectedWindow)
        return InjectedScript.wrapObject(object, "console");
    var result = {};
    result.type = typeof object;
    result.description = InjectedScript._toString(object);
    return result;
}

InjectedScript.wrapObject = function(object, objectGroupName)
{
    try {
        var objectId;
        if (typeof object === "object" || typeof object === "function" || InjectedScript._isHTMLAllCollection(object)) {
            var id = InjectedScript.lastBoundObjectId++;
            objectId = id;
            InjectedScript.idToWrappedObject[id] = object;

            var group = InjectedScript.objectGroups[objectGroupName];
            if (!group) {
                group = [];
                InjectedScript.objectGroups[objectGroupName] = group;
            }
            group.push(id);
            objectId = new InjectedScript.RemoteObjectId(InjectedScript.RemoteObjectId.Type.JsObject, id);
        }
        return InjectedScript.RemoteObject.fromObject(object, objectId);
    } catch (e) {
        return InjectedScript.RemoteObject.fromObject("[ Exception: " + e.toString() + " ]");
    }
};

InjectedScript.unwrapObject = function(objectId) {
    return InjectedScript.idToWrappedObject[objectId];
};

InjectedScript.releaseWrapperObjectGroup = function(objectGroupName) {
    var group = InjectedScript.objectGroups[objectGroupName];
    if (!group)
        return;
    for (var i = 0; i < group.length; i++)
        delete InjectedScript.idToWrappedObject[group[i]];
    delete InjectedScript.objectGroups[objectGroupName];
};

InjectedScript.dispatch = function(methodName, args, callId)
{
    var argsArray = eval("(" + args + ")");
    if (callId)
        argsArray.splice(0, 0, callId);  // Methods that run asynchronously have a call back id parameter.
    var result = InjectedScript[methodName].apply(InjectedScript, argsArray);
    if (typeof result === "undefined") {
        inspectedWindow.console.error("Web Inspector error: InjectedScript.%s returns undefined", methodName);
        result = null;
    }
    return result;
}

InjectedScript.getPrototypes = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;

    var result = [];
    var prototype = node;
    var prototypeId = new InjectedScript.RemoteObjectId(InjectedScript.RemoteObjectId.Type.Node, nodeId);
    do {
        result.push(InjectedScript.RemoteObject.fromObject(prototype, prototypeId));
        prototype = prototype.__proto__;
        prototypeId = InjectedScript.RemoteObjectId.deriveProperty(prototypeId, "__proto__");
    } while (prototype)

    return result;
}

InjectedScript.getProperties = function(objectId, ignoreHasOwnProperty, abbreviate)
{
    var object = InjectedScript._objectForId(objectId);
    if (!InjectedScript._isDefined(object))
        return false;
    var properties = [];
    
    var propertyNames = ignoreHasOwnProperty ? InjectedScript._getPropertyNames(object) : Object.getOwnPropertyNames(object);
    if (!ignoreHasOwnProperty && object.__proto__)
        propertyNames.push("__proto__");

    // Go over properties, prepare results.
    for (var i = 0; i < propertyNames.length; ++i) {
        var propertyName = propertyNames[i];

        var property = {};
        property.name = propertyName + "";
        var isGetter = object["__lookupGetter__"] && object.__lookupGetter__(propertyName);
        if (!isGetter) {
            try {
                var childObject = object[propertyName];
                var childObjectId = InjectedScript.RemoteObjectId.deriveProperty(objectId, propertyName);
                var childObjectProxy = new InjectedScript.RemoteObject.fromObject(childObject, childObjectId, abbreviate);
                property.value = childObjectProxy;
            } catch(e) {
                property.value = new InjectedScript.RemoteObject.fromException(e);
            }
        } else {
            // FIXME: this should show something like "getter" (bug 16734).
            property.value = new InjectedScript.RemoteObject.fromObject("\u2014"); // em dash
            property.isGetter = true;
        }
        properties.push(property);
    }
    return properties;
}

InjectedScript.setPropertyValue = function(objectId, propertyName, expression)
{
    var object = InjectedScript._objectForId(objectId);
    if (!InjectedScript._isDefined(object))
        return false;

    var expressionLength = expression.length;
    if (!expressionLength) {
        delete object[propertyName];
        return !(propertyName in object);
    }

    try {
        // Surround the expression in parenthesis so the result of the eval is the result
        // of the whole expression not the last potential sub-expression.

        // There is a regression introduced here: eval is now happening against global object,
        // not call frame while on a breakpoint.
        // TODO: bring evaluation against call frame back.
        var result = inspectedWindow.eval("(" + expression + ")");
        // Store the result in the property.
        object[propertyName] = result;
        return true;
    } catch(e) {
        try {
            var result = inspectedWindow.eval("\"" + expression.replace(/"/g, "\\\"") + "\"");
            object[propertyName] = result;
            return true;
        } catch(e) {
            return false;
        }
    }
}

InjectedScript._populatePropertyNames = function(object, resultSet)
{
    for (var o = object; o; o = o.__proto__) {
        try {
            var names = Object.getOwnPropertyNames(o);
            for (var i = 0; i < names.length; ++i)
                resultSet[names[i]] = true;
        } catch (e) {
        }
    }
}

InjectedScript._getPropertyNames = function(object, resultSet)
{
    var propertyNameSet = {};
    InjectedScript._populatePropertyNames(object, propertyNameSet);
    return Object.keys(propertyNameSet);
}

InjectedScript.getCompletions = function(expression, includeInspectorCommandLineAPI, callFrameId)
{
    var props = {};
    try {
        var expressionResult;
        // Evaluate on call frame if call frame id is available.
        if (typeof callFrameId === "number") {
            var callFrame = InjectedScript._callFrameForId(callFrameId);
            if (!callFrame)
                return props;
            if (expression)
                expressionResult = InjectedScript._evaluateOn(callFrame.evaluate, callFrame, expression, true);
            else {
                // Evaluate into properties in scope of the selected call frame.
                var scopeChain = callFrame.scopeChain;
                for (var i = 0; i < scopeChain.length; ++i)
                    InjectedScript._populatePropertyNames(scopeChain[i], props);
            }
        } else {
            if (!expression)
                expression = "this";
            expressionResult = InjectedScript._evaluateOn(inspectedWindow.eval, inspectedWindow, expression, false);
        }
        if (typeof expressionResult === "object")
            InjectedScript._populatePropertyNames(expressionResult, props);

        if (includeInspectorCommandLineAPI) {
            for (var prop in InjectedScript._commandLineAPI)
                props[prop] = true;
        }
    } catch(e) {
    }
    return props;
}

InjectedScript.evaluateAndStringify = function(expression)
{
    var result = {};
    try {
        var value = InjectedScript._evaluateOn(inspectedWindow.eval, inspectedWindow, expression, false);
        result.value = JSON.stringify(value);
    } catch (e) {
        result.value = e.toString();
        result.isException = true;
    }
    return result;
}

InjectedScript.evaluate = function(expression, objectGroup)
{
    return InjectedScript._evaluateAndWrap(inspectedWindow.eval, inspectedWindow, expression, objectGroup);
}

InjectedScript._evaluateAndWrap = function(evalFunction, object, expression, objectGroup, dontUseCommandLineAPI)
{
    try {
        return InjectedScript.wrapObject(InjectedScript._evaluateOn(evalFunction, object, expression, dontUseCommandLineAPI), objectGroup);
    } catch (e) {
        return InjectedScript.RemoteObject.fromException(e);
    }
}

InjectedScript._evaluateOn = function(evalFunction, object, expression, dontUseCommandLineAPI)
{
    if (!dontUseCommandLineAPI) {
        // Only install command line api object for the time of evaluation.

        // Surround the expression in with statements to inject our command line API so that
        // the window object properties still take more precedent than our API functions.
        inspectedWindow.console._commandLineAPI = InjectedScript._commandLineAPI;

        expression = "with (window.console._commandLineAPI) { with (window) {\n" + expression + "\n} }";
    }

    var value = evalFunction.call(object, expression);

    if (!dontUseCommandLineAPI)
        delete inspectedWindow.console._commandLineAPI;

    // When evaluating on call frame error is not thrown, but returned as a value.
    if (InjectedScript._type(value) === "error")
        throw value.toString();

    return value;
}

InjectedScript.getNodeId = function(node)
{
    return InjectedScriptHost.pushNodePathToFrontend(node, false, false);
}

InjectedScript.openInInspectedWindow = function(url)
{
    // Don't call window.open on wrapper - popup blocker mutes it.
    // URIs should have no double quotes.
    inspectedWindow.eval("window.open(\"" + url + "\")");
    return true;
}

InjectedScript.callFrames = function()
{
    var callFrame = InjectedScriptHost.currentCallFrame();
    if (!callFrame)
        return false;

    var result = [];
    var depth = 0;
    do {
        result.push(new InjectedScript.CallFrameProxy(depth++, callFrame));
        callFrame = callFrame.caller;
    } while (callFrame);
    return result;
}

InjectedScript.evaluateInCallFrame = function(callFrameId, code, objectGroup)
{
    var callFrame = InjectedScript._callFrameForId(callFrameId);
    if (!callFrame)
        return false;
    return InjectedScript._evaluateAndWrap(callFrame.evaluate, callFrame, code, objectGroup, true);
}

InjectedScript._callFrameForId = function(id)
{
    var callFrame = InjectedScriptHost.currentCallFrame();
    while (--id >= 0 && callFrame)
        callFrame = callFrame.caller;
    return callFrame;
}

InjectedScript._nodeForId = function(nodeId)
{
    if (!nodeId)
        return null;
    return InjectedScriptHost.nodeForId(nodeId);
}

InjectedScript._objectForId = function(objectId)
{
    // There are three types of object ids used:
    // - numbers point to DOM Node via the InspectorDOMAgent mapping
    // - strings point to console objects cached in InspectorController for lazy evaluation upon them
    // - objects contain complex ids and are currently used for scoped objects
    var object;
    if (objectId.type === InjectedScript.RemoteObjectId.Type.Node)
        object = InjectedScript._nodeForId(objectId.value);
    else if (objectId.type === InjectedScript.RemoteObjectId.Type.JsObject)
        object = InjectedScript.unwrapObject(objectId.value);
    else if (objectId.type === InjectedScript.RemoteObjectId.Type.ScopeObject) {
        var callFrame = InjectedScript._callFrameForId(objectId.value.callFrame);
        if (objectId.thisObject)
            object = callFrame.thisObject;
        else
            object = callFrame.scopeChain[objectId.value.chainIndex];
    } else
        return objectId;

    var path = objectId.path;

    // Follow the property path.
    for (var i = 0; InjectedScript._isDefined(object) && path && i < path.length; ++i)
        object = object[path[i]];

    return object;
}

InjectedScript.pushNodeToFrontend = function(objectId)
{
    var object = InjectedScript._objectForId(objectId);
    if (!object || InjectedScript._type(object) !== "node")
        return false;
    return InjectedScriptHost.pushNodePathToFrontend(object, false, false);
}

// FIXME: RemoteObjectId and RemoteObject structs must match the WebInspector.* ones. Should reuse same file instead.
InjectedScript.RemoteObjectId = function(type, value, path)
{
    this.injectedScriptId = injectedScriptId;
    this.type = type;
    this.value = value;
    this.path = path || [];
}

InjectedScript.RemoteObjectId.Type = {
    Node: "node",
    JsObject: "jsObject",
    ScopeObject: "scopeObject"
}

InjectedScript.RemoteObjectId.deriveProperty = function(objectId, propertyName)
{
    var path = objectId.path.slice() || [];
    path.push(propertyName);
    return new InjectedScript.RemoteObjectId(objectId.type, objectId.value, path);
}

InjectedScript.RemoteObject = function(objectId, type, description, hasChildren)
{
    this.objectId = objectId;
    this.type = type;
    this.description = description;
    this.hasChildren = hasChildren;
}

InjectedScript.RemoteObject.fromException = function(e)
{
    return new InjectedScript.RemoteObject(null, "error", e.toString());
}

InjectedScript.RemoteObject.fromObject = function(object, objectId, abbreviate)
{
    var type = InjectedScript._type(object);
    var rawType = typeof object;
    var hasChildren = (rawType === "object" && object !== null && (Object.getOwnPropertyNames(object).length || !!object.__proto__)) || rawType === "function";
    var description = "";
    try {
        var description = InjectedScript._describe(object, abbreviate);
        return new InjectedScript.RemoteObject(objectId, type, description, hasChildren);
    } catch (e) {
        return InjectedScript.RemoteObject.fromException(e);
    }
}

InjectedScript.evaluateOnSelf = function(funcBody, args)
{
    var func = window.eval("(" + funcBody + ")");
    return func.apply(this, args || []);
}

InjectedScript.CallFrameProxy = function(id, callFrame)
{
    this.id = id;
    this.type = callFrame.type;
    this.functionName = (this.type === "function" ? callFrame.functionName : "");
    this.sourceID = callFrame.sourceID;
    this.line = callFrame.line;
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.injectedScriptId = injectedScriptId;
}

InjectedScript.CallFrameProxy.prototype = {
    _wrapScopeChain: function(callFrame)
    {
        const GLOBAL_SCOPE = 0;
        const LOCAL_SCOPE = 1;
        const WITH_SCOPE = 2;
        const CLOSURE_SCOPE = 3;
        const CATCH_SCOPE = 4;
    
        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        var foundLocalScope = false;
        for (var i = 0; i < scopeChain.length; i++) {
            var scopeType = callFrame.scopeType(i);
            var scopeObject = scopeChain[i];
            var scopeObjectId = new InjectedScript.RemoteObjectId(InjectedScript.RemoteObjectId.Type.ScopeObject, { callFrame: this.id, chainIndex: i });
            var scopeObjectProxy = InjectedScript.RemoteObject.fromObject(scopeObject, scopeObjectId, true);

            switch(scopeType) {
                case LOCAL_SCOPE: {
                    foundLocalScope = true;
                    scopeObjectProxy.isLocal = true;
                    var thisObjectId = new InjectedScript.RemoteObjectId(InjectedScript.RemoteObjectId.Type.ScopeObject, { callFrame: this.id, thisObject: true });
                    scopeObjectProxy.thisObject = InjectedScript.RemoteObject.fromObject(callFrame.thisObject, thisObjectId, true);
                    break;
                }
                case CLOSURE_SCOPE: {
                    scopeObjectProxy.isClosure = true;
                    break;
                }
                case WITH_SCOPE:
                case CATCH_SCOPE: {
                    if (foundLocalScope && scopeObject instanceof inspectedWindow.Element)
                        scopeObjectProxy.isElement = true;
                    else if (foundLocalScope && scopeObject instanceof inspectedWindow.Document)
                        scopeObjectProxy.isDocument = true;
                    else
                        scopeObjectProxy.isWithBlock = true;
                    break;
                }
            }
            scopeChainProxy.push(scopeObjectProxy);
        }
        return scopeChainProxy;
    }
}

InjectedScript._isDefined = function(object)
{
    return object || InjectedScript._isHTMLAllCollection(object);
}

InjectedScript._isHTMLAllCollection = function(object)
{
    // document.all is reported as undefined, but we still want to process it.
    return (typeof object === "undefined") && inspectedWindow.HTMLAllCollection && object instanceof inspectedWindow.HTMLAllCollection;
}

InjectedScript._type = function(obj)
{
    if (obj === null)
        return "null";

    var type = typeof obj;
    if (type !== "object" && type !== "function") {
        // FIXME(33716): typeof document.all is always 'undefined'.
        if (InjectedScript._isHTMLAllCollection(obj))
            return "array";
        return type;
    }

    // If owning frame has navigated to somewhere else window properties will be undefined.
    // In this case just return result of the typeof.
    if (!inspectedWindow.document)
        return type;

    if (obj instanceof inspectedWindow.Node)
        return (obj.nodeType === undefined ? type : "node");
    if (obj instanceof inspectedWindow.String)
        return "string";
    if (obj instanceof inspectedWindow.Array)
        return "array";
    if (obj instanceof inspectedWindow.Boolean)
        return "boolean";
    if (obj instanceof inspectedWindow.Number)
        return "number";
    if (obj instanceof inspectedWindow.Date)
        return "date";
    if (obj instanceof inspectedWindow.RegExp)
        return "regexp";
    // FireBug's array detection.
    if (isFinite(obj.length) && typeof obj.splice === "function")
        return "array";
    if (isFinite(obj.length) && typeof obj.callee === "function") // arguments.
        return "array";
    if (obj instanceof inspectedWindow.NodeList)
        return "array";
    if (obj instanceof inspectedWindow.HTMLCollection)
        return "array";
    if (obj instanceof inspectedWindow.Error)
        return "error";
    return type;
}

InjectedScript._describe = function(obj, abbreviated)
{
    var type = InjectedScript._type(obj);

    switch (type) {
    case "object":
    case "node":
    case "array":
        var className = InjectedScript._className(obj);
        if (typeof obj.length === "number")
            className += "[" + obj.length + "]";
        return className;
    case "string":
        if (!abbreviated)
            return obj;
        if (obj.length > 100)
            return "\"" + obj.substring(0, 100) + "\u2026\"";
        return "\"" + obj + "\"";
    case "function":
        var objectText = InjectedScript._toString(obj);
        if (abbreviated)
            objectText = /.*/.exec(objectText)[0].replace(/ +$/g, "");
        return objectText;
    default:
        return InjectedScript._toString(obj);
    }
}

InjectedScript._toString = function(obj)
{
    // We don't use String(obj) because inspectedWindow.String is undefined if owning frame navigated to another page.
    return "" + obj;
}

InjectedScript._className = function(obj)
{
    // We can't use the same code for fetching class names of the dom bindings prototype chain.
    // Both of the methods below result in "Object" names on the foreign engine bindings.
    // I gave up and am using a check below to distinguish between the egine bingings.

    if (jsEngine == "JSC") {
        var str = inspectedWindow.Object ? inspectedWindow.Object.prototype.toString.call(obj) : InjectedScript._toString(obj);
        return str.replace(/^\[object (.*)\]$/i, "$1");
    } else {
        // V8
        return obj.constructor && obj.constructor.name || "Object";
    }
}

InjectedScript._logEvent = function(event)
{
    console.log(event.type, event);
}

InjectedScript._normalizeEventTypes = function(types)
{
    if (typeof types === "undefined")
        types = [ "mouse", "key", "load", "unload", "abort", "error", "select", "change", "submit", "reset", "focus", "blur", "resize", "scroll" ];
    else if (typeof types === "string")
        types = [ types ];

    var result = [];
    for (var i = 0; i < types.length; i++) {
        if (types[i] === "mouse")
            result.splice(0, 0, "mousedown", "mouseup", "click", "dblclick", "mousemove", "mouseover", "mouseout");
        else if (types[i] === "key")
            result.splice(0, 0, "keydown", "keyup", "keypress");
        else
            result.push(types[i]);
    }
    return result;
}

InjectedScript._inspectedNode = function(num)
{
    var nodeId = InjectedScriptHost.inspectedNode(num);
    return InjectedScript._nodeForId(nodeId);
}

function CommandLineAPI()
{
}

CommandLineAPI.prototype = {
    // Only add API functions here, private stuff should go to
    // InjectedScript so that it is not suggested by the completion.
    $: function()
    {
        return document.getElementById.apply(document, arguments)
    },

    $$: function()
    {
        return document.querySelectorAll.apply(document, arguments)
    },

    $x: function(xpath, context)
    {
        var nodes = [];
        try {
            var doc = context || document;
            var results = doc.evaluate(xpath, doc, null, XPathResult.ANY_TYPE, null);
            var node;
            while (node = results.iterateNext())
                nodes.push(node);
        } catch (e) {
        }
        return nodes;
    },

    dir: function()
    {
        return console.dir.apply(console, arguments)
    },

    dirxml: function()
    {
        return console.dirxml.apply(console, arguments)
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
        return console.profile.apply(console, arguments)
    },

    profileEnd: function()
    {
        return console.profileEnd.apply(console, arguments)
    },

    monitorEvents: function(object, types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        types = InjectedScript._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i) {
            object.removeEventListener(types[i], InjectedScript._logEvent, false);
            object.addEventListener(types[i], InjectedScript._logEvent, false);
        }
    },

    unmonitorEvents: function(object, types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        types = InjectedScript._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i)
            object.removeEventListener(types[i], InjectedScript._logEvent, false);
    },

    inspect: function(object)
    {
        if (arguments.length === 0)
            return;

        inspectedWindow.console.log(object);
        if (InjectedScript._type(object) === "node")
            InjectedScriptHost.pushNodePathToFrontend(object, false, true);
        else {
            switch (InjectedScript._describe(object)) {
                case "Database":
                    InjectedScriptHost.selectDatabase(object);
                    break;
                case "Storage":
                    InjectedScriptHost.selectDOMStorage(object);
                    break;
            }
        }
    },

    copy: function(object)
    {
        if (InjectedScript._type(object) === "node") {
            var nodeId = InjectedScriptHost.pushNodePathToFrontend(object, false, false);
            InjectedScriptHost.copyNode(nodeId);
        } else
            InjectedScriptHost.copyText(object);
    },

    clear: function()
    {
        InjectedScriptHost.clearConsoleMessages();
    },

    get $0()
    {
        return InjectedScript._inspectedNode(0);
    },

    get $1()
    {
        return InjectedScript._inspectedNode(1);
    },

    get $2()
    {
        return InjectedScript._inspectedNode(2);
    },

    get $3()
    {
        return InjectedScript._inspectedNode(3);
    },

    get $4()
    {
        return InjectedScript._inspectedNode(4);
    }
}

InjectedScript._commandLineAPI = new CommandLineAPI();

return InjectedScript;
});
