/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

WebInspector.DOMStorageContentView = function(representedObject)
{
    WebInspector.ContentView.call(this, representedObject);

    this.element.classList.add(WebInspector.DOMStorageContentView.StyleClassName);

    representedObject.addEventListener(WebInspector.DOMStorageObject.Event.ItemsCleared, this.itemsCleared, this);
    representedObject.addEventListener(WebInspector.DOMStorageObject.Event.ItemAdded, this.itemAdded, this);
    representedObject.addEventListener(WebInspector.DOMStorageObject.Event.ItemRemoved, this.itemRemoved, this);
    representedObject.addEventListener(WebInspector.DOMStorageObject.Event.ItemUpdated, this.itemUpdated, this);

    this.reset();
};

WebInspector.DOMStorageContentView.StyleClassName = "dom-storage";

WebInspector.DOMStorageContentView.prototype = {
    constructor: WebInspector.DOMStorageContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    reset: function()
    {
        this.representedObject.getEntries(this._showDOMStorageEntries.bind(this));
    },

    saveToCookie: function(cookie)
    {
        cookie.type = WebInspector.ContentViewCookieType.DOMStorage;
        cookie.isLocalStorage = this.representedObject.isLocalStorage();
        cookie.host = this.representedObject.host;
    },

    itemsCleared: function(event)
    {
        this._dataGrid.removeChildren();
        this._dataGrid.addCreationNode(false);
    },

    itemRemoved: function(event)
    {
        for (var node of this._dataGrid.children) {
            if (node.data[0] === event.data.key)
                return this._dataGrid.removeChild(node);
        }
    },

    itemAdded: function(event)
    {
        var key = event.data.key;
        var value = event.data.value;

        // Enforce key uniqueness.
        for (var node of this._dataGrid.children) {
            if (node.data[0] === key)
                return;
        }

        var data = {};
        data[0] = key;
        data[1] = value;

        var childNode = new WebInspector.DataGridNode(data, false);

        this._dataGrid.insertChild(childNode, this._dataGrid.children.length - 1);
        if (this._dataGrid.sortOrder)
            this._sortDataGrid();
    },

    itemUpdated: function(event)
    {
        var key = event.data.key;
        var value = event.data.value;

        var keyFound = false;
        for (var childNode of this._dataGrid.children) {
            if (childNode.data[0] === key) {
                // Remove any rows that are now duplicates.
                if (keyFound) {
                    this._dataGrid.removeChild(childNode);
                    continue;
                }

                keyFound = true;
                childNode.data[1] = value;
                childNode.refresh();
            }
        }
    },

    updateLayout: function()
    {
        if (this._dataGrid)
            this._dataGrid.updateLayout();
    },

    get scrollableElements()
    {
        if (!this._dataGrid)
            return [];
        return [this._dataGrid.scrollContainer];
    },

    // Private

    _showDOMStorageEntries: function(error, entries)
    {
        if (error)
            return;

        this._updateDataGridForDOMStorageEntries(entries);

        this._dataGrid.updateLayout();
    },

    _updateDataGridForDOMStorageEntries: function(entries)
    {
        if (!this._dataGrid) {
            var columns = {};
            columns[0] = {title: WebInspector.UIString("Key"), sortable: true};
            columns[1] = {title: WebInspector.UIString("Value"), sortable: true};

            this._dataGrid = new WebInspector.DataGrid(columns, this._editingCallback.bind(this), this._deleteCallback.bind(this));
            this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sortDataGrid, this);

            this.element.appendChild(this._dataGrid.element);
        }

        console.assert(this._dataGrid);

        var nodes = [];
        for (var i = 0; i < entries.length; ++i) {
            var data = {};

            var key = entries[i][0];
            var value = entries[i][1];

            data[0] = key;
            data[1] = value;

            var node = new WebInspector.DataGridNode(data, false);
            node.selectable = true;

            nodes.push(node);
        }

        if (this._dataGrid.creationNode)
            this._dataGrid.removeChild(this._dataGrid.creationNode);

        this._insertNodesIntoDataGridWithSort(nodes);

        if (nodes.length > 0)
            nodes[0].selected = true;
    },

    _sortDataGrid: function()
    {
        if (this._dataGrid.creationNode)
            this._dataGrid.removeChild(this._dataGrid.creationNode);

        var nodes = this._dataGrid.children.slice();
        this._insertNodesIntoDataGridWithSort(nodes);
    },

    _insertNodesIntoDataGridWithSort: function(nodes)
    {
        console.assert(!this._dataGrid.creationNode);

        var sortColumnIdentifier = this._dataGrid.sortColumnIdentifier;
        var sortAscending = this._dataGrid.sortOrder === "ascending";

        function comparator(a, b)
        {
            var result = b.data[sortColumnIdentifier].localeCompare(a.data[sortColumnIdentifier]);
            return sortAscending ? -result : result;
        }

        if (sortColumnIdentifier)
            nodes.sort(comparator);

        this._dataGrid.removeChildren();
        for (var i = 0; i < nodes.length; i++)
            this._dataGrid.appendChild(nodes[i]);
        this._dataGrid.addCreationNode(false);
    },

    _deleteCallback: function(node)
    {
        if (!node || node.isCreationNode)
            return;

        if (this.representedObject)
            this.representedObject.removeItem(node.data[0]);

        this.reset();
    },

    _editingCallback: function(editingNode, columnIdentifier, oldText, newText)
    {
        var domStorage = this.representedObject;
        if (columnIdentifier === "0") {
            if (oldText)
                domStorage.removeItem(oldText);

            domStorage.setItem(newText, editingNode.data[1]);
        } else
            domStorage.setItem(editingNode.data[0], newText);

        this.reset();
    }
};
