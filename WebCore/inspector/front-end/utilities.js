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

Object.type = function(obj, win)
{
    if (obj === null)
        return "null";

    var type = typeof obj;
    if (type !== "object" && type !== "function")
        return type;

    win = win || window;

    if (obj instanceof win.Node)
        return "node";
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
    var type2 = Object.prototype.toString.call(obj).replace(/^\[object (.*)\]$/i, "$1");

    switch (type1) {
    case "object":
    case "node":
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
    for (var prop in obj)
        properties.push(prop);
    properties.sort();
    return properties;
}

Function.prototype.bind = function(thisObject)
{
    var func = this;
    var args = Array.prototype.slice.call(arguments, 1);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))) };
}

Node.prototype.rangeOfWord = function(offset, stopCharacters, stayWithinNode, direction)
{
    var startNode;
    var startOffset = 0;
    var endNode;
    var endOffset = 0;

    if (!stayWithinNode)
        stayWithinNode = this;

    if (!direction || direction === "backward" || direction === "both") {
        var node = this;
        while (node) {
            if (node === stayWithinNode) {
                if (!startNode)
                    startNode = stayWithinNode;
                break;
            }

            if (node.nodeType === Node.TEXT_NODE) {
                var start = (node === this ? (offset - 1) : (node.nodeValue.length - 1));
                for (var i = start; i >= 0; --i) {
                    if (stopCharacters.indexOf(node.nodeValue[i]) !== -1) {
                        startNode = node;
                        startOffset = i + 1;
                        break;
                    }
                }
            }

            if (startNode)
                break;

            node = node.traversePreviousNode(false, stayWithinNode);
        }

        if (!startNode) {
            startNode = stayWithinNode;
            startOffset = 0;
        }
    } else {
        startNode = this;
        startOffset = offset;
    }

    if (!direction || direction === "forward" || direction === "both") {
        node = this;
        while (node) {
            if (node === stayWithinNode) {
                if (!endNode)
                    endNode = stayWithinNode;
                break;
            }

            if (node.nodeType === Node.TEXT_NODE) {
                var start = (node === this ? offset : 0);
                for (var i = start; i < node.nodeValue.length; ++i) {
                    if (stopCharacters.indexOf(node.nodeValue[i]) !== -1) {
                        endNode = node;
                        endOffset = i;
                        break;
                    }
                }
            }

            if (endNode)
                break;

            node = node.traverseNextNode(false, stayWithinNode);
        }

        if (!endNode) {
            endNode = stayWithinNode;
            endOffset = stayWithinNode.nodeType === Node.TEXT_NODE ? stayWithinNode.nodeValue.length : stayWithinNode.childNodes.length;
        }
    } else {
        endNode = this;
        endOffset = offset;
    }

    var result = this.ownerDocument.createRange();
    result.setStart(startNode, startOffset);
    result.setEnd(endNode, endOffset);

    return result;
}

Element.prototype.removeStyleClass = function(className) 
{
    // Test for the simple case before using a RegExp.
    if (this.className === className) {
        this.className = "";
        return;
    }

    this.removeMatchingStyleClasses(className.escapeForRegExp());
}

Element.prototype.removeMatchingStyleClasses = function(classNameRegex)
{
    var regex = new RegExp("(^|\\s+)" + classNameRegex + "($|\\s+)");
    if (regex.test(this.className))
        this.className = this.className.replace(regex, " ");
}

Element.prototype.addStyleClass = function(className) 
{
    if (className && !this.hasStyleClass(className))
        this.className += (this.className.length ? " " + className : className);
}

Element.prototype.hasStyleClass = function(className) 
{
    if (!className)
        return false;
    // Test for the simple case before using a RegExp.
    if (this.className === className)
        return true;
    var regex = new RegExp("(^|\\s)" + className.escapeForRegExp() + "($|\\s)");
    return regex.test(this.className);
}

