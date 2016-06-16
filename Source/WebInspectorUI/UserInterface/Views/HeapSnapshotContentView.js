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

WebInspector.HeapSnapshotContentView = class HeapSnapshotContentView extends WebInspector.ContentView
{
    constructor(representedObject, identifier, columns, heapSnapshotDataGridTreeConstructor)
    {
        console.assert(representedObject instanceof WebInspector.HeapSnapshotProxy || representedObject instanceof WebInspector.HeapSnapshotDiffProxy);

        super(representedObject);

        this.element.classList.add("heap-snapshot");

        this._dataGrid = new WebInspector.DataGrid(columns);
        this._dataGrid.sortColumnIdentifier = "retainedSize";
        this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Descending;
        this._dataGrid.createSettings(identifier);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sortDataGrid, this);

        let sortComparator = WebInspector.HeapSnapshotDataGridTree.buildSortComparator(this._dataGrid.sortColumnIdentifier, this._dataGrid.sortOrder);
        this._heapSnapshotDataGridTree = new heapSnapshotDataGridTreeConstructor(this.representedObject, sortComparator);
        this._heapSnapshotDataGridTree.addEventListener(WebInspector.HeapSnapshotDataGridTree.Event.DidPopulate, this._heapSnapshotDataGridTreeDidPopulate, this);

        for (let child of this._heapSnapshotDataGridTree.children)
            this._dataGrid.appendChild(child);

        this.addSubview(this._dataGrid);
        this._dataGrid.updateLayout();
    }

    // Protected

    shown()
    {
        super.shown();

        this._heapSnapshotDataGridTree.shown();
    }

    hidden()
    {
        super.hidden();

        this._heapSnapshotDataGridTree.hidden();
    }

    get scrollableElements()
    {
        return [this._dataGrid.scrollContainer];
    }

    // Private

    _sortDataGrid()
    {
        if (!this._heapSnapshotDataGridTree)
            return;

        this._heapSnapshotDataGridTree.sortComparator = WebInspector.HeapSnapshotDataGridTree.buildSortComparator(this._dataGrid.sortColumnIdentifier, this._dataGrid.sortOrder);

        this._dataGrid.removeChildren();
        for (let child of this._heapSnapshotDataGridTree.children)
            this._dataGrid.appendChild(child);
    }

    _heapSnapshotDataGridTreeDidPopulate()
    {
        this._dataGrid.removeChildren();
        for (let child of this._heapSnapshotDataGridTree.children)
            this._dataGrid.appendChild(child);
    }
};

WebInspector.HeapSnapshotInstancesContentView = class HeapSnapshotInstancesContentView extends WebInspector.HeapSnapshotContentView
{
    constructor(representedObject)
    {
        let columns = {
            retainedSize: {
                title: WebInspector.UIString("Retained Size"),
                tooltip: WebInspector.UIString("Size of the current object plus the size of all objects it keeps alive."),
                width: "140px",
                aligned: "right",
                sortable: true, 
            },
            size: {
                title: WebInspector.UIString("Self Size"),
                width: "90px",
                aligned: "right",
                sortable: true,
            },
            count: {
                title: WebInspector.UIString("Count"),
                width: "75px",
                aligned: "right",
                sortable: true,
            },
            className: {
                title: WebInspector.UIString("Class Name"),
                sortable: true,
                disclosure: true,
            }
        };

        super(representedObject, "heap-snapshot-instances-content-view", columns, WebInspector.HeapSnapshotInstancesDataGridTree);
    }
};

WebInspector.HeapSnapshotObjectGraphContentView = class HeapSnapshotObjectGraphContentView extends WebInspector.HeapSnapshotContentView
{
    constructor(representedObject)
    {
        let columns = {
            retainedSize: {
                title: WebInspector.UIString("Retained Size"),
                tooltip: WebInspector.UIString("Size of the current object plus the size of all objects it keeps alive."),
                width: "140px",
                aligned: "right",
                sortable: true, 
            },
            size: {
                title: WebInspector.UIString("Self Size"),
                width: "90px",
                aligned: "right",
                sortable: true,
            },
            className: {
                title: WebInspector.UIString("Class Name"),
                sortable: true,
                disclosure: true,
            }
        };

        super(representedObject, "heap-snapshot-object-graph-content-view", columns, WebInspector.HeapSnapshotObjectGraphDataGridTree);
    }
};
