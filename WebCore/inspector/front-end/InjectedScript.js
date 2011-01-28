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

var injectedScriptConstructor = (function (InjectedScriptHost, inspectedWindow, injectedScriptId, jsEngine) {

var InjectedScript = {};

InjectedScript.lastBoundObjectId = 1;
InjectedScript.idToWrappedObject = {};
InjectedScript.objectGroups = {};

InjectedScript.wrapObjectForConsole = function(object, canAccessInspectedWindow)
{
    if (canAccessInspectedWindow)
        return InjectedScript.wrapObject(object, "console");
    var result = {};
    result.type = typeof object;
    result.description = InjectedScript._toString(object);
    return result;
}

InjectedScript.wrapObject = function(object, objectGroupName)
{
    try {
        var objectId;
        if (typeof object === "object" || typeof object === "function" || InjectedScript._isHTMLAllCollection(object)) {
            var id = InjectedScript.lastBoundObjectId++;
            objectId = "object#" + id;
            InjectedScript.idToWrappedObject[objectId] = object;

            var group = InjectedScript.objectGroups[objectGroupName];
            if (!group) {
                group = [];
                InjectedScript.objectGroups[objectGroupName] = group;
            }
            group.push(objectId);
        }
        return InjectedScript.createProxyObject(object, objectId);
    } catch (e) {
        return InjectedScript.createProxyObject("[ Exception: " + e.toString() + " ]");
    }
    return InjectedScript.createProxyObject(object, objectId);
};

InjectedScript.unwrapObject = function(objectId) {
    return InjectedScript.idToWrappedObject[objectId];
};

InjectedScript.releaseWrapperObjectGroup = function(objectGroupName) {
    var group = InjectedScript.objectGroups[objectGroupName];
    if (!group)
        return;
    for (var i = 0; i < group.length; i++)
        delete InjectedScript.idToWrappedObject[group[i]];
    delete InjectedScript.objectGroups[objectGroupName];
};

// Called from within InspectorController on the 'inspected page' side.
InjectedScript.reset = function()
{
    InjectedScript._searchResults = [];
    InjectedScript._includedInSearchResultsPropertyName = "__includedInInspectorSearchResults";
    InjectedScript._inspectedNodes = [];
}

InjectedScript.reset();

InjectedScript.dispatch = function(methodName, args, callId)
{
    var argsArray = eval("(" + args + ")");
    if (callId)
        argsArray.splice(0, 0, callId);  // Methods that run asynchronously have a call back id parameter.
    var result = InjectedScript[methodName].apply(InjectedScript, argsArray);
    if (typeof result === "undefined") {
        inspectedWindow.console.error("Web Inspector error: InjectedScript.%s returns undefined", methodName);
        result = null;
    }
    return result;
}

InjectedScript.getPrototypes = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;

    var result = [];
    for (var prototype = node; prototype; prototype = prototype.__proto__) {
        var title = InjectedScript._describe(prototype, true);
        if (title.match(/Prototype$/)) {
            title = title.replace(/Prototype$/, "");
        }
        result.push(title);
    }
    return result;
}

InjectedScript.getProperties = function(objectProxy, ignoreHasOwnProperty, abbreviate)
{
    var object = InjectedScript._resolveObject(objectProxy);
    if (!InjectedScript._isDefined(object))
        return false;
    var properties = [];
    
    var propertyNames = ignoreHasOwnProperty ? InjectedScript._getPropertyNames(object) : Object.getOwnPropertyNames(object);
    if (!ignoreHasOwnProperty && object.__proto__)
        propertyNames.push("__proto__");

    if (jsEngine === "v8") {
        // Check if the object is a scope.
        if (InjectedScript._isScopeProxy(objectProxy)) {
            propertyNames = [];
            for (var name in object)
                propertyNames.push(name);
        } else {
            // FIXME(http://crbug.com/41243): Object.getOwnPropertyNames may return duplicated names.
            var a = [];
            propertyNames.sort();
            var prev;
            for (var i = 0; i < propertyNames.length; i++) {
                var n = propertyNames[i];
                if (n != prev)
                    a.push(n);
                prev = n;
            }
            propertyNames = a;
        }
    }
    
    // Go over properties, prepare results.
    for (var i = 0; i < propertyNames.length; ++i) {
        var propertyName = propertyNames[i];

        var property = {};
        property.name = propertyName + "";
        property.parentObjectProxy = objectProxy;
        var isGetter = object["__lookupGetter__"] && object.__lookupGetter__(propertyName);
        if (!isGetter) {
            try {
                var childObject = object[propertyName];
                var childObjectProxy = new InjectedScript.createProxyObject(childObject, objectProxy.objectId, abbreviate);
                childObjectProxy.path = objectProxy.path ? objectProxy.path.slice() : [];
                childObjectProxy.path.push(propertyName);
                property.value = childObjectProxy;
            } catch(e) {
                property.value = { description: e.toString() };
                property.isError = true;
            }
        } else {
            // FIXME: this should show something like "getter" (bug 16734).
            property.value = { description: "\u2014" }; // em dash
            property.isGetter = true;
        }
        properties.push(property);
    }
    return properties;
}

InjectedScript._isScopeProxy = function(objectProxy)
{
    var objectId = objectProxy.objectId;
    return typeof objectId === "object" && !objectId.thisObject;
} 

InjectedScript.setPropertyValue = function(objectProxy, propertyName, expression)
{
    var object = InjectedScript._resolveObject(objectProxy);
    if (!InjectedScript._isDefined(object))
        return false;

    var expressionLength = expression.length;
    if (!expressionLength) {
        delete object[propertyName];
        return !(propertyName in object);
    }

    try {
        // Surround the expression in parenthesis so the result of the eval is the result
        // of the whole expression not the last potential sub-expression.

        // There is a regression introduced here: eval is now happening against global object,
        // not call frame while on a breakpoint.
        // TODO: bring evaluation against call frame back.
        var result = inspectedWindow.eval("(" + expression + ")");
        // Store the result in the property.
        object[propertyName] = result;
        return true;
    } catch(e) {
        try {
            var result = inspectedWindow.eval("\"" + InjectedScript._escapeCharacters(expression, "\"") + "\"");
            object[propertyName] = result;
            return true;
        } catch(e) {
            return false;
        }
    }
}

InjectedScript.getNodePropertyValue = function(nodeId, propertyName)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;
    var result = node[propertyName];
    return result !== undefined ? result : false;
}