Node.prototype.enclosingNodeOrSelfWithNodeNameInArray = function(nameArray)
{
    for (var node = this; node && !objectsAreSame(node, this.ownerDocument); node = node.parentNode)
        for (var i = 0; i < nameArray.length; ++i)
            if (node.nodeName.toLowerCase() === nameArray[i].toLowerCase())
                return node;
    return null;
}

Node.prototype.enclosingNodeOrSelfWithNodeName = function(nodeName)
{
    return this.enclosingNodeOrSelfWithNodeNameInArray([nodeName]);
}

Node.prototype.enclosingNodeOrSelfWithClass = function(className)
{
    for (var node = this; node && !objectsAreSame(node, this.ownerDocument); node = node.parentNode)
        if (node.nodeType === Node.ELEMENT_NODE && node.hasStyleClass(className))
            return node;
    return null;
}

Node.prototype.enclosingNodeWithClass = function(className)
{
    if (!this.parentNode)
        return null;
    return this.parentNode.enclosingNodeOrSelfWithClass(className);
}

Element.prototype.query = function(query) 
{
    return this.ownerDocument.evaluate(query, this, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
}

Element.prototype.removeChildren = function()
{
    while (this.firstChild) 
        this.removeChild(this.firstChild);        
}

Element.prototype.isInsertionCaretInside = function()
{
    var selection = window.getSelection();
    if (!selection.rangeCount || !selection.isCollapsed)
        return false;
    var selectionRange = selection.getRangeAt(0);
    return selectionRange.startContainer === this || selectionRange.startContainer.isDescendant(this);
}

Element.prototype.__defineGetter__("totalOffsetLeft", function()
{
    var total = 0;
    for (var element = this; element; element = element.offsetParent)
        total += element.offsetLeft;
    return total;
});

Element.prototype.__defineGetter__("totalOffsetTop", function()
{
    var total = 0;
    for (var element = this; element; element = element.offsetParent)
        total += element.offsetTop;
    return total;
});

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

function getStyleTextWithShorthands(style)
{
    var cssText = "";
    var foundProperties = {};
    for (var i = 0; i < style.length; ++i) {
        var individualProperty = style[i];
        var shorthandProperty = style.getPropertyShorthand(individualProperty);
        var propertyName = (shorthandProperty || individualProperty);

        if (propertyName in foundProperties)
            continue;

        if (shorthandProperty) {
            var value = getShorthandValue(style, shorthandProperty);
            var priority = getShorthandPriority(style, shorthandProperty);
        } else {
            var value = style.getPropertyValue(individualProperty);
            var priority = style.getPropertyPriority(individualProperty);
        }

        foundProperties[propertyName] = true;

        cssText += propertyName + ": " + value;
        if (priority)
            cssText += " !" + priority;
        cssText += "; ";
    }

    return cssText;
}

function getShorthandValue(style, shorthandProperty)
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

function getShorthandPriority(style, shorthandProperty)
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

function getLonghandProperties(style, shorthandProperty)
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

function getUniqueStyleProperties(style)
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
            
        case Node.DOCUMENT_TYPE_NODE:
            var docType = "<!DOCTYPE " + this.nodeName;
            if (this.publicId) {
                docType += " PUBLIC \"" + this.publicId + "\"";
                if (this.systemId)
                    docType += " \"" + this.systemId + "\"";
            } else if (this.systemId)
                docType += " SYSTEM \"" + this.systemId + "\"";
            if (this.internalSubset)
                docType += " [" + this.internalSubset + "]";
            return docType + ">";
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

function objectsAreSame(a, b)
{
    // FIXME: Make this more generic so is works with any wrapped object, not just nodes.
    // This function is used to compare nodes that might be JSInspectedObjectWrappers, since
    // JavaScript equality is not true for JSInspectedObjectWrappers of the same node wrapped
    // with different global ExecStates, we use isSameNode to compare them.
    if (a === b)
        return true;
    if (!a || !b)
        return false;
    if (a.isSameNode && b.isSameNode)
        return a.isSameNode(b);
    return false;
}

function isAncestorNode(ancestor)
{
    if (!this || !ancestor)
        return false;

    var currentNode = ancestor.parentNode;
    while (currentNode) {
        if (objectsAreSame(this, currentNode))
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

    if ((!node1 || !node2) || !objectsAreSame(node1, node2))
        return null;

    while (node1 && node2) {
        if (!node1.parentNode || !node2.parentNode)
            break;
        if (!objectsAreSame(node1, node2))
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

    if (stayWithin && objectsAreSame(this, stayWithin))
        return null;

    node = skipWhitespace ? nextSiblingSkippingWhitespace.call(this) : this.nextSibling;
    if (node)
        return node;

    node = this;
    while (node && !(skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling) && (!stayWithin || !node.parentNode || !objectsAreSame(node.parentNode, stayWithin)))
        node = node.parentNode;
    if (!node)
        return null;

    return skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
}

function traversePreviousNode(skipWhitespace, stayWithin)
{
    if (!this)
        return;
    if (stayWithin && objectsAreSame(this, stayWithin))
        return null;
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
    var info = {title: "", hasChildren: hasChildren};

    switch (this.nodeType) {
        case Node.DOCUMENT_NODE:
            info.title = "Document";
            break;

        case Node.ELEMENT_NODE:
            info.title = "<span class=\"webkit-html-tag\">&lt;" + this.nodeName.toLowerCase().escapeHTML();

            if (this.hasAttributes()) {
                for (var i = 0; i < this.attributes.length; ++i) {
                    var attr = this.attributes[i];
                    info.title += " <span class=\"webkit-html-attribute\"><span class=\"webkit-html-attribute-name\">" + attr.name.escapeHTML() + "</span>=&#8203;\"";

                    var value = attr.value;
                    if (linkify && (attr.name === "src" || attr.name === "href")) {
                        var value = value.replace(/([\/;:\)\]\}])/g, "$1\u200B");
                        info.title += linkify(attr.value, value, "webkit-html-attribute-value", this.nodeName.toLowerCase() == "a");
                    } else {
                        var value = value.escapeHTML();
                        value = value.replace(/([\/;:\)\]\}])/g, "$1&#8203;");
                        info.title += "<span class=\"webkit-html-attribute-value\">" + value + "</span>";
                    }
                    info.title += "\"</span>";
                }
            }
            info.title += "&gt;</span>&#8203;";

            // If this element only has a single child that is a text node,
            // just show that text and the closing tag inline rather than
            // create a subtree for them

            var textChild = onlyTextChild.call(this, Preferences.ignoreWhitespace);
            var showInlineText = textChild && textChild.textContent.length < Preferences.maxInlineTextChildLength;

            if (showInlineText) {
                info.title += "<span class=\"webkit-html-text-node\">" + textChild.nodeValue.escapeHTML() + "</span>&#8203;<span class=\"webkit-html-tag\">&lt;/" + this.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
                info.hasChildren = false;
            }
            break;

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                info.title = "(whitespace)";
            else
                info.title = "\"<span class=\"webkit-html-text-node\">" + this.nodeValue.escapeHTML() + "</span>\"";
            break

        case Node.COMMENT_NODE:
            info.title = "<span class=\"webkit-html-comment\">&lt;!--" + this.nodeValue.escapeHTML() + "--&gt;</span>";
            break;

        case Node.DOCUMENT_TYPE_NODE:
            info.title = "<span class=\"webkit-html-doctype\">&lt;!DOCTYPE " + this.nodeName;
            if (this.publicId) {
                info.title += " PUBLIC \"" + this.publicId + "\"";
                if (this.systemId)
                    info.title += " \"" + this.systemId + "\"";
            } else if (this.systemId)
                info.title += " SYSTEM \"" + this.systemId + "\"";
            if (this.internalSubset)
                info.title += " [" + this.internalSubset + "]";
            info.title += "&gt;</span>";
            break;
        default:
            info.title = this.nodeName.toLowerCase().collapseWhitespace().escapeHTML();
    }

    return info;
}

