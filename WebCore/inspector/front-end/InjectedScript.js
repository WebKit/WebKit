/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

var InjectedScript = {};

// Called from within InspectorController on the 'inspected page' side.
InjectedScript.reset = function()
{
    InjectedScript._styles = {};
    InjectedScript._styleRules = {};
    InjectedScript._lastStyleId = 0;
    InjectedScript._lastStyleRuleId = 0;
    InjectedScript._searchResults = [];
    InjectedScript._includedInSearchResultsPropertyName = "__includedInInspectorSearchResults";
}

InjectedScript.reset();

InjectedScript.getStyles = function(nodeId, authorOnly)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;
    var matchedRules = InjectedScript._window().getMatchedCSSRules(node, "", authorOnly);
    var matchedCSSRules = [];
    for (var i = 0; matchedRules && i < matchedRules.length; ++i)
        matchedCSSRules.push(InjectedScript._serializeRule(matchedRules[i]));

    var styleAttributes = {};
    var attributes = node.attributes;
    for (var i = 0; attributes && i < attributes.length; ++i) {
        if (attributes[i].style)
            styleAttributes[attributes[i].name] = InjectedScript._serializeStyle(attributes[i].style, true);
    }
    var result = {};
    result.inlineStyle = InjectedScript._serializeStyle(node.style, true);
    result.computedStyle = InjectedScript._serializeStyle(InjectedScript._window().getComputedStyle(node));
    result.matchedCSSRules = matchedCSSRules;
    result.styleAttributes = styleAttributes;
    return result;
}

InjectedScript.getComputedStyle = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;
    return InjectedScript._serializeStyle(InjectedScript._window().getComputedStyle(node));
}

InjectedScript.getInlineStyle = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;
    return InjectedScript._serializeStyle(node.style, true);
}

InjectedScript.applyStyleText = function(styleId, styleText, propertyName)
{
    var style = InjectedScript._styles[styleId];
    if (!style)
        return false;

    var styleTextLength = styleText.length;

    // Create a new element to parse the user input CSS.
    var parseElement = document.createElement("span");
    parseElement.setAttribute("style", styleText);

    var tempStyle = parseElement.style;
    if (tempStyle.length || !styleTextLength) {
        // The input was parsable or the user deleted everything, so remove the
        // original property from the real style declaration. If this represents
        // a shorthand remove all the longhand properties.
        if (style.getPropertyShorthand(propertyName)) {
            var longhandProperties = InjectedScript._getLonghandProperties(style, propertyName);
            for (var i = 0; i < longhandProperties.length; ++i)
                style.removeProperty(longhandProperties[i]);
        } else
            style.removeProperty(propertyName);
    }

    if (!tempStyle.length)
        return false;

    // Iterate of the properties on the test element's style declaration and
    // add them to the real style declaration. We take care to move shorthands.
    var foundShorthands = {};
    var changedProperties = [];
    var uniqueProperties = InjectedScript._getUniqueStyleProperties(tempStyle);
    for (var i = 0; i < uniqueProperties.length; ++i) {
        var name = uniqueProperties[i];
        var shorthand = tempStyle.getPropertyShorthand(name);

        if (shorthand && shorthand in foundShorthands)
            continue;

        if (shorthand) {
            var value = InjectedScript._getShorthandValue(tempStyle, shorthand);
            var priority = InjectedScript._getShorthandPriority(tempStyle, shorthand);
            foundShorthands[shorthand] = true;
        } else {
            var value = tempStyle.getPropertyValue(name);
            var priority = tempStyle.getPropertyPriority(name);
        }

        // Set the property on the real style declaration.
        style.setProperty((shorthand || name), value, priority);
        changedProperties.push(shorthand || name);
    }
    return [InjectedScript._serializeStyle(style, true), changedProperties];
}

InjectedScript.setStyleText = function(style, cssText)
{
    style.cssText = cssText;
}

