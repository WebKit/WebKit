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

WI.NetworkSidebarPanel = class NetworkSidebarPanel extends WI.NavigationSidebarPanel
{
    constructor(contentBrowser)
    {
        super("network", WI.UIString("Network"), false);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this.contentBrowser = contentBrowser;

        this.filterBar.placeholder = WI.UIString("Filter Resource List");

        this.contentTreeOutline.element.classList.add("network-grid");
        this.contentTreeOutline.disclosureButtons = false;
    }

    // Public

    get minimumWidth()
    {
        return this._navigationBar.minimumWidth;
    }

    showDefaultContentView()
    {
        if (!this._networkGridView)
            this._networkGridView = new WI.NetworkGridContentView(null, {networkSidebarPanel: this});

        this.contentBrowser.showContentView(this._networkGridView);
    }

    canShowDifferentContentView()
    {
        if (this._clickedTreeElementGoToArrow)
            return true;

        if (this.contentBrowser.currentContentView instanceof WI.NetworkGridContentView)
            return false;

        return !this.restoringState;
    }

    // Protected

    initialLayout()
    {
        this._navigationBar = new WI.NavigationBar;
        this.addSubview(this._navigationBar);

        this._resourcesTitleBarElement = document.createElement("div");
        this._resourcesTitleBarElement.textContent = WI.UIString("Name");
        this._resourcesTitleBarElement.classList.add("title-bar");
        this.element.appendChild(this._resourcesTitleBarElement);

        let scopeItemPrefix = "network-sidebar-";
        let scopeBarItems = [];

        scopeBarItems.push(new WI.ScopeBarItem(scopeItemPrefix + "type-all", WI.UIString("All Resources"), true));

        for (let key in WI.Resource.Type) {
            let value = WI.Resource.Type[key];
            let scopeBarItem = new WI.ScopeBarItem(scopeItemPrefix + value, WI.Resource.displayNameForType(value, true));
            scopeBarItem[WI.NetworkSidebarPanel.ResourceTypeSymbol] = value;
            scopeBarItems.push(scopeBarItem);
        }

        this._scopeBar = new WI.ScopeBar("network-sidebar-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._navigationBar.addNavigationItem(this._scopeBar);

        WI.timelineManager.persistentNetworkTimeline.addEventListener(WI.Timeline.Event.Reset, this._networkTimelineReset, this);

        this.contentBrowser.addEventListener(WI.ContentBrowser.Event.CurrentContentViewDidChange, this._contentBrowserCurrentContentViewDidChange, this);
        this._contentBrowserCurrentContentViewDidChange();
    }

    saveStateToCookie(cookie)
    {
        console.assert(cookie);

        cookie[WI.NetworkSidebarPanel.ShowingNetworkGridContentViewCookieKey] = this.contentBrowser.currentContentView instanceof WI.NetworkGridContentView;

        super.saveStateToCookie(cookie);
    }

    restoreStateFromCookie(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // Don't call NavigationSidebarPanel.restoreStateFromCookie, because it tries to match based
        // on type selected tree element. This would cause the grid to be deselected.
        if (cookie[WI.NetworkSidebarPanel.ShowingNetworkGridContentViewCookieKey])
            return;

        super.restoreStateFromCookie(cookie, relaxedMatchDelay);
    }

    hasCustomFilters()
    {
        console.assert(this._scopeBar.selectedItems.length === 1);
        var selectedScopeBarItem = this._scopeBar.selectedItems[0];
        return selectedScopeBarItem && !selectedScopeBarItem.exclusive;
    }

    matchTreeElementAgainstCustomFilters(treeElement, flags)
    {
        console.assert(this._scopeBar.selectedItems.length === 1);
        var selectedScopeBarItem = this._scopeBar.selectedItems[0];

        // Show everything if there is no selection or "All Resources" is selected (the exclusive item).
        if (!selectedScopeBarItem || selectedScopeBarItem.exclusive)
            return true;

        function match()
        {
            if (treeElement instanceof WI.FrameTreeElement)
                return selectedScopeBarItem[WI.NetworkSidebarPanel.ResourceTypeSymbol] === WI.Resource.Type.Document;

            console.assert(treeElement instanceof WI.ResourceTreeElement, "Unknown treeElement", treeElement);
            if (!(treeElement instanceof WI.ResourceTreeElement))
                return false;

            return treeElement.resource.type === selectedScopeBarItem[WI.NetworkSidebarPanel.ResourceTypeSymbol];
        }

        var matched = match();
        if (matched)
            flags.expandTreeElement = true;
        return matched;
    }

    treeElementAddedOrChanged(treeElement)
    {
        if (treeElement.status && treeElement.status[WI.NetworkSidebarPanel.TreeElementStatusButtonSymbol] || !treeElement.treeOutline)
            return;

        var fragment = document.createDocumentFragment();

        var closeButton = new WI.TreeElementStatusButton(useSVGSymbol("Images/Close.svg", null, WI.UIString("Close resource view")));
        closeButton.element.classList.add("close");
        closeButton.addEventListener(WI.TreeElementStatusButton.Event.Clicked, this._treeElementCloseButtonClicked, this);
        fragment.appendChild(closeButton.element);

        let goToButton = new WI.TreeElementStatusButton(WI.createGoToArrowButton());
        goToButton[WI.NetworkSidebarPanel.TreeElementSymbol] = treeElement;
        goToButton.addEventListener(WI.TreeElementStatusButton.Event.Clicked, this._treeElementGoToArrowWasClicked, this);
        fragment.appendChild(goToButton.element);

        treeElement.status = fragment;
        treeElement.status[WI.NetworkSidebarPanel.TreeElementStatusButtonSymbol] = true;
    }

    // Private

    _mainResourceDidChange(event)
    {
        let frame = event.target;
        if (!frame.isMainFrame() || WI.settings.clearNetworkOnNavigate.value)
            return;

        for (let treeElement of this.contentTreeOutline.children)
            treeElement.element.classList.add("preserved");
    }

    _networkTimelineReset(event)
    {
        this.contentBrowser.contentViewContainer.closeAllContentViews();
        this.showDefaultContentView();
    }

    _contentBrowserCurrentContentViewDidChange(event)
    {
        var didShowNetworkGridContentView = this.contentBrowser.currentContentView instanceof WI.NetworkGridContentView;
        this.element.classList.toggle("network-grid-content-view-showing", didShowNetworkGridContentView);
    }

    _treeElementGoToArrowWasClicked(event)
    {
        this._clickedTreeElementGoToArrow = true;

        let treeElement = event.target[WI.NetworkSidebarPanel.TreeElementSymbol];
        console.assert(treeElement instanceof WI.TreeElement);

        treeElement.select(true, true);

        this._clickedTreeElementGoToArrow = false;
    }

    _treeElementCloseButtonClicked(event)
    {
        // Say we are processing a selection change to avoid the selected tree element
        // from being deselected when the default content view is shown.
        this.contentTreeOutline.processingSelectionChange = true;

        this.showDefaultContentView();

        this.contentTreeOutline.processingSelectionChange = false;
    }

    _scopeBarSelectionDidChange(event)
    {
        this.updateFilter();
    }
};

WI.NetworkSidebarPanel.ResourceTypeSymbol = Symbol("resource-type");
WI.NetworkSidebarPanel.TreeElementSymbol = Symbol("tree-element");
WI.NetworkSidebarPanel.TreeElementStatusButtonSymbol = Symbol("tree-element-status-button");

WI.NetworkSidebarPanel.ShowingNetworkGridContentViewCookieKey = "network-sidebar-panel-showing-network-grid-content-view";
