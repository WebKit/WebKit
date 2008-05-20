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

    // By default the profile isn't sorted, so sort based on our default sort
    // column and direction padded to the DataGrid above.
    profile.head.sortTotalTimeDescending();

    this.refresh();
}

WebInspector.ProfileView.prototype = {
    refresh: function()
    {
        var selectedProfileNode = this.dataGrid.selectedNode ? this.dataGrid.selectedNode.profileNode : null;

        this.dataGrid.removeChildren();

        var children = this.profile.head.children;
        var childrenLength = children.length;
        for (var i = 0; i < childrenLength; ++i)
            this.dataGrid.appendChild(new WebInspector.ProfileDataGridNode(children[i], this.showSelfTimeAsPercent, this.showTotalTimeAsPercent));

        if (selectedProfileNode && selectedProfileNode._dataGridNode)
            selectedProfileNode._dataGridNode.selected = true;
    },

    refreshShowAsPercents: function()
    {
        var child = this.dataGrid.children[0];
        while (child) {
            child.showTotalTimeAsPercent = this.showTotalTimeAsPercent;
            child.showSelfTimeAsPercent = this.showSelfTimeAsPercent;
            child.refresh();
            child = child.traverseNextNode(false, null, true);
        }
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

        if (!(sortingFunctionName in this.profile.head))
            return;

        this.profile.head[sortingFunctionName]();

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

WebInspector.ProfileDataGridNode = function(profileNode, showSelfTimeAsPercent, showTotalTimeAsPercent)
{
    this.profileNode = profileNode;
    profileNode._dataGridNode = this;

    this.showSelfTimeAsPercent = showSelfTimeAsPercent;
    this.showTotalTimeAsPercent = showTotalTimeAsPercent;

    var hasChildren = (profileNode.children.length ? true : false);
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

        if (this.showSelfTimeAsPercent)
            data["self"] = WebInspector.UIString("%.2f%%", this.profileNode.selfPercent);
        else
            data["self"] = formatMilliseconds(this.profileNode.selfTime);

        if (this.showTotalTimeAsPercent)
            data["total"] = WebInspector.UIString("%.2f%%", this.profileNode.totalPercent);
        else
            data["total"] = formatMilliseconds(this.profileNode.totalTime);

        return data;
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
            this.appendChild(new WebInspector.ProfileDataGridNode(children[i], this.showSelfTimeAsPercent, this.showTotalTimeAsPercent));
        this.removeEventListener("populate", this._populate, this);
    }
}

WebInspector.ProfileDataGridNode.prototype.__proto__ = WebInspector.DataGridNode.prototype;
