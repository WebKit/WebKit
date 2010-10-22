/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.StoragePanel = function(database)
{
    WebInspector.Panel.call(this, "storage");

    this.createSidebar();
    this.sidebarElement.addStyleClass("outline-disclosure filter-all children small");

    if (Preferences.networkPanelEnabled) {
        this.resourcesListTreeElement = new WebInspector.StorageCategoryTreeElement(this, WebInspector.UIString("Resources"), "frame-storage-tree-item");
        this.sidebarTree.appendChild(this.resourcesListTreeElement);
        this.resourcesListTreeElement.expand();
        this._treeElementForFrameId = {};
    }

    this.databasesListTreeElement = new WebInspector.StorageCategoryTreeElement(this, WebInspector.UIString("Databases"), "database-storage-tree-item");
    this.sidebarTree.appendChild(this.databasesListTreeElement);
    this.databasesListTreeElement.expand();

    this.localStorageListTreeElement = new WebInspector.StorageCategoryTreeElement(this, WebInspector.UIString("Local Storage"), "domstorage-storage-tree-item local-storage");
    this.sidebarTree.appendChild(this.localStorageListTreeElement);
    this.localStorageListTreeElement.expand();

    this.sessionStorageListTreeElement = new WebInspector.StorageCategoryTreeElement(this, WebInspector.UIString("Session Storage"), "domstorage-storage-tree-item session-storage");
    this.sidebarTree.appendChild(this.sessionStorageListTreeElement);
    this.sessionStorageListTreeElement.expand();

    this.cookieListTreeElement = new WebInspector.StorageCategoryTreeElement(this, WebInspector.UIString("Cookies"), "cookie-storage-tree-item");
    this.sidebarTree.appendChild(this.cookieListTreeElement);
    this.cookieListTreeElement.expand();
    
    this.applicationCacheListTreeElement = new WebInspector.StorageCategoryTreeElement(this, WebInspector.UIString("Application Cache"), "application-cache-storage-tree-item");
    this.sidebarTree.appendChild(this.applicationCacheListTreeElement);
    this.applicationCacheListTreeElement.expand();

    this.storageViews = document.createElement("div");
    this.storageViews.id = "storage-views";
    this.element.appendChild(this.storageViews);

    this.storageViewStatusBarItemsContainer = document.createElement("div");
    this.storageViewStatusBarItemsContainer.className = "status-bar-items";

    this._databases = [];
    this._domStorage = [];
    this._cookieViews = {};
}

