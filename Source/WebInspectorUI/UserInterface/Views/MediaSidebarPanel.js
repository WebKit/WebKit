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

WI.MediaSidebarPanel = class MediaSidebarPanel extends WI.NavigationSidebarPanel
{
    constructor()
    {
        super("media", WI.UIString("Media"));

        this._navigationBar = new WI.NavigationBar;
        this.addSubview(this._navigationBar);

        let scopeItemPrefix = "media-sidebar-";
        let scopeBarItems = [];

        scopeBarItems.push(new WI.ScopeBarItem(scopeItemPrefix + "type-all", WI.UIString("All Media Players"), {exclusive: true}));

        let mediaTypes = [
            {identifier: "audio", title: WI.UIString("Audio"), classes: [WI.MediaPlayerTreeElement]},
            {identifier: "video", title: WI.UIString("Video"), classes: [WI.MediaPlayerTreeElement]},
        ];

        mediaTypes.sort(function(a, b) { return a.title.extendedLocaleCompare(b.title); });

        for (let info of mediaTypes) {
            let scopeBarItem = new WI.ScopeBarItem(scopeItemPrefix + info.identifier, info.title);
            scopeBarItem.__mediaTypeInfo = info;
            scopeBarItems.push(scopeBarItem);
        }

        this._scopeBar = new WI.ScopeBar("media-sidebar-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._navigationBar.addNavigationItem(this._scopeBar);

        WI.mediaManager.addEventListener(WI.MediaManager.Event.MediaPlayerAdded, this._handlePlayerAdded, this);
        WI.mediaManager.addEventListener(WI.MediaManager.Event.MediaPlayerRemoved, this._handlePlayerRemoved, this);
        WI.mediaManager.addEventListener(WI.MediaManager.Event.MediaPlayerUpdated, this._handlePlayerUpdated, this);

        this.contentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        for (let player of WI.mediaManager.mediaPlayerIdentifierMap.values())
            this._addPlayer(player);
    }

    // Public

    get minimumWidth()
    {
        return this._navigationBar.minimumWidth;
    }

    showDefaultContentView()
    {
        // Don't show anything by default. It doesn't make a whole lot of sense here.
    }

    closed()
    {
        super.closed();

        WI.mediaManager.removeEventListener(WI.MediaManager.Event.MediaPlayerAdded, this._handlePlayerAdded, this);
        WI.mediaManager.removeEventListener(WI.MediaManager.Event.MediaPlayerRemoved, this._handlePlayerRemoved, this);
        WI.mediaManager.removeEventListener(WI.MediaManager.Event.MediaPlayerUpdated, this._handlePlayerUpdated, this);
    }

    // Protected

    resetFilter()
    {
        this._scopeBar.resetToDefault();

        super.resetFilter();
    }

    hasCustomFilters()
    {
        console.assert(this._scopeBar.selectedItems.length === 1);
        let selectedScopeBarItem = this._scopeBar.selectedItems[0];
        return selectedScopeBarItem && !selectedScopeBarItem.exclusive;
    }

    // Private

    _treeSelectionDidChange(event)
    {
        if (!this.selected)
            return;

        let treeElement = this.contentTreeOutline.selectedTreeElement;
        if (!treeElement)
            return;

        if (treeElement instanceof WI.MediaPlayerTreeElement) {
            WI.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    }

    _addMediaChild(childElement)
    {
        childElement.flattened = true;

        this.contentTreeOutline.insertChild(childElement, insertionIndexForObjectInListSortedByFunction(childElement, this.contentTreeOutline.children, this._compareTreeElements));

        return childElement;
    }

    _addPlayer(player)
    {
        console.assert(player instanceof WI.MediaPlayer);
        this._addMediaChild(new WI.MediaPlayerTreeElement(player));
    }

    _removePlayer(player)
    {
        let treeElement = this.treeElementForRepresentedObject(player);
        console.assert(treeElement, "Missing tree element for player.", player);

        this._closeContentViewForTreeElement(treeElement);
        this.contentTreeOutline.removeChild(treeElement);
    }

    _updatePlayer(player)
    {
        let treeElement = this.treeElementForRepresentedObject(player);
        console.assert(treeElement, "Missing tree element for player.", player);

        treeElement.updateTitles();
    }

    _handlePlayerAdded(event)
    {
        this._addPlayer(event.data);
    }

    _handlePlayerRemoved(event)
    {
        this._removePlayer(event.data);
    }

    _handlePlayerUpdated(event)
    {
        this._updatePlayer(event.data);
    }

    _compareTreeElements(a, b)
    {
        console.assert(a.mainTitle);
        console.assert(b.mainTitle);

        return (a.mainTitle || "").extendedLocaleCompare(b.mainTitle || "");
    }

    _closeContentViewForTreeElement(treeElement)
    {
        const onlyExisting = true;
        let contentView = this.contentBrowser.contentViewForRepresentedObject(treeElement.representedObject, onlyExisting);
        if (contentView)
            this.contentBrowser.contentViewContainer.closeContentView(contentView);
    }

    _scopeBarSelectionDidChange(event)
    {
        this.updateFilter();
    }
};
