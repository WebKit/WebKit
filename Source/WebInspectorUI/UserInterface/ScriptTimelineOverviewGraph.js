/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.ScriptTimelineOverviewGraph = function(recording)
{
    WebInspector.TimelineOverviewGraph.call(this, recording);

    this.element.classList.add(WebInspector.ScriptTimelineOverviewGraph.StyleClassName);

    this._scriptTimeline = recording.timelines.get(WebInspector.TimelineRecord.Type.Script);
    this._scriptTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._scriptTimelineRecordAdded, this);

    this._timelineRecordBars = [];

    this.reset();
};

WebInspector.ScriptTimelineOverviewGraph.StyleClassName = "script";

WebInspector.ScriptTimelineOverviewGraph.prototype = {
    constructor: WebInspector.ScriptTimelineOverviewGraph,
    __proto__: WebInspector.TimelineOverviewGraph.prototype,

    // Public

    reset: function()
    {
        WebInspector.TimelineOverviewGraph.prototype.reset.call(this);

        this._timelineRecordBarMap = new Map;

        this.element.removeChildren();
    },

    updateLayout: function()
    {
        WebInspector.TimelineOverviewGraph.prototype.updateLayout.call(this);

        var visibleWidth = this.element.offsetWidth;
        var secondsPerPixel = (this.endTime - this.startTime) / visibleWidth;

        var recordBarIndex = 0;

        function createBar(records, renderMode)
        {
            var timelineRecordBar = this._timelineRecordBars[recordBarIndex];
            if (!timelineRecordBar)
                timelineRecordBar = this._timelineRecordBars[recordBarIndex] = new WebInspector.TimelineRecordBar;
            timelineRecordBar.renderMode = renderMode;
            timelineRecordBar.records = records;
            timelineRecordBar.refresh(this);
            if (!timelineRecordBar.element.parentNode)
                this.element.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        }

        WebInspector.TimelineRecordBar.createCombinedBars(this._scriptTimeline.records, secondsPerPixel, this, createBar.bind(this));

        // Remove the remaining unused TimelineRecordBars.
        for (; recordBarIndex < this._timelineRecordBars.length; ++recordBarIndex) {
            this._timelineRecordBars[recordBarIndex].records = null;
            this._timelineRecordBars[recordBarIndex].element.remove();
        }
    },

    // Private

    _scriptTimelineRecordAdded: function(event)
    {
        this.needsLayout();
    }
};
