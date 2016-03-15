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

WebInspector.HeapSnapshotInstancesDataGridTree = class HeapSnapshotInstancesDataGridTree extends WebInspector.Object
{
    constructor(heapSnapshot, sortComparator, includeInternalObjects)
    {
        super();

        console.assert(heapSnapshot instanceof WebInspector.HeapSnapshot);

        this._heapSnapshot = heapSnapshot;

        this._children = [];
        this._sortComparator = sortComparator;
        this._includeInternalObjects = includeInternalObjects;

        this._popover = null;
        this._popoverNode = null;

        this._populateTopLevel();
        this.sort();
    }

    // Static

    static buildSortComparator(columnIdentifier, sortOrder)
    {
        let multiplier = sortOrder === WebInspector.DataGrid.SortOrder.Ascending ? 1 : -1;
        let numberCompare = (columnIdentifier, a, b) => multiplier * (a.data[columnIdentifier] - b.data[columnIdentifier]);
        let localeCompare = (columnIdentifier, a, b) => multiplier * (a.data[columnIdentifier].localeCompare(b.data[columnIdentifier]));

        let comparator;
        switch (columnIdentifier) {
        case "size":
            return numberCompare.bind(this, "size");
        case "count":
            return numberCompare.bind(this, "count");
        case "className":
            return localeCompare.bind(this, "className");
        }
    }

    // Public

    get heapSnapshot() { return this._heapSnapshot; }

    get includeInternalObjects()
    {
        return this._includeInternalObjects;
    }

    set includeInternalObjects(includeInternal)
    {
        if (this._includeInternalObjects === includeInternal)
            return;

        this._includeInternalObjects = includeInternal;

        this._populateTopLevel();
        this.sort();
    }

    get popover()
    {
        if (!this._popover)
            this._popover = new WebInspector.Popover(this);

        return this._popover;
    }

    get popoverNode()
    {
        return this._popoverNode;
    }

    set popoverNode(x)
    {
        this._popoverNode = x;
    }

    get children()
    {
        return this._children;
    }

    appendChild(node)
    {
        this._children.push(node);
    }

    insertChild(node, index)
    {
        this._children.splice(index, 0, node);
    }

    removeChildren()
    {
        this._children = [];
    }

    set sortComparator(comparator)
    {
        this._sortComparator = comparator;
        this.sort();
    }

    sort()
    {
        let children = this._children;
        children.sort(this._sortComparator);

        for (let i = 0; i < children.length; ++i) {
            children[i]._recalculateSiblings(i);
            children[i].sort();
        }
    }

    hidden()
    {
        if (this._popover && this._popover.visible)
            this._popover.dismiss();
    }

    // Popover delegate

    willDismissPopover(popover)
    {
        this._popoverNode = null;
    }

    // Private

    _populateTopLevel()
    {
        this.removeChildren();

        // Populate the first level with the different classes.
        let totalSize = this._heapSnapshot.totalSize;
        for (let [className, {size, count, internalCount}] of this._heapSnapshot.categories) {
            let allInternal = count === internalCount;
            if (!this._includeInternalObjects && allInternal)
                continue;
            let percent = (size / totalSize) * 100;
            this.appendChild(new WebInspector.HeapSnapshotClassDataGridNode({className, size, count, percent, allInternal}, this));
        }
    }
};
