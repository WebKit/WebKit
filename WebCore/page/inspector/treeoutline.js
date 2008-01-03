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

function TreeOutline(listNode)
{
    this.children = [];
    this.selectedTreeElement = null;
    this._childrenListNode = listNode;
    this._childrenListNode.removeChildren();
    this._knownTreeElements = [];
    this._treeElementsExpandedState = [];
    this.expandTreeElementsWhenArrowing = false;
    this.root = true;
    this.hasChildren = false;
    this.expanded = true;
    this.selected = false;
    this.treeOutline = this;
}

TreeOutline._knownTreeElementNextIdentifier = 1;

TreeOutline._appendChild = function(child)
{
    if (!child)
        throw("child can't be undefined or null");

    var lastChild = this.children[this.children.length - 1];
    if (lastChild) {
        lastChild.nextSibling = child;
        child.previousSibling = lastChild;
    } else {
        child.previousSibling = null;
        child.nextSibling = null;
    }

    this.children.push(child);
    this.hasChildren = true;
    child.parent = this;
    child.treeOutline = this.treeOutline;
    child.treeOutline._rememberTreeElement(child);

    var current = child.children[0];
    while (current) {
        current.treeOutline = this.treeOutline;
        current.treeOutline._rememberTreeElement(current);
        current = current.traverseNextTreeElement(false, child, true);
    }

    if (child.hasChildren && child.treeOutline._treeElementsExpandedState[child.identifier] !== undefined)
        child.expanded = child.treeOutline._treeElementsExpandedState[child.identifier];

    if (!this._childrenListNode) {
        this._childrenListNode = this.treeOutline._childrenListNode.ownerDocument.createElement("ol");
        this._childrenListNode.parentTreeElement = this;
        this._childrenListNode.addStyleClass("children");
        if (this.hidden)
            this._childrenListNode.addStyleClass("hidden");
    }

    child._attach();
}

TreeOutline._insertChild = function(child, index)
{
    if (!child)
        throw("child can't be undefined or null");

    var previousChild = (index > 0 ? this.children[index - 1] : null);
    if (previousChild) {
        previousChild.nextSibling = child;
        child.previousSibling = previousChild;
    } else {
        child.previousSibling = null;
    }

    var nextChild = this.children[index];
    if (nextChild) {
        nextChild.previousSibling = child;
        child.nextSibling = nextChild;
    } else {
        child.nextSibling = null;
    }

    this.children.splice(index, 0, child);
    this.hasChildren = true;
    child.parent = this;
    child.treeOutline = this.treeOutline;
    child.treeOutline._rememberTreeElement(child);

    var current = child.children[0];
    while (current) {
        current.treeOutline = this.treeOutline;
        current.treeOutline._rememberTreeElement(current);
        current = current.traverseNextTreeElement(false, child, true);
    }

    if (child.hasChildren && child.treeOutline._treeElementsExpandedState[child.identifier] !== undefined)
        child.expanded = child.treeOutline._treeElementsExpandedState[child.identifier];

    if (!this._childrenListNode) {
        this._childrenListNode = this.treeOutline._childrenListNode.ownerDocument.createElement("ol");
        this._childrenListNode.parentTreeElement = this;
        this._childrenListNode.addStyleClass("children");
        if (this.hidden)
            this._childrenListNode.addStyleClass("hidden");
    }

    child._attach();
}

TreeOutline._removeChild = function(child)
{
    if (!child)
        throw("child can't be undefined or null");

    for (var i = 0; i < this.children.length; ++i) {
        if (this.children[i] === child) {
            this.children.splice(i, 1);
            break;
        }
    }

    child.deselect();

    if (child.previousSibling)
        child.previousSibling.nextSibling = child.nextSibling;
    if (child.nextSibling)
        child.nextSibling.previousSibling = child.previousSibling;

    if (child.treeOutline)
        child.treeOutline._forgetTreeElement(child);
    child._detach();
    child.treeOutline = null;
    child.parent = null;
    child.nextSibling = null;
    child.previousSibling = null;
}

