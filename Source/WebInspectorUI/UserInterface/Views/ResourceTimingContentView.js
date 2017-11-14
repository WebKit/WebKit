/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.ResourceTimingContentView = class ResourceTimingContentView extends WI.ContentView
{
    constructor(resource)
    {
        super(null);

        console.assert(resource instanceof WI.Resource);

        this._resource = resource;
        this._resource.addEventListener(WI.Resource.Event.MetricsDidChange, this._resourceMetricsDidChange, this);
        this._resource.addEventListener(WI.Resource.Event.TimestampsDidChange, this._resourceTimestampsDidChange, this);

        this.element.classList.add("resource-details", "resource-timing");

        this._needsTimingRefresh = false;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let contentElement = this.element.appendChild(document.createElement("div"));
        contentElement.className = "content";

        this._timingSection = contentElement.appendChild(document.createElement("section"));
        this._timingSection.className = "timing";

        this._refreshTimingSection();

        this._needsTimingRefresh = false;
    }

    layout()
    {
        super.layout();

        if (this._needsTimingRefresh) {
            this._refreshTimingSection();
            this._needsTimingRefresh = false;
        }
    }

    closed()
    {
        this._resource.removeEventListener(null, null, this);

        super.closed();
    }

    // Private

    _refreshTimingSection()
    {
        this._timingSection.removeChildren();

        if (!this._resource.hasResponse()) {
            let spinner = new WI.IndeterminateProgressSpinner;
            this._timingSection.appendChild(spinner.element);
            return;
        }

        if (!this._resource.timingData.startTime || !this._resource.timingData.responseEnd) {
            let p = this._timingSection.appendChild(document.createElement("p"));
            p.className = "empty";
            p.textContent = WI.UIString("Resource does not have timing data");
            return;
        }

        // FIXME: Converge on using WI.ResourceTimingBreakdownView when a design is finalized.

        let listElement = this._timingSection.appendChild(document.createElement("ul"));
        listElement.className = "waterfall"; // Include waterfall block styles.

        const graphWidth = 380;
        const graphStartOffset = 80;

        let {startTime, domainLookupStart, domainLookupEnd, connectStart, connectEnd, secureConnectionStart, requestStart, responseStart, responseEnd} = this._resource.timingData;
        let graphStartTime = startTime;
        let graphEndTime = responseEnd;
        let secondsPerPixel = (responseEnd - startTime) / graphWidth;

        function createBlock(startTime, endTime, className, makeEmpty) {
            let startOffset = graphStartOffset + ((startTime - graphStartTime) / secondsPerPixel);
            let width = makeEmpty ? 1 : (endTime - startTime) / secondsPerPixel;
            let block = document.createElement("div");
            block.classList.add("block", className);
            let property = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? "right" : "left";
            block.style[property] = startOffset + "px";
            block.style.width = width + "px";
            return block;
        }

        function createTimeLabel(endTime, label) {
            let positionOffset = graphStartOffset + ((endTime - graphStartTime) / secondsPerPixel);
            positionOffset += 3;
            let timeLabel = document.createElement("div");
            timeLabel.className = "time-label";
            timeLabel.textContent = label;
            let property = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? "right" : "left";
            timeLabel.style[property] = positionOffset + "px";
            return timeLabel;
        }

        function createRow(label, startTime, endTime, className) {
            let row = document.createElement("li");
            let labelElement = row.appendChild(document.createElement("span"));
            labelElement.className = "row-label";
            labelElement.textContent = label;
            row.appendChild(createBlock(startTime, endTime, className));
            row.appendChild(createTimeLabel(endTime, Number.secondsToMillisecondsString(endTime - startTime)));
            return row;
        }

        listElement.appendChild(createRow(WI.UIString("Scheduled"), startTime, domainLookupStart || connectStart || requestStart, "queue"));
        if (domainLookupStart)
            listElement.appendChild(createRow(WI.UIString("DNS"), domainLookupStart, domainLookupEnd || connectStart || requestStart, "dns"));
        if (connectStart)
            listElement.appendChild(createRow(WI.UIString("TCP"), connectStart, connectEnd || requestStart, "connect"));
        if (secureConnectionStart)
            listElement.appendChild(createRow(WI.UIString("Secure"), secureConnectionStart, connectEnd || requestStart, "secure"));
        listElement.appendChild(createRow(WI.UIString("Request"), requestStart, responseStart, "request"));
        listElement.appendChild(createRow(WI.UIString("Response"), responseStart, responseEnd, "response"));

        let totalRow = createRow(WI.UIString("Total"), startTime, responseEnd, "total");
        listElement.appendChild(totalRow);
        totalRow.classList.add("total");
    }

    _resourceMetricsDidChange(event)
    {
        this._needsTimingRefresh = true;
        this.needsLayout();
    }

    _resourceTimestampsDidChange(event)
    {
        this._needsTimingRefresh = true;
        this.needsLayout();
    }
};