WebInspector.StoragePanel.prototype = {
    get toolbarItemLabel()
    {
        return WebInspector.UIString("Storage");
    },

    get statusBarItems()
    {
        return [this.storageViewStatusBarItemsContainer];
    },

    reset: function()
    {
        for (var i = 0; i < this._databases.length; ++i) {
            var database = this._databases[i];
            delete database._tableViews;
            delete database._queryView;
        }
        this._databases = [];

        var domStorageLength = this._domStorage.length;
        for (var i = 0; i < this._domStorage.length; ++i) {
            var domStorage = this._domStorage[i];
            delete domStorage._domStorageView;
        }
        this._domStorage = [];

        this._cookieViews = {};

        this._applicationCacheView = null;
        delete this._cachedApplicationCacheViewStatus;

        this.databasesListTreeElement.removeChildren();
        this.localStorageListTreeElement.removeChildren();
        this.sessionStorageListTreeElement.removeChildren();
        this.cookieListTreeElement.removeChildren();
        this.applicationCacheListTreeElement.removeChildren();

        this.storageViews.removeChildren();

        this.storageViewStatusBarItemsContainer.removeChildren();
        
        if (this.sidebarTree.selectedTreeElement)
            this.sidebarTree.selectedTreeElement.deselect();
    },

    addOrUpdateFrame: function(parentFrameId, frameId, displayName)
    {
        var frameTreeElement = this._treeElementForFrameId[frameId];
        if (frameTreeElement) {
            frameTreeElement.displayName = displayName;
            return;
        }

        var parentTreeElement = parentFrameId ? this._treeElementForFrameId[parentFrameId] : this.resourcesListTreeElement;
        if (!parentTreeElement) {
            console.error("No frame with id:" + parentFrameId + " to route " + displayName + " to.")
            return;
        }

        var frameTreeElement = new WebInspector.FrameTreeElement(this, frameId, displayName);
        this._treeElementForFrameId[frameId] = frameTreeElement;

        // Insert in the alphabetical order, first frames, then resources.
        var children = parentTreeElement.children;
        for (var i = 0; i < children.length; ++i) {
            var child = children[i];
            if (!(child instanceof WebInspector.FrameTreeElement)) {
                parentTreeElement.insertChild(frameTreeElement, i);
                return;
            }
            if (child.displayName.localeCompare(frameTreeElement.displayName) > 0) {
                parentTreeElement.insertChild(frameTreeElement, i);
                return;
            }
        }
        parentTreeElement.appendChild(frameTreeElement);
    },

    removeFrame: function(frameId)
    {
        var frameTreeElement = this._treeElementForFrameId[frameId];
        if (!frameTreeElement)
            return;

        var children = frameTreeElement.children.slice();
        for (var i = 0; i < children.length; ++i)
            this.removeFrame(children[i]._frameId);

        delete this._treeElementForFrameId[frameId];
        frameTreeElement.parent.removeChild(frameTreeElement);
    },

    addResourceToFrame: function(frameId, resource)
    {
        var frameTreeElement = this._treeElementForFrameId[frameId];
        if (!frameTreeElement) {
            console.error("No frame to add resource to");
            return;
        }

        var resourceTreeElement = new WebInspector.FrameResourceTreeElement(this, resource);

        // Insert in the alphabetical order, first frames, then resources. Document resource goes first.
        var children = frameTreeElement.children;
        for (var i = 0; i < children.length; ++i) {
            var child = children[i];
            if (!(child instanceof WebInspector.FrameResourceTreeElement))
                continue;

            if (resource.type === WebInspector.Resource.Type.Document ||
                    (child._resource.type !== WebInspector.Resource.Type.Document && child._resource.displayName.localeCompare(resource.displayName) > 0)) {
                frameTreeElement.insertChild(resourceTreeElement, i);
                return;
            }
        }
        frameTreeElement.appendChild(resourceTreeElement);
    },

    removeResourcesFromFrame: function(frameId)
    {
        var frameTreeElement = this._treeElementForFrameId[frameId];
        if (frameTreeElement)
            frameTreeElement.removeChildren();
    },

    addDatabase: function(database)
    {
        this._databases.push(database);

        var databaseTreeElement = new WebInspector.DatabaseTreeElement(this, database);
        database._databasesTreeElement = databaseTreeElement;
        this.databasesListTreeElement.appendChild(databaseTreeElement);
    },
    
    addCookieDomain: function(domain)
    {
        var cookieDomainTreeElement = new WebInspector.CookieTreeElement(this, domain);
        this.cookieListTreeElement.appendChild(cookieDomainTreeElement);
    },

    addDOMStorage: function(domStorage)
    {
        this._domStorage.push(domStorage);
        var domStorageTreeElement = new WebInspector.DOMStorageTreeElement(this, domStorage, (domStorage.isLocalStorage ? "local-storage" : "session-storage"));
        domStorage._domStorageTreeElement = domStorageTreeElement;
        if (domStorage.isLocalStorage)
            this.localStorageListTreeElement.appendChild(domStorageTreeElement);
        else
            this.sessionStorageListTreeElement.appendChild(domStorageTreeElement);
    },

    addApplicationCache: function(domain)
    {
        var applicationCacheTreeElement = new WebInspector.ApplicationCacheTreeElement(this, domain);
        this.applicationCacheListTreeElement.appendChild(applicationCacheTreeElement);
    },

    selectDatabase: function(databaseId)
    {
        var database;
        for (var i = 0, len = this._databases.length; i < len; ++i) {
            database = this._databases[i];
            if (database.id === databaseId) {
                this.showDatabase(database);
                database._databasesTreeElement.select();
                return;
            }
        }
    },

    selectDOMStorage: function(storageId)
    {
        var domStorage = this._domStorageForId(storageId);
        if (domStorage) {
            this.showDOMStorage(domStorage);
            domStorage._domStorageTreeElement.select();
        }
    },

    showResource: function(resource)
    {
        var view = WebInspector.ResourceManager.resourceViewForResource(resource);
        view.headersVisible = false;
        this._innerShowView(view);
    },

    showDatabase: function(database, tableName)
    {
        if (!database)
            return;

        var view;
        if (tableName) {
            if (!("_tableViews" in database))
                database._tableViews = {};
            view = database._tableViews[tableName];
            if (!view) {
                view = new WebInspector.DatabaseTableView(database, tableName);
                database._tableViews[tableName] = view;
            }
        } else {
            view = database._queryView;
            if (!view) {
                view = new WebInspector.DatabaseQueryView(database);
                database._queryView = view;
            }
        }

        this._innerShowView(view);
    },

    showDOMStorage: function(domStorage)
    {
        if (!domStorage)
            return;

        var view;
        view = domStorage._domStorageView;
        if (!view) {
            view = new WebInspector.DOMStorageItemsView(domStorage);
            domStorage._domStorageView = view;
        }

        this._innerShowView(view);
    },

    showCookies: function(treeElement, cookieDomain)
    {
        var view = this._cookieViews[cookieDomain];
        if (!view) {
            view = new WebInspector.CookieItemsView(treeElement, cookieDomain);
            this._cookieViews[cookieDomain] = view;
        }

        this._innerShowView(view);
    },

    showApplicationCache: function(treeElement, appcacheDomain)
    {
        var view = this._applicationCacheView;
        if (!view) {
            view = new WebInspector.ApplicationCacheItemsView(treeElement, appcacheDomain);
            this._applicationCacheView = view;
        }

        this._innerShowView(view);

        if ("_cachedApplicationCacheViewStatus" in this)
            this._applicationCacheView.updateStatus(this._cachedApplicationCacheViewStatus);
    },

    showCategoryView: function(categoryName)
    {
        if (!this._categoryView)
            this._categoryView = new WebInspector.StorageCategoryView();
        this._categoryView.setText(categoryName);
        this._innerShowView(this._categoryView);
    },

    _innerShowView: function(view)
    {
        if (this.visibleView)
            this.visibleView.hide();

        view.show(this.storageViews);
        this.visibleView = view;

        this.storageViewStatusBarItemsContainer.removeChildren();
        var statusBarItems = view.statusBarItems || [];
        for (var i = 0; i < statusBarItems.length; ++i)
            this.storageViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
    },

    closeVisibleView: function()
    {
        if (this.visibleView)
            this.visibleView.hide();
        delete this.visibleView;
    },

    updateDatabaseTables: function(database)
    {
        if (!database || !database._databasesTreeElement)
            return;

        database._databasesTreeElement.shouldRefreshChildren = true;

        if (!("_tableViews" in database))
            return;

        var tableNamesHash = {};
        var self = this;
        function tableNamesCallback(tableNames)
        {
            var tableNamesLength = tableNames.length;
            for (var i = 0; i < tableNamesLength; ++i)
                tableNamesHash[tableNames[i]] = true;

            for (var tableName in database._tableViews) {
                if (!(tableName in tableNamesHash)) {
                    if (self.visibleView === database._tableViews[tableName])
                        self.closeVisibleView();
                    delete database._tableViews[tableName];
                }
            }
        }
        database.getTableNames(tableNamesCallback);
    },

    dataGridForResult: function(columnNames, values)
    {
        var numColumns = columnNames.length;
        if (!numColumns)
            return null;

        var columns = {};

        for (var i = 0; i < columnNames.length; ++i) {
            var column = {};
            column.width = columnNames[i].length;
            column.title = columnNames[i];
            column.sortable = true;

            columns[columnNames[i]] = column;
        }

        var nodes = [];
        for (var i = 0; i < values.length / numColumns; ++i) {
            var data = {};
            for (var j = 0; j < columnNames.length; ++j)
                data[columnNames[j]] = values[numColumns * i + j];

            var node = new WebInspector.DataGridNode(data, false);
            node.selectable = false;
            nodes.push(node);
        }

        var dataGrid = new WebInspector.DataGrid(columns);
        var length = nodes.length;
        for (var i = 0; i < length; ++i)
            dataGrid.appendChild(nodes[i]);

        dataGrid.addEventListener("sorting changed", this._sortDataGrid.bind(this, dataGrid), this);
        return dataGrid;
    },

    _sortDataGrid: function(dataGrid)
    {
        var nodes = dataGrid.children.slice();
        var sortColumnIdentifier = dataGrid.sortColumnIdentifier;
        var sortDirection = dataGrid.sortOrder === "ascending" ? 1 : -1;
        var columnIsNumeric = true;

        for (var i = 0; i < nodes.length; i++) {
            if (isNaN(Number(nodes[i].data[sortColumnIdentifier])))
                columnIsNumeric = false;
        }

        function comparator(dataGridNode1, dataGridNode2)
        {
            var item1 = dataGridNode1.data[sortColumnIdentifier];
            var item2 = dataGridNode2.data[sortColumnIdentifier];

            var comparison;
            if (columnIsNumeric) {
                // Sort numbers based on comparing their values rather than a lexicographical comparison.
                var number1 = parseFloat(item1);
                var number2 = parseFloat(item2);
                comparison = number1 < number2 ? -1 : (number1 > number2 ? 1 : 0);
            } else
                comparison = item1 < item2 ? -1 : (item1 > item2 ? 1 : 0);

            return sortDirection * comparison;
        }

        nodes.sort(comparator);
        dataGrid.removeChildren();
        for (var i = 0; i < nodes.length; i++)
            dataGrid.appendChild(nodes[i]);
    },

    updateDOMStorage: function(storageId)
    {
        var domStorage = this._domStorageForId(storageId);
        if (!domStorage)
            return;

        var view = domStorage._domStorageView;
        if (this.visibleView && view === this.visibleView)
            domStorage._domStorageView.update();
    },

    updateApplicationCacheStatus: function(status)
    {
        this._cachedApplicationCacheViewStatus = status;
        if (this._applicationCacheView && this._applicationCacheView === this.visibleView)
            this._applicationCacheView.updateStatus(status);
    },

    updateNetworkState: function(isNowOnline)
    {
        if (this._applicationCacheView && this._applicationCacheView === this.visibleView)
            this._applicationCacheView.updateNetworkState(isNowOnline);
    },

    updateManifest: function(manifest)
    {
        if (this._applicationCacheView && this._applicationCacheView === this.visibleView)
            this._applicationCacheView.updateManifest(manifest);
    },

    _domStorageForId: function(storageId)
    {
        if (!this._domStorage)
            return null;
        var domStorageLength = this._domStorage.length;
        for (var i = 0; i < domStorageLength; ++i) {
            var domStorage = this._domStorage[i];
            if (domStorage.id == storageId)
                return domStorage;
        }
        return null;
    },

    updateMainViewWidth: function(width)
    {
        this.storageViews.style.left = width + "px";
        this.storageViewStatusBarItemsContainer.style.left = width + "px";
        this.resize();
    }
}

