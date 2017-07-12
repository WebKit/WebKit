/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

function(element, attributeName) {
    "use strict";

    let lowercaseAttributeName = attributeName.toLowerCase();
    let tagName = element.tagName.toUpperCase();

    // Special Case: "selected" / "checked" work analogously
    // on options and checkboxes.
    if (lowercaseAttributeName === "selected" || lowercaseAttributeName === "checked") {
        switch (tagName) {
        case "OPTION":
            return element.selected ? "true" : null;
        case "INPUT":
            if (element.type === "checkbox" || element.type === "radio")
                return element.checked ? "true" : null;
            break;
        }
    }

    // Special Case: For images and links, require the existence of
    // the attribute before accessing the property.
    if ((tagName === "IMG" && lowercaseAttributeName === "src") || (tagName === "A" && lowercaseAttributeName === "href")) {
        let value = element.getAttribute(lowercaseAttributeName);
        return value ? element[lowercaseAttributeName] : value;
    }

    // Prefer the element's property over the attribute if the
    // property value is not null and not an object. For boolean
    // properties return null if false.
    try {
        const propertyNameAliases = {"class": "className", "readonly": "readOnly"};
        let propertyName = propertyNameAliases[attributeName] || attributeName;
        let value = element[propertyName];
        if (value != null && typeof value !== "object") {
            if (typeof value === "boolean")
                return value ? "true" : null;
            return value.toString();
        }
    } catch (e) { }

    return element.getAttribute(attributeName);
}
