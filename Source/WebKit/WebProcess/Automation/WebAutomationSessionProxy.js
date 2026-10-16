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

(function (sessionIdentifier, currentFrameIdentifier, evaluate, createUUID, isValidNodeIdentifier, isKnownReference, addKnownReference, openOrClosedShadowRoot) {

const sessionNodePropertyName = "session-node-" + sessionIdentifier;

const reflectApply = Reflect.apply;
const objectDefineProperty = Object.defineProperty;
const objectHasOwn = Object.hasOwn;
const arrayIsArray = Array.isArray;
const MapConstructor = Map;
const mapPrototypeGet = Map.prototype.get;
const mapPrototypeHas = Map.prototype.has;
const mapPrototypeSet = Map.prototype.set;
const SetConstructor = Set;
const setPrototypeAdd = Set.prototype.add;
const PromiseConstructor = Promise;
const promisePrototypeThen = Promise.prototype.then;
const BigIntConstructor = BigInt;
const RegExpConstructor = RegExp;
const regExpPrototypeExec = RegExp.prototype.exec;
const DateConstructor = Date;
const datePrototypeGetTime = Date.prototype.getTime;
const StringConstructor = String;
const stringPrototypeStartsWith = String.prototype.startsWith;
const stringPrototypeIndexOf = String.prototype.indexOf;
const stringPrototypeSlice = String.prototype.slice;
const jsonStringify = JSON.stringify;

// https://tc39.es/ecma262/#sec-date-time-string-format
const dateTimeStringFormat = /^(?!-000000)(?:\d{4}|[+-]\d{6})(?:-\d{2}(?:-\d{2})?)?(?:T\d{2}:\d{2}(?::\d{2}(?:\.\d{3})?)?(?:Z|[+-]\d{2}:\d{2})?)?$/;

const bidiProtocolErrorMarker = Symbol("bidiProtocolError");

function bidiProtocolError(name, message)
{
    return { name, message, [bidiProtocolErrorMarker]: true };
}

function invalidBidiArgument(message)
{
    return bidiProtocolError("InvalidParameter", message);
}

function createDataProperty(object, key, value)
{
    objectDefineProperty(object, key, { __proto__: null, value, writable: true, enumerable: true, configurable: true });
}

let AutomationSessionProxy = class AutomationSessionProxy
{
    constructor()
    {
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

    evaluateBidiScript(expression, awaitPromise, serializationOptions, frameID, callbackID, resultCallback, callbackTimeout)
    {
        // https://w3c.github.io/webdriver-bidi/#command-script-evaluate
        let finish = this._createBidiScriptFinisher(frameID, callbackID, resultCallback, callbackTimeout);

        let result;
        try {
            result = evaluate(expression);
        } catch (error) {
            this._finishBidiScriptWithException(error, finish);
            return;
        }

        this._finishBidiScriptWithResult(result, awaitPromise, serializationOptions, finish);
    }

    callBidiFunction(functionDeclaration, argumentLocalValues, thisLocalValue, awaitPromise, serializationOptions, frameID, callbackID, resultCallback, callbackTimeout)
    {
        // https://w3c.github.io/webdriver-bidi/#command-script-callFunction
        let finish = this._createBidiScriptFinisher(frameID, callbackID, resultCallback, callbackTimeout);

        let argumentValues;
        let thisValue = null;
        try {
            argumentValues = this._deserializeBidiList(argumentLocalValues);
            if (thisLocalValue !== undefined)
                thisValue = this._deserializeBidiLocalValue(thisLocalValue);
        } catch (error) {
            finish(this._bidiDeserializationError(error));
            return;
        }

        let functionValue;
        try {
            functionValue = evaluate(this._parenthesizedFunctionDeclaration(functionDeclaration));
        } catch (error) {
            this._finishBidiScriptWithException(error, finish);
            return;
        }

        if (typeof functionValue !== "function") {
            finish(invalidBidiArgument("functionDeclaration did not evaluate to a function"));
            return;
        }

        let result;
        try {
            result = reflectApply(functionValue, thisValue, argumentValues);
        } catch (error) {
            this._finishBidiScriptWithException(error, finish);
            return;
        }

        this._finishBidiScriptWithResult(result, awaitPromise, serializationOptions, finish);
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

    _createBidiScriptFinisher(frameID, callbackID, resultCallback, callbackTimeout)
    {
        let finished = false;
        let timeoutIdentifier = 0;
        let finish = result => {
            if (finished)
                return;
            finished = true;
            if (timeoutIdentifier)
                clearTimeout(timeoutIdentifier);
            resultCallback(frameID, callbackID, result);
        };

        if (callbackTimeout >= 0) {
            timeoutIdentifier = setTimeout(() => {
                finish(bidiProtocolError("JavaScriptTimeout", `script timed out after ${callbackTimeout}ms`));
            }, callbackTimeout);
        }

        return finish;
    }

    _finishBidiScriptWithResult(result, awaitPromise, serializationOptions, finish)
    {
        try {
            if (awaitPromise && result instanceof PromiseConstructor) {
                reflectApply(promisePrototypeThen, result, [
                    value => { this._finishBidiScriptWithValue(value, serializationOptions, finish); },
                    error => { this._finishBidiScriptWithException(error, finish); },
                ]);
                return;
            }
        } catch (error) {
            this._finishBidiScriptWithException(error, finish);
            return;
        }

        this._finishBidiScriptWithValue(result, serializationOptions, finish);
    }

    _finishBidiScriptWithValue(value, serializationOptions, finish)
    {
        let envelope;
        try {
            envelope = jsonStringify({ success: true, result: this.serializeBidiRemoteValue(value, serializationOptions) });
        } catch (error) {
            this._finishBidiScriptWithException(error, finish);
            return;
        }

        finish(envelope);
    }

    _finishBidiScriptWithException(error, finish)
    {
        let envelope;
        try {
            envelope = jsonStringify({ success: false, error: this._bidiExceptionDetails(error) });
        } catch {
            finish(bidiProtocolError("InternalError", "Failed to serialize the script exception"));
            return;
        }

        finish(envelope);
    }

    _bidiExceptionDetails(error)
    {
        let details = { name: "Error", message: "", stack: undefined };
        try {
            if (error !== null && (typeof error === "object" || typeof error === "function")) {
                let name = error.name;
                let message = error.message;
                let stack = error.stack;
                if (typeof name === "string")
                    details.name = name;
                details.message = typeof message === "string" ? message : StringConstructor(error);
                if (typeof stack === "string")
                    details.stack = stack;
            } else
                details.message = StringConstructor(error);
        } catch {
        }
        return details;
    }

    _bidiDeserializationError(error)
    {
        try {
            if (error !== null && typeof error === "object" && objectHasOwn(error, bidiProtocolErrorMarker))
                return error;
        } catch {
        }
        return invalidBidiArgument("LocalValue could not be deserialized");
    }

    _parenthesizedFunctionDeclaration(functionDeclaration)
    {
        // https://w3c.github.io/webdriver-bidi/#evaluate-function-body
        let prefixLength = 0;
        while (reflectApply(stringPrototypeStartsWith, functionDeclaration, ["//", prefixLength])) {
            let lineEnd = reflectApply(stringPrototypeIndexOf, functionDeclaration, ["\n", prefixLength]);
            if (lineEnd === -1)
                break;
            prefixLength = lineEnd + 1;
        }

        let prefix = reflectApply(stringPrototypeSlice, functionDeclaration, [0, prefixLength]);
        let body = reflectApply(stringPrototypeSlice, functionDeclaration, [prefixLength]);
        return prefix + "(" + body + ")";
    }

    _deserializeBidiLocalValue(localValue)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-local-value
        if (localValue === null || typeof localValue !== "object" || arrayIsArray(localValue))
            throw invalidBidiArgument("LocalValue must be an object");

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
                throw invalidBidiArgument("Invalid bigint LocalValue");
            }
        case "date": {
            if (typeof value !== "string" || reflectApply(regExpPrototypeExec, dateTimeStringFormat, [value]) === null)
                break;
            let date = new DateConstructor(value);
            let time = reflectApply(datePrototypeGetTime, date, []);
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
                throw invalidBidiArgument("Invalid regexp LocalValue");
            }
        }
        case "array":
            return this._deserializeBidiList(value);
        case "set": {
            let items = this._deserializeBidiList(value);
            let set = new SetConstructor;
            for (let i = 0; i < items.length; ++i)
                reflectApply(setPrototypeAdd, set, [items[i]]);
            return set;
        }
        case "object": {
            let object = {};
            this._deserializeBidiMapping(value, (key, entryValue) => { createDataProperty(object, key, entryValue); });
            return object;
        }
        case "map": {
            let map = new MapConstructor;
            this._deserializeBidiMapping(value, (key, entryValue) => { reflectApply(mapPrototypeSet, map, [key, entryValue]); });
            return map;
        }
        case "channel":
            if (value === null || typeof value !== "object" || arrayIsArray(value) || !objectHasOwn(value, "channel") || typeof value.channel !== "string")
                break;
            // FIXME: Create channels for ChannelValue arguments. https://bugs.webkit.org/show_bug.cgi?id=288057
            throw bidiProtocolError("NotImplemented", "ChannelValue is not supported");
        }

        throw invalidBidiArgument(typeof type === "string" ? "Invalid LocalValue of type \"" + type + "\"" : "LocalValue has no valid type");
    }

    _deserializeBidiRemoteReference(remoteReference)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-remote-reference
        let hasSharedId = objectHasOwn(remoteReference, "sharedId");
        if (hasSharedId && typeof remoteReference.sharedId !== "string")
            throw invalidBidiArgument("sharedId must be a string");
        if (objectHasOwn(remoteReference, "handle") && typeof remoteReference.handle !== "string")
            throw invalidBidiArgument("handle must be a string");

        if (hasSharedId)
            return this._deserializeBidiSharedReference(remoteReference.sharedId);

        // FIXME: Create handles for resultOwnership "root". https://bugs.webkit.org/show_bug.cgi?id=288059
        throw bidiProtocolError("NotImplemented", "Handle is not supported");
    }

    _deserializeBidiSharedReference(sharedId)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-shared-reference
        let node = reflectApply(mapPrototypeGet, this._idToNodeMap, [sharedId]);
        if (!node || (node !== window.document && node.ownerDocument !== window.document))
            throw bidiProtocolError("NodeNotFound", "Node was not found");
        return node;
    }

    _deserializeBidiList(list)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-value-list
        if (!arrayIsArray(list))
            throw invalidBidiArgument("List LocalValue must be an array");

        let values = [];
        for (let i = 0; i < list.length; ++i)
            createDataProperty(values, i, this._deserializeBidiLocalValue(list[i]));
        return values;
    }

    _deserializeBidiMapping(mapping, addEntry)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-key-value-list
        if (!arrayIsArray(mapping))
            throw invalidBidiArgument("Mapping LocalValue must be an array");

        for (let i = 0; i < mapping.length; ++i) {
            let entry = mapping[i];
            if (!arrayIsArray(entry) || entry.length !== 2)
                throw invalidBidiArgument("Mapping LocalValue entries must have two items");
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
        for (var [node, identifier] of this._nodeToIdMap) {
            const rootNode = node.getRootNode({ composed: true });
            if (rootNode !== document) {
                this._nodeToIdMap.delete(node);
                this._idToNodeMap.delete(identifier);
                this._staleIdentifiers.add(identifier);
            }
        }
    }

    // BiDi Script utilities for W3C WebDriver BiDi specification.
    serializeBidiRemoteValue(value, serializationOptions, serializationInternalMap = new MapConstructor)
    {
        // https://w3c.github.io/webdriver-bidi/#serialize-as-a-remote-value
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
        if (typeof value === "function") return { type: "function" };

        // Handle non-primitive types.
        if (typeof value === "object" || typeof value === "function") {
            // Special handling for window object.
            if (value === window)
                return { type: "window", value: { context: "context-id-placeholder" } };

            // Handle Date, RegExp, Error, Promise.
            if (value instanceof Date)
                return { type: "date", value: value.toISOString() };
            if (value instanceof RegExp)
                return { type: "regexp", value: { pattern: value.source, flags: value.flags } };
            if (value instanceof Error)
                return { type: "error" };
            if (value instanceof Promise)
                return { type: "promise" };

            let knownObject = reflectApply(mapPrototypeHas, serializationInternalMap, [value]);

            // Handle collection types with serialization.
            if (Array.isArray(value))
                return this._serializeBidiArrayLike("array", value, knownObject, serializationOptions, serializationInternalMap);
            if (value instanceof Map) {
                let remoteValue = { type: "map" };
                this._setInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
                if (!knownObject && serializationOptions.maxObjectDepth !== 0)
                    remoteValue.value = this._serializeBidiMapping(value.entries(), serializationOptions, serializationInternalMap);
                return remoteValue;
            }
            if (value instanceof Set) {
                let remoteValue = { type: "set" };
                this._setInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
                if (!knownObject && serializationOptions.maxObjectDepth !== 0)
                    remoteValue.value = this._serializeBidiList(value.values(), serializationOptions, serializationInternalMap);
                return remoteValue;
            }

            // Handle weak collections and ArrayBuffer types.
            if (value instanceof WeakMap)
                return { type: "weakmap" };
            if (value instanceof WeakSet)
                return { type: "weakset" };
            if (value instanceof ArrayBuffer)
                return { type: "arraybuffer" };
            if (ArrayBuffer.isView(value))
                return { type: "typedarray" };

            if (value instanceof NodeList)
                return this._serializeBidiArrayLike("nodelist", value, knownObject, serializationOptions, serializationInternalMap);
            if (value instanceof HTMLCollection)
                return this._serializeBidiArrayLike("htmlcollection", value, knownObject, serializationOptions, serializationInternalMap);
            if (value instanceof Node)
                return this._serializeBidiNode(value, knownObject, serializationOptions, serializationInternalMap);

            let remoteValue = { type: "object" };
            this._setInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
            if (!knownObject && serializationOptions.maxObjectDepth !== 0) {
                let entries = [];
                for (let key of Object.keys(value))
                    entries.push([key, value[key]]);
                remoteValue.value = this._serializeBidiMapping(entries, serializationOptions, serializationInternalMap);
            }
            return remoteValue;
        }

        // Fallback for unknown types.
        return { type: "undefined" };
    }

    _setInternalIdsIfNeeded(serializationInternalMap, remoteValue, value)
    {
        // https://w3c.github.io/webdriver-bidi/#set-internal-ids-if-needed
        if (!reflectApply(mapPrototypeHas, serializationInternalMap, [value])) {
            reflectApply(mapPrototypeSet, serializationInternalMap, [value, remoteValue]);
            return;
        }

        let previouslySerializedRemoteValue = reflectApply(mapPrototypeGet, serializationInternalMap, [value]);
        if (!objectHasOwn(previouslySerializedRemoteValue, "internalId"))
            previouslySerializedRemoteValue.internalId = createUUID();
        remoteValue.internalId = previouslySerializedRemoteValue.internalId;
    }

    _serializeBidiArrayLike(type, value, knownObject, serializationOptions, serializationInternalMap)
    {
        // https://w3c.github.io/webdriver-bidi/#serialize-an-array-like
        let remoteValue = { type };
        this._setInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
        if (!knownObject && serializationOptions.maxObjectDepth !== 0) {
            let items = [];
            for (let i = 0; i < value.length; ++i)
                items.push(value[i]);
            remoteValue.value = this._serializeBidiList(items, serializationOptions, serializationInternalMap);
        }
        return remoteValue;
    }

    _serializeBidiList(iterable, serializationOptions, serializationInternalMap)
    {
        // https://w3c.github.io/webdriver-bidi/#serialize-as-a-list
        let childSerializationOptions = this._decrementedSerializationOptions(serializationOptions, "maxObjectDepth");
        let serialized = [];
        for (let childValue of iterable)
            serialized.push(this.serializeBidiRemoteValue(childValue, childSerializationOptions, serializationInternalMap));
        return serialized;
    }

    _serializeBidiMapping(iterable, serializationOptions, serializationInternalMap)
    {
        // https://w3c.github.io/webdriver-bidi/#serialize-as-a-mapping
        let childSerializationOptions = this._decrementedSerializationOptions(serializationOptions, "maxObjectDepth");
        let serialized = [];
        for (let [key, value] of iterable) {
            let serializedKey = typeof key === "string" ? key : this.serializeBidiRemoteValue(key, childSerializationOptions, serializationInternalMap);
            let serializedValue = this.serializeBidiRemoteValue(value, childSerializationOptions, serializationInternalMap);
            serialized.push([serializedKey, serializedValue]);
        }
        return serialized;
    }

    _serializeBidiNode(node, knownObject, serializationOptions, serializationInternalMap)
    {
        let remoteValue = { type: "node" };

        // https://w3c.github.io/webdriver-bidi/#get-shared-id-for-a-node
        let nodeDocument = node.nodeType === Node.DOCUMENT_NODE ? node : node.ownerDocument;
        if (nodeDocument && nodeDocument.defaultView)
            remoteValue.sharedId = this._identifierForNode(node);

        this._setInternalIdsIfNeeded(serializationInternalMap, remoteValue, node);
        if (knownObject)
            return remoteValue;

        let serialized = { nodeType: node.nodeType };
        let nodeValue = node.nodeValue;
        if (nodeValue !== null)
            serialized.nodeValue = nodeValue;
        if (node instanceof Element || node instanceof Attr) {
            serialized.localName = node.localName;
            serialized.namespaceURI = node.namespaceURI;
        }

        let childNodes = node.childNodes;
        serialized.childNodeCount = childNodes.length;

        let isShadowRoot = node instanceof ShadowRoot;
        let includeShadowTree = serializationOptions.includeShadowTree;
        let omitChildren = serializationOptions.maxDomDepth === 0
            || (isShadowRoot && includeShadowTree === "none")
            || (isShadowRoot && includeShadowTree === "open" && node.mode === "closed");
        if (!omitChildren) {
            let childSerializationOptions = this._decrementedSerializationOptions(serializationOptions, "maxDomDepth");
            let children = [];
            for (let i = 0; i < childNodes.length; ++i)
                children.push(this.serializeBidiRemoteValue(childNodes[i], childSerializationOptions, serializationInternalMap));
            serialized.children = children;
        }

        if (node instanceof Element) {
            let attributes = Object.create(null);
            for (let attribute of node.attributes)
                attributes[attribute.name] = attribute.value;
            serialized.attributes = attributes;

            let shadowRoot = openOrClosedShadowRoot(node);
            serialized.shadowRoot = shadowRoot ? this.serializeBidiRemoteValue(shadowRoot, serializationOptions, serializationInternalMap) : null;
        }

        if (isShadowRoot)
            serialized.mode = node.mode;

        remoteValue.value = serialized;
        return remoteValue;
    }

    _decrementedSerializationOptions(serializationOptions, depthName)
    {
        let childSerializationOptions = { ...serializationOptions };
        if (childSerializationOptions[depthName] !== null)
            --childSerializationOptions[depthName];
        return childSerializationOptions;
    }
};

return new AutomationSessionProxy;

})
