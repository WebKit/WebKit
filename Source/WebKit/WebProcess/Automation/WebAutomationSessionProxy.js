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

(function (sessionIdentifier, currentFrameIdentifier, evaluate, createUUID, isValidNodeIdentifier, isKnownReference, addKnownReference, shadowRootForElement, bidiRemoteValueType, frameIdentifierForWindowProxy, shadowRootMode, accessibilityPropertiesForElement, callFunctionWithUserActivation, isPromise) {

const sessionNodePropertyName = "session-node-" + sessionIdentifier;

// FIXME: These are captured when the session first uses this global object, which can be after page
// script has replaced them.
const convertToString = String;
const dateToISOString = Date.prototype.toISOString;
const jsonStringify = JSON.stringify;
const mapForEach = Map.prototype.forEach;
const promiseThen = Promise.prototype.then;
const reflectApply = Reflect.apply;
const regExpTest = RegExp.prototype.test;
const setForEach = Set.prototype.forEach;

const elementNodeType = 1;
const attributeNodeType = 2;
const documentNodeType = 9;
const documentFragmentNodeType = 11;

const htmlNamespace = "http://www.w3.org/1999/xhtml";

// XPathResult.ORDERED_NODE_SNAPSHOT_TYPE.
const xpathOrderedNodeSnapshotType = 7;

// Errors that browsingContext.locateNodes reports, named after the ErrorMessage values in Automation.json.
const bidiLocateNodesErrorNames = new Set(["InvalidParameter", "InvalidSelector", "NodeNotFound", "InternalError"]);

// The UI process can't parse JSON that is nested more than 1000 levels deep. This leaves room for
// the message around a remote value, and for the levels inside the innermost remote value.
const maxBidiRemoteValueJSONDepth = 900;

// https://tc39.es/ecma262/#sec-date-time-string-format, including expanded years.
// Date parsing checks the field ranges.
const dateTimeStringFormat = (() => {
    const year = String.raw`(?!-000000)(?:[+-]\d{6}|\d{4})`;
    const date = String.raw`${year}(?:-\d{2}(?:-\d{2})?)?`;
    const offset = String.raw`(?:Z|[+-]\d{2}:\d{2})`;
    const time = String.raw`T\d{2}:\d{2}(?::\d{2}(?:\.\d{3})?)?${offset}?`;
    return new RegExp(`^${date}(?:${time})?$`);
})();

// How deep in JSON the remote value being built is. Page script could intercept this if it were a property
// of the proxy object.
let bidiRemoteValueJSONDepth = 0;

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

class InvalidArgumentError extends Error
{
    constructor(message)
    {
        super(message);
        this.name = "InvalidParameter";
    }
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
            .catch(error => {
                // A script can throw any value, but the callback treats a string as a successful result, and reads an error's name and message from an object.
                // The object has its own name, so that page script can't supply one through Object.prototype.
                if (error === null || (typeof error !== "object" && typeof error !== "function"))
                    error = { name: "Error", message: convertToString(error) };
                resultCallback(frameID, callbackID, error);
            });
    }

    evaluateBidiScript(expression, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, frameID, callbackID, resultCallback, callbackTimeout)
    {
        this._executeBidiScript(expression, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, callbackTimeout).then(result => {
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

    // https://w3c.github.io/webdriver-bidi/#command-browsingContext-locateNodes
    // The UI process validates the parameters. Returns a JSON string, either { nodes } with each node serialized
    // as a remote value, or { error, message } where error names an ErrorMessage value.
    locateNodesForBidi(locatorString, startNodeIdentifiers, maxNodeCount, maxObjectDepth, maxDomDepth, includeShadowTree, containerElement)
    {
        try {
            // Check the type of each start node before clearing stale nodes, since a node that isn't in the document, such as an
            // attribute, is invalid as a start node rather than unknown.
            if (startNodeIdentifiers)
                startNodeIdentifiers.forEach(identifier => this._validateBidiLocateNodesStartNodeType(identifier));

            this._clearStaleNodes();

            let locator = JSON.parse(locatorString);
            let contextNodes = startNodeIdentifiers ? startNodeIdentifiers.map(identifier => this._bidiLocateNodesStartNode(identifier)) : [document];
            let nodes;
            if (locator.type === "context") {
                // The UI process checked that the context is a child of this one, and the web process found its container element.
                if (startNodeIdentifiers)
                    throw { name: "InvalidParameter", message: "The context locator can't be used with startNodes." };
                nodes = containerElement ? [containerElement] : [];
            } else
                nodes = this._bidiLocateNodes(locator, contextNodes, maxNodeCount);

            // Each node gets its own serialization internal map.
            let serializedNodes = nodes.map(node => this.serializeBidiRemoteValue(node, maxObjectDepth, maxDomDepth, includeShadowTree, new Map));
            return jsonStringify({ nodes: serializedNodes });
        } catch (error) {
            let name = safelyReadStringProperty(error, "name");
            if (!bidiLocateNodesErrorNames.has(name))
                name = "InternalError";
            return jsonStringify({ error: name, message: safelyReadStringProperty(error, "message") || "" });
        }
    }

    evaluateBidiCallFunction(functionDeclaration, argumentStrings, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, userActivation, frameID, callbackID, resultCallback, callbackTimeout)
    {
        // Chain with the built-in then, since page script can replace Promise.prototype.then.
        reflectApply(promiseThen, this._executeBidiCallFunction(functionDeclaration, argumentStrings, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, userActivation, callbackTimeout), [result => {
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
            if (name !== "JavaScriptTimeout" && name !== "InvalidParameter")
                name = "InternalError";
            let fallbackMessage = name === "JavaScriptTimeout" ? "Script evaluation timed out." : "Script call failed.";
            resultCallback(frameID, callbackID, createBidiScriptError(error, name, fallbackMessage));
        }]);
    }

    nodeForIdentifier(identifier)
    {
        this._clearStaleNodes();
        return this._nodeForIdentifier(identifier);
    }

    nodeForSharedId(identifier)
    {
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

    _executeBidiScript(expression, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, callbackTimeout)
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
                resolve(this._bidiExceptionResult(error));
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
                // Evaluate the expression in the target global scope.
                result = globalThis.eval(expression);
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

    // Builds the exception result of script.evaluate and script.callFunction.
    _bidiExceptionResult(error)
    {
        let serializedException;
        try {
            serializedException = this.serializeBidiRemoteValue(error);
        } catch {
            // Report the exception without its contents instead of failing the command. Only an
            // object can fail to serialize.
            serializedException = { type: bidiRemoteValueType(error) ?? "object" };
        }

        let errorProperties = {
            exception: serializedException,
            name: safelyReadStringProperty(error, "name"),
            message: safelyReadStringProperty(error, "message"),
            stack: safelyReadStringProperty(error, "stack")
        };
        return { success: false, error: errorProperties };
    }

    _executeBidiCallFunction(functionDeclaration, argumentStrings, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, userActivation, callbackTimeout)
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

        let executionPromise;
        let serializeResult = value => {
            try {
                return { success: true, result: this.serializeBidiRemoteValue(value, maxObjectDepth, maxDomDepth, includeShadowTree) };
            } catch (error) {
                throw createBidiScriptError(error, "InternalError", "Script result serialization failed.");
            }
        };

        try {
            // https://w3c.github.io/webdriver-bidi/#command-script-callFunction
            // The arguments are deserialized before the function declaration is evaluated, so an invalid argument
            // fails the command before the declaration has any side effects.
            this._clearStaleNodes();
            // FIXME: <https://webkit.org/b/328581> Prevent page-defined globals and array iterators from affecting
            // argument deserialization and the call.
            let argumentValues = argumentStrings.map(argument => this._deserializeBidiLocalValue(JSON.parse(argument)));

            let lines = functionDeclaration.split("\n");
            let prefixLines = [];
            while (lines.length && lines[0].startsWith("//"))
                prefixLines.push(lines.shift());

            let prefix = prefixLines.join("\n");
            if (prefix)
                prefix += "\n";

            let functionValue = evaluate(prefix + "(" + lines.join("\n") + "\n)");
            if (typeof functionValue !== "function")
                throw new InvalidArgumentError("Function declaration did not evaluate to a callable value.");

            let result = callFunctionWithUserActivation(userActivation, functionValue, null, ...argumentValues);

            if (awaitPromise && isPromise(result))
                // Await with the built-in then, which ECMAScript Await uses, not one that page script installed.
                executionPromise = reflectApply(promiseThen, result, [serializeResult, error => this._bidiExceptionResult(error)]);
            else {
                try {
                    executionPromise = Promise.resolve(serializeResult(result));
                } catch (error) {
                    executionPromise = Promise.reject(error);
                }
            }
        } catch (error) {
            if (error instanceof InvalidArgumentError)
                executionPromise = Promise.reject(error);
            else
                executionPromise = Promise.resolve(this._bidiExceptionResult(error));
        }

        // Settle with whichever of the call and the timeout settles first. Promise.race() and finally() would
        // look up then on Promise.prototype, which page script can replace.
        return new Promise((resolve, reject) => {
            let settleWith = settle => value => {
                if (timeoutIdentifier)
                    clearTimeout(timeoutIdentifier);
                settle(value);
            };
            reflectApply(promiseThen, executionPromise, [settleWith(resolve), settleWith(reject)]);
            if (timeoutPromise)
                reflectApply(promiseThen, timeoutPromise, [settleWith(resolve), settleWith(reject)]);
        });
    }

    _deserializeBidiLocalValue(localValue)
    {
        if (!localValue || typeof localValue !== "object")
            throw new InvalidArgumentError("LocalValue must be an object.");

        switch (localValue.type) {
        case "undefined":
            return undefined;
        case "null":
            return null;
        case "string":
        case "boolean":
            return localValue.value;
        case "number":
            if (typeof localValue.value === "number")
                return localValue.value;
            switch (localValue.value) {
            case "NaN":
                return NaN;
            case "-0":
                return -0;
            case "Infinity":
                return Infinity;
            case "-Infinity":
                return -Infinity;
            default:
                throw new InvalidArgumentError("Invalid number LocalValue.");
            }
        case "bigint":
            try {
                return BigInt(localValue.value);
            } catch {
                throw new InvalidArgumentError("Invalid bigint LocalValue.");
            }
        case "array":
            return localValue.value.map(value => this._deserializeBidiLocalValue(value));
        case "date": {
            // FIXME: <https://webkit.org/b/328581> RegExp.prototype.test calls the regular expression's exec, which
            // page script can replace to make any string pass this check.
            if (typeof localValue.value !== "string" || !reflectApply(regExpTest, dateTimeStringFormat, [localValue.value]))
                throw new InvalidArgumentError("Invalid date LocalValue.");
            let value = new Date(localValue.value);
            if (Number.isNaN(value.valueOf()))
                throw new InvalidArgumentError("Invalid date LocalValue.");
            return value;
        }
        case "map":
            return new Map(this._deserializeBidiLocalValueEntries(localValue.value));
        case "object":
            return Object.fromEntries(this._deserializeBidiLocalValueEntries(localValue.value));
        case "regexp":
            try {
                return new RegExp(localValue.value.pattern, localValue.value.flags);
            } catch {
                throw new InvalidArgumentError("Invalid regexp LocalValue.");
            }
        case "set":
            return new Set(localValue.value.map(value => this._deserializeBidiLocalValue(value)));
        default:
            throw new InvalidArgumentError("Unsupported LocalValue type.");
        }
    }

    _deserializeBidiLocalValueEntries(entries)
    {
        return entries.map(entry => {
            let key = typeof entry[0] === "string" ? entry[0] : this._deserializeBidiLocalValue(entry[0]);
            return [key, this._deserializeBidiLocalValue(entry[1])];
        });
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

    _validateBidiLocateNodesStartNodeType(identifier)
    {
        let node = this._idToNodeMap.get(identifier);
        if (!node)
            return;

        let nodeType = node.nodeType;
        if (nodeType !== elementNodeType && nodeType !== documentNodeType && nodeType !== documentFragmentNodeType)
            throw { name: "InvalidParameter", message: "startNodes must be elements, documents or document fragments." };
    }

    _bidiLocateNodesStartNode(identifier)
    {
        // https://w3c.github.io/webdriver-bidi/#deserialize-shared-reference
        try {
            return this._nodeForIdentifier(identifier);
        } catch {
            throw { name: "NodeNotFound", message: `No node with sharedId '${identifier}' was found in this browsing context.` };
        }
    }

    _bidiLocateNodes(locator, contextNodes, maxNodeCount)
    {
        switch (locator.type) {
        case "css":
            return this._bidiLocateNodesUsingCSS(contextNodes, locator.value, maxNodeCount);
        case "xpath":
            return this._bidiLocateNodesUsingXPath(contextNodes, locator.value, maxNodeCount);
        case "innerText": {
            if (!locator.value)
                throw { name: "InvalidSelector", message: "The innerText locator value must not be empty." };
            let ignoreCase = locator.ignoreCase ?? false;
            let searchText = ignoreCase ? locator.value.toUpperCase() : locator.value;
            return this._bidiLocateNodesUsingInnerText(contextNodes, searchText, locator.maxDepth ?? null, locator.matchType ?? "full", ignoreCase, maxNodeCount);
        }
        case "accessibility": {
            let returnedNodes = [];
            let state = { selector: locator.value, maxNodeCount, returnedNodes, needsCacheUpdate: true };
            this._bidiCollectNodesUsingAccessibilityAttributes(contextNodes, state);
            return returnedNodes;
        }
        }

        throw { name: "InvalidParameter", message: `Unknown locator type '${locator.type}'.` };
    }

    _bidiLocateNodesUsingCSS(contextNodes, selector, maxNodeCount)
    {
        // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-css
        let returnedNodes = [];
        for (let contextNode of contextNodes) {
            let elements;
            try {
                elements = contextNode.querySelectorAll(selector);
            } catch (error) {
                throw { name: "InvalidSelector", message: `Invalid CSS selector: ${safelyReadStringProperty(error, "message")}` };
            }

            for (let element of elements) {
                returnedNodes.push(element);
                if (maxNodeCount !== null && returnedNodes.length === maxNodeCount)
                    return returnedNodes;
            }
        }
        return returnedNodes;
    }

    _bidiLocateNodesUsingXPath(contextNodes, selector, maxNodeCount)
    {
        // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-xpath
        let returnedNodes = [];
        for (let contextNode of contextNodes) {
            let evaluateResult;
            try {
                evaluateResult = document.evaluate(selector, contextNode, null, xpathOrderedNodeSnapshotType, null);
            } catch (error) {
                let message = safelyReadStringProperty(error, "message");
                if (safelyReadStringProperty(error, "name") === "SyntaxError")
                    throw { name: "InvalidSelector", message: `Invalid XPath expression: ${message}` };
                throw { name: "InternalError", message: `Evaluating the XPath expression failed: ${message}` };
            }

            let length = evaluateResult.snapshotLength;
            for (let index = 0; index < length; ++index) {
                returnedNodes.push(evaluateResult.snapshotItem(index));
                if (maxNodeCount !== null && returnedNodes.length === maxNodeCount)
                    return returnedNodes;
            }
        }
        return returnedNodes;
    }

    _bidiLocateNodesUsingInnerText(contextNodes, searchText, maxDepth, matchType, ignoreCase, maxNodeCount)
    {
        // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-inner-text
        // "Children" here are element children, as in other implementations and the WPT expectations.
        let returnedNodes = [];
        for (let contextNode of contextNodes) {
            let nodeType = contextNode.nodeType;
            if (nodeType === documentNodeType || nodeType === documentFragmentNodeType) {
                // Max depth isn't decreased, so searching a document matches searching its document element.
                returnedNodes.push(...this._bidiLocateNodesUsingInnerText([...contextNode.children], searchText, maxDepth, matchType, ignoreCase, maxNodeCount));
                continue;
            }

            if (nodeType !== elementNodeType)
                continue;

            // Elements that aren't HTML elements, such as SVG text, have no innerText. Match their text content instead.
            let nodeInnerText = contextNode.namespaceURI === htmlNamespace ? contextNode.innerText : contextNode.textContent;
            let nodeText = ignoreCase ? nodeInnerText.toUpperCase() : nodeInnerText;
            if (!nodeText.includes(searchText))
                continue;

            let childNodes = [...contextNode.children];
            if (!childNodes.length || maxDepth === 0) {
                if (matchType === "partial" || nodeText === searchText)
                    returnedNodes.push(contextNode);
                continue;
            }

            let childMaxDepth = maxDepth === null ? null : maxDepth - 1;
            let childNodeMatches = this._bidiLocateNodesUsingInnerText(childNodes, searchText, childMaxDepth, matchType, ignoreCase, maxNodeCount);
            if (!childNodeMatches.length && matchType === "partial")
                returnedNodes.push(contextNode);
            else
                returnedNodes.push(...childNodeMatches);
        }

        if (maxNodeCount !== null && returnedNodes.length > maxNodeCount)
            returnedNodes.length = maxNodeCount;
        return returnedNodes;
    }

    // Returns true once the maximum returned node count is reached, so the whole walk stops.
    _bidiCollectNodesUsingAccessibilityAttributes(contextNodes, state)
    {
        // https://w3c.github.io/webdriver-bidi/#collect-nodes-using-accessibility-attributes
        let { selector, maxNodeCount, returnedNodes } = state;
        let includeRole = selector.role !== undefined;
        let includeName = selector.name !== undefined;
        for (let contextNode of contextNodes) {
            let match = contextNode.nodeType === elementNodeType;
            if (match) {
                let properties = accessibilityPropertiesForElement(contextNode, includeRole, includeName, state.needsCacheUpdate);
                state.needsCacheUpdate = false;
                if (!properties)
                    match = false;
                else {
                    // Indexing, unlike destructuring, doesn't go through the array iterator, which page script can replace.
                    if (includeRole && properties[0] !== selector.role)
                        match = false;
                    if (includeName && properties[1] !== selector.name)
                        match = false;
                }
            }

            if (match) {
                returnedNodes.push(contextNode);
                if (maxNodeCount !== null && returnedNodes.length === maxNodeCount)
                    return true;
            }

            if (this._bidiCollectNodesUsingAccessibilityAttributes([...contextNode.children], state))
                return true;
        }
        return false;
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
        // Handle primitives, without isNaN(), Object.is() or toString(), which page script can replace.
        if (value === null) return { type: "null" };
        if (value === undefined) return { type: "undefined" };
        if (typeof value === "boolean") return { type: "boolean", value: value };
        if (typeof value === "string") return { type: "string", value: value };
        if (typeof value === "number") {
            if (value !== value) return { type: "number", value: "NaN" };
            if (value === Infinity) return { type: "number", value: "Infinity" };
            if (value === -Infinity) return { type: "number", value: "-Infinity" };
            if (value === 0 && 1 / value < 0) return { type: "number", value: "-0" };
            return { type: "number", value: value };
        }
        if (typeof value === "bigint") return { type: "bigint", value: `${value}` };
        if (typeof value === "symbol") return { type: "symbol" };

        // Everything else is an object, including document.all.
        let type = bidiRemoteValueType(value);
        switch (type) {
        case null:
            return this._serializeBidiMapping(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        case "array":
        case "set":
        case "nodelist":
        case "htmlcollection":
            return this._serializeBidiList(value, type, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        case "map":
            return this._serializeBidiMap(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        case "node":
            return this._serializeBidiNode(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        case "date":
            return { type, value: reflectApply(dateToISOString, value, []) };
        case "regexp":
            // The spec converts these with ToString, so page script that overrides them can't make the pattern or flags an object.
            return { type, value: { pattern: `${value.source}`, flags: `${value.flags}` } };
        case "window": {
            // The UI process replaces the frame identifier with the browsing context id.
            // FIXME: Page script can read the frame identifier on its way out, which reveals a counter shared by
            // all pages, and can replace it with another frame's.
            let frameIdentifier = frameIdentifierForWindowProxy(value);
            if (frameIdentifier === null)
                return { type };
            return { type, value: { context: frameIdentifier } };
        }
        default:
            // Platform objects and the types that are serialized without a value.
            return { type };
        }
    }

    // Serializes a value that the remote value being built contains, |jsonLevels| levels of JSON further in:
    // 2 for a list item ({value: [item]}) or a shadow root ({value: {shadowRoot}}), and 3 for a mapping
    // entry ({value: [[key, value]]}) or a child node ({value: {children: [node]}}).
    _serializeNestedBidiRemoteValue(jsonLevels, value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap)
    {
        if (bidiRemoteValueJSONDepth + jsonLevels > maxBidiRemoteValueJSONDepth)
            throw { message: "Value is nested too deeply to serialize." };

        bidiRemoteValueJSONDepth += jsonLevels;
        try {
            return this.serializeBidiRemoteValue(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
        } finally {
            bidiRemoteValueJSONDepth -= jsonLevels;
        }
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
                remoteValue.value.push(this._serializeNestedBidiRemoteValue(2, childValue, childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap));
            }]);
            return remoteValue;
        }

        for (let i = 0; i < value.length; ++i)
            remoteValue.value.push(this._serializeNestedBidiRemoteValue(2, value[i], childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap));
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
            let serializedKey = typeof key === "string" ? key : this._serializeNestedBidiRemoteValue(3, key, childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
            let serializedValue = this._serializeNestedBidiRemoteValue(3, childValue, childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
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
            let serializedValue = this._serializeNestedBidiRemoteValue(3, value[key], childObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap);
            remoteValue.value.push([key, serializedValue]);
        }
        return remoteValue;
    }

    _serializeBidiNode(value, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap)
    {
        let remoteValue = { type: "node" };
        // FIXME: Read node properties natively. Page script can override them, for example with an own nodeType
        // property, and make serialization fail.
        let nodeType = value.nodeType;
        let nodeDocument = nodeType === documentNodeType ? value : value.ownerDocument;
        if (nodeDocument === document)
            remoteValue.sharedId = this._identifierForNode(value);

        let knownObject = serializationInternalMap.has(value);
        this._setBidiInternalIdsIfNeeded(serializationInternalMap, remoteValue, value);
        if (knownObject)
            return remoteValue;

        let serialized = {
            nodeType,
            childNodeCount: value.childNodes.length
        };

        if (value.nodeValue !== null)
            serialized.nodeValue = value.nodeValue;

        if (nodeType === elementNodeType || nodeType === attributeNodeType) {
            serialized.localName = value.localName;
            serialized.namespaceURI = value.namespaceURI;
        }

        let mode = shadowRootMode(value);
        let shouldSerializeChildren = maxDomDepth !== 0;
        if (mode) {
            if (includeShadowTree === "none" || (includeShadowTree === "open" && mode === "closed"))
                shouldSerializeChildren = false;
        }

        if (shouldSerializeChildren) {
            let childDomDepth = maxDomDepth === null ? null : maxDomDepth - 1;
            serialized.children = [];
            let childNodes = value.childNodes;
            for (let i = 0; i < childNodes.length; ++i)
                serialized.children.push(this._serializeNestedBidiRemoteValue(3, childNodes[i], maxObjectDepth, childDomDepth, includeShadowTree, serializationInternalMap));
        }

        if (nodeType === elementNodeType) {
            serialized.attributes = {};
            for (let i = 0; i < value.attributes.length; ++i) {
                let attribute = value.attributes[i];
                serialized.attributes[attribute.name] = attribute.value;
            }

            // FIXME: <https://webkit.org/b/328583> Prevent script.evaluate result serialization from exposing closed shadow roots to page code.
            let shadowRoot = shadowRootForElement(value);
            serialized.shadowRoot = shadowRoot ? this._serializeNestedBidiRemoteValue(2, shadowRoot, maxObjectDepth, maxDomDepth, includeShadowTree, serializationInternalMap) : null;
        }

        if (mode)
            serialized.mode = mode;

        remoteValue.value = serialized;
        return remoteValue;
    }
};

return new AutomationSessionProxy;

})