TreeOutline._removeChildren = function()
{
    for (var i = 0; i < this.children.length; ++i) {
        var child = this.children[i];
        child.deselect();
        if (child.treeOutline)
            child.treeOutline._forgetTreeElement(child);
        child._detach();
        child.treeOutline = null;
        child.parent = null;
        child.nextSibling = null;
        child.previousSibling = null;
    }

    this.children = [];

    if (this._childrenListNode)
        this._childrenListNode.offsetTop; // force layout
}

TreeOutline._removeChildrenRecursive = function()
{
    var childrenToRemove = this.children;

    var child = this.children[0];
    while (child) {
        if (child.children.length)
            childrenToRemove = childrenToRemove.concat(child.children);
        child = child.traverseNextTreeElement(false, this, true);
    }

    for (var i = 0; i < childrenToRemove.length; ++i) {
        var child = childrenToRemove[i];
        child.deselect();
        if (child.treeOutline)
            child.treeOutline._forgetTreeElement(child);
        child._detach();
        child.children = [];
        child.treeOutline = null;
        child.parent = null;
        child.nextSibling = null;
        child.previousSibling = null;
    }

    this.children = [];
}

TreeOutline.prototype._rememberTreeElement = function(element)
{
    if (!this._knownTreeElements[element.identifier])
        this._knownTreeElements[element.identifier] = [];

    // check if the element is already known
    var elements = this._knownTreeElements[element.identifier];
    for (var i = 0; i < elements.length; ++i)
        if (elements[i] === element)
            return;

    // add the element
    elements.push(element);
}

TreeOutline.prototype._forgetTreeElement = function(element)
{
    if (!this._knownTreeElements[element.identifier])
        return;

    var elements = this._knownTreeElements[element.identifier];
    for (var i = 0; i < elements.length; ++i) {
        if (elements[i] === element) {
            elements.splice(i, 1);
            break;
        }
    }
}

TreeOutline.prototype.findTreeElement = function(representedObject, isAncestor, getParent)
{
    if (!representedObject)
        return null;

    if ("__treeElementIdentifier" in representedObject) {
        var elements = this._knownTreeElements[representedObject.__treeElementIdentifier];
        if (elements) {
            for (var i = 0; i < elements.length; ++i)
                if (elements[i].representedObject === representedObject)
                    return elements[i];
        }
    }

    if (!isAncestor || !(isAncestor instanceof Function) || !getParent || !(getParent instanceof Function))
        return null;

    var item;
    var found = false;
    for (var i = 0; i < this.children.length; ++i) {
        item = this.children[i];
        if (item.representedObject === representedObject || isAncestor(item.representedObject, representedObject)) {
            found = true;
            break;
        }
    }

    if (!found)
        return null;

    var ancestors = [];
    var currentObject = representedObject;
    while (currentObject) {
        ancestors.unshift(currentObject);
        if (currentObject === item.representedObject)
            break;
        currentObject = getParent(currentObject);
    }

    for (var i = 0; i < ancestors.length; ++i) {
        item = this.findTreeElement(ancestors[i], isAncestor, getParent);
        if (ancestors[i] !== representedObject && item && item.onpopulate)
            item.onpopulate(item);
    }

    return item;
}

