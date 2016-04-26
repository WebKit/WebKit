/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.OverviewTimelineView = class OverviewTimelineView extends WebInspector.TimelineView
{
    constructor(recording, extraArguments)
    {
        super(recording, extraArguments);

        this._recording = recording;

        let columns = {name: {}, graph: {}};

        columns.name.title = WebInspector.UIString("Name");
        columns.name.width = "20%";
        columns.name.icon = true;
        columns.name.disclosure = true;

        this._timelineRuler = new WebInspector.TimelineRuler;
        this._timelineRuler.allowsClippedLabels = true;

        columns.graph.width = "80%";
        columns.graph.headerView = this._timelineRuler;

        this._dataGrid = new WebInspector.DataGrid(columns);

        this.setupDataGrid(this._dataGrid);

        this._currentTimeMarker = new WebInspector.TimelineMarker(0, WebInspector.TimelineMarker.Type.CurrentTime);
        this._timelineRuler.addMarker(this._currentTimeMarker);

        this.element.classList.add("overview");
        this.addSubview(this._dataGrid);

        this._networkTimeline = recording.timelines.get(WebInspector.TimelineRecord.Type.Network);
        if (this._networkTimeline)
            this._networkTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);

        recording.addEventListener(WebInspector.TimelineRecording.Event.SourceCodeTimelineAdded, this._sourceCodeTimelineAdded, this);
        recording.addEventListener(WebInspector.TimelineRecording.Event.MarkerAdded, this._markerAdded, this);
        recording.addEventListener(WebInspector.TimelineRecording.Event.Reset, this._recordingReset, this);

        this._pendingRepresentedObjects = [];
        this._resourceDataGridNodeMap = new Map;
    }

    // Public

    get secondsPerPixel()
    {
        return this._timelineRuler.secondsPerPixel;
    }

    set secondsPerPixel(x)
    {
        this._timelineRuler.secondsPerPixel = x;
    }

    shown()
    {
        super.shown();

        this._timelineRuler.updateLayout(WebInspector.View.LayoutReason.Resize);
    }

    closed()
    {
        if (this._networkTimeline)
            this._networkTimeline.removeEventListener(null, null, this);
        this._recording.removeEventListener(null, null, this);
    }

    get selectionPathComponents()
    {
        let dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode || dataGridNode.hidden)
            return null;

        let pathComponents = [];

        while (dataGridNode && !dataGridNode.root) {
            console.assert(dataGridNode instanceof WebInspector.TimelineDataGridNode);
            if (dataGridNode.hidden)
                return null;

            let pathComponent = new WebInspector.TimelineDataGridNodePathComponent(dataGridNode);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.dataGridNodePathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            dataGridNode = dataGridNode.parent;
        }

        return pathComponents;
    }

    reset()
    {
        super.reset();

        this._dataGrid.removeChildren();

        this._pendingRepresentedObjects = [];
    }

    // Protected

    dataGridNodePathComponentSelected(event)
    {
        let dataGridNode = event.data.pathComponent.timelineDataGridNode;
        console.assert(dataGridNode.dataGrid === this._dataGrid);

        dataGridNode.revealAndSelect();
    }

    layout()
    {
        let oldZeroTime = this._timelineRuler.zeroTime;
        let oldStartTime = this._timelineRuler.startTime;
        let oldEndTime = this._timelineRuler.endTime;
        let oldCurrentTime = this._currentTimeMarker.time;

        this._timelineRuler.zeroTime = this.zeroTime;
        this._timelineRuler.startTime = this.startTime;
        this._timelineRuler.endTime = this.endTime;
        this._currentTimeMarker.time = this.currentTime;

        // The TimelineDataGridNode graphs are positioned with percentages, so they auto resize with the view.
        // We only need to refresh the graphs when the any of the times change.
        if (this.zeroTime !== oldZeroTime || this.startTime !== oldStartTime || this.endTime !== oldEndTime || this.currentTime !== oldCurrentTime) {
            let dataGridNode = this._dataGrid.children[0];
            while (dataGridNode) {
                dataGridNode.refreshGraph();
                dataGridNode = dataGridNode.traverseNextNode(true, null, true);
            }
        }

        this._processPendingRepresentedObjects();
    }

    // Private

    _compareDataGridNodesByStartTime(a, b)
    {
        function getStartTime(dataGridNode)
        {
            if (dataGridNode instanceof WebInspector.ResourceTimelineDataGridNode)
                return dataGridNode.resource.firstTimestamp;
            if (dataGridNode instanceof WebInspector.SourceCodeTimelineTimelineDataGridNode)
                return dataGridNode.sourceCodeTimeline.startTime;

            console.error("Unknown data grid node.", dataGridNode);
            return 0;
        }

        let result = getStartTime(a) - getStartTime(b);
        if (result)
            return result;

        // Fallback to comparing titles.
        return a.displayName().localeCompare(b.displayName());
    }

    _insertDataGridNode(dataGridNode, parentDataGridNode)
    {
        console.assert(dataGridNode);
        console.assert(!dataGridNode.parent);

        if (parentDataGridNode)
            parentDataGridNode.insertChild(dataGridNode, insertionIndexForObjectInListSortedByFunction(dataGridNode, parentDataGridNode.children, this._compareDataGridNodesByStartTime.bind(this)));
        else
            this._dataGrid.appendChild(dataGridNode);
    }

    _addResourceToDataGridIfNeeded(resource)
    {
        console.assert(resource);
        if (!resource)
            return null;

        // FIXME: replace with this._dataGrid.findDataGridNode(resource) once <https://webkit.org/b/155305> is fixed.
        let dataGridNode = this._resourceDataGridNodeMap.get(resource);
        if (dataGridNode)
            return dataGridNode;

        let parentFrame = resource.parentFrame;
        if (!parentFrame)
            return;

        let resourceTimelineRecord = this._networkTimeline ? this._networkTimeline.recordForResource(resource) : null;
        if (!resourceTimelineRecord)
            resourceTimelineRecord = new WebInspector.ResourceTimelineRecord(resource);

        let resourceDataGridNode = new WebInspector.ResourceTimelineDataGridNode(resourceTimelineRecord, true, this);
        this._resourceDataGridNodeMap.set(resource, resourceDataGridNode);

        let expandedByDefault = false;
        if (parentFrame.mainResource === resource || parentFrame.provisionalMainResource === resource) {
            parentFrame = parentFrame.parentFrame;
            expandedByDefault = !parentFrame; // Main frame expands by default.
        }

        if (expandedByDefault)
            resourceDataGridNode.expand();

        let parentDataGridNode = null;
        if (parentFrame) {
            // Find the parent main resource, adding it if needed, to append this resource as a child.
            let parentResource = parentFrame.provisionalMainResource || parentFrame.mainResource;

            parentDataGridNode = this._addResourceToDataGridIfNeeded(parentResource);
            console.assert(parentDataGridNode);
            if (!parentDataGridNode)
                return;
        }

        this._insertDataGridNode(resourceDataGridNode, parentDataGridNode);

        return resourceDataGridNode;
    }

    _addSourceCodeTimeline(sourceCodeTimeline)
    {
        let parentDataGridNode = sourceCodeTimeline.sourceCodeLocation ? this._addResourceToDataGridIfNeeded(sourceCodeTimeline.sourceCode) : null;
        let sourceCodeTimelineDataGridNode = new WebInspector.SourceCodeTimelineTimelineDataGridNode(sourceCodeTimeline, this);
        this._resourceDataGridNodeMap.set(sourceCodeTimeline, sourceCodeTimelineDataGridNode);

        this._insertDataGridNode(sourceCodeTimelineDataGridNode, parentDataGridNode);
    }

    _processPendingRepresentedObjects()
    {
        if (!this._pendingRepresentedObjects.length)
            return;

        for (var representedObject of this._pendingRepresentedObjects) {
            if (representedObject instanceof WebInspector.Resource)
                this._addResourceToDataGridIfNeeded(representedObject);
            else if (representedObject instanceof WebInspector.SourceCodeTimeline)
                this._addSourceCodeTimeline(representedObject);
            else
                console.error("Unknown represented object");
        }

        this._pendingRepresentedObjects = [];
    }

    _networkTimelineRecordAdded(event)
    {
        var resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        this._pendingRepresentedObjects.push(resourceTimelineRecord.resource);

        this.needsLayout();

        // We don't expect to have any source code timelines yet. Those should be added with _sourceCodeTimelineAdded.
        console.assert(!this._recording.sourceCodeTimelinesForSourceCode(resourceTimelineRecord.resource).length);
    }

    _sourceCodeTimelineAdded(event)
    {
        var sourceCodeTimeline = event.data.sourceCodeTimeline;
        console.assert(sourceCodeTimeline);
        if (!sourceCodeTimeline)
            return;

        this._pendingRepresentedObjects.push(sourceCodeTimeline);

        this.needsLayout();
    }

    _markerAdded(event)
    {
        this._timelineRuler.addMarker(event.data.marker);
    }

    _recordingReset(event)
    {
        this._timelineRuler.clearMarkers();
        this._timelineRuler.addMarker(this._currentTimeMarker);
    }
};
