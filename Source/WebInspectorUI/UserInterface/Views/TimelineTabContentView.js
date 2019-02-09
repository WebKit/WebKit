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

WI.TimelineTabContentView = class TimelineTabContentView extends WI.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.TimelineTabContentView.tabInfo());
        let detailsSidebarPanelConstructors = [WI.ResourceDetailsSidebarPanel, WI.ProbeDetailsSidebarPanel];

        super(identifier || "timeline", "timeline", tabBarItem, null, detailsSidebarPanelConstructors);

        // Maintain an invisible tree outline containing tree elements for all recordings.
        // The visible recording's tree element is selected when the content view changes.
        this._recordingTreeElementMap = new Map;
        this._recordingsTreeOutline = new WI.TreeOutline;
        this._recordingsTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._recordingsTreeSelectionDidChange, this);

        this._toggleRecordingShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Space, this._toggleRecordingOnSpacebar.bind(this));
        this._toggleRecordingShortcut.implicitlyPreventsDefault = false;
        this._toggleRecordingShortcut.disabled = true;

        this._toggleNewRecordingShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Space, this._toggleNewRecordingOnSpacebar.bind(this));
        this._toggleNewRecordingShortcut.implicitlyPreventsDefault = false;
        this._toggleNewRecordingShortcut.disabled = true;

        let toolTip = WI.UIString("Start recording (%s)\nCreate new recording (%s)").format(this._toggleRecordingShortcut.displayName, this._toggleNewRecordingShortcut.displayName);
        let altToolTip = WI.UIString("Stop recording (%s)").format(this._toggleRecordingShortcut.displayName);
        this._recordButton = new WI.ToggleButtonNavigationItem("record-start-stop", toolTip, altToolTip, "Images/Record.svg", "Images/Stop.svg", 13, 13);
        this._recordButton.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._recordButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._recordButtonClicked, this);

        this.contentBrowser.navigationBar.insertNavigationItem(this._recordButton, 0);

        if (WI.FPSInstrument.supported()) {
            let timelinesNavigationItem = new WI.RadioButtonNavigationItem(WI.TimelineOverview.ViewMode.Timelines, WI.UIString("Events"));
            let renderingFramesNavigationItem = new WI.RadioButtonNavigationItem(WI.TimelineOverview.ViewMode.RenderingFrames, WI.UIString("Frames"));

            let viewModeGroup = new WI.GroupNavigationItem([timelinesNavigationItem, renderingFramesNavigationItem]);
            viewModeGroup.visibilityPriority = WI.NavigationItem.VisibilityPriority.High;

            this.contentBrowser.navigationBar.insertNavigationItem(viewModeGroup, 1);
            this.contentBrowser.navigationBar.addEventListener(WI.NavigationBar.Event.NavigationItemSelected, this._viewModeSelected, this);
        }

        WI.timelineManager.addEventListener(WI.TimelineManager.Event.RecordingCreated, this._recordingCreated, this);
        WI.timelineManager.addEventListener(WI.TimelineManager.Event.RecordingLoaded, this._recordingLoaded, this);

        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStarted, this._capturingStartedOrStopped, this);
        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStopped, this._capturingStartedOrStopped, this);

        WI.notifications.addEventListener(WI.Notification.VisibilityStateDidChange, this._inspectorVisibilityChanged, this);

        this._displayedRecording = null;
        this._displayedContentView = null;
        this._viewMode = null;
        this._previousSelectedTimelineType = null;

        const selectedByUser = false;
        this._changeViewMode(WI.TimelineOverview.ViewMode.Timelines, selectedByUser);

        for (let recording of WI.timelineManager.recordings)
            this._addRecording(recording);

        this._recordingCountChanged();

        // Explicitly update the path for the navigation bar to prevent it from showing up as blank.
        this.contentBrowser.updateHierarchicalPathForCurrentContentView();
    }

    // Static

    static tabInfo()
    {
        return {
            image: "Images/Timeline.svg",
            title: WI.UIString("Timelines"),
        };
    }

    static isTabAllowed()
    {
        return !!window.TimelineAgent || !!window.ScriptProfilerAgent;
    }

    static displayNameForTimelineType(timelineType)
    {
        switch (timelineType) {
        case WI.TimelineRecord.Type.Network:
            return WI.UIString("Network Requests");
        case WI.TimelineRecord.Type.Layout:
            return WI.UIString("Layout & Rendering");
        case WI.TimelineRecord.Type.Script:
            return WI.UIString("JavaScript & Events");
        case WI.TimelineRecord.Type.RenderingFrame:
            return WI.UIString("Rendering Frames");
        case WI.TimelineRecord.Type.CPU:
            return WI.UIString("CPU");
        case WI.TimelineRecord.Type.Memory:
            return WI.UIString("Memory");
        case WI.TimelineRecord.Type.HeapAllocations:
            return WI.UIString("JavaScript Allocations");
        case WI.TimelineRecord.Type.Media:
            return WI.UIString("Media");
        default:
            console.error("Unknown Timeline type:", timelineType);
        }

        return null;
    }

    static iconClassNameForTimelineType(timelineType)
    {
        switch (timelineType) {
        case WI.TimelineRecord.Type.Network:
            return "network-icon";
        case WI.TimelineRecord.Type.Layout:
            return "layout-icon";
        case WI.TimelineRecord.Type.CPU:
            return "cpu-icon";
        case WI.TimelineRecord.Type.Memory:
            return "memory-icon";
        case WI.TimelineRecord.Type.HeapAllocations:
            return "heap-allocations-icon";
        case WI.TimelineRecord.Type.Script:
            return "script-icon";
        case WI.TimelineRecord.Type.RenderingFrame:
            return "rendering-frame-icon";
        case WI.TimelineRecord.Type.Media:
            return "media-icon";
        default:
            console.error("Unknown Timeline type:", timelineType);
        }

        return null;
    }

    static genericClassNameForTimelineType(timelineType)
    {
        switch (timelineType) {
        case WI.TimelineRecord.Type.Network:
            return "network";
        case WI.TimelineRecord.Type.Layout:
            return "colors";
        case WI.TimelineRecord.Type.CPU:
            return "cpu";
        case WI.TimelineRecord.Type.Memory:
            return "memory";
        case WI.TimelineRecord.Type.HeapAllocations:
            return "heap-allocations";
        case WI.TimelineRecord.Type.Script:
            return "script";
        case WI.TimelineRecord.Type.RenderingFrame:
            return "rendering-frame";
        case WI.TimelineRecord.Type.Media:
            return "media";
        default:
            console.error("Unknown Timeline type:", timelineType);
        }

        return null;
    }

    static iconClassNameForRecord(timelineRecord)
    {
        switch (timelineRecord.type) {
        case WI.TimelineRecord.Type.Layout:
            switch (timelineRecord.eventType) {
            case WI.LayoutTimelineRecord.EventType.InvalidateStyles:
            case WI.LayoutTimelineRecord.EventType.RecalculateStyles:
                return WI.TimelineRecordTreeElement.StyleRecordIconStyleClass;
            case WI.LayoutTimelineRecord.EventType.InvalidateLayout:
            case WI.LayoutTimelineRecord.EventType.ForcedLayout:
            case WI.LayoutTimelineRecord.EventType.Layout:
                return WI.TimelineRecordTreeElement.LayoutRecordIconStyleClass;
            case WI.LayoutTimelineRecord.EventType.Paint:
                return WI.TimelineRecordTreeElement.PaintRecordIconStyleClass;
            case WI.LayoutTimelineRecord.EventType.Composite:
                return WI.TimelineRecordTreeElement.CompositeRecordIconStyleClass;
            default:
                console.error("Unknown LayoutTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
            }

            break;

        case WI.TimelineRecord.Type.Script:
            switch (timelineRecord.eventType) {
            case WI.ScriptTimelineRecord.EventType.APIScriptEvaluated:
                return WI.TimelineRecordTreeElement.APIRecordIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.ScriptEvaluated:
                return WI.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.MicrotaskDispatched:
            case WI.ScriptTimelineRecord.EventType.EventDispatched:
            case WI.ScriptTimelineRecord.EventType.ObserverCallback:
                return WI.TimelineRecordTreeElement.EventRecordIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
                return WI.TimelineRecordTreeElement.ProbeRecordIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.ConsoleProfileRecorded:
                return WI.TimelineRecordTreeElement.ConsoleProfileIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.GarbageCollected:
                return WI.TimelineRecordTreeElement.GarbageCollectionIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.TimerInstalled:
                return WI.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.TimerFired:
            case WI.ScriptTimelineRecord.EventType.TimerRemoved:
                return WI.TimelineRecordTreeElement.TimerRecordIconStyleClass;
            case WI.ScriptTimelineRecord.EventType.AnimationFrameFired:
            case WI.ScriptTimelineRecord.EventType.AnimationFrameRequested:
            case WI.ScriptTimelineRecord.EventType.AnimationFrameCanceled:
                return WI.TimelineRecordTreeElement.AnimationRecordIconStyleClass;
            default:
                console.error("Unknown ScriptTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
            }

            break;

        case WI.TimelineRecord.Type.RenderingFrame:
            return WI.TimelineRecordTreeElement.RenderingFrameRecordIconStyleClass;

        case WI.TimelineRecord.Type.HeapAllocations:
            return "heap-snapshot-record";

        case WI.TimelineRecord.Type.Media:
            switch (timelineRecord.eventType) {
            case WI.MediaTimelineRecord.EventType.DOMEvent:
                return "dom-event-record";
            case WI.MediaTimelineRecord.EventType.LowPower:
                return "low-power-record";
            default:
                console.error("Unknown MediaTimelineRecord eventType: " + timelineRecord.eventType, timelineRecord);
            }

            break;

        case WI.TimelineRecord.Type.CPU:
        case WI.TimelineRecord.Type.Memory:
            // Not used. Fall through to error just in case.

        default:
            console.error("Unknown TimelineRecord type: " + timelineRecord.type, timelineRecord);
        }

        return null;
    }

    static displayNameForRecord(timelineRecord, includeDetailsInMainTitle)
    {
        switch (timelineRecord.type) {
        case WI.TimelineRecord.Type.Network:
            return WI.displayNameForURL(timelineRecord.resource.url, timelineRecord.resource.urlComponents);
        case WI.TimelineRecord.Type.Layout:
            return WI.LayoutTimelineRecord.displayNameForEventType(timelineRecord.eventType);
        case WI.TimelineRecord.Type.Script:
            return WI.ScriptTimelineRecord.EventType.displayName(timelineRecord.eventType, timelineRecord.details, includeDetailsInMainTitle);
        case WI.TimelineRecord.Type.RenderingFrame:
            return WI.UIString("Frame %d").format(timelineRecord.frameNumber);
        case WI.TimelineRecord.Type.HeapAllocations:
            if (timelineRecord.heapSnapshot.imported)
                return WI.UIString("Imported \u2014 %s").format(timelineRecord.heapSnapshot.title);
            if (timelineRecord.heapSnapshot.title)
                return WI.UIString("Snapshot %d \u2014 %s").format(timelineRecord.heapSnapshot.identifier, timelineRecord.heapSnapshot.title);
            return WI.UIString("Snapshot %d").format(timelineRecord.heapSnapshot.identifier);
        case WI.TimelineRecord.Type.Media:
            return timelineRecord.displayName;
        case WI.TimelineRecord.Type.CPU:
        case WI.TimelineRecord.Type.Memory:
            // Not used. Fall through to error just in case.
        default:
            console.error("Unknown TimelineRecord type: " + timelineRecord.type, timelineRecord);
        }

        return null;
    }

    // Public

    get type()
    {
        return WI.TimelineTabContentView.Type;
    }

    shown()
    {
        super.shown();

        this._toggleRecordingShortcut.disabled = false;
        this._toggleNewRecordingShortcut.disabled = false;

        if (WI.visible)
            WI.timelineManager.autoCaptureOnPageLoad = true;
    }

    hidden()
    {
        super.hidden();

        this._toggleRecordingShortcut.disabled = true;
        this._toggleNewRecordingShortcut.disabled = true;

        WI.timelineManager.autoCaptureOnPageLoad = false;
    }

    closed()
    {
        super.closed();

        if (WI.FPSInstrument.supported())
            this.contentBrowser.navigationBar.removeEventListener(null, null, this);

        WI.timelineManager.removeEventListener(null, null, this);
        WI.notifications.removeEventListener(null, null, this);
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.TimelineRecording;
    }

    // Protected

    restoreFromCookie(cookie)
    {
        console.assert(cookie);

        this._restoredShowingTimelineRecordingContentView = cookie[WI.TimelineTabContentView.ShowingTimelineRecordingContentViewCookieKey];
        if (!this._restoredShowingTimelineRecordingContentView) {
            if (!this.contentBrowser.currentContentView) {
                // If this is the first time opening the tab, render the currently active recording.
                if (!this._displayedRecording && WI.timelineManager.activeRecording)
                    this._recordingLoaded();

                this._showTimelineViewForType(WI.TimelineTabContentView.OverviewTimelineIdentifierCookieValue);
            }
            return;
        }

        let selectedTimelineViewIdentifier = cookie[WI.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey];
        if (selectedTimelineViewIdentifier === WI.TimelineRecord.Type.RenderingFrame && !WI.FPSInstrument.supported())
            selectedTimelineViewIdentifier = null;

        this._showTimelineViewForType(selectedTimelineViewIdentifier);

        super.restoreFromCookie(cookie);
    }

    saveToCookie(cookie)
    {
        console.assert(cookie);

        cookie[WI.TimelineTabContentView.ShowingTimelineRecordingContentViewCookieKey] = this.contentBrowser.currentContentView instanceof WI.TimelineRecordingContentView;

        if (this._viewMode === WI.TimelineOverview.ViewMode.RenderingFrames)
            cookie[WI.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey] = WI.TimelineRecord.Type.RenderingFrame;
        else {
            let selectedTimeline = this._getTimelineForCurrentContentView();
            if (selectedTimeline)
                cookie[WI.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey] = selectedTimeline.type;
            else
                cookie[WI.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey] = WI.TimelineTabContentView.OverviewTimelineIdentifierCookieValue;
        }

        super.saveToCookie(cookie);
    }

    treeElementForRepresentedObject(representedObject)
    {
        // This can be called by the base class constructor before the map is created.
        if (!this._recordingTreeElementMap)
            return null;

        if (representedObject instanceof WI.TimelineRecording)
            return this._recordingTreeElementMap.get(representedObject);

        return null;
    }

    // Private

    _capturingStartedOrStopped(event)
    {
        let isCapturing = WI.timelineManager.isCapturing();
        this._recordButton.toggled = isCapturing;
    }

    _inspectorVisibilityChanged(event)
    {
        WI.timelineManager.autoCaptureOnPageLoad = !!this.visible && !!WI.visible;
    }

    _toggleRecordingOnSpacebar(event)
    {
        if (WI.isEventTargetAnEditableField(event))
            return;

        this._toggleRecording();

        event.preventDefault();
    }

    _toggleNewRecordingOnSpacebar(event)
    {
        if (WI.isEventTargetAnEditableField(event))
            return;

        this._toggleRecording(true);

        event.preventDefault();
    }

    _toggleRecording(shouldCreateRecording)
    {
        let isCapturing = WI.timelineManager.isCapturing();
        this._recordButton.toggled = isCapturing;

        if (isCapturing)
            WI.timelineManager.stopCapturing();
        else {
            WI.timelineManager.startCapturing(shouldCreateRecording);
            // Show the timeline to which events will be appended.
            this._recordingLoaded();
        }
    }

    _recordButtonClicked(event)
    {
        let shouldCreateNewRecording = window.event ? window.event.shiftKey : false;
        this._recordButton.toggled = !WI.timelineManager.isCapturing();
        this._toggleRecording(shouldCreateNewRecording);
    }

    _recordingsTreeSelectionDidChange(event)
    {
        let treeElement = this._recordingsTreeOutline.selectedTreeElement;
        if (!treeElement)
            return;

        console.assert(treeElement.representedObject instanceof WI.TimelineRecording);

        this._recordingSelected(treeElement.representedObject);
    }

    _recordingCreated(event)
    {
        this._addRecording(event.data.recording);
        this._recordingCountChanged();
    }

    _addRecording(recording)
    {
        console.assert(recording instanceof WI.TimelineRecording, recording);

        let recordingTreeElement = new WI.GeneralTreeElement(WI.TimelineTabContentView.StopwatchIconStyleClass, recording.displayName, null, recording);
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
        console.assert(recording instanceof WI.TimelineRecording, recording);

        this._displayedRecording = recording;

        // Save the current state incase we need to restore it to a new recording.
        let cookie = {};
        this.saveToCookie(cookie);

        if (this._displayedContentView)
            this._displayedContentView.removeEventListener(WI.ContentView.Event.NavigationItemsDidChange, this._displayedContentViewNavigationItemsDidChange, this);

        // Try to get the recording content view if it exists already, if it does we don't want to restore the cookie.
        let onlyExisting = true;
        this._displayedContentView = this.contentBrowser.contentViewForRepresentedObject(this._displayedRecording, onlyExisting);
        if (this._displayedContentView) {
            this._displayedContentView.addEventListener(WI.ContentView.Event.NavigationItemsDidChange, this._displayedContentViewNavigationItemsDidChange, this);

            // Show the timeline that was being shown to update the sidebar tree state.
            let currentTimelineView = this._displayedContentView.currentTimelineView;
            let timelineType = currentTimelineView && currentTimelineView.representedObject instanceof WI.Timeline ? currentTimelineView.representedObject.type : null;
            this._showTimelineViewForType(timelineType);

            return;
        }

        onlyExisting = false;
        this._displayedContentView = this.contentBrowser.contentViewForRepresentedObject(this._displayedRecording, onlyExisting);
        if (this._displayedContentView)
            this._displayedContentView.addEventListener(WI.ContentView.Event.NavigationItemsDidChange, this._displayedContentViewNavigationItemsDidChange, this);

        // Restore the cookie to carry over the previous recording view state to the new recording.
        this.restoreFromCookie(cookie);
    }

    _recordingLoaded(event)
    {
        this._recordingSelected(WI.timelineManager.activeRecording);
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
        if (this._viewMode === mode)
            return;

        let navigationItemForViewMode = this.contentBrowser.navigationBar.findNavigationItem(mode);
        console.assert(navigationItemForViewMode, "Couldn't find navigation item for this view mode.");
        if (!navigationItemForViewMode)
            return;

        this._viewMode = mode;

        this.contentBrowser.navigationBar.selectedNavigationItem = navigationItemForViewMode;

        if (!selectedByUser)
            return;

        let timelineType = this._previousSelectedTimelineType;
        if (this._viewMode === WI.TimelineOverview.ViewMode.RenderingFrames) {
            let timeline = this._getTimelineForCurrentContentView();
            this._previousSelectedTimelineType = timeline ? timeline.type : null;
            timelineType = WI.TimelineRecord.Type.RenderingFrame;
        }

        this._showTimelineViewForType(timelineType);
    }

    _showTimelineViewForType(timelineType)
    {
        console.assert(this._displayedRecording);
        console.assert(this._displayedContentView);

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
        let newViewMode = WI.TimelineOverview.ViewMode.Timelines;
        if (timeline && timeline.type === WI.TimelineRecord.Type.RenderingFrame)
            newViewMode = WI.TimelineOverview.ViewMode.RenderingFrames;

        const selectedByUser = false;
        this._changeViewMode(newViewMode, selectedByUser);
    }

    _getTimelineForCurrentContentView()
    {
        let currentContentView = this.contentBrowser.currentContentView;
        if (!(currentContentView instanceof WI.TimelineRecordingContentView))
            return null;

        let timelineView = currentContentView.currentTimelineView;
        return (timelineView && timelineView.representedObject instanceof WI.Timeline) ? timelineView.representedObject : null;
    }
};

WI.TimelineTabContentView.Type = "timeline";

WI.TimelineTabContentView.ShowingTimelineRecordingContentViewCookieKey = "timeline-sidebar-panel-showing-timeline-recording-content-view";
WI.TimelineTabContentView.SelectedTimelineViewIdentifierCookieKey = "timeline-sidebar-panel-selected-timeline-view-identifier";
WI.TimelineTabContentView.OverviewTimelineIdentifierCookieValue = "overview";
WI.TimelineTabContentView.StopwatchIconStyleClass = "stopwatch-icon";
