/*
 * Copyright (C) 2007, 2014 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

//# sourceURL=__WebInspectorInjectedScript__

(function (InjectedScriptHost, inspectedGlobalObject, injectedScriptId) {

// Protect against Object overwritten by the user code.
var Object = {}.constructor;

function toString(obj)
{
    return "" + obj;
}
    
function isUInt32(obj)
{
    if (typeof obj === "number")
        return obj >>> 0 === obj && (obj > 0 || 1 / obj > 0);
    return "" + (obj >>> 0) === obj;
}

var InjectedScript = function()
{
    this._lastBoundObjectId = 1;
    this._idToWrappedObject = {};
    this._idToObjectGroupName = {};
    this._objectGroups = {};
    this._modules = {};
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

    wrapObject: function(object, groupName, canAccessInspectedGlobalObject, generatePreview)
    {
        if (canAccessInspectedGlobalObject)
            return this._wrapObject(object, groupName, false, generatePreview);
        return this._fallbackWrapper(object);
    },

    setExceptionValue: function(value)
    {
        this._exceptionValue = value;
    },

    clearExceptionValue: function()
    {
        delete this._exceptionValue;
    },

    _fallbackWrapper: function(object)
    {
        var result = {};
        result.type = typeof object;
        if (this.isPrimitiveValue(object))
            result.value = object;
        else
            result.description = toString(object);
        return result;
    },

    wrapTable: function(canAccessInspectedGlobalObject, table, columns)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(table);
        var columnNames = null;
        if (typeof columns === "string")
            columns = [columns];
        if (InjectedScriptHost.type(columns) === "array") {
            columnNames = [];
            for (var i = 0; i < columns.length; ++i)
                columnNames.push(String(columns[i]));
        }
        return this._wrapObject(table, "console", false, true, columnNames);
    },

    inspectObject: function(object)
    {
        if (this._commandLineAPIImpl)
            this._commandLineAPIImpl.inspect(object);
    },

    _wrapObject: function(object, objectGroupName, forceValueType, generatePreview, columnNames)
    {
        try {
            return new InjectedScript.RemoteObject(object, objectGroupName, forceValueType, generatePreview, columnNames);
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
        return InjectedScriptHost.evaluate("(" + objectId + ")");
    },

    releaseObjectGroup: function(objectGroupName)
    {
        if (objectGroupName === "console")
            delete this._lastResult;
        var group = this._objectGroups[objectGroupName];
        if (!group)
            return;
        for (var i = 0; i < group.length; i++)
            this._releaseObject(group[i]);
        delete this._objectGroups[objectGroupName];
    },

    dispatch: function(methodName, args)
    {
        var argsArray = InjectedScriptHost.evaluate("(" + args + ")");
        var result = this[methodName].apply(this, argsArray);
        if (typeof result === "undefined") {
            if (inspectedGlobalObject.console)
                inspectedGlobalObject.console.error("Web Inspector error: InjectedScript.%s returns undefined", methodName);
            result = null;
        }
        return result;
    },

    getProperties: function(objectId, ownProperties, ownAndGetterProperties)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        var objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!this._isDefined(object))
            return false;

        var descriptors = this._propertyDescriptors(object, ownProperties, ownAndGetterProperties);

        // Go over properties, wrap object values.
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

    getInternalProperties: function(objectId, ownProperties)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        var objectGroupName = this._idToObjectGroupName[parsedObjectId.id];
        if (!this._isDefined(object))
            return false;

        var descriptors = [];
        var internalProperties = InjectedScriptHost.getInternalProperties(object);
        if (internalProperties) {
            for (var i = 0; i < internalProperties.length; i++) {
                var property = internalProperties[i];
                var descriptor = {
                    name: property.name,
                    value: this._wrapObject(property.value, objectGroupName)
                };
                descriptors.push(descriptor);
            }
        }

        return descriptors;
    },

    getFunctionDetails: function(functionId)
    {
        var parsedFunctionId = this._parseObjectId(functionId);
        var func = this._objectForId(parsedFunctionId);
        if (typeof func !== "function")
            return "Cannot resolve function by id.";
        var details = InjectedScriptHost.functionDetails(func);
        if (!details)
            return "Cannot resolve function details.";
        if ("rawScopes" in details) {
            var objectGroupName = this._idToObjectGroupName[parsedFunctionId.id];
            var rawScopes = details.rawScopes;
            var scopes = [];
            delete details.rawScopes;
            for (var i = 0; i < rawScopes.length; i++)
                scopes.push(InjectedScript.CallFrameProxy._createScopeJson(rawScopes[i].type, rawScopes[i].object, objectGroupName));
            details.scopeChain = scopes;
        }
        return details;
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

    evaluate: function(expression, objectGroup, injectCommandLineAPI, returnByValue, generatePreview)
    {
        return this._evaluateAndWrap(InjectedScriptHost.evaluate, InjectedScriptHost, expression, objectGroup, false, injectCommandLineAPI, returnByValue, generatePreview);
    },

    callFunctionOn: function(objectId, expression, args, returnByValue)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        if (!this._isDefined(object))
            return "Could not find object with given id";

        if (args) {
            var resolvedArgs = [];
            var callArgs = InjectedScriptHost.evaluate(args);
            for (var i = 0; i < callArgs.length; ++i) {
                try {
                    resolvedArgs[i] = this._resolveCallArgument(callArgs[i]);
                } catch (e) {
                    return String(e);
                }
            }
        }

        try {
            var objectGroup = this._idToObjectGroupName[parsedObjectId.id];
            var func = InjectedScriptHost.evaluate("(" + expression + ")");
            if (typeof func !== "function")
                return "Given expression does not evaluate to a function";

            return {
                wasThrown: false,
                result: this._wrapObject(func.apply(object, resolvedArgs), objectGroup, returnByValue)
            };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    _resolveCallArgument: function(callArgumentJson) {
        var objectId = callArgumentJson.objectId;
        if (objectId) {
            var parsedArgId = this._parseObjectId(objectId);
            if (!parsedArgId || parsedArgId["injectedScriptId"] !== injectedScriptId)
                throw "Arguments should belong to the same JavaScript world as the target object.";

            var resolvedArg = this._objectForId(parsedArgId);
            if (!this._isDefined(resolvedArg))
                throw "Could not find object with given id";

            return resolvedArg;
        } else if ("value" in callArgumentJson)
            return callArgumentJson.value;
        return undefined;
    },

    _evaluateAndWrap: function(evalFunction, object, expression, objectGroup, isEvalOnCallFrame, injectCommandLineAPI, returnByValue, generatePreview)
    {
        try {
            return {
                wasThrown: false,
                result: this._wrapObject(this._evaluateOn(evalFunction, object, objectGroup, expression, isEvalOnCallFrame, injectCommandLineAPI), objectGroup, returnByValue, generatePreview)
            };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    _createThrownValue: function(value, objectGroup)
    {
        var remoteObject = this._wrapObject(value, objectGroup);
        try {
            remoteObject.description = toString(value);
        } catch (e) {}
        return {
            wasThrown: true,
            result: remoteObject
        };
    },

    _evaluateOn: function(evalFunction, object, objectGroup, expression, isEvalOnCallFrame, injectCommandLineAPI)
    {
        var commandLineAPI = null;
        if (injectCommandLineAPI) {
            if (this.CommandLineAPI)
                commandLineAPI = new this.CommandLineAPI(this._commandLineAPIImpl, isEvalOnCallFrame ? object : null);
            else
                commandLineAPI = new BasicCommandLineAPI;
        }

        if (isEvalOnCallFrame) {
            // We can only use this approach if the evaluate function is the true 'eval'. That allows us to use it with
            // the 'eval' identifier when calling it. Using 'eval' grants access to the local scope of the closure we
            // create that provides the command line APIs.

            var parameters = [InjectedScriptHost.evaluate, expression];
            var expressionFunctionBody = "" +
                "var global = Function('return this')() || (1, eval)('this');" +
                "var __originalEval = global.eval; global.eval = __eval;" +
                "try { return eval(__currentExpression); }" +
                "finally { global.eval = __originalEval; }";

            if (commandLineAPI) {
                // To avoid using a 'with' statement (which fails in strict mode and requires injecting the API object)
                // we instead create a closure where we evaluate the expression. The command line APIs are passed as
                // parameters to the closure so they are in scope but not injected. This allows the code evaluated in
                // the console to stay in strict mode (if is was already set), or to get strict mode by prefixing
                // expressions with 'use strict';.

                var parameterNames = Object.getOwnPropertyNames(commandLineAPI);
                for (var i = 0; i < parameterNames.length; ++i)
                    parameters.push(commandLineAPI[parameterNames[i]]);

                var expressionFunctionString = "(function(__eval, __currentExpression, " + parameterNames.join(", ") + ") { " + expressionFunctionBody + " })";
            } else {
                // Use a closure in this case too to keep the same behavior of 'var' being captured by the closure instead
                // of leaking out into the calling scope.
                var expressionFunctionString = "(function(__eval, __currentExpression) { " + expressionFunctionBody + " })";
            }

            // Bind 'this' to the function expression using another closure instead of Function.prototype.bind. This ensures things will work if the page replaces bind.
            var boundExpressionFunctionString = "(function(__function, __thisObject) { return function() { return __function.apply(__thisObject, arguments) }; })(" + expressionFunctionString + ", this)";
            var expressionFunction = evalFunction.call(object, boundExpressionFunctionString);
            var result = expressionFunction.apply(null, parameters);

            if (objectGroup === "console")
                this._lastResult = result;

            return result;
        }

        // When not evaluating on a call frame we use a 'with' statement to allow var and function statements to leak
        // into the global scope. This allow them to stick around between evaluations.

        try {
            if (commandLineAPI) {
                if (inspectedGlobalObject.console)
                    inspectedGlobalObject.console.__commandLineAPI = commandLineAPI;
                else
                    inspectedGlobalObject.__commandLineAPI = commandLineAPI;
                expression = "with ((this && (this.console ? this.console.__commandLineAPI : this.__commandLineAPI)) || {}) { " + expression + "\n}";
            }

            var result = evalFunction.call(inspectedGlobalObject, expression);

            if (objectGroup === "console")
                this._lastResult = result;

            return result;
        } finally {
            if (commandLineAPI) {
                if (inspectedGlobalObject.console)
                    delete inspectedGlobalObject.console.__commandLineAPI;
                else
                    delete inspectedGlobalObject.__commandLineAPI;
            }
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

    evaluateOnCallFrame: function(topCallFrame, callFrameId, expression, objectGroup, injectCommandLineAPI, returnByValue, generatePreview)
    {
        var callFrame = this._callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        return this._evaluateAndWrap(callFrame.evaluate, callFrame, expression, objectGroup, true, injectCommandLineAPI, returnByValue, generatePreview);
    },

    _callFrameForId: function(topCallFrame, callFrameId)
    {
        var parsedCallFrameId = InjectedScriptHost.evaluate("(" + callFrameId + ")");
        var ordinal = parsedCallFrameId["ordinal"];
        var callFrame = topCallFrame;
        while (--ordinal >= 0 && callFrame)
            callFrame = callFrame.caller;
        return callFrame;
    },

    _objectForId: function(objectId)
    {
        return this._idToWrappedObject[objectId.id];
    },

    findObjectById: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        return this._objectForId(parsedObjectId);
    },

    module: function(name)
    {
        return this._modules[name];
    },

    injectModule: function(name, source, host)
    {
        delete this._modules[name];

        var moduleFunction = InjectedScriptHost.evaluate("(" + source + ")");
        if (typeof moduleFunction !== "function") {
            if (inspectedGlobalObject.console)
                inspectedGlobalObject.console.error("Web Inspector error: A function was expected for module %s evaluation", name);
            return null;
        }

        var module = moduleFunction.call(inspectedGlobalObject, InjectedScriptHost, inspectedGlobalObject, injectedScriptId, this, host);
        this._modules[name] = module;
        return module;
    },

    _propertyDescriptors: function(object, ownProperties, ownAndGetterProperties)
    {
        // Modes:
        //  - ownProperties - only own properties and __proto__
        //  - ownAndGetterProperties - own properties, __proto__, and getters in the prototype chain
        //  - neither - get all properties in the prototype chain and __proto__

        var descriptors = [];
        var nameProcessed = {};
        nameProcessed["__proto__"] = null;

        function createFakeValueDescriptor(name, descriptor, isOwnProperty)
        {
            try {
                return {name: name, value: object[name], writable: descriptor.writable || false, configurable: descriptor.configurable || false, enumerable: descriptor.enumerable || false};
            } catch (e) {
                var errorDescriptor = {name: name, value: e, wasThrown: true};
                if (isOwnProperty)
                    errorDescriptor.isOwn = true;
                return errorDescriptor;
            }
        }

        function processDescriptor(descriptor, isOwnProperty, possibleNativeBindingGetter)
        {
            // Own properties only.
            if (ownProperties) {
                if (isOwnProperty)
                    descriptors.push(descriptor);
                return;
            }

            // Own and getter properties.
            if (ownAndGetterProperties) {
                if (isOwnProperty) {
                    // Own property, include the descriptor as is.
                    descriptors.push(descriptor);
                } else if (descriptor.hasOwnProperty("get") && descriptor.get) {
                    // Getter property in the prototype chain. Create a fake value descriptor.
                    descriptors.push(createFakeValueDescriptor(descriptor.name, descriptor, isOwnProperty));
                } else if (possibleNativeBindingGetter) {
                    // Possible getter property in the prototype chain.
                    descriptors.push(descriptor);
                }
                return;
            }

            // All properties.
            descriptors.push(descriptor);
        }

        function processPropertyNames(o, names, isOwnProperty)
        {
            for (var i = 0; i < names.length; ++i) {
                var name = names[i];
                if (nameProcessed[name] || name === "__proto__")
                    continue;

                nameProcessed[name] = true;

                var descriptor = Object.getOwnPropertyDescriptor(o, name);
                if (!descriptor) {
                    // FIXME: Bad descriptor. Can we get here?
                    // Fall back to very restrictive settings.
                    var fakeDescriptor = createFakeValueDescriptor(name, {writable: false, configurable: false, enumerable: false}, isOwnProperty);
                    processDescriptor(fakeDescriptor, isOwnProperty);
                    continue;
                }

                if (descriptor.hasOwnProperty("get") && descriptor.hasOwnProperty("set") && !descriptor.get && !descriptor.set) {
                    // FIXME: <https://webkit.org/b/140575> Web Inspector: Native Bindings Descriptors are Incomplete
                    // Developers may create such a descriptors, so we should be resilient:
                    // var x = {}; Object.defineProperty(x, "p", {get:undefined}); Object.getOwnPropertyDescriptor(x, "p")
                    var fakeDescriptor = createFakeValueDescriptor(name, descriptor, isOwnProperty);
                    processDescriptor(fakeDescriptor, isOwnProperty, true);
                    continue;
                }

                descriptor.name = name;
                if (isOwnProperty)
                    descriptor.isOwn = true;
                processDescriptor(descriptor, isOwnProperty);
            }
        }

        // Iterate prototype chain.
        for (var o = object; this._isDefined(o); o = o.__proto__) {
            var isOwnProperty = o === object;
            processPropertyNames(o, Object.getOwnPropertyNames(o), isOwnProperty);
            if (ownProperties)
                break;
        }
        
        // Include __proto__ at the end.
        try {
            if (object.__proto__)
                descriptors.push({name: "__proto__", value: object.__proto__, writable: true, configurable: true, enumerable: false, isOwn: true});
        } catch (e) {}

        return descriptors;
    },

    _isDefined: function(object)
    {
        return !!object || this._isHTMLAllCollection(object);
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

        return null;
    },

    _describe: function(obj)
    {
        if (this.isPrimitiveValue(obj))
            return null;

        var subtype = this._subtype(obj);

        if (subtype === "regexp")
            return toString(obj);

        if (subtype === "date")
            return toString(obj);

        if (subtype === "error")
            return toString(obj);

        if (subtype === "node") {
            var description = obj.nodeName.toLowerCase();
            switch (obj.nodeType) {
            case 1 /* Node.ELEMENT_NODE */:
                description += obj.id ? "#" + obj.id : "";
                var className = obj.className;
                description += (className && typeof className === "string") ? "." + className.trim().replace(/\s+/g, ".") : "";
                break;
            case 10 /*Node.DOCUMENT_TYPE_NODE */:
                description = "<!DOCTYPE " + description + ">";
                break;
            }
            return description;
        }

        var className = InjectedScriptHost.internalConstructorName(obj);
        if (subtype === "array") {
            if (typeof obj.length === "number")
                className += "[" + obj.length + "]";
            return className;
        }

        // NodeList in JSC is a function, check for array prior to this.
        if (typeof obj === "function")
            return toString(obj);

        // If Object, try for a better name from the constructor.
        if (className === "Object") {
            var constructorName = obj.constructor && obj.constructor.name;
            if (constructorName)
                return constructorName;
        }

        return className;
    }
}