InjectedScript.toggleStyleEnabled = function(styleId, propertyName, disabled)
{
    var style = InjectedScript._styles[styleId];
    if (!style)
        return false;

    if (disabled) {
        if (!style.__disabledPropertyValues || !style.__disabledPropertyPriorities) {
            var inspectedWindow = InjectedScript._window();
            style.__disabledProperties = new inspectedWindow.Object;
            style.__disabledPropertyValues = new inspectedWindow.Object;
            style.__disabledPropertyPriorities = new inspectedWindow.Object;
        }

        style.__disabledPropertyValues[propertyName] = style.getPropertyValue(propertyName);
        style.__disabledPropertyPriorities[propertyName] = style.getPropertyPriority(propertyName);

        if (style.getPropertyShorthand(propertyName)) {
            var longhandProperties = InjectedScript._getLonghandProperties(style, propertyName);
            for (var i = 0; i < longhandProperties.length; ++i) {
                style.__disabledProperties[longhandProperties[i]] = true;
                style.removeProperty(longhandProperties[i]);
            }
        } else {
            style.__disabledProperties[propertyName] = true;
            style.removeProperty(propertyName);
        }
    } else if (style.__disabledProperties && style.__disabledProperties[propertyName]) {
        var value = style.__disabledPropertyValues[propertyName];
        var priority = style.__disabledPropertyPriorities[propertyName];

        style.setProperty(propertyName, value, priority);
        delete style.__disabledProperties[propertyName];
        delete style.__disabledPropertyValues[propertyName];
        delete style.__disabledPropertyPriorities[propertyName];
    }
    return InjectedScript._serializeStyle(style, true);
}

InjectedScript.applyStyleRuleText = function(ruleId, newContent, selectedNodeId)
{
    var rule = InjectedScript._styleRules[ruleId];
    if (!rule)
        return false;

    var selectedNode = InjectedScript._nodeForId(selectedNodeId);

    try {
        var stylesheet = rule.parentStyleSheet;
        stylesheet.addRule(newContent);
        var newRule = stylesheet.cssRules[stylesheet.cssRules.length - 1];
        newRule.style.cssText = rule.style.cssText;

        var parentRules = stylesheet.cssRules;
        for (var i = 0; i < parentRules.length; ++i) {
            if (parentRules[i] === rule) {
                rule.parentStyleSheet.removeRule(i);
                break;
            }
        }

        return [InjectedScript._serializeRule(newRule), InjectedScript._doesSelectorAffectNode(newContent, selectedNode)];
    } catch(e) {
        // Report invalid syntax.
        return false;
    }
}

InjectedScript.addStyleSelector = function(newContent, selectedNodeId)
{
    var stylesheet = InjectedScript.stylesheet;
    if (!stylesheet) {
        var inspectedDocument = InjectedScript._window().document;
        var head = inspectedDocument.getElementsByTagName("head")[0];
        var styleElement = inspectedDocument.createElement("style");
        styleElement.type = "text/css";
        head.appendChild(styleElement);
        stylesheet = inspectedDocument.styleSheets[inspectedDocument.styleSheets.length - 1];
        InjectedScript.stylesheet = stylesheet;
    }

    try {
        stylesheet.addRule(newContent);
    } catch (e) {
        // Invalid Syntax for a Selector
        return false;
    }

    var selectedNode = InjectedScript._nodeForId(selectedNodeId);
    var rule = stylesheet.cssRules[stylesheet.cssRules.length - 1];
    rule.__isViaInspector = true;

    return [ InjectedScript._serializeRule(rule), InjectedScript._doesSelectorAffectNode(newContent, selectedNode) ];
}

InjectedScript._doesSelectorAffectNode = function(selectorText, node)
{
    if (!node)
        return false;
    var nodes = node.ownerDocument.querySelectorAll(selectorText);
    for (var i = 0; i < nodes.length; ++i) {
        if (nodes[i] === node) {
            return true;
        }
    }
    return false;
}

InjectedScript.setStyleProperty = function(styleId, name, value)
{
    var style = InjectedScript._styles[styleId];
    if (!style)
        return false;

    style.setProperty(name, value, "");
    return true;
}

