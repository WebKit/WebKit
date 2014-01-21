/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.TreeOutlineDataGridSynchronizer = function(treeOutline, dataGrid)
{
    WebInspector.Object.call(this);

    this._treeOutline = treeOutline;
    this._dataGrid = dataGrid;

    this._treeOutline.element.parentNode.addEventListener("scroll", this._treeOutlineScrolled.bind(this));
    this._dataGrid.scrollContainer.addEventListener("scroll", this._dataGridScrolled.bind(this));

    this._treeOutline.__dataGridNode = this._dataGrid;

    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.ExpandedNode, this._dataGridNodeExpanded, this);
    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.CollapsedNode, this._dataGridNodeCollapsed, this);
    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);

    // FIXME: This is a hack. TreeOutline should just dispatch events via WebInspector.Object.
    var existingOnAdd = treeOutline.onadd;
    var existingOnRemove = treeOutline.onremove;
    var existingOnExpand = treeOutline.onexpand;
    var existingOnCollapse = treeOutline.oncollapse;
    var existingOnHidden = treeOutline.onhidden;
    var existingOnSelect = treeOutline.onselect;

    treeOutline.onadd = function(element) {
        this._treeElementAdded(element);
        if (existingOnAdd)
            existingOnAdd.call(treeOutline, element);
    }.bind(this);

    treeOutline.onremove = function(element) {
        this._treeElementRemoved(element);
        if (existingOnRemove)
            existingOnRemove.call(treeOutline, element);
    }.bind(this);

    treeOutline.onexpand = function(element) {
        this._treeElementExpanded(element);
        if (existingOnExpand)
            existingOnExpand.call(treeOutline, element);
    }.bind(this);

    treeOutline.oncollapse = function(element) {
        this._treeElementCollapsed(element);
        if (existingOnCollapse)
            existingOnCollapse.call(treeOutline, element);
    }.bind(this);

    treeOutline.onhidden = function(element, hidden) {
        this._treeElementHiddenChanged(element, hidden);
        if (existingOnHidden)
            existingOnHidden.call(treeOutline, element, hidden);
    }.bind(this);

    treeOutline.onselect = function(element, selectedByUser) {
        this._treeElementSelected(element, selectedByUser);
        if (existingOnSelect)
            existingOnSelect.call(treeOutline, element, selectedByUser);
    }.bind(this);
}

WebInspector.TreeOutlineDataGridSynchronizer.prototype = {
    constructor: WebInspector.TreeOutlineDataGridSynchronizer,
    __proto__: WebInspector.Object,

    // Public

    associate: function(treeElement, dataGridNode)
    {
        console.assert(treeElement);
        console.assert(dataGridNode);

        treeElement.__dataGridNode = dataGridNode;
        dataGridNode.__treeElement = treeElement;
    },

    synchronize: function()
    {
        this._dataGrid.scrollContainer.scrollTop = this._treeOutline.element.parentNode.scrollTop;
        if (this._treeOutline.selectedTreeElement)
            this._treeOutline.selectedTreeElement.__dataGridNode.select(true);
        else if (this._dataGrid.selectedNode)
            this._dataGrid.selectedNode.deselect(true);
    },

    // Private

    _treeOutlineScrolled: function(event)
    {
        this._dataGrid.scrollContainer.scrollTop = this._treeOutline.element.parentNode.scrollTop;
    },

    _dataGridScrolled: function(event)
    {
        this._treeOutline.element.parentNode.scrollTop = this._dataGrid.scrollContainer.scrollTop;
    },

    _dataGridNodeSelected: function(event)
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (dataGridNode)
            dataGridNode.__treeElement.select(true, true, true, true);
    },

    _dataGridNodeExpanded: function(event)
    {
        var dataGridNode = event.data.dataGridNode;
        console.assert(dataGridNode);

        if (!dataGridNode.__treeElement.expanded)
            dataGridNode.__treeElement.expand();
    },

    _dataGridNodeCollapsed: function(event)
    {
        var dataGridNode = event.data.dataGridNode;
        console.assert(dataGridNode);

        if (dataGridNode.__treeElement.expanded)
            dataGridNode.__treeElement.collapse();
    },

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        var dataGridNode = treeElement.__dataGridNode;
        console.assert(dataGridNode);

        dataGridNode.select(true);
    },

    _treeElementAdded: function(treeElement)
    {
        var dataGridNode = treeElement.__dataGridNode;
        console.assert(dataGridNode);

        var parentDataGridNode = treeElement.parent.__dataGridNode;
        console.assert(dataGridNode);

        var childIndex = treeElement.parent.children.indexOf(treeElement);
        console.assert(childIndex !== -1);

        parentDataGridNode.insertChild(dataGridNode, childIndex);
    },

    _treeElementRemoved: function(treeElement)
    {
        var dataGridNode = treeElement.__dataGridNode;
        console.assert(dataGridNode);

        dataGridNode.parent.removeChild(dataGridNode);
    },

    _treeElementExpanded: function(treeElement)
    {
        var dataGridNode = treeElement.__dataGridNode;
        console.assert(dataGridNode);

        if (!dataGridNode.expanded)
            dataGridNode.expand();
    },

    _treeElementCollapsed: function(treeElement)
    {
        var dataGridNode = treeElement.__dataGridNode;
        console.assert(dataGridNode);

        if (dataGridNode.expanded)
            dataGridNode.collapse();
    },

    _treeElementHiddenChanged: function(treeElement, hidden)
    {
        var dataGridNode = treeElement.__dataGridNode;
        console.assert(dataGridNode);

        if (hidden)
            dataGridNode.element.classList.add("hidden");
        else
            dataGridNode.element.classList.remove("hidden");
    }
}

WebInspector.TreeOutlineDataGridSynchronizer.prototype.__proto__ = WebInspector.Object.prototype;
