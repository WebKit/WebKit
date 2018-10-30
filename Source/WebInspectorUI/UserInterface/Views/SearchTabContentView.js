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

WI.SearchTabContentView = class SearchTabContentView extends WI.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let tabBarItem;
        if (WI.settings.experimentalEnableNewTabBar.value)
            tabBarItem = WI.PinnedTabBarItem.fromTabInfo(WI.SearchTabContentView.tabInfo());
        else
            tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.SearchTabContentView.tabInfo());

        let detailsSidebarPanelConstructors = [WI.ResourceDetailsSidebarPanel, WI.ProbeDetailsSidebarPanel,
            WI.DOMNodeDetailsSidebarPanel, WI.ComputedStyleDetailsSidebarPanel, WI.RulesStyleDetailsSidebarPanel];

        if (window.LayerTreeAgent)
            detailsSidebarPanelConstructors.push(WI.LayerTreeDetailsSidebarPanel);

        super(identifier || "search", "search", tabBarItem, WI.SearchSidebarPanel, detailsSidebarPanelConstructors);

        // Ensures that the Search tab is displayable from a pinned tab bar item.
        tabBarItem.representedObject = this;

        this._forcePerformSearch = false;
    }

    static tabInfo()
    {
        let image = WI.settings.experimentalEnableNewTabBar.value ? "Images/Search.svg" : "Images/SearchResults.svg";
        return {
            image,
            title: WI.UIString("Search"),
            isEphemeral: true,
        };
    }

    // Public

    get type()
    {
        return WI.SearchTabContentView.Type;
    }

    shown()
    {
        super.shown();

        // Perform on a delay because the field might not be visible yet.
        setTimeout(this.focusSearchField.bind(this));
    }

    canShowRepresentedObject(representedObject)
    {
        if (representedObject instanceof WI.DOMTree)
            return true;

        if (!(representedObject instanceof WI.Resource) && !(representedObject instanceof WI.Script))
            return false;

        return !!this.navigationSidebarPanel.contentTreeOutline.getCachedTreeElement(representedObject);
    }

    focusSearchField()
    {
        this.navigationSidebarPanel.focusSearchField(this._forcePerformSearch);

        this._forcePerformSearch = false;
    }

    performSearch(searchQuery)
    {
        this.navigationSidebarPanel.performSearch(searchQuery);

        this._forcePerformSearch = false;
    }

    handleCopyEvent(event)
    {
        let contentTreeOutline = this.navigationSidebarPanel.contentTreeOutline;
        if (contentTreeOutline.element !== document.activeElement)
            return;

        let selectedTreeElement = contentTreeOutline.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        event.clipboardData.setData("text/plain", selectedTreeElement.synthesizedTextValue);
        event.stopPropagation();
        event.preventDefault();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._forcePerformSearch = true;
    }
};

WI.SearchTabContentView.Type = "search";