TreeOutline.prototype.handleKeyEvent = function(event)
{
    if (!this.selectedTreeElement || event.shiftKey || event.metaKey || event.ctrlKey)
        return false;

    var handled = false;
    var nextSelectedElement;
    if (event.keyIdentifier === "Up" && !event.altKey) {
        nextSelectedElement = this.selectedTreeElement.traversePreviousTreeElement(true);
        while (nextSelectedElement && !nextSelectedElement.selectable)
            nextSelectedElement = nextSelectedElement.traversePreviousTreeElement(!this.expandTreeElementsWhenArrowing);
        handled = nextSelectedElement ? true : false;
    } else if (event.keyIdentifier === "Down" && !event.altKey) {
        nextSelectedElement = this.selectedTreeElement.traverseNextTreeElement(true);
        while (nextSelectedElement && !nextSelectedElement.selectable)
            nextSelectedElement = nextSelectedElement.traverseNextTreeElement(!this.expandTreeElementsWhenArrowing);
        handled = nextSelectedElement ? true : false;
    } else if (event.keyIdentifier === "Left") {
        if (this.selectedTreeElement.expanded) {
            if (event.altKey)
                this.selectedTreeElement.collapseRecursively();
            else
                this.selectedTreeElement.collapse();
            handled = true;
        } else if (this.selectedTreeElement.parent && !this.selectedTreeElement.parent.root) {
            handled = true;
            if (this.selectedTreeElement.parent.selectable) {
                nextSelectedElement = this.selectedTreeElement.parent;
                handled = nextSelectedElement ? true : false;
            } else if (this.selectedTreeElement.parent)
                this.selectedTreeElement.parent.collapse();
        }
    } else if (event.keyIdentifier === "Right") {
        if (!this.selectedTreeElement.revealed()) {
            this.selectedTreeElement.reveal();
            handled = true;
        } else if (this.selectedTreeElement.hasChildren) {
            handled = true;
            if (this.selectedTreeElement.expanded) {
                nextSelectedElement = this.selectedTreeElement.children[0];
                handled = nextSelectedElement ? true : false;
            } else {
                if (event.altKey)
                    this.selectedTreeElement.expandRecursively();
                else
                    this.selectedTreeElement.expand();
            }
        }
    }

    if (nextSelectedElement) {
        nextSelectedElement.reveal();
        nextSelectedElement.select();
    }

    if (handled) {
        event.preventDefault();
        event.stopPropagation();
    }

    return handled;
}

TreeOutline.prototype.expand = function()
{
    // this is the root, do nothing
}

TreeOutline.prototype.collapse = function()
{
    // this is the root, do nothing
}

TreeOutline.prototype.revealed = function()
{
    return true;
}

TreeOutline.prototype.reveal = function()
{
    // this is the root, do nothing
}

TreeOutline.prototype.appendChild = TreeOutline._appendChild;
TreeOutline.prototype.insertChild = TreeOutline._insertChild;
TreeOutline.prototype.removeChild = TreeOutline._removeChild;
TreeOutline.prototype.removeChildren = TreeOutline._removeChildren;
TreeOutline.prototype.removeChildrenRecursive = TreeOutline._removeChildrenRecursive;

function TreeElement(title, representedObject, hasChildren)
{
    this._title = title;
    this.representedObject = (representedObject || {});

    if (this.representedObject.__treeElementIdentifier)
        this.identifier = this.representedObject.__treeElementIdentifier;
    else {
        this.identifier = TreeOutline._knownTreeElementNextIdentifier++;
        this.representedObject.__treeElementIdentifier = this.identifier;
    }

    this._hidden = false;
    this.expanded = false;
    this.selected = false;
    this.hasChildren = hasChildren;
    this.children = [];
    this.treeOutline = null;
    this.parent = null;
    this.previousSibling = null;
    this.nextSibling = null;
    this._listItemNode = null;
}

TreeElement.prototype = {
    selectable: true,
    arrowToggleWidth: 10,

    get listItemElement() {
        return this._listItemNode;
    },

    get childrenListElement() {
        return this._childrenListNode;
    },

    get title() {
        return this._title;
    },

    set title(x) {
        this._title = x;
        if (this._listItemNode)
            this._listItemNode.innerHTML = x;
    },

    get tooltip() {
        return this._tooltip;
    },

    set tooltip(x) {
        this._tooltip = x;
        if (this._listItemNode)
            this._listItemNode.title = x ? x : "";
    },

    get hidden() {
        return this._hidden;
    },

    set hidden(x) {
        if (this._hidden === x)
            return;

        this._hidden = x;

        if (x) {
            if (this._listItemNode)
                this._listItemNode.addStyleClass("hidden");
            if (this._childrenListNode)
                this._childrenListNode.addStyleClass("hidden");
        } else {
            if (this._listItemNode)
                this._listItemNode.removeStyleClass("hidden");
            if (this._childrenListNode)
                this._childrenListNode.removeStyleClass("hidden");
        }
    }
}

TreeElement.prototype.appendChild = TreeOutline._appendChild;
TreeElement.prototype.insertChild = TreeOutline._insertChild;
TreeElement.prototype.removeChild = TreeOutline._removeChild;
TreeElement.prototype.removeChildren = TreeOutline._removeChildren;
TreeElement.prototype.removeChildrenRecursive = TreeOutline._removeChildrenRecursive;

