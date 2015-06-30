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

WebInspector.RenderingFrameTimelineRecord = class RenderingFrameTimelineRecord extends WebInspector.TimelineRecord
{
    constructor(startTime, endTime, children)
    {
        super(WebInspector.TimelineRecord.Type.RenderingFrame, startTime, endTime);

        this._children = children || [];
        this._durationByTaskType = new Map;
        this._frameIndex = WebInspector.RenderingFrameTimelineRecord._nextFrameIndex++;
    }

    // Static

    static resetFrameIndex()
    {
        WebInspector.RenderingFrameTimelineRecord._nextFrameIndex = 0;
    }

    static displayNameForTaskType(taskType)
    {
        switch(taskType) {
        case WebInspector.RenderingFrameTimelineRecord.TaskType.Script:
            return WebInspector.UIString("Script");
        case WebInspector.RenderingFrameTimelineRecord.TaskType.Layout:
            return WebInspector.UIString("Layout");
        case WebInspector.RenderingFrameTimelineRecord.TaskType.Paint:
            return WebInspector.UIString("Paint");
        case WebInspector.RenderingFrameTimelineRecord.TaskType.Other:
            return WebInspector.UIString("Other");
        }
    }

    // Public

    get frameIndex()
    {
        return this._frameIndex;
    }

    get frameNumber()
    {
        return this._frameIndex + 1;
    }

    get children()
    {
        return this._children.slice();
    }

    durationForTask(taskType)
    {
        if (this._durationByTaskType.has(taskType))
            return this._durationByTaskType.get(taskType);

        function validRecordForTaskType(record)
        {
            switch(taskType) {
            case WebInspector.RenderingFrameTimelineRecord.TaskType.Script:
                return record.type === WebInspector.TimelineRecord.Type.Script;
            case WebInspector.RenderingFrameTimelineRecord.TaskType.Layout:
                return record.type === WebInspector.TimelineRecord.Type.Layout && record.eventType !== WebInspector.LayoutTimelineRecord.EventType.Paint && record.eventType !== WebInspector.LayoutTimelineRecord.EventType.Composite;
            case WebInspector.RenderingFrameTimelineRecord.TaskType.Paint:
                return record.eventType === WebInspector.LayoutTimelineRecord.EventType.Paint || record.eventType === WebInspector.LayoutTimelineRecord.EventType.Composite;
            default:
                console.error("Unsupported task type: " + taskType);
                return false;
            }
        }

        var duration;
        if (taskType === WebInspector.RenderingFrameTimelineRecord.TaskType.Other)
            duration = this._calculateDurationRemainder();
        else {
            duration = this._children.reduce(function(previousValue, currentValue) {
                if (!validRecordForTaskType(currentValue))
                    return previousValue;

                var currentDuration = currentValue.duration;
                if (currentValue.usesActiveStartTime)
                    currentDuration -= currentValue.inactiveDuration;
                return previousValue + currentDuration;
            }, 0);

            if (taskType === WebInspector.RenderingFrameTimelineRecord.TaskType.Script) {
                // Layout events synchronously triggered from JavaScript must be subtracted from the total
                // script time, to prevent the time from being counted twice.
                duration -= this._children.reduce(function(previousValue, currentValue) {
                    if (currentValue.type === WebInspector.TimelineRecord.Type.Layout && (currentValue.sourceCodeLocation || currentValue.callFrames))
                        return previousValue + currentValue.duration;
                    return previousValue;
                }, 0);
            } else if (taskType === WebInspector.RenderingFrameTimelineRecord.TaskType.Paint) {
                // Paint events triggered during a Composite event must be subtracted from the total painting time,
                // since the compositing time already includes time spent in these Paint events.
                var paintRecords = this._children.filter(function(record) { return record.eventType === WebInspector.LayoutTimelineRecord.EventType.Paint; });
                duration -= paintRecords.reduce(function(previousValue, currentValue) {
                    return currentValue.duringComposite ? previousValue + currentValue.duration : previousValue;
                }, 0);
            }
        }

        this._durationByTaskType.set(taskType, duration);
        return duration;
    }

    // Private

    _calculateDurationRemainder()
    {
        return Object.keys(WebInspector.RenderingFrameTimelineRecord.TaskType).reduce(function(previousValue, key) {
            var taskType = WebInspector.RenderingFrameTimelineRecord.TaskType[key];
            if (taskType === WebInspector.RenderingFrameTimelineRecord.TaskType.Other)
                return previousValue;
            return previousValue - this.durationForTask(taskType);
        }.bind(this), this.duration);
    }
};

WebInspector.RenderingFrameTimelineRecord.TaskType = {
    Script: "rendering-frame-timeline-record-script",
    Layout: "rendering-frame-timeline-record-layout",
    Paint: "rendering-frame-timeline-record-paint",
    Other: "rendering-frame-timeline-record-other"
};

WebInspector.RenderingFrameTimelineRecord.TypeIdentifier = "rendering-frame-timeline-record";

WebInspector.RenderingFrameTimelineRecord._nextFrameIndex = 0;
