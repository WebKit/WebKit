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

WebInspector.TimelineSidebarPanel = function()
{
    WebInspector.NavigationSidebarPanel.call(this, "timeline", WebInspector.UIString("Timelines"), "Images/NavigationItemStopwatch.svg", "2");

    this._timelineEventsTitleBarElement = document.createElement("div");
    this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
    this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass);
    this.element.insertBefore(this._timelineEventsTitleBarElement, this.element.firstChild);

    this.contentTreeOutlineLabel = "";

    this._timelinesContentContainer = document.createElement("div");
    this._timelinesContentContainer.classList.add(WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass);
    this.element.insertBefore(this._timelinesContentContainer, this.element.firstChild);

    // Maintain an invisible tree outline containing tree elements for all recordings.
    // The visible recording's tree element is selected when the content view changes.
    this._recordingTreeElementMap = new Map;
    this._recordingsTreeOutline = this.createContentTreeOutline(true, true);
    this._recordingsTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
    this._recordingsTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.ContentTreeOutlineElementHiddenStyleClassName);
    this._recordingsTreeOutline.onselect = this._recordingsTreeElementSelected.bind(this);
    this._timelinesContentContainer.appendChild(this._recordingsTreeOutline.element);

    this._timelinesTreeOutline = this.createContentTreeOutline(true, true);
    this._timelinesTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
    this._timelinesTreeOutline.onselect = this._timelinesTreeElementSelected.bind(this);
    this._timelinesContentContainer.appendChild(this._timelinesTreeOutline.element);

    function createTimelineTreeElement(label, iconClass, identifier)
    {
        var treeElement = new WebInspector.GeneralTreeElement([iconClass, WebInspector.TimelineSidebarPanel.LargeIconStyleClass], label, null, identifier);

        const tooltip = WebInspector.UIString("Close %s timeline view").format(label);
        wrappedSVGDocument(platformImagePath("CloseLarge.svg"), WebInspector.TimelineSidebarPanel.CloseButtonStyleClass, tooltip, function(element) {
            var button = new WebInspector.TreeElementStatusButton(element);
            button.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this.showTimelineOverview, this);
            treeElement.status = button.element;
        }.bind(this));

        return treeElement;
    }

    // Timeline elements are reused; clicking them displays a TimelineView
    // for the relevant timeline of the active recording.
    this._timelineTreeElementMap = new Map;
    this._timelineTreeElementMap.set(WebInspector.TimelineRecord.Type.Network, createTimelineTreeElement.call(this, WebInspector.UIString("Network Requests"), WebInspector.TimelineSidebarPanel.NetworkIconStyleClass, WebInspector.TimelineRecord.Type.Network));
    this._timelineTreeElementMap.set(WebInspector.TimelineRecord.Type.Layout, createTimelineTreeElement.call(this, WebInspector.UIString("Layout & Rendering"), WebInspector.TimelineSidebarPanel.ColorsIconStyleClass, WebInspector.TimelineRecord.Type.Layout));
    this._timelineTreeElementMap.set(WebInspector.TimelineRecord.Type.Script, createTimelineTreeElement.call(this, WebInspector.UIString("JavaScript & Events"), WebInspector.TimelineSidebarPanel.ScriptIconStyleClass, WebInspector.TimelineRecord.Type.Script));

    for (var timelineTreeElement of this._timelineTreeElementMap.values())
        this._timelinesTreeOutline.appendChild(timelineTreeElement);

    var timelinesTitleBarElement = document.createElement("div");
    timelinesTitleBarElement.textContent = WebInspector.UIString("Timelines");
    timelinesTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
    timelinesTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TimelinesTitleBarStyleClass);
    this.element.insertBefore(timelinesTitleBarElement, this.element.firstChild);

    var statusBarElement = this._statusBarElement = document.createElement("div");
    statusBarElement.classList.add(WebInspector.TimelineSidebarPanel.StatusBarStyleClass);
    this.element.insertBefore(statusBarElement, this.element.firstChild);

    this._recordGlyphElement = document.createElement("div");
    this._recordGlyphElement.className = WebInspector.TimelineSidebarPanel.RecordGlyphStyleClass;
    this._recordGlyphElement.addEventListener("mouseover", this._recordGlyphMousedOver.bind(this));
    this._recordGlyphElement.addEventListener("mouseout", this._recordGlyphMousedOut.bind(this));
    this._recordGlyphElement.addEventListener("click", this._recordGlyphClicked.bind(this));
    statusBarElement.appendChild(this._recordGlyphElement);

    this._recordStatusElement = document.createElement("div");
    this._recordStatusElement.className = WebInspector.TimelineSidebarPanel.RecordStatusStyleClass;
    statusBarElement.appendChild(this._recordStatusElement);

    WebInspector.showReplayInterfaceSetting.addEventListener(WebInspector.Setting.Event.Changed, this._updateReplayInterfaceVisibility, this);

    // We always create a navigation bar; its visibility is controlled by WebInspector.showReplayInterfaceSetting.
    this._navigationBar = new WebInspector.NavigationBar;
    this.element.appendChild(this._navigationBar.element);

    var toolTip = WebInspector.UIString("Begin Capturing");
    var altToolTip = WebInspector.UIString("End Capturing");
    this._replayCaptureButtonItem = new WebInspector.ActivateButtonNavigationItem("replay-capture", toolTip, altToolTip, "Images/Circle.svg", 16, 16);
    this._replayCaptureButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._replayCaptureButtonClicked, this);
    this._replayCaptureButtonItem.enabled = true;
    this._navigationBar.addNavigationItem(this._replayCaptureButtonItem);

    var pauseImage, resumeImage;
    if (WebInspector.Platform.isLegacyMacOS) {
        pauseImage = {src: "Images/Legacy/Pause.svg", width: 16, height: 16};
        resumeImage = {src: "Images/Legacy/Resume.svg", width: 16, height: 16};
    } else {
        pauseImage = {src: "Images/Pause.svg", width: 15, height: 15};
        resumeImage = {src: "Images/Resume.svg", width: 15, height: 15};
    }

    toolTip = WebInspector.UIString("Start Playback");
    altToolTip = WebInspector.UIString("Pause Playback");
    this._replayPauseResumeButtonItem = new WebInspector.ToggleButtonNavigationItem("replay-pause-resume", toolTip, altToolTip, resumeImage.src, pauseImage.src, pauseImage.width, pauseImage.height, true);
    this._replayPauseResumeButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._replayPauseResumeButtonClicked, this);
    this._replayPauseResumeButtonItem.enabled = false;
    this._navigationBar.addNavigationItem(this._replayPauseResumeButtonItem);

    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStarted, this._captureStarted, this);
    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStopped, this._captureStopped, this);

    this._statusBarElement.oncontextmenu = this._contextMenuNavigationBarOrStatusBar.bind(this);
    this._navigationBar.element.oncontextmenu = this._contextMenuNavigationBarOrStatusBar.bind(this);
    this._updateReplayInterfaceVisibility();

    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingCreated, this._recordingCreated, this);
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingLoaded, this._recordingLoaded, this);

    WebInspector.contentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange, this._contentBrowserCurrentContentViewDidChange, this);
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);
};

