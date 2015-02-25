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
}

WebInspector.FormattedValue.classNameForObject = function(object)
{
    return WebInspector.FormattedValue.classNameForTypes(object.type, object.subtype);
}

WebInspector.FormattedValue.createLinkifiedElementString = function(string)
{
    var span = document.createElement("span");
    span.className = "formatted-string";
    span.appendChild(document.createTextNode("\""));
    span.appendChild(WebInspector.linkifyStringAsFragment(string.replace(/"/g, "\\\"")));
    span.appendChild(document.createTextNode("\""));
    return span;
}

WebInspector.FormattedValue.createElementForTypesAndValue = function(type, subtype, displayString, isPreview, hadException)
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
        span.textContent = "\"" + displayString.replace(/\n/g, "\u21B5").replace(/"/g, "\\\"") + "\"";
        return span;
    }

    // Function: ellided in previews.
    if (type === "function") {
        span.textContent = isPreview ? "function" : displayString;
        return span;
    }

    // Everything else, the description/value string.
    span.textContent = displayString;
    return span;
}

WebInspector.FormattedValue.createElementForRemoteObject = function(object, hadException)
{
    return WebInspector.FormattedValue.createElementForTypesAndValue(object.type, object.subtype, object.description, false, hadException);
}

WebInspector.FormattedValue.createElementForObjectPreview = function(objectPreview)
{
    return WebInspector.FormattedValue.createElementForTypesAndValue(objectPreview.type, objectPreview.subtype, objectPreview.description, true, false);
}

WebInspector.FormattedValue.createElementForPropertyPreview = function(propertyPreview)
{
    return WebInspector.FormattedValue.createElementForTypesAndValue(propertyPreview.type, propertyPreview.subtype, propertyPreview.value, true, false);
}
