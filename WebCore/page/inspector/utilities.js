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

Object.describe = function(obj, abbreviated)
{
    if (obj === undefined)
        return "undefined";
    if (obj === null)
        return "null";

    var type1 = typeof obj;
    var type2 = "";
    if (type1 == "object" || type1 == "function") {
        if (obj instanceof String)
            type1 = "string";
        else if (obj instanceof Array)
            type1 = "array";
        else if (obj instanceof Boolean)
            type1 = "boolean";
        else if (obj instanceof Number)
            type1 = "number";
        else if (obj instanceof Date)
            type1 = "date";
        else if (obj instanceof RegExp)
            type1 = "regexp";
        else if (obj instanceof Error)
            type1 = "error";
        type2 = Object.prototype.toString.call(obj).replace(/^\[object (.*)\]$/i, "$1");
    }

    switch (type1) {
    case "object":
        return type2;
    case "array":
        return "[" + obj.toString() + "]";
    case "string":
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
    default:
        return String(obj);
    }
}

Object.sortedProperties = function(obj)
{
    var properties = [];
    for (var prop in obj) {
        properties.push(prop);
    }

    properties.sort();
    return properties;
}

Element.prototype.removeStyleClass = function(className) 
{
    if (this.hasStyleClass(className))
        this.className = this.className.replace(className, "");
}

Element.prototype.addStyleClass = function(className) 
{
    if (!this.hasStyleClass(className))
        this.className += (this.className.length ? " " + className : className);
}

Element.prototype.hasStyleClass = function(className) 
{
    return this.className.indexOf(className) !== -1;
}

Element.prototype.scrollToElement = function(element)
{
    if (!element || !this.isAncestor(element))
        return;

    var offsetTop = 0;
    var current = element
    while (current && current !== this) {
        offsetTop += current.offsetTop;
        current = current.offsetParent;
    }

    if (this.scrollTop > offsetTop)
        this.scrollTop = offsetTop;
    else if ((this.scrollTop + this.offsetHeight) < (offsetTop + element.offsetHeight))
        this.scrollTop = offsetTop - this.offsetHeight + element.offsetHeight;
}

Node.prototype.firstParentOrSelfWithNodeName = function(nodeName)
{
    for (var node = this; node && (node !== document); node = node.parentNode)
        if (node.nodeName.toLowerCase() === nodeName.toLowerCase())
            return node;

    return null;
}

Node.prototype.firstParentOrSelfWithClass = function(className) 
{
    for (var node = this; node && (node !== document); node = node.parentNode)
        if (node.nodeType === Node.ELEMENT_NODE && node.hasStyleClass(className))
            return node;

    return null;
}

Node.prototype.firstParentWithClass = function(className)
{
    if (!this.parentNode)
        return null;

    return this.parentNode.firstParentOrSelfWithClass(className);
}

