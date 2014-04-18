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

WebInspector.TimelineRecordTreeElement = function(timelineRecord, subtitleNameStyle, includeTimerIdentifierInMainTitle, sourceCodeLocation, representedObject)
{
    console.assert(timelineRecord);

    this._record = timelineRecord;
    this._sourceCodeLocation = sourceCodeLocation || timelineRecord.sourceCodeLocation || null;

    var title = "";
    var subtitle = "";

    if (this._sourceCodeLocation) {
        subtitle = document.createElement("span");

        if (subtitleNameStyle !== WebInspector.SourceCodeLocation.NameStyle.None)
            this._sourceCodeLocation.populateLiveDisplayLocationString(subtitle, "textContent", null, subtitleNameStyle);
        else
            this._sourceCodeLocation.populateLiveDisplayLocationString(subtitle, "textContent", null, WebInspector.SourceCodeLocation.NameStyle.None, WebInspector.UIString("line "));
    }

    var iconStyleClass = null;

    switch (timelineRecord.type) {
    case WebInspector.TimelineRecord.Type.Layout:
        title = WebInspector.LayoutTimelineRecord.EventType.displayName(timelineRecord.eventType);

        switch (timelineRecord.eventType) {
        case WebInspector.LayoutTimelineRecord.EventType.InvalidateStyles:
        case WebInspector.LayoutTimelineRecord.EventType.RecalculateStyles:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.InvalidateLayout:
        case WebInspector.LayoutTimelineRecord.EventType.ForcedLayout:
        case WebInspector.LayoutTimelineRecord.EventType.Layout:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.Paint:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.PaintRecordIconStyleClass;
            break;
        default:
            console.error("Unknown LayoutTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
        }

        break;

    case WebInspector.TimelineRecord.Type.Script:
        title = WebInspector.ScriptTimelineRecord.EventType.displayName(timelineRecord.eventType, timelineRecord.details, includeTimerIdentifierInMainTitle);

        switch (timelineRecord.eventType) {
        case WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.EventDispatched:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.ProbeRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerFired:
        case WebInspector.ScriptTimelineRecord.EventType.TimerInstalled:
        case WebInspector.ScriptTimelineRecord.EventType.TimerRemoved:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired:
        case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameRequested:
        case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameCanceled:
            iconStyleClass = WebInspector.TimelineRecordTreeElement.AnimationRecordIconStyleClass;
            break;
        default:
            console.error("Unknown ScriptTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
        }

        break;

    default:
        console.error("Unknown TimelineRecord type: " + timelineRecord.type, timelineRecord);
    }

    WebInspector.GeneralTreeElement.call(this, [iconStyleClass], title, subtitle, representedObject || timelineRecord, false);

    this.small = true;

    if (this._sourceCodeLocation)
        this.tooltipHandledSeparately = true;
};

WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass = "style-record";
WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass = "layout-record";
WebInspector.TimelineRecordTreeElement.PaintRecordIconStyleClass = "paint-record";
WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass = "evaluated-record";
WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass = "event-record";
WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass = "timer-record";
WebInspector.TimelineRecordTreeElement.AnimationRecordIconStyleClass = "animation-record";
WebInspector.TimelineRecordTreeElement.ProbeRecordIconStyleClass = "probe-record";

WebInspector.TimelineRecordTreeElement.prototype = {
    constructor: WebInspector.TimelineRecordTreeElement,
    __proto__: WebInspector.GeneralTreeElement.prototype,

    // Public

    get record()
    {
        return this._record;
    },

    get filterableData()
    {
        var url = this._sourceCodeLocation ? this._sourceCodeLocation.sourceCode.url : "";
        return {text: [this.mainTitle, url || "", this._record.details || ""]};
    },

    // Protected

    onattach: function()
    {
        WebInspector.GeneralTreeElement.prototype.onattach.call(this);

        console.assert(this.element);

        if (!this.tooltipHandledSeparately)
            return;

        var tooltipPrefix = this.mainTitle + "\n";
        this._sourceCodeLocation.populateLiveDisplayLocationTooltip(this.element, tooltipPrefix);
    }
};