InjectedScript.setOuterHTML = function(nodeId, value, expanded)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;

    var parent = node.parentNode;
    var prevSibling = node.previousSibling;
    node.outerHTML = value;
    var newNode = prevSibling ? prevSibling.nextSibling : parent.firstChild;

    return InjectedScriptHost.pushNodePathToFrontend(newNode, expanded, false);
}

InjectedScript._populatePropertyNames = function(object, resultSet)
{
    for (var o = object; o; o = o.__proto__) {
        try {
            var names = Object.getOwnPropertyNames(o);
            for (var i = 0; i < names.length; ++i)
                resultSet[names[i] + ""] = true;
        } catch (e) {
        }
    }
}

InjectedScript._getPropertyNames = function(object, resultSet)
{
    var propertyNameSet = {};
    InjectedScript._populatePropertyNames(object, propertyNameSet);
    return Object.keys(propertyNameSet);
}

InjectedScript.getCompletions = function(expression, includeInspectorCommandLineAPI, callFrameId)
{
    var props = {};
    try {
        var expressionResult;
        // Evaluate on call frame if call frame id is available.
        if (typeof callFrameId === "number") {
            var callFrame = InjectedScript._callFrameForId(callFrameId);
            if (!callFrame)
                return props;
            if (expression)
                expressionResult = InjectedScript._evaluateOn(callFrame.evaluate, callFrame, expression, true);
            else {
                // Evaluate into properties in scope of the selected call frame.
                var scopeChain = callFrame.scopeChain;
                for (var i = 0; i < scopeChain.length; ++i)
                    InjectedScript._populatePropertyNames(scopeChain[i], props);
            }
        } else {
            if (!expression)
                expression = "this";
            expressionResult = InjectedScript._evaluateOn(inspectedWindow.eval, inspectedWindow, expression, false);
        }
        if (typeof expressionResult == "object")
            InjectedScript._populatePropertyNames(expressionResult, props);

        if (includeInspectorCommandLineAPI) {
            for (var prop in InjectedScript._commandLineAPI)
                props[prop] = true;
        }
    } catch(e) {
    }
    return props;
}

