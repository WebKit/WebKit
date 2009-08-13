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

WebInspector.CookieItemsView = function()
{
    WebInspector.View.call(this);

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

WebInspector.CookieItemsView.prototype = {
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
        var dataGrid = this.dataGridForCookies();
        if (dataGrid) {
            this._dataGrid = dataGrid;
            this.element.appendChild(dataGrid.element);
            this.deleteButton.removeStyleClass("hidden");
        } else {
            var emptyMsgElement = document.createElement("div");
            emptyMsgElement.className = "storage-table-empty";
            emptyMsgElement.textContent = WebInspector.UIString("This site has no cookies.");
            this.element.appendChild(emptyMsgElement);
            this._dataGrid = null;
            this.deleteButton.addStyleClass("hidden");
        }
    },

    buildCookies: function()
    {
        var rawCookieString = InspectorController.inspectedWindow().document.cookie;
        var rawCookies = rawCookieString.split(/;\s*/);
        var cookies = [];

        if (!(/^\s*$/.test(rawCookieString))) {
            for (var i = 0; i < rawCookies.length; ++i) {
                var cookie = rawCookies[i];
                var delimIndex = cookie.indexOf("=");
                var name = cookie.substring(0, delimIndex);
                var value = cookie.substring(delimIndex + 1);
                cookies.push(new WebInspector.Cookie(name, value));
            }                
        }

        return cookies;
    },

    dataGridForCookies: function()
    {
        var cookies = this.buildCookies();
        if (!cookies.length)
            return null;

        var columns = {};
        columns[0] = {};
        columns[1] = {};
        columns[0].title = WebInspector.UIString("Key");
        columns[0].width = columns[0].title.length;
        columns[1].title = WebInspector.UIString("Value");
        columns[1].width = columns[0].title.length;

        var nodes = [];
        for (var i = 0; i < cookies.length; ++i) {
            var cookie = cookies[i];
            var data = {};

            var key = cookie.key;
            data[0] = key;
            if (key.length > columns[0].width)
                columns[0].width = key.length;

            var value = cookie.value;
            data[1] = value;
            if (value.length > columns[1].width)
                columns[1].width = value.length;

            var node = new WebInspector.DataGridNode(data, false);
            node.selectable = true;
            nodes.push(node);
      }

      var totalColumnWidths = columns[0].width + columns[1].width;
      width = Math.round((columns[0].width * 100) / totalColumnWidths);
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

    _deleteButtonClicked: function(event)
    {
        if (!this._dataGrid)
            return;

        var node = this._dataGrid.selectedNode;
        var key = node.data[0].replace(/"/,"\\\"");
        var expire = "Thu, 01 Jan 1970 00:00:00 GMT"; // (new Date(0)).toGMTString();
        var evalStr = "document.cookie = \""+ key + "=; expires=" + expire + "; path=/\";" +
                      "document.cookie = \""+ key + "=; expires=" + expire + "\";";
        WebInspector.console.doEvalInWindow(evalStr, this.update.bind(this));
    },

    _refreshButtonClicked: function(event)
    {
        this.update();
    }
}

WebInspector.CookieItemsView.prototype.__proto__ = WebInspector.View.prototype;
