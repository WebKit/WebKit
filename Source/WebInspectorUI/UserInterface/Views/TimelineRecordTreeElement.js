/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

WebInspector.TimelineRecordTreeElement = class TimelineRecordTreeElement extends WebInspector.GeneralTreeElement
{
    constructor(timelineRecord, subtitleNameStyle, includeDetailsInMainTitle, sourceCodeLocation, representedObject)
    {
        console.assert(timelineRecord);

        sourceCodeLocation = sourceCodeLocation || timelineRecord.sourceCodeLocation || null;

        var title = "";
        var subtitle = null;
        var alternateSubtitle = null;

        if (sourceCodeLocation) {
            subtitle = document.createElement("span");

            if (subtitleNameStyle !== WebInspector.SourceCodeLocation.NameStyle.None)
                sourceCodeLocation.populateLiveDisplayLocationString(subtitle, "textContent", null, subtitleNameStyle);
            else
                sourceCodeLocation.populateLiveDisplayLocationString(subtitle, "textContent", null, WebInspector.SourceCodeLocation.NameStyle.None, WebInspector.UIString("line "));
        }

        var iconStyleClass = null;

        switch (timelineRecord.type) {
        case WebInspector.TimelineRecord.Type.Layout:
            title = WebInspector.LayoutTimelineRecord.displayNameForEventType(timelineRecord.eventType);

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
            case WebInspector.LayoutTimelineRecord.EventType.Composite:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.CompositeRecordIconStyleClass;
                break;
            default:
                console.error("Unknown LayoutTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
            }

            break;

        case WebInspector.TimelineRecord.Type.Script:
            title = WebInspector.ScriptTimelineRecord.EventType.displayName(timelineRecord.eventType, timelineRecord.details, includeDetailsInMainTitle);

            switch (timelineRecord.eventType) {
            case WebInspector.ScriptTimelineRecord.EventType.APIScriptEvaluated:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.APIRecordIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.MicrotaskDispatched:
            case WebInspector.ScriptTimelineRecord.EventType.EventDispatched:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.ProbeRecordIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.ConsoleProfileRecorded:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.ConsoleProfileIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.GarbageCollected:
                iconStyleClass = WebInspector.TimelineRecordTreeElement.GarbageCollectionIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.TimerInstalled:
                if (includeDetailsInMainTitle) {
                    let timeoutString =  Number.secondsToString(timelineRecord.details.timeout / 1000);
                    alternateSubtitle = document.createElement("span");
                    alternateSubtitle.classList.add("alternate-subtitle");
                    if (timelineRecord.details.repeating)
                        alternateSubtitle.textContent = WebInspector.UIString("%s interval").format(timeoutString);
                    else
                        alternateSubtitle.textContent = WebInspector.UIString("%s delay").format(timeoutString);
                }

                iconStyleClass = WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
                break;
            case WebInspector.ScriptTimelineRecord.EventType.TimerFired:
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

        case WebInspector.TimelineRecord.Type.RenderingFrame:
            title = WebInspector.UIString("Frame %d").format(timelineRecord.frameNumber);
            iconStyleClass = WebInspector.TimelineRecordTreeElement.RenderingFrameRecordIconStyleClass;
            break;

        default:
            console.error("Unknown TimelineRecord type: " + timelineRecord.type, timelineRecord);
        }

        super([iconStyleClass], title, subtitle, representedObject || timelineRecord, false);

        this._record = timelineRecord;
        this._sourceCodeLocation = sourceCodeLocation;

        this.small = true;

        if (this._sourceCodeLocation)
            this.tooltipHandledSeparately = true;

        if (alternateSubtitle)
            this.titlesElement.appendChild(alternateSubtitle);
    }

    // Public

    get record()
    {
        return this._record;
    }

    get filterableData()
    {
        var url = this._sourceCodeLocation ? this._sourceCodeLocation.sourceCode.url : "";
        return {text: [this.mainTitle, url || "", this._record.details || ""]};
    }

    get sourceCodeLocation()
    {
        return this._sourceCodeLocation;
    }

    // Protected

    onattach()
    {
        super.onattach();

        console.assert(this.element);

        if (!this.tooltipHandledSeparately)
            return;

        var tooltipPrefix = this.mainTitle + "\n";
        this._sourceCodeLocation.populateLiveDisplayLocationTooltip(this.element, tooltipPrefix);
    }
};

WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass = "style-record";
WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass = "layout-record";
WebInspector.TimelineRecordTreeElement.PaintRecordIconStyleClass = "paint-record";
WebInspector.TimelineRecordTreeElement.CompositeRecordIconStyleClass = "composite-record";
WebInspector.TimelineRecordTreeElement.RenderingFrameRecordIconStyleClass = "rendering-frame-record";
WebInspector.TimelineRecordTreeElement.APIRecordIconStyleClass = "api-record";
WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass = "evaluated-record";
WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass = "event-record";
WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass = "timer-record";
WebInspector.TimelineRecordTreeElement.AnimationRecordIconStyleClass = "animation-record";
WebInspector.TimelineRecordTreeElement.ProbeRecordIconStyleClass = "probe-record";
WebInspector.TimelineRecordTreeElement.ConsoleProfileIconStyleClass = "console-profile-record";
WebInspector.TimelineRecordTreeElement.GarbageCollectionIconStyleClass = "garbage-collection-profile-record";