InjectedScript._serializeRule = function(rule)
{
    var parentStyleSheet = rule.parentStyleSheet;

    var ruleValue = {};
    ruleValue.selectorText = rule.selectorText;
    if (parentStyleSheet) {
        ruleValue.parentStyleSheet = {};
        ruleValue.parentStyleSheet.href = parentStyleSheet.href;
    }
    ruleValue.isUserAgent = parentStyleSheet && !parentStyleSheet.ownerNode && !parentStyleSheet.href;
    ruleValue.isUser = parentStyleSheet && parentStyleSheet.ownerNode && parentStyleSheet.ownerNode.nodeName == "#document";
    ruleValue.isViaInspector = !!rule.__isViaInspector;

    // Bind editable scripts only.
    var doBind = !ruleValue.isUserAgent && !ruleValue.isUser;
    ruleValue.style = InjectedScript._serializeStyle(rule.style, doBind);

    if (doBind) {
        if (!rule.id) {
            rule.id = InjectedScript._lastStyleRuleId++;
            InjectedScript._styleRules[rule.id] = rule;
        }
        ruleValue.id = rule.id;
    }
    return ruleValue;
}

InjectedScript._serializeStyle = function(style, doBind)
{
    var result = {};
    result.width = style.width;
    result.height = style.height;
    result.__disabledProperties = style.__disabledProperties;
    result.__disabledPropertyValues = style.__disabledPropertyValues;
    result.__disabledPropertyPriorities = style.__disabledPropertyPriorities;
    result.properties = [];
    result.shorthandValues = {};
    var foundShorthands = {};
    for (var i = 0; i < style.length; ++i) {
        var property = {};
        var name = style[i];
        property.name = name;
        property.priority = style.getPropertyPriority(name);
        property.implicit = style.isPropertyImplicit(name);
        var shorthand =  style.getPropertyShorthand(name);
        property.shorthand = shorthand;
        if (shorthand && !(shorthand in foundShorthands)) {
            foundShorthands[shorthand] = true;
            result.shorthandValues[shorthand] = InjectedScript._getShorthandValue(style, shorthand);
        }
        property.value = style.getPropertyValue(name);
        result.properties.push(property);
    }
    result.uniqueStyleProperties = InjectedScript._getUniqueStyleProperties(style);

    if (doBind) {
        if (!style.id) {
            style.id = InjectedScript._lastStyleId++;
            InjectedScript._styles[style.id] = style;
        }
        result.id = style.id;
    }
    return result;
}

InjectedScript._getUniqueStyleProperties = function(style)
{
    var properties = [];
    var foundProperties = {};

    for (var i = 0; i < style.length; ++i) {
        var property = style[i];
        if (property in foundProperties)
            continue;
        foundProperties[property] = true;
        properties.push(property);
    }

    return properties;
}


InjectedScript._getLonghandProperties = function(style, shorthandProperty)
{
    var properties = [];
    var foundProperties = {};

    for (var i = 0; i < style.length; ++i) {
        var individualProperty = style[i];
        if (individualProperty in foundProperties || style.getPropertyShorthand(individualProperty) !== shorthandProperty)
            continue;
        foundProperties[individualProperty] = true;
        properties.push(individualProperty);
    }

    return properties;
}

InjectedScript._getShorthandValue = function(style, shorthandProperty)
{
    var value = style.getPropertyValue(shorthandProperty);
    if (!value) {
        // Some shorthands (like border) return a null value, so compute a shorthand value.
        // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15823 is fixed.

        var foundProperties = {};
        for (var i = 0; i < style.length; ++i) {
            var individualProperty = style[i];
            if (individualProperty in foundProperties || style.getPropertyShorthand(individualProperty) !== shorthandProperty)
                continue;

            var individualValue = style.getPropertyValue(individualProperty);
            if (style.isPropertyImplicit(individualProperty) || individualValue === "initial")
                continue;

            foundProperties[individualProperty] = true;

            if (!value)
                value = "";
            else if (value.length)
                value += " ";
            value += individualValue;
        }
    }
    return value;
}

