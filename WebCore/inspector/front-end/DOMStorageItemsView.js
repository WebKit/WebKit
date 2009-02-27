/*
 * Copyright (C) 2008 Nokia Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DOMStorageItemsView = function(domStorage)
{
    WebInspector.View.call(this);

    this.domStorage = domStorage;

    this.element.addStyleClass("storage-view");
    this.element.addStyleClass("table");

    this.deleteButton = document.createElement("button");
    this.deleteButton.title = WebInspector.UIString("Delete");
    this.deleteButton.className = "delete-storage-status-bar-item status-bar-item hidden";
    this.deleteButton.addEventListener("click", this._deleteButtonClicked.bind(this), false);

    this.refreshButton = document.createElement("button");
    this.refreshButton.title = WebInspector.UIString("Refresh");
    this.refreshButton.className = "refresh-storage-status-bar-item status-bar-item";
    this.refreshButton.addEventListener("click", this._refreshButtonClicked.bind(this), false);
}

WebInspector.DOMStorageItemsView.prototype = {
    get statusBarItems()
    {
        return [this.refreshButton, this.deleteButton];
    },

    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.update();
    },

    hide: function()
    {
        WebInspector.View.prototype.hide.call(this);
        this.deleteButton.addStyleClass("hidden");
    },

    update: function()
    {
        this.element.removeChildren();
        var hasDOMStorage = this.domStorage;
        if (hasDOMStorage)
            hasDOMStorage = this.domStorage.domStorage;

        if (hasDOMStorage) {
            var dataGrid = WebInspector.panels.databases.dataGridForDOMStorage(this.domStorage.domStorage);
            if (!dataGrid)
                hasDOMStorage = 0;
            else {
                this._dataGrid = dataGrid;
                this.element.appendChild(dataGrid.element);
                this.deleteButton.removeStyleClass("hidden");
            }
        }

        if (!hasDOMStorage) {
            var emptyMsgElement = document.createElement("div");
            emptyMsgElement.className = "storage-table-empty";
            if (this.domStorage)
            emptyMsgElement.textContent = WebInspector.UIString("This storage is empty.");
            this.element.appendChild(emptyMsgElement);
            this._dataGrid = null;
            this.deleteButton.addStyleClass("hidden");
        }
    },

    _deleteButtonClicked: function(event)
    {
        if (this._dataGrid) {
            this._dataGrid.deleteSelectedRow();
            
            this.show();
        }
    },

    _refreshButtonClicked: function(event)
    {
        this.update();
    }
}

WebInspector.DOMStorageItemsView.prototype.__proto__ = WebInspector.View.prototype;