Element.prototype.query = function(query) 
{
    return document.evaluate(query, this, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
}

Element.prototype.removeChildren = function()
{
    while (this.firstChild) 
        this.removeChild(this.firstChild);        
}

Element.prototype.firstChildSkippingWhitespace = firstChildSkippingWhitespace;
Element.prototype.lastChildSkippingWhitespace = lastChildSkippingWhitespace;

Node.prototype.isWhitespace = isNodeWhitespace;
Node.prototype.nodeTypeName = nodeTypeName;
Node.prototype.displayName = nodeDisplayName;
Node.prototype.contentPreview = nodeContentPreview;
Node.prototype.isAncestor = isAncestorNode;
Node.prototype.isDescendant = isDescendantNode;
Node.prototype.firstCommonAncestor = firstCommonNodeAncestor;
Node.prototype.nextSiblingSkippingWhitespace = nextSiblingSkippingWhitespace;
Node.prototype.previousSiblingSkippingWhitespace = previousSiblingSkippingWhitespace;
Node.prototype.traverseNextNode = traverseNextNode;
Node.prototype.traversePreviousNode = traversePreviousNode;
Node.prototype.onlyTextChild = onlyTextChild;
Node.prototype.titleInfo = nodeTitleInfo;

String.prototype.hasSubstring = function(string, caseInsensitive)
{
    if (!caseInsensitive)
        return this.indexOf(string) !== -1;
    return this.match(new RegExp(string.escapeForRegExp(), "i"));
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

String.prototype.escapeForRegExp = function()
{
    return this.escapeCharacters("^[]{}()\\.$*+?|");
}

String.prototype.escapeHTML = function()
{
    return this.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

String.prototype.collapseWhitespace = function()
{
    return this.replace(/[\s\xA0]+/g, " ");
}

String.prototype.trimLeadingWhitespace = function()
{
    return this.replace(/^[\s\xA0]+/g, "");
}

String.prototype.trimTrailingWhitespace = function()
{
    return this.replace(/[\s\xA0]+$/g, "");
}

String.prototype.trimWhitespace = function()
{
    return this.replace(/^[\s\xA0]+|[\s\xA0]+$/g, "");
}

String.prototype.trimURL = function(baseURLDomain)
{
    var result = this.replace(new RegExp("^http[s]?:\/\/", "i"), "");
    if (baseURLDomain)
        result = result.replace(new RegExp("^" + baseURLDomain.escapeForRegExp(), "i"), "");
    return result;
}

function isNodeWhitespace()
{
    if (!this || this.nodeType !== Node.TEXT_NODE)
        return false;
    if (!this.nodeValue.length)
        return true;
    return this.nodeValue.match(/^[\s\xA0]+$/);
}

function nodeTypeName()
{
    if (!this)
        return "(unknown)";

    switch (this.nodeType) {
        case Node.ELEMENT_NODE: return "Element";
        case Node.ATTRIBUTE_NODE: return "Attribute";
        case Node.TEXT_NODE: return "Text";
        case Node.CDATA_SECTION_NODE: return "Character Data";
        case Node.ENTITY_REFERENCE_NODE: return "Entity Reference";
        case Node.ENTITY_NODE: return "Entity";
        case Node.PROCESSING_INSTRUCTION_NODE: return "Processing Instruction";
        case Node.COMMENT_NODE: return "Comment";
        case Node.DOCUMENT_NODE: return "Document";
        case Node.DOCUMENT_TYPE_NODE: return "Document Type";
        case Node.DOCUMENT_FRAGMENT_NODE: return "Document Fragment";
        case Node.NOTATION_NODE: return "Notation";
    }

    return "(unknown)";
}

function nodeDisplayName()
{
    if (!this)
        return "";

    switch (this.nodeType) {
        case Node.DOCUMENT_NODE:
            return "Document";

        case Node.ELEMENT_NODE:
            var name = "<" + this.nodeName.toLowerCase();

            if (this.hasAttributes()) {
                var value = this.getAttribute("id");
                if (value)
                    name += " id=\"" + value + "\"";
                value = this.getAttribute("class");
                if (value)
                    name += " class=\"" + value + "\"";
                if (this.nodeName.toLowerCase() === "a") {
                    value = this.getAttribute("name");
                    if (value)
                        name += " name=\"" + value + "\"";
                    value = this.getAttribute("href");
                    if (value)
                        name += " href=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "img") {
                    value = this.getAttribute("src");
                    if (value)
                        name += " src=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "iframe") {
                    value = this.getAttribute("src");
                    if (value)
                        name += " src=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "input") {
                    value = this.getAttribute("name");
                    if (value)
                        name += " name=\"" + value + "\"";
                    value = this.getAttribute("type");
                    if (value)
                        name += " type=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "form") {
                    value = this.getAttribute("action");
                    if (value)
                        name += " action=\"" + value + "\"";
                }
            }

            return name + ">";

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                return "(whitespace)";
            return "\"" + this.nodeValue + "\"";

        case Node.COMMENT_NODE:
            return "<!--" + this.nodeValue + "-->";
    }

    return this.nodeName.toLowerCase().collapseWhitespace();
}

function nodeContentPreview()
{
    if (!this || !this.hasChildNodes || !this.hasChildNodes())
        return "";

    var limit = 0;
    var preview = "";

    // always skip whitespace here
    var currentNode = traverseNextNode.call(this, true, this);
    while (currentNode) {
        if (currentNode.nodeType === Node.TEXT_NODE)
            preview += currentNode.nodeValue.escapeHTML();
        else
            preview += nodeDisplayName.call(currentNode).escapeHTML();

        currentNode = traverseNextNode.call(currentNode, true, this);

        if (++limit > 4) {
            preview += "&#x2026;"; // ellipsis
            break;
        }
    }

    return preview.collapseWhitespace();
}

function isAncestorNode(ancestor)
{
    if (!this || !ancestor)
        return false;

    var currentNode = ancestor.parentNode;
    while (currentNode) {
        if (this === currentNode)
            return true;
        currentNode = currentNode.parentNode;
    }

    return false;
}

function isDescendantNode(descendant)
{
    return isAncestorNode.call(descendant, this);
}

function firstCommonNodeAncestor(node)
{
    if (!this || !node)
        return;

    var node1 = this.parentNode;
    var node2 = node.parentNode;

    if ((!node1 || !node2) || node1 !== node2)
        return null;

    while (node1 && node2) {
        if (!node1.parentNode || !node2.parentNode)
            break;
        if (node1 !== node2)
            break;

        node1 = node1.parentNode;
        node2 = node2.parentNode;
    }

    return node1;
}

function nextSiblingSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.nextSibling;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = node.nextSibling;
    return node;
}

function previousSiblingSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.previousSibling;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = node.previousSibling;
    return node;
}

function firstChildSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.firstChild;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = nextSiblingSkippingWhitespace.call(node);
    return node;
}

function lastChildSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.lastChild;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = previousSiblingSkippingWhitespace.call(node);
    return node;
}

function traverseNextNode(skipWhitespace, stayWithin)
{
    if (!this)
        return;

    var node = skipWhitespace ? firstChildSkippingWhitespace.call(this) : this.firstChild;
    if (node)
        return node;

    if (stayWithin && this === stayWithin)
        return null;

    node = skipWhitespace ? nextSiblingSkippingWhitespace.call(this) : this.nextSibling;
    if (node)
        return node;

    node = this;
    while (node && !(skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling) && (!stayWithin || !node.parentNode || node.parentNode !== stayWithin))
        node = node.parentNode;
    if (!node)
        return null;

    return skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
}

function traversePreviousNode(skipWhitespace)
{
    if (!this)
        return;
    var node = skipWhitespace ? previousSiblingSkippingWhitespace.call(this) : this.previousSibling;
    while (node && (skipWhitespace ? lastChildSkippingWhitespace.call(node) : node.lastChild) )
        node = skipWhitespace ? lastChildSkippingWhitespace.call(node) : node.lastChild;
    if (node)
        return node;
    return this.parentNode;
}

function onlyTextChild(ignoreWhitespace)
{
    if (!this)
        return null;

    var firstChild = ignoreWhitespace ? firstChildSkippingWhitespace.call(this) : this.firstChild;
    if (!firstChild || firstChild.nodeType !== Node.TEXT_NODE)
        return null;

    var sibling = ignoreWhitespace ? nextSiblingSkippingWhitespace.call(firstChild) : firstChild.nextSibling;
    return sibling ? null : firstChild;
}

function nodeTitleInfo(hasChildren, linkify)
{
    var info = {title: '', hasChildren: hasChildren};

    switch (this.nodeType) {
        case Node.DOCUMENT_NODE:
            info.title = "Document";
            break;

        case Node.ELEMENT_NODE:
            info.title = "<span class=\"webkit-html-tag\">&lt;" + this.nodeName.toLowerCase().escapeHTML();

            if (this.hasAttributes()) {
                for (var i = 0; i < this.attributes.length; ++i) {
                    var attr = this.attributes[i];
                    var value = attr.value.escapeHTML();
                    value = value.replace(/([\/;:\)\]\}])/g, "$1&#8203;");

                    info.title += " <span class=\"webkit-html-attribute-name\">" + attr.name.escapeHTML() + "</span>=&#8203;";

                    if (linkify && (attr.name === "src" || attr.name === "href"))
                        info.title += linkify(attr.value, value, "webkit-html-attribute-value", this.nodeName.toLowerCase() == "a");
                    else
                        info.title += "<span class=\"webkit-html-attribute-value\">\"" + value + "\"</span>";
                }
            }
            info.title += "&gt;</span>&#8203;";

            // If this element only has a single child that is a text node,
            // just show that text and the closing tag inline rather than
            // create a subtree for them

            var textChild = onlyTextChild.call(this, Preferences.ignoreWhitespace);
            var showInlineText = textChild && textChild.textContent.length < Preferences.maxInlineTextChildLength;

            if (showInlineText) {
                info.title += textChild.nodeValue.escapeHTML() + "&#8203;<span class=\"webkit-html-tag\">&lt;/" + this.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
                info.hasChildren = false;
            }
            break;

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                info.title = "(whitespace)";
            else
                info.title = "\"" + this.nodeValue.escapeHTML() + "\"";
            break

        case Node.COMMENT_NODE:
            info.title = "<span class=\"webkit-html-comment\">&lt;!--" + this.nodeValue.escapeHTML() + "--&gt;</span>";
            break;

        default:
            info.title = this.nodeName.toLowerCase().collapseWhitespace().escapeHTML();
    }

    return info;
}

