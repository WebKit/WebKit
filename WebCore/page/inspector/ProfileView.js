/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

WebInspector.ProfileView = function(profile)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("profile-view");

    this.profile = profile;

    this.showSelfTimeAsPercent = true;
    this.showTotalTimeAsPercent = true;

    var columns = { "self": { title: WebInspector.UIString("Self"), width: "72px", sortable: true },
                    "total": { title: WebInspector.UIString("Total"), width: "72px", sort: "descending", sortable: true },
                    "calls": { title: WebInspector.UIString("Calls"), width: "54px", sortable: true },
                    "function": { title: WebInspector.UIString("Function"), disclosure: true, sortable: true } };

    this.dataGrid = new WebInspector.DataGrid(columns);
    this.dataGrid.addEventListener("sorting changed", this._sortData, this);
    this.dataGrid.element.addEventListener("mousedown", this._mouseDownInDataGrid.bind(this), true);
    this.element.appendChild(this.dataGrid.element);

    this.percentButton = document.createElement("button");
    this.percentButton.className = "percent-time-status-bar-item status-bar-item";
    this.percentButton.addEventListener("click", this._percentClicked.bind(this), false);

    this.focusButton = document.createElement("button");
    this.focusButton.title = WebInspector.UIString("Focus selected function.");
    this.focusButton.className = "focus-profile-node-status-bar-item status-bar-item";
    this.focusButton.disabled = true;
    this.focusButton.addEventListener("click", this._focusClicked.bind(this), false);

    this.excludeButton = document.createElement("button");
    this.excludeButton.title = WebInspector.UIString("Exclude selected function.");
    this.excludeButton.className = "exclude-profile-node-status-bar-item status-bar-item";
    this.excludeButton.disabled = true;
    this.excludeButton.addEventListener("click", this._excludeClicked.bind(this), false);

    this.resetButton = document.createElement("button");
    this.resetButton.title = WebInspector.UIString("Restore all functions.");
    this.resetButton.className = "reset-profile-status-bar-item status-bar-item hidden";
    this.resetButton.addEventListener("click", this._resetClicked.bind(this), false);

    // By default the profile isn't sorted, so sort based on our default sort
    // column and direction padded to the DataGrid above.
    profile.sortTotalTimeDescending();

    this._updatePercentButton();

    this.refresh();
}

WebInspector.ProfileView.prototype = {
    get statusBarItems()
    {
        return [this.percentButton, this.focusButton, this.excludeButton, this.resetButton];
    },

    refresh: function()
    {
        var selectedProfileNode = this.dataGrid.selectedNode ? this.dataGrid.selectedNode.profileNode : null;

        this.dataGrid.removeChildren();

        var children = this.profile.head.children;
        var childrenLength = children.length;
        for (var i = 0; i < childrenLength; ++i)
            if (children[i].visible)
                this.dataGrid.appendChild(new WebInspector.ProfileDataGridNode(this, children[i]));

        if (selectedProfileNode && selectedProfileNode._dataGridNode)
            selectedProfileNode._dataGridNode.selected = true;
    },

    refreshShowAsPercents: function()
    {
        this._updatePercentButton();

        var child = this.dataGrid.children[0];
        while (child) {
            child.refresh();
            child = child.traverseNextNode(false, null, true);
        }
    },

    _percentClicked: function(event)
    {
        var currentState = this.showSelfTimeAsPercent && this.showTotalTimeAsPercent;
        this.showSelfTimeAsPercent = !currentState;
        this.showTotalTimeAsPercent = !currentState;
        this.refreshShowAsPercents();
    },

    _updatePercentButton: function()
    {
        if (this.showSelfTimeAsPercent && this.showTotalTimeAsPercent) {
            this.percentButton.title = WebInspector.UIString("Show absolute total and self times.");
            this.percentButton.addStyleClass("toggled-on");
        } else {
            this.percentButton.title = WebInspector.UIString("Show total and self times as percentages.");
            this.percentButton.removeStyleClass("toggled-on");
        }
    },

    _focusClicked: function(event)
    {
        if (!this.dataGrid.selectedNode || !this.dataGrid.selectedNode.profileNode)
            return;
        this.resetButton.removeStyleClass("hidden");
        this.profile.focus(this.dataGrid.selectedNode.profileNode);
        this.refresh();
    },

    _excludeClicked: function(event)
    {
        if (!this.dataGrid.selectedNode || !this.dataGrid.selectedNode.profileNode)
            return;
        this.resetButton.removeStyleClass("hidden");
        this.profile.exclude(this.dataGrid.selectedNode.profileNode);
        this.dataGrid.selectedNode.deselect();
        this.refresh();
    },

    _resetClicked: function(event)
    {
        this.resetButton.addStyleClass("hidden");
        this.profile.restoreAll();
        this.refresh();
    },

    _dataGridNodeSelected: function(node)
    {
        this.focusButton.disabled = false;
        this.excludeButton.disabled = false;
    },

    _dataGridNodeDeselected: function(node)
    {
        this.focusButton.disabled = true;
        this.excludeButton.disabled = true;
    },

    _sortData: function(event)
    {
        var sortOrder = this.dataGrid.sortOrder;
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;

        var sortingFunctionName = "sort";

        if (sortColumnIdentifier === "self")
            sortingFunctionName += "SelfTime";
        else if (sortColumnIdentifier === "total")
            sortingFunctionName += "TotalTime";
        else if (sortColumnIdentifier === "calls")
            sortingFunctionName += "Calls";
        else if (sortColumnIdentifier === "function")
            sortingFunctionName += "FunctionName";

        if (sortOrder === "ascending")
            sortingFunctionName += "Ascending";
        else
            sortingFunctionName += "Descending";

        if (!(sortingFunctionName in this.profile))
            return;

        this.profile[sortingFunctionName]();

        this.refresh();
    },

    _mouseDownInDataGrid: function(event)
    {
        if (event.detail < 2)
            return;

        var cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell || (!cell.hasStyleClass("total-column") && !cell.hasStyleClass("self-column")))
            return;

        if (cell.hasStyleClass("total-column"))
            this.showTotalTimeAsPercent = !this.showTotalTimeAsPercent;
        else if (cell.hasStyleClass("self-column"))
            this.showSelfTimeAsPercent = !this.showSelfTimeAsPercent;

        this.refreshShowAsPercents();

        event.preventDefault();
        event.stopPropagation();
    }
}