InjectedScript.evaluate = function(expression, objectGroup)
{
    return InjectedScript._evaluateAndWrap(inspectedWindow.eval, inspectedWindow, expression, objectGroup);
}

InjectedScript._evaluateAndWrap = function(evalFunction, object, expression, objectGroup, dontUseCommandLineAPI)
{
    var result = {};
    try {
        result.value = InjectedScript.wrapObject(InjectedScript._evaluateOn(evalFunction, object, expression, dontUseCommandLineAPI), objectGroup);

        // Handle error that might have happened while describing result.
        if (result.value.errorText) {
            result.value = result.value.errorText;
            result.isException = true;
        }
    } catch (e) {
        result.value = e.toString();
        result.isException = true;
    }
    return result;
}

InjectedScript._evaluateOn = function(evalFunction, object, expression, dontUseCommandLineAPI)
{
    if (!dontUseCommandLineAPI) {
        // Only install command line api object for the time of evaluation.

        // Surround the expression in with statements to inject our command line API so that
        // the window object properties still take more precedent than our API functions.
        inspectedWindow.console._commandLineAPI = InjectedScript._commandLineAPI;

        expression = "with (window.console._commandLineAPI) { with (window) {\n" + expression + "\n} }";
    }

    var value = evalFunction.call(object, expression);

    if (!dontUseCommandLineAPI)
        delete inspectedWindow.console._commandLineAPI;

    // When evaluating on call frame error is not thrown, but returned as a value.
    if (InjectedScript._type(value) === "error")
        throw value.toString();

    return value;
}

InjectedScript.addInspectedNode = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;

    var inspectedNodes = InjectedScript._inspectedNodes;
    inspectedNodes.unshift(node);
    if (inspectedNodes.length >= 5)
        inspectedNodes.pop();
    return true;
}

InjectedScript.getNodeId = function(node)
{
    return InjectedScriptHost.pushNodePathToFrontend(node, false, false);
}

