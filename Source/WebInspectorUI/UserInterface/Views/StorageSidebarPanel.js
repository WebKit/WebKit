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

        this._localStorageRootTreeElement = null;
        this._sessionStorageRootTreeElement = null;

        this._databaseRootTreeElement = null;
        this._databaseHostTreeElementMap = {};

        this._indexedDatabaseRootTreeElement = null;
        this._indexedDatabaseHostTreeElementMap = {};

        this._cookieStorageRootTreeElement = null;

        this._applicationCacheRootTreeElement = null;
        this._applicationCacheURLTreeElementMap = {};

        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.CookieStorageObjectWasAdded, this._cookieStorageObjectWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DOMStorageObjectWasAdded, this._domStorageObjectWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DOMStorageObjectWasInspected, this._domStorageObjectWasInspected, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DatabaseWasAdded, this._databaseWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.DatabaseWasInspected, this._databaseWasInspected, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.IndexedDatabaseWasAdded, this._indexedDatabaseWasAdded, this);
        WebInspector.storageManager.addEventListener(WebInspector.StorageManager.Event.Cleared, this._storageCleared, this);

        WebInspector.applicationCacheManager.addEventListener(WebInspector.ApplicationCacheManager.Event.FrameManifestAdded, this._frameManifestAdded, this);
        WebInspector.applicationCacheManager.addEventListener(WebInspector.ApplicationCacheManager.Event.FrameManifestRemoved, this._frameManifestRemoved, this);

        this.contentTreeOutline.onselect = this._treeElementSelected.bind(this);

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

    // Private

    _treeElementSelected(treeElement, selectedByUser)
    {
        if (treeElement instanceof WebInspector.FolderTreeElement || treeElement instanceof WebInspector.DatabaseHostTreeElement ||
            treeElement instanceof WebInspector.IndexedDatabaseHostTreeElement || treeElement instanceof WebInspector.IndexedDatabaseTreeElement)
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

        if (!this._databaseHostTreeElementMap[database.host]) {
            this._databaseHostTreeElementMap[database.host] = new WebInspector.DatabaseHostTreeElement(database.host);
            this._databaseRootTreeElement = this._addStorageChild(this._databaseHostTreeElementMap[database.host], this._databaseRootTreeElement, WebInspector.UIString("Databases"));
        }

        var databaseElement = new WebInspector.DatabaseTreeElement(database);
        this._databaseHostTreeElementMap[database.host].appendChild(databaseElement);
    }

    _databaseWasInspected(event)
    {
        var database = event.data.database;
        var treeElement = this.treeElementForRepresentedObject(database);
        treeElement.revealAndSelect(true);
    }

    _indexedDatabaseWasAdded(event)
    {
        this._addIndexedDatabaseWasAdded(event.data.indexedDatabase);
    }

    _addIndexedDatabase(indexedDatabase)
    {
        console.assert(indexedDatabase instanceof WebInspector.IndexedDatabase);

        if (!this._indexedDatabaseHostTreeElementMap[indexedDatabase.host]) {
            this._indexedDatabaseHostTreeElementMap[indexedDatabase.host] = new WebInspector.IndexedDatabaseHostTreeElement(indexedDatabase.host);
            this._indexedDatabaseRootTreeElement = this._addStorageChild(this._indexedDatabaseHostTreeElementMap[indexedDatabase.host], this._indexedDatabaseRootTreeElement, WebInspector.UIString("Indexed Databases"));
        }

        var indexedDatabaseElement = new WebInspector.IndexedDatabaseTreeElement(indexedDatabase);
        this._indexedDatabaseHostTreeElementMap[indexedDatabase.host].appendChild(indexedDatabaseElement);
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

        var manifest = frameManifest.manifest;
        var manifestURL = manifest.manifestURL;
        if (!this._applicationCacheURLTreeElementMap[manifestURL]) {
            this._applicationCacheURLTreeElementMap[manifestURL] = new WebInspector.ApplicationCacheManifestTreeElement(manifest);
            this._applicationCacheRootTreeElement = this._addStorageChild(this._applicationCacheURLTreeElementMap[manifestURL], this._applicationCacheRootTreeElement, WebInspector.UIString("Application Cache"));
        }

        var frameCacheElement = new WebInspector.ApplicationCacheFrameTreeElement(frameManifest);
        this._applicationCacheURLTreeElementMap[manifestURL].appendChild(frameCacheElement);
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
        this._databaseHostTreeElementMap = {};
        this._indexedDatabaseRootTreeElement = null;
        this._indexedDatabaseHostTreeElementMap = {};
        this._cookieStorageRootTreeElement = null;
        this._applicationCacheRootTreeElement = null;
        this._applicationCacheURLTreeElementMap = {};
    }
};
