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

WebInspector.HeapAllocationsTimelineView = class HeapAllocationsTimelineView extends WebInspector.TimelineView
{
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(timeline.type === WebInspector.TimelineRecord.Type.HeapAllocations, timeline);

        this.element.classList.add("heap-allocations");

        let columns = {
            name: {
                title: WebInspector.UIString("Name"),
                width: "150px",
            },
            timestamp: {
                title: WebInspector.UIString("Time"),
                width: "80px",
                sortable: true,
                aligned: "right",
            },
            size: {
                title: WebInspector.UIString("Size"),
                width: "80px",
                sortable: true,
                aligned: "right",
            },
        };

        let snapshotTooltip = WebInspector.UIString("Take snapshot");
        this._takeHeapSnapshotButtonItem = new WebInspector.ButtonNavigationItem("take-snapshot", snapshotTooltip, "Images/HeapSnapshot.svg", 16, 16);
        this._takeHeapSnapshotButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._takeHeapSnapshotClicked, this);

        let defaultToolTip = WebInspector.UIString("Compare snapshots");
        let activatedToolTip = WebInspector.UIString("Cancel comparison");
        this._compareHeapSnapshotsButtonItem = new WebInspector.ActivateButtonNavigationItem("compare-heap-snapshots", defaultToolTip, activatedToolTip, "Images/Compare.svg", 16, 16);
        this._compareHeapSnapshotsButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._compareHeapSnapshotsClicked, this);
        this._compareHeapSnapshotsButtonItem.enabled = false;
        this._compareHeapSnapshotsButtonItem.activated = false;

        this._compareHeapSnapshotHelpTextItem = new WebInspector.TextNavigationItem("compare-heap-snapshot-help-text", "");

        this._selectingComparisonHeapSnapshots = false;
        this._baselineDataGridNode = null;
        this._baselineHeapSnapshotTimelineRecord = null;
        this._heapSnapshotDiff = null;

        this._showingSnapshotList = true;

        this._snapshotListPathComponent = new WebInspector.HierarchicalPathComponent(WebInspector.UIString("Snapshot List"), "snapshot-list-icon", "snapshot-list", false, false);
        this._snapshotListPathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.Clicked, this._snapshotListPathComponentClicked, this);

        this._dataGrid = new WebInspector.TimelineDataGrid(null, columns);
        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("heap-allocations-timeline-view-sort", "timestamp");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("heap-allocations-timeline-view-sort-order", WebInspector.DataGrid.SortOrder.Ascending);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this.addSubview(this._dataGrid);

        this._contentViewContainer = new WebInspector.ContentViewContainer;
        this._contentViewContainer.addEventListener(WebInspector.ContentViewContainer.Event.CurrentContentViewDidChange, this._currentContentViewDidChange, this);
        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._contentViewSelectionPathComponentDidChange, this);

        this._pendingRecords = [];

        timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._heapAllocationsTimelineRecordAdded, this);
    }

    // Public

    showHeapSnapshotList()
    {
        if (this._showingSnapshotList)
            return;

        this._showingSnapshotList = true;
        this._heapSnapshotDiff = null;
        this._cancelSelectComparisonHeapSnapshots();

        this._contentViewContainer.hidden();
        this.removeSubview(this._contentViewContainer);
        this.addSubview(this._dataGrid);

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    }

    showHeapSnapshotTimelineRecord(heapSnapshotTimelineRecord)
    {
        if (this._showingSnapshotList) {
            this.removeSubview(this._dataGrid);
            this.addSubview(this._contentViewContainer);
            this._contentViewContainer.shown();
        }

        this._showingSnapshotList = false;
        this._heapSnapshotDiff = null;
        this._cancelSelectComparisonHeapSnapshots();

        for (let dataGridNode of this._dataGrid.children) {
            if (dataGridNode.record === heapSnapshotTimelineRecord) {
                dataGridNode.select();
                break;
            }
        }

        let shouldManuallyTriggerContentViewUpdate = this._contentViewContainer.currentContentView && this._contentViewContainer.currentContentView.representedObject === heapSnapshotTimelineRecord.heapSnapshot;

        this._contentViewContainer.showContentViewForRepresentedObject(heapSnapshotTimelineRecord.heapSnapshot);

        if (shouldManuallyTriggerContentViewUpdate)
            this._currentContentViewDidChange();
    }

    showHeapSnapshotDiff(heapSnapshotDiff)
    {
        if (this._showingSnapshotList) {
            this.removeSubview(this._dataGrid);
            this.addSubview(this._contentViewContainer);
            this._contentViewContainer.shown();
        }

        this._showingSnapshotList = false;
        this._heapSnapshotDiff = heapSnapshotDiff;

        this._contentViewContainer.showContentViewForRepresentedObject(heapSnapshotDiff);
    }

    // Protected

    get navigationItems()
    {
        if (this._showingSnapshotList) {
            let items = [this._takeHeapSnapshotButtonItem, this._compareHeapSnapshotsButtonItem];
            if (this._selectingComparisonHeapSnapshots)
                items.push(this._compareHeapSnapshotHelpTextItem);
            return items;
        }

        return this._contentViewContainer.currentContentView.navigationItems;
    }

    get selectionPathComponents()
    {
        let components = [this._snapshotListPathComponent];

        if (this._showingSnapshotList)
            return components;

        if (this._heapSnapshotDiff) {
            let firstSnapshotIdentifier = this._heapSnapshotDiff.snapshot1.identifier;
            let secondSnapshotIdentifier = this._heapSnapshotDiff.snapshot2.identifier;
            let diffComponent = new WebInspector.HierarchicalPathComponent(WebInspector.UIString("Snapshot Comparison (%d and %d)").format(firstSnapshotIdentifier, secondSnapshotIdentifier), "snapshot-diff-icon", "snapshot-diff");
            components.push(diffComponent);
        } else {
            if (this._dataGrid.selectedNode) {
                let heapSnapshotPathComponent = new WebInspector.TimelineDataGridNodePathComponent(this._dataGrid.selectedNode);
                heapSnapshotPathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._snapshotPathComponentSelected, this);
                components.push(heapSnapshotPathComponent);
            }
        }

        return components.concat(this._contentViewContainer.currentContentView.selectionPathComponents);
    }

    userSelectedRecordFromOverview(timelineRecord)
    {
        this.showHeapSnapshotTimelineRecord(timelineRecord);
    }

    shown()
    {
        super.shown();

        this._dataGrid.shown();

        if (!this._showingSnapshotList)
            this._contentViewContainer.shown();
    }

    hidden()
    {
        super.hidden();

        this._dataGrid.hidden();

        if (!this._showingSnapshotList)
            this._contentViewContainer.hidden();
    }

    closed()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);

        this._dataGrid.closed();

        this._contentViewContainer.closeAllContentViews();
    }

    layout()
    {
        // Wait to show records until our zeroTime has been set.
        if (this._pendingRecords.length && this.zeroTime) {
            for (let heapAllocationsTimelineRecord of this._pendingRecords) {
                let dataGridNode = new WebInspector.HeapAllocationsTimelineDataGridNode(heapAllocationsTimelineRecord, this.zeroTime, this);
                this._dataGrid.addRowInSortOrder(null, dataGridNode);
            }

            this._pendingRecords = [];
            this._updateCompareHeapSnapshotButton();
        }
    }

    reset()
    {
        super.reset();

        this._dataGrid.reset();

        this.showHeapSnapshotList();
        this._pendingRecords = [];
        this._updateCompareHeapSnapshotButton();
    }

    // Private

    _heapAllocationsTimelineRecordAdded(event)
    {
        this._pendingRecords.push(event.data.record);

        this.needsLayout();
    }

    _snapshotListPathComponentClicked(event)
    {
        this.showHeapSnapshotList();
    }

    _snapshotPathComponentSelected(event)
    {
        console.assert(event.data.pathComponent.representedObject instanceof WebInspector.HeapAllocationsTimelineRecord);
        this.showHeapSnapshotTimelineRecord(event.data.pathComponent.representedObject);
    }

    _currentContentViewDidChange(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    }

    _contentViewSelectionPathComponentDidChange(event)
    {
        if (event.target !== this._contentViewContainer.currentContentView)
            return;
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _updateCompareHeapSnapshotButton()
    {
        let hasAtLeastTwoSnapshots = false;

        let count = 0;
        for (let node of this._dataGrid.children) {
            if (node.revealed && !node.hidden) {
                count++;
                if (count === 2) {
                    hasAtLeastTwoSnapshots = true;
                    break;
                }
            }
        }

        this._compareHeapSnapshotsButtonItem.enabled = hasAtLeastTwoSnapshots;
    }

    _takeHeapSnapshotClicked()
    {
        HeapAgent.snapshot(function(error, timestamp, snapshotStringData) {
            let workerProxy = WebInspector.HeapSnapshotWorkerProxy.singleton();
            workerProxy.createSnapshot(snapshotStringData, ({objectId, snapshot: serializedSnapshot}) => {
                let snapshot = WebInspector.HeapSnapshotProxy.deserialize(objectId, serializedSnapshot);
                WebInspector.timelineManager.heapSnapshotAdded(timestamp, snapshot);
            });
        });
    }

    _cancelSelectComparisonHeapSnapshots()
    {
        if (!this._selectingComparisonHeapSnapshots)
            return;

        if (this._baselineDataGridNode)
            this._baselineDataGridNode.clearBaseline();

        this._selectingComparisonHeapSnapshots = false;
        this._baselineDataGridNode = null;
        this._baselineHeapSnapshotTimelineRecord = null;

        this._compareHeapSnapshotsButtonItem.activated = false;

        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    }

    _compareHeapSnapshotsClicked(event)
    {
        let newActivated = !this._compareHeapSnapshotsButtonItem.activated;
        this._compareHeapSnapshotsButtonItem.activated = newActivated;

        if (!newActivated) {
            this._cancelSelectComparisonHeapSnapshots();
            return;
        }

        if (this._dataGrid.selectedNode)
            this._dataGrid.selectedNode.deselect();

        this._selectingComparisonHeapSnapshots = true;
        this._baselineHeapSnapshotTimelineRecord = null;
        this._compareHeapSnapshotHelpTextItem.text = WebInspector.UIString("Select baseline snapshot");

        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    }

    _dataGridNodeSelected(event)
    {
        if (!this._selectingComparisonHeapSnapshots)
            return;

        let dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        let heapAllocationsTimelineRecord = dataGridNode.record;
        if (this._baselineHeapSnapshotTimelineRecord === heapAllocationsTimelineRecord) {
            this._dataGrid.selectedNode.deselect();
            return;
        }

        // Selected Baseline.
        if (!this._baselineHeapSnapshotTimelineRecord) {
            this._baselineDataGridNode = dataGridNode;
            this._baselineDataGridNode.markAsBaseline();
            this._baselineHeapSnapshotTimelineRecord = heapAllocationsTimelineRecord;
            this._dataGrid.selectedNode.deselect();
            this._compareHeapSnapshotHelpTextItem.text = WebInspector.UIString("Select comparison snapshot");
            this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
            return;
        }

        // Selected Comparison.
        let snapshot1 = this._baselineHeapSnapshotTimelineRecord.heapSnapshot;
        let snapshot2 = heapAllocationsTimelineRecord.heapSnapshot;
        let workerProxy = WebInspector.HeapSnapshotWorkerProxy.singleton();
        workerProxy.createSnapshotDiff(snapshot1.proxyObjectId, snapshot2.proxyObjectId, ({objectId, snapshotDiff: serializedSnapshotDiff}) => {
            let diff = WebInspector.HeapSnapshotDiffProxy.deserialize(objectId, serializedSnapshotDiff);
            this.showHeapSnapshotDiff(diff);
        });

        this._baselineDataGridNode.clearBaseline();
        this._selectingComparisonHeapSnapshots = false;
        this._compareHeapSnapshotsButtonItem.activated = false;
    }
};