InjectedScript.performSearch = function(whitespaceTrimmedQuery, runSynchronously)
{
    // FIXME: Few things are missing here:
    // 1) Search works with node granularity - number of matches within node is not calculated.
    // 2) Search does not work outside main documents' domain - we need to use specific InjectedScript instances
    //    for other domains.
    // 3) There is no need to push all search results to the front-end at a time, pushing next / previous result
    //    is sufficient.
    var tagNameQuery = whitespaceTrimmedQuery;
    var attributeNameQuery = whitespaceTrimmedQuery;
    var startTagFound = (tagNameQuery.indexOf("<") === 0);
    var endTagFound = (tagNameQuery.lastIndexOf(">") === (tagNameQuery.length - 1));

    if (startTagFound || endTagFound) {
        var tagNameQueryLength = tagNameQuery.length;
        tagNameQuery = tagNameQuery.substring((startTagFound ? 1 : 0), (endTagFound ? (tagNameQueryLength - 1) : tagNameQueryLength));
    }

    // Check the tagNameQuery is it is a possibly valid tag name.
    if (!/^[a-zA-Z0-9\-_:]+$/.test(tagNameQuery))
        tagNameQuery = null;

    // Check the attributeNameQuery is it is a possibly valid tag name.
    if (!/^[a-zA-Z0-9\-_:]+$/.test(attributeNameQuery))
        attributeNameQuery = null;

    const escapedQuery = InjectedScript._escapeCharacters(whitespaceTrimmedQuery, "'");
    const escapedTagNameQuery = (tagNameQuery ?  InjectedScript._escapeCharacters(tagNameQuery, "'") : null);
    const escapedWhitespaceTrimmedQuery = InjectedScript._escapeCharacters(whitespaceTrimmedQuery, "'");
    const searchResultsProperty = InjectedScript._includedInSearchResultsPropertyName;

    function addNodesToResults(nodes, length, getItem)
    {
        if (!length)
            return;

        var nodeIds = [];
        for (var i = 0; i < length; ++i) {
            var node = getItem.call(nodes, i);
            // Skip this node if it already has the property.
            if (searchResultsProperty in node)
                continue;

            if (!InjectedScript._searchResults.length) {
                InjectedScript._currentSearchResultIndex = 0;
            }

            node[searchResultsProperty] = true;
            InjectedScript._searchResults.push(node);
            var nodeId = InjectedScriptHost.pushNodePathToFrontend(node, false, false);
            nodeIds.push(nodeId);
        }
        InjectedScriptHost.addNodesToSearchResult(nodeIds.join(","));
    }

    function matchExactItems(doc)
    {
        matchExactId.call(this, doc);
        matchExactClassNames.call(this, doc);
        matchExactTagNames.call(this, doc);
        matchExactAttributeNames.call(this, doc);
    }

    function matchExactId(doc)
    {
        const result = doc.__proto__.getElementById.call(doc, whitespaceTrimmedQuery);
        addNodesToResults.call(this, result, (result ? 1 : 0), function() { return this });
    }

    function matchExactClassNames(doc)
    {
        const result = doc.__proto__.getElementsByClassName.call(doc, whitespaceTrimmedQuery);
        addNodesToResults.call(this, result, result.length, result.item);
    }

    function matchExactTagNames(doc)
    {
        if (!tagNameQuery)
            return;
        const result = doc.__proto__.getElementsByTagName.call(doc, tagNameQuery);
        addNodesToResults.call(this, result, result.length, result.item);
    }

    function matchExactAttributeNames(doc)
    {
        if (!attributeNameQuery)
            return;
        const result = doc.__proto__.querySelectorAll.call(doc, "[" + attributeNameQuery + "]");
        addNodesToResults.call(this, result, result.length, result.item);
    }

    function matchPartialTagNames(doc)
    {
        if (!tagNameQuery)
            return;
        const result = doc.__proto__.evaluate.call(doc, "//*[contains(name(), '" + escapedTagNameQuery + "')]", doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        addNodesToResults.call(this, result, result.snapshotLength, result.snapshotItem);
    }

    function matchStartOfTagNames(doc)
    {
        if (!tagNameQuery)
            return;
        const result = doc.__proto__.evaluate.call(doc, "//*[starts-with(name(), '" + escapedTagNameQuery + "')]", doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        addNodesToResults.call(this, result, result.snapshotLength, result.snapshotItem);
    }

    function matchPartialTagNamesAndAttributeValues(doc)
    {
        if (!tagNameQuery) {
            matchPartialAttributeValues.call(this, doc);
            return;
        }

        const result = doc.__proto__.evaluate.call(doc, "//*[contains(name(), '" + escapedTagNameQuery + "') or contains(@*, '" + escapedQuery + "')]", doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        addNodesToResults.call(this, result, result.snapshotLength, result.snapshotItem);
    }

    function matchPartialAttributeValues(doc)
    {
        const result = doc.__proto__.evaluate.call(doc, "//*[contains(@*, '" + escapedQuery + "')]", doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        addNodesToResults.call(this, result, result.snapshotLength, result.snapshotItem);
    }

    function matchStyleSelector(doc)
    {
        const result = doc.__proto__.querySelectorAll.call(doc, whitespaceTrimmedQuery);
        addNodesToResults.call(this, result, result.length, result.item);
    }

    function matchPlainText(doc)
    {
        const result = doc.__proto__.evaluate.call(doc, "//text()[contains(., '" + escapedQuery + "')] | //comment()[contains(., '" + escapedQuery + "')]", doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        addNodesToResults.call(this, result, result.snapshotLength, result.snapshotItem);
    }

    function matchXPathQuery(doc)
    {
        const result = doc.__proto__.evaluate.call(doc, whitespaceTrimmedQuery, doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        addNodesToResults.call(this, result, result.snapshotLength, result.snapshotItem);
    }

    function finishedSearching()
    {
        // Remove the searchResultsProperty now that the search is finished.
        for (var i = 0; i < InjectedScript._searchResults.length; ++i)
            delete InjectedScript._searchResults[i][searchResultsProperty];
    }

    const mainFrameDocument = inspectedWindow.document;
    const searchDocuments = [mainFrameDocument];
    var searchFunctions;
    if (tagNameQuery && startTagFound && endTagFound)
        searchFunctions = [matchExactTagNames, matchPlainText];
    else if (tagNameQuery && startTagFound)
        searchFunctions = [matchStartOfTagNames, matchPlainText];
    else if (tagNameQuery && endTagFound) {
        // FIXME: we should have a matchEndOfTagNames search function if endTagFound is true but not startTagFound.
        // This requires ends-with() support in XPath, WebKit only supports starts-with() and contains().
        searchFunctions = [matchPartialTagNames, matchPlainText];
    } else if (whitespaceTrimmedQuery === "//*" || whitespaceTrimmedQuery === "*") {
        // These queries will match every node. Matching everything isn't useful and can be slow for large pages,
        // so limit the search functions list to plain text and attribute matching.
        searchFunctions = [matchPartialAttributeValues, matchPlainText];
    } else
        searchFunctions = [matchExactItems, matchStyleSelector, matchPartialTagNamesAndAttributeValues, matchPlainText, matchXPathQuery];

    // Find all frames, iframes and object elements to search their documents.
    const subdocumentResult = mainFrameDocument.querySelectorAll("iframe, frame, object");

    for (var i = 0; i < subdocumentResult.length; ++i) {
        var element = subdocumentResult.item(i);
        if (element.contentDocument)
            searchDocuments.push(element.contentDocument);
    }

    const panel = InjectedScript;
    var documentIndex = 0;
    var searchFunctionIndex = 0;
    var chunkIntervalIdentifier = null;

    // Split up the work into chunks so we don't block the UI thread while processing.

    function processChunk()
    {
        var searchDocument = searchDocuments[documentIndex];
        var searchFunction = searchFunctions[searchFunctionIndex];

        if (++searchFunctionIndex > searchFunctions.length) {
            searchFunction = searchFunctions[0];
            searchFunctionIndex = 0;

            if (++documentIndex > searchDocuments.length) {
                if (panel._currentSearchChunkIntervalIdentifier === chunkIntervalIdentifier)
                    delete panel._currentSearchChunkIntervalIdentifier;
                clearInterval(chunkIntervalIdentifier);
                finishedSearching.call(panel);
                return false;
            }

            searchDocument = searchDocuments[documentIndex];
        }

        try {
            searchFunction.call(panel, searchDocument);
        } catch(err) {
            // ignore any exceptions. the query might be malformed, but we allow that.
        }
        return true;
    }

    if (runSynchronously)
        while (processChunk()) {}
    else {
        processChunk();
        chunkIntervalIdentifier = setInterval(processChunk, 25);
        InjectedScript._currentSearchChunkIntervalIdentifier = chunkIntervalIdentifier;
    }
    return true;
}

InjectedScript.searchCanceled = function()
{
    if (InjectedScript._searchResults) {
        const searchResultsProperty = InjectedScript._includedInSearchResultsPropertyName;
        for (var i = 0; i < this._searchResults.length; ++i) {
            var node = this._searchResults[i];

            // Remove the searchResultsProperty since there might be an unfinished search.
            delete node[searchResultsProperty];
        }
    }

    if (InjectedScript._currentSearchChunkIntervalIdentifier) {
        clearInterval(InjectedScript._currentSearchChunkIntervalIdentifier);
        delete InjectedScript._currentSearchChunkIntervalIdentifier;
    }
    InjectedScript._searchResults = [];
    return true;
}

InjectedScript.openInInspectedWindow = function(url)
{
    // Don't call window.open on wrapper - popup blocker mutes it.
    // URIs should have no double quotes.
    inspectedWindow.eval("window.open(\"" + url + "\")");
    return true;
}

InjectedScript.callFrames = function()
{
    var callFrame = InjectedScriptHost.currentCallFrame();
    if (!callFrame)
        return false;

    var result = [];
    var depth = 0;
    do {
        result.push(new InjectedScript.CallFrameProxy(depth++, callFrame));
        callFrame = callFrame.caller;
    } while (callFrame);
    return result;
}

InjectedScript.evaluateInCallFrame = function(callFrameId, code, objectGroup)
{
    var callFrame = InjectedScript._callFrameForId(callFrameId);
    if (!callFrame)
        return false;
    return InjectedScript._evaluateAndWrap(callFrame.evaluate, callFrame, code, objectGroup, true);
}

InjectedScript._callFrameForId = function(id)
{
    var callFrame = InjectedScriptHost.currentCallFrame();
    while (--id >= 0 && callFrame)
        callFrame = callFrame.caller;
    return callFrame;
}

InjectedScript.clearConsoleMessages = function()
{
    InjectedScriptHost.clearConsoleMessages();
    return true;
}

InjectedScript._resolveObject = function(objectProxy)
{
    var object = InjectedScript._objectForId(objectProxy.objectId);
    var path = objectProxy.path;
    var protoDepth = objectProxy.protoDepth;

    // Follow the property path.
    for (var i = 0; InjectedScript._isDefined(object) && path && i < path.length; ++i)
        object = object[path[i]];

    return object;
}

InjectedScript._nodeForId = function(nodeId)
{
    if (!nodeId)
        return null;
    return InjectedScriptHost.nodeForId(nodeId);
}

InjectedScript._objectForId = function(objectId)
{
    // There are three types of object ids used:
    // - numbers point to DOM Node via the InspectorDOMAgent mapping
    // - strings point to console objects cached in InspectorController for lazy evaluation upon them
    // - objects contain complex ids and are currently used for scoped objects
    if (typeof objectId === "number")
        return InjectedScript._nodeForId(objectId);
    else if (typeof objectId === "string")
        return InjectedScript.unwrapObject(objectId);
    else if (typeof objectId === "object") {
        var callFrame = InjectedScript._callFrameForId(objectId.callFrame);
        if (objectId.thisObject)
            return callFrame.thisObject;
        else
            return callFrame.scopeChain[objectId.chainIndex];
    }
    return objectId;
}

InjectedScript.pushNodeToFrontend = function(objectProxy)
{
    var object = InjectedScript._resolveObject(objectProxy);
    if (!object || InjectedScript._type(object) !== "node")
        return false;
    return InjectedScriptHost.pushNodePathToFrontend(object, false, false);
}

InjectedScript.nodeByPath = function(path)
{
    // We make this call through the injected script only to get a nice
    // callback for it.
    return InjectedScriptHost.pushNodeByPathToFrontend(path.join(","));
}

// Called from within InspectorController on the 'inspected page' side.
InjectedScript.createProxyObject = function(object, objectId, abbreviate)
{
    var result = {};
    result.injectedScriptId = injectedScriptId;
    result.objectId = objectId;
    result.type = InjectedScript._type(object);
    if (result.type === "array")
        result.propertyLength = object.length;

    var type = typeof object;
    
    result.hasChildren = (type === "object" && object !== null && (Object.getOwnPropertyNames(object).length || object.__proto__)) || type === "function";
    try {
        result.description = InjectedScript._describe(object, abbreviate);
    } catch (e) {
        result.errorText = e.toString();
    }
    return result;
}

InjectedScript.evaluateOnSelf = function(funcBody, args)
{
    var func = window.eval("(" + funcBody + ")");
    return func.apply(this, args || []);
}

InjectedScript.CallFrameProxy = function(id, callFrame)
{
    this.id = id;
    this.type = callFrame.type;
    this.functionName = (this.type === "function" ? callFrame.functionName : "");
    this.sourceID = callFrame.sourceID;
    this.line = callFrame.line;
    this.scopeChain = this._wrapScopeChain(callFrame);
}

InjectedScript.CallFrameProxy.prototype = {
    _wrapScopeChain: function(callFrame)
    {
        const GLOBAL_SCOPE = 0;
        const LOCAL_SCOPE = 1;
        const WITH_SCOPE = 2;
        const CLOSURE_SCOPE = 3;
        const CATCH_SCOPE = 4;
    
        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        var foundLocalScope = false;
        for (var i = 0; i < scopeChain.length; i++) {
            var scopeType = callFrame.scopeType(i);
            var scopeObject = scopeChain[i];
            var scopeObjectProxy = InjectedScript.createProxyObject(scopeObject, { callFrame: this.id, chainIndex: i }, true);

            switch(scopeType) {
                case LOCAL_SCOPE: {
                    foundLocalScope = true;
                    scopeObjectProxy.isLocal = true;
                    scopeObjectProxy.thisObject = InjectedScript.createProxyObject(callFrame.thisObject, { callFrame: this.id, thisObject: true }, true);
                    break;
                }
                case CLOSURE_SCOPE: {
                    scopeObjectProxy.isClosure = true;
                    break;
                }
                case WITH_SCOPE:
                case CATCH_SCOPE: {
                    if (foundLocalScope && scopeObject instanceof inspectedWindow.Element)
                        scopeObjectProxy.isElement = true;
                    else if (foundLocalScope && scopeObject instanceof inspectedWindow.Document)
                        scopeObjectProxy.isDocument = true;
                    else
                        scopeObjectProxy.isWithBlock = true;
                    break;
                }
            }
            scopeChainProxy.push(scopeObjectProxy);
        }
        return scopeChainProxy;
    }
}

InjectedScript.executeSql = function(callId, databaseId, query)
{
    function successCallback(tx, result)
    {
        var rows = result.rows;
        var result = [];
        var length = rows.length;
        for (var i = 0; i < length; ++i) {
            var data = {};
            result.push(data);
            var row = rows.item(i);
            for (var columnIdentifier in row) {
                // FIXME: (Bug 19439) We should specially format SQL NULL here
                // (which is represented by JavaScript null here, and turned
                // into the string "null" by the String() function).
                var text = row[columnIdentifier];
                data[columnIdentifier] = String(text);
            }
        }
        InjectedScriptHost.reportDidDispatchOnInjectedScript(callId, result, false);
    }

    function errorCallback(tx, error)
    {
        InjectedScriptHost.reportDidDispatchOnInjectedScript(callId, error, false);
    }

    function queryTransaction(tx)
    {
        tx.executeSql(query, null, successCallback, errorCallback);
    }

    var database = InjectedScriptHost.databaseForId(databaseId);
    if (!database)
        errorCallback(null, { code : 2 });  // Return as unexpected version.
    database.transaction(queryTransaction, errorCallback);
    return true;
}

InjectedScript._isDefined = function(object)
{
    return object || InjectedScript._isHTMLAllCollection(object);
}

InjectedScript._isHTMLAllCollection = function(object)
{
    // document.all is reported as undefined, but we still want to process it.
    return (typeof object === "undefined") && inspectedWindow.HTMLAllCollection && object instanceof inspectedWindow.HTMLAllCollection;
}

InjectedScript._type = function(obj)
{
    if (obj === null)
        return "null";

    var type = typeof obj;
    if (type !== "object" && type !== "function") {
        // FIXME(33716): typeof document.all is always 'undefined'.
        if (InjectedScript._isHTMLAllCollection(obj))
            return "array";
        return type;
    }

    // If owning frame has navigated to somewhere else window properties will be undefined.
    // In this case just return result of the typeof.
    if (!inspectedWindow.document)
        return type;

    if (obj instanceof inspectedWindow.Node)
        return (obj.nodeType === undefined ? type : "node");
    if (obj instanceof inspectedWindow.String)
        return "string";
    if (obj instanceof inspectedWindow.Array)
        return "array";
    if (obj instanceof inspectedWindow.Boolean)
        return "boolean";
    if (obj instanceof inspectedWindow.Number)
        return "number";
    if (obj instanceof inspectedWindow.Date)
        return "date";
    if (obj instanceof inspectedWindow.RegExp)
        return "regexp";
    if (obj instanceof inspectedWindow.NodeList)
        return "array";
    if (obj instanceof inspectedWindow.HTMLCollection)
        return "array";
    if (obj instanceof inspectedWindow.Error)
        return "error";
    return type;
}

InjectedScript._describe = function(obj, abbreviated)
{
    var type1 = InjectedScript._type(obj);
    var type2 = InjectedScript._className(obj);

    switch (type1) {
    case "object":
    case "node":
    case "array":
        return type2;
    case "string":
        if (!abbreviated)
            return obj;
        if (obj.length > 100)
            return "\"" + obj.substring(0, 100) + "\u2026\"";
        return "\"" + obj + "\"";
    case "function":
        var objectText = InjectedScript._toString(obj);
        if (!/^function /.test(objectText))
            objectText = (type2 == "object") ? type1 : type2;
        else if (abbreviated)
            objectText = /.*/.exec(obj)[0].replace(/ +$/g, "");
        return objectText;
    default:
        return InjectedScript._toString(obj);
    }
}

InjectedScript._toString = function(obj)
{
    // We don't use String(obj) because inspectedWindow.String is undefined if owning frame navigated to another page.
    return "" + obj;
}

InjectedScript._className = function(obj)
{
    var str = inspectedWindow.Object ? inspectedWindow.Object.prototype.toString.call(obj) : InjectedScript._toString(obj);
    return str.replace(/^\[object (.*)\]$/i, "$1");
}

InjectedScript._escapeCharacters = function(str, chars)
{
    var foundChar = false;
    for (var i = 0; i < chars.length; ++i) {
        if (str.indexOf(chars.charAt(i)) !== -1) {
            foundChar = true;
            break;
        }
    }

    if (!foundChar)
        return str;

    var result = "";
    for (var i = 0; i < str.length; ++i) {
        if (chars.indexOf(str.charAt(i)) !== -1)
            result += "\\";
        result += str.charAt(i);
    }

    return result;
}

InjectedScript._logEvent = function(event)
{
    console.log(event.type, event);
}

InjectedScript._normalizeEventTypes = function(types)
{
    if (typeof types === "undefined")
        types = [ "mouse", "key", "load", "unload", "abort", "error", "select", "change", "submit", "reset", "focus", "blur", "resize", "scroll" ];
    else if (typeof types === "string")
        types = [ types ];

    var result = [];
    for (var i = 0; i < types.length; i++) {
        if (types[i] === "mouse")
            result.splice(0, 0, "mousedown", "mouseup", "click", "dblclick", "mousemove", "mouseover", "mouseout");
        else if (types[i] === "key")
            result.splice(0, 0, "keydown", "keyup", "keypress");
        else
            result.push(types[i]);
    }
    return result;
};

function CommandLineAPI()
{
}

CommandLineAPI.prototype = {
    // Only add API functions here, private stuff should go to
    // InjectedScript so that it is not suggested by the completion.
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
        var nodes = [];
        try {
            var doc = context || document;
            var results = doc.evaluate(xpath, doc, null, XPathResult.ANY_TYPE, null);
            var node;
            while (node = results.iterateNext())
                nodes.push(node);
        } catch (e) {
        }
        return nodes;
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
        types = InjectedScript._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i) {
            object.removeEventListener(types[i], InjectedScript._logEvent, false);
            object.addEventListener(types[i], InjectedScript._logEvent, false);
        }
    },

    unmonitorEvents: function(object, types)
    {
        if (!object || !object.addEventListener || !object.removeEventListener)
            return;
        types = InjectedScript._normalizeEventTypes(types);
        for (var i = 0; i < types.length; ++i)
            object.removeEventListener(types[i], InjectedScript._logEvent, false);
    },

    inspect: function(object)
    {
        if (arguments.length === 0)
            return;

        inspectedWindow.console.log(object);
        if (InjectedScript._type(object) === "node")
            InjectedScriptHost.pushNodePathToFrontend(object, false, true);
        else {
            switch (InjectedScript._describe(object)) {
                case "Database":
                    InjectedScriptHost.selectDatabase(object);
                    break;
                case "Storage":
                    InjectedScriptHost.selectDOMStorage(object);
                    break;
            }
        }
    },

    copy: function(object)
    {
        if (InjectedScript._type(object) === "node") {
            var nodeId = InjectedScriptHost.pushNodePathToFrontend(object, false, false);
            InjectedScriptHost.copyNode(nodeId);
        } else
            InjectedScriptHost.copyText(object);
    },

    clear: function()
    {
        InjectedScriptHost.clearConsoleMessages();
    },

    get $0()
    {
        return InjectedScript._inspectedNodes[0];
    },

    get $1()
    {
        return InjectedScript._inspectedNodes[1];
    },

    get $2()
    {
        return InjectedScript._inspectedNodes[2];
    },

    get $3()
    {
        return InjectedScript._inspectedNodes[3];
    },

    get $4()
    {
        return InjectedScript._inspectedNodes[4];
    }
}

InjectedScript._commandLineAPI = new CommandLineAPI();

return InjectedScript;
});
