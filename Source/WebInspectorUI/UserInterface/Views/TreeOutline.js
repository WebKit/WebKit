/*
 * Copyright (C) 2007, 2013, 2015 Apple Inc.  All rights reserved.
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

WI.TreeOutline = class TreeOutline extends WI.Object
{
    constructor(element, selectable = true)
    {
        super();

        this.element = element || document.createElement("ol");
        this.element.classList.add(WI.TreeOutline.ElementStyleClassName);
        this.element.addEventListener("contextmenu", this._handleContextmenu.bind(this));

        this.children = [];
        this.selectedTreeElement = null;
        this._childrenListNode = this.element;
        this._childrenListNode.removeChildren();
        this._knownTreeElements = [];
        this._treeElementsExpandedState = [];
        this.allowsRepeatSelection = false;
        this.root = true;
        this.hasChildren = false;
        this.expanded = true;
        this.selected = false;
        this.treeOutline = this;
        this._hidden = false;
        this._compact = false;
        this._large = false;
        this._disclosureButtons = true;
        this._customIndent = false;
        this._selectable = selectable;

        this._virtualizedVisibleTreeElements = null;
        this._virtualizedAttachedTreeElements = null;
        this._virtualizedScrollContainer = null;
        this._virtualizedTreeItemHeight = NaN;
        this._virtualizedTopSpacer = null;
        this._virtualizedBottomSpacer = null;

        this._childrenListNode.tabIndex = 0;
        this._childrenListNode.addEventListener("keydown", this._treeKeyDown.bind(this), true);

        WI.TreeOutline._generateStyleRulesIfNeeded();

        if (!this._selectable)
            this.element.classList.add("non-selectable");
    }

    // Public

    get hidden()
    {
        return this._hidden;
    }

    set hidden(x)
    {
        if (this._hidden === x)
            return;

        this._hidden = x;
        this.element.hidden = this._hidden;
    }

    get compact()
    {
        return this._compact;
    }

    set compact(x)
    {
        if (this._compact === x)
            return;

        this._compact = x;
        if (this._compact)
            this.large = false;

        this.element.classList.toggle("compact", this._compact);
    }

    get large()
    {
        return this._large;
    }

    set large(x)
    {
        if (this._large === x)
            return;

        this._large = x;
        if (this._large)
            this.compact = false;

        this.element.classList.toggle("large", this._large);
    }

    get disclosureButtons()
    {
        return this._disclosureButtons;
    }

    set disclosureButtons(x)
    {
        if (this._disclosureButtons === x)
            return;

        this._disclosureButtons = x;
        this.element.classList.toggle("hide-disclosure-buttons", !this._disclosureButtons);
    }

    get customIndent()
    {
        return this._customIndent;
    }

    set customIndent(x)
    {
        if (this._customIndent === x)
            return;

        this._customIndent = x;
        this.element.classList.toggle(WI.TreeOutline.CustomIndentStyleClassName, this._customIndent);
    }

    get selectable() { return this._selectable; }

    appendChild(child)
    {
        console.assert(child);
        if (!child)
            return;

        var lastChild = this.children[this.children.length - 1];
        if (lastChild) {
            lastChild.nextSibling = child;
            child.previousSibling = lastChild;
        } else {
            child.previousSibling = null;
            child.nextSibling = null;
        }

        var isFirstChild = !this.children.length;

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

        if (this._childrenListNode)
            child._attach();

        if (this.treeOutline)
            this.treeOutline.dispatchEventToListeners(WI.TreeOutline.Event.ElementAdded, {element: child});

        if (isFirstChild && this.expanded)
            this.expand();
    }

    insertChild(child, index)
    {
        console.assert(child);
        if (!child)
            return;

        var previousChild = index > 0 ? this.children[index - 1] : null;
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

        var isFirstChild = !this.children.length;

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

        if (this._childrenListNode)
            child._attach();

        if (this.treeOutline)
            this.treeOutline.dispatchEventToListeners(WI.TreeOutline.Event.ElementAdded, {element: child});

        if (isFirstChild && this.expanded)
            this.expand();
    }

    removeChildAtIndex(childIndex, suppressOnDeselect, suppressSelectSibling)
    {
        console.assert(childIndex >= 0 && childIndex < this.children.length);
        if (childIndex < 0 || childIndex >= this.children.length)
            return;

        let child = this.children[childIndex];
        this.children.splice(childIndex, 1);

        let parent = child.parent;
        if (child.deselect(suppressOnDeselect)) {
            if (child.previousSibling && !suppressSelectSibling)
                child.previousSibling.select(true, false);
            else if (child.nextSibling && !suppressSelectSibling)
                child.nextSibling.select(true, false);
            else if (!suppressSelectSibling)
                parent.select(true, false);
        }

        if (child.previousSibling)
            child.previousSibling.nextSibling = child.nextSibling;
        if (child.nextSibling)
            child.nextSibling.previousSibling = child.previousSibling;

        let treeOutline = child.treeOutline;
        if (treeOutline) {
            treeOutline._forgetTreeElement(child);
            treeOutline._forgetChildrenRecursive(child);
        }

        child._detach();
        child.treeOutline = null;
        child.parent = null;
        child.nextSibling = null;
        child.previousSibling = null;

        if (treeOutline)
            treeOutline.dispatchEventToListeners(WI.TreeOutline.Event.ElementRemoved, {element: child});
    }

    removeChild(child, suppressOnDeselect, suppressSelectSibling)
    {
        console.assert(child);
        if (!child)
            return;

        var childIndex = this.children.indexOf(child);
        console.assert(childIndex !== -1);
        if (childIndex === -1)
            return;

        this.removeChildAtIndex(childIndex, suppressOnDeselect, suppressSelectSibling);

        if (!this.children.length) {
            if (this._listItemNode)
                this._listItemNode.classList.remove("parent");
            this.hasChildren = false;
        }
    }

    removeChildren(suppressOnDeselect)
    {
        for (let child of this.children) {
            child.deselect(suppressOnDeselect);

            let treeOutline = child.treeOutline;
            if (treeOutline) {
                treeOutline._forgetTreeElement(child);
                treeOutline._forgetChildrenRecursive(child);
            }

            child._detach();
            child.treeOutline = null;
            child.parent = null;
            child.nextSibling = null;
            child.previousSibling = null;

            if (treeOutline)
                treeOutline.dispatchEventToListeners(WI.TreeOutline.Event.ElementRemoved, {element: child});
        }

        this.children = [];
    }

    removeChildrenRecursive(suppressOnDeselect)
    {
        let childrenToRemove = this.children;
        let child = this.children[0];
        while (child) {
            if (child.children.length)
                childrenToRemove = childrenToRemove.concat(child.children);
            child = child.traverseNextTreeElement(false, this, true);
        }

        for (let child of childrenToRemove) {
            child.deselect(suppressOnDeselect);

            let treeOutline = child.treeOutline;
            if (treeOutline)
                treeOutline._forgetTreeElement(child);

            child._detach();
            child.children = [];
            child.treeOutline = null;
            child.parent = null;
            child.nextSibling = null;
            child.previousSibling = null;

            if (treeOutline)
                treeOutline.dispatchEventToListeners(WI.TreeOutline.Event.ElementRemoved, {element: child});
        }

        this.children = [];
    }

    reattachIfIndexChanged(treeElement, insertionIndex)
    {
        if (this.children[insertionIndex] === treeElement)
            return;

        let wasSelected = treeElement.selected;

        console.assert(!treeElement.parent || treeElement.parent === this);
        if (treeElement.parent === this)
            this.removeChild(treeElement);

        this.insertChild(treeElement, insertionIndex);

        if (wasSelected)
            treeElement.select();
    }

    _rememberTreeElement(element)
    {
        if (!this._knownTreeElements[element.identifier])
            this._knownTreeElements[element.identifier] = [];

        // check if the element is already known
        var elements = this._knownTreeElements[element.identifier];
        if (elements.indexOf(element) !== -1)
            return;

        // add the element
        elements.push(element);
    }

    _forgetTreeElement(element)
    {
        if (this.selectedTreeElement === element) {
            element.deselect(true);
            this.selectedTreeElement = null;
        }
        if (this._knownTreeElements[element.identifier])
            this._knownTreeElements[element.identifier].remove(element, true);
    }

    _forgetChildrenRecursive(parentElement)
    {
        var child = parentElement.children[0];
        while (child) {
            this._forgetTreeElement(child);
            child = child.traverseNextTreeElement(false, parentElement, true);
        }
    }

    getCachedTreeElement(representedObject)
    {
        if (!representedObject)
            return null;

        if (representedObject.__treeElementIdentifier) {
            // If this representedObject has a tree element identifier, and it is a known TreeElement
            // in our tree we can just return that tree element.
            var elements = this._knownTreeElements[representedObject.__treeElementIdentifier];
            if (elements) {
                for (var i = 0; i < elements.length; ++i)
                    if (elements[i].representedObject === representedObject)
                        return elements[i];
            }
        }
        return null;
    }

    selfOrDescendant(predicate)
    {
        let treeElements = [this];
        while (treeElements.length) {
            let treeElement = treeElements.shift();
            if (predicate(treeElement))
                return treeElement;

            treeElements = treeElements.concat(treeElement.children);
        }

        return false;
    }

    findTreeElement(representedObject, isAncestor, getParent)
    {
        if (!representedObject)
            return null;

        var cachedElement = this.getCachedTreeElement(representedObject);
        if (cachedElement)
            return cachedElement;

        // The representedObject isn't known, so we start at the top of the tree and work down to find the first
        // tree element that represents representedObject or one of its ancestors.
        var item;
        var found = false;
        for (var i = 0; i < this.children.length; ++i) {
            item = this.children[i];
            if (item.representedObject === representedObject || (isAncestor && isAncestor(item.representedObject, representedObject))) {
                found = true;
                break;
            }
        }

        if (!found)
            return null;

        // Make sure the item that we found is connected to the root of the tree.
        // Build up a list of representedObject's ancestors that aren't already in our tree.
        var ancestors = [];
        var currentObject = representedObject;
        while (currentObject) {
            ancestors.unshift(currentObject);
            if (currentObject === item.representedObject)
                break;
            currentObject = getParent(currentObject);
        }

        // For each of those ancestors we populate them to fill in the tree.
        for (var i = 0; i < ancestors.length; ++i) {
            // Make sure we don't call findTreeElement with the same representedObject
            // again, to prevent infinite recursion.
            if (ancestors[i] === representedObject)
                continue;

            // FIXME: we could do something faster than findTreeElement since we will know the next
            // ancestor exists in the tree.
            item = this.findTreeElement(ancestors[i], isAncestor, getParent);
            if (item)
                item.onpopulate();
        }

        return this.getCachedTreeElement(representedObject);
    }

    _treeElementDidChange(treeElement)
    {
        if (treeElement.treeOutline !== this)
            return;

        this.dispatchEventToListeners(WI.TreeOutline.Event.ElementDidChange, {element: treeElement});
    }

    treeElementFromNode(node)
    {
        var listNode = node.enclosingNodeOrSelfWithNodeNameInArray(["ol", "li"]);
        if (listNode)
            return listNode.parentTreeElement || listNode.treeElement;
        return null;
    }

    treeElementFromPoint(x, y)
    {
        var node = this._childrenListNode.ownerDocument.elementFromPoint(x, y);
        if (!node)
            return null;

        return this.treeElementFromNode(node);
    }

    _treeKeyDown(event)
    {
        if (event.target !== this._childrenListNode)
            return;

        if (!this.selectedTreeElement || event.shiftKey || event.metaKey || event.ctrlKey)
            return;

        let isRTL = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL;

        var handled = false;
        var nextSelectedElement;
        if (event.keyIdentifier === "Up" && !event.altKey) {
            nextSelectedElement = this.selectedTreeElement.traversePreviousTreeElement(true);
            while (nextSelectedElement && !nextSelectedElement.selectable)
                nextSelectedElement = nextSelectedElement.traversePreviousTreeElement(true);
            handled = nextSelectedElement ? true : false;
        } else if (event.keyIdentifier === "Down" && !event.altKey) {
            nextSelectedElement = this.selectedTreeElement.traverseNextTreeElement(true);
            while (nextSelectedElement && !nextSelectedElement.selectable)
                nextSelectedElement = nextSelectedElement.traverseNextTreeElement(true);
            handled = nextSelectedElement ? true : false;
        } else if ((!isRTL && event.keyIdentifier === "Left") || (isRTL && event.keyIdentifier === "Right")) {
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
                    while (nextSelectedElement && !nextSelectedElement.selectable)
                        nextSelectedElement = nextSelectedElement.parent;
                    handled = nextSelectedElement ? true : false;
                } else if (this.selectedTreeElement.parent)
                    this.selectedTreeElement.parent.collapse();
            }
        } else if ((!isRTL && event.keyIdentifier === "Right") || (isRTL && event.keyIdentifier === "Left")) {
            if (!this.selectedTreeElement.revealed()) {
                this.selectedTreeElement.reveal();
                handled = true;
            } else if (this.selectedTreeElement.hasChildren) {
                handled = true;
                if (this.selectedTreeElement.expanded) {
                    nextSelectedElement = this.selectedTreeElement.children[0];
                    while (nextSelectedElement && !nextSelectedElement.selectable)
                        nextSelectedElement = nextSelectedElement.nextSibling;
                    handled = nextSelectedElement ? true : false;
                } else {
                    if (event.altKey)
                        this.selectedTreeElement.expandRecursively();
                    else
                        this.selectedTreeElement.expand();
                }
            }
        } else if (event.keyCode === 8 /* Backspace */ || event.keyCode === 46 /* Delete */) {
            if (this.selectedTreeElement.ondelete)
                handled = this.selectedTreeElement.ondelete();
            if (!handled && this.treeOutline.ondelete)
                handled = this.treeOutline.ondelete(this.selectedTreeElement);
        } else if (isEnterKey(event)) {
            if (this.selectedTreeElement.onenter)
                handled = this.selectedTreeElement.onenter();
            if (!handled && this.treeOutline.onenter)
                handled = this.treeOutline.onenter(this.selectedTreeElement);
        } else if (event.keyIdentifier === "U+0020" /* Space */) {
            if (this.selectedTreeElement.onspace)
                handled = this.selectedTreeElement.onspace();
            if (!handled && this.treeOutline.onspace)
                handled = this.treeOutline.onspace(this.selectedTreeElement);
        }

        if (nextSelectedElement) {
            nextSelectedElement.reveal();
            nextSelectedElement.select(false, true);
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
        }
    }

    expand()
    {
        // this is the root, do nothing
    }

    collapse()
    {
        // this is the root, do nothing
    }

    revealed()
    {
        return true;
    }

    reveal()
    {
        // this is the root, do nothing
    }

    select()
    {
        // this is the root, do nothing
    }

    revealAndSelect(omitFocus)
    {
        // this is the root, do nothing
    }

    get selectedTreeElementIndex()
    {
        if (!this.hasChildren || !this.selectedTreeElement)
            return;

        for (var i = 0; i < this.children.length; ++i) {
            if (this.children[i] === this.selectedTreeElement)
                return i;
        }

        return false;
    }

    get virtualized()
    {
        return this._virtualizedScrollContainer && !isNaN(this._virtualizedTreeItemHeight);
    }

    registerScrollVirtualizer(scrollContainer, treeItemHeight)
    {
        console.assert(!isNaN(treeItemHeight));

        this._virtualizedVisibleTreeElements = new Set;
        this._virtualizedAttachedTreeElements = new Set;
        this._virtualizedScrollContainer = scrollContainer;
        this._virtualizedTreeItemHeight = treeItemHeight;
        this._virtualizedTopSpacer = document.createElement("div");
        this._virtualizedBottomSpacer = document.createElement("div");

        let throttler = this.throttle(1000 / 16);
        this._virtualizedScrollContainer.addEventListener("scroll", (event) => {
            throttler.updateVirtualizedElements();
        });
    }

    updateVirtualizedElements(focusedTreeElement)
    {
        if (!this.virtualized)
            return;

        function walk(parent, callback, count = 0) {
            let shouldReturn = false;
            for (let child of parent.children) {
                if (!child.revealed(false))
                    continue;

                shouldReturn = callback(child, count);
                if (shouldReturn)
                    break;

                ++count;
                if (child.expanded) {
                    let result = walk(child, callback, count);
                    count = result.count;
                    if (result.shouldReturn)
                        break;
                }
            }
            return {count, shouldReturn};
        }

        let {numberVisible, extraRows, firstItem, lastItem} = this._calculateVirtualizedValues();

        let shouldScroll = false;
        if (focusedTreeElement && focusedTreeElement.revealed(false)) {
            let index = walk(this, (treeElement) => treeElement === focusedTreeElement).count;
            if (index < firstItem) {
                firstItem = index - extraRows;
                lastItem = index + numberVisible + extraRows;
            } else if (index > lastItem) {
                firstItem = index - numberVisible - extraRows;
                lastItem = index + extraRows;
            }

            // Only scroll if the `focusedTreeElement` is outside the visible items, not including
            // the added buffer `extraRows`.
            shouldScroll = (index < firstItem + extraRows) || (index > lastItem - extraRows);
        }

        console.assert(firstItem < lastItem);

        let visibleTreeElements = new Set;
        let treeElementsToAttach = new Set;
        let treeElementsToDetach = new Set;
        let totalItems = walk(this, (treeElement, count) => {
            if (count >= firstItem && count <= lastItem) {
                treeElementsToAttach.add(treeElement);
                if (count >= firstItem + extraRows && count <= lastItem - extraRows)
                    visibleTreeElements.add(treeElement);
            } else if (treeElement.element.parentNode)
                treeElementsToDetach.add(treeElement);

            return false;
        }).count;

        // Redraw if we are about to scroll.
        if (!shouldScroll) {
            // Redraw if all of the previously centered `WI.TreeElement` are no longer centered.
            if (visibleTreeElements.intersects(this._virtualizedVisibleTreeElements)) {
                // Redraw if there is a `WI.TreeElement` that should be shown that isn't attached.
                if (visibleTreeElements.isSubsetOf(this._virtualizedAttachedTreeElements))
                    return;
            }
        }

        this._virtualizedVisibleTreeElements = visibleTreeElements;
        this._virtualizedAttachedTreeElements = treeElementsToAttach;

        for (let treeElement of treeElementsToDetach)
            treeElement.element.remove();

        for (let treeElement of treeElementsToAttach) {
            treeElement.parent._childrenListNode.appendChild(treeElement.element);
            if (treeElement._childrenListNode)
                treeElement.parent._childrenListNode.appendChild(treeElement._childrenListNode);
        }

        this._virtualizedTopSpacer.style.height = (Math.max(firstItem, 0) * this._virtualizedTreeItemHeight) + "px";
        if (this.element.previousElementSibling !== this._virtualizedTopSpacer)
            this.element.parentNode.insertBefore(this._virtualizedTopSpacer, this.element);

        this._virtualizedBottomSpacer.style.height = (Math.max(totalItems - lastItem, 0) * this._virtualizedTreeItemHeight) + "px";
        if (this.element.nextElementSibling !== this._virtualizedBottomSpacer)
            this.element.parentNode.insertBefore(this._virtualizedBottomSpacer, this.element.nextElementSibling);

        if (shouldScroll)
            this._virtualizedScrollContainer.scrollTop = (firstItem + extraRows) * this._virtualizedTreeItemHeight;
    }

    // Protected

    treeElementFromEvent(event)
    {
        let scrollContainer = this.element.parentElement;

        // We choose this X coordinate based on the knowledge that our list
        // items extend at least to the trailing edge of the outer <ol> container.
        // In the no-word-wrap mode the outer <ol> may be wider than the tree container
        // (and partially hidden), in which case we are left to use only its trailing boundary.
        // This adjustment is useful in order to find the inner-most tree element that
        // lines up horizontally with the location of the event. If the mouse event
        // happened in the space preceding a nested tree element (in the leading indentated
        // space) we use this adjustment to get the nested tree element and not a tree element
        // from a parent / outer tree outline / tree element.
        //
        // NOTE: This can fail if there is floating content over the trailing edge of
        // the <li> content, since the element from point could hit that.
        let isRTL = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL;
        let trailingEdgeOffset = isRTL ? 36 : (scrollContainer.offsetWidth - 36);
        let x = scrollContainer.totalOffsetLeft + trailingEdgeOffset;
        let y = event.pageY;

        // Our list items have 1-pixel cracks between them vertically. We avoid
        // the cracks by checking slightly above and slightly below the mouse
        // and seeing if we hit the same element each time.
        let elementUnderMouse = this.treeElementFromPoint(x, y);
        let elementAboveMouse = this.treeElementFromPoint(x, y - 2);
        let element = null;
        if (elementUnderMouse === elementAboveMouse)
            element = elementUnderMouse;
        else
            element = this.treeElementFromPoint(x, y + 2);

        return element;
    }

    populateContextMenu(contextMenu, event, treeElement)
    {
        treeElement.populateContextMenu(contextMenu, event);
    }

    // Private

    static _generateStyleRulesIfNeeded()
    {
        if (WI.TreeOutline._styleElement)
           return;

        WI.TreeOutline._styleElement = document.createElement("style");

        let maximumTreeDepth = 32;
        let baseLeftPadding = 5; // Matches the padding in TreeOutline.css for the item class. Keep in sync.
        let depthPadding = 10;

        let styleText = "";
        let childrenSubstring = "";
        for (let i = 1; i <= maximumTreeDepth; ++i) {
            // Keep all the elements at the same depth once the maximum is reached.
            childrenSubstring += i === maximumTreeDepth ? " .children" : " > .children";
            styleText += `.${WI.TreeOutline.ElementStyleClassName}:not(.${WI.TreeOutline.CustomIndentStyleClassName})${childrenSubstring} > .item { `;

            if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL)
                styleText += "padding-right: ";
            else
                styleText += "padding-left: ";

            styleText += (baseLeftPadding + (depthPadding * i)) + "px; }\n";
        }

        WI.TreeOutline._styleElement.textContent = styleText;

        document.head.appendChild(WI.TreeOutline._styleElement);
    }

    _calculateVirtualizedValues()
    {
        let numberVisible = Math.ceil(this._virtualizedScrollContainer.offsetHeight / this._virtualizedTreeItemHeight);
        let extraRows = Math.max(numberVisible * 5, 50);
        let firstItem = Math.floor(this._virtualizedScrollContainer.scrollTop / this._virtualizedTreeItemHeight) - extraRows;
        let lastItem = firstItem + numberVisible + (extraRows * 2);
        return {
            numberVisible,
            extraRows,
            firstItem,
            lastItem,
        };
    }

    _handleContextmenu(event)
    {
        let treeElement = this.treeElementFromEvent(event);
        if (!treeElement)
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);
        this.populateContextMenu(contextMenu, event, treeElement);
    }
};

WI.TreeOutline._styleElement = null;

WI.TreeOutline.ElementStyleClassName = "tree-outline";
WI.TreeOutline.CustomIndentStyleClassName = "custom-indent";

WI.TreeOutline.Event = {
    ElementAdded: Symbol("element-added"),
    ElementDidChange: Symbol("element-did-change"),
    ElementRemoved: Symbol("element-removed"),
    ElementClicked: Symbol("element-clicked"),
    ElementDisclosureDidChanged: Symbol("element-disclosure-did-change"),
    ElementVisibilityDidChange: Symbol("element-visbility-did-change"),
    SelectionDidChange: Symbol("selection-did-change")
};

WI.TreeOutline._knownTreeElementNextIdentifier = 1;
