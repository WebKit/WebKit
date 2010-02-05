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

WebInspector.CookieItemsView = function(treeElement, cookieDomain)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("storage-view");
    this.element.addStyleClass("table");

    this.deleteButton = new WebInspector.StatusBarButton(WebInspector.UIString("Delete"), "delete-storage-status-bar-item");
    this.deleteButton.visible = false;
    this.deleteButton.addEventListener("click", this._deleteButtonClicked.bind(this), false);

    this.refreshButton = new WebInspector.StatusBarButton(WebInspector.UIString("Refresh"), "refresh-storage-status-bar-item");
    this.refreshButton.addEventListener("click", this._refreshButtonClicked.bind(this), false);
    
    this._treeElement = treeElement;
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
        WebInspector.Cookies.getCookiesAsync(this._updateWithCookies.bind(this));
    },

    _updateWithCookies: function(allCookies, isAdvanced)
    {
        var cookies = this._cookiesForDomain(allCookies, isAdvanced);
        var dataGrid = (isAdvanced ? this.dataGridForCookies(cookies) : this.simpleDataGridForCookies(cookies));
        if (dataGrid) {
            this._dataGrid = dataGrid;
            this.element.appendChild(dataGrid.element);
            this._dataGrid.updateWidths();
            if (isAdvanced)
                this.deleteButton.visible = true;
        } else {
            var emptyMsgElement = document.createElement("div");
            emptyMsgElement.className = "storage-table-empty";
            emptyMsgElement.textContent = WebInspector.UIString("This site has no cookies.");
            this.element.appendChild(emptyMsgElement);
            this._dataGrid = null;
            this.deleteButton.visible = false;
        }
    },

    _cookiesForDomain: function(allCookies, isAdvanced)
    {
        var cookiesForDomain = [];
        var resourceURLsForDocumentURL = [];
        var totalSize = 0;

        for (var id in WebInspector.resources) {
            var resource = WebInspector.resources[id];
            var match = resource.documentURL.match(WebInspector.URLRegExp);
            if (match && match[2] === this._cookieDomain)
                resourceURLsForDocumentURL.push(resource.url);
        }

        for (var i = 0; i < allCookies.length; ++i) {
            var pushed = false;
            var size = allCookies[i].size;
            for (var j = 0; j < resourceURLsForDocumentURL.length; ++j) {
                var resourceURL = resourceURLsForDocumentURL[j];
                if (WebInspector.Cookies.cookieMatchesResourceURL(allCookies[i], resourceURL)) {
                    totalSize += size;
                    if (!pushed) {
                        pushed = true;
                        cookiesForDomain.push(allCookies[i]);
                    }
                }
            }
        }

        if (isAdvanced) {
            this._treeElement.subtitle = String.sprintf(WebInspector.UIString("%d cookies (%s)"), cookiesForDomain.length,
                Number.bytesToString(totalSize, WebInspector.UIString));
        }
        return cookiesForDomain;
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
        columns[0].sortable = true;
        columns[1].title = WebInspector.UIString("Value");
        columns[1].width = columns[1].title.length;
        columns[1].sortable = true;
        columns[2].title = WebInspector.UIString("Domain");
        columns[2].width = columns[2].title.length;
        columns[2].sortable = true;
        columns[3].title = WebInspector.UIString("Path");
        columns[3].width = columns[3].title.length;
        columns[3].sortable = true;
        columns[4].title = WebInspector.UIString("Expires");
        columns[4].width = columns[4].title.length;
        columns[4].sortable = true;
        columns[5].title = WebInspector.UIString("Size");
        columns[5].width = columns[5].title.length;
        columns[5].aligned = "right";
        columns[5].sortable = true;
        columns[6].title = WebInspector.UIString("HTTP");
        columns[6].width = columns[6].title.length;
        columns[6].aligned = "centered";
        columns[6].sortable = true;
        columns[7].title = WebInspector.UIString("Secure");
        columns[7].width = columns[7].title.length;
        columns[7].aligned = "centered";
        columns[7].sortable = true;

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

        var dataGrid = new WebInspector.DataGrid(columns, null, this._deleteCookieCallback.bind(this));
        var nodes = this._createNodes(dataGrid, cookies);
        for (var i = 0; i < nodes.length; ++i)
            dataGrid.appendChild(nodes[i]);
        if (nodes.length)
            nodes[0].selected = true;
        dataGrid.addEventListener("sorting changed", this._sortData.bind(this, dataGrid, cookies));

        return dataGrid;
    },

    _createNodes: function(dataGrid, cookies)
    {
        function updateDataAndColumn(data, index, value) {
            data[index] = value;
            if (value.length > dataGrid.columns[index].width)
                dataGrid.columns[index].width = value.length;
        }

        var nodes = [];
        for (var i = 0; i < cookies.length; ++i) {
            var data = {};
            var cookie = cookies[i];

            updateDataAndColumn(data, 0, cookie.name);
            updateDataAndColumn(data, 1, cookie.value);
            updateDataAndColumn(data, 2, cookie.domain);
            updateDataAndColumn(data, 3, cookie.path);
            updateDataAndColumn(data, 4, (cookie.session ? WebInspector.UIString("Session") : cookie.expires.toGMTString()));
            updateDataAndColumn(data, 5, Number.bytesToString(cookie.size, WebInspector.UIString));
            updateDataAndColumn(data, 6, (cookie.httpOnly ? "\u2713" : "")); // Checkmark
            updateDataAndColumn(data, 7, (cookie.secure ? "\u2713" : "")); // Checkmark

            var node = new WebInspector.DataGridNode(data);
            node.cookie = cookie;
            node.selectable = true;
            nodes.push(node);
        }
        return nodes;
    },

    _sortData: function(dataGrid, cookies)
    {
        var sortDirection = dataGrid.sortOrder === "ascending" ? 1 : -1;

        function localeCompare(field, cookie1, cookie2)
        {
            return sortDirection * (cookie1[field] + "").localeCompare(cookie2[field] + "")
        }

        function numberCompare(field, cookie1, cookie2)
        {
            return sortDirection * (cookie1[field] - cookie2[field]);
        }

        function expiresCompare(cookie1, cookie2)
        {
            if (cookie1.session !== cookie2.session)
                return sortDirection * (cookie1.session ? 1 : -1);

            if (cookie1.session)
                return 0;

            return sortDirection * (cookie1.expires.getTime() - cookie2.expires.getTime());
        }

        var comparator;
        switch (parseInt(dataGrid.sortColumnIdentifier)) {
            case 0: comparator = localeCompare.bind(this, "name"); break;
            case 1: comparator = localeCompare.bind(this, "value"); break;
            case 2: comparator = localeCompare.bind(this, "domain"); break;
            case 3: comparator = localeCompare.bind(this, "path"); break;
            case 4: comparator = expiresCompare; break;
            case 5: comparator = numberCompare.bind(this, "size"); break;
            case 6: comparator = localeCompare.bind(this, "httpOnly"); break;
            case 7: comparator = localeCompare.bind(this, "secure"); break;
            default: localeCompare.bind(this, "name");
        }

        cookies.sort(comparator);
        var nodes = this._createNodes(dataGrid, cookies);

        dataGrid.removeChildren();
        for (var i = 0; i < nodes.length; ++i)
            dataGrid.appendChild(nodes[i]);

        if (nodes.length)
            nodes[0].selected = true;
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
        if (!this._dataGrid || !this._dataGrid.selectedNode)
            return;

        this._deleteCookieCallback(this._dataGrid.selectedNode);
    },
    
    _deleteCookieCallback: function(node)
    {
        var cookie = node.cookie;
        InspectorBackend.deleteCookie(cookie.name, this._cookieDomain);
        this.update();
    },

    _refreshButtonClicked: function(event)
    {
        this.update();
    }
}

WebInspector.CookieItemsView.prototype.__proto__ = WebInspector.View.prototype;
