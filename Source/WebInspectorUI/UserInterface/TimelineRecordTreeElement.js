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

WebInspector.TimelineRecordTreeElement = function(timelineRecord, showFullLocationSubtitle, sourceCodeLocation, representedObject)
{
    console.assert(timelineRecord);

    this._record = timelineRecord;
    this._sourceCodeLocation = sourceCodeLocation || timelineRecord.sourceCodeLocation;

    var title = "";
    var subtitle = "";

    if (this._sourceCodeLocation) {
        // FIXME: This needs to live update the subtitle in response to the WebInspector.SourceCodeLocation.Event.DisplayLocationChanged event.

        if (showFullLocationSubtitle) {
            subtitle = this._sourceCodeLocation.displayLocationString(WebInspector.SourceCodeLocation.ColumnStyle.OnlyIfLarge);
        } else {
            subtitle = WebInspector.UIString("line ") + (this._sourceCodeLocation.displayLineNumber + 1); // The user visible line number is 1-based.
            if (this._sourceCodeLocation.displayColumnNumber > WebInspector.SourceCodeLocation.LargeColumnNumber)
                subtitle += ":" + (this._sourceCodeLocation.displayColumnNumber + 1); // The user visible column number is 1-based.
        }
    }

    var iconStyleClass = null;

    switch (timelineRecord.type) {
    case WebInspector.TimelineRecord.Type.Layout:
        switch (timelineRecord.eventType) {
        case WebInspector.LayoutTimelineRecord.EventType.InvalidateStyles:
            title = WebInspector.UIString("Styles Invalidated");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.RecalculateStyles:
            title = WebInspector.UIString("Styles Recalculated");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.InvalidateLayout:
            title = WebInspector.UIString("Layout Invalidated");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.Layout:
            title = WebInspector.UIString("Layout");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.Paint:
            title = WebInspector.UIString("Paint");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.PaintRecordIconStyleClass;
            break;
        default:
            console.error("Unknown LayoutTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
        }

        break;

    case WebInspector.TimelineRecord.Type.Script:
        switch (timelineRecord.eventType) {
        case WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated:
            title = WebInspector.UIString("Script Evaluated");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.EventDispatched:
            title = WebInspector.UIString("Event Dispatched");
            if (timelineRecord.details)
                subtitle += " \u2014 " + timelineRecord.details;
            iconStyleClass = WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerFired:
            title = WebInspector.UIString("Timer Fired");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerInstalled:
            title = WebInspector.UIString("Timer Installed");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerRemoved:
            title = WebInspector.UIString("Timer Removed");
            iconStyleClass = WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired:
            title = WebInspector.UIString("Animation Frame Fired");
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
};

WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass = "style-record";
WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass = "layout-record";
WebInspector.TimelineRecordTreeElement.PaintRecordIconStyleClass = "paint-record";
WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass = "evaluated-record";
WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass = "event-record";
WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass = "timer-record";
WebInspector.TimelineRecordTreeElement.AnimationRecordIconStyleClass = "animation-record";

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
    }
};
