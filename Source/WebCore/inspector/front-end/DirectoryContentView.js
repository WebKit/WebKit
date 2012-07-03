/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.DataGrid}
 */
WebInspector.DirectoryContentView = function()
{
    const indexes = WebInspector.DirectoryContentView.columnIndexes;
    var columns = {};
    columns[indexes.Name] = {};
    columns[indexes.Name].title = WebInspector.UIString("Name");
    columns[indexes.Name].sort = "ascending";
    columns[indexes.Name].sortable = true;
    columns[indexes.Name].width = "20%";
    columns[indexes.URL] = {};
    columns[indexes.URL].title = WebInspector.UIString("URL");
    columns[indexes.URL].sortable = true;
    columns[indexes.URL].width = "20%";
    columns[indexes.Type] = {};
    columns[indexes.Type].title = WebInspector.UIString("Type");
    columns[indexes.Type].sortable = true;
    columns[indexes.Type].width = "15%";
    columns[indexes.Size] = {};
    columns[indexes.Size].title = WebInspector.UIString("Size");
    columns[indexes.Size].sortable = true;
    columns[indexes.Size].width = "10%";
    columns[indexes.ModificationTime] = {};
    columns[indexes.ModificationTime].title = WebInspector.UIString("Modification Time");
    columns[indexes.ModificationTime].sortable = true;
    columns[indexes.ModificationTime].width = "25%";

    WebInspector.DataGrid.call(this, columns);
    this.addEventListener("sorting changed", this._sort, this);
}

WebInspector.DirectoryContentView.columnIndexes = {
    Name: "0",
    URL: "1",
    Type: "2",
    Size: "3",
    ModificationTime: "4"
}

WebInspector.DirectoryContentView.prototype = {
    /**
     * @param {Array.<WebInspector.FileSystemModel.Directory>} entries
     */
    showEntries: function(entries)
    {
        const indexes = WebInspector.DirectoryContentView.columnIndexes;
        this.rootNode().removeChildren();
        for (var i = 0; i < entries.length; ++i)
            this.rootNode().appendChild(new WebInspector.DirectoryContentView.Node(entries[i]));
    },

    _sort: function()
    {
        var column = /** @type {string} */ this.sortColumnIdentifier;
        this.sortNodes(WebInspector.DirectoryContentView.Node.comparator(column, this.sortOrder === "descending"), false);
    }
}

WebInspector.DirectoryContentView.prototype.__proto__ = WebInspector.DataGrid.prototype;

/**
 * @constructor
 * @extends {WebInspector.DataGridNode}
 * @param {WebInspector.FileSystemModel.Entry} entry
 */
WebInspector.DirectoryContentView.Node = function(entry)
{
    const indexes = WebInspector.DirectoryContentView.columnIndexes;
    var data = {};
    data[indexes.Name] = entry.name;
    data[indexes.URL] = entry.url;
    data[indexes.Type] = entry.isDirectory ? WebInspector.UIString("Directory") : entry.mimeType;
    data[indexes.Size] = "";
    data[indexes.ModificationTime] = "";

    WebInspector.DataGridNode.call(this, data);
    this._entry = entry;
    this._metadata = null;

    this._entry.requestMetadata(this._metadataReceived.bind(this));
}

/**
 * @param {string} column
 * @param {boolean} reverse
 */
WebInspector.DirectoryContentView.Node.comparator = function(column, reverse)
{
    var reverseFactor = reverse ? -1 : 1;
    const indexes = WebInspector.DirectoryContentView.columnIndexes;

    switch (column) {
    case indexes.Name:
    case indexes.URL:
        return function(x, y)
        {
            return isDirectoryCompare(x, y) || nameCompare(x, y);
        };
    case indexes.Type:
        return function(x, y)
        {
            return isDirectoryCompare(x ,y) || typeCompare(x, y) || nameCompare(x, y);
        };
    case indexes.Size:
        return function(x, y)
        {
            return isDirectoryCompare(x, y) || sizeCompare(x, y) || nameCompare(x, y);
        };
    case indexes.ModificationTime:
        return function(x, y)
        {
            return isDirectoryCompare(x, y) || modificationTimeCompare(x, y) || nameCompare(x, y);
        };
    }

    function isDirectoryCompare(x, y)
    {
        if (x._entry.isDirectory != y._entry.isDirectory)
            return y._entry.isDirectory ? 1 : -1;
        return 0;
    }

    function nameCompare(x, y)
    {
        return reverseFactor * x._entry.name.localeCompare(y._entry.name);
    }

    function typeCompare(x, y)
    {
        return reverseFactor * (x._entry.mimeType || "").localeCompare(y._entry.mimeType || "");
    }

    function sizeCompare(x, y)
    {
        return reverseFactor * ((x._metadata ? x._metadata.size : 0) - (y._metadata ? y._metadata.size : 0));
    }

    function modificationTimeCompare(x, y)
    {
        return reverseFactor * ((x._metadata ? x._metadata.modificationTime : 0) - (y._metadata ? y._metadata.modificationTime : 0));
    }
}

WebInspector.DirectoryContentView.Node.prototype = {
    /**
     * @param {number} errorCode
     * @param {FileSystemAgent.Metadata} metadata
     */
    _metadataReceived: function(errorCode, metadata)
    {
        const indexes = WebInspector.DirectoryContentView.columnIndexes;
        if (errorCode !== 0)
            return;

        this._metadata = metadata;
        var data = this.data;
        if (this._entry.isDirectory)
            data[indexes.Size] = WebInspector.UIString("-");
        else
            data[indexes.Size] = Number.bytesToString(metadata.size);
        data[indexes.ModificationTime] = new Date(metadata.modificationTime).toGMTString();
        this.data = data;
    }
}

WebInspector.DirectoryContentView.Node.prototype.__proto__ = WebInspector.DataGridNode.prototype;