TreeElement.prototype._attach = function()
{
    if (!this._listItemNode || this.parent.refreshChildren) {
        if (this._listItemNode && this._listItemNode.parentNode)
            this._listItemNode.parentNode.removeChild(this._listItemNode);

        this._listItemNode = this.treeOutline._childrenListNode.ownerDocument.createElement("li");
        this._listItemNode.treeElement = this;
        this._listItemNode.innerHTML = this._title;
        this._listItemNode.title = this._tooltip ? this._tooltip : "";

        if (this.hidden)
            this._listItemNode.addStyleClass("hidden");
        if (this.hasChildren)
            this._listItemNode.addStyleClass("parent");
        if (this.expanded)
            this._listItemNode.addStyleClass("expanded");
        if (this.selected)
            this._listItemNode.addStyleClass("selected");

        this._listItemNode.addEventListener("mousedown", TreeElement.treeElementSelected, false);
        this._listItemNode.addEventListener("click", TreeElement.treeElementToggled, false);
        this._listItemNode.addEventListener("dblclick", TreeElement.treeElementDoubleClicked, false);

        if (this.onattach)
            this.onattach(this);
    }

    this.parent._childrenListNode.insertBefore(this._listItemNode, (this.nextSibling ? this.nextSibling._listItemNode : null));
    if (this._childrenListNode)
        this.parent._childrenListNode.insertBefore(this._childrenListNode, this._listItemNode.nextSibling);
    if (this.selected)
        this.select();
    if (this.expanded)
        this.expand();
}

TreeElement.prototype._detach = function()
{
    if (this._listItemNode && this._listItemNode.parentNode)
        this._listItemNode.parentNode.removeChild(this._listItemNode);
    if (this._childrenListNode && this._childrenListNode.parentNode)
        this._childrenListNode.parentNode.removeChild(this._childrenListNode);
}

TreeElement.treeElementSelected = function(event)
{
    var element = event.currentTarget;
    if (!element || !element.treeElement || !element.treeElement.selectable)
        return;

    if (event.offsetX > element.treeElement.arrowToggleWidth || !element.treeElement.hasChildren)
        element.treeElement.select();
}

TreeElement.treeElementToggled = function(event)
{
    var element = event.currentTarget;
    if (!element || !element.treeElement)
        return;

    if (event.offsetX <= element.treeElement.arrowToggleWidth && element.treeElement.hasChildren) {
        if (element.treeElement.expanded) {
            if (event.altKey)
                element.treeElement.collapseRecursively();
            else
                element.treeElement.collapse();
        } else {
            if (event.altKey)
                element.treeElement.expandRecursively();
            else
                element.treeElement.expand();
        }
    }
}

TreeElement.treeElementDoubleClicked = function(event)
{
    var element = event.currentTarget;
    if (!element || !element.treeElement)
        return;

    if (element.treeElement.ondblclick)
        element.treeElement.ondblclick(element.treeElement, event);
    else if (element.treeElement.hasChildren && !element.treeElement.expanded)
        element.treeElement.expand();
}

TreeElement.prototype.collapse = function()
{
    if (this._listItemNode)
        this._listItemNode.removeStyleClass("expanded");
    if (this._childrenListNode)
        this._childrenListNode.removeStyleClass("expanded");

    this.expanded = false;
    if (this.treeOutline)
        this.treeOutline._treeElementsExpandedState[this.identifier] = true;

    if (this.oncollapse)
        this.oncollapse(this);
}

TreeElement.prototype.collapseRecursively = function()
{
    var item = this;
    while (item) {
        if (item.expanded)
            item.collapse();
        item = item.traverseNextTreeElement(false, this, true);
    }
}

