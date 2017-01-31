/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.NetworkGridContentView = class NetworkGridContentView extends WebInspector.ContentView
{
    constructor(representedObject, extraArguments)
    {
        console.assert(extraArguments);
        console.assert(extraArguments.networkSidebarPanel instanceof WebInspector.NetworkSidebarPanel);

        super(representedObject);

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._networkSidebarPanel = extraArguments.networkSidebarPanel;

        this._contentTreeOutline = this._networkSidebarPanel.contentTreeOutline;
        this._contentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        var columns = {domain: {}, type: {}, method: {}, scheme: {}, statusCode: {}, cached: {}, size: {}, transferSize: {}, requestSent: {}, latency: {}, duration: {}, graph: {}};

        columns.domain.title = WebInspector.UIString("Domain");
        columns.domain.width = "10%";

        columns.type.title = WebInspector.UIString("Type");
        columns.type.width = "8%";

        columns.method.title = WebInspector.UIString("Method");
        columns.method.width = "6%";

        columns.scheme.title = WebInspector.UIString("Scheme");
        columns.scheme.width = "6%";

        columns.statusCode.title = WebInspector.UIString("Status");
        columns.statusCode.width = "6%";

        columns.cached.title = WebInspector.UIString("Cached");
        columns.cached.width = "6%";

        columns.size.title = WebInspector.UIString("Size");
        columns.size.width = "8%";
        columns.size.aligned = "right";

        columns.transferSize.title = WebInspector.UIString("Transferred");
        columns.transferSize.width = "8%";
        columns.transferSize.aligned = "right";

        columns.requestSent.title = WebInspector.UIString("Start Time");
        columns.requestSent.width = "9%";
        columns.requestSent.aligned = "right";

        columns.latency.title = WebInspector.UIString("Latency");
        columns.latency.width = "9%";
        columns.latency.aligned = "right";

        columns.duration.title = WebInspector.UIString("Duration");
        columns.duration.width = "9%";
        columns.duration.aligned = "right";

        for (var column in columns)
            columns[column].sortable = true;

        this._timelineRuler = new WebInspector.TimelineRuler;
        this._timelineRuler.allowsClippedLabels = true;

        columns.graph.title = WebInspector.UIString("Timeline");
        columns.graph.width = "15%";
        columns.graph.headerView = this._timelineRuler;
        columns.graph.sortable = false;

        this._dataGrid = new WebInspector.TimelineDataGrid(columns, this._contentTreeOutline);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this._dataGrid.sortColumnIdentifier = "requestSent";
        this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;
        this._dataGrid.createSettings("network-grid-content-view");

        this.element.classList.add("network-grid");
        this.addSubview(this._dataGrid);

        var networkTimeline = WebInspector.timelineManager.persistentNetworkTimeline;
        networkTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
        networkTimeline.addEventListener(WebInspector.Timeline.Event.Reset, this._networkTimelineReset, this);

        let clearImageDimensions = WebInspector.Platform.name === "mac" ? 16 : 15;
        this._clearNetworkItemsNavigationItem = new WebInspector.ButtonNavigationItem("clear-network-items", WebInspector.UIString("Clear Network Items (%s)").format(WebInspector.clearKeyboardShortcut.displayName), "Images/NavigationItemClear.svg", clearImageDimensions, clearImageDimensions);
        this._clearNetworkItemsNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, () => this.reset());

        this._pendingRecords = [];
        this._loadingResourceCount = 0;
        this._lastRecordEndTime = NaN;
        this._lastUpdateTimestamp = NaN;
        this._startTime = NaN;
        this._endTime = NaN;
        this._scheduledCurrentTimeUpdateIdentifier = undefined;
    }

    // Public

    get secondsPerPixel() { return this._timelineRuler.secondsPerPixel; }
    get startTime() { return this._startTime; }
    get currentTime() { return this.endTime || this.startTime; }
    get endTime() { return this._endTime; }
    get zeroTime() { return this.startTime; }

    get selectionPathComponents()
    {
        if (!this._contentTreeOutline.selectedTreeElement || this._contentTreeOutline.selectedTreeElement.hidden)
            return null;

        var pathComponent = new WebInspector.GeneralTreeElementPathComponent(this._contentTreeOutline.selectedTreeElement);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._treeElementPathComponentSelected, this);
        return [pathComponent];
    }

    get navigationItems()
    {
        return [this._clearNetworkItemsNavigationItem];
    }

    shown()
    {
        super.shown();

        this._dataGrid.shown();

        this._dataGrid.updateLayout(WebInspector.View.LayoutReason.Resize);

        if (this._loadingResourceCount && !this._scheduledCurrentTimeUpdateIdentifier)
            this._startUpdatingCurrentTime();
    }

    hidden()
    {
        this._dataGrid.hidden();

        if (this._scheduledCurrentTimeUpdateIdentifier)
            this._stopUpdatingCurrentTime();

        super.hidden();
    }

    closed()
    {
        super.closed();

        this._dataGrid.closed();
    }

    reset()
    {
        this._contentTreeOutline.removeChildren();
        this._dataGrid.reset();

        if (this._scheduledCurrentTimeUpdateIdentifier)
            this._stopUpdatingCurrentTime();

        this._pendingRecords = [];
        this._loadingResourceCount = 0;
        this._lastRecordEndTime = NaN;
        this._lastUpdateTimestamp = NaN;
        this._startTime = NaN;
        this._endTime = NaN;

        this._timelineRuler.startTime = 0;
        this._timelineRuler.endTime = 0;
    }

    // Protected

    layout()
    {
        if (isNaN(this.startTime) || isNaN(this.endTime))
            return;

        let oldZeroTime = this._timelineRuler.zeroTime;
        let oldStartTime = this._timelineRuler.startTime;
        let oldEndTime = this._timelineRuler.endTime;

        this._timelineRuler.zeroTime = this.zeroTime;
        this._timelineRuler.startTime = this.startTime;

        if (this.startTime >= this.endTime)
            return;

        if (!this._scheduledCurrentTimeUpdateIdentifier) {
            this._timelineRuler.endTime = this.endTime;
            this._endTime = this._lastRecordEndTime + WebInspector.TimelineRecordBar.MinimumWidthPixels * this.secondsPerPixel;
        }

        this._timelineRuler.endTime = this.endTime;

        // We only need to refresh the graphs when the any of the times change.
        if (this.zeroTime !== oldZeroTime || this.startTime !== oldStartTime || this.endTime !== oldEndTime) {
            for (let dataGridNode of this._dataGrid.children)
                dataGridNode.refreshGraph();
        }

        this._processPendingRecords();
    }

    handleClearShortcut(event)
    {
        this.reset();
    }

    // Private

    _processPendingRecords()
    {
        if (!this._pendingRecords.length)
            return;

        for (var resourceTimelineRecord of this._pendingRecords) {
            // Skip the record if it already exists in the tree.
            var treeElement = this._contentTreeOutline.findTreeElement(resourceTimelineRecord.resource);
            if (treeElement)
                continue;

            treeElement = new WebInspector.ResourceTreeElement(resourceTimelineRecord.resource);

            const includesGraph = false;
            const shouldShowPopover = true;
            let dataGridNode = new WebInspector.ResourceTimelineDataGridNode(resourceTimelineRecord, includesGraph, this, shouldShowPopover);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);
        }

        this._pendingRecords = [];
    }

    _mainResourceDidChange(event)
    {
        let frame = event.target;
        if (!frame.isMainFrame() || WebInspector.settings.clearNetworkOnNavigate.value)
            return;

        for (let dataGridNode of this._dataGrid.children)
            dataGridNode.element.classList.add("preserved");
    }

    _networkTimelineReset(event)
    {
        this.reset();
    }

    _networkTimelineRecordAdded(event)
    {
        let resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        let update = (event) => {
            if (event.target[WebInspector.NetworkGridContentView.ResourceDidFinishOrFail])
                return;

            event.target.removeEventListener(null, null, this);
            event.target[WebInspector.NetworkGridContentView.ResourceDidFinishOrFail] = true;

            this._loadingResourceCount--;
            if (this._loadingResourceCount)
                return;

            this._lastRecordEndTime = resourceTimelineRecord.endTime;
            this._endTime = Math.max(this._lastRecordEndTime, this._endTime);

            if (this._scheduledCurrentTimeUpdateIdentifier)
                this.debounce(150)._stopUpdatingCurrentTime();
        };

        this._pendingRecords.push(resourceTimelineRecord);

        this.needsLayout();

        let resource = resourceTimelineRecord.resource;
        if (resource.finished || resource.failed || resource.canceled)
            return;

        resource[WebInspector.NetworkGridContentView.ResourceDidFinishOrFail] = false;
        resource.addEventListener(WebInspector.Resource.Event.LoadingDidFinish, update, this);
        resource.addEventListener(WebInspector.Resource.Event.LoadingDidFail, update, this);

        this._loadingResourceCount++;
        if (this._scheduledCurrentTimeUpdateIdentifier)
            return;

        if (isNaN(this._startTime))
            this._startTime = this._endTime = resourceTimelineRecord.startTime;

        // FIXME: <https://webkit.org/b/153634> Web Inspector: some background tabs think they are the foreground tab and do unnecessary work
        if (!(WebInspector.tabBrowser.selectedTabContentView instanceof WebInspector.NetworkTabContentView))
            return;

        this._startUpdatingCurrentTime();
    }

    _treeElementPathComponentSelected(event)
    {
        var dataGridNode = this._dataGrid.dataGridNodeForTreeElement(event.data.pathComponent.generalTreeElement);
        if (!dataGridNode)
            return;
        dataGridNode.revealAndSelect();
    }

    _treeSelectionDidChange(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        if (!this._networkSidebarPanel.canShowDifferentContentView())
            return;

        let treeElement = event.data.selectedElement;
        if (treeElement instanceof WebInspector.ResourceTreeElement) {
            WebInspector.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    }

    _dataGridNodeSelected(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _update(timestamp)
    {
        console.assert(this._scheduledCurrentTimeUpdateIdentifier);

        if (!isNaN(this._lastUpdateTimestamp)) {
            let timespanSinceLastUpdate = (timestamp - this._lastUpdateTimestamp) / 1000 || 0;
            this._endTime += timespanSinceLastUpdate;

            this.updateLayout();
        }

        this._lastUpdateTimestamp = timestamp;
        this._scheduledCurrentTimeUpdateIdentifier = requestAnimationFrame(this._updateCallback);
    }

    _startUpdatingCurrentTime()
    {
        console.assert(!this._scheduledCurrentTimeUpdateIdentifier);
        if (this._scheduledCurrentTimeUpdateIdentifier)
            return;

        // Don't update the current time if the Inspector is not visible, as the requestAnimationFrames won't work.
        if (!WebInspector.visible)
            return;

        if (!this._updateCallback)
            this._updateCallback = this._update.bind(this);

        this._scheduledCurrentTimeUpdateIdentifier = requestAnimationFrame(this._updateCallback);
    }

    _stopUpdatingCurrentTime()
    {
        console.assert(this._scheduledCurrentTimeUpdateIdentifier);
        if (!this._scheduledCurrentTimeUpdateIdentifier)
            return;

        this._stopUpdatingCurrentTime.cancelDebounce();

        cancelAnimationFrame(this._scheduledCurrentTimeUpdateIdentifier);
        this._scheduledCurrentTimeUpdateIdentifier = undefined;

        this.needsLayout();
    }
};

WebInspector.NetworkGridContentView.ResourceDidFinishOrFail = Symbol("ResourceDidFinishOrFail");
