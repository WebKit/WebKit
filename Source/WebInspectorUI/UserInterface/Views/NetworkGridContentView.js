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

WI.NetworkGridContentView = class NetworkGridContentView extends WI.ContentView
{
    constructor(representedObject, extraArguments)
    {
        console.assert(extraArguments);
        console.assert(extraArguments.networkSidebarPanel instanceof WI.NetworkSidebarPanel);

        super(representedObject);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._networkSidebarPanel = extraArguments.networkSidebarPanel;

        this._contentTreeOutline = this._networkSidebarPanel.contentTreeOutline;
        this._contentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        let columns = {domain: {}, type: {}, method: {}, scheme: {}, statusCode: {}, cached: {}, protocol: {}, priority: {}, remoteAddress: {}, connectionIdentifier: {}, size: {}, transferSize: {}, requestSent: {}, latency: {}, duration: {}, graph: {}};

        columns.domain.title = columns.domain.tooltip = WI.UIString("Domain");
        columns.domain.width = "10%";

        columns.type.title = columns.type.tooltip = WI.UIString("Type");
        columns.type.width = "6%";

        columns.method.title = columns.method.tooltip = WI.UIString("Method");
        columns.method.width = "5%";

        columns.scheme.title = columns.scheme.tooltip = WI.UIString("Scheme");
        columns.scheme.width = "5%";

        columns.statusCode.title = columns.statusCode.tooltip = WI.UIString("Status");
        columns.statusCode.width = "5%";

        columns.cached.title = columns.cached.tooltip = WI.UIString("Cached");
        columns.cached.width = "8%";

        columns.protocol.title = columns.protocol.tooltip = WI.UIString("Protocol");
        columns.protocol.width = "5%";
        columns.protocol.hidden = true;

        columns.priority.title = columns.priority.tooltip = WI.UIString("Priority");
        columns.priority.width = "5%";
        columns.priority.hidden = true;

        columns.remoteAddress.title = columns.remoteAddress.tooltip = WI.UIString("IP Address");
        columns.remoteAddress.width = "8%";
        columns.remoteAddress.hidden = true;

        columns.connectionIdentifier.title = columns.connectionIdentifier.tooltip = WI.UIString("Connection ID");
        columns.connectionIdentifier.width = "5%";
        columns.connectionIdentifier.hidden = true;
        columns.connectionIdentifier.aligned = "right";

        columns.size.title = columns.size.tooltip = WI.UIString("Size");
        columns.size.width = "6%";
        columns.size.aligned = "right";

        columns.transferSize.title = columns.transferSize.tooltip = WI.UIString("Transferred");
        columns.transferSize.width = "8%";
        columns.transferSize.aligned = "right";

        columns.requestSent.title = columns.requestSent.tooltip = WI.UIString("Start Time");
        columns.requestSent.width = "9%";
        columns.requestSent.aligned = "right";

        columns.latency.title = columns.latency.tooltip = WI.UIString("Latency");
        columns.latency.width = "9%";
        columns.latency.aligned = "right";

        columns.duration.title = columns.duration.tooltip = WI.UIString("Duration");
        columns.duration.width = "9%";
        columns.duration.aligned = "right";

        for (let column in columns)
            columns[column].sortable = true;

        this._timelineRuler = new WI.TimelineRuler;
        this._timelineRuler.allowsClippedLabels = true;

        columns.graph.title = WI.UIString("Timeline");
        columns.graph.width = "20%";
        columns.graph.headerView = this._timelineRuler;
        columns.graph.sortable = false;

        // COMPATIBILITY(iOS 10.3): Network load metrics were not previously available.
        if (!NetworkAgent.hasEventParameter("loadingFinished", "metrics")) {
            delete columns.protocol;
            delete columns.priority;
            delete columns.remoteAddress;
            delete columns.connectionIdentifier;
        }

        this._dataGrid = new WI.TimelineDataGrid(columns, this._contentTreeOutline);
        this._dataGrid.addEventListener(WI.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this._dataGrid.sortDelegate = this;
        this._dataGrid.sortColumnIdentifier = "requestSent";
        this._dataGrid.sortOrder = WI.DataGrid.SortOrder.Ascending;
        this._dataGrid.createSettings("network-grid-content-view");

        this.element.classList.add("network-grid");
        this.addSubview(this._dataGrid);

        let networkTimeline = WI.timelineManager.persistentNetworkTimeline;
        networkTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
        networkTimeline.addEventListener(WI.Timeline.Event.Reset, this._networkTimelineReset, this);

        // COMPATIBILITY (iOS 10.3): Network.setDisableResourceCaching did not exist.
        if (window.NetworkAgent && NetworkAgent.setResourceCachingDisabled) {
            let toolTipForDisableResourceCache = WI.UIString("Ignore the resource cache when loading resources");
            let activatedToolTipForDisableResourceCache = WI.UIString("Use the resource cache when loading resources");
            this._disableResourceCacheNavigationItem = new WI.ActivateButtonNavigationItem("disable-resource-cache", toolTipForDisableResourceCache, activatedToolTipForDisableResourceCache, "Images/IgnoreCaches.svg", 16, 16);
            this._disableResourceCacheNavigationItem.activated = WI.resourceCachingDisabledSetting.value;

            this._disableResourceCacheNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleDisableResourceCache, this);
            WI.resourceCachingDisabledSetting.addEventListener(WI.Setting.Event.Changed, this._resourceCachingDisabledSettingChanged, this);
        }

        this._clearNetworkItemsNavigationItem = new WI.ButtonNavigationItem("clear-network-items", WI.UIString("Clear Network Items (%s)").format(WI.clearKeyboardShortcut.displayName), "Images/NavigationItemClear.svg", 16, 16);
        this._clearNetworkItemsNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => this.reset());

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

        var pathComponent = new WI.GeneralTreeElementPathComponent(this._contentTreeOutline.selectedTreeElement);
        pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._treeElementPathComponentSelected, this);
        return [pathComponent];
    }

    get navigationItems()
    {
        let items = [];

        if (this._disableResourceCacheNavigationItem)
            items.push(this._disableResourceCacheNavigationItem);
        items.push(this._clearNetworkItemsNavigationItem);

        return items;
    }

    shown()
    {
        super.shown();

        this._dataGrid.shown();

        this._dataGrid.updateLayout(WI.View.LayoutReason.Resize);

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
            this._endTime = this._lastRecordEndTime + WI.TimelineRecordBar.MinimumWidthPixels * this.secondsPerPixel;
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

    // TimelineDataGrid sort delegate

    dataGridSortComparator(sortColumnIdentifier, sortDirection, node1, node2)
    {
        if (sortColumnIdentifier === "priority")
            return WI.Resource.comparePriority(node1.data.priority, node2.data.priority) * sortDirection;

        return null;
    }

    // Private

    _resourceCachingDisabledSettingChanged()
    {
        this._disableResourceCacheNavigationItem.activated = WI.resourceCachingDisabledSetting.value;
    }

    _toggleDisableResourceCache()
    {
        WI.resourceCachingDisabledSetting.value = !WI.resourceCachingDisabledSetting.value;
    }

    _processPendingRecords()
    {
        if (!this._pendingRecords.length)
            return;

        for (var resourceTimelineRecord of this._pendingRecords) {
            // Skip the record if it already exists in the tree.
            var treeElement = this._contentTreeOutline.findTreeElement(resourceTimelineRecord.resource);
            if (treeElement)
                continue;

            treeElement = new WI.ResourceTreeElement(resourceTimelineRecord.resource);

            const includesGraph = false;
            const shouldShowPopover = true;
            let dataGridNode = new WI.ResourceTimelineDataGridNode(resourceTimelineRecord, includesGraph, this, shouldShowPopover);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);
        }

        this._pendingRecords = [];
    }

    _mainResourceDidChange(event)
    {
        let frame = event.target;
        if (!frame.isMainFrame() || WI.settings.clearNetworkOnNavigate.value)
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
        console.assert(resourceTimelineRecord instanceof WI.ResourceTimelineRecord);

        let update = (event) => {
            if (event.target[WI.NetworkGridContentView.ResourceDidFinishOrFail])
                return;

            event.target.removeEventListener(null, null, this);
            event.target[WI.NetworkGridContentView.ResourceDidFinishOrFail] = true;

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

        resource[WI.NetworkGridContentView.ResourceDidFinishOrFail] = false;
        resource.addEventListener(WI.Resource.Event.LoadingDidFinish, update, this);
        resource.addEventListener(WI.Resource.Event.LoadingDidFail, update, this);

        this._loadingResourceCount++;
        if (this._scheduledCurrentTimeUpdateIdentifier)
            return;

        if (isNaN(this._startTime))
            this._startTime = this._endTime = resourceTimelineRecord.startTime;

        // FIXME: <https://webkit.org/b/153634> Web Inspector: some background tabs think they are the foreground tab and do unnecessary work
        if (!(WI.tabBrowser.selectedTabContentView instanceof WI.NetworkTabContentView))
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
        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);

        if (!this._networkSidebarPanel.canShowDifferentContentView())
            return;

        let treeElement = event.data.selectedElement;
        if (treeElement instanceof WI.ResourceTreeElement) {
            WI.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    }

    _dataGridNodeSelected(event)
    {
        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
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
        if (!WI.visible)
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

WI.NetworkGridContentView.ResourceDidFinishOrFail = Symbol("ResourceDidFinishOrFail");
