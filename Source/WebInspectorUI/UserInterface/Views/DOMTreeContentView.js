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

WI.DOMTreeContentView = class DOMTreeContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject);

        super(representedObject);

        this._compositingBordersButtonNavigationItem = new WI.ActivateButtonNavigationItem("layer-borders", WI.UIString("Show compositing borders"), WI.UIString("Hide compositing borders"), "Images/LayerBorders.svg", 13, 13);
        this._compositingBordersButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleCompositingBorders, this);
        this._compositingBordersButtonNavigationItem.enabled = !!PageAgent.getCompositingBordersVisible;
        this._compositingBordersButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;

        WI.settings.showPaintRects.addEventListener(WI.Setting.Event.Changed, this._showPaintRectsSettingChanged, this);
        this._paintFlashingButtonNavigationItem = new WI.ActivateButtonNavigationItem("paint-flashing", WI.UIString("Enable paint flashing"), WI.UIString("Disable paint flashing"), "Images/Paint.svg", 16, 16);
        this._paintFlashingButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._togglePaintFlashing, this);
        this._paintFlashingButtonNavigationItem.enabled = !!PageAgent.setShowPaintRects;
        this._paintFlashingButtonNavigationItem.activated = PageAgent.setShowPaintRects && WI.settings.showPaintRects.value;
        this._paintFlashingButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;

        WI.settings.showShadowDOM.addEventListener(WI.Setting.Event.Changed, this._showShadowDOMSettingChanged, this);
        this._showsShadowDOMButtonNavigationItem = new WI.ActivateButtonNavigationItem("shows-shadow-DOM", WI.UIString("Show shadow DOM nodes"), WI.UIString("Hide shadow DOM nodes"), "Images/ShadowDOM.svg", 13, 13);
        this._showsShadowDOMButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleShowsShadowDOMSetting, this);
        this._showsShadowDOMButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._showShadowDOMSettingChanged();

        this._showPrintStylesButtonNavigationItem = new WI.ActivateButtonNavigationItem("print-styles", WI.UIString("Force Print Media Styles"), WI.UIString("Use Default Media Styles"), "Images/Printer.svg", 16, 16);
        this._showPrintStylesButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._togglePrintStyles, this);
        this._showPrintStylesButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._showPrintStylesChanged();

        WI.settings.showRulers.addEventListener(WI.Setting.Event.Changed, this._showRulersChanged, this);
        this._showRulersButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-rulers", WI.UIString("Show Rulers"), WI.UIString("Hide Rulers"), "Images/Rulers.svg", 16, 16);
        this._showRulersButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleShowRulers, this);
        this._showRulersButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._showRulersChanged();

        this.element.classList.add("dom-tree");
        this.element.addEventListener("click", this._mouseWasClicked.bind(this), false);

        this._domTreeOutline = new WI.DOMTreeOutline(true, true, true);
        this._domTreeOutline.allowsEmptySelection = false;
        this._domTreeOutline.allowsMultipleSelection = true;
        this._domTreeOutline.addEventListener(WI.TreeOutline.Event.ElementAdded, this._domTreeElementAdded, this);
        this._domTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._domTreeSelectionDidChange, this);
        this._domTreeOutline.addEventListener(WI.DOMTreeOutline.Event.SelectedNodeChanged, this._selectedNodeDidChange, this);
        this._domTreeOutline.wireToDomAgent();
        this._domTreeOutline.editable = true;
        this.element.appendChild(this._domTreeOutline.element);

        WI.domManager.addEventListener(WI.DOMManager.Event.AttributeModified, this._domNodeChanged, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.AttributeRemoved, this._domNodeChanged, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.CharacterDataModified, this._domNodeChanged, this);

        WI.cssManager.addEventListener(WI.CSSManager.Event.DefaultAppearanceDidChange, this._defaultAppearanceDidChange, this);

        this._lastSelectedNodePathSetting = new WI.Setting("last-selected-node-path", null);

        this._numberOfSearchResults = null;

        this._breakpointGutterEnabled = false;
        this._pendingBreakpointNodeIdentifiers = new Set;

        if (WI.cssManager.canForceAppearance())
            this._defaultAppearanceDidChange();

        if (WI.domDebuggerManager.supported) {
            WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointsEnabledDidChange, this._breakpointsEnabledDidChange, this);

            WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, this._domBreakpointAddedOrRemoved, this);
            WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, this._domBreakpointAddedOrRemoved, this);

            WI.DOMBreakpoint.addEventListener(WI.DOMBreakpoint.Event.DisabledStateChanged, this._handleDOMBreakpointDisabledStateChanged, this);
            WI.DOMBreakpoint.addEventListener(WI.DOMBreakpoint.Event.DOMNodeChanged, this._handleDOMBreakpointDOMNodeChanged, this);

            this._breakpointsEnabledDidChange();
        }
    }

    // Public

    get navigationItems()
    {
        let items = [this._showPrintStylesButtonNavigationItem, this._showsShadowDOMButtonNavigationItem];

        if (this._forceAppearanceButtonNavigationItem)
            items.unshift(this._forceAppearanceButtonNavigationItem);

        // COMPATIBILITY (iOS 11.3)
        if (window.PageAgent && PageAgent.setShowRulers)
            items.unshift(this._showRulersButtonNavigationItem);

        if (!WI.settings.experimentalEnableLayersTab.value)
            items.push(this._compositingBordersButtonNavigationItem, this._paintFlashingButtonNavigationItem);

        return items;
    }

    get domTreeOutline()
    {
        return this._domTreeOutline;
    }

    get scrollableElements()
    {
        return [this.element];
    }

    get breakpointGutterEnabled()
    {
        return this._breakpointGutterEnabled;
    }

    set breakpointGutterEnabled(flag)
    {
        if (this._breakpointGutterEnabled === flag)
            return;

        this._breakpointGutterEnabled = flag;
        this.element.classList.toggle("show-gutter", this._breakpointGutterEnabled);
    }

    shown()
    {
        super.shown();

        this._domTreeOutline.setVisible(true, WI.isConsoleFocused());
        this._updateCompositingBordersButtonToMatchPageSettings();

        if (!this._domTreeOutline.rootDOMNode)
            return;

        this._restoreBreakpointsAfterUpdate();
    }

    hidden()
    {
        super.hidden();

        WI.domManager.hideDOMNodeHighlight();
        this._domTreeOutline.setVisible(false);
    }

    closed()
    {
        super.closed();

        WI.settings.showPaintRects.removeEventListener(null, null, this);
        WI.settings.showShadowDOM.removeEventListener(null, null, this);
        WI.settings.showRulers.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);
        WI.domManager.removeEventListener(null, null, this);
        WI.domDebuggerManager.removeEventListener(null, null, this);
        WI.DOMBreakpoint.removeEventListener(null, null, this);

        this._domTreeOutline.close();
        this._pendingBreakpointNodeIdentifiers.clear();
    }

    get selectionPathComponents()
    {
        var treeElement = this._domTreeOutline.selectedTreeElement;
        var pathComponents = [];

        while (treeElement && !treeElement.root) {
            // The close tag is contained within the element it closes. So skip it since we don't want to
            // show the same node twice in the hierarchy.
            if (treeElement.isCloseTag()) {
                treeElement = treeElement.parent;
                continue;
            }

            var pathComponent = new WI.DOMTreeElementPathComponent(treeElement, treeElement.representedObject);
            pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.Clicked, this._pathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            treeElement = treeElement.parent;
        }

        return pathComponents;
    }

    restoreFromCookie(cookie)
    {
        if (!cookie || !cookie.nodeToSelect)
            return;

        this.selectAndRevealDOMNode(cookie.nodeToSelect);

        // Because nodeToSelect is ephemeral, we don't want to keep
        // it around in the back-forward history entries.
        cookie.nodeToSelect = undefined;
    }

    selectAndRevealDOMNode(domNode, preventFocusChange)
    {
        this._domTreeOutline.selectDOMNode(domNode, !preventFocusChange);
    }

    handleCopyEvent(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (!selectedDOMNode)
            return;

        event.clipboardData.clearData();
        event.preventDefault();

        selectedDOMNode.copyNode();
    }

    get supportsSave()
    {
        return WI.canArchiveMainFrame();
    }

    get saveData()
    {
        return {customSaveHandler: () => { WI.archiveMainFrame(); }};
    }

    get supportsSearch()
    {
        return true;
    }

    get numberOfSearchResults()
    {
        return this._numberOfSearchResults;
    }

    get hasPerformedSearch()
    {
        return this._numberOfSearchResults !== null;
    }

    set automaticallyRevealFirstSearchResult(reveal)
    {
        this._automaticallyRevealFirstSearchResult = reveal;

        // If we haven't shown a search result yet, reveal one now.
        if (this._automaticallyRevealFirstSearchResult && this._numberOfSearchResults > 0) {
            if (this._currentSearchResultIndex === -1)
                this.revealNextSearchResult();
        }
    }

    performSearch(query)
    {
        if (this._searchQuery === query)
            return;

        if (this._searchIdentifier) {
            DOMAgent.discardSearchResults(this._searchIdentifier);
            this._hideSearchHighlights();
        }

        this._searchQuery = query;
        this._searchIdentifier = null;
        this._numberOfSearchResults = null;
        this._currentSearchResultIndex = -1;

        function searchResultsReady(error, searchIdentifier, resultsCount)
        {
            if (error)
                return;

            this._searchIdentifier = searchIdentifier;
            this._numberOfSearchResults = resultsCount;

            this.dispatchEventToListeners(WI.ContentView.Event.NumberOfSearchResultsDidChange);

            this._showSearchHighlights();

            if (this._automaticallyRevealFirstSearchResult)
                this.revealNextSearchResult();
        }

        function contextNodesReady(nodeIds)
        {
            DOMAgent.performSearch(query, nodeIds, searchResultsReady.bind(this));
        }

        this.getSearchContextNodes(contextNodesReady.bind(this));
    }

    getSearchContextNodes(callback)
    {
        // Overwrite this to limit the search to just a subtree.
        // Passing undefined will make DOMAgent.performSearch search through all the documents.
        callback(undefined);
    }

    searchCleared()
    {
        if (this._searchIdentifier) {
            DOMAgent.discardSearchResults(this._searchIdentifier);
            this._hideSearchHighlights();
        }

        this._searchQuery = null;
        this._searchIdentifier = null;
        this._numberOfSearchResults = null;
        this._currentSearchResultIndex = -1;
    }

    revealPreviousSearchResult(changeFocus)
    {
        if (!this._numberOfSearchResults)
            return;

        if (this._currentSearchResultIndex > 0)
            --this._currentSearchResultIndex;
        else
            this._currentSearchResultIndex = this._numberOfSearchResults - 1;

        this._revealSearchResult(this._currentSearchResultIndex, changeFocus);
    }

    revealNextSearchResult(changeFocus)
    {
        if (!this._numberOfSearchResults)
            return;

        if (this._currentSearchResultIndex + 1 < this._numberOfSearchResults)
            ++this._currentSearchResultIndex;
        else
            this._currentSearchResultIndex = 0;

        this._revealSearchResult(this._currentSearchResultIndex, changeFocus);
    }

    // Protected

    layout()
    {
        this._domTreeOutline.updateSelectionArea();

        if (this.layoutReason === WI.View.LayoutReason.Resize)
            this._domTreeOutline.selectDOMNode(this._domTreeOutline.selectedDOMNode());
    }

    // Private

    _revealSearchResult(index, changeFocus)
    {
        console.assert(this._searchIdentifier);

        var searchIdentifier = this._searchIdentifier;

        function revealResult(error, nodeIdentifiers)
        {
            if (error)
                return;

            // Bail if the searchIdentifier changed since we started.
            if (this._searchIdentifier !== searchIdentifier)
                return;

            console.assert(nodeIdentifiers.length === 1);

            var domNode = WI.domManager.nodeForId(nodeIdentifiers[0]);
            console.assert(domNode);
            if (!domNode)
                return;

            this._domTreeOutline.selectDOMNode(domNode, changeFocus);

            var selectedTreeElement = this._domTreeOutline.selectedTreeElement;
            if (selectedTreeElement)
                selectedTreeElement.emphasizeSearchHighlight();
        }

        DOMAgent.getSearchResults(this._searchIdentifier, index, index + 1, revealResult.bind(this));
    }

    _restoreSelectedNodeAfterUpdate(documentURL, defaultNode)
    {
        if (!WI.domManager.restoreSelectedNodeIsAllowed)
            return;

        function selectNode(lastSelectedNode)
        {
            var nodeToFocus = lastSelectedNode;
            if (!nodeToFocus)
                nodeToFocus = defaultNode;

            if (!nodeToFocus)
                return;

            this._dontSetLastSelectedNodePath = true;
            this.selectAndRevealDOMNode(nodeToFocus, WI.isConsoleFocused());
            this._dontSetLastSelectedNodePath = false;

            // If this wasn't the last selected node, then expand it.
            if (!lastSelectedNode && this._domTreeOutline.selectedTreeElement)
                this._domTreeOutline.selectedTreeElement.expand();
        }

        function selectLastSelectedNode(nodeId)
        {
            if (!WI.domManager.restoreSelectedNodeIsAllowed)
                return;

            selectNode.call(this, WI.domManager.nodeForId(nodeId));
        }

        if (documentURL && this._lastSelectedNodePathSetting.value && this._lastSelectedNodePathSetting.value.path && this._lastSelectedNodePathSetting.value.url === documentURL.hash)
            WI.domManager.pushNodeByPathToFrontend(this._lastSelectedNodePathSetting.value.path, selectLastSelectedNode.bind(this));
        else
            selectNode.call(this);
    }

    _domTreeElementAdded(event)
    {
        if (!this._pendingBreakpointNodeIdentifiers.size)
            return;

        let treeElement = event.data.element;
        let node = treeElement.representedObject;
        console.assert(node instanceof WI.DOMNode);
        if (!(node instanceof WI.DOMNode))
            return;

        if (!this._pendingBreakpointNodeIdentifiers.delete(node.id))
            return;

        this._updateBreakpointStatus(node.id);
    }

    _domTreeSelectionDidChange(event)
    {
        let treeElement = this._domTreeOutline.selectedTreeElement;
        let domNode = treeElement ? treeElement.representedObject : null;
        let selectedByUser = event.data.selectedByUser;

        this._domTreeOutline.suppressRevealAndSelect = true;
        this._domTreeOutline.selectDOMNode(domNode, selectedByUser);

        if (domNode && selectedByUser)
            WI.domManager.highlightDOMNode(domNode.id);

        this._domTreeOutline.updateSelectionArea();
        this._domTreeOutline.suppressRevealAndSelect = false;
    }

    _selectedNodeDidChange(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (selectedDOMNode && !this._dontSetLastSelectedNodePath)
            this._lastSelectedNodePathSetting.value = {url: WI.networkManager.mainFrame.url.hash, path: selectedDOMNode.path()};

        if (selectedDOMNode)
            WI.domManager.setInspectedNode(selectedDOMNode);

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _pathComponentSelected(event)
    {
        if (!event.data.pathComponent)
            return;

        console.assert(event.data.pathComponent instanceof WI.DOMTreeElementPathComponent);
        console.assert(event.data.pathComponent.domTreeElement instanceof WI.DOMTreeElement);

        this._domTreeOutline.selectDOMNode(event.data.pathComponent.domTreeElement.representedObject, true);
    }

    _domNodeChanged(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (selectedDOMNode !== event.data.node)
            return;

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _mouseWasClicked(event)
    {
        var anchorElement = event.target.closest("a");
        if (!anchorElement || !anchorElement.href)
            return;

        // Prevent the link from navigating, since we don't do any navigation by following links normally.
        event.preventDefault();
        event.stopPropagation();

        if (WI.isBeingEdited(anchorElement)) {
            // Don't follow the link when it is being edited.
            return;
        }

        // Cancel any pending link navigation.
        if (this._followLinkTimeoutIdentifier) {
            clearTimeout(this._followLinkTimeoutIdentifier);
            delete this._followLinkTimeoutIdentifier;
        }

        // If this is a double-click (or multiple-click), return early.
        if (event.detail > 1)
            return;

        function followLink()
        {
            // Since followLink is delayed, the call to WI.openURL can't look at window.event
            // to see if the command key is down like it normally would. So we need to do that check
            // before calling WI.openURL.
            const options = {
                alwaysOpenExternally: event ? event.metaKey : false,
                lineNumber: anchorElement.lineNumber,
                ignoreNetworkTab: true,
                ignoreSearchTab: true,
            };
            WI.openURL(anchorElement.href, this._frame, options);
        }

        // Start a timeout since this is a single click, if the timeout is canceled before it fires,
        // then a double-click happened or another link was clicked.
        // FIXME: The duration might be longer or shorter than the user's configured double click speed.
        this._followLinkTimeoutIdentifier = setTimeout(followLink.bind(this), 333);
    }

    _toggleCompositingBorders(event)
    {
        console.assert(PageAgent.setCompositingBordersVisible);

        var activated = !this._compositingBordersButtonNavigationItem.activated;
        this._compositingBordersButtonNavigationItem.activated = activated;
        PageAgent.setCompositingBordersVisible(activated);
    }

    _togglePaintFlashing(event)
    {
        WI.settings.showPaintRects.value = !WI.settings.showPaintRects.value;
    }

    _updateCompositingBordersButtonToMatchPageSettings()
    {
        if (WI.settings.experimentalEnableLayersTab.value)
            return;

        var button = this._compositingBordersButtonNavigationItem;

        // We need to sync with the page settings since these can be controlled
        // in a different way than just using the navigation bar button.
        PageAgent.getCompositingBordersVisible(function(error, compositingBordersVisible) {
            button.activated = error ? false : compositingBordersVisible;
            button.enabled = error !== "unsupported";
        });
    }

    _showPaintRectsSettingChanged(event)
    {
        console.assert(PageAgent.setShowPaintRects);

        this._paintFlashingButtonNavigationItem.activated = WI.settings.showPaintRects.value;

        PageAgent.setShowPaintRects(this._paintFlashingButtonNavigationItem.activated);
    }

    _showShadowDOMSettingChanged(event)
    {
        this._showsShadowDOMButtonNavigationItem.activated = WI.settings.showShadowDOM.value;
    }

    _toggleShowsShadowDOMSetting(event)
    {
        WI.settings.showShadowDOM.value = !WI.settings.showShadowDOM.value;
    }

    _showPrintStylesChanged()
    {
        this._showPrintStylesButtonNavigationItem.activated = WI.printStylesEnabled;

        let mediaType = WI.printStylesEnabled ? "print" : "";
        PageAgent.setEmulatedMedia(mediaType);

        WI.cssManager.mediaTypeChanged();
    }

    _togglePrintStyles(event)
    {
        WI.printStylesEnabled = !WI.printStylesEnabled;
        this._showPrintStylesChanged();
    }

    _defaultAppearanceDidChange()
    {
        let defaultAppearance = WI.cssManager.defaultAppearance;
        if (!defaultAppearance) {
            this._lastKnownDefaultAppearance = null;
            this._forceAppearanceButtonNavigationItem = null;
            this.dispatchEventToListeners(WI.ContentView.Event.NavigationItemsDidChange);
            return;
        }

        // Don't update the navigation item if there is currently a forced appearance.
        // The user will need to toggle it off to update it based on the new default appearance.
        if (WI.cssManager.forcedAppearance && this._forceAppearanceButtonNavigationItem)
            return;

        this._forceAppearanceButtonNavigationItem = null;

        switch (defaultAppearance) {
        case WI.CSSManager.Appearance.Light:
            this._forceAppearanceButtonNavigationItem = new WI.ActivateButtonNavigationItem("appearance", WI.UIString("Force Dark Appearance"), WI.UIString("Use Default Appearance"), "Images/Appearance.svg", 16, 16);
            break;
        case WI.CSSManager.Appearance.Dark:
            this._forceAppearanceButtonNavigationItem = new WI.ActivateButtonNavigationItem("appearance", WI.UIString("Force Light Appearance"), WI.UIString("Use Default Appearance"), "Images/Appearance.svg", 16, 16);
            break;
        }

        if (!this._forceAppearanceButtonNavigationItem) {
            console.error("Unknown default appearance name:", defaultAppearance);
            this.dispatchEventToListeners(WI.ContentView.Event.NavigationItemsDidChange);
            return;
        }

        this._lastKnownDefaultAppearance = defaultAppearance;

        this._forceAppearanceButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleAppearance, this);
        this._forceAppearanceButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._forceAppearanceButtonNavigationItem.activated = !!WI.cssManager.forcedAppearance;

        this.dispatchEventToListeners(WI.ContentView.Event.NavigationItemsDidChange);
    }

    _toggleAppearance(event)
    {
        // Use the last known default appearance, since that is the appearance this navigation item was generated for.
        let appearanceToForce = null;
        switch (this._lastKnownDefaultAppearance) {
        case WI.CSSManager.Appearance.Light:
            appearanceToForce = WI.CSSManager.Appearance.Dark;
            break;
        case WI.CSSManager.Appearance.Dark:
            appearanceToForce = WI.CSSManager.Appearance.Light;
            break;
        }

        console.assert(appearanceToForce);
        WI.cssManager.forcedAppearance = WI.cssManager.forcedAppearance == appearanceToForce ? null : appearanceToForce;

        // When no longer forcing an appearance, if the last known default appearance is different than the current
        // default appearance, then update the navigation button now. Otherwise just toggle the activated state.
        if (!WI.cssManager.forcedAppearance && this._lastKnownDefaultAppearance !== WI.cssManager.defaultAppearance)
            this._defaultAppearanceDidChange();
        else
            this._forceAppearanceButtonNavigationItem.activated = !!WI.cssManager.forcedAppearance;
    }

    _showRulersChanged()
    {
        this._showRulersButtonNavigationItem.activated = WI.settings.showRulers.value;

        // COMPATIBILITY (iOS 11.3)
        if (!PageAgent.setShowRulers)
            return;

        PageAgent.setShowRulers(this._showRulersButtonNavigationItem.activated);
    }

    _toggleShowRulers(event)
    {
        WI.settings.showRulers.value = !WI.settings.showRulers.value;

        this._showRulersChanged();
    }

    _showSearchHighlights()
    {
        console.assert(this._searchIdentifier);

        this._searchResultNodes = [];

        var searchIdentifier = this._searchIdentifier;

        DOMAgent.getSearchResults(this._searchIdentifier, 0, this._numberOfSearchResults, function(error, nodeIdentifiers) {
            if (error)
                return;

            if (this._searchIdentifier !== searchIdentifier)
                return;

            console.assert(nodeIdentifiers.length === this._numberOfSearchResults);

            for (var i = 0; i < nodeIdentifiers.length; ++i) {
                var domNode = WI.domManager.nodeForId(nodeIdentifiers[i]);
                console.assert(domNode);
                if (!domNode)
                    continue;

                this._searchResultNodes.push(domNode);

                var treeElement = this._domTreeOutline.findTreeElement(domNode);
                console.assert(treeElement);
                if (treeElement)
                    treeElement.highlightSearchResults(this._searchQuery);
            }
        }.bind(this));
    }

    _hideSearchHighlights()
    {
        if (!this._searchResultNodes)
            return;

        for (var domNode of this._searchResultNodes) {
            var treeElement = this._domTreeOutline.findTreeElement(domNode);
            if (treeElement)
                treeElement.hideSearchHighlights();
        }

        delete this._searchResultNodes;
    }

    _domBreakpointAddedOrRemoved(event)
    {
        let breakpoint = event.data.breakpoint;
        this._updateBreakpointStatus(breakpoint.domNodeIdentifier);
    }

    _handleDOMBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;
        this._updateBreakpointStatus(breakpoint.domNodeIdentifier);
    }

    _handleDOMBreakpointDOMNodeChanged(event)
    {
        let breakpoint = event.target;
        let nodeIdentifier = breakpoint.domNodeIdentifier || event.data.oldNodeIdentifier;
        this._updateBreakpointStatus(nodeIdentifier);
    }

    _updateBreakpointStatus(nodeIdentifier)
    {
        let domNode = WI.domManager.nodeForId(nodeIdentifier);
        if (!domNode)
            return;

        let treeElement = this._domTreeOutline.findTreeElement(domNode);
        if (!treeElement) {
            this._pendingBreakpointNodeIdentifiers.add(nodeIdentifier);
            return;
        }

        let breakpoints = WI.domDebuggerManager.domBreakpointsForNode(domNode);
        if (!breakpoints.length) {
            treeElement.breakpointStatus = WI.DOMTreeElement.BreakpointStatus.None;
            return;
        }

        this.breakpointGutterEnabled = true;

        let disabled = breakpoints.some((item) => item.disabled);
        treeElement.breakpointStatus = disabled ? WI.DOMTreeElement.BreakpointStatus.DisabledBreakpoint : WI.DOMTreeElement.BreakpointStatus.Breakpoint;
    }

    _restoreBreakpointsAfterUpdate()
    {
        this._pendingBreakpointNodeIdentifiers.clear();

        this.breakpointGutterEnabled = false;

        let updatedNodes = new Set;
        for (let breakpoint of WI.domDebuggerManager.domBreakpoints) {
            if (updatedNodes.has(breakpoint.domNodeIdentifier))
                continue;

            this._updateBreakpointStatus(breakpoint.domNodeIdentifier);
        }
    }

    _breakpointsEnabledDidChange(event)
    {
        this._domTreeOutline.element.classList.toggle("breakpoints-disabled", !WI.debuggerManager.breakpointsEnabled);
    }
};
