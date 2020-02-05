/*
 * Copyright (C) 2007, 2014-2015 Apple Inc.  All rights reserved.
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

//# sourceURL=__InjectedScript_InjectedScriptSource.js

(function (InjectedScriptHost, inspectedGlobalObject, injectedScriptId) {

// FIXME: <https://webkit.org/b/152294> Web Inspector: Parse InjectedScriptSource as a built-in to get guaranteed non-user-overridden built-ins

var Object = {}.constructor;

function toString(obj)
{
    return String(obj);
}

function toStringDescription(obj)
{
    if (obj === 0 && 1 / obj < 0)
        return "-0";

    if (isBigInt(obj))
        return toString(obj) + "n";

    return toString(obj);
}

function isUInt32(obj)
{
    if (typeof obj === "number")
        return obj >>> 0 === obj && (obj > 0 || 1 / obj > 0);
    return "" + (obj >>> 0) === obj;
}

function isSymbol(value)
{
    return typeof value === "symbol";
}

function isBigInt(value)
{
    return typeof value === "bigint";
}

function isEmptyObject(object)
{
    for (let key in object)
        return false;
    return true;
}

function isDefined(value)
{
    return !!value || InjectedScriptHost.isHTMLAllCollection(value);
}

function isPrimitiveValue(value)
{
    switch (typeof value) {
    case "boolean":
    case "number":
    case "string":
        return true;
    case "undefined":
        return !InjectedScriptHost.isHTMLAllCollection(value);
    default:
        return false;
    }
}

// -------

let InjectedScript = class InjectedScript
{
    constructor()
    {
        this._lastBoundObjectId = 1;
        this._idToWrappedObject = {};
        this._idToObjectGroupName = {};
        this._objectGroups = {};
        this._modules = {};
        this._nextSavedResultIndex = 1;
        this._savedResults = [];
    }

    // InjectedScript C++ API

    execute(functionString, objectGroup, includeCommandLineAPI, returnByValue, generatePreview, saveResult, args)
    {
        return this._wrapAndSaveCall(objectGroup, returnByValue, generatePreview, saveResult, () => {
            const isEvalOnCallFrame = false;
            return this._evaluateOn(InjectedScriptHost.evaluateWithScopeExtension, InjectedScriptHost, functionString, isEvalOnCallFrame, includeCommandLineAPI).apply(undefined, args);
        });
    }

    evaluate(expression, objectGroup, includeCommandLineAPI, returnByValue, generatePreview, saveResult)
    {
        const isEvalOnCallFrame = false;
        return this._evaluateAndWrap(InjectedScriptHost.evaluateWithScopeExtension, InjectedScriptHost, expression, objectGroup, isEvalOnCallFrame, includeCommandLineAPI, returnByValue, generatePreview, saveResult);
    }

    awaitPromise(promiseObjectId, returnByValue, generatePreview, saveResult, callback)
    {
        let parsedPromiseObjectId = this._parseObjectId(promiseObjectId);
        let promiseObject = this._objectForId(parsedPromiseObjectId);
        let promiseObjectGroupName = this._idToObjectGroupName[parsedPromiseObjectId.id];

        if (!isDefined(promiseObject)) {
            callback("Could not find object with given id");
            return;
        }

        if (!(promiseObject instanceof Promise)) {
            callback("Object with given id is not a Promise");
            return;
        }

        let resolve = (value) => {
            let returnObject = {
                wasThrown: false,
                result: RemoteObject.create(value, promiseObjectGroupName, returnByValue, generatePreview),
            };

            if (saveResult) {
                this._savedResultIndex = 0;
                this._saveResult(returnObject.result);
                if (this._savedResultIndex)
                    returnObject.savedResultIndex = this._savedResultIndex;
            }

            callback(returnObject);
        };
        let reject = (reason) => {
            callback(this._createThrownValue(reason, promiseObjectGroupName));
        };
        promiseObject.then(resolve, reject);
    }

    evaluateOnCallFrame(topCallFrame, callFrameId, expression, objectGroup, includeCommandLineAPI, returnByValue, generatePreview, saveResult)
    {
        let callFrame = this._callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        const isEvalOnCallFrame = true;
        return this._evaluateAndWrap(callFrame.evaluateWithScopeExtension, callFrame, expression, objectGroup, isEvalOnCallFrame, includeCommandLineAPI, returnByValue, generatePreview, saveResult);
    }

    callFunctionOn(objectId, expression, args, returnByValue, generatePreview)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        let objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return "Could not find object with given id";

        let resolvedArgs = [];
        if (args) {
            let callArgs = InjectedScriptHost.evaluate(args);
            for (let i = 0; i < callArgs.length; ++i) {
                try {
                    resolvedArgs[i] = this._resolveCallArgument(callArgs[i]);
                } catch (e) {
                    return String(e);
                }
            }
        }

        try {
            let func = InjectedScriptHost.evaluate("(" + expression + ")");
            if (typeof func !== "function")
                return "Given expression does not evaluate to a function";

            return {
                wasThrown: false,
                result: RemoteObject.create(func.apply(object, resolvedArgs), objectGroupName, returnByValue, generatePreview)
            };
        } catch (e) {
            return this._createThrownValue(e, objectGroupName);
        }
    }

    getFunctionDetails(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        if (typeof object !== "function")
            return "Cannot resolve function by id.";
        return this.functionDetails(object);
    }

    functionDetails(func)
    {
        let details = InjectedScriptHost.functionDetails(func);
        if (!details)
            return "Cannot resolve function details.";
        return details;
    }

    getPreview(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        return RemoteObject.createObjectPreviewForValue(object, true);
    }

    getProperties(objectId, ownProperties, fetchStart, fetchCount, generatePreview)
    {
        let collectionMode = ownProperties ? InjectedScript.CollectionMode.OwnProperties : InjectedScript.CollectionMode.AllProperties;
        return this._getProperties(objectId, collectionMode, {fetchStart, fetchCount, generatePreview});
    }

    getDisplayableProperties(objectId, fetchStart, fetchCount, generatePreview)
    {
        let collectionMode = InjectedScript.CollectionMode.OwnProperties | InjectedScript.CollectionMode.NativeGetterProperties;
        return this._getProperties(objectId, collectionMode, {fetchStart, fetchCount, generatePreview, nativeGettersAsValues: true});
    }

    getInternalProperties(objectId, generatePreview)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        let objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return false;

        if (isSymbol(object))
            return false;

        let descriptors = this._internalPropertyDescriptors(object);
        if (!descriptors)
            return [];

        for (let i = 0; i < descriptors.length; ++i) {
            let descriptor = descriptors[i];
            if ("value" in descriptor)
                descriptor.value = RemoteObject.create(descriptor.value, objectGroupName, false, generatePreview);
        }

        return descriptors;
    }

    getCollectionEntries(objectId, objectGroupName, fetchStart, fetchCount)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        objectGroupName = objectGroupName || this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return;

        if (typeof object !== "object")
            return;

        let entries = this._entries(object, InjectedScriptHost.subtype(object), fetchStart, fetchCount);
        return entries.map(function(entry) {
            entry.value = RemoteObject.create(entry.value, objectGroupName, false, true);
            if ("key" in entry)
                entry.key = RemoteObject.create(entry.key, objectGroupName, false, true);
            return entry;
        });
    }

    saveResult(callArgumentJSON)
    {
        this._savedResultIndex = 0;

        try {
            let callArgument = InjectedScriptHost.evaluate("(" + callArgumentJSON + ")");
            let value = this._resolveCallArgument(callArgument);
            this._saveResult(value);
        } catch { }

        return this._savedResultIndex;
    }

    wrapCallFrames(callFrame)
    {
        if (!callFrame)
            return false;

        let result = [];
        let depth = 0;
        do {
            result.push(new InjectedScript.CallFrameProxy(depth++, callFrame));
            callFrame = callFrame.caller;
        } while (callFrame);
        return result;
    }

    wrapObject(object, groupName, canAccessInspectedGlobalObject, generatePreview)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(object);

        return RemoteObject.create(object, groupName, false, generatePreview);
    }

    wrapJSONString(jsonString, groupName, generatePreview)
    {
        try {
            return this.wrapObject(JSON.parse(jsonString), groupName, true, generatePreview);
        } catch {
            return null;
        }
    }

    wrapTable(canAccessInspectedGlobalObject, table, columns)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(table);

        // FIXME: Currently columns are ignored. Instead, the frontend filters all
        // properties based on the provided column names and in the provided order.
        // We could filter here to avoid sending very large preview objects.

        let columnNames = null;
        if (typeof columns === "string")
            columns = [columns];

        if (InjectedScriptHost.subtype(columns) === "array") {
            columnNames = [];
            for (let i = 0; i < columns.length; ++i)
                columnNames.push(toString(columns[i]));
        }

        return RemoteObject.create(table, "console", false, true, columnNames);
    }

    previewValue(value)
    {
        return RemoteObject.createObjectPreviewForValue(value, true);
    }

    setEventValue(value)
    {
        this._eventValue = value;
    }

    clearEventValue()
    {
        delete this._eventValue;
    }

    setExceptionValue(value)
    {
        this._exceptionValue = value;
    }

    clearExceptionValue()
    {
        delete this._exceptionValue;
    }

    findObjectById(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        return this._objectForId(parsedObjectId);
    }

    releaseObject(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        this._releaseObject(parsedObjectId.id);
    }

    releaseObjectGroup(objectGroupName)
    {
        if (objectGroupName === "console") {
            delete this._lastResult;
            this._nextSavedResultIndex = 1;
            this._savedResults = [];
        }

        let group = this._objectGroups[objectGroupName];
        if (!group)
            return;

        for (let i = 0; i < group.length; i++)
            this._releaseObject(group[i]);

        delete this._objectGroups[objectGroupName];
    }

    // CommandLineAPI

    inspectObject(object)
    {
        if (this._inspectObject)
            this._inspectObject(object);
    }

    // InjectedScriptModule C++ API

    hasInjectedModule(name)
    {
        return this._modules[name];
    }

    injectModule(name, source, host)
    {
        this._modules[name] = false;

        let moduleFunction = InjectedScriptHost.evaluate("(" + source + ")");
        if (typeof moduleFunction !== "function")
            throw "Error: Web Inspector: a function was expected for injectModule";
        moduleFunction(InjectedScriptHost, inspectedGlobalObject, injectedScriptId, this, {RemoteObject, CommandLineAPI}, host);

        this._modules[name] = true;
    }

    // InjectedScriptModule JavaScript API

    isPrimitiveValue(value)
    {
        return isPrimitiveValue(value);
    }

    // Private

    _parseObjectId(objectId)
    {
        return InjectedScriptHost.evaluate("(" + objectId + ")");
    }

    _objectForId(objectId)
    {
        return this._idToWrappedObject[objectId.id];
    }

    _bind(object, objectGroupName)
    {
        let id = this._lastBoundObjectId++;
        let objectId = `{"injectedScriptId":${injectedScriptId},"id":${id}}`;

        this._idToWrappedObject[id] = object;

        if (objectGroupName) {
            let group = this._objectGroups[objectGroupName];
            if (!group) {
                group = [];
                this._objectGroups[objectGroupName] = group;
            }
            group.push(id);
            this._idToObjectGroupName[id] = objectGroupName;
        }

        return objectId;
    }

    _releaseObject(id)
    {
        delete this._idToWrappedObject[id];
        delete this._idToObjectGroupName[id];
    }

    _fallbackWrapper(object)
    {
        let result = {};
        result.type = typeof object;
        if (isPrimitiveValue(object))
            result.value = object;
        else
            result.description = toStringDescription(object);
        return result;
    }

    _resolveCallArgument(callArgumentJSON)
    {
        if ("value" in callArgumentJSON)
            return callArgumentJSON.value;

        let objectId = callArgumentJSON.objectId;
        if (objectId) {
            let parsedArgId = this._parseObjectId(objectId);
            if (!parsedArgId || parsedArgId["injectedScriptId"] !== injectedScriptId)
                throw "Arguments should belong to the same JavaScript world as the target object.";

            let resolvedArg = this._objectForId(parsedArgId);
            if (!isDefined(resolvedArg))
                throw "Could not find object with given id";

            return resolvedArg;
        }

        return undefined;
    }

    _createThrownValue(value, objectGroup)
    {
        let remoteObject = RemoteObject.create(value, objectGroup);
        try {
            remoteObject.description = toStringDescription(value);
        } catch { }
        return {
            wasThrown: true,
            result: remoteObject
        };
    }

    _evaluateAndWrap(evalFunction, object, expression, objectGroup, isEvalOnCallFrame, includeCommandLineAPI, returnByValue, generatePreview, saveResult)
    {
        return this._wrapAndSaveCall(objectGroup, returnByValue, generatePreview, saveResult, () => {
            return this._evaluateOn(evalFunction, object, expression, isEvalOnCallFrame, includeCommandLineAPI);
        });
    }

    _wrapAndSaveCall(objectGroup, returnByValue, generatePreview, saveResult, func)
    {
        return this._wrapCall(objectGroup, returnByValue, generatePreview, saveResult, () => {
            let result = func();
            if (saveResult)
                this._saveResult(result);
            return result;
        });
    }

    _wrapCall(objectGroup, returnByValue, generatePreview, saveResult, func)
    {
        try {
            this._savedResultIndex = 0;

            let returnObject = {
                wasThrown: false,
                result: RemoteObject.create(func(), objectGroup, returnByValue, generatePreview)
            };

            if (saveResult && this._savedResultIndex)
                returnObject.savedResultIndex = this._savedResultIndex;

            return returnObject;
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    }

    _evaluateOn(evalFunction, object, expression, isEvalOnCallFrame, includeCommandLineAPI)
    {
        let commandLineAPI = null;
        if (includeCommandLineAPI)
            commandLineAPI = new CommandLineAPI(isEvalOnCallFrame ? object : null);
        return evalFunction.call(object, expression, commandLineAPI);
    }

    _callFrameForId(topCallFrame, callFrameId)
    {
        let parsedCallFrameId = InjectedScriptHost.evaluate("(" + callFrameId + ")");
        let ordinal = parsedCallFrameId["ordinal"];
        let callFrame = topCallFrame;
        while (--ordinal >= 0 && callFrame)
            callFrame = callFrame.caller;
        return callFrame;
    }

    _getProperties(objectId, collectionMode, {fetchStart, fetchCount, generatePreview, nativeGettersAsValues})
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        let objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return false;

        if (isSymbol(object))
            return false;

        let start = fetchStart || 0;
        if (start < 0)
            start = 0;

        let count = fetchCount || 0;
        if (count < 0)
            count = 0;

        // Always include __proto__ at the end, but only for the first fetch.
        let includeProto = !start;

        let descriptors = [];
        this._forEachPropertyDescriptor(object, collectionMode, (descriptor) => {
            if (start > 0) {
                --start;
                return InjectedScript.PropertyFetchAction.Continue;
            }

            if ("get" in descriptor)
                descriptor.get = RemoteObject.create(descriptor.get, objectGroupName);
            if ("set" in descriptor)
                descriptor.set = RemoteObject.create(descriptor.set, objectGroupName);
            if ("value" in descriptor)
                descriptor.value = RemoteObject.create(descriptor.value, objectGroupName, false, generatePreview);
            if ("symbol" in descriptor)
                descriptor.symbol = RemoteObject.create(descriptor.symbol, objectGroupName);
            descriptors.push(descriptor);

            if (includeProto && count && descriptors.length >= count && descriptor.name !== "__proto__")
                return InjectedScript.PropertyFetchAction.Stop;

            return (count && descriptors.length >= count) ? InjectedScript.PropertyFetchAction.Stop : InjectedScript.PropertyFetchAction.Continue;
        }, {nativeGettersAsValues, includeProto});
        return descriptors;
    }

    _internalPropertyDescriptors(object, completeDescriptor)
    {
        let internalProperties = InjectedScriptHost.getInternalProperties(object);
        if (!internalProperties)
            return null;

        let descriptors = [];
        for (let i = 0; i < internalProperties.length; i++) {
            let property = internalProperties[i];
            let descriptor = {name: property.name, value: property.value};
            if (completeDescriptor)
                descriptor.isOwn = true;
            descriptors.push(descriptor);
        }
        return descriptors;
    }

    _forEachPropertyDescriptor(object, collectionMode, callback, {nativeGettersAsValues, includeProto})
    {
        if (InjectedScriptHost.subtype(object) === "proxy")
            return;

        let nameProcessed = new Set;

        // Handled below when `includeProto`.
        nameProcessed.add("__proto__");

        function createFakeValueDescriptor(name, symbol, descriptor, isOwnProperty, possibleNativeBindingGetter)
        {
            try {
                let fakeDescriptor = {name, value: object[name]};
                if (descriptor) {
                    if (descriptor.writable)
                        fakeDescriptor.writable = true;
                    if (descriptor.configurable)
                        fakeDescriptor.configurable = true;
                    if (descriptor.enumerable)
                        fakeDescriptor.enumerable = true;
                }
                if (possibleNativeBindingGetter)
                    fakeDescriptor.nativeGetter = true;
                if (isOwnProperty)
                    fakeDescriptor.isOwn = true;
                if (symbol)
                    fakeDescriptor.symbol = symbol;
                // Silence any possible unhandledrejection exceptions created from accessing a native accessor with a wrong this object.
                if (fakeDescriptor.value instanceof Promise && InjectedScriptHost.isPromiseRejectedWithNativeGetterTypeError(fakeDescriptor.value))
                    fakeDescriptor.value.catch(function(){});
                return fakeDescriptor;
            } catch (e) {
                let errorDescriptor = {name, value: e, wasThrown: true};
                if (isOwnProperty)
                    errorDescriptor.isOwn = true;
                if (symbol)
                    errorDescriptor.symbol = symbol;
                return errorDescriptor;
            }
        }

        function processDescriptor(descriptor, isOwnProperty, possibleNativeBindingGetter)
        {
            // All properties.
            if (collectionMode & InjectedScript.CollectionMode.AllProperties)
                return callback(descriptor);

            // Own properties.
            if (collectionMode & InjectedScript.CollectionMode.OwnProperties && isOwnProperty)
                return callback(descriptor);

            // Native Getter properties.
            if (collectionMode & InjectedScript.CollectionMode.NativeGetterProperties) {
                if (possibleNativeBindingGetter)
                    return callback(descriptor);
            }
        }

        function processProperty(o, propertyName, isOwnProperty)
        {
            if (nameProcessed.has(propertyName))
                return InjectedScript.PropertyFetchAction.Continue;

            nameProcessed.add(propertyName);

            let name = toString(propertyName);
            let symbol = isSymbol(propertyName) ? propertyName : null;

            let descriptor = Object.getOwnPropertyDescriptor(o, propertyName);
            if (!descriptor) {
                // FIXME: Bad descriptor. Can we get here?
                // Fall back to very restrictive settings.
                let fakeDescriptor = createFakeValueDescriptor(name, symbol, descriptor, isOwnProperty);
                return processDescriptor(fakeDescriptor, isOwnProperty);
            }

            if (nativeGettersAsValues) {
                if (String(descriptor.get).endsWith("[native code]\n}") || (!descriptor.get && descriptor.hasOwnProperty("get") && !descriptor.set && descriptor.hasOwnProperty("set"))) {
                    // Developers may create such a descriptor, so we should be resilient:
                    // let x = {}; Object.defineProperty(x, "p", {get:undefined}); Object.getOwnPropertyDescriptor(x, "p")
                    let fakeDescriptor = createFakeValueDescriptor(name, symbol, descriptor, isOwnProperty, true);
                    return processDescriptor(fakeDescriptor, isOwnProperty, true);
                }
            }

            descriptor.name = name;
            if (isOwnProperty)
                descriptor.isOwn = true;
            if (symbol)
                descriptor.symbol = symbol;
            return processDescriptor(descriptor, isOwnProperty);
        }

        let isArrayLike = false;
        try {
            isArrayLike = RemoteObject.subtype(object) === "array" && isFinite(object.length) && object.length > 0;
        } catch { }

        for (let o = object; isDefined(o); o = Object.getPrototypeOf(o)) {
            let isOwnProperty = o === object;
            let shouldBreak = false;

            // FIXME: <https://webkit.org/b/201861> Web Inspector: show autocomplete entries for non-index properties on arrays
            if (isArrayLike && isOwnProperty) {
                for (let i = 0; i < o.length; ++i) {
                    if (!(i in o))
                        continue;

                    let result = processProperty(o, toString(i), isOwnProperty);
                    shouldBreak = result === InjectedScript.PropertyFetchAction.Stop;
                    if (shouldBreak)
                        break;
                }
            } else {
                let propertyNames = Object.getOwnPropertyNames(o);
                for (let i = 0; i < propertyNames.length; ++i) {
                    let result = processProperty(o, propertyNames[i], isOwnProperty);
                    shouldBreak = result === InjectedScript.PropertyFetchAction.Stop;
                    if (shouldBreak)
                        break;
                }
            }

            if (shouldBreak)
                break;

            if (Object.getOwnPropertySymbols) {
                let propertySymbols = Object.getOwnPropertySymbols(o);
                for (let i = 0; i < propertySymbols.length; ++i) {
                    let result = processProperty(o, propertySymbols[i], isOwnProperty);
                    shouldBreak = result === InjectedScript.PropertyFetchAction.Stop;
                    if (shouldBreak)
                        break;
                }
            }

            if (shouldBreak)
                break;

            if (collectionMode === InjectedScript.CollectionMode.OwnProperties)
                break;
        }

        if (includeProto) {
            try {
                if (object.__proto__)
                    callback({name: "__proto__", value: object.__proto__, writable: true, configurable: true, isOwn: true});
            } catch { }
        }
    }

    _getSetEntries(object, fetchStart, fetchCount)
    {
        let entries = [];

        // FIXME: This is observable if the page overrides Set.prototype[Symbol.iterator].
        for (let value of object) {
            if (fetchStart > 0) {
                fetchStart--;
                continue;
            }

            entries.push({value});

            if (fetchCount && entries.length === fetchCount)
                break;
        }

        return entries;
    }

    _getMapEntries(object, fetchStart, fetchCount)
    {
        let entries = [];

        // FIXME: This is observable if the page overrides Map.prototype[Symbol.iterator].
        for (let [key, value] of object) {
            if (fetchStart > 0) {
                fetchStart--;
                continue;
            }

            entries.push({key, value});

            if (fetchCount && entries.length === fetchCount)
                break;
        }

        return entries;
    }

    _getWeakMapEntries(object, fetchCount)
    {
        return InjectedScriptHost.weakMapEntries(object, fetchCount);
    }

    _getWeakSetEntries(object, fetchCount)
    {
        return InjectedScriptHost.weakSetEntries(object, fetchCount);
    }

    _getIteratorEntries(object, fetchCount)
    {
        return InjectedScriptHost.iteratorEntries(object, fetchCount);
    }

    _entries(object, subtype, fetchStart, fetchCount)
    {
        if (subtype === "set")
            return this._getSetEntries(object, fetchStart, fetchCount);
        if (subtype === "map")
            return this._getMapEntries(object, fetchStart, fetchCount);
        if (subtype === "weakmap")
            return this._getWeakMapEntries(object, fetchCount);
        if (subtype === "weakset")
            return this._getWeakSetEntries(object, fetchCount);
        if (subtype === "iterator")
            return this._getIteratorEntries(object, fetchCount);

        throw "unexpected type";
    }

    _saveResult(result)
    {
        this._lastResult = result;

        if (result === undefined || result === null)
            return;

        let existingIndex = this._savedResults.indexOf(result);
        if (existingIndex !== -1) {
            this._savedResultIndex = existingIndex;
            return;
        }

        this._savedResultIndex = this._nextSavedResultIndex;
        this._savedResults[this._nextSavedResultIndex++] = result;

        // $n is limited from $1-$99. $0 is special.
        if (this._nextSavedResultIndex >= 100)
            this._nextSavedResultIndex = 1;
    }
};

InjectedScript.CollectionMode = {
    OwnProperties: 1 << 0,          // own properties.
    NativeGetterProperties: 1 << 1, // native getter properties in the prototype chain.
    AllProperties: 1 << 2,          // all properties in the prototype chain.
};

InjectedScript.PropertyFetchAction = {
    Continue: Symbol("continue"),
    Stop: Symbol("stop"),
}

var injectedScript = new InjectedScript;

// -------

let RemoteObject = class RemoteObject
{
    constructor(object, objectGroupName, forceValueType, generatePreview, columnNames)
    {
        this.type = typeof object;

        if (this.type === "undefined" && InjectedScriptHost.isHTMLAllCollection(object))
            this.type = "object";

        if (isPrimitiveValue(object) || isBigInt(object) || object === null || forceValueType) {
            // We don't send undefined values over JSON.
            // BigInt values are not JSON serializable.
            if (this.type !== "undefined" && this.type !== "bigint")
                this.value = object;

            // Null object is object with 'null' subtype.
            if (object === null)
                this.subtype = "null";

            // Provide user-friendly number values.
            if (this.type === "number" || this.type === "bigint")
                this.description = toStringDescription(object);

            return;
        }

        this.objectId = injectedScript._bind(object, objectGroupName);

        let subtype = RemoteObject.subtype(object);
        if (subtype)
            this.subtype = subtype;

        this.className = InjectedScriptHost.internalConstructorName(object);
        this.description = RemoteObject.describe(object);

        if (subtype === "array")
            this.size = typeof object.length === "number" ? object.length : 0;
        else if (subtype === "set" || subtype === "map")
            this.size = object.size;
        else if (subtype === "weakmap")
            this.size = InjectedScriptHost.weakMapSize(object);
        else if (subtype === "weakset")
            this.size = InjectedScriptHost.weakSetSize(object);
        else if (subtype === "class") {
            this.classPrototype = RemoteObject.create(object.prototype, objectGroupName);
            this.className = object.name;
        }

        if (generatePreview && this.type === "object") {
            if (subtype === "proxy") {
                this.preview = this._generatePreview(InjectedScriptHost.proxyTargetValue(object));
                this.preview.lossless = false;
            } else
                this.preview = this._generatePreview(object, undefined, columnNames);
        }
    }

    // Static

    static create(object, objectGroupName, forceValueType, generatePreview, columnNames)
    {
        try {
            return new RemoteObject(object, objectGroupName, forceValueType, generatePreview, columnNames);
        } catch (e) {
            let description;
            try {
                description = RemoteObject.describe(e);
            } catch (ex) {
                alert(ex.message);
                description = "<failed to convert exception to string>";
            }
            return new RemoteObject(description);
        }
    }

    static createObjectPreviewForValue(value, generatePreview, columnNames)
    {
        let remoteObject = new RemoteObject(value, undefined, false, generatePreview, columnNames);
        if (remoteObject.objectId)
            injectedScript.releaseObject(remoteObject.objectId);
        if (remoteObject.classPrototype && remoteObject.classPrototype.objectId)
            injectedScript.releaseObject(remoteObject.classPrototype.objectId);
        return remoteObject.preview || remoteObject._emptyPreview();
    }

    static subtype(value)
    {
        if (value === null)
            return "null";

        if (isPrimitiveValue(value) || isBigInt(value) || isSymbol(value))
            return null;

        if (InjectedScriptHost.isHTMLAllCollection(value))
            return "array";

        let preciseType = InjectedScriptHost.subtype(value);
        if (preciseType)
            return preciseType;

        // FireBug's array detection.
        try {
            if (typeof value.splice === "function" && isFinite(value.length))
                return "array";
        } catch { }

        return null;
    }

    static describe(value)
    {
        if (isPrimitiveValue(value))
            return null;

        if (isBigInt(value))
            return null;

        if (isSymbol(value))
            return toString(value);

        let subtype = RemoteObject.subtype(value);

        if (subtype === "regexp")
            return toString(value);

        if (subtype === "date")
            return toString(value);

        if (subtype === "error")
            return toString(value);

        if (subtype === "proxy")
            return "Proxy";

        if (subtype === "node")
            return RemoteObject.nodePreview(value);

        let className = InjectedScriptHost.internalConstructorName(value);
        if (subtype === "array")
            return className;

        if (subtype === "iterator" && Symbol.toStringTag in value)
            return value[Symbol.toStringTag];

        // NodeList in JSC is a function, check for array prior to this.
        if (typeof value === "function")
            return value.toString();

        // If Object, try for a better name from the constructor.
        if (className === "Object") {
            let constructorName = value.constructor && value.constructor.name;
            if (constructorName)
                return constructorName;
        }

        return className;
    }

    static nodePreview(node)
    {
        let isXMLDocument = node.ownerDocument && !!node.ownerDocument.xmlVersion;
        let nodeName = isXMLDocument ? node.nodeName : node.nodeName.toLowerCase();

        switch (node.nodeType) {
        case 1: // Node.ELEMENT_NODE
            if (node.id)
                return "<" + nodeName + " id=\"" + node.id + "\">";
            if (node.classList.length)
                return "<" + nodeName + " class=\"" + node.classList.toString().replace(/\s+/, " ") + "\">";
            if (nodeName === "input" && node.type)
                return "<" + nodeName + " type=\"" + node.type + "\">";
            return "<" + nodeName + ">";

        case 3: // Node.TEXT_NODE
            return nodeName + " \"" + node.nodeValue + "\"";

        case 8: // Node.COMMENT_NODE
            return "<!--" + node.nodeValue + "-->";

        case 10: // Node.DOCUMENT_TYPE_NODE
            return "<!DOCTYPE " + nodeName + ">";

        default:
            return nodeName;
        }
    }

    // Private

    _initialPreview()
    {
        let preview = {
            type: this.type,
            description: this.description || toString(this.value),
            lossless: true,
        };

        if (this.subtype) {
            preview.subtype = this.subtype;
            if (this.subtype !== "null") {
                preview.overflow = false;
                preview.properties = [];
            }
        }

        if ("size" in this)
            preview.size = this.size;

        return preview;
    }

    _emptyPreview()
    {
        let preview = this._initialPreview();

        if (this.subtype === "map" || this.subtype === "set" || this.subtype === "weakmap" || this.subtype === "weakset" || this.subtype === "iterator") {
            if (this.size) {
                preview.entries = [];
                preview.lossless = false;
                preview.overflow = true;
            }
        }

        return preview;
    }

    _generatePreview(object, firstLevelKeys, secondLevelKeys)
    {
        let preview = this._initialPreview();
        let isTableRowsRequest = secondLevelKeys === null || secondLevelKeys;
        let firstLevelKeysCount = firstLevelKeys ? firstLevelKeys.length : 0;

        let propertiesThreshold = {
            properties: isTableRowsRequest ? 1000 : Math.max(5, firstLevelKeysCount),
            indexes: isTableRowsRequest ? 1000 : Math.max(10, firstLevelKeysCount)
        };

        try {
            // Maps, Sets, and Iterators have entries.
            if (this.subtype === "map" || this.subtype === "set" || this.subtype === "weakmap" || this.subtype === "weakset" || this.subtype === "iterator")
                this._appendEntryPreviews(object, preview);

            preview.properties = [];

            // Internal Properties.
            let internalPropertyDescriptors = injectedScript._internalPropertyDescriptors(object, true);
            if (internalPropertyDescriptors) {
                for (let i = 0; i < internalPropertyDescriptors.length; ++i) {
                    let result = this._appendPropertyPreview(object, preview, internalPropertyDescriptors[i], propertiesThreshold, firstLevelKeys, secondLevelKeys, {internal: true});
                    if (result === InjectedScript.PropertyFetchAction.Stop)
                        return preview;
                }
            }

            if (preview.entries)
                return preview;

            // Properties.
            injectedScript._forEachPropertyDescriptor(object, InjectedScript.CollectionMode.AllProperties, (descriptor) => {
                return this._appendPropertyPreview(object, preview, descriptor, propertiesThreshold, firstLevelKeys, secondLevelKeys);
            }, {nativeGettersAsValues: true, includeProto: true})
        } catch {
            preview.lossless = false;
        }

        return preview;
    }

    _appendPropertyPreview(object, preview, descriptor, propertiesThreshold, firstLevelKeys, secondLevelKeys, {internal} = {})
    {
        // Error in descriptor.
        if (descriptor.wasThrown) {
            preview.lossless = false;
            return InjectedScript.PropertyFetchAction.Continue;
        }

        // Do not show "__proto__" in preview.
        let name = descriptor.name;
        if (name === "__proto__") {
            // Non basic __proto__ objects may have interesting, non-enumerable, methods to show.
            if (descriptor.value && descriptor.value.constructor
                && descriptor.value.constructor !== Object
                && descriptor.value.constructor !== Array
                && descriptor.value.constructor !== RegExp)
                preview.lossless = false;
            return InjectedScript.PropertyFetchAction.Continue;
        }

        // For arrays, only allow indexes.
        if (this.subtype === "array" && !isUInt32(name))
            return InjectedScript.PropertyFetchAction.Continue;

        // Do not show non-enumerable non-own properties.
        // Special case to allow array indexes that may be on the prototype.
        // Special case to allow native getters on non-RegExp objects.
        if (!descriptor.enumerable && !descriptor.isOwn && !(this.subtype === "array" || (this.subtype !== "regexp" && descriptor.nativeGetter)))
            return InjectedScript.PropertyFetchAction.Continue;

        // If we have a filter, only show properties in the filter.
        // FIXME: Currently these filters do nothing on the backend.
        if (firstLevelKeys && !firstLevelKeys.includes(name))
            return InjectedScript.PropertyFetchAction.Continue;

        function appendPreview(property) {
            if (toString(property.name >>> 0) === property.name)
                propertiesThreshold.indexes--;
            else
                propertiesThreshold.properties--;

            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0) {
                preview.overflow = true;
                preview.lossless = false;
                return InjectedScript.PropertyFetchAction.Stop;
            }

            if (internal)
                property.internal = true;

            preview.properties.push(property);
            return InjectedScript.PropertyFetchAction.Continue;
        }

        // Getter/setter.
        if (!("value" in descriptor)) {
            preview.lossless = false;
            return appendPreview({name, type: "accessor"});
        }

        // Null value.
        let value = descriptor.value;
        if (value === null)
            return appendPreview({name, type: "object", subtype: "null", value: "null"});

        // Ignore non-enumerable functions.
        let type = typeof value;
        if (!descriptor.enumerable && type === "function")
            return InjectedScript.PropertyFetchAction.Continue;

        // Fix type of document.all.
        if (InjectedScriptHost.isHTMLAllCollection(value))
            type = "object";

        // Primitive.
        const maxLength = 100;
        if (isPrimitiveValue(value) || isBigInt(value)) {
            if (type === "string" && value.length > maxLength) {
                value = this._abbreviateString(value, maxLength, true);
                preview.lossless = false;
            }
            return appendPreview({name, type, value: toStringDescription(value)});
        }

        // Symbol.
        if (isSymbol(value)) {
            let symbolString = toString(value);
            if (symbolString.length > maxLength) {
                symbolString = this._abbreviateString(symbolString, maxLength, true);
                preview.lossless = false;
            }
            return appendPreview({name, type, value: symbolString});
        }

        // Object.
        let property = {name, type};
        let subtype = RemoteObject.subtype(value);
        if (subtype)
            property.subtype = subtype;

        // Second level.
        if ((secondLevelKeys === null || secondLevelKeys) || this._isPreviewableObject(value, object)) {
            // FIXME: If we want secondLevelKeys filter to continue we would need some refactoring.
            let subPreview = RemoteObject.createObjectPreviewForValue(value, value !== object, secondLevelKeys);
            property.valuePreview = subPreview;
            if (!subPreview.lossless)
                preview.lossless = false;
            if (subPreview.overflow)
                preview.overflow = true;
        } else {
            let description = "";
            if (type !== "function" || subtype === "class") {
                let fullDescription;
                if (subtype === "class")
                    fullDescription = "class " + value.name;
                else if (subtype === "node")
                    fullDescription = RemoteObject.nodePreview(value);
                else
                    fullDescription = RemoteObject.describe(value);
                description = this._abbreviateString(fullDescription, maxLength, subtype === "regexp");
            }
            property.value = description;
            preview.lossless = false;
        }

        return appendPreview(property);
    }

    _appendEntryPreviews(object, preview)
    {
        // Fetch 6, but only return 5, so we can tell if we overflowed.
        let entries = injectedScript._entries(object, this.subtype, 0, 6);
        if (!entries)
            return;

        if (entries.length > 5) {
            entries.pop();
            preview.overflow = true;
            preview.lossless = false;
        }

        function updateMainPreview(subPreview) {
            if (!subPreview.lossless)
                preview.lossless = false;
        }

        preview.entries = entries.map(function(entry) {
            entry.value = RemoteObject.createObjectPreviewForValue(entry.value, entry.value !== object);
            updateMainPreview(entry.value);
            if ("key" in entry) {
                entry.key = RemoteObject.createObjectPreviewForValue(entry.key, entry.key !== object);
                updateMainPreview(entry.key);
            }
            return entry;
        });
    }

    _isPreviewableObject(value, object)
    {
        let set = new Set;
        set.add(object);

        return this._isPreviewableObjectInternal(value, set, 1);
    }

    _isPreviewableObjectInternal(object, knownObjects, depth)
    {
        // Deep object.
        if (depth > 3)
            return false;

        // Primitive.
        if (isPrimitiveValue(object) || isBigInt(object) || isSymbol(object))
            return true;

        // Null.
        if (object === null)
            return true;

        // Cyclic objects.
        if (knownObjects.has(object))
            return false;

        ++depth;
        knownObjects.add(object);

        // Arrays are simple if they have 5 or less simple objects.
        let subtype = RemoteObject.subtype(object);
        if (subtype === "array") {
            let length = object.length;
            if (length > 5)
                return false;
            for (let i = 0; i < length; ++i) {
                if (!this._isPreviewableObjectInternal(object[i], knownObjects, depth))
                    return false;
            }
            return true;
        }

        // Not a basic object.
        if (object.__proto__ && object.__proto__.__proto__)
            return false;

        // Objects are simple if they have 3 or less simple value properties.
        let ownPropertyNames = Object.getOwnPropertyNames(object);
        if (ownPropertyNames.length > 3)
            return false;
        for (let i = 0; i < ownPropertyNames.length; ++i) {
            let propertyName = ownPropertyNames[i];
            let descriptor = Object.getOwnPropertyDescriptor(object, propertyName);
            if (descriptor && !("value" in descriptor))
                return false;
            if (!this._isPreviewableObjectInternal(object[propertyName], knownObjects, depth))
                return false;
        }

        return true;
    }

    _abbreviateString(string, maxLength, middle)
    {
        if (string.length <= maxLength)
            return string;

        if (middle) {
            let leftHalf = maxLength >> 1;
            let rightHalf = maxLength - leftHalf - 1;
            return string.substr(0, leftHalf) + "\u2026" + string.substr(string.length - rightHalf, rightHalf);
        }

        return string.substr(0, maxLength) + "\u2026";
    }
};

// -------

InjectedScript.CallFrameProxy = function(ordinal, callFrame)
{
    this.callFrameId = `{"ordinal":${ordinal},"injectedScriptId":${injectedScriptId}}`;
    this.functionName = callFrame.functionName;
    this.location = {scriptId: String(callFrame.sourceID), lineNumber: callFrame.line, columnNumber: callFrame.column};
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.this = RemoteObject.create(callFrame.thisObject, "backtrace");
    this.isTailDeleted = callFrame.isTailDeleted;
};

InjectedScript.CallFrameProxy.prototype = {
    _wrapScopeChain(callFrame)
    {
        let scopeChain = callFrame.scopeChain;
        let scopeDescriptions = callFrame.scopeDescriptions();

        let scopeChainProxy = [];
        for (let i = 0; i < scopeChain.length; i++)
            scopeChainProxy[i] = InjectedScript.CallFrameProxy._createScopeJson(scopeChain[i], scopeDescriptions[i], "backtrace");
        return scopeChainProxy;
    }
};

InjectedScript.CallFrameProxy._scopeTypeNames = {
    0: "global", // GLOBAL_SCOPE
    1: "with", // WITH_SCOPE
    2: "closure", // CLOSURE_SCOPE
    3: "catch", // CATCH_SCOPE
    4: "functionName", // FUNCTION_NAME_SCOPE
    5: "globalLexicalEnvironment", // GLOBAL_LEXICAL_ENVIRONMENT_SCOPE
    6: "nestedLexical", // NESTED_LEXICAL_SCOPE
};

InjectedScript.CallFrameProxy._createScopeJson = function(object, {name, type, location}, groupId)
{
    let scope = {
        object: RemoteObject.create(object, groupId),
        type: InjectedScript.CallFrameProxy._scopeTypeNames[type],
    };

    if (name)
        scope.name = name;

    if (location)
        scope.location = location;

    if (isEmptyObject(object))
        scope.empty = true;

    return scope;
}

// -------

function CommandLineAPI(callFrame)
{
    let savedResultAlias = InjectedScriptHost.savedResultAlias;

    let defineGetter = (key, value, wrap) => {
        if (wrap) {
            let originalValue = value;
            value = function() { return originalValue; };
        }

        this.__defineGetter__("$" + key, value);
        if (savedResultAlias && savedResultAlias !== "$")
            this.__defineGetter__(savedResultAlias + key, value);
    };

    if ("_lastResult" in injectedScript)
        defineGetter("_", injectedScript._lastResult, true);

    if ("_exceptionValue" in injectedScript)
        defineGetter("exception", injectedScript._exceptionValue, true);

    if ("_eventValue" in injectedScript)
        defineGetter("event", injectedScript._eventValue, true);

    // $1-$99
    for (let i = 1; i < injectedScript._savedResults.length; ++i)
        defineGetter(i, injectedScript._savedResults[i], true);

    for (let name in CommandLineAPI.getters)
        defineGetter(name, CommandLineAPI.getters[name]);

    for (let name in CommandLineAPI.methods)
        this[name] = CommandLineAPI.methods[name];
}

CommandLineAPI.getters = {};

CommandLineAPI.methods = {};

CommandLineAPI.methods["keys"] = function(object) { return Object.keys(object); };
CommandLineAPI.methods["values"] = function(object) { return Object.values(object); };

CommandLineAPI.methods["queryInstances"] = function() { return InjectedScriptHost.queryInstances(...arguments); };
CommandLineAPI.methods["queryObjects"] = function() { return InjectedScriptHost.queryInstances(...arguments); };
CommandLineAPI.methods["queryHolders"] = function() { return InjectedScriptHost.queryHolders(...arguments); };

CommandLineAPI.methods["inspect"] = function(object) { return injectedScript.inspectObject(object); };

CommandLineAPI.methods["assert"] = function() { return inspectedGlobalObject.console.assert(...arguments); };
CommandLineAPI.methods["clear"] = function() { return inspectedGlobalObject.console.clear(...arguments); };
CommandLineAPI.methods["count"] = function() { return inspectedGlobalObject.console.count(...arguments); };
CommandLineAPI.methods["countReset"] = function() { return inspectedGlobalObject.console.countReset(...arguments); };
CommandLineAPI.methods["debug"] = function() { return inspectedGlobalObject.console.debug(...arguments); };
CommandLineAPI.methods["dir"] = function() { return inspectedGlobalObject.console.dir(...arguments); };
CommandLineAPI.methods["dirxml"] = function() { return inspectedGlobalObject.console.dirxml(...arguments); };
CommandLineAPI.methods["error"] = function() { return inspectedGlobalObject.console.error(...arguments); };
CommandLineAPI.methods["group"] = function() { return inspectedGlobalObject.console.group(...arguments); };
CommandLineAPI.methods["groupCollapsed"] = function() { return inspectedGlobalObject.console.groupCollapsed(...arguments); };
CommandLineAPI.methods["groupEnd"] = function() { return inspectedGlobalObject.console.groupEnd(...arguments); };
CommandLineAPI.methods["info"] = function() { return inspectedGlobalObject.console.info(...arguments); };
CommandLineAPI.methods["log"] = function() { return inspectedGlobalObject.console.log(...arguments); };
CommandLineAPI.methods["profile"] = function() { return inspectedGlobalObject.console.profile(...arguments); };
CommandLineAPI.methods["profileEnd"] = function() { return inspectedGlobalObject.console.profileEnd(...arguments); };
CommandLineAPI.methods["record"] = function() { return inspectedGlobalObject.console.record(...arguments); };
CommandLineAPI.methods["recordEnd"] = function() { return inspectedGlobalObject.console.recordEnd(...arguments); };
CommandLineAPI.methods["screenshot"] = function() { return inspectedGlobalObject.console.screenshot(...arguments); };
CommandLineAPI.methods["table"] = function() { return inspectedGlobalObject.console.table(...arguments); };
CommandLineAPI.methods["takeHeapSnapshot"] = function() { return inspectedGlobalObject.console.takeHeapSnapshot(...arguments); };
CommandLineAPI.methods["time"] = function() { return inspectedGlobalObject.console.time(...arguments); };
CommandLineAPI.methods["timeEnd"] = function() { return inspectedGlobalObject.console.timeEnd(...arguments); };
CommandLineAPI.methods["timeLog"] = function() { return inspectedGlobalObject.console.timeLog(...arguments); };
CommandLineAPI.methods["timeStamp"] = function() { return inspectedGlobalObject.console.timeStamp(...arguments); };
CommandLineAPI.methods["trace"] = function() { return inspectedGlobalObject.console.trace(...arguments); };
CommandLineAPI.methods["warn"] = function() { return inspectedGlobalObject.console.warn(...arguments); };

for (let name in CommandLineAPI.methods)
    CommandLineAPI.methods[name].toString = function() { return "function " + name + "() { [Command Line API] }"; };

return injectedScript;
})
