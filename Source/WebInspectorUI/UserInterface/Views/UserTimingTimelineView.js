/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.UserTimingTimelineView = class UserTimingTimelineView extends WI.TimelineView
{
    constructor(timeline, extraArguments)
    {
        console.assert(timeline instanceof WI.Timeline);
        console.assert(timeline.type === WI.TimelineRecord.Type.UserTiming);

        super(timeline, extraArguments);

        this.element.classList.add("user-timing");

        const columns = {
            name: {
                title: WI.UIString("Label"),
                width: "20%",
                icon: true,
                sortable: true,
                locked: true,
            },
            startTime: {
                title: WI.UIString("Start Time"),
                width: "10%",
                aligned: "right",
                sortable: true,
            },
            totalTime: {
                title: WI.UIString("Total Time"),
                width: "10%",
                aligned: "right",
                sortable: true,
            },
            endTime: {
                title: WI.UIString("End Time"),
                width: "10%",
                aligned: "right",
                sortable: true,
            },
            location: {
                title: WI.UIString("Location"),
                width: "15%",
            },
            detail: {
                title: WI.UIString("Detail"),
                width: "35%",
            },
        };

        this._dataGrid = new WI.TimelineDataGrid(columns);
        this._dataGrid.sortDelegate = this;
        this._dataGrid.sortColumnIdentifier = "startTime";
        this._dataGrid.sortOrder = WI.DataGrid.SortOrder.Ascending;
        this._dataGrid.variableHeightRows = true;
        this._dataGrid.createSettings("user-timing-timeline-view");
        this.setupDataGrid(this._dataGrid);
        this.addSubview(this._dataGrid);

        timeline.addEventListener(WI.Timeline.Event.RecordAdded, this._handleRecordAdded, this);

        this._pendingRecords = [];

        for (let record of timeline.records)
            this._processRecord(record);
    }

    // Public

    get selectionPathComponents()
    {
        if (!this._dataGrid.selectedNode || this._dataGrid.selectedNode.hidden)
            return null;

        let pathComponent = new WI.TimelineDataGridNodePathComponent(this._dataGrid.selectedNode);
        pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._handleSelectionPathComponentSiblingSelected, this);
        return [pathComponent];
    }

    closed()
    {
        this.representedObject.removeEventListener(WI.Timeline.Event.RecordAdded, this._handleRecordAdded, this);

        this._dataGrid.closed();

        super.closed();
    }

    reset()
    {
        this._dataGrid.reset();

        this._pendingRecords = [];

        super.reset();
    }

    // TimelineDataGrid delegate

    dataGridSortComparator(sortColumnIdentifier, sortDirection, node1, node2)
    {
        function compareTimes(a, b) {
            if (isNaN(a))
                return isNaN(b) ? 0 : -1;
            if (isNaN(b))
                return 1;
            return a - b;
        }

        if (sortColumnIdentifier === "name")
            return (node1.record.label || "").extendedLocaleCompare(node2.record.label || "") * sortDirection;

        if (sortColumnIdentifier === "startTime")
            return compareTimes(node1.record.startTime, node2.record.startTime) * sortDirection;

        if (sortColumnIdentifier === "endTime")
            return compareTimes(node1.record.endTime, node2.record.endTime) * sortDirection;

        if (sortColumnIdentifier === "totalTime")
            return compareTimes(node1.record.duration, node2.record.duration) * sortDirection;

        return null;
    }

    // Protected

    dataGridMatchNodeAgainstCustomFilters(node)
    {
        if (!this.matchDataGridNodeAgainstCustomFilters(node))
            return false;

        let record = node.record;
        let startInRange = isNaN(record.startTime) || record.startTime <= this.filterEndTime;
        let endInRange = isNaN(record.endTime) || record.endTime >= this.filterStartTime;
        return startInRange && endInRange;
    }

    layout()
    {
        super.layout();

        this._processPendingRecords();
    }

    // Private

    _processPendingRecords()
    {
        if (!this._pendingRecords.length)
            return;

        for (let userTimingTimelineRecord of this._pendingRecords) {
            this._dataGrid.addRowInSortOrder(new WI.UserTimingTimelineDataGridNode(userTimingTimelineRecord, {
                graphDataSource: this,
            }));
        }

        this._pendingRecords = [];
    }

    _handleRecordAdded(event)
    {
        let userTimingTimelineRecord = event.data.record;
        console.assert(userTimingTimelineRecord instanceof WI.UserTimingTimelineRecord);

        this._processRecord(userTimingTimelineRecord);

        this.needsLayout();
    }

    _processRecord(userTimingTimelineRecord)
    {
        this._pendingRecords.push(userTimingTimelineRecord);
    }

    _handleSelectionPathComponentSiblingSelected(event)
    {
        let pathComponent = event.data.pathComponent;
        console.assert(pathComponent instanceof WI.TimelineDataGridNodePathComponent);

        let dataGridNode = pathComponent.timelineDataGridNode;
        console.assert(dataGridNode.dataGrid === this._dataGrid);

        dataGridNode.revealAndSelect();
    }
};

WI.UserTimingTimelineView.ReferencePage = WI.ReferencePage.TimelinesTab.UserTimingTimeline;