var injectedScript = new InjectedScript;


InjectedScript.RemoteObject = function(object, objectGroupName, forceValueType, generatePreview, columnNames)
{
    this.type = typeof object;

    if (this.type === "undefined" && injectedScript._isHTMLAllCollection(object))
        this.type = "object";

    if (injectedScript.isPrimitiveValue(object) || object === null || forceValueType) {
        // We don't send undefined values over JSON.
        if (this.type !== "undefined")
            this.value = object;

        // Null object is object with 'null' subtype'
        if (object === null)
            this.subtype = "null";

        // Provide user-friendly number values.
        if (this.type === "number")
            this.description = object + "";
        return;
    }

    this.objectId = injectedScript._bind(object, objectGroupName);
    var subtype = injectedScript._subtype(object);
    if (subtype)
        this.subtype = subtype;

    this.className = InjectedScriptHost.internalConstructorName(object);
    this.description = injectedScript._describe(object);

    if (generatePreview && this.type === "object")
        this.preview = this._generatePreview(object, undefined, columnNames);
}

InjectedScript.RemoteObject.prototype = {
    _generatePreview: function(object, firstLevelKeys, secondLevelKeys)
    {
        var preview = {};
        preview.lossless = true;
        preview.overflow = false;
        preview.properties = [];

        var isTableRowsRequest = secondLevelKeys === null || secondLevelKeys;
        var firstLevelKeysCount = firstLevelKeys ? firstLevelKeys.length : 0;

        var propertiesThreshold = {
            properties: isTableRowsRequest ? 1000 : Math.max(5, firstLevelKeysCount),
            indexes: isTableRowsRequest ? 1000 : Math.max(100, firstLevelKeysCount)
        };

        try {
            // All properties.
            var descriptors = injectedScript._propertyDescriptors(object);
            this._appendPropertyDescriptors(preview, descriptors, propertiesThreshold, secondLevelKeys);
            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                return preview;

            // FIXME: Internal properties.
            // FIXME: Map/Set/Iterator entries.
        } catch (e) {
            preview.lossless = false;
        }

        return preview;
    },

    _appendPropertyDescriptors: function(preview, descriptors, propertiesThreshold, secondLevelKeys)
    {
        for (var descriptor of descriptors) {
            // Seen enough.
            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                break;

            // Error in descriptor.
            if (descriptor.wasThrown) {
                preview.lossless = false;
                continue;
            }

            // Do not show "__proto__" in preview.
            var name = descriptor.name;
            if (name === "__proto__")
                continue;

            // Do not show "length" on array like objects in preview.
            if (this.subtype === "array" && name === "length")
                continue;

            // Do not show non-enumerable non-own properties. Special case to allow array indexes that may be on the prototype.
            if (!descriptor.enumerable && !descriptor.isOwn && !(this.subtype === "array" && isUInt32(name)))
                continue;

            // Getter/setter.
            if (!("value" in descriptor)) {
                preview.lossless = false;
                this._appendPropertyPreview(preview, {name: name, type: "accessor"}, propertiesThreshold);
                continue;
            }

            // Null value.
            var value = descriptor.value;
            if (value === null) {
                this._appendPropertyPreview(preview, {name: name, type: "object", subtype: "null", value: "null"}, propertiesThreshold);
                continue;
            }

            // Ignore non-enumerable functions.
            var type = typeof value;
            if (!descriptor.enumerable && type === "function")
                continue;

            // Fix type of document.all.
            if (type === "undefined" && injectedScript._isHTMLAllCollection(value))
                type = "object";

            // Primitive.
            const maxLength = 100;
            if (InjectedScript.primitiveTypes[type]) {
                if (type === "string" && value.length > maxLength) {
                    value = this._abbreviateString(value, maxLength, true);
                    preview.lossless = false;
                }
                this._appendPropertyPreview(preview, {name: name, type: type, value: toString(value)}, propertiesThreshold);
                continue;
            }

            // Object.
            var property = {name: name, type: type};
            var subtype = injectedScript._subtype(value);
            if (subtype)
                property.subtype = subtype;

            // Second level.
            if (secondLevelKeys === null || secondLevelKeys) {
                var subPreview = this._generatePreview(value, secondLevelKeys || undefined, undefined);
                property.valuePreview = subPreview;
                if (!subPreview.lossless)
                    preview.lossless = false;
                if (subPreview.overflow)
                    preview.overflow = true;
            } else {
                var description = "";
                if (type !== "function")
                    description = this._abbreviateString(injectedScript._describe(value), maxLength, subtype === "regexp");
                property.value = description;
                preview.lossless = false;
            }

            this._appendPropertyPreview(preview, property, propertiesThreshold);
        }
    },

    _appendPropertyPreview: function(preview, property, propertiesThreshold)
    {
        if (toString(property.name >>> 0) === property.name)
            propertiesThreshold.indexes--;
        else
            propertiesThreshold.properties--;

        if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0) {
            preview.overflow = true;
            preview.lossless = false;
            return;
        }

        preview.properties.push(property);
    },

    _abbreviateString: function(string, maxLength, middle)
    {
        if (string.length <= maxLength)
            return string;

        if (middle) {
            var leftHalf = maxLength >> 1;
            var rightHalf = maxLength - leftHalf - 1;
            return string.substr(0, leftHalf) + "\u2026" + string.substr(string.length - rightHalf, rightHalf);
        }

        return string.substr(0, maxLength) + "\u2026";
    }
}

