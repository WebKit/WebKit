/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

WebInspector.ApplicationCacheItemsView = function(treeElement, appcacheDomain)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("storage-view");
    this.element.addStyleClass("table");

    // FIXME: Delete Button semantics are not yet defined.
    // FIXME: Needs better tooltip. (Localized)
    this.deleteButton = new WebInspector.StatusBarButton(WebInspector.UIString("Delete"), "delete-storage-status-bar-item");
    this.deleteButton.visible = false;
    this.deleteButton.addEventListener("click", this._deleteButtonClicked.bind(this), false);

    // FIXME: Refresh Button semantics are not yet defined.
    // FIXME: Needs better tooltip. (Localized)
    this.refreshButton = new WebInspector.StatusBarButton(WebInspector.UIString("Refresh"), "refresh-storage-status-bar-item");
    this.refreshButton.addEventListener("click", this._refreshButtonClicked.bind(this), false);

    this.connectivityIcon = document.createElement("img");
    this.connectivityIcon.className = "storage-application-cache-connectivity-icon";
    this.connectivityIcon.src = "";
    this.connectivityMessage = document.createElement("span");
    this.connectivityMessage.className = "storage-application-cache-connectivity";
    this.connectivityMessage.textContent = "";

    this.divider = document.createElement("span");
    this.divider.className = "status-bar-item status-bar-divider";

    this.statusIcon = document.createElement("img");
    this.statusIcon.className = "storage-application-cache-status-icon";
    this.statusIcon.src = "";
    this.statusMessage = document.createElement("span");
    this.statusMessage.className = "storage-application-cache-status";
    this.statusMessage.textContent = "";

    this._treeElement = treeElement;
    this._appcacheDomain = appcacheDomain;

    this._emptyMsgElement = document.createElement("div");
    this._emptyMsgElement.className = "storage-table-empty";
    this._emptyMsgElement.textContent = WebInspector.UIString("No Application Cache information available.");
    this.element.appendChild(this._emptyMsgElement);

    this.updateStatus(applicationCache.UNCACHED);
}

WebInspector.ApplicationCacheItemsView.prototype = {
    get statusBarItems()
    {
        return [
            this.refreshButton.element, this.deleteButton.element,
            this.connectivityIcon, this.connectivityMessage, this.divider,
            this.statusIcon, this.statusMessage
        ];
    },

    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.updateNetworkState(navigator.onLine);
        this._update();
    },

    hide: function()
    {
        WebInspector.View.prototype.hide.call(this);
        this.deleteButton.visible = false;
    },

    updateStatus: function(status)
    {
        var statusInformation = {};
        statusInformation[applicationCache.UNCACHED]    = { src: "Images/warningOrangeDot.png", text: "UNCACHED"    };
        statusInformation[applicationCache.IDLE]        = { src: "Images/warningOrangeDot.png", text: "IDLE"        };
        statusInformation[applicationCache.CHECKING]    = { src: "Images/successGreenDot.png",  text: "CHECKING"    };
        statusInformation[applicationCache.DOWNLOADING] = { src: "Images/successGreenDot.png",  text: "DOWNLOADING" };
        statusInformation[applicationCache.UPDATEREADY] = { src: "Images/successGreenDot.png",  text: "UPDATEREADY" };
        statusInformation[applicationCache.OBSOLETE]    = { src: "Images/errorRedDot.png",      text: "OBSOLETE"    };

        var info = statusInformation[status];
        if (!info) {
            console.error("Unknown Application Cache Status was Not Handled: %d", status);
            return;
        }

        this.statusIcon.src = info.src;
        this.statusMessage.textContent = info.text;
    },

    updateNetworkState: function(isNowOnline)
    {
        if (isNowOnline) {
            this.connectivityIcon.src = "Images/successGreenDot.png";
            this.connectivityMessage.textContent = WebInspector.UIString("Online");
        } else {
            this.connectivityIcon.src = "Images/errorRedDot.png";
            this.connectivityMessage.textContent = WebInspector.UIString("Offline");
        }
    },

    _update: function()
    {
        // FIXME: Implement in next part
    },

    _updateCallback: function(appcaches, creationTime, updateTime, size, resources)
    {
        // FIXME: Implement in next part
    },

    _createDataGrid: function()
    {
        // FIXME: Implement in next part
    },

    _populateDataGrid: function()
    {
        // FIXME: Implement in next part
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

        // FIXME: Delete Button semantics are not yet defined. (Delete a single, or all?)
        this._deleteCallback(this._dataGrid.selectedNode);
    },

    _deleteCallback: function(node)
    {
        // FIXME: Should we delete a single (selected) resource or all resources?
        // InspectorBackend.deleteCachedResource(...)
        // this._update();
    },

    _refreshButtonClicked: function(event)
    {
        // FIXME: Is this a refresh button or a re-fetch manifest button?
        // this._update();
    }
}

WebInspector.ApplicationCacheItemsView.prototype.__proto__ = WebInspector.View.prototype;
