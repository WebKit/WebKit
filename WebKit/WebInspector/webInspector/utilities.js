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

Node.prototype.firstParentWithClass = function(className) 
{
    var node = this.parentNode;
    while (node) {
        if (node === document) 
            return null;
        if (node.nodeType === Node.ELEMENT_NODE && node.hasStyleClass(className))
            return node;
        node = node.parentNode;
    }

    return null;
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

String.prototype.trimWhitespace = function()
{
    return this.replace(/^[\s\xA0]+|[\s\xA0]+$/g, "");
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
