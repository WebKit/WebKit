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

var InjectedScript = {
    _styles: {},
    _styleRules: {},
    _lastStyleId: 0,
    _lastStyleRuleId: 0
};

InjectedScript.getStyles = function(nodeId, authorOnly)
{
    var node = InjectedScript._nodeForId(nodeId);
    if (!node)
        return null;
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
            var longhandProperties = getLonghandProperties(style, propertyName);
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
    var uniqueProperties = getUniqueStyleProperties(tempStyle);
    for (var i = 0; i < uniqueProperties.length; ++i) {
        var name = uniqueProperties[i];
        var shorthand = tempStyle.getPropertyShorthand(name);

        if (shorthand && shorthand in foundShorthands)
            continue;

        if (shorthand) {
            var value = getShorthandValue(tempStyle, shorthand);
            var priority = getShorthandPriority(tempStyle, shorthand);
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
            var longhandProperties = getLonghandProperties(style, propertyName);
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

InjectedScript.applyStyleRuleText = function(ruleId, newContent, selectedNode)
{
    var rule = InjectedScript._styleRules[ruleId];
    if (!rule)
        return false;

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

        var nodes = selectedNode.ownerDocument.querySelectorAll(newContent);
        for (var i = 0; i < nodes.length; ++i) {
            if (nodes[i] === selectedNode) {
                return [InjectedScript._serializeRule(newRule), true];
            }
        }
        return [InjectedScript._serializeRule(newRule), false];
    } catch(e) {
        // Report invalid syntax.
        return false;
    }
}

InjectedScript.addStyleSelector = function(newContent)
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

    return InjectedScript._serializeRule(stylesheet.cssRules[stylesheet.cssRules.length - 1]);
}

InjectedScript.setStyleProperty = function(styleId, name, value) {
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

    // Bind editable scripts only.
    var doBind = !ruleValue.isUserAgent && !ruleValue.isUser;
    ruleValue.style = InjectedScript._serializeStyle(rule.style, doBind);

    if (doBind) {
        if (!rule._id) {
            rule._id = InjectedScript._lastStyleRuleId++;
            InjectedScript._styleRules[rule._id] = rule;
        }
        ruleValue.id = rule._id;
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
    for (var i = 0; i < style.length; ++i) {
        var property = {};
        var name = style[i];
        property.name = name;
        property.priority = style.getPropertyPriority(name);
        property.implicit = style.isPropertyImplicit(name);
        property.shorthand = style.getPropertyShorthand(name);
        property.value = style.getPropertyValue(name);
        result.properties.push(property);
    }

    if (doBind) {
        if (!style._id) {
            style._id = InjectedScript._lastStyleId++;
            InjectedScript._styles[style._id] = style;
        }
        result.id = style._id;
    }
    return result;
}

InjectedScript._window = function()
{
    return InspectorController.inspectedWindow();
}

InjectedScript._nodeForId = function(nodeId)
{
    return nodeId;
}
