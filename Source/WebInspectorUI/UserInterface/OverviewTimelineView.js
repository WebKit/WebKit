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

WebInspector.OverviewTimelineView = function()
{
    WebInspector.TimelineView.call(this);

    this.navigationSidebarTreeOutline.onselect = this._treeElementSelected.bind(this);

    var columns = {"graph": {width: "100%"}};
    this._dataGrid = new WebInspector.DataGrid(columns);

    this._treeOutlineDataGridSynchronizer = new WebInspector.TreeOutlineDataGridSynchronizer(this._contentTreeOutline, this._dataGrid);

    this._timelineRuler = new WebInspector.TimelineRuler;
    this._timelineRuler.allowsClippedLabels = true;
    this.element.appendChild(this._timelineRuler.element);

    this._currentTimeMarker = new WebInspector.TimelineMarker(0, WebInspector.TimelineMarker.Type.CurrentTime);
    this._timelineRuler.addMarker(this._currentTimeMarker);

    this.element.classList.add(WebInspector.OverviewTimelineView.StyleClassName);
    this.element.appendChild(this._dataGrid.element);

    this._pendingRepresentedObjects = [];

    WebInspector.timelineManager.recording.addEventListener(WebInspector.TimelineRecording.Event.SourceCodeTimelineAdded, this._sourceCodeTimelineAdded, this);
};

WebInspector.OverviewTimelineView.StyleClassName = "overview";

