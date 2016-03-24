/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.NavigationBar = class NavigationBar extends WebInspector.View
{
    constructor(element, navigationItems, role, label)
    {
        super(element);

        this.element.classList.add(this.constructor.StyleClassName || "navigation-bar");
        this.element.tabIndex = 0;

        if (role)
            this.element.setAttribute("role", role);
        if (label)
            this.element.setAttribute("aria-label", label);

        this.element.addEventListener("focus", this._focus.bind(this), false);
        this.element.addEventListener("blur", this._blur.bind(this), false);
        this.element.addEventListener("keydown", this._keyDown.bind(this), false);
        this.element.addEventListener("mousedown", this._mouseDown.bind(this), false);

        this._forceLayout = false;
        this._minimumWidth = NaN;
        this._navigationItems = [];

        if (navigationItems) {
            for (var i = 0; i < navigationItems.length; ++i)
                this.addNavigationItem(navigationItems[i]);
        }
    }

    // Public

    addNavigationItem(navigationItem, parentElement)
    {
        return this.insertNavigationItem(navigationItem, this._navigationItems.length, parentElement);
    }

    insertNavigationItem(navigationItem, index, parentElement)
    {
        console.assert(navigationItem instanceof WebInspector.NavigationItem);
        if (!(navigationItem instanceof WebInspector.NavigationItem))
            return null;

        if (navigationItem.parentNavigationBar)
            navigationItem.parentNavigationBar.removeNavigationItem(navigationItem);

        navigationItem._parentNavigationBar = this;

        console.assert(index >= 0 && index <= this._navigationItems.length);
        index = Math.max(0, Math.min(index, this._navigationItems.length));

        this._navigationItems.splice(index, 0, navigationItem);

        if (!parentElement)
            parentElement = this.element;

        var nextSibling = this._navigationItems[index + 1];
        var nextSiblingElement = nextSibling ? nextSibling.element : null;
        if (nextSiblingElement && nextSiblingElement.parentNode !== parentElement)
            nextSiblingElement = null;

        parentElement.insertBefore(navigationItem.element, nextSiblingElement);

        this._minimumWidth = NaN;

        this.needsLayout();

        return navigationItem;
    }

    removeNavigationItem(navigationItemOrIdentifierOrIndex)
    {
        var navigationItem = this._findNavigationItem(navigationItemOrIdentifierOrIndex);
        if (!navigationItem)
            return null;

        navigationItem._parentNavigationBar = null;

        if (this._selectedNavigationItem === navigationItem)
            this.selectedNavigationItem = null;

        this._navigationItems.remove(navigationItem);
        navigationItem.element.remove();

        this._minimumWidth = NaN;

        this.needsLayout();

        return navigationItem;
    }

    get selectedNavigationItem()
    {
        return this._selectedNavigationItem || null;
    }

    set selectedNavigationItem(navigationItemOrIdentifierOrIndex)
    {
        var navigationItem = this._findNavigationItem(navigationItemOrIdentifierOrIndex);

        // Only radio navigation items can be selected.
        if (!(navigationItem instanceof WebInspector.RadioButtonNavigationItem))
            navigationItem = null;

        if (this._selectedNavigationItem === navigationItem)
            return;

        if (this._selectedNavigationItem)
            this._selectedNavigationItem.selected = false;

        this._selectedNavigationItem = navigationItem || null;

        if (this._selectedNavigationItem)
            this._selectedNavigationItem.selected = true;

        // When the mouse is down don't dispatch the selected event, it will be dispatched on mouse up.
        // This prevents sending the event while the user is scrubbing the bar.
        if (!this._mouseIsDown)
            this.dispatchEventToListeners(WebInspector.NavigationBar.Event.NavigationItemSelected);
    }

    get navigationItems()
    {
        return this._navigationItems;
    }

    get minimumWidth()
    {
        if (isNaN(this._minimumWidth))
            this._minimumWidth = this._calculateMinimumWidth();
        return this._minimumWidth;
    }

    get sizesToFit()
    {
        // Can be overriden by subclasses.
        return false;
    }

    needsLayout()
    {
        this._forceLayout = true;

        super.needsLayout();
    }

    layout(layoutReason)
    {
        if (layoutReason !== WebInspector.View.LayoutReason.Resize && !this._forceLayout)
            return;

        this._forceLayout = false;

        // Remove the collapsed style class to test if the items can fit at full width.
        this.element.classList.remove(WebInspector.NavigationBar.CollapsedStyleClassName);

        // Tell each navigation item to update to full width if needed.
        for (let navigationItem of this._navigationItems)
            navigationItem.updateLayout(true);

        let totalItemWidth = 0;
        for (let navigationItem of this._navigationItems) {
            // Skip flexible space items since they can take up no space at the minimum width.
            if (navigationItem instanceof WebInspector.FlexibleSpaceNavigationItem)
                continue;

            totalItemWidth += navigationItem.element.realOffsetWidth;
        }

        const barWidth = this.element.realOffsetWidth;

        // Add the collapsed class back if the items are wider than the bar.
        if (totalItemWidth > barWidth)
            this.element.classList.add(WebInspector.NavigationBar.CollapsedStyleClassName);

        // Give each navigation item the opportunity to collapse further.
        for (let navigationItem of this._navigationItems)
            navigationItem.updateLayout(false);
    }

    // Private

    _findNavigationItem(navigationItemOrIdentifierOrIndex)
    {
        var navigationItem = null;

        if (navigationItemOrIdentifierOrIndex instanceof WebInspector.NavigationItem) {
            if (this._navigationItems.includes(navigationItemOrIdentifierOrIndex))
                navigationItem = navigationItemOrIdentifierOrIndex;
        } else if (typeof navigationItemOrIdentifierOrIndex === "number") {
            navigationItem = this._navigationItems[navigationItemOrIdentifierOrIndex];
        } else if (typeof navigationItemOrIdentifierOrIndex === "string") {
            for (var i = 0; i < this._navigationItems.length; ++i) {
                if (this._navigationItems[i].identifier === navigationItemOrIdentifierOrIndex) {
                    navigationItem = this._navigationItems[i];
                    break;
                }
            }
        }

        return navigationItem;
    }

    _mouseDown(event)
    {
        // Only handle left mouse clicks.
        if (event.button !== 0)
            return;

        // Remove the tabIndex so clicking the navigation bar does not give it focus.
        // Only keep the tabIndex if already focused from keyboard navigation. This matches Xcode.
        if (!this._focused)
            this.element.removeAttribute("tabindex");

        var itemElement = event.target.enclosingNodeOrSelfWithClass(WebInspector.RadioButtonNavigationItem.StyleClassName);
        if (!itemElement || !itemElement.navigationItem)
            return;

        this._previousSelectedNavigationItem = this.selectedNavigationItem;
        this.selectedNavigationItem = itemElement.navigationItem;

        this._mouseIsDown = true;

        this._mouseMovedEventListener = this._mouseMoved.bind(this);
        this._mouseUpEventListener = this._mouseUp.bind(this);

        if (typeof this.selectedNavigationItem.dontPreventDefaultOnNavigationBarMouseDown === "function"
            && this.selectedNavigationItem.dontPreventDefaultOnNavigationBarMouseDown()
            && this._previousSelectedNavigationItem === this.selectedNavigationItem)
            return;

        // Register these listeners on the document so we can track the mouse if it leaves the navigation bar.
        document.addEventListener("mousemove", this._mouseMovedEventListener, false);
        document.addEventListener("mouseup", this._mouseUpEventListener, false);

        event.preventDefault();
        event.stopPropagation();
    }

    _mouseMoved(event)
    {
        console.assert(event.button === 0);
        console.assert(this._mouseIsDown);
        if (!this._mouseIsDown)
            return;

        event.preventDefault();
        event.stopPropagation();

        var itemElement = event.target.enclosingNodeOrSelfWithClass(WebInspector.RadioButtonNavigationItem.StyleClassName);
        if (!itemElement || !itemElement.navigationItem || !this.element.contains(itemElement)) {
            // Find the element that is at the X position of the mouse, even when the mouse is no longer
            // vertically in the navigation bar.
            var element = document.elementFromPoint(event.pageX, this.element.totalOffsetTop + (this.element.offsetHeight / 2));
            if (!element)
                return;

            itemElement = element.enclosingNodeOrSelfWithClass(WebInspector.RadioButtonNavigationItem.StyleClassName);
            if (!itemElement || !itemElement.navigationItem || !this.element.contains(itemElement))
                return;
        }

        if (this.selectedNavigationItem)
            this.selectedNavigationItem.active = false;

        this.selectedNavigationItem = itemElement.navigationItem;

        this.selectedNavigationItem.active = true;
    }

    _mouseUp(event)
    {
        console.assert(event.button === 0);
        console.assert(this._mouseIsDown);
        if (!this._mouseIsDown)
            return;

        if (this.selectedNavigationItem)
            this.selectedNavigationItem.active = false;

        this._mouseIsDown = false;

        document.removeEventListener("mousemove", this._mouseMovedEventListener, false);
        document.removeEventListener("mouseup", this._mouseUpEventListener, false);

        delete this._mouseMovedEventListener;
        delete this._mouseUpEventListener;

        // Restore the tabIndex so the navigation bar can be in the keyboard tab loop.
        this.element.tabIndex = 0;

        // Dispatch the selected event here since the selectedNavigationItem setter surpresses it
        // while the mouse is down to prevent sending it while scrubbing the bar.
        if (this._previousSelectedNavigationItem !== this.selectedNavigationItem)
            this.dispatchEventToListeners(WebInspector.NavigationBar.Event.NavigationItemSelected);

        delete this._previousSelectedNavigationItem;

        event.preventDefault();
        event.stopPropagation();
    }

    _keyDown(event)
    {
        if (!this._focused)
            return;

        if (event.keyIdentifier !== "Left" && event.keyIdentifier !== "Right")
            return;

        event.preventDefault();
        event.stopPropagation();

        var selectedNavigationItemIndex = this._navigationItems.indexOf(this._selectedNavigationItem);

        if (event.keyIdentifier === "Left") {
            if (selectedNavigationItemIndex === -1)
                selectedNavigationItemIndex = this._navigationItems.length;

            do {
                selectedNavigationItemIndex = Math.max(0, selectedNavigationItemIndex - 1);
            } while (selectedNavigationItemIndex && !(this._navigationItems[selectedNavigationItemIndex] instanceof WebInspector.RadioButtonNavigationItem));
        } else if (event.keyIdentifier === "Right") {
            do {
                selectedNavigationItemIndex = Math.min(selectedNavigationItemIndex + 1, this._navigationItems.length - 1);
            } while (selectedNavigationItemIndex < this._navigationItems.length - 1 && !(this._navigationItems[selectedNavigationItemIndex] instanceof WebInspector.RadioButtonNavigationItem));
        }

        if (!(this._navigationItems[selectedNavigationItemIndex] instanceof WebInspector.RadioButtonNavigationItem))
            return;

        this.selectedNavigationItem = this._navigationItems[selectedNavigationItemIndex];
    }

    _focus(event)
    {
        this._focused = true;
    }

    _blur(event)
    {
        this._focused = false;
    }

    _calculateMinimumWidth()
    {
        const wasCollapsed = this.element.classList.contains(WebInspector.NavigationBar.CollapsedStyleClassName);

        // Add the collapsed style class to calculate the width of the items when they are collapsed.
        if (!wasCollapsed)
            this.element.classList.add(WebInspector.NavigationBar.CollapsedStyleClassName);

        let totalItemWidth = 0;
        for (let item of this._navigationItems) {
            // Skip flexible space items since they can take up no space at the minimum width.
            if (item instanceof WebInspector.FlexibleSpaceNavigationItem)
                continue;

            totalItemWidth += item.minimumWidth;
        }

        // Remove the collapsed style class if we were not collapsed before.
        if (!wasCollapsed)
            this.element.classList.remove(WebInspector.NavigationBar.CollapsedStyleClassName);

        return totalItemWidth;
    }
};

WebInspector.NavigationBar.CollapsedStyleClassName = "collapsed";

WebInspector.NavigationBar.Event = {
    NavigationItemSelected: "navigation-bar-navigation-item-selected"
};
