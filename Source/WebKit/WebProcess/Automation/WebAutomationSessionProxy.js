/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

//# sourceURL=__InjectedScript_WebAutomationSessionProxy.js

(function (sessionIdentifier, currentFrameIdentifier, evaluate, createUUID, isValidNodeIdentifier, isKnownReference, addKnownReference, shadowRootForElement, specialBidiRemoteValueType) {

const sessionNodePropertyName = "session-node-" + sessionIdentifier;
const BigIntConstructor = BigInt;
const DateConstructor = Date;
const MapConstructor = Map;
const RegExpConstructor = RegExp;
const SetConstructor = Set;
const arrayIsArray = Array.isArray;
const dateGetTime = Date.prototype.getTime;
const jsonStringify = JSON.stringify;
const mapForEach = Map.prototype.forEach;
const mapGet = Map.prototype.get;
const mapSet = Map.prototype.set;
const objectDefineProperty = Object.defineProperty;
const objectHasOwn = Object.hasOwn;
const reflectApply = Reflect.apply;
const regExpExec = RegExp.prototype.exec;
const setAdd = Set.prototype.add;
const setForEach = Set.prototype.forEach;

// https://tc39.es/ecma262/#sec-date-time-string-format
const dateTimeStringFormat = /^(?!-000000)(?:\d{4}|[+-]\d{6})(?:-\d{2}(?:-\d{2})?)?(?:T\d{2}:\d{2}(?::\d{2}(?:\.\d{3})?)?(?:Z|[+-]\d{2}:\d{2})?)?$/;

function safelyReadStringProperty(value, property)
{
    if (value === null || value === undefined)
        return undefined;

    try {
        let propertyValue = value[property];
        if (typeof propertyValue === "string")
            return propertyValue;
    } catch { }

    return undefined;
}

function createBidiScriptError(error, name, fallbackMessage)
{
    let message = typeof error === "string" ? error : safelyReadStringProperty(error, "message");
    return {
        name,
        message: message || fallbackMessage
    };
}

function createDataProperty(object, key, value)
{
    objectDefineProperty(object, key, { __proto__: null, value, writable: true, enumerable: true, configurable: true });
}

let AutomationSessionProxy = class AutomationSessionProxy
{
    constructor()
    {
        // FIXME: <https://webkit.org/b/328548> Use weak node-reference storage for Classic and BiDi.
        this._nodeToIdMap = new Map;
        this._idToNodeMap = new Map;
        this._staleIdentifiers = new Set;
    }

    // Public

    evaluateJavaScriptFunction(functionString, argumentStrings, expectsImplicitCallbackArgument, forceUserGesture, frameID, callbackID, resultCallback, callbackTimeout)
    {
        this._execute(functionString, argumentStrings, expectsImplicitCallbackArgument, callbackTimeout)
            .then(result => { resultCallback(frameID, callbackID, this._jsonStringify(result)); })
            .catch(error => { resultCallback(frameID, callbackID, error); });
    }

    evaluateBidiScript(expression, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout)
    {
        this._runBidiScript(() => globalThis.eval(expression), awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout);
    }

    callBidiFunction(functionDeclaration, argumentLocalValues, thisLocalValue, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout)
    {
        // https://w3c.github.io/webdriver-bidi/#command-script-callFunction
        let argumentValues = this._deserializeBidiList(argumentLocalValues);
        let thisValue = thisLocalValue === undefined ? null : this._deserializeBidiLocalValue(thisLocalValue);

        let functionValue;
        try {
            functionValue = evaluate("(" + functionDeclaration + ")");
        } catch (error) {
            this._runBidiScript(() => { throw error; }, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout);
            return;
        }

        if (typeof functionValue !== "function") {
            resultCallback(frameID, callbackID, createBidiScriptError("functionDeclaration did not evaluate to a function", "InvalidParameter"));
            return;
        }

        this._runBidiScript(() => reflectApply(functionValue, thisValue, argumentValues), awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout);
    }

    _runBidiScript(evaluateScript, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout)
    {
        this._executeBidiScript(evaluateScript, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, callbackTimeout).then(result => {
            let serializedResult;
            let serializationError;
            try {
                serializedResult = jsonStringify(result);
            } catch (error) {
                serializationError = createBidiScriptError(error, "InternalError", "Script result serialization failed.");
            }

            if (serializationError) {
                resultCallback(frameID, callbackID, serializationError);
                return;
            }

            if (typeof serializedResult !== "string") {
                resultCallback(frameID, callbackID, createBidiScriptError(undefined, "InternalError", "Script result serialization failed."));
                return;
            }

            resultCallback(frameID, callbackID, serializedResult);
        }, error => {
            let name = safelyReadStringProperty(error, "name");
            if (name !== "JavaScriptTimeout")
                name = "InternalError";
            let fallbackMessage = name === "JavaScriptTimeout" ? "Script evaluation timed out." : "Script evaluation failed.";
            resultCallback(frameID, callbackID, createBidiScriptError(error, name, fallbackMessage));
        });
    }

    nodeForIdentifier(identifier)
    {
        this._clearStaleNodes();
        return this._nodeForIdentifier(identifier);
    }

    // Private

    _execute(functionString, argumentStrings, expectsImplicitCallbackArgument, callbackTimeout)
    {
        let timeoutPromise;
        let timeoutIdentifier = 0;
        if (callbackTimeout >= 0) {
            timeoutPromise = new Promise((resolve, reject) => {
                timeoutIdentifier = setTimeout(() => {
                    reject({ name: "JavaScriptTimeout", message: `script timed out after ${callbackTimeout}ms` });
                }, callbackTimeout);
            });
        }

        let promise = new Promise((resolve, reject) => {
            // Split initial line comments like "//# __injectedScript" source map that would break the async expression below.
            let lines = functionString.split("\n");
            let prefixLines = [];
            while (lines && lines[0].startsWith("//")) {
                prefixLines.push(lines.shift());
            }
            functionString = lines.join("\n");

            let prefix = prefixLines.join("\n")
            if (prefix)
                prefix += "\n";

            // The script is expected to be a function declaration. Evaluate it inside parenthesis to get the function value.
            let functionValue = evaluate(prefix + "(async " + functionString + ")");
            if (typeof functionValue !== "function")
                reject(new TypeError("Script did not evaluate to a function."));

            this._clearStaleNodes();

            let argumentValues = argumentStrings.map(this._jsonParse, this);
            if (expectsImplicitCallbackArgument)
                argumentValues.push(resolve);
            let resultPromise = functionValue.apply(null, argumentValues);

            let promises = [resultPromise];
            if (timeoutPromise)
                promises.push(timeoutPromise);
            Promise.race(promises)
                .then(result => {
                    if (!expectsImplicitCallbackArgument) {
                        resolve(result);
                    }
                })
                .catch(error => {
                    reject(error);
                });
        });

        // Async scripts can call Promise.resolve() in the function script, generating a new promise that is resolved in a
        // timer (see w3c test execute_async_script/promise.py::test_promise_resolve_timeout). In that case, the internal race
        // finishes resolved, so we need to start a new one here to wait for the second promise to be resolved or the timeout.
        let promises = [promise];
        if (timeoutPromise)
            promises.push(timeoutPromise);
        return Promise.race(promises)
            .finally(() => {
                if (timeoutIdentifier) {
                    clearTimeout(timeoutIdentifier);
                }
            });
    }

    _executeBidiScript(evaluateScript, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, callbackTimeout)
    {
        let timeoutPromise;
        let timeoutIdentifier = 0;
        if (callbackTimeout >= 0) {
            timeoutPromise = new Promise((resolve, reject) => {
                timeoutIdentifier = setTimeout(() => {
                    reject({ name: "JavaScriptTimeout", message: `script timed out after ${callbackTimeout}ms` });
                }, callbackTimeout);
            });
        }

        let promise = new Promise((resolve, reject) => {
            this._clearStaleNodes();

            let resolveWithExceptionResult = error => {
                let serializedException;
                try {
                    serializedException = this.serializeBidiRemoteValue(error);
                } catch (serializationError) {
                    reject(createBidiScriptError(serializationError, "InternalError", "Script exception serialization failed."));
                    return;
                }

                let errorProperties = {
                    exception: serializedException,
                    name: safelyReadStringProperty(error, "name"),
                    message: safelyReadStringProperty(error, "message"),
                    stack: safelyReadStringProperty(error, "stack")
                };
                resolve({ success: false, error: errorProperties });
            };

            let serializeAndResolveResult = value => {
                try {
                    let serializedValue = this.serializeBidiRemoteValue(value, maxObjectDepth, maxDomDepth, includeShadowTree);
                    resolve({ success: true, result: serializedValue });
                } catch (error) {
                    reject(createBidiScriptError(error, "InternalError", "Script result serialization failed."));
                }
            };

            let result;
            try {
                result = evaluateScript();
            } catch (error) {
                resolveWithExceptionResult(error);
                return;
            }

            if (awaitPromise && result) {
                try {
                    let then = result.then;
                    if (typeof then === "function") {
                        reflectApply(then, result, [serializeAndResolveResult, resolveWithExceptionResult]);
                        return;
                    }
                } catch (error) {
                    resolveWithExceptionResult(error);
                    return;
                }
            }

            serializeAndResolveResult(result);
        });

        // FIXME: <https://webkit.org/b/328581> Prevent page-defined array iterators from affecting script.evaluate.
        let promises = [promise];
        if (timeoutPromise)
            promises.push(timeoutPromise);
        return Promise.race(promises)
            .finally(() => {
                if (timeoutIdentifier) {
                    clearTimeout(timeoutIdentifier);
                }
            });
    }

    _deserializeBidiLocalValue(localValue)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-local-value
        if (localValue === null || typeof localValue !== "object" || arrayIsArray(localValue))
            throw createBidiScriptError("LocalValue must be an object", "InvalidParameter");

        if (objectHasOwn(localValue, "sharedId") || objectHasOwn(localValue, "handle"))
            return this._deserializeBidiRemoteReference(localValue);

        let type = objectHasOwn(localValue, "type") ? localValue.type : undefined;
        let value = objectHasOwn(localValue, "value") ? localValue.value : undefined;

        switch (type) {
        case "undefined":
            return undefined;
        case "null":
            return null;
        case "string":
            if (typeof value === "string")
                return value;
            break;
        case "number":
            if (typeof value === "number")
                return value;
            switch (value) {
            case "NaN":
                return NaN;
            case "-0":
                return -0;
            case "Infinity":
                return Infinity;
            case "-Infinity":
                return -Infinity;
            }
            break;
        case "boolean":
            if (typeof value === "boolean")
                return value;
            break;
        case "bigint":
            if (typeof value !== "string")
                break;
            try {
                return BigIntConstructor(value);
            } catch {
                throw createBidiScriptError("Invalid bigint LocalValue", "InvalidParameter");
            }
        case "date": {
            if (typeof value !== "string" || reflectApply(regExpExec, dateTimeStringFormat, [value]) === null)
                break;
            let date = new DateConstructor(value);
            let time = reflectApply(dateGetTime, date, []);
            if (time !== time)
                break;
            return date;
        }
        case "regexp": {
            if (value === null || typeof value !== "object" || arrayIsArray(value))
                break;
            let pattern = objectHasOwn(value, "pattern") ? value.pattern : undefined;
            let flags = objectHasOwn(value, "flags") ? value.flags : undefined;
            if (typeof pattern !== "string" || (flags !== undefined && typeof flags !== "string"))
                break;
            try {
                return new RegExpConstructor(pattern, flags);
            } catch {
                throw createBidiScriptError("Invalid regexp LocalValue", "InvalidParameter");
            }
        }
        case "array":
            return this._deserializeBidiList(value);
        case "set": {
            let items = this._deserializeBidiList(value);
            let set = new SetConstructor;
            for (let i = 0; i < items.length; ++i)
                reflectApply(setAdd, set, [items[i]]);
            return set;
        }
        case "object": {
            let object = {};
            this._deserializeBidiMapping(value, (key, entryValue) => { createDataProperty(object, key, entryValue); });
            return object;
        }
        case "map": {
            let map = new MapConstructor;
            this._deserializeBidiMapping(value, (key, entryValue) => { reflectApply(mapSet, map, [key, entryValue]); });
            return map;
        }
        case "channel":
            if (value === null || typeof value !== "object" || arrayIsArray(value) || !objectHasOwn(value, "channel") || typeof value.channel !== "string")
                break;
            // FIXME: Create channels for ChannelValue arguments. https://bugs.webkit.org/show_bug.cgi?id=288057
            throw createBidiScriptError("ChannelValue is not supported", "NotImplemented");
        }

        throw createBidiScriptError(typeof type === "string" ? "Invalid LocalValue of type \"" + type + "\"" : "LocalValue has no valid type", "InvalidParameter");
    }

    _deserializeBidiRemoteReference(remoteReference)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-remote-reference
        let hasSharedId = objectHasOwn(remoteReference, "sharedId");
        if (hasSharedId && typeof remoteReference.sharedId !== "string")
            throw createBidiScriptError("sharedId must be a string", "InvalidParameter");
        if (objectHasOwn(remoteReference, "handle") && typeof remoteReference.handle !== "string")
            throw createBidiScriptError("handle must be a string", "InvalidParameter");

        if (hasSharedId)
            return this._deserializeBidiSharedReference(remoteReference.sharedId);

        // FIXME: Create handles for resultOwnership "root". https://bugs.webkit.org/show_bug.cgi?id=288059
        throw createBidiScriptError("Handle is not supported", "NotImplemented");
    }

    _deserializeBidiSharedReference(sharedId)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-shared-reference
        let node = reflectApply(mapGet, this._idToNodeMap, [sharedId]);
        if (!node || (node !== document && node.ownerDocument !== document))
            throw createBidiScriptError("Node with sharedId '" + sharedId + "' was not found", "NodeNotFound");
        return node;
    }

    _deserializeBidiList(list)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-value-list
        if (!arrayIsArray(list))
            throw createBidiScriptError("List LocalValue must be an array", "InvalidParameter");

        let values = [];
        for (let i = 0; i < list.length; ++i)
            createDataProperty(values, i, this._deserializeBidiLocalValue(list[i]));
        return values;
    }

    _deserializeBidiMapping(mapping, addEntry)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-key-value-list
        if (!arrayIsArray(mapping))
            throw createBidiScriptError("Mapping LocalValue must be an array", "InvalidParameter");

        for (let i = 0; i < mapping.length; ++i) {
            let entry = mapping[i];
            if (!arrayIsArray(entry) || entry.length !== 2)
                throw createBidiScriptError("Mapping LocalValue entries must have two items", "InvalidParameter");
            let key = typeof entry[0] === "string" ? entry[0] : this._deserializeBidiLocalValue(entry[0]);
            addEntry(key, this._deserializeBidiLocalValue(entry[1]));
        }
    }

    _jsonParse(string)
    {
        if (!string)
            return undefined;
        return JSON.parse(string, (key, value) => this._reviveJSONValue(key, value));
    }

    _jsonStringify(value)
    {
        return JSON.stringify(this._jsonClone(value));
    }

    _reviveJSONValue(key, value)
    {
        if (value && typeof value === "object" && value[sessionNodePropertyName])
            return this._nodeForIdentifier(value[sessionNodePropertyName]);
        return value;
    }

    _isCollection(value) {
        switch (Object.prototype.toString.call(value)) {
        case "[object Arguments]":
        case "[object Array]":
        case "[object FileList]":
        case "[object HTMLAllCollection]":
        case "[object HTMLCollection]":
        case "[object HTMLFormControlsCollection]":
        case "[object HTMLOptionsCollection]":
        case "[object NodeList]":
            return true;
        }
        return false;
    }

    _checkCyclic(value, stack = [])
    {
        function isCyclic(value, proxy, stack = []) {
            if (value === undefined || value === null)
                return false;

            if (typeof value === "boolean" || typeof value === "number" || typeof value === "string")
                return false;

            if (value instanceof Node)
                return false;

            if (stack.includes(value))
                return true;

            if (proxy._isCollection(value)) {
                stack.push(value);
                for (let i = 0; i < value.length; i++) {
                    if (isCyclic(value[i], proxy, stack))
                        return true;
                }

                stack.pop();
                return false;
            }

            stack.push(value);
            for (let property in value) {
                if (isCyclic(value[property], proxy, stack))
                    return true;
            }

            stack.pop();
            return false;
        }

        if (isCyclic(value, this))
            throw new TypeError("cannot serialize cyclic structures.");
    }

    _jsonClone(value)
    {
        // Internal JSON clone algorithm.
        // https://w3c.github.io/webdriver/#dfn-internal-json-clone-algorithm
        if (value === undefined || value === null)
            return null;

        if (typeof value === "boolean" || typeof value === "number" || typeof value === "string")
            return value;

        if (this._isCollection(value)) {
            this._checkCyclic(value);
            return [...value].map(item => this._jsonClone(item));
        }

        if (value instanceof Node)
            return this._createNodeHandle(value);

        // FIXME: implement window proxy serialization.

        if (typeof value.toJSON === "function")
            return value.toJSON();

        let customObject = {};
        for (let property in value) {
            this._checkCyclic(value);
            customObject[property] = this._jsonClone(value[property]);
        }
        return customObject;
    }

    _createNodeHandle(node)
    {
        if (node.ownerDocument !== window.document || !node.isConnected) {
            // https://w3c.github.io/webdriver/#dfn-internal-json-clone
            if (node instanceof Element)
                throw {name: "StaleNode", message: "Stale element found when trying to create the node handle"};
            throw {name: "NodeNotFound", message: "Stale node found when trying to create the node handle"};
        }

        return {[sessionNodePropertyName]: this._identifierForNode(node)};
    }

    _nodeForIdentifier(identifier)
    {
        if (!isValidNodeIdentifier(identifier))
            throw {name: "InvalidNodeIdentifier", message: "Node identifier '" + identifier + "' is invalid"};

        let node = this._idToNodeMap.get(identifier);
        if (node)
            return node;

        // A node this frame knew about and then evicted is stale. One it never knew about
        // belongs to a different browsing context, which callers must distinguish.
        if (this._staleIdentifiers.has(identifier) || isKnownReference(currentFrameIdentifier, identifier))
            throw {name: "StaleNode", message: "Node with identifier '" + identifier + "' is stale"};
        throw {name: "NodeNotFound", message: "Node with identifier '" + identifier + "' was not found"};
    }

    _identifierForNode(node)
    {
        let identifier = this._nodeToIdMap.get(node);
        if (identifier)
            return identifier;

        identifier = "node-" + createUUID();

        this._nodeToIdMap.set(node, identifier);
        this._idToNodeMap.set(identifier, node);
        addKnownReference(currentFrameIdentifier, identifier);

        return identifier;
    }

    _clearStaleNodes()
    {
        reflectApply(mapForEach, this._nodeToIdMap, [(identifier, node) => {
            const rootNode = node.getRootNode({ composed: true });
            if (rootNode !== document) {
                this._nodeToIdMap.delete(node);
                this._idToNodeMap.delete(identifier);
                this._staleIdentifiers.add(identifier);
            }
        }]);
    }

    // BiDi Script utilities for W3C WebDriver BiDi specification.
    serializeBidiRemoteValue(value, maxObjectDepth = null, maxDomDepth = 0, includeShadowTree = "none", serializationInternalMap = new Map)
    {
        // Handle primitives.
        if (value === null) return { type: "null" };
        if (value === undefined) return { type: "undefined" };
        if (typeof value === "boolean") return { type: "boolean", value: value };
        if (typeof value === "string") return { type: "string", value: value };
        if (typeof value === "number") {
            if (isNaN(value)) return { type: "number", value: "NaN" };
            if (value === Infinity) return { type: "number", value: "Infinity" };
            if (value === -Infinity) return { type: "number", value: "-Infinity" };
            if (Object.is(value, -0)) return { type: "number", value: "-0" };
            return { type: "number", value: value };
        }
        if (typeof value === "bigint") return { type: "bigint", value: value.toString() };
        if (typeof value === "symbol") return { type: "symbol" };

        let specialType = specialBidiRemoteValueType(value);
        if (specialType !== null)
            return { type: specialType };

        if (typeof value === "function") return { type: "function" };

        if (typeof value !== "object")
            return { type: "undefined" };

        // FIXME: <https://webkit.org/b/328591> Recognize WindowProxy objects for child browsing contexts.
        if (value === window)
            return { type: "window", value: { context: "context-id-placeholder" } };

        // FIXME: <https://webkit.org/b/328651> Recognize objects, including DOM nodes, from other JavaScript realms.
        if (value instanceof Node)
            return this._serializeBidiNode(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        if (value instanceof Date)
            return { type: "date", value: value.toISOString() };
        if (value instanceof RegExp)
            return { type: "regexp", value: { pattern: value.source, flags: value.flags } };
        if (value instanceof Error)
            return { type: "error" };
        if (value instanceof Promise)
            return { type: "promise" };
        if (Array.isArray(value))
            return this._serializeBidiList(value, "array", maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        if (value instanceof Map)
            return this._serializeBidiMap(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        if (value instanceof Set)
            return this._serializeBidiList(value, "set", maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        if (value instanceof NodeList)
            return this._serializeBidiList(value, "nodelist", maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        if (value instanceof HTMLCollection)
            return this._serializeBidiList(value, "htmlcollection", maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        if (value instanceof WeakMap)
            return { type: "weakmap" };
        if (value instanceof WeakSet)
            return { type: "weakset" };
        if (value instanceof ArrayBuffer)
            return { type: "arraybuffer" };
        if (ArrayBuffer.isView(value))
            return { type: "typedarray" };

        return this._serializeBidiMapping(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
    }

    _setBidiInternalIdsIfNeeded(serializationInternalMap, remoteValue, value)
    {
        if (!serializationInternalMap.has(value)) {
            serializationInternalMap.set(value, remoteValue);
            return;
        }

        let previouslySerializedRemoteValue = serializationInternalMap.get(value);
        if (!previouslySerializedRemoteValue.internalId)
            previouslySerializedRemoteValue.internalId = createUUID();
        remoteValue.internalId = previouslySerializedRemoteValue.internalId;
    }

    _serializeBidiList(value, type, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap)
    {
        let remoteValue = { type };
        let knownObject = serializationInternalMap.has(value);
        this._setBidiInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
        if (knownObject || maxObjectDepth === 0)
            return remoteValue;

        let childObjectDepth = maxObjectDepth === null ? null : maxObjectDepth - 1;
        remoteValue.value = [];
        if (type === "set") {
            reflectApply(setForEach, value, [childValue => {
                remoteValue.value.push(this.serializeBidiRemoteValue(childValue, childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap));
            }]);
            return remoteValue;
        }

        for (let i = 0; i < value.length; ++i)
            remoteValue.value.push(this.serializeBidiRemoteValue(value[i], childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap));
        return remoteValue;
    }

    _serializeBidiMap(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap)
    {
        let remoteValue = { type: "map" };
        let knownObject = serializationInternalMap.has(value);
        this._setBidiInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
        if (knownObject || maxObjectDepth === 0)
            return remoteValue;

        let childObjectDepth = maxObjectDepth === null ? null : maxObjectDepth - 1;
        remoteValue.value = [];
        reflectApply(mapForEach, value, [(childValue, key) => {
            let serializedKey = typeof key === "string" ? key : this.serializeBidiRemoteValue(key, childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
            let serializedValue = this.serializeBidiRemoteValue(childValue, childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
            remoteValue.value.push([serializedKey, serializedValue]);
        }]);
        return remoteValue;
    }

    _serializeBidiMapping(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap)
    {
        let remoteValue = { type: "object" };
        let knownObject = serializationInternalMap.has(value);
        this._setBidiInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
        if (knownObject || maxObjectDepth === 0)
            return remoteValue;

        let childObjectDepth = maxObjectDepth === null ? null : maxObjectDepth - 1;
        remoteValue.value = [];
        let keys = Object.keys(value);
        for (let i = 0; i < keys.length; ++i) {
            let key = keys[i];
            let serializedValue = this.serializeBidiRemoteValue(value[key], childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
            remoteValue.value.push([key, serializedValue]);
        }
        return remoteValue;
    }

    _serializeBidiNode(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap)
    {
        let remoteValue = { type: "node" };
        let nodeDocument = value.nodeType === Node.DOCUMENT_NODE ? value : value.ownerDocument;
        if (nodeDocument === document)
            remoteValue.sharedId = this._identifierForNode(value);

        let knownObject = serializationInternalMap.has(value);
        this._setBidiInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
        if (knownObject)
            return remoteValue;

        let serialized = {
            nodeType: value.nodeType,
            childNodeCount: value.childNodes.length
        };

        if (value.nodeValue !== null)
            serialized.nodeValue = value.nodeValue;

        if (value instanceof Element || value instanceof Attr) {
            serialized.localName = value.localName;
            serialized.namespaceURI = value.namespaceURI;
        }

        let shouldSerializeChildren = maxDomDepth !== 0;
        if (value instanceof ShadowRoot) {
            if (includeShadowTree === "none" || (includeShadowTree === "open" && value.mode === "closed"))
                shouldSerializeChildren = false;
        }

        if (shouldSerializeChildren) {
            let childDomDepth = maxDomDepth === null ? null : maxDomDepth - 1;
            serialized.children = [];
            let childNodes = value.childNodes;
            for (let i = 0; i < childNodes.length; ++i)
                serialized.children.push(this.serializeBidiRemoteValue(childNodes[i], maxObjectDepth, childDomDepth, includeShadowTree, serializationInternalMap));
        }

        if (value instanceof Element) {
            serialized.attributes = {};
            for (let i = 0; i < value.attributes.length; ++i) {
                let attribute = value.attributes[i];
                serialized.attributes[attribute.name] = attribute.value;
            }

            // FIXME: <https://webkit.org/b/328583> Prevent script.evaluate result serialization from exposing closed shadow roots to page code.
            let shadowRoot = shadowRootForElement(value);
            serialized.shadowRoot = shadowRoot ? this.serializeBidiRemoteValue(shadowRoot, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap) : null;
        }

        if (value instanceof ShadowRoot)
            serialized.mode = value.mode;

        remoteValue.value = serialized;
        return remoteValue;
    }
};

return new AutomationSessionProxy;

})