WebInspector.ProfileView.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.ProfileDataGridNode = function(profileView, profileNode)
{
    this.profileView = profileView;

    this.profileNode = profileNode;
    profileNode._dataGridNode = this;

    // Find the first child that is visible. Since we don't want to claim
    // we have children if all the children are invisible.
    var hasChildren = false;
    var children = this.profileNode.children;
    var childrenLength = children.length;
    for (var i = 0; i < childrenLength; ++i) {
        if (children[i].visible) {
            hasChildren = true;
            break;
        }
    }

    WebInspector.DataGridNode.call(this, null, hasChildren);

    this.addEventListener("populate", this._populate, this);

    this.expanded = profileNode._expanded;
}

WebInspector.ProfileDataGridNode.prototype = {
    get data()
    {
        function formatMilliseconds(time)
        {
            return Number.secondsToString(time / 1000, WebInspector.UIString.bind(WebInspector), true);
        }

        var data = {};
        data["function"] = this.profileNode.functionName;
        data["calls"] = this.profileNode.numberOfCalls;

        if (this.profileView.showSelfTimeAsPercent)
            data["self"] = WebInspector.UIString("%.2f%%", this.profileNode.selfPercent);
        else
            data["self"] = formatMilliseconds(this.profileNode.selfTime);

        if (this.profileView.showTotalTimeAsPercent)
            data["total"] = WebInspector.UIString("%.2f%%", this.profileNode.totalPercent);
        else
            data["total"] = formatMilliseconds(this.profileNode.totalTime);

        return data;
    },

    createCell: function(columnIdentifier)
    {
        var cell = WebInspector.DataGridNode.prototype.createCell.call(this, columnIdentifier);
        if (columnIdentifier !== "function")
            return cell;

        if (this.profileNode.url) {
            var fileName = WebInspector.displayNameForURL(this.profileNode.url);

            var urlElement = document.createElement("a");
            urlElement.className = "profile-node-file webkit-html-resource-link";
            urlElement.href = this.profileNode.url;
            urlElement.lineNumber = this.profileNode.lineNumber;

            if (this.profileNode.lineNumber > 0)
                urlElement.textContent = fileName + ":" + this.profileNode.lineNumber;
            else
                urlElement.textContent = fileName;

            cell.insertBefore(urlElement, cell.firstChild);
        }

        return cell;
    },

    select: function(supressSelectedEvent)
    {
        WebInspector.DataGridNode.prototype.select.call(this, supressSelectedEvent);
        this.profileView._dataGridNodeSelected(this);
    },

    deselect: function(supressDeselectedEvent)
    {
        WebInspector.DataGridNode.prototype.deselect.call(this, supressDeselectedEvent);
        this.profileView._dataGridNodeDeselected(this);
    },

    expand: function()
    {
        WebInspector.DataGridNode.prototype.expand.call(this);
        this.profileNode._expanded = true;
    },

    collapse: function()
    {
        WebInspector.DataGridNode.prototype.collapse.call(this);
        this.profileNode._expanded = false;
    },

    _populate: function(event)
    {
        var children = this.profileNode.children;
        var childrenLength = children.length;
        for (var i = 0; i < childrenLength; ++i)
            if (children[i].visible)
                this.appendChild(new WebInspector.ProfileDataGridNode(this.profileView, children[i]));
        this.removeEventListener("populate", this._populate, this);
    }
}

WebInspector.ProfileDataGridNode.prototype.__proto__ = WebInspector.DataGridNode.prototype;
