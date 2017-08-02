/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WI.TabBar = class TabBar extends WI.View
{
    constructor(element, tabBarItems)
    {
        super(element);

        this.element.classList.add("tab-bar");
        this.element.setAttribute("role", "tablist");
        this.element.addEventListener("mousedown", this._handleMouseDown.bind(this));
        this.element.addEventListener("click", this._handleClick.bind(this));
        this.element.addEventListener("mouseleave", this._handleMouseLeave.bind(this));

        this.element.createChild("div", "top-border");

        this._tabBarItems = [];

        if (tabBarItems) {
            for (let tabBarItem in tabBarItems)
                this.addTabBarItem(tabBarItem);
        }

        this.addTabBarItem(WI.settingsTabContentView.tabBarItem, {suppressAnimations: true});

        this._newTabTabBarItem = new WI.PinnedTabBarItem("Images/NewTabPlus.svg", WI.UIString("Create a new tab"));
        this._newTabTabBarItem.element.addEventListener("mouseenter", this._handleNewTabMouseEnter.bind(this));
        this._newTabTabBarItem.element.addEventListener("click", this._handleNewTabClick.bind(this));
        this.addTabBarItem(this._newTabTabBarItem, {suppressAnimations: true});
    }

    // Public

    get newTabTabBarItem() { return this._newTabTabBarItem; }

    updateNewTabTabBarItemState()
    {
        let newTabExists = !WI.isNewTabWithTypeAllowed(WI.NewTabContentView.Type);
        this._newTabTabBarItem.disabled = newTabExists;
    }

    addTabBarItem(tabBarItem, options = {})
    {
        return this.insertTabBarItem(tabBarItem, this._tabBarItems.length, options);
    }

    insertTabBarItem(tabBarItem, index, options = {})
    {
        console.assert(tabBarItem instanceof WI.TabBarItem);
        if (!(tabBarItem instanceof WI.TabBarItem))
            return null;

        if (tabBarItem.parentTabBar === this)
            return null;

        if (this._tabAnimatedClosedSinceMouseEnter) {
            // Delay adding the new tab until we can expand the tabs after a closed tab.
            this._finishExpandingTabsAfterClose().then(() => {
                this.insertTabBarItem(tabBarItem, index, options);
            });
            return null;
        }

        if (tabBarItem.parentTabBar)
            tabBarItem.parentTabBar.removeTabBarItem(tabBarItem);

        tabBarItem.parentTabBar = this;

        index = Number.constrain(index, 0, this.normalTabCount);

        if (this.element.classList.contains("animating")) {
            requestAnimationFrame(removeStyles.bind(this));
            options.suppressAnimations = true;
        }

        var beforeTabSizesAndPositions;
        if (!options.suppressAnimations)
            beforeTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

        this._tabBarItems.splice(index, 0, tabBarItem);

        var nextSibling = this._tabBarItems[index + 1];
        let nextSiblingElement = nextSibling ? nextSibling.element : this._tabBarItems.lastValue.element;

        if (this.element.isAncestor(nextSiblingElement))
            this.element.insertBefore(tabBarItem.element, nextSiblingElement);
        else
            this.element.appendChild(tabBarItem.element);

        this.element.classList.toggle("single-tab", !this._hasMoreThanOneNormalTab());

        tabBarItem.element.style.left = null;
        tabBarItem.element.style.width = null;

        function animateTabs()
        {
            this.element.classList.add("animating");
            this.element.classList.add("inserting-tab");

            this._applyTabBarItemSizesAndPositions(afterTabSizesAndPositions);

            this.element.addEventListener("webkitTransitionEnd", removeStylesListener);
        }

        function removeStyles()
        {
            this.element.classList.remove("static-layout");
            this.element.classList.remove("animating");
            this.element.classList.remove("inserting-tab");

            tabBarItem.element.classList.remove("being-inserted");

            this._clearTabBarItemSizesAndPositions();

            this.element.removeEventListener("webkitTransitionEnd", removeStylesListener);
        }

        if (!options.suppressAnimations) {
            var afterTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

            this.updateLayout();

            let tabBarItems = this._tabBarItemsFromLeftToRight();
            let previousTabBarItem = tabBarItems[tabBarItems.indexOf(tabBarItem) - 1] || null;
            let previousTabBarItemSizeAndPosition = previousTabBarItem ? beforeTabSizesAndPositions.get(previousTabBarItem) : null;

            if (previousTabBarItemSizeAndPosition)
                beforeTabSizesAndPositions.set(tabBarItem, {left: previousTabBarItemSizeAndPosition.left + previousTabBarItemSizeAndPosition.width, width: 0});
            else
                beforeTabSizesAndPositions.set(tabBarItem, {left: 0, width: 0});

            this.element.classList.add("static-layout");
            tabBarItem.element.classList.add("being-inserted");

            this._applyTabBarItemSizesAndPositions(beforeTabSizesAndPositions);

            var removeStylesListener = removeStyles.bind(this);

            requestAnimationFrame(animateTabs.bind(this));
        } else
            this.needsLayout();

        if (!(tabBarItem instanceof WI.PinnedTabBarItem))
            this.updateNewTabTabBarItemState();

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemAdded, {tabBarItem});

        return tabBarItem;
    }

    removeTabBarItem(tabBarItemOrIndex, options = {})
    {
        let tabBarItem = this._findTabBarItem(tabBarItemOrIndex);
        if (!tabBarItem || tabBarItem instanceof WI.PinnedTabBarItem)
            return null;

        tabBarItem.parentTabBar = null;

        if (this._selectedTabBarItem === tabBarItem) {
            var index = this._tabBarItems.indexOf(tabBarItem);
            var nextTabBarItem = this._tabBarItems[index + 1];
            if (!nextTabBarItem || nextTabBarItem instanceof WI.PinnedTabBarItem)
                nextTabBarItem = this._tabBarItems[index - 1];

            this.selectedTabBarItem = nextTabBarItem;
        }

        if (this.element.classList.contains("animating")) {
            requestAnimationFrame(removeStyles.bind(this));
            options.suppressAnimations = true;
        }

        var beforeTabSizesAndPositions;
        if (!options.suppressAnimations)
            beforeTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

        // Subtract 1 from normalTabCount since arrays begin indexing at 0.
        let wasLastNormalTab = this._tabBarItems.indexOf(tabBarItem) === this.normalTabCount - 1;

        this._tabBarItems.remove(tabBarItem);
        tabBarItem.element.remove();

        var hasMoreThanOneNormalTab = this._hasMoreThanOneNormalTab();
        this.element.classList.toggle("single-tab", !hasMoreThanOneNormalTab);

        const shouldOpenDefaultTab = !tabBarItem.isDefaultTab && !this.normalTabCount;
        if (shouldOpenDefaultTab)
            options.suppressAnimations = true;

        if (!hasMoreThanOneNormalTab || wasLastNormalTab || !options.suppressExpansion) {
            if (!options.suppressAnimations) {
                this._tabAnimatedClosedSinceMouseEnter = true;
                this._finishExpandingTabsAfterClose(beforeTabSizesAndPositions);
            } else
                this.needsLayout();

            this.updateNewTabTabBarItemState();

            this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemRemoved, {tabBarItem});

            if (shouldOpenDefaultTab)
                this._openDefaultTab();

            return tabBarItem;
        }

        var lastNormalTabBarItem;

        function animateTabs()
        {
            this.element.classList.add("animating");
            this.element.classList.add("closing-tab");

            // For RTL, we need to place extra space between pinned tab and first normal tab.
            // From left to right there is pinned tabs, extra space, then normal tabs. Compute
            // how much extra space we need to additionally add for normal tab items.
            let extraSpaceBetweenNormalAndPinnedTabs = 0;
            if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL) {
                extraSpaceBetweenNormalAndPinnedTabs = this.element.getBoundingClientRect().width;
                for (let currentTabBarItem of this._tabBarItemsFromLeftToRight())
                    extraSpaceBetweenNormalAndPinnedTabs -= currentTabBarItem.element.getBoundingClientRect().width;
            }

            let left = 0;
            for (let currentTabBarItem of this._tabBarItemsFromLeftToRight()) {
                let sizeAndPosition = beforeTabSizesAndPositions.get(currentTabBarItem);

                if (!(currentTabBarItem instanceof WI.PinnedTabBarItem)) {
                    currentTabBarItem.element.style.left = extraSpaceBetweenNormalAndPinnedTabs + left + "px";
                    left += sizeAndPosition.width;
                    lastNormalTabBarItem = currentTabBarItem;
                } else
                    left = sizeAndPosition.left + sizeAndPosition.width;
            }

            // The selected tab and last tab need to draw a right border as well, so make them 1px wider.
            if (this._selectedTabBarItem)
                this._selectedTabBarItem.element.style.width = (parseFloat(this._selectedTabBarItem.element.style.width) + 1) + "px";

            if (lastNormalTabBarItem !== this._selectedTabBarItem)
                lastNormalTabBarItem.element.style.width = (parseFloat(lastNormalTabBarItem.element.style.width) + 1) + "px";

            this.element.addEventListener("webkitTransitionEnd", removeStylesListener);
        }

        function removeStyles()
        {
            // The selected tab needs to stop drawing the right border, so make it 1px smaller. Only if it isn't the last.
            if (this._selectedTabBarItem && this._selectedTabBarItem !== lastNormalTabBarItem)
                this._selectedTabBarItem.element.style.width = (parseFloat(this._selectedTabBarItem.element.style.width) - 1) + "px";

            this.element.classList.remove("animating");
            this.element.classList.remove("closing-tab");

            this.updateLayout();

            this.element.removeEventListener("webkitTransitionEnd", removeStylesListener);
        }

        if (!options.suppressAnimations) {
            this.element.classList.add("static-layout");

            this._tabAnimatedClosedSinceMouseEnter = true;

            this._applyTabBarItemSizesAndPositions(beforeTabSizesAndPositions);

            var removeStylesListener = removeStyles.bind(this);

            requestAnimationFrame(animateTabs.bind(this));
        } else
            this.needsLayout();

        this.updateNewTabTabBarItemState();

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemRemoved, {tabBarItem});

        if (shouldOpenDefaultTab)
            this._openDefaultTab();

        return tabBarItem;
    }

    selectPreviousTab()
    {
        if (this._tabBarItems.length <= 1)
            return;

        var startIndex = this._tabBarItems.indexOf(this._selectedTabBarItem);
        var newIndex = startIndex;
        do {
            if (newIndex === 0)
                newIndex = this._tabBarItems.length - 1;
            else
                newIndex--;

            if (!(this._tabBarItems[newIndex] instanceof WI.PinnedTabBarItem))
                break;
        } while (newIndex !== startIndex);

        if (newIndex === startIndex)
            return;

        this.selectedTabBarItem = this._tabBarItems[newIndex];
    }

    selectNextTab()
    {
        if (this._tabBarItems.length <= 1)
            return;

        var startIndex = this._tabBarItems.indexOf(this._selectedTabBarItem);
        var newIndex = startIndex;
        do {
            if (newIndex === this._tabBarItems.length - 1)
                newIndex = 0;
            else
                newIndex++;

            if (!(this._tabBarItems[newIndex] instanceof WI.PinnedTabBarItem))
                break;
        } while (newIndex !== startIndex);

        if (newIndex === startIndex)
            return;

        this.selectedTabBarItem = this._tabBarItems[newIndex];
    }

    get selectedTabBarItem()
    {
        return this._selectedTabBarItem;
    }

    set selectedTabBarItem(tabBarItemOrIndex)
    {
        let tabBarItem = this._findTabBarItem(tabBarItemOrIndex);
        if (tabBarItem === this._newTabTabBarItem) {
            // Get the item before the New-Tab item since it is not selectable.
            tabBarItem = this._tabBarItems[this.normalTabCount - 1];
        }

        if (this._selectedTabBarItem === tabBarItem)
            return;

        if (this._selectedTabBarItem)
            this._selectedTabBarItem.selected = false;

        this._selectedTabBarItem = tabBarItem || null;

        if (this._selectedTabBarItem)
            this._selectedTabBarItem.selected = true;

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemSelected);
    }

    get tabBarItems()
    {
        return this._tabBarItems;
    }

    get normalTabCount()
    {
        return this._tabBarItems.filter((item) => !(item instanceof WI.PinnedTabBarItem)).length;
    }

    // Protected

    layout()
    {
        if (this.element.classList.contains("static-layout"))
            return;

        this.element.classList.remove("hide-titles");
        this.element.classList.remove("collapsed");

        let firstNormalTabItem = null;
        for (let tabItem of this._tabBarItems) {
            if (tabItem instanceof WI.PinnedTabBarItem)
                continue;

            firstNormalTabItem = tabItem;
            break;
        }

        if (!firstNormalTabItem)
            return;

        if (firstNormalTabItem.element.offsetWidth >= 120)
            return;

        this.element.classList.add("collapsed");

        if (firstNormalTabItem.element.offsetWidth >= 75)
            return;

        this.element.classList.add("hide-titles");
    }

    // Private

    _tabBarItemsFromLeftToRight()
    {
        return WI.resolvedLayoutDirection() === WI.LayoutDirection.LTR ? this._tabBarItems : this._tabBarItems.slice().reverse();
    }

    _findTabBarItem(tabBarItemOrIndex)
    {
        if (typeof tabBarItemOrIndex === "number")
            return this._tabBarItems[tabBarItemOrIndex] || null;

        if (tabBarItemOrIndex instanceof WI.TabBarItem) {
            if (this._tabBarItems.includes(tabBarItemOrIndex))
                return tabBarItemOrIndex;
        }

        return null;
    }

    _hasMoreThanOneNormalTab()
    {
        let normalTabCount = 0;
        for (let tabBarItem of this._tabBarItems) {
            if (tabBarItem instanceof WI.PinnedTabBarItem)
                continue;

            ++normalTabCount;
            if (normalTabCount >= 2)
                return true;
        }

        return false;
    }

    _openDefaultTab()
    {
        this.dispatchEventToListeners(WI.TabBar.Event.OpenDefaultTab);
    }

    _recordTabBarItemSizesAndPositions()
    {
        var tabBarItemSizesAndPositions = new Map;

        const barRect = this.element.getBoundingClientRect();

        for (var tabBarItem of this._tabBarItems) {
            var boundingRect = tabBarItem.element.getBoundingClientRect();
            tabBarItemSizesAndPositions.set(tabBarItem, {left: boundingRect.left - barRect.left, width: boundingRect.width});
        }

        return tabBarItemSizesAndPositions;
    }

    _applyTabBarItemSizesAndPositions(tabBarItemSizesAndPositions, skipTabBarItem)
    {
        for (var [tabBarItem, sizeAndPosition] of tabBarItemSizesAndPositions) {
            if (skipTabBarItem && tabBarItem === skipTabBarItem)
                continue;
            tabBarItem.element.style.left = sizeAndPosition.left + "px";
            tabBarItem.element.style.width = sizeAndPosition.width + "px";
        }
    }

    _clearTabBarItemSizesAndPositions(skipTabBarItem)
    {
        for (var tabBarItem of this._tabBarItems) {
            if (skipTabBarItem && tabBarItem === skipTabBarItem)
                continue;
            tabBarItem.element.style.left = null;
            tabBarItem.element.style.width = null;
        }
    }

    _finishExpandingTabsAfterClose(beforeTabSizesAndPositions)
    {
        return new Promise(function(resolve, reject) {
            console.assert(this._tabAnimatedClosedSinceMouseEnter);
            this._tabAnimatedClosedSinceMouseEnter = false;

            if (!beforeTabSizesAndPositions)
                beforeTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

            this.element.classList.remove("static-layout");
            this._clearTabBarItemSizesAndPositions();

            var afterTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

            this._applyTabBarItemSizesAndPositions(beforeTabSizesAndPositions);
            this.element.classList.add("static-layout");

            function animateTabs()
            {
                this.element.classList.add("static-layout");
                this.element.classList.add("animating");
                this.element.classList.add("expanding-tabs");

                this._applyTabBarItemSizesAndPositions(afterTabSizesAndPositions);

                this.element.addEventListener("webkitTransitionEnd", removeStylesListener);
            }

            function removeStyles()
            {
                this.element.classList.remove("static-layout");
                this.element.classList.remove("animating");
                this.element.classList.remove("expanding-tabs");

                this._clearTabBarItemSizesAndPositions();

                this.updateLayout();

                this.element.removeEventListener("webkitTransitionEnd", removeStylesListener);

                resolve();
            }

            var removeStylesListener = removeStyles.bind(this);

            requestAnimationFrame(animateTabs.bind(this));
        }.bind(this));
    }

    _handleMouseDown(event)
    {
        // Only consider left mouse clicks for tab movement.
        if (event.button !== 0 || event.ctrlKey)
            return;

        let itemElement = event.target.enclosingNodeOrSelfWithClass(WI.TabBarItem.StyleClassName);
        if (!itemElement)
            return;

        let tabBarItem = itemElement[WI.TabBarItem.ElementReferenceSymbol];
        if (!tabBarItem)
            return;

        if (tabBarItem.disabled)
            return;

        if (tabBarItem === this._newTabTabBarItem)
            return;

        let closeButtonElement = event.target.enclosingNodeOrSelfWithClass(WI.TabBarItem.CloseButtonStyleClassName);
        if (closeButtonElement)
            return;

        this.selectedTabBarItem = tabBarItem;

        if (tabBarItem instanceof WI.PinnedTabBarItem || !this._hasMoreThanOneNormalTab())
            return;

        this._firstNormalTabItemIndex = 0;
        for (let i = 0; i < this._tabBarItems.length; ++i) {
            if (this._tabBarItems[i] instanceof WI.PinnedTabBarItem)
                continue;

            this._firstNormalTabItemIndex = i;
            break;
        }

        this._mouseIsDown = true;

        this._mouseMovedEventListener = this._handleMouseMoved.bind(this);
        this._mouseUpEventListener = this._handleMouseUp.bind(this);

        // Register these listeners on the document so we can track the mouse if it leaves the tab bar.
        document.addEventListener("mousemove", this._mouseMovedEventListener, true);
        document.addEventListener("mouseup", this._mouseUpEventListener, true);

        event.preventDefault();
        event.stopPropagation();
    }

    _handleClick(event)
    {
        var itemElement = event.target.enclosingNodeOrSelfWithClass(WI.TabBarItem.StyleClassName);
        if (!itemElement)
            return;

        var tabBarItem = itemElement[WI.TabBarItem.ElementReferenceSymbol];
        if (!tabBarItem)
            return;

        if (tabBarItem.disabled)
            return;

        const clickedMiddleButton = event.button === 1;

        var closeButtonElement = event.target.enclosingNodeOrSelfWithClass(WI.TabBarItem.CloseButtonStyleClassName);
        if (closeButtonElement || clickedMiddleButton) {
            // Disallow closing the default tab if it is the only tab.
            if (tabBarItem.isDefaultTab && this.element.classList.contains("single-tab"))
                return;

            if (!event.altKey) {
                this.removeTabBarItem(tabBarItem, {suppressExpansion: true});
                return;
            }

            for (let i = this._tabBarItems.length - 1; i >= 0; --i) {
                let item = this._tabBarItems[i];
                if (item === tabBarItem || item instanceof WI.PinnedTabBarItem)
                    continue;
                this.removeTabBarItem(item);
            }
        }
    }

    _handleMouseMoved(event)
    {
        console.assert(event.button === 0);
        console.assert(this._mouseIsDown);
        if (!this._mouseIsDown)
            return;

        console.assert(this._selectedTabBarItem);
        if (!this._selectedTabBarItem)
            return;

        event.preventDefault();
        event.stopPropagation();

        if (!this.element.classList.contains("static-layout")) {
            this._applyTabBarItemSizesAndPositions(this._recordTabBarItemSizesAndPositions());
            this.element.classList.add("static-layout");
            this.element.classList.add("dragging-tab");
        }

        if (this._mouseOffset === undefined)
            this._mouseOffset = event.pageX - this._selectedTabBarItem.element.totalOffsetLeft;

        var tabBarMouseOffset = event.pageX - this.element.totalOffsetLeft;
        var newLeft = tabBarMouseOffset - this._mouseOffset;

        this._selectedTabBarItem.element.style.left = newLeft + "px";

        var selectedTabMidX = newLeft + (this._selectedTabBarItem.element.realOffsetWidth / 2);

        var currentIndex = this._tabBarItems.indexOf(this._selectedTabBarItem);
        var newIndex = currentIndex;

        for (let tabBarItem of this._tabBarItems) {
            if (tabBarItem === this._selectedTabBarItem)
                continue;

            var tabBarItemRect = tabBarItem.element.getBoundingClientRect();

            if (selectedTabMidX < tabBarItemRect.left || selectedTabMidX > tabBarItemRect.right)
                continue;

            newIndex = this._tabBarItems.indexOf(tabBarItem);
            break;
        }

        // Subtract 1 from normalTabCount since arrays begin indexing at 0.
        newIndex = Number.constrain(newIndex, this._firstNormalTabItemIndex, this.normalTabCount - 1);

        if (currentIndex === newIndex)
            return;

        this._tabBarItems.splice(currentIndex, 1);
        this._tabBarItems.splice(newIndex, 0, this._selectedTabBarItem);

        let nextSibling = this._tabBarItems[newIndex + 1];
        let nextSiblingElement = nextSibling ? nextSibling.element : this._newTabTabBarItem.element;

        this.element.insertBefore(this._selectedTabBarItem.element, nextSiblingElement);

        // FIXME: Animate the tabs that move to make room for the selected tab. This was causing me trouble when I tried.

        let left = 0;
        for (let tabBarItem of this._tabBarItemsFromLeftToRight()) {
            if (tabBarItem !== this._selectedTabBarItem && tabBarItem !== this._newTabTabBarItem && parseFloat(tabBarItem.element.style.left) !== left)
                tabBarItem.element.style.left = left + "px";
            left += parseFloat(tabBarItem.element.style.width);
        }
    }

    _handleMouseUp(event)
    {
        console.assert(event.button === 0);
        console.assert(this._mouseIsDown);
        if (!this._mouseIsDown)
            return;

        this.element.classList.remove("dragging-tab");

        if (!this._tabAnimatedClosedSinceMouseEnter) {
            this.element.classList.remove("static-layout");
            this._clearTabBarItemSizesAndPositions();
        } else {
            let left = 0;
            for (let tabBarItem of this._tabBarItemsFromLeftToRight()) {
                if (tabBarItem === this._selectedTabBarItem)
                    tabBarItem.element.style.left = left + "px";
                left += parseFloat(tabBarItem.element.style.width);
            }
        }

        this._mouseIsDown = false;
        this._mouseOffset = undefined;

        document.removeEventListener("mousemove", this._mouseMovedEventListener, true);
        document.removeEventListener("mouseup", this._mouseUpEventListener, true);

        this._mouseMovedEventListener = null;
        this._mouseUpEventListener = null;

        event.preventDefault();
        event.stopPropagation();

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemsReordered);
    }

    _handleMouseLeave(event)
    {
        if (this._mouseIsDown || !this._tabAnimatedClosedSinceMouseEnter || !this.element.classList.contains("static-layout") || this.element.classList.contains("animating"))
            return;

        // This event can still fire when the mouse is inside the element if DOM nodes are added, removed or generally change inside.
        // Check if the mouse really did leave the element by checking the bounds.
        // FIXME: Is this a WebKit bug or correct behavior?
        const barRect = this.element.getBoundingClientRect();
        const newTabItemRect = this._newTabTabBarItem.element.getBoundingClientRect();
        if (event.pageY > barRect.top && event.pageY < barRect.bottom && event.pageX > barRect.left && event.pageX < (newTabItemRect ? newTabItemRect.right : barRect.right))
            return;

        this._finishExpandingTabsAfterClose();
    }

    _handleNewTabClick(event)
    {
        WI.showNewTabTab();
    }

    _handleNewTabMouseEnter(event)
    {
        if (!this._tabAnimatedClosedSinceMouseEnter || !this.element.classList.contains("static-layout") || this.element.classList.contains("animating"))
            return;

        this._finishExpandingTabsAfterClose();
    }
};

WI.TabBar.Event = {
    TabBarItemSelected: "tab-bar-tab-bar-item-selected",
    TabBarItemAdded: "tab-bar-tab-bar-item-added",
    TabBarItemRemoved: "tab-bar-tab-bar-item-removed",
    TabBarItemsReordered: "tab-bar-tab-bar-items-reordered",
    OpenDefaultTab: "tab-bar-open-default-tab"
};
