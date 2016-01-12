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

WebInspector.StorageSidebarPanel = class StorageSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor(contentBrowser)
    {
        super("storage", WebInspector.UIString("Storage"));

        this.contentBrowser = contentBrowser;

        this.filterBar.placeholder = WebInspector.UIString("Filter Storage List");

        this._navigationBar = new WebInspector.NavigationBar;
        this.addSubview(this._navigationBar);

        var scopeItemPrefix = "storage-sidebar-";
        var scopeBarItems = [];

        scopeBarItems.push(new WebInspector.ScopeBarItem(scopeItemPrefix + "type-all", WebInspector.UIString("All Storage"), true));

        var storageTypes = [{identifier: "application-cache", title: WebInspector.UIString("Application Cache"), classes: [WebInspector.ApplicationCacheFrameTreeElement, WebInspector.ApplicationCacheManifestTreeElement]},
            {identifier: "cookies", title: WebInspector.UIString("Cookies"), classes: [WebInspector.CookieStorageTreeElement]},
            {identifier: "database", title: WebInspector.UIString("Databases"), classes: [WebInspector.DatabaseHostTreeElement, WebInspector.DatabaseTableTreeElement, WebInspector.DatabaseTreeElement]},
            {identifier: "indexed-database", title: WebInspector.UIString("Indexed Databases"), classes: [WebInspector.IndexedDatabaseHostTreeElement, WebInspector.IndexedDatabaseObjectStoreTreeElement, WebInspector.IndexedDatabaseTreeElement]},
            {identifier: "local-storage", title: WebInspector.UIString("Local Storage"), classes: [WebInspector.DOMStorageTreeElement], localStorage: true},
            {identifier: "session-storage", title: WebInspector.UIString("Session Storage"), classes: [WebInspector.DOMStorageTreeElement], localStorage: false}];

        storageTypes.sort(function(a, b) { return a.title.localeCompare(b.title); });

        for (var info of storageTypes) {
            var scopeBarItem = new WebInspector.ScopeBarItem(scopeItemPrefix + info.identifier, info.title);
            scopeBarItem.__storageTypeInfo = info;
            scopeBarItems.push(scopeBarItem);
        }

        this._scopeBar = new WebInspector.ScopeBar("storage-sidebar-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._navigationBar.addNavigationItem(this._scopeBar);

        this._localStorageRootTreeElement = null;
        this._sessionStorageRootTreeElement = null;

        this._databaseRootTreeElement = null;
        this._databaseHostTreeElementMap = new Map;

        this._indexedDatabaseRootTreeElement = null;
        this._indexedDatabaseHostTreeElementMap = new Map;

        this._cookieStorageRootTreeElement = null;

        this._applicationCacheRootTreeElement = null;
        this._applicationCacheURLTreeElementMap = new Map;

        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.CookieStorageObjectWasAdded, this._cookieStorageObjectWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DOMStorageObjectWasAdded, this._domStorageObjectWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DOMStorageObjectWasInspected, this._domStorageObjectWasInspected, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DatabaseWasAdded, this._databaseWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DatabaseWasInspected, this._databaseWasInspected, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.IndexedDatabaseWasAdded, this._indexedDatabaseWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.Cleared, this._storageCleared, this);

        WebInspector.applicationCacheManager.addEventListener(WebInspector.ApplicationCacheManager.Event.FrameManifestAdded, this._frameManifestAdded, this);
        WebInspector.applicationCacheManager.addEventListener(WebInspector.ApplicationCacheManager.Event.FrameManifestRemoved, this._frameManifestRemoved, this);

        this.contentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        for (var domStorageObject of WebInspector.storageManager.domStorageObjects)
            this._addDOMStorageObject(domStorageObject);

        for (var cookieStorageObject of WebInspector.storageManager.cookieStorageObjects)
            this._addCookieStorageObject(cookieStorageObject);

        for (var database of WebInspector.storageManager.databases)
            this._addDatabase(database);

        for (var indexedDatabase of WebInspector.storageManager.indexedDatabases)
            this._addIndexedDatabase(indexedDatabase);

        for (var applicationCacheObject of WebInspector.applicationCacheManager.applicationCacheObjects)
            this._addFrameManifest(applicationCacheObject);
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

        WebInspector.storageManager.removeEventListener(null, null, this);
        WebInspector.applicationCacheManager.removeEventListener(null, null, this);
    }

    // Protected

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

        // Show everything if there is no selection or "All Storage" is selected (the exclusive item).
        if (!selectedScopeBarItem || selectedScopeBarItem.exclusive)
            return true;

        // Folders are hidden on the first pass, but visible childen under the folder will force the folder visible again.
        if (treeElement instanceof WebInspector.FolderTreeElement)
            return false;

        function match()
        {
            for (var constructor of selectedScopeBarItem.__storageTypeInfo.classes) {
                if (constructor === WebInspector.DOMStorageTreeElement && treeElement instanceof constructor)
                    return treeElement.representedObject.isLocalStorage() === selectedScopeBarItem.__storageTypeInfo.localStorage;
                if (treeElement instanceof constructor)
                    return true;
            }

            return false;
        }

        var matched = match();
        if (matched)
            flags.expandTreeElement = true;
        return matched;
    }

    // Private

    _treeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        if (treeElement instanceof WebInspector.FolderTreeElement || treeElement instanceof WebInspector.DatabaseHostTreeElement ||
            treeElement instanceof WebInspector.IndexedDatabaseHostTreeElement || treeElement instanceof WebInspector.IndexedDatabaseTreeElement
            || treeElement instanceof WebInspector.ApplicationCacheManifestTreeElement)
            return;

        if (treeElement instanceof WebInspector.StorageTreeElement || treeElement instanceof WebInspector.DatabaseTableTreeElement ||
            treeElement instanceof WebInspector.DatabaseTreeElement || treeElement instanceof WebInspector.ApplicationCacheFrameTreeElement ||
            treeElement instanceof WebInspector.IndexedDatabaseObjectStoreTreeElement || treeElement instanceof WebInspector.IndexedDatabaseObjectStoreIndexTreeElement) {
            WebInspector.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    }

    _domStorageObjectWasAdded(event)
    {
        var domStorage = event.data.domStorage;
        this._addDOMStorageObject(event.data.domStorage);
    }

    _addDOMStorageObject(domStorage)
    {
        var storageElement = new WebInspector.DOMStorageTreeElement(domStorage);

        if (domStorage.isLocalStorage())
            this._localStorageRootTreeElement = this._addStorageChild(storageElement, this._localStorageRootTreeElement, WebInspector.UIString("Local Storage"));
        else
            this._sessionStorageRootTreeElement = this._addStorageChild(storageElement, this._sessionStorageRootTreeElement, WebInspector.UIString("Session Storage"));
    }

    _domStorageObjectWasInspected(event)
    {
        var domStorage = event.data.domStorage;
        var treeElement = this.treeElementForRepresentedObject(domStorage);
        treeElement.revealAndSelect(true);
    }

    _databaseWasAdded(event)
    {
        var database = event.data.database;
        this._addDatabase(event.data.database);
    }

    _addDatabase(database)
    {
        console.assert(database instanceof WebInspector.DatabaseObject);

        let databaseHostElement = this._databaseHostTreeElementMap.get(database.host);
        if (!databaseHostElement) {
            databaseHostElement = new WebInspector.DatabaseHostTreeElement(database.host);
            this._databaseHostTreeElementMap.set(database.host, databaseHostElement);
            this._databaseRootTreeElement = this._addStorageChild(databaseHostElement, this._databaseRootTreeElement, WebInspector.UIString("Databases"));
        }

        let databaseElement = new WebInspector.DatabaseTreeElement(database);
        databaseHostElement.appendChild(databaseElement);
    }

    _databaseWasInspected(event)
    {
        var database = event.data.database;
        var treeElement = this.treeElementForRepresentedObject(database);
        treeElement.revealAndSelect(true);
    }

    _indexedDatabaseWasAdded(event)
    {
        this._addIndexedDatabase(event.data.indexedDatabase);
    }

    _addIndexedDatabase(indexedDatabase)
    {
        console.assert(indexedDatabase instanceof WebInspector.IndexedDatabase);

        let indexedDatabaseHostElement = this._indexedDatabaseHostTreeElementMap.get(indexedDatabase.host);
        if (!indexedDatabaseHostElement) {
            indexedDatabaseHostElement = new WebInspector.IndexedDatabaseHostTreeElement(indexedDatabase.host);
            this._indexedDatabaseHostTreeElementMap.set(indexedDatabase.host, indexedDatabaseHostElement);
            this._indexedDatabaseRootTreeElement = this._addStorageChild(indexedDatabaseHostElement, this._indexedDatabaseRootTreeElement, WebInspector.UIString("Indexed Databases"));
        }

        let indexedDatabaseElement = new WebInspector.IndexedDatabaseTreeElement(indexedDatabase);
        indexedDatabaseHostElement.appendChild(indexedDatabaseElement);
    }

    _cookieStorageObjectWasAdded(event)
    {
        this._addCookieStorageObject(event.data.cookieStorage);
    }

    _addCookieStorageObject(cookieStorage)
    {
        console.assert(cookieStorage instanceof WebInspector.CookieStorageObject);

        var cookieElement = new WebInspector.CookieStorageTreeElement(cookieStorage);
        this._cookieStorageRootTreeElement = this._addStorageChild(cookieElement, this._cookieStorageRootTreeElement, WebInspector.UIString("Cookies"));
    }

    _frameManifestAdded(event)
    {
        this._addFrameManifest(event.data.frameManifest);
    }

    _addFrameManifest(frameManifest)
    {
        console.assert(frameManifest instanceof WebInspector.ApplicationCacheFrame);

        let manifest = frameManifest.manifest;
        let manifestURL = manifest.manifestURL;
        let applicationCacheManifestElement = this._applicationCacheURLTreeElementMap.get(manifestURL);
        if (!applicationCacheManifestElement) {
            applicationCacheManifestElement = new WebInspector.ApplicationCacheManifestTreeElement(manifest);
            this._applicationCacheURLTreeElementMap.set(manifestURL, applicationCacheManifestElement);
            this._applicationCacheRootTreeElement = this._addStorageChild(applicationCacheManifestElement, this._applicationCacheRootTreeElement, WebInspector.UIString("Application Cache"));
        }

        let frameCacheElement = new WebInspector.ApplicationCacheFrameTreeElement(frameManifest);
        applicationCacheManifestElement.appendChild(frameCacheElement);
    }

    _frameManifestRemoved(event)
    {
         // FIXME: Implement this.
    }

    _compareTreeElements(a, b)
    {
        console.assert(a.mainTitle);
        console.assert(b.mainTitle);

        return (a.mainTitle || "").localeCompare(b.mainTitle || "");
    }

    _addStorageChild(childElement, parentElement, folderName)
    {
        if (!parentElement) {
            childElement.flattened = true;

            this.contentTreeOutline.insertChild(childElement, insertionIndexForObjectInListSortedByFunction(childElement, this.contentTreeOutline.children, this._compareTreeElements));

            return childElement;
        }

        if (parentElement instanceof WebInspector.StorageTreeElement) {
            console.assert(parentElement.flattened);

            var previousOnlyChild = parentElement;
            previousOnlyChild.flattened = false;
            this.contentTreeOutline.removeChild(previousOnlyChild);

            var folderElement = new WebInspector.FolderTreeElement(folderName);
            this.contentTreeOutline.insertChild(folderElement, insertionIndexForObjectInListSortedByFunction(folderElement, this.contentTreeOutline.children, this._compareTreeElements));

            folderElement.appendChild(previousOnlyChild);
            folderElement.insertChild(childElement, insertionIndexForObjectInListSortedByFunction(childElement, folderElement.children, this._compareTreeElements));

            return folderElement;
        }

        console.assert(parentElement instanceof WebInspector.FolderTreeElement);
        parentElement.insertChild(childElement, insertionIndexForObjectInListSortedByFunction(childElement, parentElement.children, this._compareTreeElements));

        return parentElement;
    }

    _storageCleared(event)
    {
        // Close all DOM and cookie storage content views since the main frame has navigated and all storages are cleared.
        this.contentBrowser.contentViewContainer.closeAllContentViewsOfPrototype(WebInspector.CookieStorageContentView);
        this.contentBrowser.contentViewContainer.closeAllContentViewsOfPrototype(WebInspector.DOMStorageContentView);
        this.contentBrowser.contentViewContainer.closeAllContentViewsOfPrototype(WebInspector.DatabaseTableContentView);
        this.contentBrowser.contentViewContainer.closeAllContentViewsOfPrototype(WebInspector.DatabaseContentView);
        this.contentBrowser.contentViewContainer.closeAllContentViewsOfPrototype(WebInspector.ApplicationCacheFrameContentView);

        if (this._localStorageRootTreeElement && this._localStorageRootTreeElement.parent)
            this._localStorageRootTreeElement.parent.removeChild(this._localStorageRootTreeElement);

        if (this._sessionStorageRootTreeElement && this._sessionStorageRootTreeElement.parent)
            this._sessionStorageRootTreeElement.parent.removeChild(this._sessionStorageRootTreeElement);

        if (this._databaseRootTreeElement && this._databaseRootTreeElement.parent)
            this._databaseRootTreeElement.parent.removeChild(this._databaseRootTreeElement);

        if (this._indexedDatabaseRootTreeElement && this._indexedDatabaseRootTreeElement.parent)
            this._indexedDatabaseRootTreeElement.parent.removeChild(this._indexedDatabaseRootTreeElement);

        if (this._cookieStorageRootTreeElement && this._cookieStorageRootTreeElement.parent)
            this._cookieStorageRootTreeElement.parent.removeChild(this._cookieStorageRootTreeElement);

        if (this._applicationCacheRootTreeElement && this._applicationCacheRootTreeElement.parent)
            this._applicationCacheRootTreeElement.parent.removeChild(this._applicationCacheRootTreeElement);

        this._localStorageRootTreeElement = null;
        this._sessionStorageRootTreeElement = null;
        this._databaseRootTreeElement = null;
        this._databaseHostTreeElementMap.clear();
        this._indexedDatabaseRootTreeElement = null;
        this._indexedDatabaseHostTreeElementMap.clear();
        this._cookieStorageRootTreeElement = null;
        this._applicationCacheRootTreeElement = null;
        this._applicationCacheURLTreeElementMap.clear();
    }

    _scopeBarSelectionDidChange(event)
    {
        this.updateFilter();
    }
};
