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

//# sourceURL=__WebInspectorInjectedScript__

/**
 * @param {InjectedScriptHost} InjectedScriptHost
 * @param {GlobalObject} inspectedGlobalObject
 * @param {number} injectedScriptId
 */
(function (InjectedScriptHost, inspectedGlobalObject, injectedScriptId) {

// Protect against Object overwritten by the user code.
var Object = {}.constructor;

/**
 * @constructor
 */
var InjectedScript = function()
{
    this._lastBoundObjectId = 1;
    this._idToWrappedObject = {};
    this._idToObjectGroupName = {};
    this._objectGroups = {};
    this._modules = {};
}

/**
 * @type {Object.<string, boolean>}
 * @const
 */
InjectedScript.primitiveTypes = {
    undefined: true,
    boolean: true,
    number: true,
    string: true
}

InjectedScript.prototype = {
    /**
     * @param {*} object
     * @return {boolean}
     */
    isPrimitiveValue: function(object)
    {
        // FIXME(33716): typeof document.all is always 'undefined'.
        return InjectedScript.primitiveTypes[typeof object] && !this._isHTMLAllCollection(object);
    },

    /**
     * @param {*} object
     * @param {string} groupName
     * @param {boolean} canAccessInspectedGlobalObject
     * @param {boolean} generatePreview
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapObject: function(object, groupName, canAccessInspectedGlobalObject, generatePreview)
    {
        if (canAccessInspectedGlobalObject)
            return this._wrapObject(object, groupName, false, generatePreview);
        return this._fallbackWrapper(object);
    },

    /**
     * @param {*} object
     * @return {!RuntimeAgent.RemoteObject}
     */
    _fallbackWrapper: function(object)
    {
        var result = {};
        result.type = typeof object;
        if (this.isPrimitiveValue(object))
            result.value = object;
        else
            result.description = this._toString(object);
        return /** @type {!RuntimeAgent.RemoteObject} */ (result);
    },

    /**
     * @param {boolean} canAccessInspectedGlobalObject
     * @param {Object} table
     * @param {Array.<string>|string|boolean} columns
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapTable: function(canAccessInspectedGlobalObject, table, columns)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(table);
        var columnNames = null;
        if (typeof columns === "string")
            columns = [columns];
        if (InjectedScriptHost.type(columns) == "array") {
            columnNames = [];
            for (var i = 0; i < columns.length; ++i)
                columnNames.push(String(columns[i]));
        }
        return this._wrapObject(table, "console", false, true, columnNames);
    },

    /**
     * @param {*} object
     */
    inspectObject: function(object)
    {
        if (this._commandLineAPIImpl)
            this._commandLineAPIImpl.inspect(object);
    },

    /**
     * This method cannot throw.
     * @param {*} object
     * @param {string=} objectGroupName
     * @param {boolean=} forceValueType
     * @param {boolean=} generatePreview
     * @param {?Array.<string>=} columnNames
     * @return {!RuntimeAgent.RemoteObject}
     * @suppress {checkTypes}
     */
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

    /**
     * @param {*} object
     * @param {string=} objectGroupName
     * @return {string}
     */
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

    /**
     * @param {string} objectId
     * @return {Object}
     */
    _parseObjectId: function(objectId)
    {
        return InjectedScriptHost.evaluate("(" + objectId + ")");
    },

    /**
     * @param {string} objectGroupName
     */
    releaseObjectGroup: function(objectGroupName)
    {
        var group = this._objectGroups[objectGroupName];
        if (!group)
            return;
        for (var i = 0; i < group.length; i++)
            this._releaseObject(group[i]);
        delete this._objectGroups[objectGroupName];
    },

    /**
     * @param {string} methodName
     * @param {string} args
     * @return {*}
     */
    dispatch: function(methodName, args)
    {
        var argsArray = InjectedScriptHost.evaluate("(" + args + ")");
        var result = this[methodName].apply(this, argsArray);
        if (typeof result === "undefined") {
            // FIXME: JS Context inspection currently does not have a global.console object.
            if (inspectedGlobalObject.console)
                inspectedGlobalObject.console.error("Web Inspector error: InjectedScript.%s returns undefined", methodName);
            result = null;
        }
        return result;
    },

    /**
     * @param {string} objectId
     * @param {boolean} ownProperties
     * @return {Array.<RuntimeAgent.PropertyDescriptor>|boolean}
     */
    getProperties: function(objectId, ownProperties)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        var objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!this._isDefined(object))
            return false;
        var descriptors = this._propertyDescriptors(object, ownProperties);

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

    /**
     * @param {string} objectId
     * @return {Array.<Object>|boolean}
     */
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

    /**
     * @param {string} functionId
     * @return {!DebuggerAgent.FunctionDetails|string}
     */
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

    /**
     * @param {string} objectId
     */
    releaseObject: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        this._releaseObject(parsedObjectId.id);
    },

    /**
     * @param {string} id
     */
    _releaseObject: function(id)
    {
        delete this._idToWrappedObject[id];
        delete this._idToObjectGroupName[id];
    },

    /**
     * @param {Object} object
     * @param {boolean} ownProperties
     * @return {Array.<Object>}
     */
    _propertyDescriptors: function(object, ownProperties)
    {
        var descriptors = [];
        var nameProcessed = {};
        nameProcessed["__proto__"] = null;
        for (var o = object; this._isDefined(o); o = o.__proto__) {
            var names = Object.getOwnPropertyNames(/** @type {!Object} */ (o));
            for (var i = 0; i < names.length; ++i) {
                var name = names[i];
                if (nameProcessed[name])
                    continue;

                try {
                    nameProcessed[name] = true;
                    var descriptor = Object.getOwnPropertyDescriptor(/** @type {!Object} */ (object), name);
                    if (!descriptor) {
                        // Not all bindings provide proper descriptors. Fall back to the writable, configurable property.
                        try {
                            descriptor = { name: name, value: object[name], writable: false, configurable: false, enumerable: false};
                            if (o === object)
                                descriptor.isOwn = true;
                            descriptors.push(descriptor);
                        } catch (e) {
                            // Silent catch.
                        }
                        continue;
                    }
                    if (descriptor.hasOwnProperty("get") && descriptor.hasOwnProperty("set") && !descriptor.get && !descriptor.set) {
                        // Not all bindings provide proper descriptors. Fall back to the writable, configurable property.
                        try {
                            descriptor = { name: name, value: object[name], writable: false, configurable: false, enumerable: false};
                            if (o === object)
                                descriptor.isOwn = true;
                            descriptors.push(descriptor);
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
                if (o === object)
                    descriptor.isOwn = true;
                descriptors.push(descriptor);
            }
            if (ownProperties) {
                if (object.__proto__)
                    descriptors.push({ name: "__proto__", value: object.__proto__, writable: true, configurable: true, enumerable: false, isOwn: true});
                break;
            }
        }
        return descriptors;
    },

    /**
     * @param {string} expression
     * @param {string} objectGroup
     * @param {boolean} injectCommandLineAPI
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @return {*}
     */
    evaluate: function(expression, objectGroup, injectCommandLineAPI, returnByValue, generatePreview)
    {
        return this._evaluateAndWrap(InjectedScriptHost.evaluate, InjectedScriptHost, expression, objectGroup, false, injectCommandLineAPI, returnByValue, generatePreview);
    },

    /**
     * @param {string} objectId
     * @param {string} expression
     * @param {boolean} returnByValue
     * @return {Object|string}
     */
    callFunctionOn: function(objectId, expression, args, returnByValue)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        var object = this._objectForId(parsedObjectId);
        if (!this._isDefined(object))
            return "Could not find object with given id";

        if (args) {
            var resolvedArgs = [];
            args = InjectedScriptHost.evaluate(args);
            for (var i = 0; i < args.length; ++i) {
                var resolvedCallArgument;
                try {
                    resolvedCallArgument = this._resolveCallArgument(args[i]);
                } catch (e) {
                    return String(e);
                }
                resolvedArgs.push(resolvedCallArgument)
            }
        }

        try {
            var objectGroup = this._idToObjectGroupName[parsedObjectId.id];
            var func = InjectedScriptHost.evaluate("(" + expression + ")");
            if (typeof func !== "function")
                return "Given expression does not evaluate to a function";

            return { wasThrown: false,
                     result: this._wrapObject(func.apply(object, resolvedArgs), objectGroup, returnByValue) };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    /**
     * Resolves a value from CallArgument description.
     * @param {RuntimeAgent.CallArgument} callArgumentJson
     * @return {*} resolved value
     * @throws {string} error message
     */
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
        else
            return undefined;
    },

    /**
     * @param {Function} evalFunction
     * @param {Object} object
     * @param {string} objectGroup
     * @param {boolean} isEvalOnCallFrame
     * @param {boolean} injectCommandLineAPI
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @return {*}
     */
    _evaluateAndWrap: function(evalFunction, object, expression, objectGroup, isEvalOnCallFrame, injectCommandLineAPI, returnByValue, generatePreview)
    {
        try {
            return { wasThrown: false,
                     result: this._wrapObject(this._evaluateOn(evalFunction, object, objectGroup, expression, isEvalOnCallFrame, injectCommandLineAPI), objectGroup, returnByValue, generatePreview) };
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    },

    /**
     * @param {*} value
     * @param {string} objectGroup
     * @return {Object}
     */
    _createThrownValue: function(value, objectGroup)
    {
        var remoteObject = this._wrapObject(value, objectGroup);
        try {
            remoteObject.description = this._toString(value);
        } catch (e) {}
        return { wasThrown: true,
                 result: remoteObject };
    },

    /**
     * @param {Function} evalFunction
     * @param {Object} object
     * @param {string} objectGroup
     * @param {string} expression
     * @param {boolean} isEvalOnCallFrame
     * @param {boolean} injectCommandLineAPI
     * @return {*}
     */
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

        // FIXME: JS Context inspection currently does not have a global.console object.
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

    /**
     * @param {Object} callFrame
     * @return {Array.<InjectedScript.CallFrameProxy>|boolean}
     */
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

    /**
     * @param {Object} topCallFrame
     * @param {string} callFrameId
     * @param {string} expression
     * @param {string} objectGroup
     * @param {boolean} injectCommandLineAPI
     * @param {boolean} returnByValue
     * @param {boolean} generatePreview
     * @return {*}
     */
    evaluateOnCallFrame: function(topCallFrame, callFrameId, expression, objectGroup, injectCommandLineAPI, returnByValue, generatePreview)
    {
        var callFrame = this._callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        return this._evaluateAndWrap(callFrame.evaluate, callFrame, expression, objectGroup, true, injectCommandLineAPI, returnByValue, generatePreview);
    },

    /**
     * @param {Object} topCallFrame
     * @param {string} callFrameId
     * @return {Object}
     */
    _callFrameForId: function(topCallFrame, callFrameId)
    {
        var parsedCallFrameId = InjectedScriptHost.evaluate("(" + callFrameId + ")");
        var ordinal = parsedCallFrameId["ordinal"];
        var callFrame = topCallFrame;
        while (--ordinal >= 0 && callFrame)
            callFrame = callFrame.caller;
        return callFrame;
    },

    /**
     * @param {Object} objectId
     * @return {Object}
     */
    _objectForId: function(objectId)
    {
        return this._idToWrappedObject[objectId.id];
    },

    /**
     * @param {string} objectId
     * @return {Object}
     */
    findObjectById: function(objectId)
    {
        var parsedObjectId = this._parseObjectId(objectId);
        return this._objectForId(parsedObjectId);
    },

    /**
     * @param {string} name
     * @return {Object}
     */
    module: function(name)
    {
        return this._modules[name];
    },

    /**
     * @param {string} name
     * @param {string} source
     * @return {Object}
     */
    injectModule: function(name, source, host)
    {
        delete this._modules[name];
        var moduleFunction = InjectedScriptHost.evaluate("(" + source + ")");
        if (typeof moduleFunction !== "function") {
            // FIXME: JS Context inspection currently does not have a global.console object.
            if (inspectedGlobalObject.console)
                inspectedGlobalObject.console.error("Web Inspector error: A function was expected for module %s evaluation", name);
            return null;
        }
        var module = moduleFunction.call(inspectedGlobalObject, InjectedScriptHost, inspectedGlobalObject, injectedScriptId, this, host);
        this._modules[name] = module;
        return module;
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _isDefined: function(object)
    {
        return !!object || this._isHTMLAllCollection(object);
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _isHTMLAllCollection: function(object)
    {
        // document.all is reported as undefined, but we still want to process it.
        return (typeof object === "undefined") && InjectedScriptHost.isHTMLAllCollection(object);
    },

    /**
     * @param {Object=} obj
     * @return {string?}
     */
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

        // If owning frame has navigated to somewhere else window properties will be undefined.
        return null;
    },

    /**
     * @param {*} obj
     * @return {string?}
     */
    _describe: function(obj)
    {
        if (this.isPrimitiveValue(obj))
            return null;

        obj = /** @type {Object} */ (obj);

        // Type is object, get subtype.
        var subtype = this._subtype(obj);

        if (subtype === "regexp")
            return this._toString(obj);

        if (subtype === "date")
            return this._toString(obj);

        if (subtype === "node") {
            var description = obj.nodeName.toLowerCase();
            switch (obj.nodeType) {
            case 1 /* Node.ELEMENT_NODE */:
                description += obj.id ? "#" + obj.id : "";
                var className = obj.className;
                description += className ? "." + className : "";
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

    /**
     * @param {*} obj
     * @return {string}
     */
    _toString: function(obj)
    {
        // We don't use String(obj) because inspectedGlobalObject.String is undefined if owning frame navigated to another page.
        return "" + obj;
    }
}

/**
 * @type {InjectedScript}
 * @const
 */
var injectedScript = new InjectedScript();

/**
 * @constructor
 * @param {*} object
 * @param {string=} objectGroupName
 * @param {boolean=} forceValueType
 * @param {boolean=} generatePreview
 * @param {?Array.<string>=} columnNames
 */
InjectedScript.RemoteObject = function(object, objectGroupName, forceValueType, generatePreview, columnNames)
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

    object = /** @type {Object} */ (object);

    this.objectId = injectedScript._bind(object, objectGroupName);
    var subtype = injectedScript._subtype(object);
    if (subtype)
        this.subtype = subtype;
    this.className = InjectedScriptHost.internalConstructorName(object);
    this.description = injectedScript._describe(object);

    if (generatePreview && (this.type === "object" || injectedScript._isHTMLAllCollection(object)))
        this.preview = this._generatePreview(object, undefined, columnNames);
}

InjectedScript.RemoteObject.prototype = {
    /**
     * @param {Object} object
     * @param {Array.<string>=} firstLevelKeys
     * @param {?Array.<string>=} secondLevelKeys
     * @return {Object} preview
     */
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
        for (var o = object; injectedScript._isDefined(o); o = o.__proto__)
            this._generateProtoPreview(o, preview, propertiesThreshold, firstLevelKeys, secondLevelKeys);
        return preview;
    },

    /**
     * @param {Object} object
     * @param {Object} preview
     * @param {Object} propertiesThreshold
     * @param {Array.<string>=} firstLevelKeys
     * @param {Array.<string>=} secondLevelKeys
     */
    _generateProtoPreview: function(object, preview, propertiesThreshold, firstLevelKeys, secondLevelKeys)
    {
        var propertyNames = firstLevelKeys ? firstLevelKeys : Object.keys(/** @type {!Object} */(object));
        try {
            for (var i = 0; i < propertyNames.length; ++i) {
                if (!propertiesThreshold.properties || !propertiesThreshold.indexes) {
                    preview.overflow = true;
                    preview.lossless = false;
                    break;
                }
                var name = propertyNames[i];
                if (this.subtype === "array" && name === "length")
                    continue;

                var descriptor = Object.getOwnPropertyDescriptor(/** @type {!Object} */(object), name);
                if (!("value" in descriptor) || !descriptor.enumerable) {
                    preview.lossless = false;
                    continue;
                }

                var value = descriptor.value;
                if (value === null) {
                    this._appendPropertyPreview(preview, { name: name, type: "object", value: "null" }, propertiesThreshold);
                    continue;
                }

                const maxLength = 100;
                var type = typeof value;

                if (InjectedScript.primitiveTypes[type]) {
                    if (type === "string") {
                        if (value.length > maxLength) {
                            value = this._abbreviateString(value, maxLength, true);
                            preview.lossless = false;
                        }
                        value = value.replace(/\n/g, "\u21B5");
                    }
                    this._appendPropertyPreview(preview, { name: name, type: type, value: value + "" }, propertiesThreshold);
                    continue;
                }

                if (secondLevelKeys === null || secondLevelKeys) {
                    var subPreview = this._generatePreview(value, secondLevelKeys || undefined);
                    var property = { name: name, type: type, valuePreview: subPreview };
                    this._appendPropertyPreview(preview, property, propertiesThreshold);
                    if (!subPreview.lossless)
                        preview.lossless = false;
                    if (subPreview.overflow)
                        preview.overflow = true;
                    continue;
                }

                preview.lossless = false;

                var subtype = injectedScript._subtype(value);
                var description = "";
                if (type !== "function")
                    description = this._abbreviateString(/** @type {string} */ (injectedScript._describe(value)), maxLength, subtype === "regexp");

                var property = { name: name, type: type, value: description };
                if (subtype)
                    property.subtype = subtype;
                this._appendPropertyPreview(preview, property, propertiesThreshold);
            }
        } catch (e) {
        }
    },

    /**
     * @param {Object} preview
     * @param {Object} property
     * @param {Object} propertiesThreshold
     */
    _appendPropertyPreview: function(preview, property, propertiesThreshold)
    {
        if (isNaN(property.name))
            propertiesThreshold.properties--;
        else
            propertiesThreshold.indexes--;
        preview.properties.push(property);
    },

    /**
     * @param {string} string
     * @param {number} maxLength
     * @param {boolean=} middle
     * @returns
     */
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
/**
 * @constructor
 * @param {number} ordinal
 * @param {Object} callFrame
 */
InjectedScript.CallFrameProxy = function(ordinal, callFrame)
{
    this.callFrameId = "{\"ordinal\":" + ordinal + ",\"injectedScriptId\":" + injectedScriptId + "}";
    this.functionName = (callFrame.type === "function" ? callFrame.functionName : "");
    this.location = { scriptId: String(callFrame.sourceID), lineNumber: callFrame.line, columnNumber: callFrame.column };
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.this = injectedScript._wrapObject(callFrame.thisObject, "backtrace");
}

InjectedScript.CallFrameProxy.prototype = {
    /**
     * @param {Object} callFrame
     * @return {!Array.<DebuggerAgent.Scope>}
     */
    _wrapScopeChain: function(callFrame)
    {
        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        for (var i = 0; i < scopeChain.length; i++) {
            var scope = InjectedScript.CallFrameProxy._createScopeJson(callFrame.scopeType(i), scopeChain[i], "backtrace");
            scopeChainProxy.push(scope);
        }
        return scopeChainProxy;
    }
}

/**
 * @param {number} scopeTypeCode
 * @param {*} scopeObject
 * @param {string} groupId
 * @return {!DebuggerAgent.Scope}
 */
InjectedScript.CallFrameProxy._createScopeJson = function(scopeTypeCode, scopeObject, groupId) {
    const GLOBAL_SCOPE = 0;
    const LOCAL_SCOPE = 1;
    const WITH_SCOPE = 2;
    const CLOSURE_SCOPE = 3;
    const CATCH_SCOPE = 4;

    /** @type {!Object.<number, string>} */
    var scopeTypeNames = {};
    scopeTypeNames[GLOBAL_SCOPE] = "global";
    scopeTypeNames[LOCAL_SCOPE] = "local";
    scopeTypeNames[WITH_SCOPE] = "with";
    scopeTypeNames[CLOSURE_SCOPE] = "closure";
    scopeTypeNames[CATCH_SCOPE] = "catch";

    return {
        object: injectedScript._wrapObject(scopeObject, groupId),
        type: /** @type {DebuggerAgent.ScopeType} */ (scopeTypeNames[scopeTypeCode])
    };
}

function BasicCommandLineAPI()
{
    this.$_ = injectedScript._lastResult;
}

return injectedScript;
})