WebInspector.TimelineSidebarPanel.HiddenStyleClassName = "hidden";
WebInspector.TimelineSidebarPanel.StatusBarStyleClass = "status-bar";
WebInspector.TimelineSidebarPanel.RecordGlyphStyleClass = "record-glyph";
WebInspector.TimelineSidebarPanel.RecordGlyphRecordingStyleClass = "recording";
WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass = "forced";
WebInspector.TimelineSidebarPanel.RecordStatusStyleClass = "record-status";
WebInspector.TimelineSidebarPanel.TitleBarStyleClass = "title-bar";
WebInspector.TimelineSidebarPanel.TimelinesTitleBarStyleClass = "timelines";
WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass = "timeline-events";
WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass = "timelines-content";
WebInspector.TimelineSidebarPanel.CloseButtonStyleClass = "close-button";
WebInspector.TimelineSidebarPanel.LargeIconStyleClass = "large";
WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass = "stopwatch-icon";
WebInspector.TimelineSidebarPanel.NetworkIconStyleClass = "network-icon";
WebInspector.TimelineSidebarPanel.ColorsIconStyleClass = "colors-icon";
WebInspector.TimelineSidebarPanel.ScriptIconStyleClass = "script-icon";
WebInspector.TimelineSidebarPanel.TimelineContentViewShowingStyleClass = "timeline-content-view-showing";