TreeElement.prototype.expand = function()
{
    if (!this.hasChildren || (this.expanded && !this.refreshChildren && this._childrenListNode))
        return;

    if (!this._childrenListNode || this.refreshChildren) {
        if (this._childrenListNode && this._childrenListNode.parentNode)
            this._childrenListNode.parentNode.removeChild(this._childrenListNode);

        this._childrenListNode = this.treeOutline._childrenListNode.ownerDocument.createElement("ol");
        this._childrenListNode.parentTreeElement = this;
        this._childrenListNode.addStyleClass("children");

        if (this.hidden)
            this._childrenListNode.addStyleClass("hidden");

        if (this.onpopulate)
            this.onpopulate(this);

        for (var i = 0; i < this.children.length; ++i)
            this.children[i]._attach();

        delete this.refreshChildren;
    }

    if (this._listItemNode) {
        this._listItemNode.addStyleClass("expanded");
        if (this._childrenListNode.parentNode != this._listItemNode.parentNode)
            this.parent._childrenListNode.insertBefore(this._childrenListNode, this._listItemNode.nextSibling);
    }

    if (this._childrenListNode)
        this._childrenListNode.addStyleClass("expanded");

    this.expanded = true;
    if (this.treeOutline)
        this.treeOutline._treeElementsExpandedState[this.identifier] = true;

    if (this.onexpand)
        this.onexpand(this);
}

TreeElement.prototype.expandRecursively = function()
{
    var item = this;
    while (item) {
        item.expand();
        item = item.traverseNextTreeElement(false, this);
    }
}

TreeElement.prototype.reveal = function()
{
    var currentAncestor = this.parent;
    while (currentAncestor && !currentAncestor.root) {
        if (!currentAncestor.expanded)
            currentAncestor.expand();
        currentAncestor = currentAncestor.parent;
    }

    if (this.onreveal)
        this.onreveal(this);
}

TreeElement.prototype.revealed = function()
{
    var currentAncestor = this.parent;
    while (currentAncestor && !currentAncestor.root) {
        if (!currentAncestor.expanded)
            return false;
        currentAncestor = currentAncestor.parent;
    }

    return true;
}

TreeElement.prototype.select = function(supressOnSelect)
{
    if (!this.treeOutline || !this.selectable || this.selected)
        return;

    if (this.treeOutline.selectedTreeElement)
        this.treeOutline.selectedTreeElement.deselect();

    this.selected = true;
    this.treeOutline.selectedTreeElement = this;
    if (this._listItemNode)
        this._listItemNode.addStyleClass("selected");

    if (this.onselect && !supressOnSelect)
        this.onselect(this);
}

TreeElement.prototype.deselect = function(supressOnDeselect)
{
    if (!this.treeOutline || this.treeOutline.selectedTreeElement !== this || !this.selected)
        return;

    this.selected = false;
    this.treeOutline.selectedTreeElement = null;
    if (this._listItemNode)
        this._listItemNode.removeStyleClass("selected");

    if (this.ondeselect && !supressOnDeselect)
        this.ondeselect(this);
}

TreeElement.prototype.traverseNextTreeElement = function(skipHidden, stayWithin, dontPopulate)
{
    if (!dontPopulate && this.hasChildren && this.onpopulate)
        this.onpopulate(this);

    var element = skipHidden ? (this.revealed() ? this.children[0] : null) : this.children[0];
    if (element && (!skipHidden || (skipHidden && this.expanded)))
        return element;

    if (this === stayWithin)
        return null;

    element = skipHidden ? (this.revealed() ? this.nextSibling : null) : this.nextSibling;
    if (element)
        return element;

    element = this;
    while (element && !element.root && !(skipHidden ? (element.revealed() ? element.nextSibling : null) : element.nextSibling) && element.parent !== stayWithin)
        element = element.parent;

    if (!element)
        return null;

    return (skipHidden ? (element.revealed() ? element.nextSibling : null) : element.nextSibling);
}

TreeElement.prototype.traversePreviousTreeElement = function(skipHidden, dontPopulate)
{
    var element = skipHidden ? (this.revealed() ? this.previousSibling : null) : this.previousSibling;
    if (!dontPopulate && element && element.hasChildren && element.onpopulate)
        element.onpopulate(element);

    while (element && (skipHidden ? (element.revealed() && element.expanded ? element.children[element.children.length - 1] : null) : element.children[element.children.length - 1])) {
        if (!dontPopulate && element.hasChildren && element.onpopulate)
            element.onpopulate(element);
        element = (skipHidden ? (element.revealed() && element.expanded ? element.children[element.children.length - 1] : null) : element.children[element.children.length - 1]);
    }

    if (element)
        return element;

    if (!this.parent || this.parent.root)
        return null;

    return this.parent;
}
