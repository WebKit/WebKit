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

WI.ResourceTimingBreakdownView = class ResourceTimingBreakdownView extends WI.View
{
    constructor(resource)
    {
        super(null);

        console.assert(resource.timingData.startTime && resource.timingData.responseEnd, "Timing breakdown view requires a resource with timing data.");

        this._resource = resource;

        this.element.classList.add("resource-timing-breakdown");
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        const graphWidth = 250;
        const graphStartOffset = 25;

        let {startTime, domainLookupStart, domainLookupEnd, connectStart, connectEnd, secureConnectionStart, requestStart, responseStart, responseEnd} = this._resource.timingData;
        let graphStartTime = startTime;
        let graphEndTime = responseEnd;
        let secondsPerPixel = (responseEnd - startTime) / graphWidth;

        let waterfallElement = this.element.appendChild(document.createElement("div"));
        waterfallElement.className = "waterfall";

        function appendBlock(startTime, endTime, className) {
            let startOffset = graphStartOffset + ((startTime - graphStartTime) / secondsPerPixel);
            let width = (endTime - startTime) / secondsPerPixel;
            let block = waterfallElement.appendChild(document.createElement("div"));
            block.classList.add("block", className);
            let styleAttribute = WI.resolvedLayoutDirection() === WI.LayoutDirection.LTR ? "left" : "right";
            block.style[styleAttribute] = startOffset + "px";
            block.style.width = width + "px";
        }

        if (domainLookupStart) {
            appendBlock(startTime, domainLookupStart, "queue");
            appendBlock(domainLookupStart, connectStart || requestStart, "dns");
        } else if (connectStart)
            appendBlock(startTime, connectStart, "queue");
        else if (requestStart)
            appendBlock(startTime, requestStart, "queue");
        if (connectStart)
            appendBlock(connectStart, connectEnd, "connect");
        if (secureConnectionStart)
            appendBlock(secureConnectionStart, connectEnd, "secure");
        appendBlock(requestStart, responseStart, "request");
        appendBlock(responseStart, responseEnd, "response");

        let numbersSection = this.element.appendChild(document.createElement("div"));
        numbersSection.className = "numbers";

        function appendRow(label, duration, paragraphClass, swatchClass) {
            let p = numbersSection.appendChild(document.createElement("p"));
            if (paragraphClass)
                p.className = paragraphClass;

            if (swatchClass) {
                let swatch = p.appendChild(document.createElement("span"));
                swatch.classList.add("swatch", swatchClass);
            }

            let labelElement = p.appendChild(document.createElement("span"));
            labelElement.className = "label";
            labelElement.textContent = label;

            p.append(": ");

            let durationElement = p.appendChild(document.createElement("span"));
            durationElement.className = "duration";
            durationElement.textContent = Number.secondsToMillisecondsString(duration);
        }

        let scheduledDuration = (domainLookupStart || connectStart || requestStart) - startTime;
        let connectionDuration = (connectEnd || requestStart) - (domainLookupStart || connectStart || connectEnd || requestStart);
        let requestResponseDuration = responseEnd - requestStart;

        appendRow(WI.UIString("Scheduled"), scheduledDuration);
        if (connectionDuration) {
            appendRow(WI.UIString("Connection"), connectionDuration);
            if (domainLookupStart)
                appendRow(WI.UIString("DNS"), (domainLookupEnd || connectStart) - domainLookupStart, "sub", "dns");
            appendRow(WI.UIString("TCP"), connectEnd - connectStart, "sub", "connect");
            if (secureConnectionStart)
                appendRow(WI.UIString("Secure"), connectEnd - secureConnectionStart, "sub", "secure");
        }
        appendRow(WI.UIString("Request & Response"), responseEnd - requestStart);
        appendRow(WI.UIString("Waiting"), responseStart - requestStart, "sub", "request");
        appendRow(WI.UIString("Response"), responseEnd - responseStart, "sub", "response");
        appendRow(WI.UIString("Total"), responseEnd - startTime, "total");
    }
};
