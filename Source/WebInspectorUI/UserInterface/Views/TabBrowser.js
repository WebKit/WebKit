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

WebInspector.TabBrowser = class TabBrowser extends WebInspector.View
{
    constructor(element, tabBar, navigationSidebar, detailsSidebar)
    {
        console.assert(tabBar, "Must provide a TabBar.");

        super(element);

        this.element.classList.add("tab-browser");

        this._tabBar = tabBar;
        this._navigationSidebar = navigationSidebar || null;
        this._detailsSidebar = detailsSidebar || null;

        if (this._navigationSidebar)
            this._navigationSidebar.addEventListener(WebInspector.Sidebar.Event.CollapsedStateDidChange, this._sidebarCollapsedStateDidChange, this);

        if (this._detailsSidebar) {
            this._detailsSidebar.addEventListener(WebInspector.Sidebar.Event.CollapsedStateDidChange, this._sidebarCollapsedStateDidChange, this);
            this._detailsSidebar.addEventListener(WebInspector.Sidebar.Event.SidebarPanelSelected, this._sidebarPanelSelected, this);
        }

        this._contentViewContainer = new WebInspector.ContentViewContainer;
        this.addSubview(this._contentViewContainer);

        var showNextTab = this._showNextTab.bind(this);
        var showPreviousTab = this._showPreviousTab.bind(this);

        this._showNextTabKeyboardShortcut1 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.RightCurlyBrace, showNextTab);
        this._showPreviousTabKeyboardShortcut1 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.LeftCurlyBrace, showPreviousTab);
        this._showNextTabKeyboardShortcut2 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control, WebInspector.KeyboardShortcut.Key.Tab, showNextTab);
        this._showPreviousTabKeyboardShortcut2 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Tab, showPreviousTab);

        this._showNextTabKeyboardShortcut3 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Right, this._showNextTabCheckingForEditableField.bind(this));
        this._showNextTabKeyboardShortcut3.implicitlyPreventsDefault = false;
        this._showPreviousTabKeyboardShortcut3 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Left, this._showPreviousTabCheckingForEditableField.bind(this));
        this._showPreviousTabKeyboardShortcut3.implicitlyPreventsDefault = false;

        this._tabBar.newTabItem = new WebInspector.TabBarItem("Images/NewTabPlus.svg", WebInspector.UIString("Create a new tab"), true);

        this._tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemSelected, this._tabBarItemSelected, this);
        this._tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemRemoved, this._tabBarItemRemoved, this);

        this._recentTabContentViews = [];
    }

    // Public

    get tabBar()
    {
        return this._tabBar;
    }

    get navigationSidebar()
    {
        return this._navigationSidebar;
    }

    get detailsSidebar()
    {
        return this._detailsSidebar;
    }

    get selectedTabContentView()
    {
        return this._contentViewContainer.currentContentView;
    }

    bestTabContentViewForClass(constructor)
    {
        console.assert(!this.selectedTabContentView || this.selectedTabContentView === this._recentTabContentViews[0]);

        for (var tabContentView of this._recentTabContentViews) {
            if (tabContentView instanceof constructor)
                return tabContentView;
        }

        return null;
    }

    bestTabContentViewForRepresentedObject(representedObject)
    {
        console.assert(!this.selectedTabContentView || this.selectedTabContentView === this._recentTabContentViews[0]);

        for (var tabContentView of this._recentTabContentViews) {
            if (tabContentView.canShowRepresentedObject(representedObject))
                return tabContentView;
        }

        return null;
    }

    addTabForContentView(tabContentView, doNotAnimate, insertionIndex)
    {
        console.assert(tabContentView instanceof WebInspector.TabContentView);
        if (!(tabContentView instanceof WebInspector.TabContentView))
            return false;

        var tabBarItem = tabContentView.tabBarItem;

        console.assert(tabBarItem instanceof WebInspector.TabBarItem);
        if (!(tabBarItem instanceof WebInspector.TabBarItem))
            return false;

        if (tabBarItem.representedObject !== tabContentView)
            tabBarItem.representedObject = tabContentView;

        tabContentView.parentTabBrowser = this;

        if (tabBarItem.parentTabBar === this._tabBar)
            return true;

        // Add the tab after the first tab content view, since the first
        // tab content view is the currently selected one.
        if (this._recentTabContentViews.length && this.selectedTabContentView)
            this._recentTabContentViews.splice(1, 0, tabContentView);
        else
            this._recentTabContentViews.push(tabContentView);

        if (typeof insertionIndex === "number")
            this._tabBar.insertTabBarItem(tabBarItem, insertionIndex, doNotAnimate);
        else
            this._tabBar.addTabBarItem(tabBarItem, doNotAnimate);

        console.assert(this._recentTabContentViews.length === this._tabBar.tabBarItems.length - (this._tabBar.newTabItem ? 1 : 0));
        console.assert(!this.selectedTabContentView || this.selectedTabContentView === this._recentTabContentViews[0]);

        return true;
    }

    showTabForContentView(tabContentView, doNotAnimate, insertionIndex)
    {
        if (!this.addTabForContentView(tabContentView, doNotAnimate, insertionIndex))
            return false;

        this._tabBar.selectedTabBarItem = tabContentView.tabBarItem;

        // FIXME: this is a workaround for <https://webkit.org/b/151876>.
        // Without this extra call, we might never lay out the child tab
        // if it has already marked itself as dirty in the same run loop
        // as it is attached. It will schedule a layout, but when the rAF
        // fires the parent will abort the layout because the counter is
        // out of sync.
        this.needsLayout();
        return true;
    }

    closeTabForContentView(tabContentView, doNotAnimate)
    {
        console.assert(tabContentView instanceof WebInspector.TabContentView);
        if (!(tabContentView instanceof WebInspector.TabContentView))
            return false;

        console.assert(tabContentView.tabBarItem instanceof WebInspector.TabBarItem);
        if (!(tabContentView.tabBarItem instanceof WebInspector.TabBarItem))
            return false;

        if (tabContentView.tabBarItem.parentTabBar !== this._tabBar)
            return false;

        this._tabBar.removeTabBarItem(tabContentView.tabBarItem, doNotAnimate);

        console.assert(this._recentTabContentViews.length === this._tabBar.tabBarItems.length - (this._tabBar.newTabItem ? 1 : 0));
        console.assert(!this.selectedTabContentView || this.selectedTabContentView === this._recentTabContentViews[0]);

        return true;
    }

    // Private

    _tabBarItemSelected(event)
    {
        var tabContentView = this._tabBar.selectedTabBarItem ? this._tabBar.selectedTabBarItem.representedObject : null;

        if (tabContentView) {
            this._recentTabContentViews.remove(tabContentView);
            this._recentTabContentViews.unshift(tabContentView);

            this._contentViewContainer.showContentView(tabContentView);

            console.assert(this.selectedTabContentView);
            console.assert(this._recentTabContentViews.length === this._tabBar.tabBarItems.length - (this._tabBar.newTabItem ? 1 : 0));
            console.assert(this.selectedTabContentView === this._recentTabContentViews[0]);
        } else {
            this._contentViewContainer.closeAllContentViews();

            console.assert(!this.selectedTabContentView);
        }

        this._showNavigationSidebarPanelForTabContentView(tabContentView);
        this._showDetailsSidebarPanelsForTabContentView(tabContentView);

        this.dispatchEventToListeners(WebInspector.TabBrowser.Event.SelectedTabContentViewDidChange);
    }

    _tabBarItemRemoved(event)
    {
        var tabContentView = event.data.tabBarItem.representedObject;

        console.assert(tabContentView);
        if (!tabContentView)
            return;

        this._recentTabContentViews.remove(tabContentView);
        this._contentViewContainer.closeContentView(tabContentView);

        tabContentView.parentTabBrowser = null;

        console.assert(this._recentTabContentViews.length === this._tabBar.tabBarItems.length - (this._tabBar.newTabItem ? 1 : 0));
        console.assert(!this.selectedTabContentView || this.selectedTabContentView === this._recentTabContentViews[0]);
    }

    _sidebarPanelSelected(event)
    {
        if (this._ignoreSidebarEvents)
            return;

        var tabContentView = this.selectedTabContentView;
        if (!tabContentView)
            return;

        console.assert(event.target === this._detailsSidebar);

        if (tabContentView.managesDetailsSidebarPanels)
            return;

        var selectedSidebarPanel = this._detailsSidebar.selectedSidebarPanel;
        tabContentView.detailsSidebarSelectedPanelSetting.value = selectedSidebarPanel ? selectedSidebarPanel.identifier : null;
    }

    _sidebarCollapsedStateDidChange(event)
    {
        if (this._ignoreSidebarEvents)
            return;

        var tabContentView = this.selectedTabContentView;
        if (!tabContentView)
            return;

        if (event.target === this._navigationSidebar)
            tabContentView.navigationSidebarCollapsedSetting.value = this._navigationSidebar.collapsed;
        else if (event.target === this._detailsSidebar && !tabContentView.managesDetailsSidebarPanels)
            tabContentView.detailsSidebarCollapsedSetting.value = this._detailsSidebar.collapsed;
    }

    _showNavigationSidebarPanelForTabContentView(tabContentView)
    {
        if (!this._navigationSidebar)
            return;

        this._ignoreSidebarEvents = true;

        this._navigationSidebar.removeSidebarPanel(0);

        console.assert(!this._navigationSidebar.sidebarPanels.length);

        if (!tabContentView) {
            this._ignoreSidebarEvents = false;
            return;
        }

        var navigationSidebarPanel = tabContentView.navigationSidebarPanel;
        if (!navigationSidebarPanel) {
            this._navigationSidebar.collapsed = true;
            this._ignoreSidebarEvents = false;
            return;
        }

        this._navigationSidebar.addSidebarPanel(navigationSidebarPanel);
        this._navigationSidebar.selectedSidebarPanel = navigationSidebarPanel;

        this._navigationSidebar.collapsed = tabContentView.navigationSidebarCollapsedSetting.value;

        this._ignoreSidebarEvents = false;
    }

    _showDetailsSidebarPanelsForTabContentView(tabContentView)
    {
        if (!this._detailsSidebar)
            return;

        this._ignoreSidebarEvents = true;

        for (var i = this._detailsSidebar.sidebarPanels.length - 1; i >= 0; --i)
            this._detailsSidebar.removeSidebarPanel(i);

        console.assert(!this._detailsSidebar.sidebarPanels.length);

        if (!tabContentView) {
            this._ignoreSidebarEvents = false;
            return;
        }

        if (tabContentView.managesDetailsSidebarPanels) {
            tabContentView.showDetailsSidebarPanels();
            this._ignoreSidebarEvents = false;
            return;
        }

        var detailsSidebarPanels = tabContentView.detailsSidebarPanels;
        if (!detailsSidebarPanels) {
            this._detailsSidebar.collapsed = true;
            this._ignoreSidebarEvents = false;
            return;
        }

        for (var detailsSidebarPanel of detailsSidebarPanels)
            this._detailsSidebar.addSidebarPanel(detailsSidebarPanel);

        this._detailsSidebar.selectedSidebarPanel = tabContentView.detailsSidebarSelectedPanelSetting.value || detailsSidebarPanels[0];

        this._detailsSidebar.collapsed = tabContentView.detailsSidebarCollapsedSetting.value || !detailsSidebarPanels.length;

        this._ignoreSidebarEvents = false;
    }

    _showPreviousTab(event)
    {
        this._tabBar.selectPreviousTab();
    }

    _showNextTab(event)
    {
        this._tabBar.selectNextTab();
    }

    _showNextTabCheckingForEditableField(event)
    {
        if (WebInspector.isEventTargetAnEditableField(event))
            return;

        this._showNextTab(event);

        event.preventDefault();
    }

    _showPreviousTabCheckingForEditableField(event)
    {
        if (WebInspector.isEventTargetAnEditableField(event))
            return;

        this._showPreviousTab(event);

        event.preventDefault();
    }
};

WebInspector.TabBrowser.Event = {
    SelectedTabContentViewDidChange: "tab-browser-selected-tab-content-view-did-change"
};
