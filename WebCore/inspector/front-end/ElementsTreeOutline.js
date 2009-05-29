/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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

WebInspector.ElementsTreeOutline = function() {
    this.element = document.createElement("ol");
    this.element.addEventListener("mousedown", this._onmousedown.bind(this), false);
    this.element.addEventListener("dblclick", this._ondblclick.bind(this), false);
    this.element.addEventListener("mousemove", this._onmousemove.bind(this), false);
    this.element.addEventListener("mouseout", this._onmouseout.bind(this), false);

    TreeOutline.call(this, this.element);

    this.includeRootDOMNode = true;
    this.selectEnabled = false;
    this.rootDOMNode = null;
    this.focusedDOMNode = null;
}

WebInspector.ElementsTreeOutline.prototype = {
    get rootDOMNode()
    {
        return this._rootDOMNode;
    },

    set rootDOMNode(x)
    {
        if (objectsAreSame(this._rootDOMNode, x))
            return;

        this._rootDOMNode = x;

        this.update();
    },

    get focusedDOMNode()
    {
        return this._focusedDOMNode;
    },

    set focusedDOMNode(x)
    {
        if (objectsAreSame(this._focusedDOMNode, x)) {
            this.revealAndSelectNode(x);
            return;
        }

        this._focusedDOMNode = x;

        this.revealAndSelectNode(x);

        // The revealAndSelectNode() method might find a different element if there is inlined text,
        // and the select() call would change the focusedDOMNode and reenter this setter. So to
        // avoid calling focusedNodeChanged() twice, first check if _focusedDOMNode is the same
        // node as the one passed in.
        if (objectsAreSame(this._focusedDOMNode, x)) {
            this.focusedNodeChanged();

            if (x && !this.suppressSelectHighlight) {
                InspectorController.highlightDOMNode(x);

                if ("_restorePreviousHighlightNodeTimeout" in this)
                    clearTimeout(this._restorePreviousHighlightNodeTimeout);

                function restoreHighlightToHoveredNode()
                {
                    var hoveredNode = WebInspector.hoveredDOMNode;
                    if (hoveredNode)
                        InspectorController.highlightDOMNode(hoveredNode);
                    else
                        InspectorController.hideDOMNodeHighlight();
                }

                this._restorePreviousHighlightNodeTimeout = setTimeout(restoreHighlightToHoveredNode, 2000);
            }
        }
    },

    update: function()
    {
        this.removeChildren();

        if (!this.rootDOMNode)
            return;

        var treeElement;
        if (this.includeRootDOMNode) {
            treeElement = new WebInspector.ElementsTreeElement(this.rootDOMNode);
            treeElement.selectable = this.selectEnabled;
            this.appendChild(treeElement);
        } else {
            // FIXME: this could use findTreeElement to reuse a tree element if it already exists
            var node = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(this.rootDOMNode) : this.rootDOMNode.firstChild);
            while (node) {
                treeElement = new WebInspector.ElementsTreeElement(node);
                treeElement.selectable = this.selectEnabled;
                this.appendChild(treeElement);
                node = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
            }
        }

        this.updateSelection();
    },

    updateSelection: function()
    {
        if (!this.selectedTreeElement)
            return;
        var element = this.treeOutline.selectedTreeElement;
        element.updateSelection();
    },

    focusedNodeChanged: function(forceUpdate) {},

    findTreeElement: function(node, isAncestor, getParent, equal)
    {
        if (typeof isAncestor === "undefined")
            isAncestor = isAncestorIncludingParentFrames;
        if (typeof getParent === "undefined")
            getParent = parentNodeOrFrameElement;
        if (typeof equal === "undefined")
            equal = objectsAreSame;

        var treeElement = TreeOutline.prototype.findTreeElement.call(this, node, isAncestor, getParent, equal);
        if (!treeElement && node.nodeType === Node.TEXT_NODE) {
            // The text node might have been inlined if it was short, so try to find the parent element.
            treeElement = TreeOutline.prototype.findTreeElement.call(this, node.parentNode, isAncestor, getParent, equal);
        }

        return treeElement;
    },

    revealAndSelectNode: function(node)
    {
        if (!node)
            return;

        var treeElement = this.findTreeElement(node);
        if (!treeElement)
            return;

        treeElement.reveal();
        treeElement.select();
    },

    _treeElementFromEvent: function(event)
    {
        var root = this.element;

        // We choose this X coordinate based on the knowledge that our list
        // items extend nearly to the right edge of the outer <ol>.
        var x = root.totalOffsetLeft + root.offsetWidth - 20;

        var y = event.pageY;

        // Our list items have 1-pixel cracks between them vertically. We avoid
        // the cracks by checking slightly above and slightly below the mouse
        // and seeing if we hit the same element each time.
        var elementUnderMouse = this.treeElementFromPoint(x, y);
        var elementAboveMouse = this.treeElementFromPoint(x, y - 2);
        var element;
        if (elementUnderMouse === elementAboveMouse)
            element = elementUnderMouse;
        else
            element = this.treeElementFromPoint(x, y + 2);

        return element;
    },

    _ondblclick: function(event)
    {
        var element = this._treeElementFromEvent(event);

        if (!element || !element.ondblclick)
            return;

        element.ondblclick(element, event);
    },

    _onmousedown: function(event)
    {
        var element = this._treeElementFromEvent(event);

        if (!element || element.isEventWithinDisclosureTriangle(event))
            return;

        element.select();
    },

    _onmousemove: function(event)
    {
        if (this._previousHoveredElement) {
            this._previousHoveredElement.hovered = false;
            delete this._previousHoveredElement;
        }

        var element = this._treeElementFromEvent(event);
        if (element && !element.elementCloseTag) {
            element.hovered = true;
            this._previousHoveredElement = element;
        }

        WebInspector.hoveredDOMNode = (element && !element.elementCloseTag ? element.representedObject : null);
    },

    _onmouseout: function(event)
    {
        var nodeUnderMouse = document.elementFromPoint(event.pageX, event.pageY);
        if (nodeUnderMouse.isDescendant(this.element))
            return;

        if (this._previousHoveredElement) {
            this._previousHoveredElement.hovered = false;
            delete this._previousHoveredElement;
        }

        WebInspector.hoveredDOMNode = null;
    }
}