InjectedScript.CallFrameProxy = function(ordinal, callFrame)
{
    this.callFrameId = "{\"ordinal\":" + ordinal + ",\"injectedScriptId\":" + injectedScriptId + "}";
    this.functionName = (callFrame.type === "function" ? callFrame.functionName : "");
    this.location = {scriptId: String(callFrame.sourceID), lineNumber: callFrame.line, columnNumber: callFrame.column};
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.this = injectedScript._wrapObject(callFrame.thisObject, "backtrace");
}

InjectedScript.CallFrameProxy.prototype = {
    _wrapScopeChain: function(callFrame)
    {
        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        for (var i = 0; i < scopeChain.length; i++)
            scopeChainProxy[i] = InjectedScript.CallFrameProxy._createScopeJson(callFrame.scopeType(i), scopeChain[i], "backtrace");
        return scopeChainProxy;
    }
}

InjectedScript.CallFrameProxy._scopeTypeNames = {
    0: "global", // GLOBAL_SCOPE
    1: "local", // LOCAL_SCOPE
    2: "with", // WITH_SCOPE
    3: "closure", // CLOSURE_SCOPE
    4: "catch", // CATCH_SCOPE
    5: "functionName", // FUNCTION_NAME_SCOPE
}

InjectedScript.CallFrameProxy._createScopeJson = function(scopeTypeCode, scopeObject, groupId)
{
    return {
        object: injectedScript._wrapObject(scopeObject, groupId),
        type: InjectedScript.CallFrameProxy._scopeTypeNames[scopeTypeCode]
    };
}

function BasicCommandLineAPI()
{
    this.$_ = injectedScript._lastResult;
    this.$exception = injectedScript._exceptionValue;
}

return injectedScript;
})