WebInspector.TimelineSidebarPanel.ShowingTimelineContentViewCookieKey = "timeline-sidebar-panel-showing-timeline-content-view";
WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey = "timeline-sidebar-panel-selected-timeline-view-identifier";
WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue = "overview";

WebInspector.TimelineSidebarPanel.prototype = {
    constructor: WebInspector.TimelineSidebarPanel,
    __proto__: WebInspector.NavigationSidebarPanel.prototype,

    // Public

    shown: function()
    {
        WebInspector.NavigationSidebarPanel.prototype.shown.call(this);

        if (this._activeContentView)
            WebInspector.contentBrowser.showContentView(this._activeContentView);
    },

    showDefaultContentView: function()
    {
        if (this._activeContentView)
            this.showTimelineOverview();
    },

    get hasSelectedElement()
    {
        return !!this._contentTreeOutline.selectedTreeElement || !!this._recordingsTreeOutline.selectedTreeElement;
    },

    treeElementForRepresentedObject: function(representedObject)
    {
        if (representedObject instanceof WebInspector.TimelineRecording)
            return this._recordingTreeElementMap.get(representedObject);

        // The main resource is used as the representedObject instead of Frame in our tree.
        if (representedObject instanceof WebInspector.Frame)
            representedObject = representedObject.mainResource;

        var foundTreeElement = this.contentTreeOutline.getCachedTreeElement(representedObject);
        if (foundTreeElement)
            return foundTreeElement;

        // Look for TreeElements loosely based on represented objects that can contain the represented
        // object we are really looking for. This allows a SourceCodeTimelineTreeElement or a
        // TimelineRecordTreeElement to stay selected when the Resource it represents is showing.

        function looselyCompareRepresentedObjects(candidateTreeElement)
        {
            if (!candidateTreeElement)
                return false;

            var candidateRepresentedObject = candidateTreeElement.representedObject;
            if (candidateRepresentedObject instanceof WebInspector.SourceCodeTimeline) {
                if (candidateRepresentedObject.sourceCode === representedObject)
                    return true;
                return false;
            }

            if (candidateRepresentedObject instanceof WebInspector.TimelineRecord) {
                if (!candidateRepresentedObject.sourceCodeLocation)
                    return false;
                if (candidateRepresentedObject.sourceCodeLocation.sourceCode === representedObject)
                    return true;
                return false;
            }

            if (candidateRepresentedObject instanceof WebInspector.ProfileNode)
                return false;

            console.error("Unknown TreeElement", candidateTreeElement);
            return false;
        }

        // Check the selected tree element first so we don't need to do a longer search and it is
        // likely to be the best candidate for the current view.
        if (looselyCompareRepresentedObjects(this.contentTreeOutline.selectedTreeElement))
            return this.contentTreeOutline.selectedTreeElement;

        var currentTreeElement = this._contentTreeOutline.children[0];
        while (currentTreeElement && !currentTreeElement.root) {
            if (looselyCompareRepresentedObjects(currentTreeElement))
                return currentTreeElement;
            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, false);
        }

        return null;
    },

    get contentTreeOutlineLabel()
    {
        return this._timelineEventsTitleBarElement.textContent;
    },

    set contentTreeOutlineLabel(label)
    {
        label = label || WebInspector.UIString("Timeline Events");

        this._timelineEventsTitleBarElement.textContent = label;
        this.filterBar.placeholder = WebInspector.UIString("Filter %s").format(label);
    },

    showTimelineOverview: function()
    {
        if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();

        this._activeContentView.showOverviewTimelineView();
        WebInspector.contentBrowser.showContentView(this._activeContentView);
    },

    showTimelineViewForType: function(timelineType)
    {
        console.assert(this._timelineTreeElementMap.has(timelineType), timelineType);
        if (!this._timelineTreeElementMap.has(timelineType))
            return;

        // Defer showing the relevant timeline to the onselect handler of the timelines tree element.
        const wasSelectedByUser = true;
        const shouldSuppressOnSelect = false;
        this._timelineTreeElementMap.get(timelineType).select(true, wasSelectedByUser, shouldSuppressOnSelect, true);
    },

    // Protected

    hasCustomFilters: function()
    {
        return true;
    },

    matchTreeElementAgainstCustomFilters: function(treeElement)
    {
        if (!this._activeContentView)
            return true;

        return this._activeContentView.matchTreeElementAgainstCustomFilters(treeElement);
    },

    canShowDifferentContentView: function()
    {
        return !this.restoringState || !this._restoredShowingTimelineContentView;
    },

    saveStateToCookie: function(cookie)
    {
        console.assert(cookie);

        cookie[WebInspector.TimelineSidebarPanel.ShowingTimelineContentViewCookieKey] = WebInspector.contentBrowser.currentContentView instanceof WebInspector.TimelineContentView;

        var selectedTreeElement = this._timelinesTreeOutline.selectedTreeElement;
        if (selectedTreeElement)
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = selectedTreeElement.representedObject.type;
        else
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue;

        WebInspector.NavigationSidebarPanel.prototype.saveStateToCookie.call(this, cookie);
    },

    restoreStateFromCookie: function(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // The _activeContentView is not ready on initial load, so delay the restore.
        // This matches the delayed work in the WebInspector.TimelineSidebarPanel constructor.
        if (!this._activeContentView) {
            setTimeout(this.restoreStateFromCookie.bind(this, cookie, relaxedMatchDelay), 0);
            return;
        }

        this._restoredShowingTimelineContentView = cookie[WebInspector.TimelineSidebarPanel.ShowingTimelineContentViewCookieKey];

        var selectedTimelineViewIdentifier = cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey];
        if (!selectedTimelineViewIdentifier || selectedTimelineViewIdentifier === WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue)
            this.showTimelineOverview();
        else
            this.showTimelineViewForType(selectedTimelineViewIdentifier);

        // Don't call NavigationSidebarPanel.restoreStateFromCookie, because it tries to match based
        // on type only as a last resort. This would cause the first recording to be reselected on reload.
    },

    // Private

    _recordingsTreeElementSelected: function(treeElement, selectedByUser)
    {
        console.assert(treeElement.representedObject instanceof WebInspector.TimelineRecording);
        console.assert(!selectedByUser, "Recording tree elements should be hidden and only programmatically selectable.");

        this._activeContentView = WebInspector.contentBrowser.contentViewForRepresentedObject(treeElement.representedObject);

        // Deselect or re-select the timeline tree element for the timeline view being displayed.
        var currentTimelineView = this._activeContentView.currentTimelineView;
        if (currentTimelineView && currentTimelineView.representedObject instanceof WebInspector.Timeline) {
            const wasSelectedByUser = false; // This is a simulated selection.
            const shouldSuppressOnSelect = false;
            this._timelineTreeElementMap.get(currentTimelineView.representedObject.type).select(true, wasSelectedByUser, shouldSuppressOnSelect, true);
        } else if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();

        this.updateFilter();
    },

    _timelinesTreeElementSelected: function(treeElement, selectedByUser)
    {
        console.assert(this._timelineTreeElementMap.get(treeElement.representedObject) === treeElement, treeElement);

        // If not selected by user, then this selection merely synced the tree element with the content view's contents.
        if (!selectedByUser) {
            console.assert(this._activeContentView.currentTimelineView.representedObject.type === treeElement.representedObject);
            return;
        }

        var timelineType = treeElement.representedObject;
        var timeline = this._activeContentView.representedObject.timelines.get(timelineType);
        this._activeContentView.showTimelineViewForTimeline(timeline);
        WebInspector.contentBrowser.showContentView(this._activeContentView);
    },

    _contentBrowserCurrentContentViewDidChange: function(event)
    {
        var didShowTimelineContentView = WebInspector.contentBrowser.currentContentView instanceof WebInspector.TimelineContentView;
        this.element.classList.toggle(WebInspector.TimelineSidebarPanel.TimelineContentViewShowingStyleClass, didShowTimelineContentView);
    },

    _capturingStarted: function(event)
    {
        this._recordStatusElement.textContent = WebInspector.UIString("Recording");
        this._recordGlyphElement.classList.add(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingStyleClass);
    },

    _capturingStopped: function(event)
    {
        this._recordStatusElement.textContent = "";
        this._recordGlyphElement.classList.remove(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingStyleClass);
    },

    _recordingCreated: function(event)
    {
        var recording = event.data.recording;
        console.assert(recording instanceof WebInspector.TimelineRecording, recording);

        var recordingTreeElement = new WebInspector.GeneralTreeElement(WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass, recording.displayName, null, recording);
        this._recordingTreeElementMap.set(recording, recordingTreeElement);
        this._recordingsTreeOutline.appendChild(recordingTreeElement);

        var previousTreeElement = null;
        for (var treeElement of this._recordingTreeElementMap.values()) {
            if (previousTreeElement) {
                previousTreeElement.nextSibling = treeElement;
                treeElement.previousSibling = previousTreeElement;
            }

            previousTreeElement = treeElement;
        }
    },

    _recordingLoaded: function()
    {
        this._activeContentView = WebInspector.contentBrowser.contentViewForRepresentedObject(WebInspector.timelineManager.activeRecording);

        if (this.selected)
            WebInspector.contentBrowser.showContentView(this._activeContentView);
    },

    _recordGlyphMousedOver: function(event)
    {
        this._recordGlyphElement.classList.remove(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass);

        if (WebInspector.timelineManager.isCapturing())
            this._recordStatusElement.textContent = WebInspector.UIString("Stop Recording");
        else
            this._recordStatusElement.textContent = WebInspector.UIString("Start Recording");
    },

    _recordGlyphMousedOut: function(event)
    {
        this._recordGlyphElement.classList.remove(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass);

        if (WebInspector.timelineManager.isCapturing())
            this._recordStatusElement.textContent = WebInspector.UIString("Recording");
        else
            this._recordStatusElement.textContent = "";
    },

    _recordGlyphClicked: function(event)
    {
        // Add forced class to prevent the glyph from showing a confusing status after click.
        this._recordGlyphElement.classList.add(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass);

        var shouldCreateRecording = event.shiftKey;

        if (WebInspector.timelineManager.isCapturing())
            WebInspector.timelineManager.stopCapturing();
        else {
            WebInspector.timelineManager.startCapturing(shouldCreateRecording);
            // Show the timeline to which events will be appended.
            this._recordingLoaded();
        }
    },

    // These methods are only used when ReplayAgent is available.

    _updateReplayInterfaceVisibility: function()
    {
        var shouldShowReplayInterface = window.ReplayAgent && WebInspector.showReplayInterfaceSetting.value;

        this._statusBarElement.classList.toggle(WebInspector.TimelineSidebarPanel.HiddenStyleClassName, shouldShowReplayInterface);
        this._navigationBar.element.classList.toggle(WebInspector.TimelineSidebarPanel.HiddenStyleClassName, !shouldShowReplayInterface);
    },

    _contextMenuNavigationBarOrStatusBar: function()
    {
        if (!window.ReplayAgent)
            return;

        function toggleReplayInterface() {
            WebInspector.showReplayInterfaceSetting.value = !WebInspector.showReplayInterfaceSetting.value;
        }

        var contextMenu = new WebInspector.ContextMenu(event);
        if (WebInspector.showReplayInterfaceSetting.value)
            contextMenu.appendItem(WebInspector.UIString("Hide Replay Controls"), toggleReplayInterface);
        else
            contextMenu.appendItem(WebInspector.UIString("Show Replay Controls"), toggleReplayInterface);
        contextMenu.show();
    },

    _replayCaptureButtonClicked: function()
    {
        if (!this._replayCaptureButtonItem.activated) {
            WebInspector.replayManager.startCapturing();
            WebInspector.timelineManager.startCapturing();

            // De-bounce further presses until the backend has begun capturing.
            this._replayCaptureButtonItem.activated = true;
            this._replayCaptureButtonItem.enabled = false;
        } else {
            WebInspector.replayManager.stopCapturing();
            WebInspector.timelineManager.stopCapturing();

            this._replayCaptureButtonItem.enabled = false;
        }
    },

    _replayPauseResumeButtonClicked: function()
    {
        if (this._replayPauseResumeButtonItem.toggled)
            WebInspector.replayManager.pausePlayback();
        else
            WebInspector.replayManager.replayToCompletion();
    },

    _captureStarted: function()
    {
        this._replayCaptureButtonItem.enabled = true;
    },

    _captureStopped: function()
    {
        this._replayCaptureButtonItem.activated = false;
        this._replayPauseResumeButtonItem.enabled = true;
    },

    _playbackStarted: function()
    {
        this._replayPauseResumeButtonItem.toggled = true;
    },

    _playbackPaused: function()
    {
        this._replayPauseResumeButtonItem.toggled = false;
    }
};
