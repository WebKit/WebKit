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

(function (sessionIdentifier, evaluate, createUUID, isValidNodeIdentifier) {

const sessionNodePropertyName = "session-node-" + sessionIdentifier;

let AutomationSessionProxy = class AutomationSessionProxy
{
    constructor()
    {
        this._nodeToIdMap = new Map;
        this._idToNodeMap = new Map;
    }

    // Public

    evaluateJavaScriptFunction(functionString, argumentStrings, expectsImplicitCallbackArgument, frameID, callbackID, resultCallback, callbackTimeout)
    {
        this._execute(functionString, argumentStrings, expectsImplicitCallbackArgument, callbackTimeout)
            .then(result => { resultCallback(frameID, callbackID, this._jsonStringify(result)); })
            .catch(error => { resultCallback(frameID, callbackID, error); });
    }

    nodeForIdentifier(identifier)
    {
        this._clearStaleNodes();
        try {
            return this._nodeForIdentifier(identifier);
        } catch (error) {
            return null;
        }
    }

    // Private

    _execute(functionString, argumentStrings, expectsImplicitCallbackArgument, callbackTimeout)
    {
        let timeoutPromise;
        let timeoutIdentifier = 0;
        if (callbackTimeout >= 0) {
            timeoutPromise = new Promise((resolve, reject) => {
                timeoutIdentifier = setTimeout(() => {
                    reject({ name: "JavaScriptTimeout", message: "script timed out after " + callbackTimeout + "ms" });
                }, callbackTimeout);
            });
        }

        let promise = new Promise((resolve, reject) => {
            // The script is expected to be a function declaration. Evaluate it inside parenthesis to get the function value.
            let functionValue = evaluate("(async " + functionString + ")");
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
        if (node.ownerDocument !== window.document || !node.isConnected)
            throw {name: "NodeNotFound", message: "Stale element found when trying to create the node handle"};

        return {[sessionNodePropertyName]: this._identifierForNode(node)};
    }

    _nodeForIdentifier(identifier)
    {
        if (!isValidNodeIdentifier(identifier))
            throw {name: "InvalidNodeIdentifier", message: "Node identifier '" + identifier + "' is invalid"};

        let node = this._idToNodeMap.get(identifier);
        if (node)
            return node;
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

        return identifier;
    }

    _clearStaleNodes()
    {
        for (var [node, identifier] of this._nodeToIdMap) {
            if (!document.contains(node)) {
                this._nodeToIdMap.delete(node);
                this._idToNodeMap.delete(identifier);
            }
        }
    }
};

return new AutomationSessionProxy;

})