Number.secondsToString = function(seconds)
{
    var ms = seconds * 1000;
    if (ms < 1000)
        return Math.round(ms) + "ms";

    if (seconds < 60)
        return (Math.round(seconds * 100) / 100) + "s";

    var minutes = seconds / 60;
    if (minutes < 60)
        return (Math.round(minutes * 10) / 10) + "min";

    var hours = minutes / 60;
    if (hours < 24)
        return (Math.round(hours * 10) / 10) + "hrs";

    var days = hours / 24;
    return (Math.round(days * 10) / 10) + " days";
}

Number.bytesToString = function(bytes)
{
    if (bytes < 1024)
        return bytes + "B";

    var kilobytes = bytes / 1024;
    if (kilobytes < 1024)
        return (Math.round(kilobytes * 100) / 100) + "KB";

    var megabytes = kilobytes / 1024;
    return (Math.round(megabytes * 1000) / 1000) + "MB";
}

HTMLTextAreaElement.prototype.moveCursorToEnd = function()
{
    var length = this.value.length;
    this.setSelectionRange(length, length);
}

// This code is in the public domain. Feel free to link back to http://jan.moesen.nu/
String.sprintf = function()
{
        if (!arguments || arguments.length < 1 || !RegExp)
        {
                return;
        }
        var str = arguments[0];
        var re = /([^%]*)%('.|0|\x20)?(-)?(\d+)?(\.\d+)?(%|b|c|d|u|f|o|s|x|X)(.*)/;
        var a = b = [], numSubstitutions = 0, numMatches = 0;
        while (a = re.exec(str))
        {
                var leftpart = a[1], pPad = a[2], pJustify = a[3], pMinLength = a[4];
                var pPrecision = a[5], pType = a[6], rightPart = a[7];

                //alert(a + '\n' + [a[0], leftpart, pPad, pJustify, pMinLength, pPrecision);

                numMatches++;
                if (pType == '%')
                {
                        subst = '%';
                }
                else
                {
                        numSubstitutions++;
                        if (numSubstitutions >= arguments.length)
                        {
                                alert('Error! Not enough function arguments (' + (arguments.length - 1) + ', excluding the string)\nfor the number of substitution parameters in string (' + numSubstitutions + ' so far).');
                        }
                        var param = arguments[numSubstitutions];
                        var pad = '';
                               if (pPad && pPad.substr(0,1) == "'") pad = leftpart.substr(1,1);
                          else if (pPad) pad = pPad;
                        var justifyRight = true;
                               if (pJustify && pJustify === "-") justifyRight = false;
                        var minLength = -1;
                               if (pMinLength) minLength = parseInt(pMinLength);
                        var precision = -1;
                               if (pPrecision && pType == 'f') precision = parseInt(pPrecision.substring(1));
                        var subst = param;
                               if (pType == 'b') subst = parseInt(param).toString(2);
                          else if (pType == 'c') subst = String.fromCharCode(parseInt(param));
                          else if (pType == 'd') subst = parseInt(param) ? parseInt(param) : 0;
                          else if (pType == 'u') subst = Math.abs(param);
                          else if (pType == 'f') subst = (precision > -1) ? Math.round(parseFloat(param) * Math.pow(10, precision)) / Math.pow(10, precision): parseFloat(param);
                          else if (pType == 'o') subst = parseInt(param).toString(8);
                          else if (pType == 's') subst = param;
                          else if (pType == 'x') subst = ('' + parseInt(param).toString(16)).toLowerCase();
                          else if (pType == 'X') subst = ('' + parseInt(param).toString(16)).toUpperCase();
                }
                str = leftpart + subst + rightPart;
        }
        return str;
}
