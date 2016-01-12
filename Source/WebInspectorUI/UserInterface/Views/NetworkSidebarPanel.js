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

WebInspector.NetworkSidebarPanel = class NetworkSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor(contentBrowser)
    {
        super("network", WebInspector.UIString("Network"), true);

        this.contentBrowser = contentBrowser;

        this.filterBar.placeholder = WebInspector.UIString("Filter Resource List");

        this._navigationBar = new WebInspector.NavigationBar;
        this.addSubview(this._navigationBar);

        this._resourcesTitleBarElement = document.createElement("div");
        this._resourcesTitleBarElement.textContent = WebInspector.UIString("Name");
        this._resourcesTitleBarElement.classList.add("title-bar");
        this.element.appendChild(this._resourcesTitleBarElement);

        var scopeItemPrefix = "network-sidebar-";
        var scopeBarItems = [];

        scopeBarItems.push(new WebInspector.ScopeBarItem(scopeItemPrefix + "type-all", WebInspector.UIString("All Resources"), true));

        for (var key in WebInspector.Resource.Type) {
            var value = WebInspector.Resource.Type[key];
            var scopeBarItem = new WebInspector.ScopeBarItem(scopeItemPrefix + value, WebInspector.Resource.displayNameForType(value, true));
            scopeBarItem[WebInspector.NetworkSidebarPanel.ResourceTypeSymbol] = value;
            scopeBarItems.push(scopeBarItem);
        }

        this._scopeBar = new WebInspector.ScopeBar("network-sidebar-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._navigationBar.addNavigationItem(this._scopeBar);

        this.contentTreeOutline.element.classList.add("network-grid");
        this.contentTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);

        this.contentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange, this._contentBrowserCurrentContentViewDidChange, this);

        var networkTimeline = WebInspector.timelineManager.persistentNetworkTimeline;
        networkTimeline.addEventListener(WebInspector.Timeline.Event.Reset, this._networkTimelineReset, this);

        this._networkGridView = new WebInspector.NetworkGridContentView(null, {networkSidebarPanel: this});
    }

    // Public

    get minimumWidth()
    {
        return this._navigationBar.minimumWidth;
    }

    closed()
    {
        super.closed();

        WebInspector.frameResourceManager.removeEventListener(null, null, this);
    }

    showDefaultContentView()
    {
        this.contentBrowser.showContentView(this._networkGridView);
    }

    canShowDifferentContentView()
    {
        if (this._clickedTreeElementGoToArrow)
            return true;

        if (this.contentBrowser.currentContentView instanceof WebInspector.NetworkGridContentView)
            return false;

        return !this.restoringState || !this._restoredShowingNetworkGridContentView;
    }

    // Protected

    saveStateToCookie(cookie)
    {
        console.assert(cookie);

        cookie[WebInspector.NetworkSidebarPanel.ShowingNetworkGridContentViewCookieKey] = this.contentBrowser.currentContentView instanceof WebInspector.NetworkGridContentView;

        super.saveStateToCookie(cookie);
    }

    restoreStateFromCookie(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // Don't call NavigationSidebarPanel.restoreStateFromCookie, because it tries to match based
        // on type selected tree element. This would cause the grid to be deselected.
        if (cookie[WebInspector.NetworkSidebarPanel.ShowingNetworkGridContentViewCookieKey])
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
            if (treeElement instanceof WebInspector.FrameTreeElement)
                return selectedScopeBarItem[WebInspector.NetworkSidebarPanel.ResourceTypeSymbol] === WebInspector.Resource.Type.Document;

            console.assert(treeElement instanceof WebInspector.ResourceTreeElement, "Unknown treeElement", treeElement);
            if (!(treeElement instanceof WebInspector.ResourceTreeElement))
                return false;

            return treeElement.resource.type === selectedScopeBarItem[WebInspector.NetworkSidebarPanel.ResourceTypeSymbol];
        }

        var matched = match();
        if (matched)
            flags.expandTreeElement = true;
        return matched;
    }

    treeElementAddedOrChanged(treeElement)
    {
        if (treeElement.status || !treeElement.treeOutline)
            return;

        var fragment = document.createDocumentFragment();

        var closeButton = new WebInspector.TreeElementStatusButton(useSVGSymbol("Images/Close.svg", null, WebInspector.UIString("Close resource view")));
        closeButton.element.classList.add("close");
        closeButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._treeElementCloseButtonClicked, this);
        fragment.appendChild(closeButton.element);

        var goToButton = new WebInspector.TreeElementStatusButton(WebInspector.createGoToArrowButton());
        goToButton[WebInspector.NetworkSidebarPanel.TreeElementSymbol] = treeElement;
        goToButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._treeElementGoToArrowWasClicked, this);
        fragment.appendChild(goToButton.element);

        treeElement.status = fragment;
    }

    // Private

    _networkTimelineReset(event)
    {
        this.contentBrowser.contentViewContainer.closeAllContentViews();
        this.showDefaultContentView();
    }

    _contentBrowserCurrentContentViewDidChange(event)
    {
        var didShowNetworkGridContentView = this.contentBrowser.currentContentView instanceof WebInspector.NetworkGridContentView;
        this.element.classList.toggle("network-grid-content-view-showing", didShowNetworkGridContentView);
    }

    _treeElementGoToArrowWasClicked(event)
    {
        this._clickedTreeElementGoToArrow = true;

        let treeElement = event.target[WebInspector.NetworkSidebarPanel.TreeElementSymbol];
        console.assert(treeElement instanceof WebInspector.TreeElement);

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

WebInspector.NetworkSidebarPanel.ResourceTypeSymbol = Symbol("resource-type");
WebInspector.NetworkSidebarPanel.TreeElementSymbol = Symbol("tree-element");

WebInspector.NetworkSidebarPanel.ShowingNetworkGridContentViewCookieKey = "network-sidebar-panel-showing-network-grid-content-view";
