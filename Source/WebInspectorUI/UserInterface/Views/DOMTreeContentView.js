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

WebInspector.DOMTreeContentView = class DOMTreeContentView extends WebInspector.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject);

        super(representedObject);

        this._compositingBordersButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("layer-borders", WebInspector.UIString("Show compositing borders"), WebInspector.UIString("Hide compositing borders"), "Images/LayerBorders.svg", 13, 13);
        this._compositingBordersButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._toggleCompositingBorders, this);
        this._compositingBordersButtonNavigationItem.enabled = !!PageAgent.getCompositingBordersVisible;

        WebInspector.showPaintRectsSetting.addEventListener(WebInspector.Setting.Event.Changed, this._showPaintRectsSettingChanged, this);
        this._paintFlashingButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("paint-flashing", WebInspector.UIString("Enable paint flashing"), WebInspector.UIString("Disable paint flashing"), "Images/PaintFlashing.svg", 16, 16);
        this._paintFlashingButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._togglePaintFlashing, this);
        this._paintFlashingButtonNavigationItem.enabled = !!PageAgent.setShowPaintRects;
        this._paintFlashingButtonNavigationItem.activated = PageAgent.setShowPaintRects && WebInspector.showPaintRectsSetting.value;

        WebInspector.showShadowDOMSetting.addEventListener(WebInspector.Setting.Event.Changed, this._showShadowDOMSettingChanged, this);
        this._showsShadowDOMButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("shows-shadow-DOM", WebInspector.UIString("Show shadow DOM nodes"), WebInspector.UIString("Hide shadow DOM nodes"), "Images/ShadowDOM.svg", 13, 13);
        this._showsShadowDOMButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._toggleShowsShadowDOMSetting, this);
        this._showShadowDOMSettingChanged();

        this.element.classList.add("dom-tree");
        this.element.addEventListener("click", this._mouseWasClicked.bind(this), false);

        this._domTreeOutline = new WebInspector.DOMTreeOutline(true, true, true);
        this._domTreeOutline.addEventListener(WebInspector.DOMTreeOutline.Event.SelectedNodeChanged, this._selectedNodeDidChange, this);
        this._domTreeOutline.wireToDomAgent();
        this._domTreeOutline.editable = true;
        this.element.appendChild(this._domTreeOutline.element);

        WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.AttributeModified, this._domNodeChanged, this);
        WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.AttributeRemoved, this._domNodeChanged, this);
        WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.CharacterDataModified, this._domNodeChanged, this);

        this._lastSelectedNodePathSetting = new WebInspector.Setting("last-selected-node-path", null);

        this._numberOfSearchResults = null;
    }

    // Public

    get navigationItems()
    {
        return [this._showsShadowDOMButtonNavigationItem, this._compositingBordersButtonNavigationItem, this._paintFlashingButtonNavigationItem];
    }

    get domTreeOutline()
    {
        return this._domTreeOutline;
    }

    get scrollableElements()
    {
        return [this.element];
    }

    shown()
    {
        this._domTreeOutline.setVisible(true, WebInspector.isConsoleFocused());
        this._updateCompositingBordersButtonToMatchPageSettings();
    }

    hidden()
    {
        WebInspector.domTreeManager.hideDOMNodeHighlight();
        this._domTreeOutline.setVisible(false);
    }

    closed()
    {
        WebInspector.showPaintRectsSetting.removeEventListener(null, null, this);
        WebInspector.showShadowDOMSetting.removeEventListener(null, null, this);
        WebInspector.domTreeManager.removeEventListener(null, null, this);

        this._domTreeOutline.close();
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

            var pathComponent = new WebInspector.DOMTreeElementPathComponent(treeElement, treeElement.representedObject);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.Clicked, this._pathComponentSelected, this);
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
        return WebInspector.canArchiveMainFrame();
    }

    get saveData()
    {
        function saveHandler(forceSaveAs)
        {
            WebInspector.archiveMainFrame();
        }

        return { customSaveHandler: saveHandler };
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

            this.dispatchEventToListeners(WebInspector.ContentView.Event.NumberOfSearchResultsDidChange);

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
        this._domTreeOutline.updateSelection();
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

            var domNode = WebInspector.domTreeManager.nodeForId(nodeIdentifiers[0]);
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
        if (!WebInspector.domTreeManager.restoreSelectedNodeIsAllowed)
            return;

        function selectNode(lastSelectedNode)
        {
            var nodeToFocus = lastSelectedNode;
            if (!nodeToFocus)
                nodeToFocus = defaultNode;

            if (!nodeToFocus)
                return;

            this._dontSetLastSelectedNodePath = true;
            this.selectAndRevealDOMNode(nodeToFocus, WebInspector.isConsoleFocused());
            this._dontSetLastSelectedNodePath = false;

            // If this wasn't the last selected node, then expand it.
            if (!lastSelectedNode && this._domTreeOutline.selectedTreeElement)
                this._domTreeOutline.selectedTreeElement.expand();
        }

        function selectLastSelectedNode(nodeId)
        {
            if (!WebInspector.domTreeManager.restoreSelectedNodeIsAllowed)
                return;

            selectNode.call(this, WebInspector.domTreeManager.nodeForId(nodeId));
        }

        if (documentURL && this._lastSelectedNodePathSetting.value && this._lastSelectedNodePathSetting.value.path && this._lastSelectedNodePathSetting.value.url === documentURL.hash)
            WebInspector.domTreeManager.pushNodeByPathToFrontend(this._lastSelectedNodePathSetting.value.path, selectLastSelectedNode.bind(this));
        else
            selectNode.call(this);
    }

    _selectedNodeDidChange(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (selectedDOMNode && !this._dontSetLastSelectedNodePath)
            this._lastSelectedNodePathSetting.value = {url: selectedDOMNode.ownerDocument.documentURL.hash, path: selectedDOMNode.path()};

        if (selectedDOMNode)
            ConsoleAgent.addInspectedNode(selectedDOMNode.id);

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _pathComponentSelected(event)
    {
        if (!event.data.pathComponent)
            return;

        console.assert(event.data.pathComponent instanceof WebInspector.DOMTreeElementPathComponent);
        console.assert(event.data.pathComponent.domTreeElement instanceof WebInspector.DOMTreeElement);

        this._domTreeOutline.selectDOMNode(event.data.pathComponent.domTreeElement.representedObject, true);
    }

    _domNodeChanged(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (selectedDOMNode !== event.data.node)
            return;

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _mouseWasClicked(event)
    {
        var anchorElement = event.target.enclosingNodeOrSelfWithNodeName("a");
        if (!anchorElement || !anchorElement.href)
            return;

        // Prevent the link from navigating, since we don't do any navigation by following links normally.
        event.preventDefault();
        event.stopPropagation();

        if (WebInspector.isBeingEdited(anchorElement)) {
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
            // Since followLink is delayed, the call to WebInspector.openURL can't look at window.event
            // to see if the command key is down like it normally would. So we need to do that check
            // before calling WebInspector.openURL.
            var alwaysOpenExternally = event ? event.metaKey : false;
            WebInspector.openURL(anchorElement.href, this._frame, alwaysOpenExternally, anchorElement.lineNumber);
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
        WebInspector.showPaintRectsSetting.value = !WebInspector.showPaintRectsSetting.value;
    }

    _updateCompositingBordersButtonToMatchPageSettings()
    {
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

        this._paintFlashingButtonNavigationItem.activated = WebInspector.showPaintRectsSetting.value;

        PageAgent.setShowPaintRects(this._paintFlashingButtonNavigationItem.activated);
    }

    _showShadowDOMSettingChanged(event)
    {
        this._showsShadowDOMButtonNavigationItem.activated = WebInspector.showShadowDOMSetting.value;
    }

    _toggleShowsShadowDOMSetting(event)
    {
        WebInspector.showShadowDOMSetting.value = !WebInspector.showShadowDOMSetting.value;
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
                var domNode = WebInspector.domTreeManager.nodeForId(nodeIdentifiers[i]);
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
};