WebInspector.OverviewTimelineView.prototype = {
    constructor: WebInspector.OverviewTimelineView,
    __proto__: WebInspector.TimelineView.prototype,

    // Public

    get navigationSidebarTreeOutlineLabel()
    {
        return WebInspector.UIString("Timeline Events");
    },

    reset: function()
    {
        WebInspector.TimelineView.prototype.reset.call(this);

        this._networkTimeline = WebInspector.timelineManager.recording.timelines.get(WebInspector.TimelineRecord.Type.Network);
        console.assert(this._networkTimeline);

        this._networkTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
    },

    shown: function()
    {
        WebInspector.TimelineView.prototype.shown.call(this);

        this._treeOutlineDataGridSynchronizer.synchronize();
    },

    updateLayout: function()
    {
        WebInspector.TimelineView.prototype.updateLayout.call(this);

        var oldZeroTime = this._timelineRuler.zeroTime;
        var oldStartTime = this._timelineRuler.startTime;
        var oldEndTime = this._timelineRuler.endTime;
        var oldCurrentTime = this._currentTimeMarker.time;

        this._timelineRuler.zeroTime = this.zeroTime;
        this._timelineRuler.startTime = this.startTime;
        this._timelineRuler.endTime = this.endTime;
        this._currentTimeMarker.time = this.currentTime;

        // The TimelineDataGridNode graphs are positioned with percentages, so they auto resize with the view.
        // We only need to refresh the graphs when the any of the times change.
        if (this.zeroTime !== oldZeroTime || this.startTime !== oldStartTime || this.endTime !== oldEndTime || this.currentTime !== oldCurrentTime) {
            var item = this._dataGrid.children[0];
            while (item) {
                item.refreshGraph();
                item = item.traverseNextNode(false, null, true);
            }
        }

        this._timelineRuler.updateLayout();

        this._processPendingRepresentedObjects();
    },

    // Private

    _compareTreeElementsByDetails: function(a, b)
    {
        if (a instanceof WebInspector.SourceCodeTimelineTreeElement && b instanceof WebInspector.ResourceTreeElement)
            return -1;

        if (a instanceof WebInspector.ResourceTreeElement && b instanceof WebInspector.SourceCodeTimelineTreeElement)
            return 1;

        if (a instanceof WebInspector.SourceCodeTimelineTreeElement && b instanceof WebInspector.SourceCodeTimelineTreeElement) {
            aTimeline = a.sourceCodeTimeline;
            bTimeline = b.sourceCodeTimeline;

            if (!aTimeline.sourceCodeLocation && !bTimeline.sourceCodeLocation) {
                if (aTimeline.recordType !== bTimeline.recordType)
                    return aTimeline.recordType.localeCompare(bTimeline.recordType);

                return a.mainTitle.localeCompare(b.mainTitle);
            }

            if (!aTimeline.sourceCodeLocation || !bTimeline.sourceCodeLocation)
                return !!aTimeline.sourceCodeLocation - !!bTimeline.sourceCodeLocation;

            if (aTimeline.sourceCodeLocation.lineNumber !== bTimeline.sourceCodeLocation.lineNumber)
                return aTimeline.sourceCodeLocation.lineNumber - bTimeline.sourceCodeLocation.lineNumber;

            return aTimeline.sourceCodeLocation.columnNumber - bTimeline.sourceCodeLocation.columnNumber;
        }

        // Fallback to comparing by start time for ResourceTreeElement or anything else.
        return this._compareTreeElementsByStartTime(a, b);
    },

    _compareTreeElementsByStartTime: function(a, b)
    {
        function getStartTime(treeElement)
        {
            if (treeElement instanceof WebInspector.ResourceTreeElement)
                return treeElement.resource.firstTimestamp;
            if (treeElement instanceof WebInspector.SourceCodeTimelineTreeElement)
                return treeElement.sourceCodeTimeline.startTime;

            console.error("Unknown tree element.");
            return 0;
        }

        var result = getStartTime(a) - getStartTime(b);
        if (result)
            return result;

        // Fallback to comparing titles.
        return a.mainTitle.localeCompare(b.mainTitle);
    },

    _insertTreeElement: function(treeElement, parentTreeElement)
    {
        console.assert(treeElement);
        console.assert(!treeElement.parent);
        console.assert(parentTreeElement);

        parentTreeElement.insertChild(treeElement, insertionIndexForObjectInListSortedByFunction(treeElement, parentTreeElement.children, this._compareTreeElementsByStartTime.bind(this)));
    },

    _addResourceToTreeIfNeeded: function(resource)
    {
        console.assert(resource);
        if (!resource)
            return null;

        var treeElement = this.navigationSidebarTreeOutline.findTreeElement(resource);
        if (treeElement)
            return treeElement;

        var parentFrame = resource.parentFrame;
        if (!parentFrame)
            return;

        if (parentFrame.mainResource === resource || parentFrame.provisionalMainResource === resource)
            parentFrame = parentFrame.parentFrame;

        var resourceTreeElement = new WebInspector.ResourceTreeElement(resource);
        resourceTreeElement.expand();

        var resourceTimelineRecord = this._networkTimeline ? this._networkTimeline.recordForResource(resource) : null;
        if (!resourceTimelineRecord)
            resourceTimelineRecord = new WebInspector.ResourceTimelineRecord(resource);

        var resourceDataGridNode = new WebInspector.ResourceTimelineDataGridNode(resourceTimelineRecord, true, this);
        this._treeOutlineDataGridSynchronizer.associate(resourceTreeElement, resourceDataGridNode);

        var parentTreeElement = this.navigationSidebarTreeOutline;
        if (parentFrame) {
            // Find the parent main resource, adding it if needed, to append this resource as a child.
            var parentResource = parentFrame.provisionalMainResource || parentFrame.mainResource;

            parentTreeElement = this._addResourceToTreeIfNeeded(parentResource);
            console.assert(parentTreeElement);
            if (!parentTreeElement)
                return;
        }

        this._insertTreeElement(resourceTreeElement, parentTreeElement);

        return resourceTreeElement;
    },

    _addSourceCodeTimeline: function(sourceCodeTimeline)
    {
        var parentTreeElement = sourceCodeTimeline.sourceCodeLocation ? this._addResourceToTreeIfNeeded(sourceCodeTimeline.sourceCode) : this.navigationSidebarTreeOutline;
        console.assert(parentTreeElement);
        if (!parentTreeElement)
            return;

        var sourceCodeTimelineTreeElement = new WebInspector.SourceCodeTimelineTreeElement(sourceCodeTimeline);
        var sourceCodeTimelineDataGridNode = new WebInspector.SourceCodeTimelineTimelineDataGridNode(sourceCodeTimeline, this);

        this._treeOutlineDataGridSynchronizer.associate(sourceCodeTimelineTreeElement, sourceCodeTimelineDataGridNode);
        this._insertTreeElement(sourceCodeTimelineTreeElement, parentTreeElement);
    },

    _processPendingRepresentedObjects: function()
    {
        if (!this._pendingRepresentedObjects || !this._pendingRepresentedObjects.length)
            return;

        for (var representedObject of this._pendingRepresentedObjects) {
            if (representedObject instanceof WebInspector.Resource)
                this._addResourceToTreeIfNeeded(representedObject);
            else if (representedObject instanceof WebInspector.SourceCodeTimeline)
                this._addSourceCodeTimeline(representedObject);
            else
                console.error("Unknown represented object");
        }

        this._pendingRepresentedObjects = [];
    },

    _networkTimelineRecordAdded: function(event)
    {
        var resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        this._pendingRepresentedObjects.push(resourceTimelineRecord.resource);

        this.needsLayout();

        // We don't expect to have any source code timelines yet. Those should be added with _sourceCodeTimelineAdded.
        console.assert(!WebInspector.timelineManager.recording.sourceCodeTimelinesForSourceCode(resourceTimelineRecord.resource).length);
    },

    _sourceCodeTimelineAdded: function(event)
    {
        var sourceCodeTimeline = event.data.sourceCodeTimeline;
        console.assert(sourceCodeTimeline);
        if (!sourceCodeTimeline)
            return;

        this._pendingRepresentedObjects.push(sourceCodeTimeline);

        this.needsLayout();
    },

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        if (!WebInspector.timelineSidebarPanel.canShowDifferentContentView())
            return;

        if (treeElement instanceof WebInspector.FolderTreeElement)
            return;

        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            WebInspector.resourceSidebarPanel.showSourceCode(treeElement.representedObject);
            return;
        }

        if (!(treeElement instanceof WebInspector.SourceCodeTimelineTreeElement)) {
            console.error("Unknown tree element selected.");
            return;
        }

        if (!treeElement.sourceCodeTimeline.sourceCodeLocation) {
            WebInspector.timelineSidebarPanel.showTimelineOverview();
            return;
        }

        WebInspector.resourceSidebarPanel.showOriginalOrFormattedSourceCodeLocation(treeElement.sourceCodeTimeline.sourceCodeLocation);
    }
};
