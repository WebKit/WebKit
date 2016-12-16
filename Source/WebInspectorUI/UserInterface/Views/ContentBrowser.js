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

WebInspector.ContentBrowser = class ContentBrowser extends WebInspector.View
{
    constructor(element, delegate, disableBackForward, disableFindBanner)
    {
        super(element);

        this.element.classList.add("content-browser");

        this._navigationBar = new WebInspector.NavigationBar;
        this.addSubview(this._navigationBar);

        this._contentViewContainer = new WebInspector.ContentViewContainer;
        this._contentViewContainer.addEventListener(WebInspector.ContentViewContainer.Event.CurrentContentViewDidChange, this._currentContentViewDidChange, this);
        this.addSubview(this._contentViewContainer);

        if (!disableBackForward) {
            this._backKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Control, WebInspector.KeyboardShortcut.Key.Left, this._backButtonClicked.bind(this), this.element);
            this._forwardKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Control, WebInspector.KeyboardShortcut.Key.Right, this._forwardButtonClicked.bind(this), this.element);

            this._backButtonNavigationItem = new WebInspector.ButtonNavigationItem("back", WebInspector.UIString("Back (%s)").format(this._backKeyboardShortcut.displayName), "Images/BackForwardArrows.svg#back-arrow-mask", 8, 13);
            this._backButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._backButtonClicked, this);
            this._backButtonNavigationItem.enabled = false;
            this._navigationBar.addNavigationItem(this._backButtonNavigationItem);

            this._forwardButtonNavigationItem = new WebInspector.ButtonNavigationItem("forward", WebInspector.UIString("Forward (%s)").format(this._forwardKeyboardShortcut.displayName), "Images/BackForwardArrows.svg#forward-arrow-mask", 8, 13);
            this._forwardButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._forwardButtonClicked, this);
            this._forwardButtonNavigationItem.enabled = false;
            this._navigationBar.addNavigationItem(this._forwardButtonNavigationItem);

            this._navigationBar.addNavigationItem(new WebInspector.DividerNavigationItem);
        }

        if (!disableFindBanner) {
            this._findBanner = new WebInspector.FindBanner(this);
            this._findBanner.addEventListener(WebInspector.FindBanner.Event.DidShow, this._findBannerDidShow, this);
            this._findBanner.addEventListener(WebInspector.FindBanner.Event.DidHide, this._findBannerDidHide, this);
        }

        this._hierarchicalPathNavigationItem = new WebInspector.HierarchicalPathNavigationItem;
        this._hierarchicalPathNavigationItem.addEventListener(WebInspector.HierarchicalPathNavigationItem.Event.PathComponentWasSelected, this._hierarchicalPathComponentWasSelected, this);
        this._navigationBar.addNavigationItem(this._hierarchicalPathNavigationItem);

        this._contentViewSelectionPathNavigationItem = new WebInspector.HierarchicalPathNavigationItem;

        this._dividingFlexibleSpaceNavigationItem = new WebInspector.FlexibleSpaceNavigationItem;
        this._navigationBar.addNavigationItem(this._dividingFlexibleSpaceNavigationItem);

        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._contentViewSelectionPathComponentDidChange, this);
        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange, this._contentViewSupplementalRepresentedObjectsDidChange, this);
        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.NumberOfSearchResultsDidChange, this._contentViewNumberOfSearchResultsDidChange, this);
        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.NavigationItemsDidChange, this._contentViewNavigationItemsDidChange, this);

        this._delegate = delegate || null;

        this._currentContentViewNavigationItems = [];
    }

    // Public

    get navigationBar()
    {
        return this._navigationBar;
    }

    get contentViewContainer()
    {
        return this._contentViewContainer;
    }

    get delegate()
    {
        return this._delegate;
    }

    set delegate(newDelegate)
    {
        this._delegate = newDelegate || null;
    }

    get currentContentView()
    {
        return this._contentViewContainer.currentContentView;
    }

    get currentRepresentedObjects()
    {
        var representedObjects = [];

        var lastComponent = this._hierarchicalPathNavigationItem.lastComponent;
        if (lastComponent && lastComponent.representedObject)
            representedObjects.push(lastComponent.representedObject);

        lastComponent = this._contentViewSelectionPathNavigationItem.lastComponent;
        if (lastComponent && lastComponent.representedObject)
            representedObjects.push(lastComponent.representedObject);

        var currentContentView = this.currentContentView;
        if (currentContentView) {
            var supplementalRepresentedObjects = currentContentView.supplementalRepresentedObjects;
            if (supplementalRepresentedObjects && supplementalRepresentedObjects.length)
                representedObjects = representedObjects.concat(supplementalRepresentedObjects);
        }

        return representedObjects;
    }

    showContentViewForRepresentedObject(representedObject, cookie, extraArguments)
    {
        var contentView = this.contentViewForRepresentedObject(representedObject, false, extraArguments);
        return this._contentViewContainer.showContentView(contentView, cookie);
    }

    showContentView(contentView, cookie)
    {
        return this._contentViewContainer.showContentView(contentView, cookie);
    }

    contentViewForRepresentedObject(representedObject, onlyExisting, extraArguments)
    {
        return this._contentViewContainer.contentViewForRepresentedObject(representedObject, onlyExisting, extraArguments);
    }

    updateHierarchicalPathForCurrentContentView()
    {
        var currentContentView = this.currentContentView;
        this._updateHierarchicalPathNavigationItem(currentContentView ? currentContentView.representedObject : null);
    }

    canGoBack()
    {
        var currentContentView = this.currentContentView;
        if (currentContentView && currentContentView.canGoBack())
            return true;
        return this._contentViewContainer.canGoBack();
    }

    canGoForward()
    {
        var currentContentView = this.currentContentView;
        if (currentContentView && currentContentView.canGoForward())
            return true;
        return this._contentViewContainer.canGoForward();
    }

    goBack()
    {
        var currentContentView = this.currentContentView;
        if (currentContentView && currentContentView.canGoBack()) {
            currentContentView.goBack();
            this._updateBackForwardButtons();
            return;
        }

        this._contentViewContainer.goBack();

        // The _updateBackForwardButtons function is called by _currentContentViewDidChange,
        // so it does not need to be called here.
    }

    goForward()
    {
        var currentContentView = this.currentContentView;
        if (currentContentView && currentContentView.canGoForward()) {
            currentContentView.goForward();
            this._updateBackForwardButtons();
            return;
        }

        this._contentViewContainer.goForward();

        // The _updateBackForwardButtons function is called by _currentContentViewDidChange,
        // so it does not need to be called here.
    }

    showFindBanner()
    {
        if (!this._findBanner)
            return;

        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        if (currentContentView.supportsCustomFindBanner) {
            currentContentView.showCustomFindBanner();
            return;
        }

        this._findBanner.show();
    }

    findBannerPerformSearch(findBanner, query)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        currentContentView.performSearch(query);
    }

    findBannerSearchCleared(findBanner)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        currentContentView.searchCleared();
    }

    findBannerSearchQueryForSelection(findBanner)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return null;

        return currentContentView.searchQueryWithSelection();
    }

    findBannerRevealPreviousResult(findBanner)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        currentContentView.revealPreviousSearchResult(!findBanner.showing);
    }

    findBannerRevealNextResult(findBanner)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        currentContentView.revealNextSearchResult(!findBanner.showing);
    }

    shown()
    {
        this._contentViewContainer.shown();

        if (this._findBanner)
            this._findBanner.enableKeyboardShortcuts();
    }

    hidden()
    {
        this._contentViewContainer.hidden();

        if (this._findBanner)
            this._findBanner.disableKeyboardShortcuts();
    }

    // Private

    _backButtonClicked(event)
    {
        this.goBack();
    }

    _forwardButtonClicked(event)
    {
        this.goForward();
    }

    _findBannerDidShow(event)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        currentContentView.automaticallyRevealFirstSearchResult = true;
        if (this._findBanner.searchQuery !== "")
            currentContentView.performSearch(this._findBanner.searchQuery);
    }

    _findBannerDidHide(event)
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView || !currentContentView.supportsSearch)
            return;

        currentContentView.automaticallyRevealFirstSearchResult = false;
        currentContentView.searchCleared();
    }

    _contentViewNumberOfSearchResultsDidChange(event)
    {
        if (!this._findBanner)
            return;

        if (event.target !== this.currentContentView)
            return;

        this._findBanner.numberOfResults = this.currentContentView.numberOfSearchResults;
    }

    _updateHierarchicalPathNavigationItem(representedObject)
    {
        if (!this.delegate || typeof this.delegate.contentBrowserTreeElementForRepresentedObject !== "function")
            return;

        var treeElement = representedObject ? this.delegate.contentBrowserTreeElementForRepresentedObject(this, representedObject) : null;
        var pathComponents = [];

        while (treeElement && !treeElement.root) {
            var pathComponent = new WebInspector.GeneralTreeElementPathComponent(treeElement);
            pathComponents.unshift(pathComponent);
            treeElement = treeElement.parent;
        }

        this._hierarchicalPathNavigationItem.components = pathComponents;
    }

    _updateContentViewSelectionPathNavigationItem(contentView)
    {
        var selectionPathComponents = contentView ? contentView.selectionPathComponents || [] : [];
        this._contentViewSelectionPathNavigationItem.components = selectionPathComponents;

        if (!selectionPathComponents.length) {
            this._hierarchicalPathNavigationItem.alwaysShowLastPathComponentSeparator = false;
            this._navigationBar.removeNavigationItem(this._contentViewSelectionPathNavigationItem);
            return;
        }

        // Insert the _contentViewSelectionPathNavigationItem after the _hierarchicalPathNavigationItem, if needed.
        if (!this._navigationBar.navigationItems.includes(this._contentViewSelectionPathNavigationItem)) {
            var hierarchicalPathItemIndex = this._navigationBar.navigationItems.indexOf(this._hierarchicalPathNavigationItem);
            console.assert(hierarchicalPathItemIndex !== -1);
            this._navigationBar.insertNavigationItem(this._contentViewSelectionPathNavigationItem, hierarchicalPathItemIndex + 1);
            this._hierarchicalPathNavigationItem.alwaysShowLastPathComponentSeparator = true;
        }
    }

    _updateBackForwardButtons()
    {
        if (!this._backButtonNavigationItem || !this._forwardButtonNavigationItem)
            return;

        this._backButtonNavigationItem.enabled = this.canGoBack();
        this._forwardButtonNavigationItem.enabled = this.canGoForward();
    }

    _updateContentViewNavigationItems()
    {
        var currentContentView = this.currentContentView;
        if (!currentContentView) {
            this._removeAllNavigationItems();
            this._currentContentViewNavigationItems = [];
            return;
        }

        let previousItems = this._currentContentViewNavigationItems.filter((item) => !(item instanceof WebInspector.DividerNavigationItem));
        let isUnchanged = Array.shallowEqual(previousItems, currentContentView.navigationItems);

        if (isUnchanged)
            return;

        this._removeAllNavigationItems();

        let navigationBar = this.navigationBar;
        let insertionIndex = navigationBar.navigationItems.indexOf(this._dividingFlexibleSpaceNavigationItem) + 1;
        console.assert(insertionIndex >= 0);

        // Keep track of items we'll be adding to the navigation bar.
        let newNavigationItems = [];

        // Go through each of the items of the new content view and add a divider before them.
        currentContentView.navigationItems.forEach(function(navigationItem, index) {
            // Add dividers before items unless it's the first item and not a button.
            if (index !== 0 || navigationItem instanceof WebInspector.ButtonNavigationItem) {
                let divider = new WebInspector.DividerNavigationItem;
                navigationBar.insertNavigationItem(divider, insertionIndex++);
                newNavigationItems.push(divider);
            }
            navigationBar.insertNavigationItem(navigationItem, insertionIndex++);
            newNavigationItems.push(navigationItem);
        });

        // Remember the navigation items we inserted so we can remove them
        // for the next content view.
        this._currentContentViewNavigationItems = newNavigationItems;
    }

    _removeAllNavigationItems()
    {
        for (let navigationItem of this._currentContentViewNavigationItems)
            this.navigationBar.removeNavigationItem(navigationItem);
    }

    _updateFindBanner(currentContentView)
    {
        if (!this._findBanner)
            return;

        if (!currentContentView) {
            this._findBanner.targetElement = null;
            this._findBanner.numberOfResults = null;
            return;
        }

        this._findBanner.targetElement = currentContentView.element;
        this._findBanner.numberOfResults = currentContentView.hasPerformedSearch ? currentContentView.numberOfSearchResults : null;

        if (currentContentView.supportsSearch && this._findBanner.searchQuery) {
            currentContentView.automaticallyRevealFirstSearchResult = this._findBanner.showing;
            currentContentView.performSearch(this._findBanner.searchQuery);
        }
    }

    _dispatchCurrentRepresentedObjectsDidChangeEvent()
    {
        this._dispatchCurrentRepresentedObjectsDidChangeEvent.cancelDebounce();

        this.dispatchEventToListeners(WebInspector.ContentBrowser.Event.CurrentRepresentedObjectsDidChange);
    }

    _contentViewSelectionPathComponentDidChange(event)
    {
        if (event.target !== this.currentContentView)
            return;

        this._updateContentViewSelectionPathNavigationItem(event.target);
        this._updateBackForwardButtons();

        this._updateContentViewNavigationItems();

        this._navigationBar.needsLayout();

        this.soon._dispatchCurrentRepresentedObjectsDidChangeEvent();
    }

    _contentViewSupplementalRepresentedObjectsDidChange(event)
    {
        if (event.target !== this.currentContentView)
            return;

        this.soon._dispatchCurrentRepresentedObjectsDidChangeEvent();
    }

    _currentContentViewDidChange(event)
    {
        var currentContentView = this.currentContentView;

        this._updateHierarchicalPathNavigationItem(currentContentView ? currentContentView.representedObject : null);
        this._updateContentViewSelectionPathNavigationItem(currentContentView);
        this._updateBackForwardButtons();

        this._updateContentViewNavigationItems();
        this._updateFindBanner(currentContentView);

        this._navigationBar.needsLayout();

        this.dispatchEventToListeners(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange);

        this._dispatchCurrentRepresentedObjectsDidChangeEvent();
    }

    _contentViewNavigationItemsDidChange(event)
    {
        if (event.target !== this.currentContentView)
            return;

        this._updateContentViewNavigationItems();
        this._navigationBar.needsLayout();
    }

    _hierarchicalPathComponentWasSelected(event)
    {
        console.assert(event.data.pathComponent instanceof WebInspector.GeneralTreeElementPathComponent);

        var treeElement = event.data.pathComponent.generalTreeElement;
        var originalTreeElement = treeElement;

        // Some tree elements (like folders) are not viewable. Find the first descendant that is viewable.
        while (treeElement && !WebInspector.ContentView.isViewable(treeElement.representedObject))
            treeElement = treeElement.traverseNextTreeElement(false, originalTreeElement, false);

        if (!treeElement)
            return;

        treeElement.revealAndSelect();
    }
};

WebInspector.ContentBrowser.Event = {
    CurrentRepresentedObjectsDidChange: "content-browser-current-represented-objects-did-change",
    CurrentContentViewDidChange: "content-browser-current-content-view-did-change"
};