function getDocumentForNode(node) {
    return node.nodeType == Node.DOCUMENT_NODE ? node : node.ownerDocument;
}

function parentNodeOrFrameElement(node) {
    var parent = node.parentNode;
    if (parent)
        return parent;

    return getDocumentForNode(node).defaultView.frameElement;
}

function isAncestorIncludingParentFrames(a, b) {
    if (objectsAreSame(a, b))
        return false;
    for (var node = b; node; node = getDocumentForNode(node).defaultView.frameElement)
        if (objectsAreSame(a, node) || isAncestorNode.call(a, node))
            return true;
    return false;
}

Number.secondsToString = function(seconds, formatterFunction, higherResolution)
{
    if (!formatterFunction)
        formatterFunction = String.sprintf;

    var ms = seconds * 1000;
    if (higherResolution && ms < 1000)
        return formatterFunction("%.3fms", ms);
    else if (ms < 1000)
        return formatterFunction("%.0fms", ms);

    if (seconds < 60)
        return formatterFunction("%.2fs", seconds);

    var minutes = seconds / 60;
    if (minutes < 60)
        return formatterFunction("%.1fmin", minutes);

    var hours = minutes / 60;
    if (hours < 24)
        return formatterFunction("%.1fhrs", hours);

    var days = hours / 24;
    return formatterFunction("%.1f days", days);
}

