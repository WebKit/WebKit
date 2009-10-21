/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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

WebInspector.ElementsTreeOutline = function() {
    this.element = document.createElement("ol");
    this.element.addEventListener("mousedown", this._onmousedown.bind(this), false);
    this.element.addEventListener("mousemove", this._onmousemove.bind(this), false);
    this.element.addEventListener("mouseout", this._onmouseout.bind(this), false);

    TreeOutline.call(this, this.element);

    this.includeRootDOMNode = true;
    this.selectEnabled = false;
    this.showInElementsPanelEnabled = false;
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
        if (this._rootDOMNode === x)
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
        if (this._focusedDOMNode === x) {
            this.revealAndSelectNode(x);
            return;
        }

        this._focusedDOMNode = x;

        this.revealAndSelectNode(x);

        // The revealAndSelectNode() method might find a different element if there is inlined text,
        // and the select() call would change the focusedDOMNode and reenter this setter. So to
        // avoid calling focusedNodeChanged() twice, first check if _focusedDOMNode is the same
        // node as the one passed in.
        if (this._focusedDOMNode === x) {
            this.focusedNodeChanged();

            if (x && !this.suppressSelectHighlight) {
                InspectorController.highlightDOMNode(x.id);

                if ("_restorePreviousHighlightNodeTimeout" in this)
                    clearTimeout(this._restorePreviousHighlightNodeTimeout);

                function restoreHighlightToHoveredNode()
                {
                    var hoveredNode = WebInspector.hoveredDOMNode;
                    if (hoveredNode)
                        InspectorController.highlightDOMNode(hoveredNode.id);
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
            var node = this.rootDOMNode.firstChild;
            while (node) {
                treeElement = new WebInspector.ElementsTreeElement(node);
                treeElement.selectable = this.selectEnabled;
                this.appendChild(treeElement);
                node = node.nextSibling;
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

    findTreeElement: function(node)
    {
        var treeElement = TreeOutline.prototype.findTreeElement.call(this, node, isAncestorNode, parentNode);
        if (!treeElement && node.nodeType === Node.TEXT_NODE) {
            // The text node might have been inlined if it was short, so try to find the parent element.
            treeElement = TreeOutline.prototype.findTreeElement.call(this, node.parentNode, isAncestorNode, parentNode);
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
    
    handleKeyEvent: function(event)
    {
        var selectedElement = this.selectedTreeElement;
        if (!selectedElement)
            return;

        // Delete or backspace pressed, delete the node.
        if (event.keyCode === 8 || event.keyCode === 46) {
            selectedElement.remove();
            return;
        }

        // On Enter or Return start editing the first attribute
        // or create a new attribute on the selected element.
        if (event.keyIdentifier === "Enter") {
            if (this._editing)
                return;

            selectedElement._startEditing();

            // prevent a newline from being immediately inserted
            event.preventDefault();
            return;
        }

        TreeOutline.prototype.handleKeyEvent.call(this, event);
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
        var element = this._treeElementFromEvent(event);
        if (element && this._previousHoveredElement === element)
            return;

        if (this._previousHoveredElement) {
            this._previousHoveredElement.hovered = false;
            delete this._previousHoveredElement;
        }

        if (element && !element.elementCloseTag) {
            element.hovered = true;
            this._previousHoveredElement = element;
        }

        WebInspector.hoveredDOMNode = (element && !element.elementCloseTag ? element.representedObject : null);
    },

    _onmouseout: function(event)
    {
        var nodeUnderMouse = document.elementFromPoint(event.pageX, event.pageY);
        if (nodeUnderMouse && nodeUnderMouse.isDescendant(this.element))
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
    var hasChildrenOverride = node.hasChildNodes() && !this._showInlineText(node);

    // The title will be updated in onattach.
    TreeElement.call(this, "", node, hasChildrenOverride);

    if (this.representedObject.nodeType == Node.ELEMENT_NODE)
        this._canAddAttributes = true;
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
                if (this._canAddAttributes)
                    this._pendingToggleNewAttribute = setTimeout(this.toggleNewAttributeButton.bind(this, true), 500);
            } else {
                this.listItemElement.removeStyleClass("hovered");
                if (this._pendingToggleNewAttribute) {
                    clearTimeout(this._pendingToggleNewAttribute);
                    delete this._pendingToggleNewAttribute;
                }
                this.toggleNewAttributeButton(false);
            }
        }
    },

    toggleNewAttributeButton: function(visible)
    {
        function removeAddAttributeSpan()
        {
            if (this._addAttributeElement && this._addAttributeElement.parentNode)
                this._addAttributeElement.parentNode.removeChild(this._addAttributeElement);
            delete this._addAttributeElement;
        }

        if (!this._addAttributeElement && visible && !this._editing) {
            var span = document.createElement("span");
            span.className = "add-attribute webkit-html-attribute-name";
            span.textContent = " ?=\"\"";
            span.addEventListener("dblclick", removeAddAttributeSpan.bind(this), false);
            this._addAttributeElement = span;

            var tag = this.listItemElement.getElementsByClassName("webkit-html-tag")[0];
            this._insertInLastAttributePosition(tag, span);
        } else if (!visible && this._addAttributeElement)
            removeAddAttributeSpan.call(this);
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
        if (this.children.length || this._showInlineText(this.representedObject))
            return;

        this.updateChildren();
    },
    
    updateChildren: function(fullRefresh)
    {
        WebInspector.domAgent.getChildNodesAsync(this.representedObject, this._updateChildren.bind(this, fullRefresh));
    },

    _updateChildren: function(fullRefresh)
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
            var child = node.firstChild;
            while (child) {
                var currentTreeElement = treeElement.children[treeChildIndex];
                if (!currentTreeElement || currentTreeElement.representedObject !== child) {
                    // Find any existing element that is later in the children list.
                    var existingTreeElement = null;
                    for (var i = (treeChildIndex + 1); i < treeElement.children.length; ++i) {
                        if (treeElement.children[i].representedObject === child) {
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

                child = child.nextSibling;
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

            if (currentParentNode === this.representedObject)
                continue;

            var selectedTreeElement = this.treeOutline.selectedTreeElement;
            if (selectedTreeElement && (selectedTreeElement === currentChild || selectedTreeElement.hasAncestor(currentChild)))
                this.select();

            this.removeChildAtIndex(i);
        }

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

        if (this.treeOutline.showInElementsPanelEnabled) {    
            WebInspector.showElementsPanel();
            WebInspector.panels.elements.focusedDOMNode = this.representedObject;
        }

        // Prevent selecting the nearest word on double click.
        if (event.detail >= 2)
            event.preventDefault();
    },

    ondblclick: function(treeElement, event)
    {
        if (this._editing)
            return;

        if (this._startEditingFromEvent(event, treeElement))
            return;

        if (this.treeOutline.panel) {
            this.treeOutline.rootDOMNode = this.representedObject.parentNode;
            this.treeOutline.focusedDOMNode = this.representedObject;
        }

        if (this.hasChildren && !this.expanded)
            this.expand();
    },

    _insertInLastAttributePosition: function(tag, node)
    {
        if (tag.getElementsByClassName("webkit-html-attribute").length > 0)
            tag.insertBefore(node, tag.lastChild);
        else {
            var nodeName = tag.textContent.match(/^<(.*?)>$/)[1];
            tag.textContent = '';
            tag.appendChild(document.createTextNode('<'+nodeName));
            tag.appendChild(node);
            tag.appendChild(document.createTextNode('>'));
        }
    },

    _startEditingFromEvent: function(event, treeElement)
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
            return this._startEditingAttribute(attribute, event.target);

        var newAttribute = event.target.enclosingNodeOrSelfWithClass("add-attribute");
        if (newAttribute)
            return this._addNewAttribute(treeElement.listItemElement);

        return false;
    },

    _startEditing: function()
    {
        if (this.treeOutline.focusedDOMNode !== this.representedObject)
            return;

        var listItem = this._listItemNode;

        if (this._canAddAttributes) {
            this.toggleNewAttributeButton(false);
            var attribute = listItem.getElementsByClassName("webkit-html-attribute")[0];
            if (attribute)
                return this._startEditingAttribute(attribute, attribute.getElementsByClassName("webkit-html-attribute-value")[0]);

            return this._addNewAttribute(listItem);
        }

        if (this.representedObject.nodeType === Node.TEXT_NODE) {
            var textNode = listItem.getElementsByClassName("webkit-html-text-node")[0];
            if (textNode)
                return this._startEditingTextNode(textNode);
            return;
        }
    },

    _addNewAttribute: function(listItemElement)
    {
        var attr = document.createElement("span");
        attr.className = "webkit-html-attribute";
        attr.style.marginLeft = "2px"; // overrides the .editing margin rule
        attr.style.marginRight = "2px"; // overrides the .editing margin rule
        var name = document.createElement("span");
        name.className = "webkit-html-attribute-name new-attribute";
        name.textContent = " ";
        var value = document.createElement("span");
        value.className = "webkit-html-attribute-value";
        attr.appendChild(name);
        attr.appendChild(value);

        var tag = listItemElement.getElementsByClassName("webkit-html-tag")[0];
        this._insertInLastAttributePosition(tag, attr);
        return this._startEditingAttribute(attr, attr);
    },

    _triggerEditAttribute: function(attributeName)
    {
        var attributeElements = this.listItemElement.getElementsByClassName("webkit-html-attribute-name");
        for (var i = 0, len = attributeElements.length; i < len; ++i) {
            if (attributeElements[i].textContent === attributeName) {
                for (var elem = attributeElements[i].nextSibling; elem; elem = elem.nextSibling) {
                    if (elem.nodeType !== Node.ELEMENT_NODE)
                        continue;

                    if (elem.hasStyleClass("webkit-html-attribute-value"))
                        return this._startEditingAttribute(attributeElements[i].parentNode, elem);
                }
            }
        }
    },

    _startEditingAttribute: function(attribute, elementForSelection)
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
        window.getSelection().setBaseAndExtent(elementForSelection, 0, elementForSelection, 1);

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

    _attributeEditingCommitted: function(element, newText, oldText, attributeName, moveDirection)
    {
        delete this._editing;

        // Before we do anything, determine where we should move
        // next based on the current element's settings
        var moveToAttribute;
        var newAttribute;
        if (moveDirection) {
            var found = false;
            var attributes = this.representedObject.attributes;
            for (var i = 0, len = attributes.length; i < len; ++i) {
                if (attributes[i].name === attributeName) {
                    found = true;
                    if (moveDirection === "backward" && i > 0)
                        moveToAttribute = attributes[i - 1].name;
                    else if (moveDirection === "forward" && i < attributes.length - 1)
                        moveToAttribute = attributes[i + 1].name;
                    else if (moveDirection === "forward" && i === attributes.length - 1)
                        newAttribute = true;
                }
            }

            if (!found && moveDirection === "backward" && attributes.length > 0)
                moveToAttribute = attributes[attributes.length - 1].name;
            else if (!found && moveDirection === "forward" && !/^\s*$/.test(newText))
                newAttribute = true;
        }

        function moveToNextAttributeIfNeeded() {
            if (moveToAttribute)
                this._triggerEditAttribute(moveToAttribute);
            else if (newAttribute)
                this._addNewAttribute(this.listItemElement);
        }

        var parseContainerElement = document.createElement("span");
        parseContainerElement.innerHTML = "<span " + newText + "></span>";
        var parseElement = parseContainerElement.firstChild;

        if (!parseElement) {
            this._editingCancelled(element, attributeName);
            moveToNextAttributeIfNeeded.call(this);
            return;
        }

        if (!parseElement.hasAttributes()) {
            this.representedObject.removeAttribute(attributeName);
            this._updateTitle();
            moveToNextAttributeIfNeeded.call(this);
            return;
        }

        var foundOriginalAttribute = false;
        for (var i = 0; i < parseElement.attributes.length; ++i) {
            var attr = parseElement.attributes[i];
            foundOriginalAttribute = foundOriginalAttribute || attr.name === attributeName;
            try {
                this.representedObject.setAttribute(attr.name, attr.value);
            } catch(e) {} // ignore invalid attribute (innerHTML doesn't throw errors, but this can)
        }

        if (!foundOriginalAttribute)
            this.representedObject.removeAttribute(attributeName);

        this._updateTitle();

        this.treeOutline.focusedNodeChanged(true);

        moveToNextAttributeIfNeeded.call(this);
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
        var title = this._nodeTitleInfo(this.representedObject, this.hasChildren, WebInspector.linkifyURL).title;
        this.title = "<span class=\"highlight\">" + title + "</span>";
        delete this.selectionElement;
        this.updateSelection();
        this._preventFollowingLinksOnDoubleClick();
    },

    _nodeTitleInfo: function(node, hasChildren, linkify)
    {
        var info = {title: "", hasChildren: hasChildren};
        
        switch (node.nodeType) {
            case Node.DOCUMENT_NODE:
                info.title = "Document";
                break;
                
            case Node.ELEMENT_NODE:
                info.title = "<span class=\"webkit-html-tag\">&lt;" + node.nodeName.toLowerCase().escapeHTML();
                
                if (node.hasAttributes()) {
                    for (var i = 0; i < node.attributes.length; ++i) {
                        var attr = node.attributes[i];
                        info.title += " <span class=\"webkit-html-attribute\"><span class=\"webkit-html-attribute-name\">" + attr.name.escapeHTML() + "</span>=&#8203;\"";
                        
                        var value = attr.value;
                        if (linkify && (attr.name === "src" || attr.name === "href")) {
                            var value = value.replace(/([\/;:\)\]\}])/g, "$1\u200B");
                            info.title += linkify(attr.value, value, "webkit-html-attribute-value", node.nodeName.toLowerCase() == "a");
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
                
                var textChild = onlyTextChild.call(node);
                var showInlineText = textChild && textChild.textContent.length < Preferences.maxInlineTextChildLength;
                
                if (showInlineText) {
                    info.title += "<span class=\"webkit-html-text-node\">" + textChild.nodeValue.escapeHTML() + "</span>&#8203;<span class=\"webkit-html-tag\">&lt;/" + node.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
                    info.hasChildren = false;
                }
                break;
                
            case Node.TEXT_NODE:
                if (isNodeWhitespace.call(node))
                    info.title = "(whitespace)";
                else {
                    if (node.parentNode && node.parentNode.nodeName.toLowerCase() == "script") {
                        var newNode = document.createElement("span");
                        newNode.textContent = node.textContent;
                        
                        var javascriptSyntaxHighlighter = new WebInspector.JavaScriptSourceSyntaxHighlighter(null, null);
                        javascriptSyntaxHighlighter.syntaxHighlightLine(newNode, null);
                        
                        info.title = "<span class=\"webkit-html-text-node webkit-html-js-node\">" + newNode.innerHTML.replace(/^[\n\r]*/, "").replace(/\s*$/, "") + "</span>";
                    } else if (node.parentNode && node.parentNode.nodeName.toLowerCase() == "style") {
                        var newNode = document.createElement("span");
                        newNode.textContent = node.textContent;
                        
                        var cssSyntaxHighlighter = new WebInspector.CSSSourceSyntaxHighligher(null, null);
                        cssSyntaxHighlighter.syntaxHighlightLine(newNode, null);
                        
                        info.title = "<span class=\"webkit-html-text-node webkit-html-css-node\">" + newNode.innerHTML.replace(/^[\n\r]*/, "").replace(/\s*$/, "") + "</span>";
                    } else {
                        info.title = "\"<span class=\"webkit-html-text-node\">" + node.nodeValue.escapeHTML() + "</span>\""; 
                    }
                } 
                break;
                
            case Node.COMMENT_NODE:
                info.title = "<span class=\"webkit-html-comment\">&lt;!--" + node.nodeValue.escapeHTML() + "--&gt;</span>";
                break;
                
            case Node.DOCUMENT_TYPE_NODE:
                info.title = "<span class=\"webkit-html-doctype\">&lt;!DOCTYPE " + node.nodeName;
                if (node.publicId) {
                    info.title += " PUBLIC \"" + node.publicId + "\"";
                    if (node.systemId)
                        info.title += " \"" + node.systemId + "\"";
                } else if (node.systemId)
                    info.title += " SYSTEM \"" + node.systemId + "\"";
                if (node.internalSubset)
                    info.title += " [" + node.internalSubset + "]";
                info.title += "&gt;</span>";
                break;
            default:
                info.title = node.nodeName.toLowerCase().collapseWhitespace().escapeHTML();
        }
        
        return info;
    },

    _showInlineText: function(node)
    {
        if (node.nodeType === Node.ELEMENT_NODE) {
            var textChild = onlyTextChild.call(node);
            if (textChild && textChild.textContent.length < Preferences.maxInlineTextChildLength)
                return true;
        }
        return false;
    },
    
    remove: function()
    {
        var parentElement = this.parent;
        if (!parentElement)
            return;

        var self = this;
        function removeNodeCallback(removedNodeId)
        {
            // -1 is an error code, which means removing the node from the DOM failed,
            // so we shouldn't remove it from the tree.
            if (removedNodeId === -1)
                return;

            parentElement.removeChild(self);
        }

        var callId = WebInspector.Callback.wrap(removeNodeCallback);
        InspectorController.removeNode(callId, this.representedObject.id);
    }
}

WebInspector.ElementsTreeElement.prototype.__proto__ = TreeElement.prototype;

WebInspector.didRemoveNode = WebInspector.Callback.processCallback;