WebInspector.StoragePanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.BaseStorageTreeElement = function(storagePanel, title, iconClass, hasChildren)
{
    TreeElement.call(this, "", null, hasChildren);
    this._storagePanel = storagePanel;
    this._titleText = title;
    this._iconClass = iconClass;
}

WebInspector.BaseStorageTreeElement.prototype = {
    onattach: function()
    {
        this.listItemElement.removeChildren();
        this.listItemElement.addStyleClass(this._iconClass);

        this.imageElement = document.createElement("img");
        this.imageElement.className = "icon";
        this.listItemElement.appendChild(this.imageElement);

        this.titleElement = document.createElement("span");
        this.titleElement.textContent = this._titleText;
        this.titleElement.className = "storage-base-tree-element-title";
        this.listItemElement.appendChild(this.titleElement);

        var selectionElement = document.createElement("div");
        selectionElement.className = "selection";
        this.listItemElement.appendChild(selectionElement);
    },

    onreveal: function()
    {
        if (this.listItemElement)
            this.listItemElement.scrollIntoViewIfNeeded(false);
    },

    set titleText(titleText)
    {
        this._titleText = titleText;
        this.titleElement.textContent = this._titleText;
    }
}

WebInspector.BaseStorageTreeElement.prototype.__proto__ = TreeElement.prototype;

