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
    constructor(element)
    {
        super(element);

        this.element.classList.add("tab-bar");
        this.element.addEventListener("mousedown", this._handleMouseDown.bind(this));

        this.element.createChild("div", "border top");

        const navigationBarBeforeElement = null;
        this._navigationBarBefore = new WI.NavigationBar(navigationBarBeforeElement, {sizesToFit: true});
        this.addSubview(this._navigationBarBefore);

        this._tabContainer = this.element.appendChild(document.createElement("div"));
        this._tabContainer.className = "tabs";
        this._tabContainer.setAttribute("role", "tablist");
        this._tabContainer.addEventListener("mousedown", this._handleTabContainerMouseDown.bind(this));
        this._tabContainer.addEventListener("click", this._handleTabContainerClick.bind(this));
        this._tabContainer.addEventListener("mouseleave", this._handleTabContainerMouseLeave.bind(this));
        this._tabContainer.addEventListener("contextmenu", this._handleTabContainerContextMenu.bind(this));

        this._flexibleSpaceBeforeElement = this._tabContainer.appendChild(document.createElement("div"));
        this._flexibleSpaceBeforeElement.className = "flexible-space";

        this._flexibleSpaceAfterElement = this._tabContainer.appendChild(document.createElement("div"));
        this._flexibleSpaceAfterElement.className = "flexible-space";

        const navigationBarAfterElement = null;
        this._navigationBarAfter = new WI.NavigationBar(navigationBarAfterElement, {sizesToFit: true});
        this.addSubview(this._navigationBarAfter);

        this.element.createChild("div", "border bottom");

        this._tabBarItems = [];
        this._hiddenTabBarItems = [];

        const tabPickerRepresentedObject = null;
        const tabPickerDisplayName = null;
        this._tabPickerTabBarItem = new WI.PinnedTabBarItem(tabPickerRepresentedObject, "Images/TabPicker.svg", tabPickerDisplayName, WI.UIString("Show hidden tabs"));
        this._tabPickerTabBarItem.hidden = true;
        this._tabPickerTabBarItem.element.classList.add("tab-picker");
        this.addTabBarItem(this._tabPickerTabBarItem, {suppressAnimations: true});

        this._mouseDownPageX = NaN;
        this._isDragging = false;
    }

    // Static

    static get horizontalPadding()
    {
        return (WI.dockConfiguration === WI.DockConfiguration.Undocked) ? 0 : 8; // Keep in sync with `body.docked .tab-bar .tabs`
    }

    // Public

    addNavigationItemBefore(navigationItem)
    {
        this._navigationBarBefore.addNavigationItem(navigationItem);

        this.needsLayout();
    }

    addNavigationItemAfter(navigationItem)
    {
        this._navigationBarAfter.addNavigationItem(navigationItem);

        this.needsLayout();
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

        if (tabBarItem instanceof WI.GeneralTabBarItem)
            index = Number.constrain(index, 0, this.normalTabCount);
        else
            index = Number.constrain(index, this.normalTabCount, this._tabBarItems.length);

        if (this._tabContainer.classList.contains("animating")) {
            requestAnimationFrame(removeStyles.bind(this));
            options.suppressAnimations = true;
        }

        var beforeTabSizesAndPositions;
        if (!options.suppressAnimations)
            beforeTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

        this._tabBarItems.splice(index, 0, tabBarItem);

        let nextSibling = this._tabBarItems[index + 1] || this._tabBarItems.lastValue;

        if (this._tabContainer.contains(nextSibling.element)) {
            if (!(tabBarItem instanceof WI.PinnedTabBarItem) && nextSibling instanceof WI.PinnedTabBarItem && nextSibling !== this._tabPickerTabBarItem)
                this._tabContainer.insertBefore(tabBarItem.element, this._flexibleSpaceAfterElement);
            else
                this._tabContainer.insertBefore(tabBarItem.element, nextSibling.element);
        } else {
            if (tabBarItem instanceof WI.PinnedTabBarItem && tabBarItem !== this._tabPickerTabBarItem)
                this._tabContainer.appendChild(tabBarItem.element);
            else
                this._tabContainer.insertBefore(tabBarItem.element, this._flexibleSpaceAfterElement);
        }

        this._tabContainer.classList.toggle("single-tab", !this._hasMoreThanOneNormalTab());

        tabBarItem.element.style.left = null;
        tabBarItem.element.style.width = null;

        function animateTabs()
        {
            this._tabContainer.classList.add("animating");
            this._tabContainer.classList.add("inserting-tab");

            this._applyTabBarItemSizesAndPositions(afterTabSizesAndPositions);

            this._tabContainer.addEventListener("webkitTransitionEnd", removeStylesListener);
        }

        function removeStyles()
        {
            this._tabContainer.classList.remove("static-layout");
            this._tabContainer.classList.remove("animating");
            this._tabContainer.classList.remove("inserting-tab");

            tabBarItem.element.classList.remove("being-inserted");

            this._clearTabBarItemSizesAndPositions();

            this._tabContainer.removeEventListener("webkitTransitionEnd", removeStylesListener);
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

            this._tabContainer.classList.add("static-layout");
            tabBarItem.element.classList.add("being-inserted");

            this._applyTabBarItemSizesAndPositions(beforeTabSizesAndPositions);

            var removeStylesListener = removeStyles.bind(this);

            requestAnimationFrame(animateTabs.bind(this));
        } else
            this.needsLayout();

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemAdded, {tabBarItem});

        return tabBarItem;
    }

    removeTabBarItem(tabBarItemOrIndex, options = {})
    {
        let tabBarItem = this._findTabBarItem(tabBarItemOrIndex);
        if (!tabBarItem || tabBarItem instanceof WI.PinnedTabBarItem)
            return null;

        if (this.normalTabCount === 1)
            return null;

        tabBarItem.parentTabBar = null;

        if (this._selectedTabBarItem === tabBarItem) {
            var index = this._tabBarItems.indexOf(tabBarItem);
            var nextTabBarItem = this._tabBarItems[index + 1];
            if (!nextTabBarItem || nextTabBarItem instanceof WI.PinnedTabBarItem)
                nextTabBarItem = this._tabBarItems[index - 1];

            this.selectedTabBarItem = nextTabBarItem;
        }

        if (this._tabContainer.classList.contains("animating")) {
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
        this._tabContainer.classList.toggle("single-tab", !hasMoreThanOneNormalTab);

        if (!hasMoreThanOneNormalTab || wasLastNormalTab || !options.suppressExpansion) {
            if (!options.suppressAnimations) {
                this._tabAnimatedClosedSinceMouseEnter = true;
                this._finishExpandingTabsAfterClose(beforeTabSizesAndPositions);
            } else
                this.needsLayout();

            this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemRemoved, {tabBarItem});
            return tabBarItem;
        }

        var lastNormalTabBarItem;

        function animateTabs()
        {
            this._tabContainer.classList.add("animating");
            this._tabContainer.classList.add("closing-tab");

            // For RTL, we need to place extra space between pinned tab and first normal tab.
            // From left to right there is pinned tabs, extra space, then normal tabs. Compute
            // how much extra space we need to additionally add for normal tab items.
            let extraSpaceBetweenNormalAndPinnedTabs = 0;
            if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL) {
                extraSpaceBetweenNormalAndPinnedTabs = this._tabContainer.getBoundingClientRect().width;
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

            this._tabContainer.addEventListener("webkitTransitionEnd", removeStylesListener);
        }

        function removeStyles()
        {
            // The selected tab needs to stop drawing the right border, so make it 1px smaller. Only if it isn't the last.
            if (this._selectedTabBarItem && this._selectedTabBarItem !== lastNormalTabBarItem)
                this._selectedTabBarItem.element.style.width = (parseFloat(this._selectedTabBarItem.element.style.width) - 1) + "px";

            this._tabContainer.classList.remove("animating");
            this._tabContainer.classList.remove("closing-tab");

            this.updateLayout();

            this._tabContainer.removeEventListener("webkitTransitionEnd", removeStylesListener);
        }

        if (!options.suppressAnimations) {
            this._tabContainer.classList.add("static-layout");

            this._tabAnimatedClosedSinceMouseEnter = true;

            this._applyTabBarItemSizesAndPositions(beforeTabSizesAndPositions);

            var removeStylesListener = removeStyles.bind(this);

            requestAnimationFrame(animateTabs.bind(this));
        } else
            this.needsLayout();

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemRemoved, {tabBarItem});

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
        this.selectTabBarItem(tabBarItemOrIndex);
    }

    selectTabBarItem(tabBarItemOrIndex, options = {})
    {
        let tabBarItem = this._findTabBarItem(tabBarItemOrIndex);
        if (tabBarItem === this._tabPickerTabBarItem) {
            // Get the last normal tab item if the item is not selectable.
            tabBarItem = this._tabBarItems[this.normalTabCount - 1];
        }

        if (this._selectedTabBarItem === tabBarItem)
            return;

        let previousTabBarItem = this._selectedTabBarItem;

        if (this._selectedTabBarItem)
            this._selectedTabBarItem.selected = false;

        this._selectedTabBarItem = tabBarItem || null;

        if (this._selectedTabBarItem) {
            this._selectedTabBarItem.selected = true;
            if (this._selectedTabBarItem.hidden)
                this.needsLayout();
        }

        let initiatorHint = options.initiatorHint || WI.TabBrowser.TabNavigationInitiator.Unknown;
        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemSelected, {previousTabBarItem, initiatorHint});
    }

    get tabBarItems()
    {
        return this._tabBarItems;
    }

    get tabCount()
    {
        return this._tabBarItems.filter((item) => item.representedObject instanceof WI.TabContentView).length;
    }

    get normalTabCount()
    {
        return this._tabBarItems.filter((item) => !(item instanceof WI.PinnedTabBarItem)).length;
    }

    resetCachedWidths()
    {
        for (let tabBarItem of this._tabBarItems)
            tabBarItem[WI.TabBar.CachedWidthSymbol] = 0;
    }

    // Protected

    layout()
    {
        if (this._tabContainer.classList.contains("static-layout"))
            return;

        const tabBarHorizontalPadding = WI.TabBar.horizontalPadding;
        const tabBarItemHorizontalMargin = WI.TabBarItem.horizontalMargin;

        let undocked = WI.dockConfiguration === WI.DockConfiguration.Undocked;

        function measureWidth(tabBarItem) {
            if (!tabBarItem[WI.TabBar.CachedWidthSymbol])
                tabBarItem[WI.TabBar.CachedWidthSymbol] = tabBarItem.element.realOffsetWidth + tabBarItemHorizontalMargin;
            return tabBarItem[WI.TabBar.CachedWidthSymbol];
        }

        let availableSpace = this._tabContainer.realOffsetWidth - tabBarHorizontalPadding;

        this._tabContainer.classList.add("calculate-width");

        this._hiddenTabBarItems = [];

        let normalTabBarItems = [];
        for (let tabBarItem of this._tabBarItemsFromLeftToRight()) {
            if (tabBarItem === this._tabPickerTabBarItem) {
                tabBarItem.hidden = true;
                continue;
            }

            tabBarItem.hidden = false;

            if (tabBarItem instanceof WI.PinnedTabBarItem) {
                availableSpace -= measureWidth(tabBarItem);
                continue;
            }

            normalTabBarItems.push(tabBarItem);

            // When undocked, `WI.TabBarItem` grow to fill any available space. As a result, if a
            // `WI.TabBarItem` is added or removed, the width of all `WI.TabBarItem` will change.
            if (undocked)
                tabBarItem[WI.TabBar.CachedWidthSymbol] = 0;
        }

        // Wait to measure widths until all `WI.TabBarItem` are un-hidden for the reason above.
        let normalTabBarItemsWidth = normalTabBarItems.reduce((accumulator, tabBarItem) => accumulator + measureWidth(tabBarItem), 0);
        if (Math.round(normalTabBarItemsWidth) >= Math.floor(availableSpace)) {
            this._tabPickerTabBarItem.hidden = false;
            availableSpace -= measureWidth(this._tabPickerTabBarItem);

            let index = normalTabBarItems.length - 1;
            do {
                let tabBarItem = normalTabBarItems[index];
                if (tabBarItem === this._selectedTabBarItem)
                    continue;

                normalTabBarItemsWidth -= measureWidth(tabBarItem);

                tabBarItem.hidden = true;
                this._hiddenTabBarItems.push(tabBarItem);
            } while (normalTabBarItemsWidth >= availableSpace && --index >= 0);
        }

        // Tabs are marked as hidden from right to left, meaning that the right-most item will be
        // first in the list. Reverse the list so that the right-most item is last.
        this._hiddenTabBarItems.reverse();

        this._tabContainer.classList.remove("calculate-width");
    }

    didLayoutSubtree()
    {
        super.didLayoutSubtree();

        this._tabContainer.classList.toggle("hide-border-start", this._navigationBarBefore.navigationItems.every((item) => item.hidden));
        this._tabContainer.classList.toggle("hide-border-end", this._navigationBarAfter.navigationItems.every((item) => item.hidden));
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

    _recordTabBarItemSizesAndPositions()
    {
        const tabBarItemLeftMargin = WI.TabBarItem.horizontalMargin / 2;

        var tabBarItemSizesAndPositions = new Map;

        let barRect = this._tabContainer.getBoundingClientRect();

        function add(key, element, leftMargin) {
            let boundingRect = element.getBoundingClientRect();
            tabBarItemSizesAndPositions.set(key, {
                left: boundingRect.left - barRect.left - leftMargin,
                width: boundingRect.width,
            });
        }

        add(this._flexibleSpaceBeforeElement, this._flexibleSpaceBeforeElement, 0);
        add(this._flexibleSpaceAfterElement, this._flexibleSpaceAfterElement, 0);
        for (let tabBarItem of this._tabBarItems) {
            if (tabBarItem.hidden)
                continue;

            add(tabBarItem, tabBarItem.element, tabBarItemLeftMargin);
        }

        return tabBarItemSizesAndPositions;
    }

    _applyTabBarItemSizesAndPositions(tabBarItemSizesAndPositions, skipTabBarItem)
    {
        for (var [tabBarItem, sizeAndPosition] of tabBarItemSizesAndPositions) {
            if (skipTabBarItem && tabBarItem === skipTabBarItem)
                continue;

            let isFlexibleSpace = tabBarItem === this._flexibleSpaceBeforeElement || tabBarItem === this._flexibleSpaceAfterElement;

            let element = isFlexibleSpace ? tabBarItem : tabBarItem.element;
            element.style.left = sizeAndPosition.left + "px";
            element.style.width = sizeAndPosition.width + "px";
        }
    }

    _clearTabBarItemSizesAndPositions(skipTabBarItem)
    {
        this._flexibleSpaceBeforeElement.style.left = null;
        this._flexibleSpaceBeforeElement.style.width = null;

        this._flexibleSpaceAfterElement.style.left = null;
        this._flexibleSpaceAfterElement.style.width = null;

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

            this._tabContainer.classList.remove("static-layout");
            this._clearTabBarItemSizesAndPositions();

            var afterTabSizesAndPositions = this._recordTabBarItemSizesAndPositions();

            this._applyTabBarItemSizesAndPositions(beforeTabSizesAndPositions);
            this._tabContainer.classList.add("static-layout");

            function animateTabs()
            {
                this._tabContainer.classList.add("static-layout");
                this._tabContainer.classList.add("animating");
                this._tabContainer.classList.add("expanding-tabs");

                this._applyTabBarItemSizesAndPositions(afterTabSizesAndPositions);

                this._tabContainer.addEventListener("webkitTransitionEnd", removeStylesListener);
            }

            function removeStyles()
            {
                this._tabContainer.classList.remove("static-layout");
                this._tabContainer.classList.remove("animating");
                this._tabContainer.classList.remove("expanding-tabs");

                this._clearTabBarItemSizesAndPositions();

                this.updateLayout();

                this._tabContainer.removeEventListener("webkitTransitionEnd", removeStylesListener);

                resolve();
            }

            var removeStylesListener = removeStyles.bind(this);

            requestAnimationFrame(animateTabs.bind(this));
        }.bind(this));
    }

    _handleMouseDown(event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        if (event.target !== this.element && event.target !== this._flexibleSpaceBeforeElement && event.target !== this._flexibleSpaceAfterElement)
            return;

        switch (WI.dockConfiguration) {
        case WI.DockConfiguration.Bottom:
            WI.resizeDockedFrameMouseDown(event);
            break;

        case WI.DockConfiguration.Undocked:
            WI.moveUndockedWindowMouseDown(event);
            break;
        }
    }

    _handleTabContainerMouseDown(event)
    {
        // Only consider left mouse clicks for tab movement.
        if (event.button !== 0 || event.ctrlKey)
            return;

        let itemElement = event.target.closest("." + WI.TabBarItem.StyleClassName);
        if (!itemElement)
            return;

        let tabBarItem = itemElement[WI.TabBarItem.ElementReferenceSymbol];
        if (!tabBarItem)
            return;

        if (tabBarItem.disabled)
            return;

        if (tabBarItem === this._tabPickerTabBarItem) {
            if (!this._hiddenTabBarItems.length)
                return;

            if (this._ignoreTabPickerMouseDown)
                return;

            this._ignoreTabPickerMouseDown = true;

            let contextMenu = WI.ContextMenu.createFromEvent(event);
            contextMenu.addBeforeShowCallback(() => {
                this._ignoreTabPickerMouseDown = false;
            });

            for (let item of this._hiddenTabBarItems) {
                contextMenu.appendItem(item.displayName, () => {
                    this.selectTabBarItem(item, {
                        initiator: WI.TabBrowser.TabNavigationInitiator.ContextMenu
                    });
                });
            }

            contextMenu.show();
            return;
        }

        this.selectTabBarItem(tabBarItem, {
            initiatorHint: WI.TabBrowser.TabNavigationInitiator.TabClick
        });

        if (tabBarItem instanceof WI.PinnedTabBarItem || !this._hasMoreThanOneNormalTab())
            return;

        this._firstNormalTabItemIndex = 0;
        for (let i = 0; i < this._tabBarItems.length; ++i) {
            if (this._tabBarItems[i] instanceof WI.PinnedTabBarItem)
                continue;

            this._firstNormalTabItemIndex = i;
            break;
        }

        this._mouseDownPageX = event.pageX;

        this._mouseMovedEventListener = this._handleMouseMoved.bind(this);
        this._mouseUpEventListener = this._handleMouseUp.bind(this);

        // Register these listeners on the document so we can track the mouse if it leaves the tab bar.
        document.addEventListener("mousemove", this._mouseMovedEventListener, true);
        document.addEventListener("mouseup", this._mouseUpEventListener, true);

        event.preventDefault();
        event.stopPropagation();
    }

    _handleTabContainerClick(event)
    {
        var itemElement = event.target.closest("." + WI.TabBarItem.StyleClassName);
        if (!itemElement)
            return;

        var tabBarItem = itemElement[WI.TabBarItem.ElementReferenceSymbol];
        if (!tabBarItem)
            return;

        if (tabBarItem.disabled)
            return;

        if (event.button === 1) {
            // Disallow closing the only tab.
            if (this._tabContainer.classList.contains("single-tab"))
                return;

            if (!event.altKey) {
                let options = {
                    suppressExpansion: true,
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.TabClick,
                };
                this.removeTabBarItem(tabBarItem, options);
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
        console.assert(typeof this._mouseDownPageX === "number" && !isNaN(this._mouseDownPageX));
        if (isNaN(this._mouseDownPageX))
            return;

        console.assert(this._selectedTabBarItem);
        if (!this._selectedTabBarItem)
            return;

        if (this._mouseOffset === undefined)
            this._mouseOffset = event.pageX - this._selectedTabBarItem.element.totalOffsetLeft;

        if (!this._isDragging) {
            const dragThreshold = 12;
            if (Math.abs(this._mouseDownPageX - event.pageX) < dragThreshold)
                return;

            this._isDragging = true;
        }

        const tabBarLeftPadding = WI.TabBar.horizontalPadding / 2;
        const tabBarItemHorizontalMargin = WI.TabBarItem.horizontalMargin;

        event.preventDefault();
        event.stopPropagation();

        if (!this._tabContainer.classList.contains("static-layout")) {
            this._applyTabBarItemSizesAndPositions(this._recordTabBarItemSizesAndPositions());
            this._tabContainer.classList.add("static-layout");
            this._tabContainer.classList.add("dragging-tab");
        }

        let containerOffset = this._tabContainer.totalOffsetLeft;

        let tabBarMouseOffset = event.pageX - containerOffset;
        var newLeft = tabBarMouseOffset - this._mouseOffset;

        this._selectedTabBarItem.element.style.left = newLeft + "px";

        let selectedTabMidX = containerOffset + newLeft + ((this._selectedTabBarItem.element.realOffsetWidth + tabBarItemHorizontalMargin) / 2);

        var currentIndex = this._tabBarItems.indexOf(this._selectedTabBarItem);
        var newIndex = currentIndex;

        for (let tabBarItem of this._tabBarItems) {
            if (tabBarItem.hidden)
                continue;
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
        let nextSiblingElement = nextSibling ? nextSibling.element : null;

        this._tabContainer.insertBefore(this._selectedTabBarItem.element, nextSiblingElement);

        // FIXME: Animate the tabs that move to make room for the selected tab. This was causing me trouble when I tried.

        let isLTR = WI.resolvedLayoutDirection() === WI.LayoutDirection.LTR;

        function inlineStyleValue(element, property) {
            return element.style.getPropertyCSSValue(property).getFloatValue(CSSPrimitiveValue.CSS_PX) || 0;
        }

        let offsetAfter = inlineStyleValue(this._flexibleSpaceAfterElement, "width");

        let accumulatedLeft = isLTR ? (inlineStyleValue(this._flexibleSpaceBeforeElement, "width") + inlineStyleValue(this._flexibleSpaceBeforeElement, "left")) : tabBarLeftPadding;
        for (let tabBarItem of this._tabBarItemsFromLeftToRight()) {
            if (tabBarItem.hidden)
                continue;

            if (tabBarItem !== this._selectedTabBarItem && inlineStyleValue(tabBarItem.element, "left") !== accumulatedLeft) {
                let left = accumulatedLeft;
                if (isLTR) {
                    if (tabBarItem instanceof WI.PinnedTabBarItem && tabBarItem !== this._tabPickerTabBarItem)
                        left += offsetAfter;
                } else {
                    if (!(tabBarItem instanceof WI.PinnedTabBarItem) || tabBarItem === this._tabPickerTabBarItem)
                        left += offsetAfter;
                }
                tabBarItem.element.style.left = left + "px";
            }

            accumulatedLeft += inlineStyleValue(tabBarItem.element, "width") + tabBarItemHorizontalMargin;
        }
    }

    _handleMouseUp(event)
    {
        console.assert(event.button === 0);
        console.assert(typeof this._mouseDownPageX === "number" && !isNaN(this._mouseDownPageX));
        if (isNaN(this._mouseDownPageX))
            return;

        this._tabContainer.classList.remove("dragging-tab");

        if (!this._tabAnimatedClosedSinceMouseEnter) {
            this._tabContainer.classList.remove("static-layout");
            this._clearTabBarItemSizesAndPositions();
        } else {
            let left = 0;
            for (let tabBarItem of this._tabBarItemsFromLeftToRight()) {
                if (tabBarItem === this._selectedTabBarItem)
                    tabBarItem.element.style.left = left + "px";
                left += parseFloat(tabBarItem.element.style.width);
            }
        }

        this._isDragging = false;
        this._mouseDownPageX = NaN;
        this._mouseOffset = undefined;

        document.removeEventListener("mousemove", this._mouseMovedEventListener, true);
        document.removeEventListener("mouseup", this._mouseUpEventListener, true);

        this._mouseMovedEventListener = null;
        this._mouseUpEventListener = null;

        event.preventDefault();
        event.stopPropagation();

        this.dispatchEventToListeners(WI.TabBar.Event.TabBarItemsReordered);
    }

    _handleTabContainerMouseLeave(event)
    {
        if (!isNaN(this._mouseDownPageX) || !this._tabAnimatedClosedSinceMouseEnter || !this._tabContainer.classList.contains("static-layout") || this._tabContainer.classList.contains("animating"))
            return;

        // This event can still fire when the mouse is inside the element if DOM nodes are added, removed or generally change inside.
        // Check if the mouse really did leave the element by checking the bounds.
        // FIXME: Is this a WebKit bug or correct behavior?
        let barRect = this._tabContainer.getBoundingClientRect();
        if (event.pageY > barRect.top && event.pageY < barRect.bottom && event.pageX > barRect.left && event.pageX < barRect.right)
            return;

        this._finishExpandingTabsAfterClose();
    }

    _handleTabContainerContextMenu(event)
    {
        let contextMenu = WI.ContextMenu.createFromEvent(event);

        for (let tabClass of WI.knownTabClasses()) {
            if (!tabClass.isTabAllowed() || !tabClass.shouldSaveTab())
                continue;

            let openTabBarItem = null;
            for (let tabBarItem of this._tabBarItems) {
                let tabContentView = tabBarItem.representedObject;
                if (!(tabContentView instanceof WI.TabContentView))
                    continue;

                if (tabContentView.type === tabClass.Type) {
                    openTabBarItem = tabBarItem;
                    break;
                }
            }

            let checked = !!openTabBarItem;
            let disabled = checked && this.normalTabCount === 1;
            contextMenu.appendCheckboxItem(tabClass.tabInfo().displayName, () => {
                if (openTabBarItem)
                    this.removeTabBarItem(openTabBarItem);
                else
                    WI.createNewTabWithType(tabClass.Type, {shouldShowNewTab: true});
            }, checked, disabled);
        }
    }
};

WI.TabBar.CachedWidthSymbol = Symbol("cached-width");

WI.TabBar.Event = {
    TabBarItemSelected: "tab-bar-tab-bar-item-selected",
    TabBarItemAdded: "tab-bar-tab-bar-item-added",
    TabBarItemRemoved: "tab-bar-tab-bar-item-removed",
    TabBarItemsReordered: "tab-bar-tab-bar-items-reordered",
};
