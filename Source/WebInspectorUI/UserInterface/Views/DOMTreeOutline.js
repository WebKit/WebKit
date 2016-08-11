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

WebInspector.DOMTreeOutline = class DOMTreeOutline extends WebInspector.TreeOutline
{
    constructor(omitRootDOMNode, selectEnabled, excludeRevealElementContextMenu)
    {
        super();

        this.element.addEventListener("mousedown", this._onmousedown.bind(this), false);
        this.element.addEventListener("mousemove", this._onmousemove.bind(this), false);
        this.element.addEventListener("mouseout", this._onmouseout.bind(this), false);
        this.element.addEventListener("dragstart", this._ondragstart.bind(this), false);
        this.element.addEventListener("dragover", this._ondragover.bind(this), false);
        this.element.addEventListener("dragleave", this._ondragleave.bind(this), false);
        this.element.addEventListener("drop", this._ondrop.bind(this), false);
        this.element.addEventListener("dragend", this._ondragend.bind(this), false);

        this.element.classList.add("dom", WebInspector.SyntaxHighlightedStyleClassName);

        this._includeRootDOMNode = !omitRootDOMNode;
        this._selectEnabled = selectEnabled;
        this._excludeRevealElementContextMenu = excludeRevealElementContextMenu;
        this._rootDOMNode = null;
        this._selectedDOMNode = null;

        this._editable = false;
        this._editing = false;
        this._visible = false;

        this.element.addEventListener("contextmenu", this._contextMenuEventFired.bind(this));

        this._hideElementKeyboardShortcut = new WebInspector.KeyboardShortcut(null, "H", this._hideElement.bind(this), this.element);
        this._hideElementKeyboardShortcut.implicitlyPreventsDefault = false;

        WebInspector.showShadowDOMSetting.addEventListener(WebInspector.Setting.Event.Changed, this._showShadowDOMSettingChanged, this);
    }

    // Public

    wireToDomAgent()
    {
        this._elementsTreeUpdater = new WebInspector.DOMTreeUpdater(this);
    }

    close()
    {
        WebInspector.showShadowDOMSetting.removeEventListener(null, null, this);

        if (this._elementsTreeUpdater) {
            this._elementsTreeUpdater.close();
            this._elementsTreeUpdater = null;
        }
    }

    setVisible(visible, omitFocus)
    {
        this._visible = visible;
        if (!this._visible)
            return;

        this._updateModifiedNodes();

        if (this._selectedDOMNode)
            this._revealAndSelectNode(this._selectedDOMNode, omitFocus);

        this.update();
    }

    get rootDOMNode()
    {
        return this._rootDOMNode;
    }

    set rootDOMNode(x)
    {
        if (this._rootDOMNode === x)
            return;

        this._rootDOMNode = x;

        this._isXMLMimeType = x && x.isXMLNode();

        this.update();
    }

    get isXMLMimeType()
    {
        return this._isXMLMimeType;
    }

    selectedDOMNode()
    {
        return this._selectedDOMNode;
    }

    selectDOMNode(node, focus)
    {
        if (this._selectedDOMNode === node) {
            this._revealAndSelectNode(node, !focus);
            return;
        }

        this._selectedDOMNode = node;
        this._revealAndSelectNode(node, !focus);

        // The _revealAndSelectNode() method might find a different element if there is inlined text,
        // and the select() call would change the selectedDOMNode and reenter this setter. So to
        // avoid calling _selectedNodeChanged() twice, first check if _selectedDOMNode is the same
        // node as the one passed in.
        // Note that _revealAndSelectNode will not do anything for a null node.
        if (!node || this._selectedDOMNode === node)
            this._selectedNodeChanged();
    }

    get editable()
    {
        return this._editable;
    }

    set editable(x)
    {
        this._editable = x;
    }

    get editing()
    {
        return this._editing;
    }

    update()
    {
        var selectedNode = this.selectedTreeElement ? this.selectedTreeElement.representedObject : null;

        if (!this.rootDOMNode)
            return;

        this.removeChildren();

        var treeElement;
        if (this._includeRootDOMNode) {
            treeElement = new WebInspector.DOMTreeElement(this.rootDOMNode);
            treeElement.selectable = this._selectEnabled;
            this.appendChild(treeElement);
        } else {
            // FIXME: this could use findTreeElement to reuse a tree element if it already exists
            var node = this.rootDOMNode.firstChild;
            while (node) {
                treeElement = new WebInspector.DOMTreeElement(node);
                treeElement.selectable = this._selectEnabled;
                this.appendChild(treeElement);
                node = node.nextSibling;

                if (treeElement.hasChildren && !treeElement.expanded)
                    treeElement.expand();
            }
        }

        if (selectedNode)
            this._revealAndSelectNode(selectedNode, true);
    }

    updateSelection()
    {
        // This will miss updating selection areas used for the hovered tree element and
        // and those used to show forced pseudo class indicators, but this should be okay.
        // The hovered element will update when user moves the mouse, and indicators don't need the
        // selection area height to be accurate since they use ::before to place the indicator.
        if (this.selectedTreeElement)
            this.selectedTreeElement.updateSelectionArea();
    }

    _selectedNodeChanged()
    {
        this.dispatchEventToListeners(WebInspector.DOMTreeOutline.Event.SelectedNodeChanged);
    }

    findTreeElement(node)
    {
        let isAncestorNode = (ancestor, node) => ancestor.isAncestor(node);
        let parentNode = (node) => node.parentNode;
        let treeElement = super.findTreeElement(node, isAncestorNode, parentNode);
        if (!treeElement && node.nodeType() === Node.TEXT_NODE) {
            // The text node might have been inlined if it was short, so try to find the parent element.
            treeElement = super.findTreeElement(node.parentNode, isAncestorNode, parentNode);
        }

        return treeElement;
    }

    createTreeElementFor(node)
    {
        var treeElement = this.findTreeElement(node);
        if (treeElement)
            return treeElement;

        if (!node.parentNode)
            return null;

        treeElement = this.createTreeElementFor(node.parentNode);
        if (!treeElement)
            return null;

        return treeElement.showChildNode(node);
    }

    set suppressRevealAndSelect(x)
    {
        if (this._suppressRevealAndSelect === x)
            return;
        this._suppressRevealAndSelect = x;
    }

    populateContextMenu(contextMenu, event, treeElement)
    {
        var tag = event.target.enclosingNodeOrSelfWithClass("html-tag");
        var textNode = event.target.enclosingNodeOrSelfWithClass("html-text-node");
        var commentNode = event.target.enclosingNodeOrSelfWithClass("html-comment");

        var populated = false;
        if (tag && treeElement._populateTagContextMenu) {
            if (populated)
                contextMenu.appendSeparator();
            treeElement._populateTagContextMenu(contextMenu, event);
            populated = true;
        } else if (textNode && treeElement._populateTextContextMenu) {
            if (populated)
                contextMenu.appendSeparator();
            treeElement._populateTextContextMenu(contextMenu, textNode);
            populated = true;
        } else if (commentNode && treeElement._populateNodeContextMenu) {
            if (populated)
                contextMenu.appendSeparator();
            treeElement._populateNodeContextMenu(contextMenu);
            populated = true;
        }

        return populated;
    }

    adjustCollapsedRange()
    {
    }

    // Private

    _revealAndSelectNode(node, omitFocus)
    {
        if (!node || this._suppressRevealAndSelect)
            return;

        if (!WebInspector.showShadowDOMSetting.value) {
            while (node && node.isInShadowTree())
                node = node.parentNode;
            if (!node)
                return;
        }

        var treeElement = this.createTreeElementFor(node);
        if (!treeElement)
            return;

        treeElement.revealAndSelect(omitFocus);
    }

    _treeElementFromEvent(event)
    {
        var scrollContainer = this.element.parentElement;

        // We choose this X coordinate based on the knowledge that our list
        // items extend at least to the right edge of the outer <ol> container.
        // In the no-word-wrap mode the outer <ol> may be wider than the tree container
        // (and partially hidden), in which case we are left to use only its right boundary.
        var x = scrollContainer.totalOffsetLeft + scrollContainer.offsetWidth - 36;

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
    }

    _onmousedown(event)
    {
        var element = this._treeElementFromEvent(event);
        if (!element || element.isEventWithinDisclosureTriangle(event)) {
            event.preventDefault();
            return;
        }

        element.select();
    }

    _onmousemove(event)
    {
        var element = this._treeElementFromEvent(event);
        if (element && this._previousHoveredElement === element)
            return;

        if (this._previousHoveredElement) {
            this._previousHoveredElement.hovered = false;
            this._previousHoveredElement = null;
        }

        if (element) {
            element.hovered = true;
            this._previousHoveredElement = element;

            // Lazily compute tag-specific tooltips.
            if (element.representedObject && !element.tooltip && element._createTooltipForNode)
                element._createTooltipForNode();
        }

        WebInspector.domTreeManager.highlightDOMNode(element ? element.representedObject.id : 0);
    }

    _onmouseout(event)
    {
        var nodeUnderMouse = document.elementFromPoint(event.pageX, event.pageY);
        if (nodeUnderMouse && nodeUnderMouse.isDescendant(this.element))
            return;

        if (this._previousHoveredElement) {
            this._previousHoveredElement.hovered = false;
            this._previousHoveredElement = null;
        }

        WebInspector.domTreeManager.hideDOMNodeHighlight();
    }

    _ondragstart(event)
    {
        var treeElement = this._treeElementFromEvent(event);
        if (!treeElement)
            return false;

        if (!this._isValidDragSourceOrTarget(treeElement))
            return false;

        if (treeElement.representedObject.nodeName() === "BODY" || treeElement.representedObject.nodeName() === "HEAD")
            return false;

        event.dataTransfer.setData("text/plain", treeElement.listItemElement.textContent);
        event.dataTransfer.effectAllowed = "copyMove";
        this._nodeBeingDragged = treeElement.representedObject;

        WebInspector.domTreeManager.hideDOMNodeHighlight();

        return true;
    }

    _ondragover(event)
    {
        if (!this._nodeBeingDragged)
            return false;

        var treeElement = this._treeElementFromEvent(event);
        if (!this._isValidDragSourceOrTarget(treeElement))
            return false;

        var node = treeElement.representedObject;
        while (node) {
            if (node === this._nodeBeingDragged)
                return false;
            node = node.parentNode;
        }

        this.dragOverTreeElement = treeElement;
        treeElement.listItemElement.classList.add("elements-drag-over");
        treeElement.updateSelectionArea();

        event.preventDefault();
        event.dataTransfer.dropEffect = "move";
        return false;
    }

    _ondragleave(event)
    {
        this._clearDragOverTreeElementMarker();
        event.preventDefault();
        return false;
    }

    _isValidDragSourceOrTarget(treeElement)
    {
        if (!treeElement)
            return false;

        var node = treeElement.representedObject;
        if (!(node instanceof WebInspector.DOMNode))
            return false;

        if (!node.parentNode || node.parentNode.nodeType() !== Node.ELEMENT_NODE)
            return false;

        return true;
    }

    _ondrop(event)
    {
        event.preventDefault();

        function callback(error, newNodeId)
        {
            if (error)
                return;

            this._updateModifiedNodes();
            var newNode = WebInspector.domTreeManager.nodeForId(newNodeId);
            if (newNode)
                this.selectDOMNode(newNode, true);
        }

        var treeElement = this._treeElementFromEvent(event);
        if (this._nodeBeingDragged && treeElement) {
            var parentNode;
            var anchorNode;

            if (treeElement._elementCloseTag) {
                // Drop onto closing tag -> insert as last child.
                parentNode = treeElement.representedObject;
            } else {
                var dragTargetNode = treeElement.representedObject;
                parentNode = dragTargetNode.parentNode;
                anchorNode = dragTargetNode;
            }

            this._nodeBeingDragged.moveTo(parentNode, anchorNode, callback.bind(this));
        }

        delete this._nodeBeingDragged;
    }

    _ondragend(event)
    {
        event.preventDefault();
        this._clearDragOverTreeElementMarker();
        delete this._nodeBeingDragged;
    }

    _clearDragOverTreeElementMarker()
    {
        if (this.dragOverTreeElement) {
            let element = this.dragOverTreeElement;
            this.dragOverTreeElement = null;

            element.listItemElement.classList.remove("elements-drag-over");
            element.updateSelectionArea();
        }
    }

    _contextMenuEventFired(event)
    {
        let treeElement = this._treeElementFromEvent(event);
        if (!treeElement)
            return;

        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);
        this.populateContextMenu(contextMenu, event, treeElement);
    }

    _updateModifiedNodes()
    {
        if (this._elementsTreeUpdater)
            this._elementsTreeUpdater._updateModifiedNodes();
    }

    _populateContextMenu(contextMenu, domNode)
    {
        function revealElement()
        {
            WebInspector.domTreeManager.inspectElement(domNode.id);
        }

        function logElement()
        {
            WebInspector.RemoteObject.resolveNode(domNode, WebInspector.RuntimeManager.ConsoleObjectGroup, function(remoteObject) {
                if (!remoteObject)
                    return;
                let text = WebInspector.UIString("Selected Element");
                WebInspector.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, true);
            });
        }

        contextMenu.appendSeparator();

        if (!this._excludeRevealElementContextMenu)
            contextMenu.appendItem(WebInspector.UIString("Reveal in DOM Tree"), revealElement);

        if (!domNode.isInUserAgentShadowTree())
            contextMenu.appendItem(WebInspector.UIString("Log Element"), logElement);
    }

    _showShadowDOMSettingChanged(event)
    {
        var nodeToSelect = this.selectedTreeElement ? this.selectedTreeElement.representedObject : null;
        while (nodeToSelect) {
            if (!nodeToSelect.isInShadowTree())
                break;
            nodeToSelect = nodeToSelect.parentNode;
        }

        this.children.forEach(function(child) {
            child.updateChildren(true);
        });

        if (nodeToSelect)
            this.selectDOMNode(nodeToSelect);
    }

    _hideElement(event, keyboardShortcut)
    {
        if (!this.selectedTreeElement || WebInspector.isEditingAnyField())
            return;

        event.preventDefault();

        var effectiveNode = this.selectedTreeElement.representedObject;
        console.assert(effectiveNode);
        if (!effectiveNode)
            return;

        if (effectiveNode.isPseudoElement()) {
            effectiveNode = effectiveNode.parentNode;
            console.assert(effectiveNode);
            if (!effectiveNode)
                return;
        }            

        if (effectiveNode.nodeType() !== Node.ELEMENT_NODE)
            return;

        function resolvedNode(object)
        {
            if (!object)
                return;

            function injectStyleAndToggleClass()
            {
                var hideElementStyleSheetIdOrClassName = "__WebInspectorHideElement__";
                var styleElement = document.getElementById(hideElementStyleSheetIdOrClassName);
                if (!styleElement) {
                    styleElement = document.createElement("style");
                    styleElement.id = hideElementStyleSheetIdOrClassName;
                    styleElement.textContent = "." + hideElementStyleSheetIdOrClassName + " { visibility: hidden !important; }";
                    document.head.appendChild(styleElement);
                }

                this.classList.toggle(hideElementStyleSheetIdOrClassName);
            }

            object.callFunction(injectStyleAndToggleClass, undefined, false, function(){});
            object.release();
        }

        WebInspector.RemoteObject.resolveNode(effectiveNode, "", resolvedNode);
    }
};

WebInspector.DOMTreeOutline.Event = {
    SelectedNodeChanged: "dom-tree-outline-selected-node-changed"
};
