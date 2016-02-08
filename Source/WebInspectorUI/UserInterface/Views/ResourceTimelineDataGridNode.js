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
    constructor(resourceTimelineRecord, graphOnly, graphDataSource)
    {
        super(graphOnly, graphDataSource);

        this._resource = resourceTimelineRecord.resource;
        this._record = resourceTimelineRecord;

        this._record.addEventListener(WebInspector.TimelineRecord.Event.Updated, graphOnly ? this._timelineRecordUpdated : this._needsRefresh, this);

        if (!graphOnly) {
            this._resource.addEventListener(WebInspector.Resource.Event.URLDidChange, this._needsRefresh, this);
            this._resource.addEventListener(WebInspector.Resource.Event.TypeDidChange, this._needsRefresh, this);
            this._resource.addEventListener(WebInspector.Resource.Event.LoadingDidFinish, this._needsRefresh, this);
            this._resource.addEventListener(WebInspector.Resource.Event.LoadingDidFail, this._needsRefresh, this);
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

        if (!this._graphOnly) {
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

    // Private

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
};
