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

InjectedScript.dispatch = function(methodName, args)
{
    var result = InjectedScript[methodName].apply(InjectedScript, JSON.parse(args));
    return JSON.stringify(result);
}

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

    // Notify caller that the property was successfully deleted.
    if (!styleTextLength)
        return [null, [propertyName]];

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
        var title = Object.describe(prototype, true);
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

        var property = {};
        property.name = propertyName;
        property.parentObjectProxy = objectProxy;
        var isGetter = object["__lookupGetter__"] && object.__lookupGetter__(propertyName);
        if (!property.isGetter) {
            var childObject = object[propertyName];
            var childObjectProxy = new InjectedScript.createProxyObject(childObject, objectProxy.objectId, true);
            childObjectProxy.path = objectProxy.path ? objectProxy.path.slice() : [];
            childObjectProxy.path.push(propertyName);
            childObjectProxy.protoDepth = objectProxy.protoDepth || 0;
            property.value = childObjectProxy;
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


InjectedScript.getCompletions = function(expression, includeInspectorCommandLineAPI)
{
    var props = {};
    try {
        var expressionResult = InjectedScript._evaluateOn(InjectedScript._window().eval, InjectedScript._window(), expression);
        for (var prop in expressionResult)
            props[prop] = true;
        if (includeInspectorCommandLineAPI)
            for (var prop in InjectedScript._window()._inspectorCommandLineAPI)
                if (prop.charAt(0) !== '_')
                    props[prop] = true;
    } catch(e) {
    }
    return props;
}

InjectedScript.evaluate = function(expression)
{
    return InjectedScript._evaluateAndWrap(InjectedScript._window().eval, InjectedScript._window(), expression);
}

InjectedScript._evaluateAndWrap = function(evalFunction, object, expression)
{
    var result = {};
    try {
        result.value = InspectorController.wrapObject(InjectedScript._evaluateOn(evalFunction, object, expression));
        // Handle error that might have happened while describing result.
        if (result.value.errorText) {
            result.value = InspectorController.wrapObject(result.value.errorText);
            result.isException = true;
        }
    } catch (e) {
        result.value = InspectorController.wrapObject(e.toString());
        result.isException = true;
    }
    return result;
}

InjectedScript._evaluateOn = function(evalFunction, object, expression)
{
    InjectedScript._ensureCommandLineAPIInstalled();
    // Surround the expression in with statements to inject our command line API so that
    // the window object properties still take more precedent than our API functions.
    expression = "with (window._inspectorCommandLineAPI) { with (window) { " + expression + " } }";
    var value = evalFunction.call(object, expression);

    // When evaluating on call frame error is not thrown, but returned as a value.
    if (Object.type(value) === "error")
        throw value.toString();

    return value;
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

InjectedScript.performSearch = function(whitespaceTrimmedQuery)
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

InjectedScript.openInInspectedWindow = function(url)
{
    InjectedScript._window().open(url);
}

InjectedScript.getCallFrames = function()
{
    var callFrame = InspectorController.currentCallFrame();
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

InjectedScript.evaluateInCallFrame = function(callFrameId, code)
{
    var callFrame = InjectedScript._callFrameForId(callFrameId);
    if (!callFrame)
        return false;
    return InjectedScript._evaluateAndWrap(callFrame.evaluate, callFrame, code);
}

InjectedScript._callFrameForId = function(id)
{
    var callFrame = InspectorController.currentCallFrame();
    while (--id >= 0 && callFrame)
        callFrame = callFrame.caller;
    return callFrame;
}

InjectedScript._clearConsoleMessages = function()
{
    InspectorController.clearMessages(true);
}

InjectedScript._inspectObject = function(o)
{
    if (arguments.length === 0)
        return;

    var inspectedWindow = InjectedScript._window();
    inspectedWindow.console.log(o);
    if (Object.type(o) === "node") {
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

    inspectedWindow._inspectorCommandLineAPI.clear = InspectorController.wrapCallback(InjectedScript._clearConsoleMessages);
    inspectedWindow._inspectorCommandLineAPI.inspect = InspectorController.wrapCallback(InjectedScript._inspectObject);
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
    // There are three types of object ids used:
    // - numbers point to DOM Node via the InspectorDOMAgent mapping
    // - strings point to console objects cached in InspectorController for lazy evaluation upon them
    // - objects contain complex ids and are currently used for scoped objects
    if (typeof objectId === "number") {
        return InjectedScript._nodeForId(objectId);
    } else if (typeof objectId === "string") {
        return InspectorController.unwrapObject(objectId);
    } else if (typeof objectId === "object") {
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
    if (!object || Object.type(object) !== "node")
        return false;
    return InspectorController.pushNodePathToFrontend(object, false);
}

// Called from within InspectorController on the 'inspected page' side.
InjectedScript.createProxyObject = function(object, objectId, abbreviate)
{
    var result = {};
    result.objectId = objectId;
    result.type = Object.type(object);

    var type = typeof object;
    if ((type === "object" && object !== null) || type === "function") {
        for (var subPropertyName in object) {
            result.hasChildren = true;
            break;
        }
    }
    try {
        result.description = Object.describe(object, abbreviate);
    } catch (e) {
        result.errorText = e.toString();
    }
    return result;
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
        var foundLocalScope = false;
        var scopeChain = callFrame.scopeChain;
        var scopeChainProxy = [];
        for (var i = 0; i < scopeChain.length; ++i) {
            var scopeObject = scopeChain[i];
            var scopeObjectProxy = InjectedScript.createProxyObject(scopeObject, { callFrame: this.id, chainIndex: i }, true);

            if (Object.prototype.toString.call(scopeObject) === "[object JSActivation]") {
                if (!foundLocalScope)
                    scopeObjectProxy.thisObject = InjectedScript.createProxyObject(callFrame.thisObject, { callFrame: this.id, thisObject: true }, true);
                else
                    scopeObjectProxy.isClosure = true;
                foundLocalScope = true;
                scopeObjectProxy.isLocal = true;
            } else if (foundLocalScope && scopeObject instanceof InjectedScript._window().Element)
                scopeObjectProxy.isElement = true;
            else if (foundLocalScope && scopeObject instanceof InjectedScript._window().Document)
                scopeObjectProxy.isDocument = true;
            else if (!foundLocalScope)
                scopeObjectProxy.isWithBlock = true;
            scopeObjectProxy.properties = [];
            try {
                for (var propertyName in scopeObject)
                    scopeObjectProxy.properties.push(propertyName);
            } catch (e) {
            }
            scopeChainProxy.push(scopeObjectProxy);
        }
        return scopeChainProxy;
    }
}

Object.type = function(obj)
{
    if (obj === null)
        return "null";

    var type = typeof obj;
    if (type !== "object" && type !== "function")
        return type;

    var win = InjectedScript._window();

    if (obj instanceof win.Node)
        return (obj.nodeType === undefined ? type : "node");
    if (obj instanceof win.String)
        return "string";
    if (obj instanceof win.Array)
        return "array";
    if (obj instanceof win.Boolean)
        return "boolean";
    if (obj instanceof win.Number)
        return "number";
    if (obj instanceof win.Date)
        return "date";
    if (obj instanceof win.RegExp)
        return "regexp";
    if (obj instanceof win.Error)
        return "error";
    return type;
}

Object.hasProperties = function(obj)
{
    if (typeof obj === "undefined" || typeof obj === "null")
        return false;
    for (var name in obj)
        return true;
    return false;
}

Object.describe = function(obj, abbreviated)
{
    var type1 = Object.type(obj);
    var type2 = Object.className(obj);

    switch (type1) {
    case "object":
    case "node":
        return type2;
    case "array":
        return "[" + obj.toString() + "]";
    case "string":
        if (!abbreviated)
            return obj;
        if (obj.length > 100)
            return "\"" + obj.substring(0, 100) + "\u2026\"";
        return "\"" + obj + "\"";
    case "function":
        var objectText = String(obj);
        if (!/^function /.test(objectText))
            objectText = (type2 == "object") ? type1 : type2;
        else if (abbreviated)
            objectText = /.*/.exec(obj)[0].replace(/ +$/g, "");
        return objectText;
    case "regexp":
        return String(obj).replace(/([\\\/])/g, "\\$1").replace(/\\(\/[gim]*)$/, "$1").substring(1);
    case "boolean":
    case "number":
    case "null":
        return obj;
    default:
        return String(obj);
    }
}

Object.className = function(obj)
{
    return Object.prototype.toString.call(obj).replace(/^\[object (.*)\]$/i, "$1")
}

// Although Function.prototype.bind and String.prototype.escapeCharacters are defined in utilities.js they will soon become
// unavailable in the InjectedScript context. So we define them here for the local use.
// TODO: remove this comment once InjectedScript runs in a separate context.
Function.prototype.bind = function(thisObject)
{
    var func = this;
    var args = Array.prototype.slice.call(arguments, 1);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))) };
}

String.prototype.escapeCharacters = function(chars)
{
    var foundChar = false;
    for (var i = 0; i < chars.length; ++i) {
        if (this.indexOf(chars.charAt(i)) !== -1) {
            foundChar = true;
            break;
        }
    }

    if (!foundChar)
        return this;

    var result = "";
    for (var i = 0; i < this.length; ++i) {
        if (chars.indexOf(this.charAt(i)) !== -1)
            result += "\\";
        result += this.charAt(i);
    }

    return result;
}
