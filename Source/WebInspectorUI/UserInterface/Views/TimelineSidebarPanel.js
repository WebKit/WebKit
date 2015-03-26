/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.TimelineSidebarPanel = class TimelineSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor()
    {
        super("timeline", WebInspector.UIString("Timelines"), "Images/NavigationItemStopwatch.svg", "2");

        this._timelineEventsTitleBarElement = document.createElement("div");
        this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
        this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass);
        this.element.insertBefore(this._timelineEventsTitleBarElement, this.element.firstChild);

        this.contentTreeOutlineLabel = "";

        this._timelinesContentContainerElement = document.createElement("div");
        this._timelinesContentContainerElement.classList.add(WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass);
        this.element.insertBefore(this._timelinesContentContainerElement, this.element.firstChild);

        this._displayedRecording = null;
        this._displayedContentView = null;

        // Maintain an invisible tree outline containing tree elements for all recordings.
        // The visible recording's tree element is selected when the content view changes.
        this._recordingTreeElementMap = new Map;
        this._recordingsTreeOutline = this.createContentTreeOutline(true, true);
        this._recordingsTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
        this._recordingsTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.ContentTreeOutlineElementHiddenStyleClassName);
        this._recordingsTreeOutline.onselect = this._recordingsTreeElementSelected.bind(this);
        this._timelinesContentContainerElement.appendChild(this._recordingsTreeOutline.element);

        // Maintain a tree outline with tree elements for each timeline of the selected recording.
        this._timelinesTreeOutline = this.createContentTreeOutline(true, true);
        this._timelinesTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
        this._timelinesTreeOutline.onselect = this._timelinesTreeElementSelected.bind(this);
        this._timelinesContentContainerElement.appendChild(this._timelinesTreeOutline.element);

        this._timelineTreeElementMap = new Map;

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
    }

    // Public

    shown()
    {
        WebInspector.NavigationSidebarPanel.prototype.shown.call(this);

        if (this._displayedContentView)
            WebInspector.contentBrowser.showContentView(this._displayedContentView);
    }

    showDefaultContentView()
    {
        if (this._displayedContentView)
            this.showTimelineOverview();
    }

    get hasSelectedElement()
    {
        return !!this._contentTreeOutline.selectedTreeElement || !!this._recordingsTreeOutline.selectedTreeElement;
    }

    treeElementForRepresentedObject(representedObject)
    {
        if (representedObject instanceof WebInspector.TimelineRecording)
            return this._recordingTreeElementMap.get(representedObject);

        // This fails if the timeline does not belong to the selected recording.
        if (representedObject instanceof WebInspector.Timeline) {
            var foundTreeElement = this._timelineTreeElementMap.get(representedObject);
            if (foundTreeElement)
                return foundTreeElement;
        }

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
            } else if (candidateRepresentedObject instanceof WebInspector.Timeline && representedObject instanceof WebInspector.Timeline) {
                // Reopen to the same timeline, even if a different parent recording is currently shown.
                if (candidateRepresentedObject.type === representedObject.type)
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
    }

    get contentTreeOutlineLabel()
    {
        return this._timelineEventsTitleBarElement.textContent;
    }

    set contentTreeOutlineLabel(label)
    {
        label = label || WebInspector.UIString("Timeline Events");

        this._timelineEventsTitleBarElement.textContent = label;
        this.filterBar.placeholder = WebInspector.UIString("Filter %s").format(label);
    }

    showTimelineOverview()
    {
        if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();

        this._displayedContentView.showOverviewTimelineView();
        WebInspector.contentBrowser.showContentView(this._displayedContentView);
    }

    showTimelineViewForTimeline(timeline)
    {
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._timelineTreeElementMap.has(timeline), "Cannot show timeline because it does not belong to the shown recording.", timeline);

        // Defer showing the relevant timeline to the onselect handler of the timelines tree element.
        var wasSelectedByUser = true;
        var shouldSuppressOnSelect = false;
        this._timelineTreeElementMap.get(timeline).select(true, wasSelectedByUser, shouldSuppressOnSelect, true);
    }

    // Protected

    updateFilter()
    {
        WebInspector.NavigationSidebarPanel.prototype.updateFilter.call(this);

        this._displayedContentView.filterDidChange();
    }

    hasCustomFilters()
    {
        return true;
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        if (!this._displayedContentView)
            return true;

        return this._displayedContentView.matchTreeElementAgainstCustomFilters(treeElement);
    }

    canShowDifferentContentView()
    {
        return !this.restoringState || !this._restoredShowingTimelineRecordingContentView;
    }

    saveStateToCookie(cookie)
    {
        console.assert(cookie);

        cookie[WebInspector.TimelineSidebarPanel.ShowingTimelineRecordingContentViewCookieKey] = WebInspector.contentBrowser.currentContentView instanceof WebInspector.TimelineRecordingContentView;

        var selectedTreeElement = this._timelinesTreeOutline.selectedTreeElement;
        if (selectedTreeElement)
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = selectedTreeElement.representedObject.type;
        else
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue;

        super.saveStateToCookie(cookie);
    }

    restoreStateFromCookie(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // The _displayedContentView is not ready on initial load, so delay the restore.
        // This matches the delayed work in the WebInspector.TimelineSidebarPanel constructor.
        if (!this._displayedContentView) {
            setTimeout(this.restoreStateFromCookie.bind(this, cookie, relaxedMatchDelay), 0);
            return;
        }

        this._restoredShowingTimelineRecordingContentView = cookie[WebInspector.TimelineSidebarPanel.ShowingTimelineRecordingContentViewCookieKey];

        var selectedTimelineViewIdentifier = cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey];
        if (!selectedTimelineViewIdentifier || selectedTimelineViewIdentifier === WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue)
            this.showTimelineOverview();
        else if (this._displayedRecording.timelines.has(selectedTimelineViewIdentifier))
            this.showTimelineViewForTimeline(this._displayedRecording.timelines.get(selectedTimelineViewIdentifier));
        else
            this.showTimelineOverview();

        // Don't call NavigationSidebarPanel.restoreStateFromCookie, because it tries to match based
        // on type only as a last resort. This would cause the first recording to be reselected on reload.
    }

    // Private

    _recordingsTreeElementSelected(treeElement, selectedByUser)
    {
        console.assert(treeElement.representedObject instanceof WebInspector.TimelineRecording);
        console.assert(!selectedByUser, "Recording tree elements should be hidden and only programmatically selectable.");

        this._recordingSelected(treeElement.representedObject);

        // Deselect or re-select the timeline tree element for the timeline view being displayed.
        var currentTimelineView = this._displayedContentView.currentTimelineView;
        if (currentTimelineView && currentTimelineView.representedObject instanceof WebInspector.Timeline) {
            var wasSelectedByUser = false; // This is a simulated selection.
            var shouldSuppressOnSelect = false;
            this._timelineTreeElementMap.get(currentTimelineView.representedObject).select(true, wasSelectedByUser, shouldSuppressOnSelect, true);
        } else if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();

        this.updateFilter();
    }

    _timelinesTreeElementSelected(treeElement, selectedByUser)
    {
        console.assert(this._timelineTreeElementMap.get(treeElement.representedObject) === treeElement, treeElement);

        // If not selected by user, then this selection merely synced the tree element with the content view's contents.
        if (!selectedByUser) {
            console.assert(this._displayedContentView.currentTimelineView.representedObject === treeElement.representedObject);
            return;
        }

        var timeline = treeElement.representedObject;
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._displayedRecording.timelines.get(timeline.type) === timeline, timeline);

        this._displayedContentView.showTimelineViewForTimeline(timeline);
        WebInspector.contentBrowser.showContentView(this._displayedContentView);
    }

    _contentBrowserCurrentContentViewDidChange(event)
    {
        var didShowTimelineRecordingContentView = WebInspector.contentBrowser.currentContentView instanceof WebInspector.TimelineRecordingContentView;
        this.element.classList.toggle(WebInspector.TimelineSidebarPanel.TimelineRecordingContentViewShowingStyleClass, didShowTimelineRecordingContentView);
    }

    _capturingStarted(event)
    {
        this._recordStatusElement.textContent = WebInspector.UIString("Recording");
        this._recordGlyphElement.classList.add(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingStyleClass);
    }

    _capturingStopped(event)
    {
        this._recordStatusElement.textContent = "";
        this._recordGlyphElement.classList.remove(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingStyleClass);
    }

    _recordingCreated(event)
    {
        var recording = event.data.recording;
        console.assert(recording instanceof WebInspector.TimelineRecording, recording);

        var recordingTreeElement = new WebInspector.GeneralTreeElement(WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass, recording.displayName, null, recording);
        this._recordingTreeElementMap.set(recording, recordingTreeElement);
        this._recordingsTreeOutline.appendChild(recordingTreeElement);

        this._recordingCountChanged();
    }

    _recordingCountChanged()
    {
        var previousTreeElement = null;
        for (var treeElement of this._recordingTreeElementMap.values()) {
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

        var oldRecording = this._displayedRecording || null;
        if (oldRecording) {
            oldRecording.removeEventListener(WebInspector.TimelineRecording.Event.TimelineAdded, this._timelineAdded, this);
            oldRecording.removeEventListener(WebInspector.TimelineRecording.Event.TimelineRemoved, this._timelineRemoved, this);

            // Destroy tree elements in one operation to avoid unnecessary fixups.
            this._timelinesTreeOutline.removeChildren();
            this._timelineTreeElementMap.clear();
        }

        this._displayedRecording = recording;
        this._displayedRecording.addEventListener(WebInspector.TimelineRecording.Event.TimelineAdded, this._timelineAdded, this);
        this._displayedRecording.addEventListener(WebInspector.TimelineRecording.Event.TimelineRemoved, this._timelineRemoved, this);

        for (var timeline of recording.timelines.values())
            this._timelineAdded(timeline);

        this._displayedContentView = WebInspector.contentBrowser.contentViewForRepresentedObject(this._displayedRecording);
        if (this.selected)
            WebInspector.contentBrowser.showContentView(this._displayedContentView);
    }

    _recordingLoaded(event)
    {
        this._recordingSelected(WebInspector.timelineManager.activeRecording);
    }

    _timelineAdded(timelineOrEvent)
    {
        var timeline = timelineOrEvent;
        if (!(timeline instanceof WebInspector.Timeline))
            timeline = timelineOrEvent.data.timeline;

        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(!this._timelineTreeElementMap.has(timeline), timeline);

        var timelineTreeElement = new WebInspector.GeneralTreeElement([timeline.iconClassName, WebInspector.TimelineSidebarPanel.LargeIconStyleClass], timeline.displayName, null, timeline);
        var tooltip = WebInspector.UIString("Close %s timeline view").format(timeline.displayName);
        wrappedSVGDocument(platformImagePath("CloseLarge.svg"), WebInspector.TimelineSidebarPanel.CloseButtonStyleClass, tooltip, function(element) {
            var button = new WebInspector.TreeElementStatusButton(element);
            button.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this.showTimelineOverview, this);
            timelineTreeElement.status = button.element;
        }.bind(this));
        this._timelinesTreeOutline.appendChild(timelineTreeElement);
        this._timelineTreeElementMap.set(timeline, timelineTreeElement);

        this._timelineCountChanged();
    }

    _timelineRemoved(event)
    {
        var timeline = event.data.timeline;
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._timelineTreeElementMap.has(timeline), timeline);

        var timelineTreeElement = this._timelineTreeElementMap.take(timeline);
        var shouldSuppressOnDeselect = false;
        var shouldSuppressSelectSibling = true;
        this._timelinesTreeOutline.removeChild(timelineTreeElement, shouldSuppressOnDeselect, shouldSuppressSelectSibling);
        this._timelineTreeElementMap.delete(timeline);

        this._timelineCountChanged();
    }

    _timelineCountChanged()
    {
        var previousTreeElement = null;
        for (var treeElement of this._timelineTreeElementMap.values()) {
            if (previousTreeElement) {
                previousTreeElement.nextSibling = treeElement;
                treeElement.previousSibling = previousTreeElement;
            }

            previousTreeElement = treeElement;
        }

        var timelineHeight = 36;
        var eventTitleBarOffset = 51;
        var contentElementOffset = 74;
        var timelineCount = this._displayedRecording.timelines.size;
        this._timelinesContentContainerElement.style.height = (timelineHeight * timelineCount) + "px";
        this._timelineEventsTitleBarElement.style.top = (timelineHeight * timelineCount + eventTitleBarOffset) + "px";
        this.contentElement.style.top = (timelineHeight * timelineCount + contentElementOffset) + "px";
    }

    _recordGlyphMousedOver(event)
    {
        this._recordGlyphElement.classList.remove(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass);

        if (WebInspector.timelineManager.isCapturing())
            this._recordStatusElement.textContent = WebInspector.UIString("Stop Recording");
        else
            this._recordStatusElement.textContent = WebInspector.UIString("Start Recording");
    }

    _recordGlyphMousedOut(event)
    {
        this._recordGlyphElement.classList.remove(WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass);

        if (WebInspector.timelineManager.isCapturing())
            this._recordStatusElement.textContent = WebInspector.UIString("Recording");
        else
            this._recordStatusElement.textContent = "";
    }

    _recordGlyphClicked(event)
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
    }

    // These methods are only used when ReplayAgent is available.

    _updateReplayInterfaceVisibility()
    {
        var shouldShowReplayInterface = window.ReplayAgent && WebInspector.showReplayInterfaceSetting.value;

        this._statusBarElement.classList.toggle(WebInspector.TimelineSidebarPanel.HiddenStyleClassName, shouldShowReplayInterface);
        this._navigationBar.element.classList.toggle(WebInspector.TimelineSidebarPanel.HiddenStyleClassName, !shouldShowReplayInterface);
    }

    _contextMenuNavigationBarOrStatusBar()
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
    }

    _replayCaptureButtonClicked()
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
    }

    _replayPauseResumeButtonClicked()
    {
        if (this._replayPauseResumeButtonItem.toggled)
            WebInspector.replayManager.pausePlayback();
        else
            WebInspector.replayManager.replayToCompletion();
    }

    _captureStarted()
    {
        this._replayCaptureButtonItem.enabled = true;
    }

    _captureStopped()
    {
        this._replayCaptureButtonItem.activated = false;
        this._replayPauseResumeButtonItem.enabled = true;
    }

    _playbackStarted()
    {
        this._replayPauseResumeButtonItem.toggled = true;
    }

    _playbackPaused()
    {
        this._replayPauseResumeButtonItem.toggled = false;
    }
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
WebInspector.TimelineSidebarPanel.RunLoopIconStyleClass = "runloop-icon";
WebInspector.TimelineSidebarPanel.TimelineRecordingContentViewShowingStyleClass = "timeline-recording-content-view-showing";

WebInspector.TimelineSidebarPanel.ShowingTimelineRecordingContentViewCookieKey = "timeline-sidebar-panel-showing-timeline-recording-content-view";
WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey = "timeline-sidebar-panel-selected-timeline-view-identifier";
WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue = "overview";
