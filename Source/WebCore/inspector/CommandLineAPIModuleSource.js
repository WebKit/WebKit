/*
 * Copyright (C) 2007-2022 Apple Inc.  All rights reserved.
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

// @internal

function injectModule(InjectedScriptHost, inspectedGlobalObject, injectedScriptId, injectedScript, {RemoteObject, CommandLineAPI}, CommandLineAPIHost)
{

// FIXME: <webkit.org/b/239774> Injected script should use WebCore built-ins.

injectedScript._inspectObject = function(object) {
    if (arguments.length === 0)
        return;

    let objectId = RemoteObject.create(object);
    let hints = {};

    switch (RemoteObject.describe(object)) {
    case "Database":
        var databaseId = CommandLineAPIHost.databaseId(object);
        if (databaseId)
            hints.databaseId = databaseId;
        break;
    case "Storage":
        var storageId = CommandLineAPIHost.storageId(object);
        if (storageId)
            hints.domStorageId = InjectedScriptHost.evaluate("(" + storageId + ")");
        break;
    }

    CommandLineAPIHost.inspect(objectId, hints);
};

CommandLineAPI.getters["0"] = function() {
    return CommandLineAPIHost.inspectedObject();
};

CommandLineAPI.methods["copy"] = function(object) {
    let string = null;

    let subtype = RemoteObject.subtype(object);
    if (subtype === "node")
        string = object.outerHTML;
    else if (subtype === "regexp")
        string = "" + object;
    else if (injectedScript.isPrimitiveValue(object))
        string = "" + object;
    else if (typeof object === "symbol")
        string = @String(object);
    else if (typeof object === "function")
        string = "" + object;
    else {
        try {
            string = @jsonStringify(object, "  ");
        } catch {
            string = "" + object;
        }
    }

    CommandLineAPIHost.copyText(string);
};

CommandLineAPI.methods["getEventListeners"] = function(target) {
    return CommandLineAPIHost.getEventListeners(target);
};

function normalizeEventTypes(types) {
    if (types === @undefined)
        types = @createArrayWithoutPrototype("mouse", "key", "touch", "control", "abort", "blur", "change", "devicemotion", "deviceorientation", "error", "focus", "load", "reset", "resize", "scroll", "search", "select", "submit", "unload");
    else if (typeof types === "string")
        types = @createArrayWithoutPrototype(types);

    let result = new @Set;
    function addTypesToResult() {
        for (let i = 0; i < arguments.length; ++i)
            result.@add(arguments[i]);
    }

    for (let i = 0; i < types.length; i++) {
        if (types[i] === "mouse")
            addTypesToResult("click", "dblclick", "mousedown", "mousemove", "mouseout", "mouseover", "mouseup", "mousewheel");
        else if (types[i] === "key")
            addTypesToResult("keydown", "keypress", "keyup", "textInput");
        else if (types[i] === "touch")
            addTypesToResult("touchcancel", "touchend", "touchmove", "touchstart");
        else if (types[i] === "control")
            addTypesToResult("blur", "change", "focus", "reset", "resize", "scroll", "select", "submit", "zoom");
        else
            result.@add(types[i]);
    }
    return result;
}

function logEvent(event)
{
    inspectedGlobalObject.console.log(event.type, event);
}

CommandLineAPI.methods["monitorEvents"] = function(object, types) {
    if (!object || !object.addEventListener || !object.removeEventListener)
        return;
    for (let type of @builtinSetIterable(normalizeEventTypes(types))) {
        object.removeEventListener(type, logEvent, false);
        object.addEventListener(type, logEvent, false);
    }
};

CommandLineAPI.methods["unmonitorEvents"] = function(object, types) {
    if (!object || !object.addEventListener || !object.removeEventListener)
        return;
    for (let type of @builtinSetIterable(normalizeEventTypes(types)))
        object.removeEventListener(type, logEvent, false);
};

if (inspectedGlobalObject.document && inspectedGlobalObject.Node) {
    function canQuerySelectorOnNode(node) {
        return node && InjectedScriptHost.subtype(node) === "node" && (node.nodeType === inspectedGlobalObject.Node.ELEMENT_NODE || node.nodeType === inspectedGlobalObject.Node.DOCUMENT_NODE || node.nodeType === inspectedGlobalObject.Node.DOCUMENT_FRAGMENT_NODE);
    }

    CommandLineAPI.methods["$"] = function(selector, start) {
        if (canQuerySelectorOnNode(start))
            return start.querySelector(selector);

        let result = inspectedGlobalObject.document.querySelector(selector);
        if (result)
            return result;

        if (selector && selector[0] !== "#") {
            result = inspectedGlobalObject.document.getElementById(selector);
            if (result) {
                inspectedGlobalObject.console.warn("The console function $() has changed from $=getElementById(id) to $=querySelector(selector). You might try $(\"#%s\")", selector);
                return null;
            }
        }

        return result;
    };

    CommandLineAPI.methods["$$"] = function(selector, start) {
        // Don't return a prototypeless array here so that developers can use the results as a normal Array.
        if (canQuerySelectorOnNode(start))
            return @Array.@from(start.querySelectorAll(selector));
        return @Array.@from(inspectedGlobalObject.document.querySelectorAll(selector));
    };

    CommandLineAPI.methods["$x"] = function(xpath, context) {
        let doc = (context && context.ownerDocument) || inspectedGlobalObject.document;
        let result = doc.evaluate(xpath, context || doc, null, inspectedGlobalObject.XPathResult.ANY_TYPE, null);
        switch (result.resultType) {
        case inspectedGlobalObject.XPathResult.NUMBER_TYPE:
            return result.numberValue;
        case inspectedGlobalObject.XPathResult.STRING_TYPE:
            return result.stringValue;
        case inspectedGlobalObject.XPathResult.BOOLEAN_TYPE:
            return result.booleanValue;
        }

        // Don't return a prototypeless array here so that developers can use the results as a normal Array.
        let nodes = new @Array;
        let node = null;
        while (node = result.iterateNext())
            @arrayPush(nodes, node);
        return nodes;
    };
}

for (let name in CommandLineAPI.methods)
    CommandLineAPI.methods[name].toString = function() { return "function " + name + "() { [Command Line API] }"; };

}
