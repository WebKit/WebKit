/*
 * Copyright (C) 2010, 2013, 2014 Apple Inc. All rights reserved.
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

WebInspector.ApplicationCacheFrameContentView = function(representedObject)
{
    console.assert(representedObject instanceof WebInspector.ApplicationCacheFrame);

    WebInspector.ContentView.call(this, representedObject);

    this.element.classList.add(WebInspector.ApplicationCacheFrameContentView.StyleClassName);

    this.element.classList.add("storage-view");
    this.element.classList.add("table");

    this._frame = representedObject.frame;

    this._emptyView = WebInspector.createMessageTextView(WebInspector.UIString("No Application Cache information available"), false);
    this._emptyView.classList.add("hidden");
    this.element.appendChild(this._emptyView);

    this._markDirty();

    var status = representedObject.status;
    this.updateStatus(status);

    WebInspector.applicationCacheManager.addEventListener(WebInspector.ApplicationCacheManager.Event.FrameManifestStatusChanged, this._updateStatus, this);
}

WebInspector.ApplicationCacheFrameContentView.StyleClassName = "application-cache-frame";

WebInspector.ApplicationCacheFrameContentView.prototype = {
    constructor: WebInspector.ApplicationCacheFrameContentView,

    shown: function()
    {
        this._maybeUpdate();
    },

    closed: function()
    {
        WebInspector.applicationCacheManager.removeEventListener(WebInspector.ApplicationCacheManager.Event.FrameManifestStatusChanged, this._updateStatus, this);
    },

    updateLayout: function()
    {
        if (this.dataGrid)
            this.dataGrid.updateLayout();
    },

    saveToCookie: function(cookie)
    {
        cookie.type = WebInspector.ContentViewCookieType.ApplicationCache;
        cookie.frame = this.representedObject.frame.url;
        cookie.manifest = this.representedObject.manifest.manifestURL;
    },

    get scrollableElements()
    {
        if (!this._dataGrid)
            return [];
        return [this._dataGrid.scrollContainer];
    },

    _maybeUpdate: function()
    {
        if (!this.visible || !this._viewDirty)
            return;

        this._update();
        this._viewDirty = false;
    },

    _markDirty: function()
    {
        this._viewDirty = true;
    },

    _updateStatus: function(event)
    {
        var frameManifest = event.data.frameManifest;
        if (frameManifest !== this.representedObject)
            return;

        console.assert(frameManifest instanceof WebInspector.ApplicationCacheFrame);

        this.updateStatus(frameManifest.status);
    },

    updateStatus: function(status)
    {
        var oldStatus = this._status;
        this._status = status;

        if (this.visible && this._status === WebInspector.ApplicationCacheManager.Status.Idle && (oldStatus === WebInspector.ApplicationCacheManager.Status.UpdateReady || !this._resources))
            this._markDirty();

        this._maybeUpdate();
    },

    _update: function()
    {
        WebInspector.applicationCacheManager.requestApplicationCache(this._frame, this._updateCallback.bind(this));
    },

    _updateCallback: function(applicationCache)
    {
        if (!applicationCache || !applicationCache.manifestURL) {
            delete this._manifest;
            delete this._creationTime;
            delete this._updateTime;
            delete this._size;
            delete this._resources;

            this._emptyView.classList.remove("hidden");

            if (this._dataGrid)
                this._dataGrid.element.classList.add("hidden");
            return;
        }

        // FIXME: are these variables needed anywhere else?
        this._manifest = applicationCache.manifestURL;
        this._creationTime = applicationCache.creationTime;
        this._updateTime = applicationCache.updateTime;
        this._size = applicationCache.size;
        this._resources = applicationCache.resources;

        if (!this._dataGrid)
            this._createDataGrid();

        this._populateDataGrid();
        this._dataGrid.autoSizeColumns(20, 80);
        this._dataGrid.element.classList.remove("hidden");

        this._emptyView.classList.add("hidden");
    },

    _createDataGrid: function()
    {
        var columns = {url: {}, type: {}, size: {}};

        columns.url.title = WebInspector.UIString("Resource");
        columns.url.sortable = true;

        columns.type.title = WebInspector.UIString("Type");
        columns.type.sortable = true;

        columns.size.title = WebInspector.UIString("Size");
        columns.size.aligned = "right";
        columns.size.sortable = true;

        this._dataGrid = new WebInspector.DataGrid(columns);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sortDataGrid, this);

        this._dataGrid.sortColumnIdentifier = "url";
        this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;

        this.element.appendChild(this._dataGrid.element);
        this._dataGrid.updateLayout();
    },

    _sortDataGrid: function()
    {
        function numberCompare(columnIdentifier, nodeA, nodeB)
        {
            console.log(columnIdentifier, nodeA, nodeB);
            return nodeA.data[columnIdentifier] - nodeB.data[columnIdentifier];
        }
        function localeCompare(columnIdentifier, nodeA, nodeB)
        {
             return (nodeA.data[columnIdentifier] + "").localeCompare(nodeB.data[columnIdentifier] + "");
        }

        var comparator;
        switch (this._dataGrid.sortColumnIdentifier) {
            case "type": comparator = localeCompare.bind(this, "type"); break;
            case "size": comparator = numberCompare.bind(this, "size"); break;
            case "url":
            default:  comparator = localeCompare.bind(this, "url"); break;
        }

        this._dataGrid.sortNodes(comparator);
    },

    _populateDataGrid: function()
    {
        this._dataGrid.removeChildren();

        for (var resource of this._resources) {
            var data = {
                url: resource.url,
                type: resource.type,
                size: Number.bytesToString(resource.size)
            };
            var node = new WebInspector.DataGridNode(data);
            node.selectable = true;
            this._dataGrid.appendChild(node);
        }
    },

    _deleteButtonClicked: function(event)
    {
        if (!this._dataGrid || !this._dataGrid.selectedNode)
            return;

        // FIXME: Delete Button semantics are not yet defined. (Delete a single, or all?)
        this._deleteCallback(this._dataGrid.selectedNode);
    },

    _deleteCallback: function(node)
    {
        // FIXME: Should we delete a single (selected) resource or all resources?
        // InspectorBackend.deleteCachedResource(...)
        // this._update();
    }
}

WebInspector.ApplicationCacheFrameContentView.prototype.__proto__ = WebInspector.ContentView.prototype;
