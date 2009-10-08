/*
 * Copyright (C) 2009 Apple Inc.  All rights reserved.
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

WebInspector.CookieItemsView = function(cookieDomain)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("storage-view");
    this.element.addStyleClass("table");

    this.deleteButton = new WebInspector.StatusBarButton(WebInspector.UIString("Delete"), "delete-storage-status-bar-item");
    this.deleteButton.visible = false;
    this.deleteButton.addEventListener("click", this._deleteButtonClicked.bind(this), false);

    this.refreshButton = new WebInspector.StatusBarButton(WebInspector.UIString("Refresh"), "refresh-storage-status-bar-item");
    this.refreshButton.addEventListener("click", this._refreshButtonClicked.bind(this), false);
    
    this._cookieDomain = cookieDomain;
}

WebInspector.CookieItemsView.prototype = {
    get statusBarItems()
    {
        return [this.refreshButton.element, this.deleteButton.element];
    },

    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.update();
    },

    hide: function()
    {
        WebInspector.View.prototype.hide.call(this);
        this.deleteButton.visible = false;
    },

    update: function()
    {
        this.element.removeChildren();

        var self = this;
        function callback(cookies, isAdvanced) {
            var dataGrid = (isAdvanced ? self.dataGridForCookies(cookies) : self.simpleDataGridForCookies(cookies));
            if (dataGrid) {
                self._dataGrid = dataGrid;
                self.element.appendChild(dataGrid.element);
                self._dataGrid.updateWidths();
                if (isAdvanced)
                    self.deleteButton.visible = true;
            } else {
                var emptyMsgElement = document.createElement("div");
                emptyMsgElement.className = "storage-table-empty";
                emptyMsgElement.textContent = WebInspector.UIString("This site has no cookies.");
                self.element.appendChild(emptyMsgElement);
                self._dataGrid = null;
                self.deleteButton.visible = false;
            }
        }

        WebInspector.Cookies.getCookiesAsync(callback, this._cookieDomain);
    },

    dataGridForCookies: function(cookies)
    {
        if (!cookies.length)
            return null;

        for (var i = 0; i < cookies.length; ++i)
            cookies[i].expires = new Date(cookies[i].expires);

        var columns = { 0: {}, 1: {}, 2: {}, 3: {}, 4: {}, 5: {}, 6: {}, 7: {} };
        columns[0].title = WebInspector.UIString("Name");
        columns[0].width = columns[0].title.length;
        columns[1].title = WebInspector.UIString("Value");
        columns[1].width = columns[1].title.length;
        columns[2].title = WebInspector.UIString("Domain");
        columns[2].width = columns[2].title.length;
        columns[3].title = WebInspector.UIString("Path");
        columns[3].width = columns[3].title.length;
        columns[4].title = WebInspector.UIString("Expires");
        columns[4].width = columns[4].title.length;
        columns[5].title = WebInspector.UIString("Size");
        columns[5].width = columns[5].title.length;
        columns[5].aligned = "right";
        columns[6].title = WebInspector.UIString("HTTP");
        columns[6].width = columns[6].title.length;
        columns[6].aligned = "centered";
        columns[7].title = WebInspector.UIString("Secure");
        columns[7].width = columns[7].title.length;
        columns[7].aligned = "centered";

        function updateDataAndColumn(index, value) {
            data[index] = value;
            if (value.length > columns[index].width)
                columns[index].width = value.length;
        }

        var data;
        var nodes = [];
        for (var i = 0; i < cookies.length; ++i) {
            var cookie = cookies[i];
            data = {};

            updateDataAndColumn(0, cookie.name);
            updateDataAndColumn(1, cookie.value);
            updateDataAndColumn(2, cookie.domain);
            updateDataAndColumn(3, cookie.path);
            updateDataAndColumn(4, (cookie.session ? WebInspector.UIString("Session") : cookie.expires.toGMTString()));
            updateDataAndColumn(5, Number.bytesToString(cookie.size, WebInspector.UIString));
            updateDataAndColumn(6, (cookie.httpOnly ? "\u2713" : "")); // Checkmark
            updateDataAndColumn(7, (cookie.secure ? "\u2713" : "")); // Checkmark

            var node = new WebInspector.DataGridNode(data, false);
            node.cookie = cookie;
            node.selectable = true;
            nodes.push(node);
        }

        var totalColumnWidths = 0;
        for (var columnIdentifier in columns)
            totalColumnWidths += columns[columnIdentifier].width;

        // Enforce the Value column (the 2nd column) to be a max of 33%
        // tweaking the raw total width because may massively outshadow the others
        var valueColumnWidth = columns[1].width;
        if (valueColumnWidth / totalColumnWidths > 0.33) {
            totalColumnWidths -= valueColumnWidth;
            totalColumnWidths *= 1.33;
            columns[1].width = totalColumnWidths * 0.33;
        }

        // Calculate the percentage width for the columns.
        const minimumPrecent = 6;
        var recoupPercent = 0;
        for (var columnIdentifier in columns) {
            var width = columns[columnIdentifier].width;
            width = Math.round((width / totalColumnWidths) * 100);
            if (width < minimumPrecent) {
                recoupPercent += (minimumPrecent - width);
                width = minimumPrecent;
            }
            columns[columnIdentifier].width = width;
        }

        // Enforce the minimum percentage width. (need to narrow total percentage due to earlier additions)
        while (recoupPercent > 0) {
            for (var columnIdentifier in columns) {
                if (columns[columnIdentifier].width > minimumPrecent) {
                    --columns[columnIdentifier].width;
                    --recoupPercent;
                    if (!recoupPercent)
                        break;
                }
            }
        }

        for (var columnIdentifier in columns)
            columns[columnIdentifier].width += "%";

        var dataGrid = new WebInspector.DataGrid(columns);
        var length = nodes.length;
        for (var i = 0; i < length; ++i)
            dataGrid.appendChild(nodes[i]);
        if (length > 0)
            nodes[0].selected = true;

        return dataGrid;
    },

    simpleDataGridForCookies: function(cookies)
    {
        if (!cookies.length)
            return null;

        var columns = {};
        columns[0] = {};
        columns[1] = {};
        columns[0].title = WebInspector.UIString("Name");
        columns[0].width = columns[0].title.length;
        columns[1].title = WebInspector.UIString("Value");
        columns[1].width = columns[1].title.length;

        var nodes = [];
        for (var i = 0; i < cookies.length; ++i) {
            var cookie = cookies[i];
            var data = {};

            var name = cookie.name;
            data[0] = name;
            if (name.length > columns[0].width)
                columns[0].width = name.length;

            var value = cookie.value;
            data[1] = value;
            if (value.length > columns[1].width)
                columns[1].width = value.length;

            var node = new WebInspector.DataGridNode(data, false);
            node.selectable = true;
            nodes.push(node);
        }

        var totalColumnWidths = columns[0].width + columns[1].width;
        var width = Math.round((columns[0].width * 100) / totalColumnWidths);
        const minimumPrecent = 20;
        if (width < minimumPrecent)
            width = minimumPrecent;
        if (width > 100 - minimumPrecent)
            width = 100 - minimumPrecent;
        columns[0].width = width;
        columns[1].width = 100 - width;
        columns[0].width += "%";
        columns[1].width += "%";

        var dataGrid = new WebInspector.DataGrid(columns);
        var length = nodes.length;
        for (var i = 0; i < length; ++i)
            dataGrid.appendChild(nodes[i]);
        if (length > 0)
            nodes[0].selected = true;

        return dataGrid;
    },
    
    resize: function()
    {
        if (this._dataGrid)
            this._dataGrid.updateWidths();
    },

    _deleteButtonClicked: function(event)
    {
        if (!this._dataGrid)
            return;

        var cookie = this._dataGrid.selectedNode.cookie;
        InspectorController.deleteCookie(cookie.name, this._cookieDomain);
        this.update();
    },

    _refreshButtonClicked: function(event)
    {
        this.update();
    }
}

WebInspector.CookieItemsView.prototype.__proto__ = WebInspector.View.prototype;
