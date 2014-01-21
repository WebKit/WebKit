/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.SourceCodeTimelineTreeElement = function(sourceCodeTimeline)
{
    console.assert(sourceCodeTimeline);

    this._sourceCodeTimeline = sourceCodeTimeline;

    var title = "";

    var subtitle = "";
    var sourceCodeLocation = sourceCodeTimeline.sourceCodeLocation;
    if (sourceCodeLocation) {
        // FIXME: This needs to live update the subtitle in response to the WebInspector.SourceCodeLocation.Event.DisplayLocationChanged event.
        subtitle = WebInspector.UIString("line ") + (sourceCodeLocation.displayLineNumber + 1); // The user visible line number is 1-based.
        if (sourceCodeLocation.displayColumnNumber > WebInspector.SourceCodeLocation.LargeColumnNumber)
            subtitle += ":" + (sourceCodeLocation.displayColumnNumber + 1); // The user visible column number is 1-based.
    }

    var iconStyleClass = null;

    switch (sourceCodeTimeline.recordType) {
    case WebInspector.TimelineRecord.Type.Layout:
        switch (sourceCodeTimeline.recordEventType) {
        case WebInspector.LayoutTimelineRecord.EventType.InvalidateStyles:
            title = WebInspector.UIString("Styles Invalidated");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.StyleRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.RecalculateStyles:
            title = WebInspector.UIString("Styles Recalculated");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.StyleRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.InvalidateLayout:
            title = WebInspector.UIString("Layout Invalidated");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.LayoutRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.Layout:
            title = WebInspector.UIString("Layout");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.LayoutRecordIconStyleClass;
            break;
        case WebInspector.LayoutTimelineRecord.EventType.Paint:
            title = WebInspector.UIString("Paint");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.PaintRecordIconStyleClass;
            break;
        }

    case WebInspector.TimelineRecord.Type.Script:
        switch (sourceCodeTimeline.recordEventType) {
        case WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated:
            title = WebInspector.UIString("Script Evaluated");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.EvaluatedRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.EventDispatched:
            title = WebInspector.UIString("Event Dispatched");
            subtitle += " \u2014 " + sourceCodeTimeline.records[0].details;
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.EventRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerFired:
            title = WebInspector.UIString("Timer Fired");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerInstalled:
            title = WebInspector.UIString("Timer Installed");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.TimerRemoved:
            title = WebInspector.UIString("Timer Removed");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.TimerRecordIconStyleClass;
            break;
        case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired:
            title = WebInspector.UIString("Animation Frame Fired");
            iconStyleClass = WebInspector.SourceCodeTimelineTreeElement.AnimationRecordIconStyleClass;
            break;
        }
    }

    WebInspector.GeneralTreeElement.call(this, [iconStyleClass], title, subtitle, sourceCodeTimeline, false);

    this.small = true;
};

WebInspector.SourceCodeTimelineTreeElement.StyleRecordIconStyleClass = "style-record";
WebInspector.SourceCodeTimelineTreeElement.LayoutRecordIconStyleClass = "layout-record";
WebInspector.SourceCodeTimelineTreeElement.PaintRecordIconStyleClass = "paint-record";
WebInspector.SourceCodeTimelineTreeElement.EvaluatedRecordIconStyleClass = "evaluated-record";
WebInspector.SourceCodeTimelineTreeElement.EventRecordIconStyleClass = "event-record";
WebInspector.SourceCodeTimelineTreeElement.TimerRecordIconStyleClass = "timer-record";
WebInspector.SourceCodeTimelineTreeElement.AnimationRecordIconStyleClass = "animation-record";

WebInspector.SourceCodeTimelineTreeElement.prototype = {
    constructor: WebInspector.SourceCodeTimelineTreeElement,
    __proto__: WebInspector.GeneralTreeElement.prototype,

    // Public

    get sourceCodeTimeline()
    {
        return this._sourceCodeTimeline;
    }
};
