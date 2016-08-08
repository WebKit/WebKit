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

WebInspector.HeapSnapshotDataGridTree = class HeapSnapshotDataGridTree extends WebInspector.Object
{
    constructor(heapSnapshot, sortComparator)
    {
        super();

        console.assert(heapSnapshot instanceof WebInspector.HeapSnapshotProxy || heapSnapshot instanceof WebInspector.HeapSnapshotDiffProxy);

        this._heapSnapshot = heapSnapshot;
        this._heapSnapshot.addEventListener(WebInspector.HeapSnapshotProxy.Event.CollectedNodes, this._heapSnapshotCollectedNodes, this);

        this._children = [];
        this._sortComparator = sortComparator;

        this._visible = false;
        this._popover = null;
        this._popoverGridNode = null;
        this._popoverTargetElement = null;

        this.populateTopLevel();
    }

    // Static

    static buildSortComparator(columnIdentifier, sortOrder)
    {
        let multiplier = sortOrder === WebInspector.DataGrid.SortOrder.Ascending ? 1 : -1;
        let numberCompare = (columnIdentifier, a, b) => multiplier * (a.data[columnIdentifier] - b.data[columnIdentifier]);
        let localeCompare = (columnIdentifier, a, b) => multiplier * (a.data[columnIdentifier].localeCompare(b.data[columnIdentifier]));

        let comparator;
        switch (columnIdentifier) {
        case "retainedSize":
            return numberCompare.bind(this, "retainedSize");
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

    get visible() { return this._visible; }
    get popoverGridNode() { return this._popoverGridNode; }
    set popoverGridNode(x) { this._popoverGridNode = x; }
    get popoverTargetElement() { return this._popoverTargetElement; }
    set popoverTargetElement(x) { this._popoverTargetElement = x; }

    get popover()
    {
        if (!this._popover) {
            this._popover = new WebInspector.Popover(this);
            this._popover.windowResizeHandler = () => {
                let bounds = WebInspector.Rect.rectFromClientRect(this._popoverTargetElement.getBoundingClientRect());
                this._popover.present(bounds.pad(2), [WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MIN_Y, WebInspector.RectEdge.MAX_X]);
            };
        }

        return this._popover;
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

    removeChild(node)
    {
        this._children.remove(node, true);
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

    shown()
    {
        this._visible = true;
    }

    hidden()
    {
        this._visible = false;

        if (this._popover && this._popover.visible)
            this._popover.dismiss();
    }

    // Popover delegate

    willDismissPopover(popover)
    {
        this._popoverGridNode = null;
        this._popoverTargetElement = null;
    }

    // Protected

    get alwaysShowRetainedSize()
    {
        return false;
    }

    populateTopLevel()
    {
        // Implemented by subclasses.
    }

    removeCollectedNodes(collectedNodes)
    {
        // Implemented by subclasses.
    }

    didPopulate()
    {
        this.sort();

        this.dispatchEventToListeners(WebInspector.HeapSnapshotDataGridTree.Event.DidPopulate);
    }

    // Private

    _heapSnapshotCollectedNodes(event)
    {
        this.removeCollectedNodes(event.data.collectedNodes);
    }
};

WebInspector.HeapSnapshotDataGridTree.Event = {
    DidPopulate: "heap-snapshot-data-grid-tree-did-populate",
};

WebInspector.HeapSnapshotInstancesDataGridTree = class HeapSnapshotInstancesDataGridTree extends WebInspector.HeapSnapshotDataGridTree
{
    get alwaysShowRetainedSize()
    {
        return false;
    }

    populateTopLevel()
    {
        // Populate the first level with the different non-internal classes.
        for (let [className, {size, retainedSize, count, internalCount, deadCount}] of this.heapSnapshot.categories) {
            if (count === internalCount)
                continue;

            // FIXME: <https://webkit.org/b/157905> Web Inspector: Provide a way to toggle between showing only live objects and live+dead objects
            let liveCount = count - deadCount;
            if (!liveCount)
                continue;

            this.appendChild(new WebInspector.HeapSnapshotClassDataGridNode({className, size, retainedSize, count: liveCount}, this));
        }

        this.didPopulate()
    }

    removeCollectedNodes(collectedNodes)
    {
        for (let classDataGridNode of this.children) {
            let {count, deadCount} = this.heapSnapshot.categories.get(classDataGridNode.data.className);
            let liveCount = count - deadCount;
            classDataGridNode.updateCount(liveCount);
            if (liveCount)
                classDataGridNode.removeCollectedNodes(collectedNodes);
        }

        this.didPopulate();
    }
};

WebInspector.HeapSnapshotObjectGraphDataGridTree = class HeapSnapshotInstancesDataGridTree extends WebInspector.HeapSnapshotDataGridTree
{
    get alwaysShowRetainedSize()
    {
        return true;
    }

    populateTopLevel()
    {
        this.heapSnapshot.instancesWithClassName("GlobalObject", (instances) => {
            for (let instance of instances)
                this.appendChild(new WebInspector.HeapSnapshotInstanceDataGridNode(instance, this));
        });

        this.heapSnapshot.instancesWithClassName("Window", (instances) => {
            for (let instance of instances) {
                // FIXME: Why is the window.Window Function classified as a Window?
                // In any case, ignore objects not dominated by the root, as they
                // are probably not what we want.
                if (instance.dominatorNodeIdentifier === 0)
                    this.appendChild(new WebInspector.HeapSnapshotInstanceDataGridNode(instance, this));
            }

            this.didPopulate();
        });
    }
};
