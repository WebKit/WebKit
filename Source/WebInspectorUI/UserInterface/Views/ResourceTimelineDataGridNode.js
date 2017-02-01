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

WebInspector.ResourceTimelineDataGridNode = class ResourceTimelineDataGridNode extends WebInspector.TimelineDataGridNode
{
    constructor(resourceTimelineRecord, includesGraph, graphDataSource, shouldShowPopover)
    {
        super(includesGraph, graphDataSource);

        this._resource = resourceTimelineRecord.resource;
        this._record = resourceTimelineRecord;
        this._shouldShowPopover = shouldShowPopover;

        this._resource.addEventListener(WebInspector.Resource.Event.LoadingDidFinish, this._needsRefresh, this);
        this._resource.addEventListener(WebInspector.Resource.Event.LoadingDidFail, this._needsRefresh, this);
        this._resource.addEventListener(WebInspector.Resource.Event.URLDidChange, this._needsRefresh, this);

        if (includesGraph)
            this._record.addEventListener(WebInspector.TimelineRecord.Event.Updated, this._timelineRecordUpdated, this);
        else {
            this._resource.addEventListener(WebInspector.Resource.Event.TypeDidChange, this._needsRefresh, this);
            this._resource.addEventListener(WebInspector.Resource.Event.SizeDidChange, this._needsRefresh, this);
            this._resource.addEventListener(WebInspector.Resource.Event.TransferSizeDidChange, this._needsRefresh, this);
        }
    }

    // Public

    get records()
    {
        return [this._record];
    }

    get resource()
    {
        return this._resource;
    }

    get data()
    {
        if (this._cachedData)
            return this._cachedData;

        var resource = this._resource;
        var data = {};

        if (!this._includesGraph) {
            var zeroTime = this.graphDataSource ? this.graphDataSource.zeroTime : 0;

            data.domain = WebInspector.displayNameForHost(resource.urlComponents.host);
            data.scheme = resource.urlComponents.scheme ? resource.urlComponents.scheme.toUpperCase() : "";
            data.method = resource.requestMethod;
            data.type = resource.type;
            data.statusCode = resource.statusCode;
            data.cached = resource.cached;
            data.size = resource.size;
            data.transferSize = resource.transferSize;
            data.requestSent = resource.requestSentTimestamp - zeroTime;
            data.duration = resource.receiveDuration;
            data.latency = resource.latency;
        }

        data.graph = this._record.startTime;

        this._cachedData = data;
        return data;
    }

    createCellContent(columnIdentifier, cell)
    {
        var resource = this._resource;

        if (resource.failed || resource.canceled || resource.statusCode >= 400)
            cell.classList.add("error");

        var value = this.data[columnIdentifier];

        switch (columnIdentifier) {
        case "name":
            cell.classList.add(...this.iconClassNames());
            cell.title = resource.displayURL;
            this._updateStatus(cell);
            return this._createNameCellDocumentFragment();

        case "type":
            return WebInspector.Resource.displayNameForType(value);

        case "statusCode":
            cell.title = resource.statusText || "";
            return value || emDash;

        case "cached":
            return value ? WebInspector.UIString("Yes") : WebInspector.UIString("No");

        case "domain":
            return value || emDash;

        case "size":
        case "transferSize":
            return isNaN(value) ? emDash : Number.bytesToString(value, true);

        case "requestSent":
        case "latency":
        case "duration":
            return isNaN(value) ? emDash : Number.secondsToString(value, true);
        }

        return super.createCellContent(columnIdentifier, cell);
    }

    refresh()
    {
        if (this._scheduledRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledRefreshIdentifier);
            this._scheduledRefreshIdentifier = undefined;
        }

        this._cachedData = null;

        super.refresh();
    }

    iconClassNames()
    {
        return [WebInspector.ResourceTreeElement.ResourceIconStyleClassName, this.resource.type];
    }

    appendContextMenuItems(contextMenu)
    {
        WebInspector.appendContextMenuItemsForSourceCode(contextMenu, this._resource);
    }

    // Protected

    didAddRecordBar(recordBar)
    {
        if (!this._shouldShowPopover)
            return;

        if (!recordBar.records.length || recordBar.records[0].type !== WebInspector.TimelineRecord.Type.Network)
            return;

        console.assert(!this._mouseEnterRecordBarListener);
        this._mouseEnterRecordBarListener = this._mouseoverRecordBar.bind(this);
        recordBar.element.addEventListener("mouseenter", this._mouseEnterRecordBarListener);
    }

    didRemoveRecordBar(recordBar)
    {
        if (!this._shouldShowPopover)
            return;

        if (!recordBar.records.length || recordBar.records[0].type !== WebInspector.TimelineRecord.Type.Network)
            return;

        recordBar.element.removeEventListener("mouseenter", this._mouseEnterRecordBarListener);
        this._mouseEnterRecordBarListener = null;
    }

    filterableDataForColumn(columnIdentifier)
    {
        if (columnIdentifier === "name")
            return this._resource.url;
        return super.filterableDataForColumn(columnIdentifier);
    }

    // Private

    _createNameCellDocumentFragment()
    {
        let fragment = document.createDocumentFragment();
        let mainTitle = this.displayName();
        fragment.append(mainTitle);

        // Show the host as the subtitle if it is different from the main resource or if this is the main frame's main resource.
        let frame = this._resource.parentFrame;
        let isMainResource = this._resource.isMainResource();
        let parentResourceHost;
        if (frame && isMainResource) {
            // When the resource is a main resource, get the host from the current frame's parent frame instead of the current frame.
            parentResourceHost = frame.parentFrame ? frame.parentFrame.mainResource.urlComponents.host : null;
        } else if (frame) {
            // When the resource is a normal sub-resource, get the host from the current frame's main resource.
            parentResourceHost = frame.mainResource.urlComponents.host;
        }

        if (parentResourceHost !== this._resource.urlComponents.host || frame.isMainFrame() && isMainResource) {
            let subtitle = WebInspector.displayNameForHost(this._resource.urlComponents.host);
            if (mainTitle !== subtitle) {
                let subtitleElement = document.createElement("span");
                subtitleElement.classList.add("subtitle");
                subtitleElement.textContent = subtitle;
                fragment.append(subtitleElement);
            }
        }

        return fragment;
    }

    _needsRefresh()
    {
        if (this.dataGrid instanceof WebInspector.TimelineDataGrid) {
            this.dataGrid.dataGridNodeNeedsRefresh(this);
            return;
        }

        if (this._scheduledRefreshIdentifier)
            return;

        this._scheduledRefreshIdentifier = requestAnimationFrame(this.refresh.bind(this));
    }

    _timelineRecordUpdated(event)
    {
        if (this.isRecordVisible(this._record))
            this.needsGraphRefresh();
    }

    _dataGridNodeGoToArrowClicked()
    {
        WebInspector.showSourceCode(this._resource);
    }

    _updateStatus(cell)
    {
        if (this._resource.failed)
            cell.classList.add("error");
        else {
            cell.classList.remove("error");

            if (this._resource.finished)
                this.createGoToArrowButton(cell, this._dataGridNodeGoToArrowClicked.bind(this));
        }

        if (this._spinner)
            this._spinner.element.remove();

        if (this._resource.finished || this._resource.failed)
            return;

        if (!this._spinner)
            this._spinner = new WebInspector.IndeterminateProgressSpinner;

        let contentElement = cell.firstChild;
        contentElement.appendChild(this._spinner.element);
    }

    _mouseoverRecordBar(event)
    {
        let recordBar = WebInspector.TimelineRecordBar.fromElement(event.target);
        console.assert(recordBar);
        if (!recordBar)
            return;

        let calculateTargetFrame = () => {
            let columnRect = WebInspector.Rect.rectFromClientRect(this.elementWithColumnIdentifier("graph").getBoundingClientRect());
            let barRect = WebInspector.Rect.rectFromClientRect(event.target.getBoundingClientRect());
            return columnRect.intersectionWithRect(barRect);
        };

        let targetFrame = calculateTargetFrame();
        if (!targetFrame.size.width && !targetFrame.size.height)
            return;

        console.assert(recordBar.records.length);
        let resource = recordBar.records[0].resource;
        if (!resource.timingData)
            return;

        if (!resource.timingData.responseEnd)
            return;

        if (this.dataGrid._dismissPopoverTimeout) {
            clearTimeout(this.dataGrid._dismissPopoverTimeout);
            this.dataGrid._dismissPopoverTimeout = undefined;
        }

        let popoverContentElement = document.createElement("div");
        popoverContentElement.classList.add("resource-timing-popover-content");

        if (resource.failed || resource.urlComponents.scheme === "data" || (resource.cached && resource.statusCode !== 304)) {
            let descriptionElement = document.createElement("span");
            descriptionElement.classList.add("description");
            if (resource.failed)
                descriptionElement.textContent = WebInspector.UIString("Resource failed to load.");
            else if (resource.urlComponents.scheme === "data")
                descriptionElement.textContent = WebInspector.UIString("Resource was loaded with the “data“ scheme.");
            else
                descriptionElement.textContent = WebInspector.UIString("Resource was served from the cache.");
            popoverContentElement.appendChild(descriptionElement);
        } else {
            let columns = {
                description: {
                    width: "80px"
                },
                graph: {
                    width: `${WebInspector.ResourceTimelineDataGridNode.PopoverGraphColumnWidthPixels}px`
                },
                duration: {
                    width: "70px",
                    aligned: "right"
                }
            };

            let popoverDataGrid = new WebInspector.DataGrid(columns);
            popoverDataGrid.inline = true;
            popoverDataGrid.headerVisible = false;
            popoverContentElement.appendChild(popoverDataGrid.element);

            let graphDataSource = {
                get secondsPerPixel() { return resource.duration / WebInspector.ResourceTimelineDataGridNode.PopoverGraphColumnWidthPixels; },
                get zeroTime() { return resource.firstTimestamp; },
                get startTime() { return resource.firstTimestamp; },
                get currentTime() { return this.endTime; },

                get endTime()
                {
                    let endTimePadding = this.secondsPerPixel * WebInspector.TimelineRecordBar.MinimumWidthPixels;
                    return resource.lastTimestamp + endTimePadding;
                }
            };

            let secondTimestamp = resource.timingData.domainLookupStart || resource.timingData.connectStart || resource.timingData.requestStart;
            if (secondTimestamp - resource.timingData.startTime)
                popoverDataGrid.appendChild(new WebInspector.ResourceTimingPopoverDataGridNode(WebInspector.UIString("Stalled"), resource.timingData.startTime, secondTimestamp, graphDataSource));
            if (resource.timingData.domainLookupStart)
                popoverDataGrid.appendChild(new WebInspector.ResourceTimingPopoverDataGridNode(WebInspector.UIString("DNS"), resource.timingData.domainLookupStart, resource.timingData.domainLookupEnd, graphDataSource));
            if (resource.timingData.connectStart)
                popoverDataGrid.appendChild(new WebInspector.ResourceTimingPopoverDataGridNode(WebInspector.UIString("Connection"), resource.timingData.connectStart, resource.timingData.connectEnd, graphDataSource));
            if (resource.timingData.secureConnectionStart)
                popoverDataGrid.appendChild(new WebInspector.ResourceTimingPopoverDataGridNode(WebInspector.UIString("Secure"), resource.timingData.secureConnectionStart, resource.timingData.connectEnd, graphDataSource));
            popoverDataGrid.appendChild(new WebInspector.ResourceTimingPopoverDataGridNode(WebInspector.UIString("Request"), resource.timingData.requestStart, resource.timingData.responseStart, graphDataSource));
            popoverDataGrid.appendChild(new WebInspector.ResourceTimingPopoverDataGridNode(WebInspector.UIString("Response"), resource.timingData.responseStart, resource.timingData.responseEnd, graphDataSource));

            const higherResolution = true;
            let totalData = {
                description: WebInspector.UIString("Total time"),
                duration: Number.secondsToMillisecondsString(resource.timingData.responseEnd - resource.timingData.startTime, higherResolution)
            };
            popoverDataGrid.appendChild(new WebInspector.DataGridNode(totalData));

            popoverDataGrid.updateLayout();
        }

        if (!this.dataGrid._popover)
            this.dataGrid._popover = new WebInspector.Popover;

        let preferredEdges = [WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MIN_Y, WebInspector.RectEdge.MIN_X];
        this.dataGrid._popover.windowResizeHandler = () => {
            let bounds = calculateTargetFrame();
            this.dataGrid._popover.present(bounds.pad(2), preferredEdges);
        };

        recordBar.element.addEventListener("mouseleave", () => {
            if (!this.dataGrid)
                return;
            this.dataGrid._dismissPopoverTimeout = setTimeout(() => this.dataGrid._popover.dismiss(), WebInspector.ResourceTimelineDataGridNode.DelayedPopoverDismissalTimeout);
        }, {once: true});

        this.dataGrid._popover.presentNewContentWithFrame(popoverContentElement, targetFrame.pad(2), preferredEdges);
    }
};

WebInspector.ResourceTimelineDataGridNode.PopoverGraphColumnWidthPixels = 110;
WebInspector.ResourceTimelineDataGridNode.DelayedPopoverDismissalTimeout = 500;
