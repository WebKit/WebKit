/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.HeapSnapshotInstancesContentView = class HeapSnapshotInstancesContentView extends WebInspector.ContentView
{
    constructor(representedObject, extraArguments)
    {
        console.assert(representedObject instanceof WebInspector.HeapSnapshotProxy || representedObject instanceof WebInspector.HeapSnapshotDiffProxy);

        super(representedObject);

        this.element.classList.add("heap-snapshot");

        let {showInternalObjects} = extraArguments;

        let columns = {
            size: {
                title: WebInspector.UIString("Total Size"),
                width: "140px",
                aligned: "right",
                sortable: true,
            },
            count: {
                title: WebInspector.UIString("Count"),
                width: "80px",
                aligned: "right",
                sortable: true,
            },
            className: {
                title: WebInspector.UIString("Class Name"),
                sortable: true,
                disclosure: true,
            }
        };

        this._dataGrid = new WebInspector.DataGrid(columns);
        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("heap-snapshot-content-view-sort", "size");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("heap-snapshot-content-view-sort-order", WebInspector.DataGrid.SortOrder.Descending);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sortDataGrid, this);

        let sortComparator = WebInspector.HeapSnapshotInstancesDataGridTree.buildSortComparator(this._dataGrid.sortColumnIdentifier, this._dataGrid.sortOrder);
        this._heapSnapshotDataGridTree = new WebInspector.HeapSnapshotInstancesDataGridTree(this.representedObject, sortComparator, showInternalObjects);

        for (let child of this._heapSnapshotDataGridTree.children)
            this._dataGrid.appendChild(child);

        this.addSubview(this._dataGrid);
        this._dataGrid.updateLayout();
    }

    // Public

    get showInternalObjects()
    {
        return this._heapSnapshotDataGridTree.includeInternalObjects;
    }

    set showInternalObjects(showInternalObjects)
    {
        if (this.showInternalObjects === showInternalObjects)
            return;

        this._heapSnapshotDataGridTree.includeInternalObjects = showInternalObjects;

        this._sortDataGrid();
    }

    // Protected

    hidden()
    {
        super.hidden();

        this._heapSnapshotDataGridTree.hidden();
    }

    // Private

    _sortDataGrid()
    {
        if (!this._heapSnapshotDataGridTree)
            return;

        this._heapSnapshotDataGridTree.sortComparator = WebInspector.HeapSnapshotInstancesDataGridTree.buildSortComparator(this._dataGrid.sortColumnIdentifier, this._dataGrid.sortOrder);

        this._dataGrid.removeChildren();
        for (let child of this._heapSnapshotDataGridTree.children)
            this._dataGrid.appendChild(child);
    }
};
