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

(function (InjectedScriptHost, inspectedWindow, injectedScriptId) {

function bind(thisObject, memberFunction)
{
    var func = memberFunction;
    var args = Array.prototype.slice.call(arguments, 2);
    function bound()
    {
        return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0)));
    }
    bound.toString = function() {
        return "bound: " + func;
    };
    return bound;
}

var InjectedScript = function()
{
    this._lastBoundObjectId = 1;
    this._idToWrappedObject = {};
    this._idToObjectGroupName = {};
    this._objectGroups = {};
}

InjectedScript.primitiveTypes = {
    undefined: true,
    boolean: true,
    number: true,
    string: true
}

InjectedScript.prototype = {
    isPrimitiveValue: function(object)
    {
        // FIXME(33716): typeof document.all is always 'undefined'.
        return InjectedScript.primitiveTypes[typeof object] && !this._isHTMLAllCollection(object);
    },

    wrapObject: function(object, groupName, canAccessInspectedWindow)
    {
        if (canAccessInspectedWindow)
            return this._wrapObject(object, groupName);

        var result = {};
        result.type = typeof object;
        if (this._isPrimitiveValue(object))
            result.value = object;
        else
            result.description = this._toString(object);
        return result;
    },

    inspectNode: function(object)
    {
        this._inspect(object);
    },

    _inspect: function(object)
    {
        if (arguments.length === 0)
            return;

        var objectId = this._wrapObject(object, "");
        var hints = {};

        switch (injectedScript._describe(object)) {
            case "Database":
                var databaseId = InjectedScriptHost.databaseId(object)
                if (databaseId)
                    hints.databaseId = databaseId;
                break;
            case "Storage":
                var storageId = InjectedScriptHost.storageId(object)
                if (storageId)
                    hints.domStorageId = storageId;
                break;
        }
        InjectedScriptHost.inspect(objectId, hints);
        return object;
    },

    // This method cannot throw.
    _wrapObject: function(object, objectGroupName, forceValueType)
    {
        try {
            return new InjectedScript.RemoteObject(object, objectGroupName, forceValueType);
        } catch (e) {
            try {
                var description = injectedScript._describe(e);
            } catch (ex) {
                var description = "<failed to convert exception to string>";
            }
            return new InjectedScript.RemoteObject(description);
        }
    },

    _bind: function(object, objectGroupName)
    {
        var id = this._lastBoundObjectId++;
        this._idToWrappedObject[id] = object;
        var objectId = "{\"injectedScriptId\":" + injectedScriptId + ",\"id\":" + id + "}";
        if (objectGroupName) {
            var group = this._objectGroups[objectGroupName];
            if (!group) {
                group = [];
                this._objectGroups[objectGroupName] = group;
            }
            group.push(id);
            this._idToObjectGroupName[id] = objectGroupName;
        }
        return objectId;
    },

    _parseObjectId: function(objectId)
    {
        return eval("(" + objectId + ")");
    },

    releaseObjectGroup: function(objectGroupName)
    {
        var group = this._objectGroups[objectGroupName];
        if (!group)
            return;
        for (var i = 0; i < group.length; i++)
            this._releaseObject(group[i]);
        delete this._objectGroups[objectGroupName];
    },

    dispatch: function(methodName, args)
    {
        var argsArray = eval("(" + args + ")");
        var result = this[methodName].apply(this, argsArray);
        if (typeof result === "undefined") {
            inspectedWindow.console.error("Web Inspector error: InjectedScript.%s returns undefined", methodName);
            result = null;
        }
        return result;
    },

    getProperties: function(objectId, ownProperties)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        var objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!this._isDefined(object))
            return false;
        var descriptors = this._propertyDescriptors(object, ownProperties);

        // Go over properties, wrap object values.
        if (descriptors.length === 0 && "arguments" in object) {
            // Fill in JSC scope object.
            for (var key in object)
                descriptors.push({ name: key, value: object[key], writable: false, configurable: false, enumerable: true});
        }

        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            if ("get" in descriptor)
                descriptor.get = this._wrapObject(descriptor.get, objectGroupName);
            if ("set" in descriptor)
                descriptor.set = this._wrapObject(descriptor.set, objectGroupName);
            if ("value" in descriptor)
                descriptor.value = this._wrapObject(descriptor.value, objectGroupName);
            if (!("configurable" in descriptor))
                descriptor.configurable = false;
            if (!("enumerable" in descriptor))
                descriptor.enumerable = false;
            
        }
        return descriptors;
    },

    getFunctionDetails: function(functionId)
    {
        var parsedFunctionId = this._parseObjectId(functionId);
        var func = this._objectForId(parsedFunctionId);
        if (typeof func !== "function")
            return "Cannot resolve function by id.";
        return InjectedScriptHost.functionDetails(func);
    },

    releaseObject: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        this._releaseObject(parsedObjectId.id);
    },

    _releaseObject: function(id)
    {
        delete this._idToWrappedObject[id];
        delete this._idToObjectGroupName[id];
    },

    _propertyDescriptors: function(object, ownProperties)
    {
        var descriptors = [];
        var nameProcessed = {};
        nameProcessed.__proto__ = null;
        for (var o = object; this._isDefined(o); o = o.__proto__) {
            var names = Object.getOwnPropertyNames(o);
            for (var i = 0; i < names.length; ++i) {
                var name = names[i];
                if (nameProcessed[name])
                    continue;

                try {
                    nameProcessed[name] = true;
                    var descriptor = Object.getOwnPropertyDescriptor(object, name);
                    if (!descriptor) {
                        // Not all bindings provide proper descriptors. Fall back to the writable, configurable property.
                        try {
                            descriptors.push({ name: name, value: object[name], writable: false, configurable: false, enumerable: false});
                        } catch (e) {
                            // Silent catch.
                        }
                        continue;
                    }
                } catch (e) {
                    var descriptor = {};
                    descriptor.value = e;
                    descriptor.wasThrown = true;
                }

                descriptor.name = name;
                descriptors.push(descriptor); 
            }
            if (ownProperties) {
                if (object.__proto__)
                    descriptors.push({ name: "__proto__", value: object.__proto__, writable: true, configurable: true, enumerable: false});
                break;
            }
        }
        return descriptors;
    },

    evaluate: function(expression, objectGroup, injectCommandLineAPI, returnByValue)
    {
        return this._evaluateAndWrap(inspectedWindow.eval, inspectedWindow, expression, objectGroup, false, injectCommandLineAPI, returnByValue);
    },

    callFunctionOn: function(objectId, expression, args, returnByValue)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        if (!object)
            return "Could not find object with given id";

        if (args) {
            var resolvedArgs = [];
            args = eval(args);
            for (var i = 0; i < args.length; ++i) {
                var objectId = args[i].objectId;
                if (objectId) {
                    var parsedArgId = this._parseObjectId(objectId);
                    if (!parsedArgId || parsedArgId.injectedScriptId !== injectedScriptId)
                        return "Arguments should belong to the same JavaScript world as the target object.";

                    var resolvedArg = this._objectForId(parsedArgId);
                    if (!resolvedArg)
                        return "Could not find object with given id";

                    resolvedArgs.push(resolvedArg);
                } else if ("value" in args[i])
                    resolvedArgs.push(args[i].value);
                else
                    resolvedArgs.push(undefined);
            }
        }

        try {
            var objectGroup = this._idToObjectGroupName[parsedObjectId.id];
            var func = eval("(" + expression + ")");
            if (typeof func !== "function")
                return "Given expression does not evaluate to a function";

            return { wasThrown: false,
                     result: this._wrapObject(func.apply(object, resolvedArgs), objectGroup, returnByValue) };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    _evaluateAndWrap: function(evalFunction, object, expression, objectGroup, isEvalOnCallFrame, injectCommandLineAPI, returnByValue)
    {
        try {
            return { wasThrown: false,
                     result: this._wrapObject(this._evaluateOn(evalFunction, object, expression, isEvalOnCallFrame, injectCommandLineAPI), objectGroup, returnByValue) };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    _createThrownValue: function(value, objectGroup)
    {
        var remoteObject = this._wrapObject(value, objectGroup);
        try {
            remoteObject.description = this._toString(value);
        } catch (e) {}
        return { wasThrown: true,
                 result: remoteObject };
    },

    _evaluateOn: function(evalFunction, object, expression, isEvalOnCallFrame, injectCommandLineAPI)
    {
        // Only install command line api object for the time of evaluation.
        // Surround the expression in with statements to inject our command line API so that
        // the window object properties still take more precedent than our API functions.

        try {
            if (injectCommandLineAPI && inspectedWindow.console) {
                inspectedWindow.console._commandLineAPI = new CommandLineAPI(this._commandLineAPIImpl, isEvalOnCallFrame ? object : null);
                expression = "with ((window && window.console && window.console._commandLineAPI) || {}) {\n" + expression + "\n}";
            }
            return evalFunction.call(object, expression);
        } finally {
            if (injectCommandLineAPI && inspectedWindow.console)
                delete inspectedWindow.console._commandLineAPI;
        }
    },

    wrapCallFrames: function(callFrame)
    {
        if (!callFrame)
            return false;

        var result = [];
        var depth = 0;
        do {
            result.push(new InjectedScript.CallFrameProxy(depth++, callFrame));
            callFrame = callFrame.caller;
        } while (callFrame);
        return result;
    },

    evaluateOnCallFrame: function(topCallFrame, callFrameId, expression, objectGroup, injectCommandLineAPI, returnByValue)
    {
        var callFrame = this._callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        return this._evaluateAndWrap(callFrame.evaluate, callFrame, expression, objectGroup, true, injectCommandLineAPI, returnByValue);
    },

    _callFrameForId: function(topCallFrame, callFrameId)
    {
        var parsedCallFrameId = eval("(" + callFrameId + ")");
        var ordinal = parsedCallFrameId.ordinal;
        var callFrame = topCallFrame;
        while (--ordinal >= 0 && callFrame)
            callFrame = callFrame.caller;
        return callFrame;
    },

    _objectForId: function(objectId)
    {
        return this._idToWrappedObject[objectId.id];
    },

    nodeForObjectId: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        if (!object || this._subtype(object) !== "node")
            return null;
        return object;
    },

    _isDefined: function(object)
    {
        return object || this._isHTMLAllCollection(object);
    },

    _isHTMLAllCollection: function(object)
    {
        // document.all is reported as undefined, but we still want to process it.
        return (typeof object === "undefined") && InjectedScriptHost.isHTMLAllCollection(object);
    },

    _subtype: function(obj)
    {
        if (obj === null)
            return "null";

        var type = typeof obj;
        if (this.isPrimitiveValue(obj))
            return null;

        if (this._isHTMLAllCollection(obj))
            return "array";

        var preciseType = InjectedScriptHost.type(obj);
        if (preciseType)
            return preciseType;

        // FireBug's array detection.
        try {
            if (typeof obj.splice === "function" && isFinite(obj.length))
                return "array";
            if (Object.prototype.toString.call(obj) === "[object Arguments]" && isFinite(obj.length)) // arguments.
                return "array";
        } catch (e) {
        }

        // If owning frame has navigated to somewhere else window properties will be undefined.
        return null;
    },

    _describe: function(obj)
    {
        if (this.isPrimitiveValue(obj))
            return null;

        // Type is object, get subtype.
        var subtype = this._subtype(obj);

        if (subtype === "regexp")
            return this._toString(obj);

        if (subtype === "date")
            return this._toString(obj);

        var className = InjectedScriptHost.internalConstructorName(obj);
        if (subtype === "array") {
            if (typeof obj.length === "number")
                className += "[" + obj.length + "]";
            return className;
        }

        // NodeList in JSC is a function, check for array prior to this.
        if (typeof obj === "function")
            return this._toString(obj);

        if (className === "Object") {
            // In Chromium DOM wrapper prototypes will have Object as their constructor name,
            // get the real DOM wrapper name from the constructor property.
            var constructorName = obj.constructor && obj.constructor.name;
            if (constructorName)
                return constructorName;
        }
        return className;
    },

    _toString: function(obj)
    {
        // We don't use String(obj) because inspectedWindow.String is undefined if owning frame navigated to another page.
        return "" + obj;
    }
}

var injectedScript = new InjectedScript();

InjectedScript.RemoteObject = function(object, objectGroupName, forceValueType)
{
    this.type = typeof object;
    if (injectedScript.isPrimitiveValue(object) || object === null || forceValueType) {
        // We don't send undefined values over JSON.
        if (typeof object !== "undefined")
            this.value = object;

        // Null object is object with 'null' subtype'
        if (object === null)
            this.subtype = "null";

        // Provide user-friendly number values.
        if (typeof object === "number")
            this.description = object + "";
        return;
    }

    this.objectId = injectedScript._bind(object, objectGroupName);
    var subtype = injectedScript._subtype(object)
    if (subtype)
        this.subtype = subtype;
    this.className = InjectedScriptHost.internalConstructorName(object);
    this.description = injectedScript._describe(object);
}

InjectedScript.CallFrameProxy = function(ordinal, callFrame)
{
    this.callFrameId = "{\"ordinal\":" + ordinal + ",\"injectedScriptId\":" + injectedScriptId + "}";
    this.functionName = (callFrame.type === "function" ? callFrame.functionName : "");
    this.location = { scriptId: String(callFrame.sourceID), lineNumber: callFrame.line, columnNumber: callFrame.column };
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.this = injectedScript._wrapObject(callFrame.thisObject, "backtrace");
}

InjectedScript.CallFrameProxy.prototype = {
    _wrapScopeChain: function(callFrame)
    {
        const GLOBAL_SCOPE = 0;
        const LOCAL_SCOPE = 1;
        const WITH_SCOPE = 2;
        const CLOSURE_SCOPE = 3;
        const CATCH_SCOPE = 4;

        var scopeTypeNames = {};
        scopeTypeNames[GLOBAL_SCOPE] = "global";
        scopeTypeNames[LOCAL_SCOPE] = "local";
        scopeTypeNames[WITH_SCOPE] = "with";
        scopeTypeNames[CLOSURE_SCOPE] = "closure";
        scopeTypeNames[CATCH_SCOPE] = "catch";

        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        var foundLocalScope = false;
        for (var i = 0; i < scopeChain.length; i++) {
            var scope = {};
            scope.object = injectedScript._wrapObject(scopeChain[i], "backtrace");

            var scopeType = callFrame.scopeType(i);
            scope.type = scopeTypeNames[scopeType];
            scopeChainProxy.push(scope);
        }
        return scopeChainProxy;
    }
}

function CommandLineAPI(commandLineAPIImpl, callFrame)
{
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

    for (var i = 0; i < CommandLineAPI.members_.length; ++i) {
        var member = CommandLineAPI.members_[i];
        if (member in inspectedWindow || inScopeVariables(member))
            continue;

        this[member] = bind(commandLineAPIImpl, commandLineAPIImpl[member]);
    }

    for (var i = 0; i < 5; ++i) {
        var member = "$" + i;
        if (member in inspectedWindow || inScopeVariables(member))
            continue;

        this.__defineGetter__("$" + i, bind(commandLineAPIImpl, commandLineAPIImpl._inspectedObject, i));
    }
}

CommandLineAPI.members_ = [
    "$", "$$", "$x", "dir", "dirxml", "keys", "values", "profile", "profileEnd",
    "monitorEvents", "unmonitorEvents", "inspect", "copy", "clear", "getEventListeners"
];

function CommandLineAPIImpl()
{
}

CommandLineAPIImpl.prototype = {
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
        types = this._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i) {
            object.removeEventListener(types[i], this._logEvent, false);
            object.addEventListener(types[i], this._logEvent, false);
        }
    },

    unmonitorEvents: function(object, types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        types = this._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i)
            object.removeEventListener(types[i], this._logEvent, false);
    },

    inspect: function(object)
    {
        return injectedScript._inspect(object);
    },

    copy: function(object)
    {
        if (injectedScript._subtype(object) === "node")
            object = object.outerHTML;
        InjectedScriptHost.copyText(object);
    },

    clear: function()
    {
        InjectedScriptHost.clearConsoleMessages();
    },

    getEventListeners: function(node)
    {
        return InjectedScriptHost.getEventListeners(node);
    },

    _inspectedObject: function(num)
    {
        return InjectedScriptHost.inspectedObject(num);
    },

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

    _logEvent: function(event)
    {
        console.log(event.type, event);
    }
}

injectedScript._commandLineAPIImpl = new CommandLineAPIImpl();
return injectedScript;
})