InjectedScript._getShorthandPriority = function(style, shorthandProperty)
{
    var priority = style.getPropertyPriority(shorthandProperty);
    if (!priority) {
        for (var i = 0; i < style.length; ++i) {
            var individualProperty = style[i];
            if (style.getPropertyShorthand(individualProperty) !== shorthandProperty)
                continue;
            priority = style.getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}

InjectedScript.getPrototypes = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;

    var result = [];
    for (var prototype = node; prototype; prototype = prototype.__proto__) {
        var title = Object.describe(prototype);
        if (title.match(/Prototype$/)) {
            title = title.replace(/Prototype$/, "");
        }
        result.push(title);
    }
    return result;
}

InjectedScript.getProperties = function(objectProxy, ignoreHasOwnProperty)
{
    var object = InjectedScript._resolveObject(objectProxy);
    if (!object)
        return false;

    var properties = [];

    // Go over properties, prepare results.
    for (var propertyName in object) {
        if (!ignoreHasOwnProperty && "hasOwnProperty" in object && !object.hasOwnProperty(propertyName))
            continue;

        //TODO: remove this once object becomes really remote.
        if (propertyName === "__treeElementIdentifier")
            continue;
        var property = {};
        property.name = propertyName;
        property.parentObjectProxy = objectProxy;
        var isGetter = object["__lookupGetter__"] && object.__lookupGetter__(propertyName);
        if (!property.isGetter) {
            var childObject = object[propertyName];
            var childObjectProxy = {};
            childObjectProxy.objectId = objectProxy.objectId;
            childObjectProxy.path = objectProxy.path ? objectProxy.path.slice() : [];
            childObjectProxy.path.push(propertyName);

            childObjectProxy.protoDepth = objectProxy.protoDepth || 0;
            childObjectProxy.description = Object.describe(childObject, true);
            property.value = childObjectProxy;

            var type = typeof childObject;
            if (type === "object" || type === "function") {
                for (var subPropertyName in childObject) {
                    if (propertyName === "__treeElementIdentifier")
                        continue;
                    childObjectProxy.hasChildren = true;
                    break;
                }
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

InjectedScript.setPropertyValue = function(objectProxy, propertyName, expression)
{
    var object = InjectedScript._resolveObject(objectProxy);
    if (!object)
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
        var result = InjectedScript._window().eval("(" + expression + ")");
        // Store the result in the property.
        object[propertyName] = result;
        return true;
    } catch(e) {
        try {
            var result = InjectedScript._window().eval("\"" + expression.escapeCharacters("\"") + "\"");
            object[propertyName] = result;
            return true;
        } catch(e) {
            return false;
        }
    }
}

InjectedScript.evaluate = function(expression)
{
    InjectedScript._ensureCommandLineAPIInstalled();
    // Surround the expression in with statements to inject our command line API so that
    // the window object properties still take more precedent than our API functions.
    expression = "with (window._inspectorCommandLineAPI) { with (window) { " + expression + " } }";

    var result = {};
    try {
        var value = InjectedScript._window().eval(expression);
        var wrapper = InspectorController.wrapObject(value);
        if (typeof wrapper === "object" && wrapper.exception)
            result.exception = wrapper.exception;
        else
            result.value = wrapper;
    } catch (e) {
        result.exception = e.toString();
    }
    return result;
}

InjectedScript.addInspectedNode = function(nodeId)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return false;

    InjectedScript._ensureCommandLineAPIInstalled();
    var inspectedNodes = InjectedScript._window()._inspectorCommandLineAPI._inspectedNodes;
    inspectedNodes.unshift(node);
    if (inspectedNodes.length >= 5)
        inspectedNodes.pop();
    return true;
}

InjectedScript.performSearch = function(whitespaceTrimmedQuery, searchResultsProperty)
{
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

    const escapedQuery = whitespaceTrimmedQuery.escapeCharacters("'");
    const escapedTagNameQuery = (tagNameQuery ? tagNameQuery.escapeCharacters("'") : null);
    const escapedWhitespaceTrimmedQuery = whitespaceTrimmedQuery.escapeCharacters("'");
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
            var nodeId = InspectorController.pushNodePathToFrontend(node, false);
            nodeIds.push(nodeId);
        }
        InspectorController.addNodesToSearchResult(nodeIds.join(","));
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

    const mainFrameDocument = InjectedScript._window().document;
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
    const querySelectorAllFunction = InjectedScript._window().Document.prototype.querySelectorAll;
    const subdocumentResult = querySelectorAllFunction.call(mainFrameDocument, "iframe, frame, object");

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
                return;
            }

            searchDocument = searchDocuments[documentIndex];
        }

        if (!searchDocument || !searchFunction)
            return;

        try {
            searchFunction.call(panel, searchDocument);
        } catch(err) {
            // ignore any exceptions. the query might be malformed, but we allow that.
        }
    }

    processChunk();

    chunkIntervalIdentifier = setInterval(processChunk, 25);
    InjectedScript._currentSearchChunkIntervalIdentifier = chunkIntervalIdentifier;
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