WebInspector.ElementsTreeOutline.prototype.__proto__ = TreeOutline.prototype;

WebInspector.ElementsTreeElement = function(node)
{
    var hasChildren = node.contentDocument || (Preferences.ignoreWhitespace ? (firstChildSkippingWhitespace.call(node) ? true : false) : node.hasChildNodes());
    var titleInfo = nodeTitleInfo.call(node, hasChildren, WebInspector.linkifyURL);

    if (titleInfo.hasChildren) 
        this.whitespaceIgnored = Preferences.ignoreWhitespace;

    // The title will be updated in onattach.
    TreeElement.call(this, "", node, titleInfo.hasChildren);
}

WebInspector.ElementsTreeElement.prototype = {
    get highlighted()
    {
        return this._highlighted;
    },

    set highlighted(x)
    {
        if (this._highlighted === x)
            return;

        this._highlighted = x;

        if (this.listItemElement) {
            if (x)
                this.listItemElement.addStyleClass("highlighted");
            else
                this.listItemElement.removeStyleClass("highlighted");
        }
    },

    get hovered()
    {
        return this._hovered;
    },

    set hovered(x)
    {
        if (this._hovered === x)
            return;

        this._hovered = x;

        if (this.listItemElement) {
            if (x) {
                this.updateSelection();
                this.listItemElement.addStyleClass("hovered");
            } else
                this.listItemElement.removeStyleClass("hovered");
        }
    },

    updateSelection: function()
    {
        var listItemElement = this.listItemElement;
        if (!listItemElement)
            return;

        if (document.body.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
            return;
        }

        if (!this.selectionElement) {
            this.selectionElement = document.createElement("div");
            this.selectionElement.className = "selection selected";
            listItemElement.insertBefore(this.selectionElement, listItemElement.firstChild);
        }

        this.selectionElement.style.height = listItemElement.offsetHeight + "px";
    },

    onattach: function()
    {
        this.listItemElement.addEventListener("mousedown", this.onmousedown.bind(this), false);

        if (this._highlighted)
            this.listItemElement.addStyleClass("highlighted");

        if (this._hovered) {
            this.updateSelection();
            this.listItemElement.addStyleClass("hovered");
        }

        this._updateTitle();

        this._preventFollowingLinksOnDoubleClick();
    },

    _preventFollowingLinksOnDoubleClick: function()
    {
        var links = this.listItemElement.querySelectorAll("li > .webkit-html-tag > .webkit-html-attribute > .webkit-html-external-link, li > .webkit-html-tag > .webkit-html-attribute > .webkit-html-resource-link");
        if (!links)
            return;

        for (var i = 0; i < links.length; ++i)
            links[i].preventFollowOnDoubleClick = true;
    },

    onpopulate: function()
    {
        if (this.children.length || this.whitespaceIgnored !== Preferences.ignoreWhitespace)
            return;

        this.whitespaceIgnored = Preferences.ignoreWhitespace;

        this.updateChildren();
    },

    updateChildren: function(fullRefresh)
    {
        if (fullRefresh) {
            var selectedTreeElement = this.treeOutline.selectedTreeElement;
            if (selectedTreeElement && selectedTreeElement.hasAncestor(this))
                this.select();
            this.removeChildren();
        }

        var treeElement = this;
        var treeChildIndex = 0;

        function updateChildrenOfNode(node)
        {
            var treeOutline = treeElement.treeOutline;
            var child = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(node) : node.firstChild);
            while (child) {
                var currentTreeElement = treeElement.children[treeChildIndex];
                if (!currentTreeElement || !objectsAreSame(currentTreeElement.representedObject, child)) {
                    // Find any existing element that is later in the children list.
                    var existingTreeElement = null;
                    for (var i = (treeChildIndex + 1); i < treeElement.children.length; ++i) {
                        if (objectsAreSame(treeElement.children[i].representedObject, child)) {
                            existingTreeElement = treeElement.children[i];
                            break;
                        }
                    }

                    if (existingTreeElement && existingTreeElement.parent === treeElement) {
                        // If an existing element was found and it has the same parent, just move it.
                        var wasSelected = existingTreeElement.selected;
                        treeElement.removeChild(existingTreeElement);
                        treeElement.insertChild(existingTreeElement, treeChildIndex);
                        if (wasSelected)
                            existingTreeElement.select();
                    } else {
                        // No existing element found, insert a new element.
                        var newElement = new WebInspector.ElementsTreeElement(child);
                        newElement.selectable = treeOutline.selectEnabled;
                        treeElement.insertChild(newElement, treeChildIndex);
                    }
                }

                child = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(child) : child.nextSibling;
                ++treeChildIndex;
            }
        }

        // Remove any tree elements that no longer have this node (or this node's contentDocument) as their parent.
        for (var i = (this.children.length - 1); i >= 0; --i) {
            if ("elementCloseTag" in this.children[i])
                continue;

            var currentChild = this.children[i];
            var currentNode = currentChild.representedObject;
            var currentParentNode = currentNode.parentNode;

            if (objectsAreSame(currentParentNode, this.representedObject))
                continue;
            if (this.representedObject.contentDocument && objectsAreSame(currentParentNode, this.representedObject.contentDocument))
                continue;

            var selectedTreeElement = this.treeOutline.selectedTreeElement;
            if (selectedTreeElement && (selectedTreeElement === currentChild || selectedTreeElement.hasAncestor(currentChild)))
                this.select();

            this.removeChildAtIndex(i);

            if (this.treeOutline.panel && currentNode.contentDocument)
                this.treeOutline.panel.unregisterMutationEventListeners(currentNode.contentDocument.defaultView);
        }

        if (this.representedObject.contentDocument)
            updateChildrenOfNode(this.representedObject.contentDocument);
        updateChildrenOfNode(this.representedObject);

        var lastChild = this.children[this.children.length - 1];
        if (this.representedObject.nodeType == Node.ELEMENT_NODE && (!lastChild || !lastChild.elementCloseTag)) {
            var title = "<span class=\"webkit-html-tag close\">&lt;/" + this.representedObject.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
            var item = new TreeElement(title, null, false);
            item.selectable = false;
            item.elementCloseTag = true;
            this.appendChild(item);
        }
    },

    onexpand: function()
    {
        this.treeOutline.updateSelection();

        if (this.treeOutline.panel && this.representedObject.contentDocument)
            this.treeOutline.panel.registerMutationEventListeners(this.representedObject.contentDocument.defaultView);
    },

    oncollapse: function()
    {
        this.treeOutline.updateSelection();
    },

    onreveal: function()
    {
        if (this.listItemElement)
            this.listItemElement.scrollIntoViewIfNeeded(false);
    },

    onselect: function()
    {
        this.treeOutline.focusedDOMNode = this.representedObject;
        this.updateSelection();
    },

    onmousedown: function(event)
    {
        if (this._editing)
            return;

        // Prevent selecting the nearest word on double click.
        if (event.detail >= 2)
            event.preventDefault();
    },

    ondblclick: function(treeElement, event)
    {
        if (this._editing)
            return;

        if (this._startEditing(event))
            return;

        if (this.treeOutline.panel) {
            this.treeOutline.rootDOMNode = this.representedObject.parentNode;
            this.treeOutline.focusedDOMNode = this.representedObject;
        }

        if (this.hasChildren && !this.expanded)
            this.expand();
    },

    _startEditing: function(event)
    {
        if (this.treeOutline.focusedDOMNode != this.representedObject)
            return;

        if (this.representedObject.nodeType != Node.ELEMENT_NODE && this.representedObject.nodeType != Node.TEXT_NODE)
            return false;

        var textNode = event.target.enclosingNodeOrSelfWithClass("webkit-html-text-node");
        if (textNode)
            return this._startEditingTextNode(textNode);

        var attribute = event.target.enclosingNodeOrSelfWithClass("webkit-html-attribute");
        if (attribute)
            return this._startEditingAttribute(attribute, event);

        return false;
    },

    _startEditingAttribute: function(attribute, event)
    {
        if (WebInspector.isBeingEdited(attribute))
            return true;

        var attributeNameElement = attribute.getElementsByClassName("webkit-html-attribute-name")[0];
        if (!attributeNameElement)
            return false;

        var attributeName = attributeNameElement.innerText;

        function removeZeroWidthSpaceRecursive(node)
        {
            if (node.nodeType === Node.TEXT_NODE) {
                node.nodeValue = node.nodeValue.replace(/\u200B/g, "");
                return;
            }

            if (node.nodeType !== Node.ELEMENT_NODE)
                return;

            for (var child = node.firstChild; child; child = child.nextSibling)
                removeZeroWidthSpaceRecursive(child);
        }

        // Remove zero-width spaces that were added by nodeTitleInfo.
        removeZeroWidthSpaceRecursive(attribute);

        this._editing = true;

        WebInspector.startEditing(attribute, this._attributeEditingCommitted.bind(this), this._editingCancelled.bind(this), attributeName);
        window.getSelection().setBaseAndExtent(event.target, 0, event.target, 1);

        return true;
    },

    _startEditingTextNode: function(textNode)
    {
        if (WebInspector.isBeingEdited(textNode))
            return true;

        this._editing = true;

        WebInspector.startEditing(textNode, this._textNodeEditingCommitted.bind(this), this._editingCancelled.bind(this));
        window.getSelection().setBaseAndExtent(textNode, 0, textNode, 1);

        return true;
    },

    _attributeEditingCommitted: function(element, newText, oldText, attributeName)
    {
        delete this._editing;

        var parseContainerElement = document.createElement("span");
        parseContainerElement.innerHTML = "<span " + newText + "></span>";
        var parseElement = parseContainerElement.firstChild;
        if (!parseElement || !parseElement.hasAttributes()) {
            this._editingCancelled(element, context);
            return;
        }

        var foundOriginalAttribute = false;
        for (var i = 0; i < parseElement.attributes.length; ++i) {
            var attr = parseElement.attributes[i];
            foundOriginalAttribute = foundOriginalAttribute || attr.name === attributeName;
            InspectorController.inspectedWindow().Element.prototype.setAttribute.call(this.representedObject, attr.name, attr.value);
        }

        if (!foundOriginalAttribute)
            InspectorController.inspectedWindow().Element.prototype.removeAttribute.call(this.representedObject, attributeName);

        this._updateTitle();

        this.treeOutline.focusedNodeChanged(true);
    },

    _textNodeEditingCommitted: function(element, newText)
    {
        delete this._editing;

        var textNode;
        if (this.representedObject.nodeType == Node.ELEMENT_NODE) {
            // We only show text nodes inline in elements if the element only
            // has a single child, and that child is a text node.
            textNode = this.representedObject.firstChild;
        } else if (this.representedObject.nodeType == Node.TEXT_NODE)
            textNode = this.representedObject;

        textNode.nodeValue = newText;
        this._updateTitle();
    },

    _editingCancelled: function(element, context)
    {
        delete this._editing;

        this._updateTitle();
    },

    _updateTitle: function()
    {
        var title = nodeTitleInfo.call(this.representedObject, this.hasChildren, WebInspector.linkifyURL).title;
        this.title = "<span class=\"highlight\">" + title + "</span>";
        delete this.selectionElement;
        this.updateSelection();
        this._preventFollowingLinksOnDoubleClick();
    },
}

WebInspector.ElementsTreeElement.prototype.__proto__ = TreeElement.prototype;
