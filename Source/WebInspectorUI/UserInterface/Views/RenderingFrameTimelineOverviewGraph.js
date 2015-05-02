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

WebInspector.RenderingFrameTimelineOverviewGraph = function(timeline)
{
    WebInspector.TimelineOverviewGraph.call(this, timeline);

    this.element.classList.add(WebInspector.RenderingFrameTimelineOverviewGraph.StyleClassName);

    this._renderingFrameTimeline = timeline;
    this._renderingFrameTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._timelineRecordAdded, this);

    this._timelineRecordFrames = [];
    this._graphHeightSeconds = NaN;
    this._framesPerSecondDividerMap = new Map;

    this.reset();
};

WebInspector.RenderingFrameTimelineOverviewGraph.StyleClassName = "rendering-frame";
WebInspector.RenderingFrameTimelineOverviewGraph.MaximumGraphHeightSeconds = 0.037;
WebInspector.RenderingFrameTimelineOverviewGraph.MinimumGraphHeightSeconds = 0.0185;

WebInspector.RenderingFrameTimelineOverviewGraph.prototype = {
    constructor: WebInspector.RenderingFrameTimelineOverviewGraph,
    __proto__: WebInspector.TimelineOverviewGraph.prototype,

    // Public

    get graphHeightSeconds()
    {
        if (!isNaN(this._graphHeightSeconds))
            return this._graphHeightSeconds;

        var maximumFrameDuration = this._renderingFrameTimeline.records.reduce(function(previousValue, currentValue) {
            return Math.max(previousValue, currentValue.duration);
        }, 0);

        this._graphHeightSeconds = maximumFrameDuration * 1.1;  // Add 10% margin above frames.
        this._graphHeightSeconds = Math.min(this._graphHeightSeconds, WebInspector.RenderingFrameTimelineOverviewGraph.MaximumGraphHeightSeconds);
        this._graphHeightSeconds = Math.max(this._graphHeightSeconds, WebInspector.RenderingFrameTimelineOverviewGraph.MinimumGraphHeightSeconds);
        return this._graphHeightSeconds;
    },

    reset()
    {
        WebInspector.TimelineOverviewGraph.prototype.reset.call(this);

        this.element.removeChildren();

        this._framesPerSecondDividerMap.clear();
    },

    updateLayout()
    {
        WebInspector.TimelineOverviewGraph.prototype.updateLayout.call(this);

        if (!this._renderingFrameTimeline.records.length)
            return;

        var records = this._renderingFrameTimeline.records;
        var startIndex = Math.floor(this.startTime);
        var endIndex = Math.min(Math.floor(this.endTime), records.length - 1);
        var recordFrameIndex = 0;

        for (var i = startIndex; i <= endIndex; ++i) {
            var record = records[i];
            var timelineRecordFrame = this._timelineRecordFrames[recordFrameIndex];
            if (!timelineRecordFrame)
                timelineRecordFrame = this._timelineRecordFrames[recordFrameIndex] = new WebInspector.TimelineRecordFrame(this, record);
            else
                timelineRecordFrame.record = record;

            timelineRecordFrame.refresh(this);
            if (!timelineRecordFrame.element.parentNode)
                this.element.appendChild(timelineRecordFrame.element);

            ++recordFrameIndex;
        }

        // Remove the remaining unused TimelineRecordFrames.
        for (; recordFrameIndex < this._timelineRecordFrames.length; ++recordFrameIndex) {
            this._timelineRecordFrames[recordFrameIndex].record = null;
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

            var divider = this._framesPerSecondDividerMap.get(framesPerSecond);
            if (!divider) {
                divider = document.createElement("div");
                divider.classList.add("divider");

                var label = document.createElement("div");
                label.classList.add("label");
                label.innerText = framesPerSecond + " fps";
                divider.appendChild(label);

                this.element.appendChild(divider);

                this._framesPerSecondDividerMap.set(framesPerSecond, divider);
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