Number.bytesToString = function(bytes, formatterFunction)
{
    if (!formatterFunction)
        formatterFunction = String.sprintf;

    if (bytes < 1024)
        return formatterFunction("%.0fB", bytes);

    var kilobytes = bytes / 1024;
    if (kilobytes < 1024)
        return formatterFunction("%.2fKB", kilobytes);

    var megabytes = kilobytes / 1024;
    return formatterFunction("%.3fMB", megabytes);
}

Number.constrain = function(num, min, max)
{
    if (num < min)
        num = min;
    else if (num > max)
        num = max;
    return num;
}

HTMLTextAreaElement.prototype.moveCursorToEnd = function()
{
    var length = this.value.length;
    this.setSelectionRange(length, length);
}

Array.prototype.remove = function(value, onlyFirst)
{
    if (onlyFirst) {
        var index = this.indexOf(value);
        if (index !== -1)
            this.splice(index, 1);
        return;
    }

    var length = this.length;
    for (var i = 0; i < length; ++i) {
        if (this[i] === value)
            this.splice(i, 1);
    }
}

function insertionIndexForObjectInListSortedByFunction(anObject, aList, aFunction)
{
    // indexOf returns (-lowerBound - 1). Taking (-result - 1) works out to lowerBound.
    return (-indexOfObjectInListSortedByFunction(anObject, aList, aFunction) - 1);
}

function indexOfObjectInListSortedByFunction(anObject, aList, aFunction)
{
    var first = 0;
    var last = aList.length - 1;
    var floor = Math.floor;
    var mid, c;

    while (first <= last) {
        mid = floor((first + last) / 2);
        c = aFunction(anObject, aList[mid]);

        if (c > 0)
            first = mid + 1;
        else if (c < 0)
            last = mid - 1;
        else {
            // Return the first occurance of an item in the list.
            while (mid > 0 && aFunction(anObject, aList[mid - 1]) === 0)
                mid--;
            first = mid;
            break;
        }
    }

    // By returning 1 less than the negative lower search bound, we can reuse this function
    // for both indexOf and insertionIndexFor, with some simple arithmetic.
    return (-first - 1);
}

String.sprintf = function(format)
{
    return String.vsprintf(format, Array.prototype.slice.call(arguments, 1));
}

