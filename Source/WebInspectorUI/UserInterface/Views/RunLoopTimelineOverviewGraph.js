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

WebInspector.RunLoopTimelineOverviewGraph = function(timeline)
{
    WebInspector.TimelineOverviewGraph.call(this, timeline);

    this.element.classList.add(WebInspector.RunLoopTimelineOverviewGraph.StyleClassName);

    this._runLoopTimeline = timeline;
    this._timelineRecordFrames = [];
    this._graphHeightSeconds = NaN;
    this._framesPerSecondDividers = new Map;

    this._runLoopTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._timelineRecordAdded, this);

    this.reset();
};

WebInspector.RunLoopTimelineOverviewGraph.StyleClassName = "runloop";
WebInspector.RunLoopTimelineOverviewGraph.MaximumGraphHeightSeconds = 0.05;

WebInspector.RunLoopTimelineOverviewGraph.prototype = {
    constructor: WebInspector.RunLoopTimelineOverviewGraph,
    __proto__: WebInspector.TimelineOverviewGraph.prototype,

    // Public

    get graphHeightSeconds()
    {
        if (!isNaN(this._graphHeightSeconds))
            return this._graphHeightSeconds;

        var maximumFrameDuration = this._runLoopTimeline.records.reduce(function(previousValue, currentValue) {
            return Math.max(previousValue, currentValue.duration);
        }, 0);

        this._graphHeightSeconds = maximumFrameDuration * 1.1;  // Add 10% margin above frames.
        this._graphHeightSeconds = Math.min(this._graphHeightSeconds, WebInspector.RunLoopTimelineOverviewGraph.MaximumGraphHeightSeconds);
        return this._graphHeightSeconds;
    },

    reset()
    {
        WebInspector.TimelineOverviewGraph.prototype.reset.call(this);

        this.element.removeChildren();

        this._framesPerSecondDividers.clear();
    },

    updateLayout()
    {
        WebInspector.TimelineOverviewGraph.prototype.updateLayout.call(this);

        var secondsPerPixel = this.timelineOverview.secondsPerPixel;

        var recordFrameIndex = 0;

        function createFrame(records)
        {
            var timelineRecordFrame = this._timelineRecordFrames[recordFrameIndex];
            if (!timelineRecordFrame)
                timelineRecordFrame = this._timelineRecordFrames[recordFrameIndex] = new WebInspector.TimelineRecordFrame(this, records);
            else
                timelineRecordFrame.records = records;

            timelineRecordFrame.refresh(this);
            if (!timelineRecordFrame.element.parentNode)
                this.element.appendChild(timelineRecordFrame.element);
            ++recordFrameIndex;
        }

        WebInspector.TimelineRecordFrame.createCombinedFrames(this._runLoopTimeline.records, secondsPerPixel, this, createFrame.bind(this));

        // Remove the remaining unused TimelineRecordFrames.
        for (; recordFrameIndex < this._timelineRecordFrames.length; ++recordFrameIndex) {
            this._timelineRecordFrames[recordFrameIndex].records = null;
            this._timelineRecordFrames[recordFrameIndex].element.remove();
        }

        this._updateDividers();
    },

    // Private

    _timelineRecordAdded(event)
    {
        this._graphHeightSeconds = NaN;

        this.needsLayout();
    },

    _updateDividers()
    {
        if (this.graphHeightSeconds === 0)
            return;

        var overviewGraphHeight = this.element.offsetHeight;

        function createDividerAtPosition(framesPerSecond)
        {
            var secondsPerFrame = 1 / framesPerSecond;
            var dividerTop = 1 - secondsPerFrame / this.graphHeightSeconds;
            if (dividerTop < 0.01 || dividerTop >= 1)
                return;

            var divider = this._framesPerSecondDividers.get(framesPerSecond);
            if (!divider) {
                divider = document.createElement("div");
                divider.classList.add("divider");

                var label = document.createElement("span");
                label.innerText = framesPerSecond + " fps";
                divider.appendChild(label);

                this.element.appendChild(divider);

                this._framesPerSecondDividers.set(framesPerSecond, divider);
            }

            divider.style.marginTop = (dividerTop * overviewGraphHeight).toFixed(2) + "px";
        }

        createDividerAtPosition.call(this, 60);
        createDividerAtPosition.call(this, 30);
    },

    _updateElementPosition(element, newPosition, property)
    {
        newPosition *= 100;
        newPosition = newPosition.toFixed(2);

        var currentPosition = parseFloat(element.style[property]).toFixed(2);
        if (currentPosition !== newPosition)
            element.style[property] = newPosition + "%";
    }
};
