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
    span.append("\"", WebInspector.linkifyStringAsFragment(string.replace(/\\/g, "\\\\").replace(/"/g, "\\\"")), "\"");
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

WebInspector.FormattedValue.createElementForError = function(object)
{
    var span = document.createElement("span");
    span.classList.add("formatted-error");
    span.textContent = object.description;

    if (!object.preview)
        return span;

    function previewToObject(preview)
    {
        var result = {};
        for (var property of preview.propertyPreviews)
            result[property.name] = property.value;

        return result;
    }

    var preview = previewToObject(object.preview);
    if (!preview.sourceURL)
        return span;

    var sourceLinkWithPrefix = WebInspector.ErrorObjectView.makeSourceLinkWithPrefix(preview.sourceURL, preview.line, preview.column);
    span.append(sourceLinkWithPrefix);
    return span;
};

WebInspector.FormattedValue.createElementForNodePreview = function(preview)
{
    var value = preview.value || preview.description;
    var span = document.createElement("span");
    span.className = "formatted-node-preview syntax-highlighted";

    // Comment node preview.
    if (value.startsWith("<!--")) {
        var comment = span.appendChild(document.createElement("span"));
        comment.className = "html-comment";
        comment.textContent = value;
        return span;
    }

    // Doctype node preview.
    if (value.startsWith("<!DOCTYPE")) {
        var doctype = span.appendChild(document.createElement("span"));
        doctype.className = "html-doctype";
        doctype.textContent = value;
        return span;
    }

    // Element node previews have a very strict format, with at most a single attribute.
    // We can style it up like a DOMNode without interactivity.
    var matches = value.match(/^<(\S+?)(?: (\S+?)="(.*?)")?>$/);

    // Remaining node types are often #text, #document, etc, with attribute nodes potentially being any string.
    if (!matches) {
        console.assert(!value.startsWith("<"), "Unexpected node preview format: " + value);
        span.textContent = value;
        return span;
    }

    var tag = document.createElement("span");
    tag.className = "html-tag";
    tag.append("<");

    var tagName = tag.appendChild(document.createElement("span"));
    tagName.className = "html-tag-name";
    tagName.textContent = matches[1];

    if (matches[2]) {
        tag.append(" ");
        var attribute = tag.appendChild(document.createElement("span"));
        attribute.className = "html-attribute";
        var attributeName = attribute.appendChild(document.createElement("span"));
        attributeName.className = "html-attribute-name";
        attributeName.textContent = matches[2];
        attribute.append("=\"");
        var attributeValue = attribute.appendChild(document.createElement("span"));
        attributeValue.className = "html-attribute-value";
        attributeValue.textContent = matches[3];
        attribute.append("\"");
    }

    tag.append(">");
    span.appendChild(tag);

    return span;
};

WebInspector.FormattedValue.createElementForFunctionWithName = function(description)
{
    var span = document.createElement("span");
    span.classList.add("formatted-function");
    span.textContent = description.substring(0, description.indexOf("("));
    return span;
}

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
        displayString = displayString.truncate(WebInspector.FormattedValue.MAX_PREVIEW_STRING_LENGTH);
        span.textContent = doubleQuotedString(displayString.replace(/\n/g, "\u21B5"));
        return span;
    }

    // Function: if class, show the description, otherwise elide in previews.
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

WebInspector.FormattedValue.createObjectPreviewOrFormattedValueForObjectPreview = function(objectPreview, previewViewMode)
{
    if (objectPreview.subtype === "node")
        return WebInspector.FormattedValue.createElementForNodePreview(objectPreview);

    if (objectPreview.type === "function")
        return WebInspector.FormattedValue.createElementForFunctionWithName(objectPreview.description);

    return new WebInspector.ObjectPreviewView(objectPreview, previewViewMode).element;
}

WebInspector.FormattedValue.createObjectPreviewOrFormattedValueForRemoteObject = function(object, previewViewMode)
{
    if (object.subtype === "node")
        return WebInspector.FormattedValue.createElementForNode(object);

    if (object.subtype === "error")
        return WebInspector.FormattedValue.createElementForError(object);

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

WebInspector.FormattedValue.MAX_PREVIEW_STRING_LENGTH = 140;