String.tokenizeFormatString = function(format)
{
    var tokens = [];
    var substitutionIndex = 0;

    function addStringToken(str)
    {
        tokens.push({ type: "string", value: str });
    }

    function addSpecifierToken(specifier, precision, substitutionIndex)
    {
        tokens.push({ type: "specifier", specifier: specifier, precision: precision, substitutionIndex: substitutionIndex });
    }

    var index = 0;
    for (var precentIndex = format.indexOf("%", index); precentIndex !== -1; precentIndex = format.indexOf("%", index)) {
        addStringToken(format.substring(index, precentIndex));
        index = precentIndex + 1;

        if (format[index] === "%") {
            addStringToken("%");
            ++index;
            continue;
        }

        if (!isNaN(format[index])) {
            // The first character is a number, it might be a substitution index.
            var number = parseInt(format.substring(index));
            while (!isNaN(format[index]))
                ++index;
            // If the number is greater than zero and ends with a "$",
            // then this is a substitution index.
            if (number > 0 && format[index] === "$") {
                substitutionIndex = (number - 1);
                ++index;
            }
        }

        var precision = -1;
        if (format[index] === ".") {
            // This is a precision specifier. If no digit follows the ".",
            // then the precision should be zero.
            ++index;
            precision = parseInt(format.substring(index));
            if (isNaN(precision))
                precision = 0;
            while (!isNaN(format[index]))
                ++index;
        }

        addSpecifierToken(format[index], precision, substitutionIndex);

        ++substitutionIndex;
        ++index;
    }

    addStringToken(format.substring(index));

    return tokens;
}

String.standardFormatters = {
    d: function(substitution)
    {
        substitution = parseInt(substitution);
        return !isNaN(substitution) ? substitution : 0;
    },

    f: function(substitution, token)
    {
        substitution = parseFloat(substitution);
        if (substitution && token.precision > -1)
            substitution = substitution.toFixed(token.precision);
        return !isNaN(substitution) ? substitution : (token.precision > -1 ? Number(0).toFixed(token.precision) : 0);
    },

    s: function(substitution)
    {
        return substitution;
    },
};

String.vsprintf = function(format, substitutions)
{
    return String.format(format, substitutions, String.standardFormatters, "", function(a, b) { return a + b; }).formattedResult;
}

String.format = function(format, substitutions, formatters, initialValue, append)
{
    if (!format || !substitutions || !substitutions.length)
        return { formattedResult: append(initialValue, format), unusedSubstitutions: substitutions };

    function prettyFunctionName()
    {
        return "String.format(\"" + format + "\", \"" + substitutions.join("\", \"") + "\")";
    }

    function warn(msg)
    {
        console.warn(prettyFunctionName() + ": " + msg);
    }

    function error(msg)
    {
        console.error(prettyFunctionName() + ": " + msg);
    }

    var result = initialValue;
    var tokens = String.tokenizeFormatString(format);
    var usedSubstitutionIndexes = {};

    for (var i = 0; i < tokens.length; ++i) {
        var token = tokens[i];

        if (token.type === "string") {
            result = append(result, token.value);
            continue;
        }

        if (token.type !== "specifier") {
            error("Unknown token type \"" + token.type + "\" found.");
            continue;
        }

        if (token.substitutionIndex >= substitutions.length) {
            // If there are not enough substitutions for the current substitutionIndex
            // just output the format specifier literally and move on.
            error("not enough substitution arguments. Had " + substitutions.length + " but needed " + (token.substitutionIndex + 1) + ", so substitution was skipped.");
            result = append(result, "%" + (token.precision > -1 ? token.precision : "") + token.specifier);
            continue;
        }

        usedSubstitutionIndexes[token.substitutionIndex] = true;

        if (!(token.specifier in formatters)) {
            // Encountered an unsupported format character, treat as a string.
            warn("unsupported format character \u201C" + token.specifier + "\u201D. Treating as a string.");
            result = append(result, substitutions[token.substitutionIndex]);
            continue;
        }

        result = append(result, formatters[token.specifier](substitutions[token.substitutionIndex], token));
    }

    var unusedSubstitutions = [];
    for (var i = 0; i < substitutions.length; ++i) {
        if (i in usedSubstitutionIndexes)
            continue;
        unusedSubstitutions.push(substitutions[i]);
    }

    return { formattedResult: result, unusedSubstitutions: unusedSubstitutions };
}
