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

(function (sessionIdentifier, evaluate, createUUID) {

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
        // The script is expected to be a function declaration. Evaluate it inside parenthesis to get the function value.
        let functionValue = evaluate("(" + functionString + ")");
        if (typeof functionValue !== "function")
            throw new TypeError("Script did not evaluate to a function.");

        this._clearStaleNodes();

        let argumentValues = argumentStrings.map(this._jsonParse, this);

        let timeoutIdentifier = 0;
        let resultReported = false;

        let reportResult = (result) => {
            if (timeoutIdentifier)
                clearTimeout(timeoutIdentifier);
            resultCallback(frameID, callbackID, this._jsonStringify(result), false);
            resultReported = true;
        };
        let reportTimeoutError = () => { resultCallback(frameID, callbackID, "JavaScriptTimeout", true); };

        if (expectsImplicitCallbackArgument) {
            argumentValues.push(reportResult);
            functionValue.apply(null, argumentValues);
            if (!resultReported)
                timeoutIdentifier = setTimeout(reportTimeoutError, callbackTimeout);
        } else
            reportResult(functionValue.apply(null, argumentValues));
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

    _jsonParse(string)
    {
        if (!string)
            return undefined;
        return JSON.parse(string, (key, value) => this._reviveJSONValue(key, value));
    }

    _jsonStringify(original)
    {
        return JSON.stringify(original, (key, value) => this._replaceJSONValue(key, value)) || "null";
    }

    _reviveJSONValue(key, value)
    {
        if (value && typeof value === "object" && value[sessionNodePropertyName])
            return this._nodeForIdentifier(value[sessionNodePropertyName]);
        return value;
    }

    _replaceJSONValue(key, value)
    {
        if (value instanceof Node)
            return this._createNodeHandle(value);

        if (value instanceof NodeList || value instanceof HTMLCollection)
            value = Array.from(value).map(this._createNodeHandle, this);

        return value;
    }

    _createNodeHandle(node)
    {
        return {[sessionNodePropertyName]: this._identifierForNode(node)};
    }

    _nodeForIdentifier(identifier)
    {
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