WebInspector.StorageCategoryTreeElement = function(storagePanel, categoryName, iconClass)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, categoryName, iconClass, true);
    this._categoryName = categoryName;
}

WebInspector.StorageCategoryTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showCategoryView(this._categoryName);
    }
}
WebInspector.StorageCategoryTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.FrameTreeElement = function(storagePanel, frameId, displayName)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, displayName, "frame-storage-tree-item");
    this._frameId = frameId;
    this._displayName = displayName;
}

WebInspector.FrameTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showCategoryView(this._displayName);
    },

    get displayName()
    {
        return this._displayName;
    },

    set displayName(displayName)
    {
        this._displayName = displayName;
        this.titleText = displayName;
    }
}
WebInspector.FrameTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.FrameResourceTreeElement = function(storagePanel, resource)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, resource.displayName, "resource-sidebar-tree-item resources-category-" + resource.category.name);
    this._resource = resource;
}

WebInspector.FrameResourceTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showResource(this._resource);
    },

    onattach: function()
    {
        WebInspector.BaseStorageTreeElement.prototype.onattach.call(this);

        if (this._resource.category === WebInspector.resourceCategories.images) {
            var previewImage = document.createElement("img");
            previewImage.className = "image-resource-icon-preview";
            previewImage.src = this._resource.url;

            var iconElement = document.createElement("div");
            iconElement.className = "icon";
            iconElement.appendChild(previewImage);
            this.listItemElement.replaceChild(iconElement, this.imageElement);
        }
    }
}

