/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.FormattedValue = {};

WebInspector.FormattedValue.classNameForTypes = function(type, subtype)
{
    return "formatted-" + (subtype ? subtype : type);
};

WebInspector.FormattedValue.classNameForObject = function(object)
{
    return WebInspector.FormattedValue.classNameForTypes(object.type, object.subtype);
};

WebInspector.FormattedValue.createLinkifiedElementString = function(string)
{
    var span = document.createElement("span");
    span.className = "formatted-string";
    span.appendChild(document.createTextNode("\""));
    span.appendChild(WebInspector.linkifyStringAsFragment(string.replace(/"/g, "\\\"")));
    span.appendChild(document.createTextNode("\""));
    return span;
};

WebInspector.FormattedValue.createElementForNode = function(object)
{
    var span = document.createElement("span");
    span.className = "formatted-node";

    object.pushNodeToFrontend(function(nodeId) {
        if (!nodeId) {
            span.textContent = object.description;
            return;
        }

        var treeOutline = new WebInspector.DOMTreeOutline;
        treeOutline.setVisible(true);
        treeOutline.rootDOMNode = WebInspector.domTreeManager.nodeForId(nodeId);
        if (!treeOutline.children[0].hasChildren)
            treeOutline.element.classList.add("single-node");
        span.appendChild(treeOutline.element);
    });

    return span;
};

WebInspector.FormattedValue.createElementForTypesAndValue = function(type, subtype, displayString, size, isPreview, hadException)
{
    var span = document.createElement("span");
    span.classList.add(WebInspector.FormattedValue.classNameForTypes(type, subtype));

    // Exception.
    if (hadException) {
        span.textContent = "[Exception: " + displayString + "]";
        return span;
    }

    // String: quoted and replace newlines as nice unicode symbols.
    if (type === "string") {
        span.textContent = doubleQuotedString(displayString.replace(/\n/g, "\u21B5"));
        return span;
    }

    // Function: if class, show the description, otherwise ellide in previews.
    if (type === "function") {
        if (subtype === "class")
            span.textContent = displayString;
        else
            span.textContent = isPreview ? "function" : displayString;
        return span;
    }

    // Everything else, the description/value string.
    span.textContent = displayString;

    // If there is a size, include it.
    if (size !== undefined && (subtype === "array" || subtype === "set" || subtype === "map" || subtype === "weakmap" || subtype === "weakset")) {
        var sizeElement = span.appendChild(document.createElement("span"));
        sizeElement.className = "size";
        sizeElement.textContent = " (" + size + ")";
    }

    return span;
};

WebInspector.FormattedValue.createElementForRemoteObject = function(object, hadException)
{
    return WebInspector.FormattedValue.createElementForTypesAndValue(object.type, object.subtype, object.description, object.size, false, hadException);
};

WebInspector.FormattedValue.createElementForObjectPreview = function(objectPreview)
{
    return WebInspector.FormattedValue.createElementForTypesAndValue(objectPreview.type, objectPreview.subtype, objectPreview.description, objectPreview.size, true, false);
};

WebInspector.FormattedValue.createElementForPropertyPreview = function(propertyPreview)
{
    return WebInspector.FormattedValue.createElementForTypesAndValue(propertyPreview.type, propertyPreview.subtype, propertyPreview.value, undefined, true, false);
};

WebInspector.FormattedValue.createObjectPreviewOrFormattedValueForRemoteObject = function(object, previewViewMode)
{
    if (object.subtype === "node")
        return WebInspector.FormattedValue.createElementForNode(object);

    if (object.preview)
        return new WebInspector.ObjectPreviewView(object.preview, previewViewMode);

    return WebInspector.FormattedValue.createElementForRemoteObject(object);
};

WebInspector.FormattedValue.createObjectTreeOrFormattedValueForRemoteObject = function(object, propertyPath, forceExpanding)
{
    if (object.subtype === "node")
        return WebInspector.FormattedValue.createElementForNode(object);

    if (object.subtype === "null")
        return WebInspector.FormattedValue.createElementForRemoteObject(object);

    if (object.type === "object" || object.subtype === "class") {
        var objectTree = new WebInspector.ObjectTreeView(object, null, propertyPath, forceExpanding);
        return objectTree.element;
    }

    return WebInspector.FormattedValue.createElementForRemoteObject(object);
};
