/*
 * Copyright (C) 2019 Igalia S.L.
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

function(element) {
    "use strict";

    if (["application/xml", "text/xml"].includes(document.contentType))
        return false;

    function isEnabled(element) {

        function isDisabledAttributeSupported(element) {
            switch (element.localName) {
            case "option":
            case "optgroup":
            case "button":
            case "input":
            case "select":
            case "textarea":
                return true;
            default:
                return false;
            }
        }

        if (!isDisabledAttributeSupported(element))
            return true;

        if (element.disabled)
            return false;

        function findParent(element, selector) {
            let node = element;
            // ElementNode = 1.
            while (node.parentNode && node.parentNode.nodeType == 1) {
                node = node.parentNode;
                if (node.matches(selector)) {
                    return node;
                }
            }
            return null;
        }

        switch (element.localName) {
        case "option":
        case "optgroup":
            let parent = findParent(element, "optgroup,select");
            if (parent != null)
                return isEnabled(parent);
        }

        let parent = findParent(element, "fieldset");
        if (parent === null)
            return true;

        return parent.children[0].localName == "legend" ? true : !parent.disabled;
    }

    return isEnabled(element);
}