WebInspector.FrameResourceTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.DatabaseTreeElement = function(storagePanel, database)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, database.name, "database-storage-tree-item", true);
    this._database = database;
}

WebInspector.DatabaseTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showDatabase(this._database);
    },

    oncollapse: function()
    {
        // Request a refresh after every collapse so the next
        // expand will have an updated table list.
        this.shouldRefreshChildren = true;
    },

    onpopulate: function()
    {
        this.removeChildren();

        function tableNamesCallback(tableNames)
        {
            var tableNamesLength = tableNames.length;
            for (var i = 0; i < tableNamesLength; ++i)
                this.appendChild(new WebInspector.DatabaseTableTreeElement(this._storagePanel, this._database, tableNames[i]));
        }
        this._database.getTableNames(tableNamesCallback.bind(this));
    }

}
WebInspector.DatabaseTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.DatabaseTableTreeElement = function(storagePanel, database, tableName)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, tableName, "database-storage-tree-item");
    this._database = database;
    this._tableName = tableName;
}

WebInspector.DatabaseTableTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showDatabase(this._database, this._tableName);
    }
}
WebInspector.DatabaseTableTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.DOMStorageTreeElement = function(storagePanel, domStorage, className)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, domStorage.domain ? domStorage.domain : WebInspector.UIString("Local Files"), "domstorage-storage-tree-item " + className);
    this._domStorage = domStorage;
}

WebInspector.DOMStorageTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showDOMStorage(this._domStorage);
    }
}
WebInspector.DOMStorageTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.CookieTreeElement = function(storagePanel, cookieDomain)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, cookieDomain ? cookieDomain : WebInspector.UIString("Local Files"), "cookie-storage-tree-item");
    this._cookieDomain = cookieDomain;
}

WebInspector.CookieTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showCookies(this, this._cookieDomain);
    }
}
WebInspector.CookieTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.ApplicationCacheTreeElement = function(storagePanel, appcacheDomain)
{
    WebInspector.BaseStorageTreeElement.call(this, storagePanel, appcacheDomain ? appcacheDomain : WebInspector.UIString("Local Files"), "application-cache-storage-tree-item");
    this._appcacheDomain = appcacheDomain;
}

WebInspector.ApplicationCacheTreeElement.prototype = {
    onselect: function()
    {
        this._storagePanel.showApplicationCache(this, this._appcacheDomain);
    }
}
WebInspector.ApplicationCacheTreeElement.prototype.__proto__ = WebInspector.BaseStorageTreeElement.prototype;

WebInspector.StorageCategoryView = function()
{
    WebInspector.View.call(this);

    this.element.addStyleClass("storage-view");

    this._emptyMsgElement = document.createElement("div");
    this._emptyMsgElement.className = "storage-empty-view";
    this.element.appendChild(this._emptyMsgElement);
}

WebInspector.StorageCategoryView.prototype = {
    setText: function(text)
    {
        this._emptyMsgElement.textContent = text;
    }
}

WebInspector.StorageCategoryView.prototype.__proto__ = WebInspector.View.prototype;