InjectedScript.getCookies = function()
{
    return InjectedScript._window().document.cookie;
}

InjectedScript._ensureCommandLineAPIInstalled = function(inspectedWindow)
{
    var inspectedWindow = InjectedScript._window();
    if (inspectedWindow._inspectorCommandLineAPI)
        return;
    
    inspectedWindow.eval("window._inspectorCommandLineAPI = { \
        $: function() { return document.getElementById.apply(document, arguments) }, \
        $$: function() { return document.querySelectorAll.apply(document, arguments) }, \
        $x: function(xpath, context) { \
            var nodes = []; \
            try { \
                var doc = context || document; \
                var results = doc.evaluate(xpath, doc, null, XPathResult.ANY_TYPE, null); \
                var node; \
                while (node = results.iterateNext()) nodes.push(node); \
            } catch (e) {} \
            return nodes; \
        }, \
        dir: function() { return console.dir.apply(console, arguments) }, \
        dirxml: function() { return console.dirxml.apply(console, arguments) }, \
        keys: function(o) { var a = []; for (var k in o) a.push(k); return a; }, \
        values: function(o) { var a = []; for (var k in o) a.push(o[k]); return a; }, \
        profile: function() { return console.profile.apply(console, arguments) }, \
        profileEnd: function() { return console.profileEnd.apply(console, arguments) }, \
        _inspectedNodes: [], \
        get $0() { return _inspectorCommandLineAPI._inspectedNodes[0] }, \
        get $1() { return _inspectorCommandLineAPI._inspectedNodes[1] }, \
        get $2() { return _inspectorCommandLineAPI._inspectedNodes[2] }, \
        get $3() { return _inspectorCommandLineAPI._inspectedNodes[3] }, \
        get $4() { return _inspectorCommandLineAPI._inspectedNodes[4] } \
    };");

    inspectedWindow._inspectorCommandLineAPI.clear = InspectorController.wrapCallback(InspectorController.clearMessages.bind(InspectorController, true));
    inspectedWindow._inspectorCommandLineAPI.inspect = InspectorController.wrapCallback(inspectObject.bind(this));

    function inspectObject(o)
    {
        if (arguments.length === 0)
            return;

        inspectedWindow.console.log(o);
        if (Object.type(o, inspectedWindow) === "node") {
            InspectorController.pushNodePathToFrontend(o, true);
        } else {
            switch (Object.describe(o)) {
                case "Database":
                    InspectorController.selectDatabase(o);
                    break;
                case "Storage":
                    InspectorController.selectDOMStorage(o);
                    break;
            }
        }
    }
}

InjectedScript._resolveObject = function(objectProxy)
{
    var object = InjectedScript._objectForId(objectProxy.objectId);
    var path = objectProxy.path;
    var protoDepth = objectProxy.protoDepth;

    // Follow the property path.
    for (var i = 0; object && path && i < path.length; ++i)
        object = object[path[i]];

    // Get to the necessary proto layer.
    for (var i = 0; object && protoDepth && i < protoDepth; ++i)
        object = object.__proto__;

    return object;
}

InjectedScript._window = function()
{
    // TODO: replace with 'return window;' once this script is injected into
    // the page's context.
    return InspectorController.inspectedWindow();
}

InjectedScript._nodeForId = function(nodeId)
{
    if (!nodeId)
        return null;
    return InspectorController.nodeForId(nodeId);
}

InjectedScript._objectForId = function(objectId)
{
    if (typeof objectId === "number")
        return InjectedScript._nodeForId(objectId);
    else if (typeof objectId === "string")
        return InspectorController.unwrapObject(objectId);

    // TODO: move scope chain objects to proxy-based schema.
    return objectId;
}

// Called from within InspectorController on the 'inspected page' side.
InjectedScript.createProxyObject = function(object, objectId)
{
    var result = {};
    result.objectId = objectId;
    result.type = Object.type(object, InjectedScript._window());
    if (result.type === "node")
        result.nodeId = InspectorController.pushNodePathToFrontend(object, false);
    try {
        result.description = Object.describe(object, true, InjectedScript._window());
    } catch (e) {
        result.exception = e.toString();
    }
    return result;
}
