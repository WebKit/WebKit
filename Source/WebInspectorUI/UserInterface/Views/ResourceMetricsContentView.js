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

WI.ResourceMetricsContentView = class ResourceMetricsContentView extends WI.ContentView
{
    constructor(resource, delegate)
    {
        super(null);

        console.assert(resource instanceof WI.Resource);
        console.assert(delegate);

        this._resource = resource;
        this._resource.addEventListener(WI.Resource.Event.SizeDidChange, this._resourceSizeDidChange, this);
        this._resource.addEventListener(WI.Resource.Event.TransferSizeDidChange, this._resourceTransferSizeDidChange, this);
        this._resource.addEventListener(WI.Resource.Event.MetricsDidChange, this._resourceMetricsDidChange, this);
        this._resource.addEventListener(WI.Resource.Event.TimestampsDidChange, this._resourceTimestampsDidChange, this);

        this._delegate = delegate;

        this.element.classList.add("resource-details", "resource-metrics");

        this._needsTransferSizesRefresh = false;
        this._needsResourceSizeRefresh = false;
        this._needsTimingRefresh = false;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let contentElement = this.element.appendChild(document.createElement("div"));
        contentElement.className = "content";

        // Network.

        let networkSection = contentElement.appendChild(document.createElement("section"));
        networkSection.className = "network split";

        function createSizeComponents(parentElement, subtitle, imageSource, label1, label2) {
            let subtitleElement = parentElement.appendChild(document.createElement("div"));
            subtitleElement.className = "subtitle";
            subtitleElement.textContent = subtitle;

            let container = parentElement.appendChild(document.createElement("div"));
            container.className = "container";

            let imageElement = container.appendChild(document.createElement("img"));
            if (imageSource)
                imageElement.src = imageSource;

            let groupElement = container.appendChild(document.createElement("div"));
            groupElement.className = "bytes-group";

            let bytesElement = groupElement.appendChild(document.createElement("div"));
            bytesElement.className = "bytes";

            let suffixElement = groupElement.appendChild(document.createElement("div"));
            suffixElement.className = "suffix";

            let table = parentElement.appendChild(document.createElement("table"));
            let headerRow = table.appendChild(document.createElement("tr"));
            let label1Element = headerRow.appendChild(document.createElement("td"));
            let value1Element = headerRow.appendChild(document.createElement("td"));
            let bodyRow = table.appendChild(document.createElement("tr"));
            let label2Element = bodyRow.appendChild(document.createElement("td"));
            let value2Element = bodyRow.appendChild(document.createElement("td"));

            label1Element.textContent = label1;
            label1Element.className = "label";
            label2Element.textContent = label2;
            label2Element.className = "label";

            return {
                container,
                bytesElement,
                suffixElement,
                imageElement,
                value1Element,
                value2Element,
            };
        }

        let sendingSection = networkSection.appendChild(document.createElement("div"));
        sendingSection.className = "subsection";

        let sendingComponents = createSizeComponents(sendingSection, WI.UIString("Bytes Sent"), "Images/Sending.svg", WI.UIString("Headers:"), WI.UIString("Body:"));
        this._sendingBytesElement = sendingComponents.bytesElement;
        this._sendingBytesSuffixElement = sendingComponents.suffixElement;
        this._sendingHeaderBytesElement = sendingComponents.value1Element;
        this._sendingBodyBytesElement = sendingComponents.value2Element;

        let bytesDivider = networkSection.appendChild(document.createElement("div"));
        bytesDivider.className = "divider";

        let receivingSection = networkSection.appendChild(document.createElement("div"));
        receivingSection.className = "subsection";

        let receivingComponents = createSizeComponents(receivingSection, WI.UIString("Bytes Received"), "Images/Receiving.svg", WI.UIString("Headers:"), WI.UIString("Body:"));
        this._receivingBytesElement = receivingComponents.bytesElement;
        this._receivingBytesSuffixElement = receivingComponents.suffixElement;
        this._receivingHeaderBytesElement = receivingComponents.value1Element;
        this._receivingBodyBytesElement = receivingComponents.value2Element;

        let resourceDivider = networkSection.appendChild(document.createElement("div"));
        resourceDivider.className = "divider";

        let resourceSection = networkSection.appendChild(document.createElement("div"));
        resourceSection.className = "subsection large";

        let resourceComponents = createSizeComponents(resourceSection, WI.UIString("Resource Size"), null, WI.UIString("Compression:"), WI.UIString("MIME Type:"));
        resourceComponents.container.classList.add(WI.ResourceTreeElement.ResourceIconStyleClassName, this._resource.type);
        resourceComponents.imageElement.classList.add("icon");
        this._resourceBytesElement = resourceComponents.bytesElement;
        this._resourceBytesSuffixElement = resourceComponents.suffixElement;
        this._compressionElement = resourceComponents.value1Element;
        this._contentTypeElement = resourceComponents.value2Element;

        // Timing.

        this._timingSection = contentElement.appendChild(document.createElement("section"));
        this._timingSection.className = "timing";

        this._timingSubtitle = document.createElement("div");
        this._timingSubtitle.className = "subtitle";
        this._timingSubtitle.textContent = WI.UIString("Timing");

        // Populate.

        this._refreshTransferSizeSections();
        this._refreshResourceSizeSection();
        this._refreshTimingSection();

        this._needsTransferSizesRefresh = false;
        this._needsResourceSizeRefresh = false;
        this._needsTimingRefresh = false;
    }

    layout()
    {
        super.layout();

        if (this._needsTransferSizesRefresh) {
            this._refreshTransferSizeSections();
            this._needsTransferSizesRefresh = false;
        }

        if (this._needsResourceSizeRefresh) {
            this._refreshResourceSizeSection();
            this._needsResourceSizeRefresh = false;
        }

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

    _sizeComponents(bytes)
    {
        console.assert(bytes >= 0);

        // Prefer KB over B. And prefer 1 decimal point to keep sizes simple
        // but we will still need B if bytes is less than 0.1 KB.
        if (bytes < 103)
            return [bytes.toFixed(0), "B"];

        let kilobytes = bytes / 1024;
        if (kilobytes < 1024)
            return [kilobytes.toFixed(1), "KB"];

        let megabytes = kilobytes / 1024;
        if (megabytes < 1024)
            return [megabytes.toFixed(1), "MB"];

        let gigabytes = megabytes / 1024;
        return [gigabytes.toFixed(1), "GB"];
    }

    _refreshTransferSizeSections()
    {
        let bytesSentHeader = this._resource.requestHeadersTransferSize;
        let bytesSentBody = this._resource.requestBodyTransferSize;
        let bytesSent = bytesSentHeader + bytesSentBody;

        let bytesReceivedHeader = this._resource.responseHeadersTransferSize;
        let bytesReceivedBody = this._resource.responseBodyTransferSize;
        let bytesReceived = bytesReceivedHeader + bytesReceivedBody;

        let [sentValue, sentSuffix] = this._sizeComponents(bytesSent || 0);
        this._sendingBytesElement.textContent = sentValue;
        this._sendingBytesSuffixElement.textContent = sentSuffix;

        this._sendingHeaderBytesElement.textContent = bytesSentHeader ? Number.bytesToString(bytesSentHeader) : emDash;
        this._sendingBodyBytesElement.textContent = bytesSentBody ? Number.bytesToString(bytesSentBody) : emDash;

        let [receivedValue, receivedSuffix] = this._sizeComponents(bytesReceived || 0);
        this._receivingBytesElement.textContent = receivedValue;
        this._receivingBytesSuffixElement.textContent = receivedSuffix;

        this._receivingHeaderBytesElement.textContent = bytesReceivedHeader ? Number.bytesToString(bytesReceivedHeader) : emDash;
        this._receivingBodyBytesElement.textContent = bytesReceivedBody ? Number.bytesToString(bytesReceivedBody) : emDash;

        function appendGoToArrow(parentElement, handler) {
            let goToButton = parentElement.appendChild(WI.createGoToArrowButton());
            goToButton.addEventListener("click", handler);
        }

        if (bytesSentHeader)
            appendGoToArrow(this._sendingHeaderBytesElement, () => { this._delegate.metricsContentViewGoToHeaders(this); });
        if (bytesSentBody)
            appendGoToArrow(this._sendingBodyBytesElement, () => { this._delegate.metricsContentViewGoToRequestBody(this); });
        if (bytesReceivedHeader)
            appendGoToArrow(this._receivingHeaderBytesElement, () => { this._delegate.metricsContentViewGoToHeaders(this); });
        if (bytesReceivedBody)
            appendGoToArrow(this._receivingBodyBytesElement, () => { this._delegate.metricsContentViewGoToResponseBody(this); });
    }

    _refreshResourceSizeSection()
    {
        let encodedSize = !isNaN(this._resource.networkEncodedSize) ? this._resource.networkEncodedSize : this._resource.estimatedNetworkEncodedSize;
        let decodedSize = !isNaN(this._resource.networkDecodedSize) ? this._resource.networkDecodedSize : this._resource.size;
        let compressionRate = decodedSize / encodedSize;
        let compressionString = compressionRate > 0 && isFinite(compressionRate) ? WI.UIString("%.2f\u00d7").format(compressionRate) : WI.UIString("None");

        let [resourceSizeValue, resourceSizeSuffix] = this._sizeComponents(decodedSize || 0);
        this._resourceBytesElement.textContent = resourceSizeValue;
        this._resourceBytesSuffixElement.textContent = resourceSizeSuffix;

        let contentEncoding = this._resource.responseHeaders.valueForCaseInsensitiveKey("Content-Encoding");
        if (contentEncoding)
            compressionString += ` (${contentEncoding.toLowerCase()})`;

        this._compressionElement.textContent = compressionString;
        this._contentTypeElement.textContent = this._resource.mimeType || emDash;

        const minimumSizeBeforeWarning = 1024;
        if (compressionRate <= 1 && encodedSize >= minimumSizeBeforeWarning && WI.shouldTreatMIMETypeAsText(this._resource.mimeType))
            this._compressionElement.appendChild(WI.ImageUtilities.useSVGSymbol("Images/Warning.svg", "warning", WI.UIString("This text resource could benefit from compression")));
    }

    _refreshTimingSection()
    {
        this._timingSection.removeChildren();

        this._timingSection.appendChild(this._timingSubtitle);

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

    _resourceSizeDidChange(event)
    {
        this._needsTransferSizesRefresh = true;
        this.needsLayout();
    }

    _resourceTransferSizeDidChange(event)
    {
        this._needsTransferSizesRefresh = true;
        this.needsLayout();
    }

    _resourceMetricsDidChange(event)
    {
        this._needsTransferSizesRefresh = true;
        this._needsResourceSizeRefresh = true;
        this._needsTimingRefresh = true;
        this.needsLayout();
    }

    _resourceTimestampsDidChange(event)
    {
        this._needsTimingRefresh = true;
        this.needsLayout();
    }
};
