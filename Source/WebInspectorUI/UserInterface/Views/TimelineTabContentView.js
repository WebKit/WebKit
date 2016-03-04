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

        // FIXME: Remove these when the TimelineSidebarPanel is removed. https://bugs.webkit.org/show_bug.cgi?id=154973
        this.contentBrowser.navigationBar.removeNavigationItem(this._showNavigationSidebarItem);
        this.navigationSidebarPanel.hide();

        // Maintain an invisible tree outline containing tree elements for all recordings.
        // The visible recording's tree element is selected when the content view changes.
        this._recordingTreeElementMap = new Map;
        this._recordingsTreeOutline = new WebInspector.TreeOutline;
        this._recordingsTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._recordingsTreeSelectionDidChange, this);

        this._toggleRecordingShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Space, this._toggleRecordingOnSpacebar.bind(this));
        this._toggleRecordingShortcut.implicitlyPreventsDefault = false;
        this._toggleRecordingShortcut.disabled = true;

        this._toggleNewRecordingShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Space, this._toggleNewRecordingOnSpacebar.bind(this));
        this._toggleNewRecordingShortcut.implicitlyPreventsDefault = false;
        this._toggleNewRecordingShortcut.disabled = true;

        let toolTip = WebInspector.UIString("Start recording (%s)\nCreate new recording (%s)").format(this._toggleRecordingShortcut.displayName, this._toggleNewRecordingShortcut.displayName);
        let altToolTip = WebInspector.UIString("Stop recording (%s)").format(this._toggleRecordingShortcut.displayName);
        this._recordButton = new WebInspector.ToggleButtonNavigationItem("record-start-stop", toolTip, altToolTip, "Images/Record.svg", "Images/Stop.svg", 13, 13);
        this._recordButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._recordButtonClicked, this);

        this.contentBrowser.navigationBar.insertNavigationItem(this._recordButton, 0);

        if (WebInspector.FPSInstrument.supported()) {
            let timelinesNavigationItem = new WebInspector.RadioButtonNavigationItem(WebInspector.TimelineOverview.ViewMode.Timelines, WebInspector.UIString("Events"));
            let renderingFramesNavigationItem = new WebInspector.RadioButtonNavigationItem(WebInspector.TimelineOverview.ViewMode.RenderingFrames, WebInspector.UIString("Frames"));

            this.contentBrowser.navigationBar.insertNavigationItem(timelinesNavigationItem, 1);
            this.contentBrowser.navigationBar.insertNavigationItem(renderingFramesNavigationItem, 2);

            this.contentBrowser.navigationBar.addEventListener(WebInspector.NavigationBar.Event.NavigationItemSelected, this._viewModeSelected, this);
        }

        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingCreated, this._recordingCreated, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingLoaded, this._recordingLoaded, this);

        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStartedOrStopped, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStartedOrStopped, this);

        this._displayedRecording = null;
        this._displayedContentView = null;
        this._viewMode = null;
        this._previousSelectedTimelineType = null;

        const selectedByUser = false;
        this._changeViewMode(WebInspector.TimelineOverview.ViewMode.Timelines, selectedByUser);

        for (let recording of WebInspector.timelineManager.recordings)
            this._addRecording(recording);

        this._recordingCountChanged();

        if (WebInspector.timelineManager.activeRecording)
            this._recordingLoaded();

        // Explicitly update the path for the navigation bar to prevent it from showing up as blank.
        this.contentBrowser.updateHierarchicalPathForCurrentContentView();
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
            return "layout-icon";
        case WebInspector.TimelineRecord.Type.Memory:
            return "memory-icon";
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

        this._toggleRecordingShortcut.disabled = false;
        this._toggleNewRecordingShortcut.disabled = false;

        WebInspector.timelineManager.autoCaptureOnPageLoad = true;
    }

    hidden()
    {
        super.hidden();

        this._toggleRecordingShortcut.disabled = true;
        this._toggleNewRecordingShortcut.disabled = true;

        WebInspector.timelineManager.autoCaptureOnPageLoad = false;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WebInspector.TimelineRecording;
    }

    get supportsSplitContentBrowser()
    {
        return false;
    }

    // Protected

    restoreFromCookie(cookie)
    {
        console.assert(cookie);
        console.assert(this._displayedContentView);

        this._restoredShowingTimelineRecordingContentView = cookie[WebInspector.TimelineTabContentView.ShowingTimelineRecordingContentViewCookieKey];
        if (!this._restoredShowingTimelineRecordingContentView)
            return;

        let selectedTimelineViewIdentifier = cookie[WebInspector.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey];
        if (selectedTimelineViewIdentifier === WebInspector.TimelineRecord.Type.RenderingFrame && !WebInspector.FPSInstrument.supported())
            selectedTimelineViewIdentifier = null;

        this._showTimelineViewForType(selectedTimelineViewIdentifier);

        super.restoreFromCookie(cookie);
    }

    saveToCookie(cookie)
    {
        console.assert(cookie);

        cookie[WebInspector.TimelineTabContentView.ShowingTimelineRecordingContentViewCookieKey] = this.contentBrowser.currentContentView instanceof WebInspector.TimelineRecordingContentView;

        if (this._viewMode === WebInspector.TimelineOverview.ViewMode.RenderingFrames)
            cookie[WebInspector.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineRecord.Type.RenderingFrame;
        else {
            let selectedTimeline = this._getTimelineForCurrentContentView();
            if (selectedTimeline)
                cookie[WebInspector.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey] = selectedTimeline.type;
            else
                cookie[WebInspector.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineTabContentView.OverviewTimelineIdentifierCookieValue;
        }

        super.saveToCookie(cookie);
    }

    treeElementForRepresentedObject(representedObject)
    {
        // This can be called by the base class constructor before the map is created.
        if (!this._recordingTreeElementMap)
            return null;

        if (representedObject instanceof WebInspector.TimelineRecording)
            return this._recordingTreeElementMap.get(representedObject);

        return null;
    }

    // Private

    _capturingStartedOrStopped(event)
    {
        let isCapturing = WebInspector.timelineManager.isCapturing();
        this._recordButton.toggled = isCapturing;
    }

    _toggleRecordingOnSpacebar(event)
    {
        if (WebInspector.isEventTargetAnEditableField(event))
            return;

        this._toggleRecording();

        event.preventDefault();
    }

    _toggleNewRecordingOnSpacebar(event)
    {
        if (WebInspector.isEventTargetAnEditableField(event))
            return;

        this._toggleRecording(true);

        event.preventDefault();
    }

    _toggleRecording(shouldCreateRecording)
    {
        let isCapturing = WebInspector.timelineManager.isCapturing();
        this._recordButton.toggled = isCapturing;

        if (isCapturing)
            WebInspector.timelineManager.stopCapturing();
        else {
            WebInspector.timelineManager.startCapturing(shouldCreateRecording);
            // Show the timeline to which events will be appended.
            this._recordingLoaded();
        }
    }

    _recordButtonClicked(event)
    {
        this._recordButton.toggled = !WebInspector.timelineManager.isCapturing();
        this._toggleRecording(event.shiftKey);
    }

    _recordingsTreeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        console.assert(treeElement.representedObject instanceof WebInspector.TimelineRecording);

        this._recordingSelected(treeElement.representedObject);
    }

    _recordingCreated(event)
    {
        this._addRecording(event.data.recording)
        this._recordingCountChanged();
    }

    _addRecording(recording)
    {
        console.assert(recording instanceof WebInspector.TimelineRecording, recording);

        let recordingTreeElement = new WebInspector.GeneralTreeElement(WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass, recording.displayName, null, recording);
        this._recordingTreeElementMap.set(recording, recordingTreeElement);
        this._recordingsTreeOutline.appendChild(recordingTreeElement);
    }

    _recordingCountChanged()
    {
        let previousTreeElement = null;
        for (let treeElement of this._recordingTreeElementMap.values()) {
            if (previousTreeElement) {
                previousTreeElement.nextSibling = treeElement;
                treeElement.previousSibling = previousTreeElement;
            }

            previousTreeElement = treeElement;
        }
    }

    _recordingSelected(recording)
    {
        console.assert(recording instanceof WebInspector.TimelineRecording, recording);

        this._displayedRecording = recording;

        // Save the current state incase we need to restore it to a new recording.
        let cookie = {};
        this.saveToCookie(cookie);

        if (this._displayedContentView)
            this._displayedContentView.removeEventListener(WebInspector.ContentView.Event.NavigationItemsDidChange, this._displayedContentViewNavigationItemsDidChange, this);

        // Try to get the recording content view if it exists already, if it does we don't want to restore the cookie.
        let onlyExisting = true;
        this._displayedContentView = this.contentBrowser.contentViewForRepresentedObject(this._displayedRecording, onlyExisting);
        if (this._displayedContentView) {
            this._displayedContentView.addEventListener(WebInspector.ContentView.Event.NavigationItemsDidChange, this._displayedContentViewNavigationItemsDidChange, this);

            // Show the timeline that was being shown to update the sidebar tree state.
            let currentTimelineView = this._displayedContentView.currentTimelineView;
            let timelineType = currentTimelineView && currentTimelineView.representedObject instanceof WebInspector.Timeline ? currentTimelineView.type : null;
            this._showTimelineViewForType(timelineType);

            return;
        }

        onlyExisting = false;
        this._displayedContentView = this.contentBrowser.contentViewForRepresentedObject(this._displayedRecording, onlyExisting);
        if (this._displayedContentView)
            this._displayedContentView.addEventListener(WebInspector.ContentView.Event.NavigationItemsDidChange, this._displayedContentViewNavigationItemsDidChange, this);

        // Restore the cookie to carry over the previous recording view state to the new recording.
        this.restoreFromCookie(cookie);
    }

    _recordingLoaded(event)
    {
        this._recordingSelected(WebInspector.timelineManager.activeRecording);
    }

    _viewModeSelected(event)
    {
        let selectedNavigationItem = event.target.selectedNavigationItem;
        console.assert(selectedNavigationItem);
        if (!selectedNavigationItem)
            return;

        const selectedByUser = true;
        this._changeViewMode(selectedNavigationItem.identifier, selectedByUser);
    }

    _changeViewMode(mode, selectedByUser)
    {
        console.assert(WebInspector.FPSInstrument.supported());

        if (this._viewMode === mode)
            return;

        this._viewMode = mode;
        this.contentBrowser.navigationBar.selectedNavigationItem = this._viewMode;

        if (!selectedByUser)
            return;

        let timelineType = this._previousSelectedTimelineType;
        if (this._viewMode === WebInspector.TimelineOverview.ViewMode.RenderingFrames) {
            let timeline = this._getTimelineForCurrentContentView();
            this._previousSelectedTimelineType = timeline ? timeline.type : null;
            timelineType = WebInspector.TimelineRecord.Type.RenderingFrame;
        }

        this._showTimelineViewForType(timelineType);
    }

    _showTimelineViewForType(timelineType)
    {
        let timeline = timelineType ? this._displayedRecording.timelines.get(timelineType) : null;
        if (timeline)
            this._displayedContentView.showTimelineViewForTimeline(timeline);
        else
            this._displayedContentView.showOverviewTimelineView();

        if (this.contentBrowser.currentContentView !== this._displayedContentView)
            this.contentBrowser.showContentView(this._displayedContentView);
    }

    _displayedContentViewNavigationItemsDidChange(event)
    {
        let timeline = this._getTimelineForCurrentContentView();
        let newViewMode = WebInspector.TimelineOverview.ViewMode.Timelines;
        if (timeline && timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            newViewMode = WebInspector.TimelineOverview.ViewMode.RenderingFrames;

        const selectedByUser = false;
        this._changeViewMode(newViewMode, selectedByUser);
    }

    _getTimelineForCurrentContentView()
    {
        let currentContentView = this.contentBrowser.currentContentView;
        if (!(currentContentView instanceof WebInspector.TimelineRecordingContentView))
            return null;

        let timelineView = currentContentView.currentTimelineView;
        return (timelineView && timelineView.representedObject instanceof WebInspector.Timeline) ? timelineView.representedObject : null;
    }
};

WebInspector.TimelineTabContentView.Type = "timeline";

WebInspector.TimelineTabContentView.ShowingTimelineRecordingContentViewCookieKey = "timeline-sidebar-panel-showing-timeline-recording-content-view";
WebInspector.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey = "timeline-sidebar-panel-selected-timeline-view-identifier";
WebInspector.TimelineTabContentView.OverviewTimelineIdentifierCookieValue = "overview";
