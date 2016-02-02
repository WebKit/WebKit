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

WebInspector.TimelineTabContentView = class TimelineTabContentView extends WebInspector.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let {image, title} = WebInspector.TimelineTabContentView.tabInfo();
        let tabBarItem = new WebInspector.TabBarItem(image, title);
        let detailsSidebarPanels = [WebInspector.resourceDetailsSidebarPanel, WebInspector.probeDetailsSidebarPanel];

        super(identifier || "timeline", "timeline", tabBarItem, WebInspector.TimelineSidebarPanel, detailsSidebarPanels);
    }

    // Static

    static tabInfo()
    {
        return {
            image: "Images/Timeline.svg",
            title: WebInspector.UIString("Timelines"),
        };
    }

    static isTabAllowed()
    {
        return !!window.TimelineAgent || !!window.ScriptProfilerAgent;
    }

    static displayNameForTimeline(timeline)
    {
        switch (timeline.type) {
        case WebInspector.TimelineRecord.Type.Network:
            return WebInspector.UIString("Network Requests");
        case WebInspector.TimelineRecord.Type.Layout:
            return WebInspector.UIString("Layout & Rendering");
        case WebInspector.TimelineRecord.Type.Script:
            return WebInspector.UIString("JavaScript & Events");
        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return WebInspector.UIString("Rendering Frames");
        case WebInspector.TimelineRecord.Type.Memory:
            return WebInspector.UIString("Memory");
        default:
            console.error("Unknown Timeline type:", timeline.type);
        }

        return null;
    }

    static iconClassNameForTimeline(timeline)
    {
        switch (timeline.type) {
        case WebInspector.TimelineRecord.Type.Network:
            return "network-icon";
        case WebInspector.TimelineRecord.Type.Layout:
            return "colors-icon";
        case WebInspector.TimelineRecord.Type.Memory:
            // FIXME: Need a new icon. For now fall through to the Script icon.
        case WebInspector.TimelineRecord.Type.Script:
            return "script-icon";
        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return "rendering-frame-icon";
        default:
            console.error("Unknown Timeline type:", timeline.type);
        }

        return null;
    }

    static genericClassNameForTimeline(timeline)
    {
        switch (timeline.type) {
        case WebInspector.TimelineRecord.Type.Network:
            return "network";
        case WebInspector.TimelineRecord.Type.Layout:
            return "colors";
        case WebInspector.TimelineRecord.Type.Memory:
            return "memory";
        case WebInspector.TimelineRecord.Type.Script:
            return "script";
        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return "rendering-frame";
        default:
            console.error("Unknown Timeline type:", timeline.type);
        }

        return null;
    }

    static iconClassNameForRecord(timelineRecord)
    {
        switch (timelineRecord.type) {
        case WebInspector.TimelineRecord.Type.Layout:
            switch (timelineRecord.eventType) {
            case WebInspector.LayoutTimelineRecord.EventType.InvalidateStyles:
            case WebInspector.LayoutTimelineRecord.EventType.RecalculateStyles:
                return WebInspector.TimelineRecordTreeElement.StyleRecordIconStyleClass;
            case WebInspector.LayoutTimelineRecord.EventType.InvalidateLayout:
            case WebInspector.LayoutTimelineRecord.EventType.ForcedLayout:
            case WebInspector.LayoutTimelineRecord.EventType.Layout:
                return WebInspector.TimelineRecordTreeElement.LayoutRecordIconStyleClass;
            case WebInspector.LayoutTimelineRecord.EventType.Paint:
                return WebInspector.TimelineRecordTreeElement.PaintRecordIconStyleClass;
            case WebInspector.LayoutTimelineRecord.EventType.Composite:
                return WebInspector.TimelineRecordTreeElement.CompositeRecordIconStyleClass;
            default:
                console.error("Unknown LayoutTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
            }

            break;

        case WebInspector.TimelineRecord.Type.Script:
            switch (timelineRecord.eventType) {
            case WebInspector.ScriptTimelineRecord.EventType.APIScriptEvaluated:
                return WebInspector.TimelineRecordTreeElement.APIRecordIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated:
                return WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.MicrotaskDispatched:
            case WebInspector.ScriptTimelineRecord.EventType.EventDispatched:
                return WebInspector.TimelineRecordTreeElement.EventRecordIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
                return WebInspector.TimelineRecordTreeElement.ProbeRecordIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.ConsoleProfileRecorded:
                return WebInspector.TimelineRecordTreeElement.ConsoleProfileIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.GarbageCollected:
                return WebInspector.TimelineRecordTreeElement.GarbageCollectionIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.TimerInstalled:
                return WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.TimerFired:
            case WebInspector.ScriptTimelineRecord.EventType.TimerRemoved:
                return WebInspector.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired:
            case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameRequested:
            case WebInspector.ScriptTimelineRecord.EventType.AnimationFrameCanceled:
                return WebInspector.TimelineRecordTreeElement.AnimationRecordIconStyleClass;
            default:
                console.error("Unknown ScriptTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
            }

            break;

        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return WebInspector.TimelineRecordTreeElement.RenderingFrameRecordIconStyleClass;

        case WebInspector.TimelineRecord.Type.Memory:
            // Not used. Fall through to error just in case.

        default:
            console.error("Unknown TimelineRecord type: " + timelineRecord.type, timelineRecord);
        }

        return null;
    }

    static displayNameForRecord(timelineRecord, includeDetailsInMainTitle)
    {
        switch (timelineRecord.type) {
        case WebInspector.TimelineRecord.Type.Network:
            return WebInspector.displayNameForURL(timelineRecord.resource.url, timelineRecord.resource.urlComponents);
        case WebInspector.TimelineRecord.Type.Layout:
            return WebInspector.LayoutTimelineRecord.displayNameForEventType(timelineRecord.eventType);
        case WebInspector.TimelineRecord.Type.Script:
            return WebInspector.ScriptTimelineRecord.EventType.displayName(timelineRecord.eventType, timelineRecord.details, includeDetailsInMainTitle);
        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return WebInspector.UIString("Frame %d").format(timelineRecord.frameNumber);
        case WebInspector.TimelineRecord.Type.Memory:
            // Not used. Fall through to error just in case.
        default:
            console.error("Unknown TimelineRecord type: " + timelineRecord.type, timelineRecord);
        }

        return null;
    }

    // Public

    get type()
    {
        return WebInspector.TimelineTabContentView.Type;
    }

    shown()
    {
        super.shown();

        WebInspector.timelineManager.autoCaptureOnPageLoad = true;
    }

    hidden()
    {
        super.hidden();

        WebInspector.timelineManager.autoCaptureOnPageLoad = false;
    }

    canShowRepresentedObject(representedObject)
    {
        if (representedObject instanceof WebInspector.TimelineRecording)
            return true;

        // Only support showing a resource or script if we have that represented object in the sidebar.
        if (representedObject instanceof WebInspector.Resource || representedObject instanceof WebInspector.Script)
            return !!this.navigationSidebarPanel.treeElementForRepresentedObject(representedObject);

        return false;
    }

    get supportsSplitContentBrowser()
    {
        return false;
    }
};

WebInspector.TimelineTabContentView.Type = "timeline";
