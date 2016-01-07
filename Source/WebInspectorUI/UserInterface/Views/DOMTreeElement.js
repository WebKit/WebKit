/*
 * Copyright (C) 2007, 2008, 2013, 2015 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

WebInspector.DOMTreeElement = class DOMTreeElement extends WebInspector.TreeElement
{
    constructor(node, elementCloseTag)
    {
        super("", node);

        this._elementCloseTag = elementCloseTag;
        this.hasChildren = !elementCloseTag && this._hasVisibleChildren();

        if (this.representedObject.nodeType() === Node.ELEMENT_NODE && !elementCloseTag)
            this._canAddAttributes = true;
        this._searchQuery = null;
        this._expandedChildrenLimit = WebInspector.DOMTreeElement.InitialChildrenLimit;

        this._nodeStateChanges = [];
        this._boundNodeChangedAnimationEnd = this._nodeChangedAnimationEnd.bind(this);

        node.addEventListener(WebInspector.DOMNode.Event.EnabledPseudoClassesChanged, this._nodePseudoClassesDidChange, this);
    }

    isCloseTag()
    {
        return this._elementCloseTag;
    }

    highlightSearchResults(searchQuery)
    {
        if (this._searchQuery !== searchQuery) {
            this._updateSearchHighlight(false);
            this._highlightResult = undefined; // A new search query.
        }

        this._searchQuery = searchQuery;
        this._searchHighlightsVisible = true;
        this.updateTitle(true);
    }

    hideSearchHighlights()
    {
        this._searchHighlightsVisible = false;
        this._updateSearchHighlight(false);
    }

    emphasizeSearchHighlight()
    {
        var highlightElement = this.title.querySelector("." + WebInspector.DOMTreeElement.SearchHighlightStyleClassName);
        console.assert(highlightElement);
        if (!highlightElement)
            return;

        if (this._bouncyHighlightElement)
            this._bouncyHighlightElement.remove();

        this._bouncyHighlightElement = document.createElement("div");
        this._bouncyHighlightElement.className = WebInspector.DOMTreeElement.BouncyHighlightStyleClassName;
        this._bouncyHighlightElement.textContent = highlightElement.textContent;

        // Position and show the bouncy highlight adjusting the coordinates to be inside the TreeOutline's space.
        var highlightElementRect = highlightElement.getBoundingClientRect();
        var treeOutlineRect = this.treeOutline.element.getBoundingClientRect();
        this._bouncyHighlightElement.style.top = (highlightElementRect.top - treeOutlineRect.top) + "px";
        this._bouncyHighlightElement.style.left = (highlightElementRect.left - treeOutlineRect.left) + "px";
        this.title.appendChild(this._bouncyHighlightElement);

        function animationEnded()
        {
            if (!this._bouncyHighlightElement)
                return;

            this._bouncyHighlightElement.remove();
            this._bouncyHighlightElement = null;
        }

        this._bouncyHighlightElement.addEventListener("animationend", animationEnded.bind(this));
    }

    _updateSearchHighlight(show)
    {
        if (!this._highlightResult)
            return;

        function updateEntryShow(entry)
        {
            switch (entry.type) {
                case "added":
                    entry.parent.insertBefore(entry.node, entry.nextSibling);
                    break;
                case "changed":
                    entry.node.textContent = entry.newText;
                    break;
            }
        }

        function updateEntryHide(entry)
        {
            switch (entry.type) {
                case "added":
                    entry.node.remove();
                    break;
                case "changed":
                    entry.node.textContent = entry.oldText;
                    break;
            }
        }

        var updater = show ? updateEntryShow : updateEntryHide;

        for (var i = 0, size = this._highlightResult.length; i < size; ++i)
            updater(this._highlightResult[i]);
    }

    get hovered()
    {
        return this._hovered;
    }

    set hovered(x)
    {
        if (this._hovered === x)
            return;

        this._hovered = x;

        if (this.listItemElement) {
            if (x) {
                this.updateSelection();
                this.listItemElement.classList.add("hovered");
            } else {
                this.listItemElement.classList.remove("hovered");
            }
        }
    }

    get editable()
    {
        var node = this.representedObject;
        if (node.isInShadowTree())
            return false;
        if (node.isPseudoElement())
            return false;

        return this.treeOutline.editable;
    }

    get expandedChildrenLimit()
    {
        return this._expandedChildrenLimit;
    }

    set expandedChildrenLimit(x)
    {
        if (this._expandedChildrenLimit === x)
            return;

        this._expandedChildrenLimit = x;
        if (this.treeOutline && !this._updateChildrenInProgress)
            this._updateChildren(true);
    }

    get expandedChildCount()
    {
        var count = this.children.length;
        if (count && this.children[count - 1]._elementCloseTag)
            count--;
        if (count && this.children[count - 1].expandAllButton)
            count--;
        return count;
    }

    nodeStateChanged(change)
    {
        if (!change)
            return;

        this._nodeStateChanges.push(change);
    }

    showChildNode(node)
    {
        console.assert(!this._elementCloseTag);
        if (this._elementCloseTag)
            return null;

        var index = this._visibleChildren().indexOf(node);
        if (index === -1)
            return null;

        if (index >= this.expandedChildrenLimit) {
            this._expandedChildrenLimit = index + 1;
            this._updateChildren(true);
        }

        return this.children[index];
    }

    _createTooltipForNode()
    {
        var node = this.representedObject;
        if (!node.nodeName() || node.nodeName().toLowerCase() !== "img")
            return;

        function setTooltip(error, result, wasThrown)
        {
            if (error || wasThrown || !result || result.type !== "string")
                return;

            try {
                var properties = JSON.parse(result.description);
                var offsetWidth = properties[0];
                var offsetHeight = properties[1];
                var naturalWidth = properties[2];
                var naturalHeight = properties[3];
                if (offsetHeight === naturalHeight && offsetWidth === naturalWidth)
                    this.tooltip = WebInspector.UIString("%d \xd7 %d pixels").format(offsetWidth, offsetHeight);
                else
                    this.tooltip = WebInspector.UIString("%d \xd7 %d pixels (Natural: %d \xd7 %d pixels)").format(offsetWidth, offsetHeight, naturalWidth, naturalHeight);
            } catch (e) {
                console.error(e);
            }
        }

        function resolvedNode(object)
        {
            if (!object)
                return;

            function dimensions()
            {
                return "[" + this.offsetWidth + "," + this.offsetHeight + "," + this.naturalWidth + "," + this.naturalHeight + "]";
            }

            object.callFunction(dimensions, undefined, false, setTooltip.bind(this));
            object.release();
        }
        WebInspector.RemoteObject.resolveNode(node, "", resolvedNode.bind(this));
    }

    updateSelection()
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
    }

    onattach()
    {
        if (this._hovered) {
            this.updateSelection();
            this.listItemElement.classList.add("hovered");
        }

        this.updateTitle();

        if (this.editable) {
            this.listItemElement.draggable = true;
            this.listItemElement.addEventListener("dragstart", this);
        }
    }

    onpopulate()
    {
        if (this.children.length || !this._hasVisibleChildren() || this._elementCloseTag)
            return;

        this.updateChildren();
    }

    expandRecursively()
    {
        this.representedObject.getSubtree(-1, super.expandRecursively.bind(this, Number.MAX_VALUE));
    }

    updateChildren(fullRefresh)
    {
        if (this._elementCloseTag)
            return;

        this.representedObject.getChildNodes(this._updateChildren.bind(this, fullRefresh));
    }

    insertChildElement(child, index, closingTag)
    {
        var newElement = new WebInspector.DOMTreeElement(child, closingTag);
        newElement.selectable = this.treeOutline._selectEnabled;
        this.insertChild(newElement, index);
        return newElement;
    }

    moveChild(child, targetIndex)
    {
        var wasSelected = child.selected;
        this.removeChild(child);
        this.insertChild(child, targetIndex);
        if (wasSelected)
            child.select();
    }

    _updateChildren(fullRefresh)
    {
        if (this._updateChildrenInProgress || !this.treeOutline._visible)
            return;

        this._updateChildrenInProgress = true;

        var node = this.representedObject;
        var selectedNode = this.treeOutline.selectedDOMNode();
        var originalScrollTop = 0;

        var hasVisibleChildren = this._hasVisibleChildren();

        if (fullRefresh || !hasVisibleChildren) {
            var treeOutlineContainerElement = this.treeOutline.element.parentNode;
            originalScrollTop = treeOutlineContainerElement.scrollTop;
            var selectedTreeElement = this.treeOutline.selectedTreeElement;
            if (selectedTreeElement && selectedTreeElement.hasAncestor(this))
                this.select();
            this.removeChildren();

            // No longer have children.
            if (!hasVisibleChildren) {
                this.hasChildren = false;
                this.updateTitle();
                this._updateChildrenInProgress = false;
                return;
            }
        }

        // We now have children.
        if (!this.hasChildren) {
            this.hasChildren = true;
            this.updateTitle();
        }

        // Remove any tree elements that no longer have this node (or this node's contentDocument) as their parent.
        // Keep a list of existing tree elements for nodes that we can use later.
        var existingChildTreeElements = new Map;
        for (var i = (this.children.length - 1); i >= 0; --i) {
            var currentChildTreeElement = this.children[i];
            var currentNode = currentChildTreeElement.representedObject;
            var currentParentNode = currentNode.parentNode;
            if (currentParentNode === node) {
                existingChildTreeElements.set(currentNode, currentChildTreeElement);
                continue;
            }

            var selectedTreeElement = this.treeOutline.selectedTreeElement;
            if (selectedTreeElement && (selectedTreeElement === currentChildTreeElement || selectedTreeElement.hasAncestor(currentChildTreeElement)))
                this.select();

            this.removeChildAtIndex(i);
        }

        // Move / create TreeElements for our visible children.
        var childIndex = 0;
        var elementToSelect = null;
        var visibleChildren = this._visibleChildren();
        for (var i = 0; i < visibleChildren.length && i < this.expandedChildrenLimit; ++i) {
            var childNode = visibleChildren[i];

            // Already have a tree element for this child, just move it.
            var existingChildTreeElement = existingChildTreeElements.get(childNode);
            if (existingChildTreeElement) {
                this.moveChild(existingChildTreeElement, i);
                continue;
            }

            // No existing tree element for this child. Insert a new element.
            var newChildTreeElement = this.insertChildElement(childNode, i);

            // Update state.
            if (childNode === selectedNode)
                elementToSelect = newChildTreeElement;
            if (this.expandedChildCount > this.expandedChildrenLimit)
                this.expandedChildrenLimit++;
        }

        // Update expand all children button.
        this.adjustCollapsedRange();

        // Insert closing tag tree element.
        var lastChild = this.children.lastValue;
        if (node.nodeType() === Node.ELEMENT_NODE && (!lastChild || !lastChild._elementCloseTag))
            this.insertChildElement(this.representedObject, this.children.length, true);

        // We want to restore the original selection and tree scroll position after a full refresh, if possible.
        if (fullRefresh && elementToSelect) {
            elementToSelect.select();
            if (treeOutlineContainerElement && originalScrollTop <= treeOutlineContainerElement.scrollHeight)
                treeOutlineContainerElement.scrollTop = originalScrollTop;
        }

        this._updateChildrenInProgress = false;
    }

    adjustCollapsedRange()
    {
        // Ensure precondition: only the tree elements for node children are found in the tree
        // (not the Expand All button or the closing tag).
        if (this.expandAllButtonElement && this.expandAllButtonElement.__treeElement.parent)
            this.removeChild(this.expandAllButtonElement.__treeElement);

        var node = this.representedObject;
        if (!this._hasVisibleChildren())
            return;

        var visibleChildren = this._visibleChildren();
        var totalChildrenCount = visibleChildren.length;

        // In case some nodes from the expanded range were removed, pull some nodes from the collapsed range into the expanded range at the bottom.
        for (var i = this.expandedChildCount, limit = Math.min(this.expandedChildrenLimit, totalChildrenCount); i < limit; ++i)
            this.insertChildElement(totalChildrenCount[i], i);

        var expandedChildCount = this.expandedChildCount;
        if (totalChildrenCount > this.expandedChildCount) {
            var targetButtonIndex = expandedChildCount;
            if (!this.expandAllButtonElement) {
                var button = document.createElement("button");
                button.className = "show-all-nodes";
                button.value = "";

                var item = new WebInspector.TreeElement(button, null, false);
                item.selectable = false;
                item.expandAllButton = true;

                this.insertChild(item, targetButtonIndex);
                this.expandAllButtonElement = button;
                this.expandAllButtonElement.__treeElement = item;
                this.expandAllButtonElement.addEventListener("click", this.handleLoadAllChildren.bind(this), false);
            } else if (!this.expandAllButtonElement.__treeElement.parent)
                this.insertChild(this.expandAllButtonElement.__treeElement, targetButtonIndex);

            this.expandAllButtonElement.textContent = WebInspector.UIString("Show All Nodes (%d More)").format(totalChildrenCount - expandedChildCount);
        } else if (this.expandAllButtonElement)
            this.expandAllButtonElement = null;
    }

    handleLoadAllChildren()
    {
        var visibleChildren = this._visibleChildren();
        var totalChildrenCount = visibleChildren.length;
        this.expandedChildrenLimit = Math.max(visibleChildren.length, this.expandedChildrenLimit + WebInspector.DOMTreeElement.InitialChildrenLimit);
    }

    onexpand()
    {
        if (this._elementCloseTag)
            return;

        this.updateTitle();
        this.treeOutline.updateSelection();
    }

    oncollapse()
    {
        if (this._elementCloseTag)
            return;

        this.updateTitle();
        this.treeOutline.updateSelection();
    }

    onreveal()
    {
        if (this.listItemElement) {
            var tagSpans = this.listItemElement.getElementsByClassName("html-tag-name");
            if (tagSpans.length)
                tagSpans[0].scrollIntoViewIfNeeded(false);
            else
                this.listItemElement.scrollIntoViewIfNeeded(false);
        }
    }

    onselect(treeElement, selectedByUser)
    {
        this.treeOutline.suppressRevealAndSelect = true;
        this.treeOutline.selectDOMNode(this.representedObject, selectedByUser);
        if (selectedByUser)
            WebInspector.domTreeManager.highlightDOMNode(this.representedObject.id);
        this.updateSelection();
        this.treeOutline.suppressRevealAndSelect = false;
    }

    ondeselect(treeElement)
    {
        this.treeOutline.selectDOMNode(null);
    }

    ondelete()
    {
        if (!this.editable)
            return false;

        var startTagTreeElement = this.treeOutline.findTreeElement(this.representedObject);
        if (startTagTreeElement)
            startTagTreeElement.remove();
        else
            this.remove();
        return true;
    }

    onenter()
    {
        if (!this.editable)
            return false;

        // On Enter or Return start editing the first attribute
        // or create a new attribute on the selected element.
        if (this.treeOutline.editing)
            return false;

        this._startEditing();

        // prevent a newline from being immediately inserted
        return true;
    }

    selectOnMouseDown(event)
    {
        super.selectOnMouseDown(event);

        if (this._editing)
            return;

        // Prevent selecting the nearest word on double click.
        if (event.detail >= 2)
            event.preventDefault();
    }

    ondblclick(event)
    {
        if (!this.editable)
            return false;

        if (this._editing || this._elementCloseTag)
            return;

        if (this._startEditingTarget(event.target))
            return;

        if (this.hasChildren && !this.expanded)
            this.expand();
    }

    _insertInLastAttributePosition(tag, node)
    {
        if (tag.getElementsByClassName("html-attribute").length > 0)
            tag.insertBefore(node, tag.lastChild);
        else {
            var nodeName = tag.textContent.match(/^<(.*?)>$/)[1];
            tag.textContent = "";
            tag.append("<" + nodeName, node, ">");
        }

        this.updateSelection();
    }

    _startEditingTarget(eventTarget)
    {
        if (this.treeOutline.selectedDOMNode() !== this.representedObject)
            return false;

        if (this.representedObject.isInShadowTree() || this.representedObject.isPseudoElement())
            return false;

        if (this.representedObject.nodeType() !== Node.ELEMENT_NODE && this.representedObject.nodeType() !== Node.TEXT_NODE)
            return false;

        var textNode = eventTarget.enclosingNodeOrSelfWithClass("html-text-node");
        if (textNode)
            return this._startEditingTextNode(textNode);

        var attribute = eventTarget.enclosingNodeOrSelfWithClass("html-attribute");
        if (attribute)
            return this._startEditingAttribute(attribute, eventTarget);

        var tagName = eventTarget.enclosingNodeOrSelfWithClass("html-tag-name");
        if (tagName)
            return this._startEditingTagName(tagName);

        return false;
    }

    _populateTagContextMenu(contextMenu, event)
    {
        var node = this.representedObject;
        if (!node.isInShadowTree()) {
            var attribute = event.target.enclosingNodeOrSelfWithClass("html-attribute");

            // Add attribute-related actions.
            if (this.editable) {
                contextMenu.appendItem(WebInspector.UIString("Add Attribute"), this._addNewAttribute.bind(this));
                if (attribute)
                    contextMenu.appendItem(WebInspector.UIString("Edit Attribute"), this._startEditingAttribute.bind(this, attribute, event.target));
                contextMenu.appendSeparator();
            }

            if (WebInspector.cssStyleManager.canForcePseudoClasses()) {
                var pseudoSubMenu = contextMenu.appendSubMenuItem(WebInspector.UIString("Forced Pseudo-Classes"));
                this._populateForcedPseudoStateItems(pseudoSubMenu);
                contextMenu.appendSeparator();
            }
        }

        this._populateNodeContextMenu(contextMenu);
        this.treeOutline._populateContextMenu(contextMenu, this.representedObject);
    }

    _populateForcedPseudoStateItems(subMenu)
    {
        var node = this.representedObject;
        var enabledPseudoClasses = node.enabledPseudoClasses;
        // These strings don't need to be localized as they are CSS pseudo-classes.
        WebInspector.CSSStyleManager.ForceablePseudoClasses.forEach(function(pseudoClass) {
            var label = pseudoClass.capitalize();
            var enabled = enabledPseudoClasses.includes(pseudoClass);
            subMenu.appendCheckboxItem(label, function() {
                node.setPseudoClassEnabled(pseudoClass, !enabled);
            }, enabled, false);
        });
    }

    _populateTextContextMenu(contextMenu, textNode)
    {
        if (this.editable)
            contextMenu.appendItem(WebInspector.UIString("Edit Text"), this._startEditingTextNode.bind(this, textNode));

        this._populateNodeContextMenu(contextMenu);
    }

    _populateNodeContextMenu(contextMenu)
    {
        // Add free-form node-related actions.
        if (this.editable)
            contextMenu.appendItem(WebInspector.UIString("Edit as HTML"), this._editAsHTML.bind(this));
        contextMenu.appendItem(WebInspector.UIString("Copy as HTML"), this._copyHTML.bind(this));
        if (this.editable)
            contextMenu.appendItem(WebInspector.UIString("Delete Node"), this.remove.bind(this));

        let node = this.representedObject;
        if (node.nodeType() === Node.ELEMENT_NODE)
            contextMenu.appendItem(WebInspector.UIString("Scroll Into View"), this._scrollIntoView.bind(this));
    }

    _startEditing()
    {
        if (this.treeOutline.selectedDOMNode() !== this.representedObject)
            return false;

        if (!this.editable)
            return false;

        var listItem = this._listItemNode;

        if (this._canAddAttributes) {
            var attribute = listItem.getElementsByClassName("html-attribute")[0];
            if (attribute)
                return this._startEditingAttribute(attribute, attribute.getElementsByClassName("html-attribute-value")[0]);

            return this._addNewAttribute();
        }

        if (this.representedObject.nodeType() === Node.TEXT_NODE) {
            var textNode = listItem.getElementsByClassName("html-text-node")[0];
            if (textNode)
                return this._startEditingTextNode(textNode);
            return false;
        }
    }

    _addNewAttribute()
    {
        // Cannot just convert the textual html into an element without
        // a parent node. Use a temporary span container for the HTML.
        var container = document.createElement("span");
        this._buildAttributeDOM(container, " ", "");
        var attr = container.firstChild;
        attr.style.marginLeft = "2px"; // overrides the .editing margin rule
        attr.style.marginRight = "2px"; // overrides the .editing margin rule

        var tag = this.listItemElement.getElementsByClassName("html-tag")[0];
        this._insertInLastAttributePosition(tag, attr);
        return this._startEditingAttribute(attr, attr);
    }

    _triggerEditAttribute(attributeName)
    {
        var attributeElements = this.listItemElement.getElementsByClassName("html-attribute-name");
        for (var i = 0, len = attributeElements.length; i < len; ++i) {
            if (attributeElements[i].textContent === attributeName) {
                for (var elem = attributeElements[i].nextSibling; elem; elem = elem.nextSibling) {
                    if (elem.nodeType !== Node.ELEMENT_NODE)
                        continue;

                    if (elem.classList.contains("html-attribute-value"))
                        return this._startEditingAttribute(elem.parentNode, elem);
                }
            }
        }
    }

    _startEditingAttribute(attribute, elementForSelection)
    {
        if (WebInspector.isBeingEdited(attribute))
            return true;

        var attributeNameElement = attribute.getElementsByClassName("html-attribute-name")[0];
        if (!attributeNameElement)
            return false;

        var attributeName = attributeNameElement.textContent;

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

        var config = new WebInspector.EditingConfig(this._attributeEditingCommitted.bind(this), this._editingCancelled.bind(this), attributeName);
        config.setNumberCommitHandler(this._attributeNumberEditingCommitted.bind(this));
        this._editing = WebInspector.startEditing(attribute, config);

        window.getSelection().setBaseAndExtent(elementForSelection, 0, elementForSelection, 1);

        return true;
    }

    _startEditingTextNode(textNode)
    {
        if (WebInspector.isBeingEdited(textNode))
            return true;

        var config = new WebInspector.EditingConfig(this._textNodeEditingCommitted.bind(this), this._editingCancelled.bind(this));
        config.spellcheck = true;
        this._editing = WebInspector.startEditing(textNode, config);
        window.getSelection().setBaseAndExtent(textNode, 0, textNode, 1);

        return true;
    }

    _startEditingTagName(tagNameElement)
    {
        if (!tagNameElement) {
            tagNameElement = this.listItemElement.getElementsByClassName("html-tag-name")[0];
            if (!tagNameElement)
                return false;
        }

        var tagName = tagNameElement.textContent;
        if (WebInspector.DOMTreeElement.EditTagBlacklist[tagName.toLowerCase()])
            return false;

        if (WebInspector.isBeingEdited(tagNameElement))
            return true;

        let closingTagElement = this._distinctClosingTagElement();
        let originalClosingTagTextContent = closingTagElement ? closingTagElement.textContent : "";

        function keyupListener(event)
        {
            if (closingTagElement)
                closingTagElement.textContent = "</" + tagNameElement.textContent + ">";
        }

        function editingComitted(element, newTagName)
        {
            tagNameElement.removeEventListener("keyup", keyupListener, false);
            this._tagNameEditingCommitted.apply(this, arguments);
        }

        function editingCancelled()
        {
            if (closingTagElement)
                closingTagElement.textContent = originalClosingTagTextContent;

            tagNameElement.removeEventListener("keyup", keyupListener, false);
            this._editingCancelled.apply(this, arguments);
        }

        tagNameElement.addEventListener("keyup", keyupListener, false);

        var config = new WebInspector.EditingConfig(editingComitted.bind(this), editingCancelled.bind(this), tagName);
        this._editing = WebInspector.startEditing(tagNameElement, config);
        window.getSelection().setBaseAndExtent(tagNameElement, 0, tagNameElement, 1);
        return true;
    }

    _startEditingAsHTML(commitCallback, error, initialValue)
    {
        if (error)
            return;
        if (this._htmlEditElement && WebInspector.isBeingEdited(this._htmlEditElement))
            return;

        this._htmlEditElement = document.createElement("div");
        this._htmlEditElement.textContent = initialValue;

        // Hide header items.
        var child = this.listItemElement.firstChild;
        while (child) {
            child.style.display = "none";
            child = child.nextSibling;
        }
        // Hide children item.
        if (this._childrenListNode)
            this._childrenListNode.style.display = "none";
        // Append editor.
        this.listItemElement.appendChild(this._htmlEditElement);

        this.updateSelection();

        function commit()
        {
            commitCallback(this._htmlEditElement.textContent);
            dispose.call(this);
        }

        function dispose()
        {
            this._editing = false;

            // Remove editor.
            this.listItemElement.removeChild(this._htmlEditElement);
            this._htmlEditElement = null;
            // Unhide children item.
            if (this._childrenListNode)
                this._childrenListNode.style.removeProperty("display");
            // Unhide header items.
            var child = this.listItemElement.firstChild;
            while (child) {
                child.style.removeProperty("display");
                child = child.nextSibling;
            }

            this.updateSelection();
        }

        var config = new WebInspector.EditingConfig(commit.bind(this), dispose.bind(this));
        config.setMultiline(true);
        this._editing = WebInspector.startEditing(this._htmlEditElement, config);
    }

    _attributeEditingCommitted(element, newText, oldText, attributeName, moveDirection)
    {
        this._editing = false;

        if (newText === oldText)
            return;

        var treeOutline = this.treeOutline;
        function moveToNextAttributeIfNeeded(error)
        {
            if (error)
                this._editingCancelled(element, attributeName);

            if (!moveDirection)
                return;

            treeOutline._updateModifiedNodes();

            // Search for the attribute's position, and then decide where to move to.
            var attributes = this.representedObject.attributes();
            for (var i = 0; i < attributes.length; ++i) {
                if (attributes[i].name !== attributeName)
                    continue;

                if (moveDirection === "backward") {
                    if (i === 0)
                        this._startEditingTagName();
                    else
                        this._triggerEditAttribute(attributes[i - 1].name);
                } else {
                    if (i === attributes.length - 1)
                        this._addNewAttribute();
                    else
                        this._triggerEditAttribute(attributes[i + 1].name);
                }
                return;
            }

            // Moving From the "New Attribute" position.
            if (moveDirection === "backward") {
                if (newText === " ") {
                    // Moving from "New Attribute" that was not edited
                    if (attributes.length)
                        this._triggerEditAttribute(attributes.lastValue.name);
                } else {
                    // Moving from "New Attribute" that holds new value
                    if (attributes.length > 1)
                        this._triggerEditAttribute(attributes[attributes.length - 2].name);
                }
            } else if (moveDirection === "forward") {
                if (!/^\s*$/.test(newText))
                    this._addNewAttribute();
                else
                    this._startEditingTagName();
            }
        }

        this.representedObject.setAttribute(attributeName, newText, moveToNextAttributeIfNeeded.bind(this));
    }

    _attributeNumberEditingCommitted(element, newText, oldText, attributeName, moveDirection)
    {
        if (newText === oldText)
            return;

        this.representedObject.setAttribute(attributeName, newText);
    }

    _tagNameEditingCommitted(element, newText, oldText, tagName, moveDirection)
    {
        this._editing = false;
        var self = this;

        function cancel()
        {
            var closingTagElement = self._distinctClosingTagElement();
            if (closingTagElement)
                closingTagElement.textContent = "</" + tagName + ">";

            self._editingCancelled(element, tagName);
            moveToNextAttributeIfNeeded.call(self);
        }

        function moveToNextAttributeIfNeeded()
        {
            if (moveDirection !== "forward") {
                this._addNewAttribute();
                return;
            }

            var attributes = this.representedObject.attributes();
            if (attributes.length > 0)
                this._triggerEditAttribute(attributes[0].name);
            else
                this._addNewAttribute();
        }

        newText = newText.trim();
        if (newText === oldText) {
            cancel();
            return;
        }

        var treeOutline = this.treeOutline;
        var wasExpanded = this.expanded;

        function changeTagNameCallback(error, nodeId)
        {
            if (error || !nodeId) {
                cancel();
                return;
            }

            var node = WebInspector.domTreeManager.nodeForId(nodeId);

            // Select it and expand if necessary. We force tree update so that it processes dom events and is up to date.
            treeOutline._updateModifiedNodes();
            treeOutline.selectDOMNode(node, true);

            var newTreeItem = treeOutline.findTreeElement(node);
            if (wasExpanded)
                newTreeItem.expand();

            moveToNextAttributeIfNeeded.call(newTreeItem);
        }

        this.representedObject.setNodeName(newText, changeTagNameCallback);
    }

    _textNodeEditingCommitted(element, newText)
    {
        this._editing = false;

        var textNode;
        if (this.representedObject.nodeType() === Node.ELEMENT_NODE) {
            // We only show text nodes inline in elements if the element only
            // has a single child, and that child is a text node.
            textNode = this.representedObject.firstChild;
        } else if (this.representedObject.nodeType() === Node.TEXT_NODE)
            textNode = this.representedObject;

        textNode.setNodeValue(newText, this.updateTitle.bind(this));
    }

    _editingCancelled(element, context)
    {
        this._editing = false;

        // Need to restore attributes structure.
        this.updateTitle();
    }

    _distinctClosingTagElement()
    {
        // FIXME: Improve the Tree Element / Outline Abstraction to prevent crawling the DOM

        // For an expanded element, it will be the last element with class "close"
        // in the child element list.
        if (this.expanded) {
            var closers = this._childrenListNode.querySelectorAll(".close");
            return closers[closers.length - 1];
        }

        // Remaining cases are single line non-expanded elements with a closing
        // tag, or HTML elements without a closing tag (such as <br>). Return
        // null in the case where there isn't a closing tag.
        var tags = this.listItemElement.getElementsByClassName("html-tag");
        return (tags.length === 1 ? null : tags[tags.length - 1]);
    }

    updateTitle(onlySearchQueryChanged)
    {
        // If we are editing, return early to prevent canceling the edit.
        // After editing is committed updateTitle will be called.
        if (this._editing)
            return;

        if (onlySearchQueryChanged) {
            if (this._highlightResult)
                this._updateSearchHighlight(false);
        } else {
            this.title = document.createElement("span");
            this.title.appendChild(this._nodeTitleInfo().titleDOM);
            this._highlightResult = undefined;
        }

        this.selectionElement = null;
        this.updateSelection();
        this._highlightSearchResults();
    }

    _buildAttributeDOM(parentElement, name, value, node)
    {
        let hasText = (value.length > 0);
        let attrSpanElement = parentElement.createChild("span", "html-attribute");
        let attrNameElement = attrSpanElement.createChild("span", "html-attribute-name");
        attrNameElement.textContent = name;
        let attrValueElement = null;
        if (hasText)
            attrSpanElement.append("=\u200B\"");

        if (name === "src" || /\bhref\b/.test(name)) {
            let baseURL = node.ownerDocument ? node.ownerDocument.documentURL : null;
            let rewrittenURL = absoluteURL(value, baseURL);
            value = value.insertWordBreakCharacters();
            if (!rewrittenURL) {
                attrValueElement = attrSpanElement.createChild("span", "html-attribute-value");
                attrValueElement.textContent = value;
            } else {
                if (value.startsWith("data:"))
                    value = value.trimMiddle(60);

                attrValueElement = document.createElement("a");
                attrValueElement.href = rewrittenURL;
                attrValueElement.textContent = value;
                attrSpanElement.appendChild(attrValueElement);
            }
        } else if (name === "srcset") {
            let baseURL = node.ownerDocument ? node.ownerDocument.documentURL : null;
            attrValueElement = attrSpanElement.createChild("span", "html-attribute-value");

            // Leading whitespace.
            let groups = value.split(/\s*,\s*/);
            for (let i = 0; i < groups.length; ++i) {
                let string = groups[i].trim();
                let spaceIndex = string.search(/\s/);

                if (spaceIndex === -1) {
                    let linkText = string;
                    let rewrittenURL = absoluteURL(string, baseURL);
                    let linkElement = attrValueElement.appendChild(document.createElement("a"));
                    linkElement.href = rewrittenURL;
                    linkElement.textContent = linkText.insertWordBreakCharacters();
                } else {
                    let linkText = string.substring(0, spaceIndex);
                    let descriptorText = string.substring(spaceIndex).insertWordBreakCharacters();
                    let rewrittenURL = absoluteURL(linkText, baseURL);
                    let linkElement = attrValueElement.appendChild(document.createElement("a"));
                    linkElement.href = rewrittenURL;
                    linkElement.textContent = linkText.insertWordBreakCharacters();
                    let descriptorElement = attrValueElement.appendChild(document.createElement("span"));
                    descriptorElement.textContent = string.substring(spaceIndex);
                }

                if (i < groups.length - 1) {
                    let commaElement = attrValueElement.appendChild(document.createElement("span"));
                    commaElement.textContent = ", ";
                }
            }
        } else {
            value = value.insertWordBreakCharacters();
            attrValueElement = attrSpanElement.createChild("span", "html-attribute-value");
            attrValueElement.textContent = value;
        }

        if (hasText)
            attrSpanElement.append("\"");

        for (let change of this._nodeStateChanges) {
            if (change.type === WebInspector.DOMTreeElement.ChangeType.Attribute && change.attribute === name)
                change.element = hasText ? attrValueElement : attrNameElement;
        }
    }

    _buildTagDOM(parentElement, tagName, isClosingTag, isDistinctTreeElement)
    {
        var node = this.representedObject;
        var classes = [ "html-tag" ];
        if (isClosingTag && isDistinctTreeElement)
            classes.push("close");
        if (node.isInShadowTree())
            classes.push("shadow");
        var tagElement = parentElement.createChild("span", classes.join(" "));
        tagElement.append("<");
        var tagNameElement = tagElement.createChild("span", isClosingTag ? "" : "html-tag-name");
        tagNameElement.textContent = (isClosingTag ? "/" : "") + tagName;
        if (!isClosingTag && node.hasAttributes()) {
            var attributes = node.attributes();
            for (var i = 0; i < attributes.length; ++i) {
                var attr = attributes[i];
                tagElement.append(" ");
                this._buildAttributeDOM(tagElement, attr.name, attr.value, node);
            }
        }
        tagElement.append(">");
        parentElement.append("\u200B");
    }

    _nodeTitleInfo()
    {
        var node = this.representedObject;
        var info = {titleDOM: document.createDocumentFragment(), hasChildren: this.hasChildren};

        function trimedNodeValue()
        {
            // Trim empty lines from the beginning and extra space at the end since most style and script tags begin with a newline
            // and end with a newline and indentation for the end tag.
            return node.nodeValue().replace(/^[\n\r]*/, "").replace(/\s*$/, "");
        }

        switch (node.nodeType()) {
            case Node.DOCUMENT_FRAGMENT_NODE:
                var fragmentElement = info.titleDOM.createChild("span", "html-fragment");
                if (node.isInShadowTree()) {
                    fragmentElement.textContent = WebInspector.UIString("Shadow Content");
                    fragmentElement.classList.add("shadow");
                } else
                    fragmentElement.textContent = WebInspector.UIString("Document Fragment");
                break;

            case Node.ATTRIBUTE_NODE:
                var value = node.value || "\u200B"; // Zero width space to force showing an empty value.
                this._buildAttributeDOM(info.titleDOM, node.name, value);
                break;

            case Node.ELEMENT_NODE:
                if (node.isPseudoElement()) {
                    var pseudoElement = info.titleDOM.createChild("span", "html-pseudo-element");
                    pseudoElement.textContent = "::" + node.pseudoType();
                    info.titleDOM.appendChild(document.createTextNode("\u200B"));
                    info.hasChildren = false;
                    break;
                }

                var tagName = node.nodeNameInCorrectCase();
                if (this._elementCloseTag) {
                    this._buildTagDOM(info.titleDOM, tagName, true, true);
                    info.hasChildren = false;
                    break;
                }

                this._buildTagDOM(info.titleDOM, tagName, false, false);

                var textChild = this._singleTextChild(node);
                var showInlineText = textChild && textChild.nodeValue().length < WebInspector.DOMTreeElement.MaximumInlineTextChildLength;

                if (!this.expanded && (!showInlineText && (this.treeOutline.isXMLMimeType || !WebInspector.DOMTreeElement.ForbiddenClosingTagElements[tagName]))) {
                    if (this.hasChildren) {
                        var textNodeElement = info.titleDOM.createChild("span", "html-text-node");
                        textNodeElement.textContent = "\u2026";
                        info.titleDOM.append("\u200B");
                    }
                    this._buildTagDOM(info.titleDOM, tagName, true, false);
                }

                // If this element only has a single child that is a text node,
                // just show that text and the closing tag inline rather than
                // create a subtree for them
                if (showInlineText) {
                    var textNodeElement = info.titleDOM.createChild("span", "html-text-node");
                    var nodeNameLowerCase = node.nodeName().toLowerCase();

                    if (nodeNameLowerCase === "script")
                        textNodeElement.appendChild(WebInspector.syntaxHighlightStringAsDocumentFragment(textChild.nodeValue().trim(), "text/javascript"));
                    else if (nodeNameLowerCase === "style")
                        textNodeElement.appendChild(WebInspector.syntaxHighlightStringAsDocumentFragment(textChild.nodeValue().trim(), "text/css"));
                    else
                        textNodeElement.textContent = textChild.nodeValue();

                    info.titleDOM.append("\u200B");

                    this._buildTagDOM(info.titleDOM, tagName, true, false);
                    info.hasChildren = false;
                }
                break;

            case Node.TEXT_NODE:
                if (node.parentNode && node.parentNode.nodeName().toLowerCase() === "script") {
                    var newNode = info.titleDOM.createChild("span", "html-text-node large");
                    newNode.appendChild(WebInspector.syntaxHighlightStringAsDocumentFragment(trimedNodeValue(), "text/javascript"));
                } else if (node.parentNode && node.parentNode.nodeName().toLowerCase() === "style") {
                    var newNode = info.titleDOM.createChild("span", "html-text-node large");
                    newNode.appendChild(WebInspector.syntaxHighlightStringAsDocumentFragment(trimedNodeValue(), "text/css"));
                } else {
                    info.titleDOM.append("\"");
                    var textNodeElement = info.titleDOM.createChild("span", "html-text-node");
                    textNodeElement.textContent = node.nodeValue();
                    info.titleDOM.append("\"");
                }
                break;

            case Node.COMMENT_NODE:
                var commentElement = info.titleDOM.createChild("span", "html-comment");
                commentElement.append("<!--" + node.nodeValue() + "-->");
                break;

            case Node.DOCUMENT_TYPE_NODE:
                var docTypeElement = info.titleDOM.createChild("span", "html-doctype");
                docTypeElement.append("<!DOCTYPE " + node.nodeName());
                if (node.publicId) {
                    docTypeElement.append(" PUBLIC \"" + node.publicId + "\"");
                    if (node.systemId)
                        docTypeElement.append(" \"" + node.systemId + "\"");
                } else if (node.systemId)
                    docTypeElement.append(" SYSTEM \"" + node.systemId + "\"");

                if (node.internalSubset)
                    docTypeElement.append(" [" + node.internalSubset + "]");

                docTypeElement.append(">");
                break;

            case Node.CDATA_SECTION_NODE:
                var cdataElement = info.titleDOM.createChild("span", "html-text-node");
                cdataElement.append("<![CDATA[" + node.nodeValue() + "]]>");
                break;

            case Node.PROCESSING_INSTRUCTION_NODE:
                var processingInstructionElement = info.titleDOM.createChild("span", "html-processing-instruction");
                var data = node.nodeValue();
                var dataString = data.length ? " " + data : "";
                var title = "<?" + node.nodeNameInCorrectCase() + dataString + "?>";
                processingInstructionElement.append(title);
                break;

            default:
                info.titleDOM.append(node.nodeNameInCorrectCase().collapseWhitespace());
        }

        return info;
    }

    _singleTextChild(node)
    {
        if (!node)
            return null;

        var firstChild = node.firstChild;
        if (!firstChild || firstChild.nodeType() !== Node.TEXT_NODE)
            return null;

        if (node.hasShadowRoots())
            return null;
        if (node.templateContent())
            return null;
        if (node.hasPseudoElements())
            return null;

        var sibling = firstChild.nextSibling;
        return sibling ? null : firstChild;
    }

    _showInlineText(node)
    {
        if (node.nodeType() === Node.ELEMENT_NODE) {
            var textChild = this._singleTextChild(node);
            if (textChild && textChild.nodeValue().length < WebInspector.DOMTreeElement.MaximumInlineTextChildLength)
                return true;
        }
        return false;
    }

    _hasVisibleChildren()
    {
        var node = this.representedObject;

        if (this._showInlineText(node))
            return false;

        if (node.hasChildNodes())
            return true;
        if (node.templateContent())
            return true;
        if (node.hasPseudoElements())
            return true;

        return false;
    }

    _visibleChildren()
    {
        var node = this.representedObject;

        var visibleChildren = [];

        var templateContent = node.templateContent();
        if (templateContent)
            visibleChildren.push(templateContent);

        var beforePseudoElement = node.beforePseudoElement();
        if (beforePseudoElement)
            visibleChildren.push(beforePseudoElement);

        if (node.childNodeCount && node.children)
            visibleChildren = visibleChildren.concat(node.children);

        var afterPseudoElement = node.afterPseudoElement();
        if (afterPseudoElement)
            visibleChildren.push(afterPseudoElement);

        return visibleChildren;
    }

    remove()
    {
        var parentElement = this.parent;
        if (!parentElement)
            return;

        var self = this;
        function removeNodeCallback(error, removedNodeId)
        {
            if (error)
                return;

            if (!self.parent)
                return;

            parentElement.removeChild(self);
            parentElement.adjustCollapsedRange();
        }

        this.representedObject.removeNode(removeNodeCallback);
    }

    _scrollIntoView()
    {
        function resolvedNode(object)
        {
            if (!object)
                return;

            function scrollIntoView()
            {
                this.scrollIntoViewIfNeeded(true);
            }

            object.callFunction(scrollIntoView, undefined, false, function() {});
            object.release();
        }

        let node = this.representedObject;
        WebInspector.RemoteObject.resolveNode(node, "", resolvedNode);
    }

    _editAsHTML()
    {
        var treeOutline = this.treeOutline;
        var node = this.representedObject;
        var parentNode = node.parentNode;
        var index = node.index;
        var wasExpanded = this.expanded;

        function selectNode(error, nodeId)
        {
            if (error)
                return;

            // Select it and expand if necessary. We force tree update so that it processes dom events and is up to date.
            treeOutline._updateModifiedNodes();

            var newNode = parentNode ? parentNode.children[index] || parentNode : null;
            if (!newNode)
                return;

            treeOutline.selectDOMNode(newNode, true);

            if (wasExpanded) {
                var newTreeItem = treeOutline.findTreeElement(newNode);
                if (newTreeItem)
                    newTreeItem.expand();
            }
        }

        function commitChange(value)
        {
            node.setOuterHTML(value, selectNode);
        }

        node.getOuterHTML(this._startEditingAsHTML.bind(this, commitChange));
    }

    _copyHTML()
    {
        this.representedObject.copyNode();
    }

    _highlightSearchResults()
    {
        if (!this.title || !this._searchQuery || !this._searchHighlightsVisible)
            return;

        if (this._highlightResult) {
            this._updateSearchHighlight(true);
            return;
        }

        var text = this.title.textContent;
        var searchRegex = new RegExp(this._searchQuery.escapeForRegExp(), "gi");

        var offset = 0;
        var match = searchRegex.exec(text);
        var matchRanges = [];
        while (match) {
            matchRanges.push({ offset: match.index, length: match[0].length });
            match = searchRegex.exec(text);
        }

        // Fall back for XPath, etc. matches.
        if (!matchRanges.length)
            matchRanges.push({ offset: 0, length: text.length });

        this._highlightResult = [];
        WebInspector.highlightRangesWithStyleClass(this.title, matchRanges, WebInspector.DOMTreeElement.SearchHighlightStyleClassName, this._highlightResult);
    }

    _markNodeChanged()
    {
        for (let change of this._nodeStateChanges) {
            let element = change.element;
            if (!element)
                continue;

            element.classList.remove("node-state-changed");
            element.addEventListener("animationend", this._boundNodeChangedAnimationEnd);
            element.classList.add("node-state-changed");
        }
    }

    _nodeChangedAnimationEnd(event)
    {
        let element = event.target;
        element.classList.remove("node-state-changed");
        element.removeEventListener("animationend", this._boundNodeChangedAnimationEnd);

        for (let i = this._nodeStateChanges.length - 1; i >= 0; --i) {
            if (this._nodeStateChanges[i].element === element)
                this._nodeStateChanges.splice(i, 1);
        }
    }

    _nodePseudoClassesDidChange(event)
    {
        if (this._elementCloseTag)
            return;

        this._listItemNode.classList.toggle("pseudo-class-enabled", !!this.representedObject.enabledPseudoClasses.length);
    }

    _fireDidChange()
    {
        super._fireDidChange();

        if (this._nodeStateChanges)
            this._markNodeChanged();
    }

    handleEvent(event)
    {
        if (event.type === "dragstart" && this._editing)
            event.preventDefault();
    }
};

WebInspector.DOMTreeElement.InitialChildrenLimit = 500;
WebInspector.DOMTreeElement.MaximumInlineTextChildLength = 80;

// A union of HTML4 and HTML5-Draft elements that explicitly
// or implicitly (for HTML5) forbid the closing tag.
WebInspector.DOMTreeElement.ForbiddenClosingTagElements = [
    "area", "base", "basefont", "br", "canvas", "col", "command", "embed", "frame",
    "hr", "img", "input", "isindex", "keygen", "link", "meta", "param", "source",
    "wbr", "track", "menuitem"
].keySet();

// These tags we do not allow editing their tag name.
WebInspector.DOMTreeElement.EditTagBlacklist = [
    "html", "head", "body"
].keySet();

WebInspector.DOMTreeElement.ChangeType = {
    Attribute: "dom-tree-element-change-type-attribute"
};

WebInspector.DOMTreeElement.SearchHighlightStyleClassName = "search-highlight";
WebInspector.DOMTreeElement.BouncyHighlightStyleClassName = "bouncy-highlight";
