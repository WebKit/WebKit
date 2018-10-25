/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.DOMEventsBreakdownView = class DOMEventsBreakdownView extends WI.View
{
    constructor(domEvents, {includeGraph, startTimestamp} = {})
    {
        super();

        this._domEvents = domEvents;
        this._includeGraph = includeGraph || false;
        this._startTimestamp = startTimestamp || 0;

        this._tableBodyElement = null;

        this.element.classList.add("dom-events-breakdown");
    }

    // Public

    addEvent(domEvent)
    {
        this._domEvents.push(domEvent);

        this.soon._populateTable();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let tableElement = this.element.appendChild(document.createElement("table"));

        let headElement = tableElement.appendChild(document.createElement("thead"));

        let headRowElement = headElement.appendChild(document.createElement("tr"));

        let eventHeadCell = headRowElement.appendChild(document.createElement("th"));
        eventHeadCell.textContent = WI.UIString("Event");

        if (this._includeGraph)
            headRowElement.appendChild(document.createElement("th"));

        let timeHeadCell = headRowElement.appendChild(document.createElement("th"));
        timeHeadCell.classList.add("time");
        timeHeadCell.textContent = WI.UIString("Time");

        let originatorHeadCell = headRowElement.appendChild(document.createElement("th"));
        originatorHeadCell.classList.add("originator");
        originatorHeadCell.textContent = WI.UIString("Originator");

        this._tableBodyElement = tableElement.appendChild(document.createElement("tbody"));

        this._populateTable();
    }

    // Private

    _populateTable()
    {
        this._tableBodyElement.removeChildren();

        let startTimestamp = this._domEvents[0].timestamp;
        let endTimestamp = this._domEvents.lastValue.timestamp;
        let totalTime = endTimestamp - startTimestamp;
        let styleAttribute = WI.resolvedLayoutDirection() === WI.LayoutDirection.LTR ? "left" : "right";

        function percentOfTotalTime(time) {
            return time / totalTime * 100;
        }

        let fullscreenRanges = [];
        let fullscreenDOMEvents = WI.DOMNode.getFullscreenDOMEvents(this._domEvents);
        for (let fullscreenDOMEvent of fullscreenDOMEvents) {
            let {enabled} = fullscreenDOMEvent.data;
            if (enabled || !fullscreenRanges.length) {
                fullscreenRanges.push({
                    startTimestamp: enabled ? fullscreenDOMEvent.timestamp : startTimestamp,
                });
            }
            fullscreenRanges.lastValue.endTimestamp = (enabled && fullscreenDOMEvent === fullscreenDOMEvents.lastValue) ? endTimestamp : fullscreenDOMEvent.timestamp;
        }

        for (let domEvent of this._domEvents) {
            let rowElement = this._tableBodyElement.appendChild(document.createElement("tr"));

            let nameCell = rowElement.appendChild(document.createElement("td"));
            nameCell.classList.add("name");
            nameCell.textContent = domEvent.eventName;

            if (this._includeGraph) {
                let graphCell = rowElement.appendChild(document.createElement("td"));
                graphCell.classList.add("graph");

                let fullscreenRange = fullscreenRanges.find((range) => domEvent.timestamp >= range.startTimestamp && domEvent.timestamp <= range.endTimestamp);
                if (fullscreenRange) {
                    let fullscreenArea = graphCell.appendChild(document.createElement("div"));
                    fullscreenArea.classList.add("area", "fullscreen");
                    fullscreenArea.style.setProperty(styleAttribute, percentOfTotalTime(fullscreenRange.startTimestamp - startTimestamp) + "%");
                    fullscreenArea.style.setProperty("width", percentOfTotalTime(fullscreenRange.endTimestamp - fullscreenRange.startTimestamp) + "%");
                }

                let graphPoint = graphCell.appendChild(document.createElement("div"));
                graphPoint.classList.add("point");
                graphPoint.style.setProperty(styleAttribute, `calc(${percentOfTotalTime(domEvent.timestamp - startTimestamp)}% - (var(--point-size) / 2))`);
            }

            let timeCell = rowElement.appendChild(document.createElement("td"));
            timeCell.classList.add("time");

            const higherResolution = true;
            timeCell.textContent = Number.secondsToString(domEvent.timestamp - this._startTimestamp, higherResolution);

            let originatorCell = rowElement.appendChild(document.createElement("td"));
            originatorCell.classList.add("originator");
            if (domEvent.originator) {
                originatorCell.appendChild(WI.linkifyNodeReference(domEvent.originator));

                rowElement.classList.add("inherited");
                this.element.classList.add("has-inherited");
            }
        }
    }
};
