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
    constructor(contentBrowser)
    {
        super("timeline", WebInspector.UIString("Timelines"));

        this.contentBrowser = contentBrowser;

        this._timelineEventsTitleBarContainer = document.createElement("div");
        this._timelineEventsTitleBarContainer.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
        this._timelineEventsTitleBarContainer.classList.add(WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass);

        this._timelineEventsTitleBarElement = document.createElement("div");
        this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TitleBarTextStyleClass);
        this._timelineEventsTitleBarContainer.appendChild(this._timelineEventsTitleBarElement);

        this._timelineEventsTitleBarScopeContainer = document.createElement("div");
        this._timelineEventsTitleBarScopeContainer.classList.add(WebInspector.TimelineSidebarPanel.TitleBarScopeBarStyleClass);
        this._timelineEventsTitleBarContainer.appendChild(this._timelineEventsTitleBarScopeContainer);

        this.element.insertBefore(this._timelineEventsTitleBarContainer, this.element.firstChild);

        this.contentTreeOutlineLabel = "";

        this._timelinesContentContainerElement = document.createElement("div");
        this._timelinesContentContainerElement.classList.add(WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass);
        this.element.insertBefore(this._timelinesContentContainerElement, this.element.firstChild);

        this._displayedRecording = null;
        this._displayedContentView = null;
        this._viewMode = null;
        this._previousSelectedTimelineType = null;

        // Maintain an invisible tree outline containing tree elements for all recordings.
        // The visible recording's tree element is selected when the content view changes.
        this._recordingTreeElementMap = new Map;
        this._recordingsTreeOutline = this.createContentTreeOutline(true, true);
        this._recordingsTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
        this._recordingsTreeOutline.hidden = true;
        this._recordingsTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._recordingsTreeSelectionDidChange, this);
        this._timelinesContentContainerElement.appendChild(this._recordingsTreeOutline.element);

        // Maintain a tree outline with tree elements for each timeline of the selected recording.
        this._timelinesTreeOutline = this.createContentTreeOutline(true, true);
        this._timelinesTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
        this._timelinesTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._timelinesTreeSelectionDidChange, this);
        this._timelinesContentContainerElement.appendChild(this._timelinesTreeOutline.element);

        this._timelineTreeElementMap = new Map;

        this._basicTitleBar = document.createElement("div");
        this._basicTitleBar.textContent = WebInspector.UIString("Timelines");
        this._basicTitleBar.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
        this._basicTitleBar.classList.add(WebInspector.TimelineSidebarPanel.TimelinesTitleBarStyleClass);
        this.element.insertBefore(this._basicTitleBar, this.element.firstChild);

        if (WebInspector.FPSInstrument.supported()) {
            var timelinesNavigationItem = new WebInspector.RadioButtonNavigationItem(WebInspector.TimelineSidebarPanel.ViewMode.Timelines, WebInspector.UIString("Timelines"))
            var renderingFramesNavigationItem = new WebInspector.RadioButtonNavigationItem(WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames, WebInspector.UIString("Rendering Frames"))
            this._viewModeNavigationBar = new WebInspector.NavigationBar(null, [timelinesNavigationItem, renderingFramesNavigationItem], "tablist");
            this._viewModeNavigationBar.addEventListener(WebInspector.NavigationBar.Event.NavigationItemSelected, this._viewModeSelected, this);

            this._renderingFramesTitleBar = document.createElement("div");
            this._renderingFramesTitleBar.className = "navigation-bar-container";
            this._renderingFramesTitleBar.appendChild(this._viewModeNavigationBar.element);
            this._renderingFramesTitleBar.hidden = true;
            this.element.insertBefore(this._renderingFramesTitleBar, this.element.firstChild);

            this._chartColors = new Map;
            this._chartColors.set(WebInspector.RenderingFrameTimelineRecord.TaskType.Script, "rgb(153, 113, 185)");
            this._chartColors.set(WebInspector.RenderingFrameTimelineRecord.TaskType.Layout, "rgb(212, 108, 108)");
            this._chartColors.set(WebInspector.RenderingFrameTimelineRecord.TaskType.Paint, "rgb(152, 188, 77)");
            this._chartColors.set(WebInspector.RenderingFrameTimelineRecord.TaskType.Other, "rgb(221, 221, 221)");

            this._frameSelectionChartRow = new WebInspector.ChartDetailsSectionRow(this, 74, 0.5);
            this._frameSelectionChartRow.addEventListener(WebInspector.ChartDetailsSectionRow.Event.LegendItemChecked, this._frameSelectionLegendItemChecked, this);

            for (let key in WebInspector.RenderingFrameTimelineRecord.TaskType) {
                let taskType = WebInspector.RenderingFrameTimelineRecord.TaskType[key];
                let label = WebInspector.RenderingFrameTimelineRecord.displayNameForTaskType(taskType);
                let color = this._chartColors.get(taskType);
                let checkbox = taskType !== WebInspector.RenderingFrameTimelineRecord.TaskType.Other;
                this._frameSelectionChartRow.addItem(taskType, label, 0, color, checkbox, true);
            }

            this._renderingFrameTaskFilter = new Set;

            var chartGroup = new WebInspector.DetailsSectionGroup([this._frameSelectionChartRow]);
            this._frameSelectionChartSection = new WebInspector.DetailsSection("frames-selection-chart", WebInspector.UIString("Selected Frames"), [chartGroup], null, true);
            this._timelinesContentContainerElement.appendChild(this._frameSelectionChartSection.element);
        }

        var statusBarElement = this._statusBarElement = document.createElement("div");
        statusBarElement.classList.add(WebInspector.TimelineSidebarPanel.StatusBarStyleClass);
        this.element.insertBefore(statusBarElement, this.element.firstChild);

        this._recordGlyphElement = document.createElement("div");
        this._recordGlyphElement.className = WebInspector.TimelineSidebarPanel.RecordGlyphStyleClass;
        this._recordGlyphElement.title = WebInspector.UIString("Click or press the spacebar to record.")
        this._recordGlyphElement.addEventListener("mouseover", this._recordGlyphMousedOver.bind(this));
        this._recordGlyphElement.addEventListener("mouseout", this._recordGlyphMousedOut.bind(this));
        this._recordGlyphElement.addEventListener("click", this._recordGlyphClicked.bind(this));
        statusBarElement.appendChild(this._recordGlyphElement);

        this._recordStatusElement = document.createElement("div");
        this._recordStatusElement.className = WebInspector.TimelineSidebarPanel.RecordStatusStyleClass;
        statusBarElement.appendChild(this._recordStatusElement);

        WebInspector.showReplayInterfaceSetting.addEventListener(WebInspector.Setting.Event.Changed, this._updateReplayInterfaceVisibility, this);

        // We always create a replay navigation bar; its visibility is controlled by WebInspector.showReplayInterfaceSetting.
        this._replayNavigationBar = new WebInspector.NavigationBar;
        this.element.appendChild(this._replayNavigationBar.element);

        var toolTip = WebInspector.UIString("Begin Capturing");
        var altToolTip = WebInspector.UIString("End Capturing");
        this._replayCaptureButtonItem = new WebInspector.ActivateButtonNavigationItem("replay-capture", toolTip, altToolTip, "Images/Circle.svg", 16, 16);
        this._replayCaptureButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._replayCaptureButtonClicked, this);
        this._replayCaptureButtonItem.enabled = true;
        this._replayNavigationBar.addNavigationItem(this._replayCaptureButtonItem);

        toolTip = WebInspector.UIString("Start Playback");
        altToolTip = WebInspector.UIString("Pause Playback");
        this._replayPauseResumeButtonItem = new WebInspector.ToggleButtonNavigationItem("replay-pause-resume", toolTip, altToolTip, "Images/Resume.svg", "Images/Pause.svg", 15, 15);
        this._replayPauseResumeButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._replayPauseResumeButtonClicked, this);
        this._replayPauseResumeButtonItem.enabled = false;
        this._replayNavigationBar.addNavigationItem(this._replayPauseResumeButtonItem);

        WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStarted, this._captureStarted, this);
        WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStopped, this._captureStopped, this);

        this._statusBarElement.oncontextmenu = this._contextMenuNavigationBarOrStatusBar.bind(this);
        this._replayNavigationBar.element.oncontextmenu = this._contextMenuNavigationBarOrStatusBar.bind(this);
        this._updateReplayInterfaceVisibility();

        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingCreated, this._recordingCreated, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingLoaded, this._recordingLoaded, this);

        this.contentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange, this._contentBrowserCurrentContentViewDidChange, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);

        for (var recording of WebInspector.timelineManager.recordings)
            this._addRecording(recording);

        this._recordingCountChanged();

        if (WebInspector.timelineManager.activeRecording)
            this._recordingLoaded();

        this._toggleRecordingShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Space, this._toggleRecordingOnSpacebar.bind(this));
        this._toggleRecordingShortcut.implicitlyPreventsDefault = false;
        this._toggleRecordingShortcut.disabled = true;

        this._toggleNewRecordingShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Space, this._toggleNewRecordingOnSpacebar.bind(this));
        this._toggleNewRecordingShortcut.implicitlyPreventsDefault = false;
        this._toggleNewRecordingShortcut.disabled = true;
    }

    // Static

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
        }

        console.error("Unknown Timeline type:", timeline.type);
    }

    static iconClassNameForTimeline(timeline)
    {
        switch (timeline.type) {
        case WebInspector.TimelineRecord.Type.Network:
            return "network-icon";
        case WebInspector.TimelineRecord.Type.Layout:
            return "colors-icon";
        case WebInspector.TimelineRecord.Type.Script:
            return "script-icon";
        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return "rendering-frame-icon";
        }

        console.error("Unknown Timeline type:", timeline.type);
    }

    // Public

    shown()
    {
        super.shown();

        if (this._displayedContentView)
            this.contentBrowser.showContentView(this._displayedContentView);

        if (this.viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames)
            this._refreshFrameSelectionChart();

        this._toggleRecordingShortcut.disabled = false;
        this._toggleNewRecordingShortcut.disabled = false;

        this._updateTimelineOverviewHeight();
    }

    hidden()
    {
        super.hidden();

        this._toggleRecordingShortcut.disabled = true;
        this._toggleNewRecordingShortcut.disabled = true;
    }

    closed()
    {
        super.closed();

        WebInspector.showReplayInterfaceSetting.removeEventListener(null, null, this);
        WebInspector.replayManager.removeEventListener(null, null, this);
        WebInspector.timelineManager.removeEventListener(null, null, this);

        WebInspector.timelineManager.reset();
    }

    get viewMode()
    {
        return this._viewMode;
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

    get contentTreeOutlineScopeBar()
    {
        return this._timelineEventsTitleBarScopeContainer.children;
    }

    set contentTreeOutlineScopeBar(scopeBar)
    {
        this._timelineEventsTitleBarScopeContainer.removeChildren();

        if (!scopeBar || !scopeBar.element)
            return;

        this._timelineEventsTitleBarScopeContainer.appendChild(scopeBar.element);
    }

    showTimelineOverview()
    {
        if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();

        this._displayedContentView.showOverviewTimelineView();
        this.contentBrowser.showContentView(this._displayedContentView);

        var selectedByUser = false;
        this._changeViewMode(WebInspector.TimelineSidebarPanel.ViewMode.Timelines, selectedByUser);
    }

    showTimelineViewForTimeline(timeline)
    {
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._displayedRecording.timelines.has(timeline.type), "Cannot show timeline because it does not belong to the shown recording.", timeline.type);

        // The sidebar view mode must be in the correct state before changing the content view.
        var selectedByUser = false;
        this._changeViewMode(this._viewModeForTimeline(timeline), selectedByUser);

        if (this._timelineTreeElementMap.has(timeline)) {
            // Defer showing the relevant timeline to the onselect handler of the timelines tree element.
            var wasSelectedByUser = true;
            var shouldSuppressOnSelect = false;
            this._timelineTreeElementMap.get(timeline).select(true, wasSelectedByUser, shouldSuppressOnSelect, true);
        } else {
            this._displayedContentView.showTimelineViewForTimeline(timeline);
            this.contentBrowser.showContentView(this._displayedContentView);
        }
    }

    updateFrameSelection(startFrameIndex, endFrameIndex)
    {
        console.assert(startFrameIndex <= endFrameIndex, startFrameIndex, endFrameIndex);
        console.assert(this.viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames, this._viewMode);
        if (this._startFrameIndex === startFrameIndex && this._endFrameIndex === endFrameIndex)
            return;

        this._startFrameIndex = startFrameIndex;
        this._endFrameIndex = endFrameIndex;

        this._refreshFrameSelectionChart();
    }

    formatChartValue(value)
    {
        return this._frameSelectionChartRow.total === 0 ? "" : Number.secondsToString(value);
    }

    // Protected

    representedObjectWasFiltered(representedObject, filtered)
    {
        super.representedObjectWasFiltered(representedObject, filtered);

        if (representedObject instanceof WebInspector.TimelineRecord)
            this._displayedContentView.recordWasFiltered(representedObject, filtered);
    }

    updateFilter()
    {
        super.updateFilter();

        this._displayedContentView.filterDidChange();
    }

    shouldFilterPopulate()
    {
        return false;
    }

    hasCustomFilters()
    {
        return true;
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        if (!this._displayedContentView)
            return true;

        if (this._viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames && this._renderingFrameTaskFilter.size) {
            while (treeElement && !(treeElement.record instanceof WebInspector.TimelineRecord))
                treeElement = treeElement.parent;

            console.assert(treeElement, "Cannot apply task filter: no TimelineRecord found.");
            if (!treeElement)
                return false;

            let records;
            if (treeElement.record instanceof WebInspector.RenderingFrameTimelineRecord)
                records = treeElement.record.children;
            else
                records = [treeElement.record];

            const filtered = records.every(function(record) {
                var taskType = WebInspector.RenderingFrameTimelineRecord.taskTypeForTimelineRecord(record);
                return this._renderingFrameTaskFilter.has(taskType);
            }, this);

            if (filtered)
                return false;
        }

        return this._displayedContentView.matchTreeElementAgainstCustomFilters(treeElement);
    }

    treeElementAddedOrChanged(treeElement)
    {
        if (treeElement.status)
            return;

        if (!treeElement.treeOutline || typeof treeElement.treeOutline.__canShowContentViewForTreeElement !== "function")
            return;

        if (!treeElement.treeOutline.__canShowContentViewForTreeElement(treeElement))
            return;

        var fragment = document.createDocumentFragment();

        var closeButton = new WebInspector.TreeElementStatusButton(useSVGSymbol("Images/Close.svg", null, WebInspector.UIString("Close resource view")));
        closeButton.element.classList.add("close");
        closeButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._treeElementCloseButtonClicked, this);
        fragment.appendChild(closeButton.element);

        var goToButton = new WebInspector.TreeElementStatusButton(WebInspector.createGoToArrowButton());
        goToButton.__treeElement = treeElement;
        goToButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._treeElementGoToArrowWasClicked, this);
        fragment.appendChild(goToButton.element);

        treeElement.status = fragment;
    }

    canShowDifferentContentView()
    {
        if (this._clickedTreeElementGoToArrow)
            return true;

        if (this.contentBrowser.currentContentView instanceof WebInspector.TimelineRecordingContentView)
            return false;

        return !this.restoringState || !this._restoredShowingTimelineRecordingContentView;
    }

    saveStateToCookie(cookie)
    {
        console.assert(cookie);

        cookie[WebInspector.TimelineSidebarPanel.ShowingTimelineRecordingContentViewCookieKey] = this.contentBrowser.currentContentView instanceof WebInspector.TimelineRecordingContentView;

        if (this._viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames)
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineRecord.Type.RenderingFrame;
        else {
            var selectedTreeElement = this._timelinesTreeOutline.selectedTreeElement;
            if (selectedTreeElement)
                cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = selectedTreeElement.representedObject.type;
            else
                cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue;    
        }

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
        if (selectedTimelineViewIdentifier === WebInspector.TimelineRecord.Type.RenderingFrame && !WebInspector.FPSInstrument.supported())
            selectedTimelineViewIdentifier = null;

        if (selectedTimelineViewIdentifier && this._displayedRecording.timelines.has(selectedTimelineViewIdentifier))
            this.showTimelineViewForTimeline(this._displayedRecording.timelines.get(selectedTimelineViewIdentifier));
        else
            this.showTimelineOverview();

        // Don't call NavigationSidebarPanel.restoreStateFromCookie, because it tries to match based
        // on type only as a last resort. This would cause the first recording to be reselected on reload.
    }

    // Private

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
        if (WebInspector.timelineManager.isCapturing()) {
            WebInspector.timelineManager.stopCapturing();

            this._recordGlyphElement.title = WebInspector.UIString("Click or press the spacebar to record.")
        } else {
            WebInspector.timelineManager.startCapturing(shouldCreateRecording);
            // Show the timeline to which events will be appended.
            this._recordingLoaded();

            this._recordGlyphElement.title = WebInspector.UIString("Click or press the spacebar to stop recording.")
        }
    }

    _treeElementGoToArrowWasClicked(event)
    {
        this._clickedTreeElementGoToArrow = true;

        var treeElement = event.target.__treeElement;
        console.assert(treeElement instanceof WebInspector.TreeElement);

        treeElement.select(true, true);

        this._clickedTreeElementGoToArrow = false;
    }

    _treeElementCloseButtonClicked(event)
    {
        var currentTimelineView = this._displayedContentView ? this._displayedContentView.currentTimelineView : null;
        if (currentTimelineView && currentTimelineView.representedObject instanceof WebInspector.Timeline)
            this.showTimelineViewForTimeline(currentTimelineView.representedObject);
        else
            this.showTimelineOverview();
    }

    _recordingsTreeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        console.assert(treeElement.representedObject instanceof WebInspector.TimelineRecording);

        this._recordingSelected(treeElement.representedObject);
    }

    _renderingFrameTimelineTimesUpdated(event)
    {
        if (this.viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames)
            this._refreshFrameSelectionChart();
    }

    _timelinesTreeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        console.assert(this._timelineTreeElementMap.get(treeElement.representedObject) === treeElement, treeElement);

        // If not selected by user, then this selection merely synced the tree element with the content view's contents.
        let selectedByUser = event.data.selectedByUser;
        if (!selectedByUser) {
            console.assert(this._displayedContentView.currentTimelineView.representedObject === treeElement.representedObject);
            return;
        }

        let timeline = treeElement.representedObject;
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._displayedRecording.timelines.get(timeline.type) === timeline, timeline);

        this._previousSelectedTimelineType = timeline.type;

        this._displayedContentView.showTimelineViewForTimeline(timeline);
        this.contentBrowser.showContentView(this._displayedContentView);
    }

    _contentBrowserCurrentContentViewDidChange(event)
    {
        var didShowTimelineRecordingContentView = this.contentBrowser.currentContentView instanceof WebInspector.TimelineRecordingContentView;
        this.element.classList.toggle(WebInspector.TimelineSidebarPanel.TimelineRecordingContentViewShowingStyleClass, didShowTimelineRecordingContentView);

        if (this.viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames)
            this._refreshFrameSelectionChart();
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
        this._addRecording(event.data.recording)
        this._recordingCountChanged();
    }

    _addRecording(recording)
    {
        console.assert(recording instanceof WebInspector.TimelineRecording, recording);

        var recordingTreeElement = new WebInspector.GeneralTreeElement(WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass, recording.displayName, null, recording);
        this._recordingTreeElementMap.set(recording, recordingTreeElement);
        this._recordingsTreeOutline.appendChild(recordingTreeElement);
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
            oldRecording.removeEventListener(WebInspector.TimelineRecording.Event.InstrumentAdded, this._instrumentAdded, this);
            oldRecording.removeEventListener(WebInspector.TimelineRecording.Event.InstrumentRemoved, this._instrumentRemoved, this);

            // Destroy tree elements in one operation to avoid unnecessary fixups.
            this._timelinesTreeOutline.removeChildren();
            this._timelineTreeElementMap.clear();
        }

        this._clearInstruments();

        this._displayedRecording = recording;
        this._displayedRecording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentAdded, this._instrumentAdded, this);
        this._displayedRecording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentRemoved, this._instrumentRemoved, this);

        for (let instrument of recording.instruments)
            this._instrumentAdded(instrument);

        // Save the current state incase we need to restore it to a new recording.
        var cookie = {};
        this.saveStateToCookie(cookie);

        // Try to get the recording content view if it exists already, if it does we don't want to restore the cookie.
        var onlyExisting = true;
        this._displayedContentView = this.contentBrowser.contentViewForRepresentedObject(this._displayedRecording, onlyExisting, {timelineSidebarPanel: this});
        if (this._displayedContentView) {
            // Show the timeline that was being shown to update the sidebar tree state.
            var currentTimelineView = this._displayedContentView.currentTimelineView;
            if (currentTimelineView && currentTimelineView.representedObject instanceof WebInspector.Timeline)
                this.showTimelineViewForTimeline(currentTimelineView.representedObject);
            else
                this.showTimelineOverview();

            this.updateFilter();
            return;
        }

        onlyExisting = false;
        this._displayedContentView = this.contentBrowser.contentViewForRepresentedObject(this._displayedRecording, onlyExisting, {timelineSidebarPanel: this});

        // Restore the cookie to carry over the previous recording view state to the new recording.
        this.restoreStateFromCookie(cookie);

        this.updateFilter();
    }

    _recordingLoaded(event)
    {
        this._recordingSelected(WebInspector.timelineManager.activeRecording);
    }

    _clearInstruments()
    {
        this._timelineTreeElementMap.clear();

        if (WebInspector.FPSInstrument.supported())
            this._removedFPSInstrument();
    }

    _instrumentAdded(instrumentOrEvent)
    {
        let instrument = instrumentOrEvent instanceof WebInspector.Instrument ? instrumentOrEvent : instrumentOrEvent.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._displayedRecording.timelineForInstrument(instrument);
        console.assert(!this._timelineTreeElementMap.has(timeline), timeline);

        if (timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame) {
            this._addedFPSInstrument();
            timeline.addEventListener(WebInspector.Timeline.Event.TimesUpdated, this._renderingFrameTimelineTimesUpdated, this);
            return;
        }

        let displayName = WebInspector.TimelineSidebarPanel.displayNameForTimeline(timeline);
        let iconClassName = WebInspector.TimelineSidebarPanel.iconClassNameForTimeline(timeline);
        let timelineTreeElement = new WebInspector.GeneralTreeElement([iconClassName, WebInspector.TimelineSidebarPanel.LargeIconStyleClass], displayName, null, timeline);
        let tooltip = WebInspector.UIString("Close %s timeline view").format(displayName);
        let button = new WebInspector.TreeElementStatusButton(useSVGSymbol("Images/CloseLarge.svg", "close-button", tooltip));
        button.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this.showTimelineOverview, this);
        timelineTreeElement.status = button.element;

        this._timelinesTreeOutline.appendChild(timelineTreeElement);
        this._timelineTreeElementMap.set(timeline, timelineTreeElement);

        this._timelineCountChanged();
    }

    _instrumentRemoved(event)
    {
        let instrument = event.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._displayedRecording.timelineForInstrument(instrument);

        if (timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame) {
            this._removedFPSInstrument();
            timeline.removeEventListener(WebInspector.Timeline.Event.TimesUpdated, this._renderingFrameTimelineTimesUpdated, this);
            return;
        }

        console.assert(this._timelineTreeElementMap.has(timeline), timeline);

        let timelineTreeElement = this._timelineTreeElementMap.take(timeline);
        let shouldSuppressOnDeselect = false;
        let shouldSuppressSelectSibling = true;
        this._timelinesTreeOutline.removeChild(timelineTreeElement, shouldSuppressOnDeselect, shouldSuppressSelectSibling);
        this._timelineTreeElementMap.delete(timeline);

        this._timelineCountChanged();
    }

    _addedFPSInstrument()
    {
        this._basicTitleBar.hidden = true;
        this._renderingFramesTitleBar.hidden = false;
    }

    _removedFPSInstrument()
    {
        this._basicTitleBar.hidden = false;
        this._renderingFramesTitleBar.hidden = true;
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

        this._updateTimelineOverviewHeight();
    }

    _updateTimelineOverviewHeight()
    {
        const timelineHeight = 36;
        const eventTitleBarOffset = 58;
        const contentElementOffset = 81;
        const renderingFramesTimelineHeight = 108;

        if (!this._displayedContentView)
            return;

        let overviewHeight;
        let currentTimelineView = this._displayedContentView.currentTimelineView;
        if (currentTimelineView && currentTimelineView.representedObject.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            overviewHeight = renderingFramesTimelineHeight;
        else {
            let timelineCount = this._timelineTreeElementMap.size;
            if (this._timelineTreeElementMap.has(WebInspector.TimelineRecord.Type.RenderingFrame))
                timelineCount--;
            overviewHeight = timelineCount * timelineHeight;
        }

        this._timelineEventsTitleBarContainer.style.top = (overviewHeight + eventTitleBarOffset) + "px";
        this.contentElement.style.top = (overviewHeight + contentElementOffset) + "px";
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

        this._toggleRecording(event.shiftKey);
    }

    _viewModeSelected(event)
    {
        console.assert(event.target.selectedNavigationItem);
        if (!event.target.selectedNavigationItem)
            return;

        var selectedNavigationItem = event.target.selectedNavigationItem;
        var selectedByUser = true;
        this._changeViewMode(selectedNavigationItem.identifier, selectedByUser);
    }

    _viewModeForTimeline(timeline)
    {
        if (timeline && timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            return WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames;

        return WebInspector.TimelineSidebarPanel.ViewMode.Timelines;
    }

    _changeViewMode(mode, selectedByUser)
    {
        if (!WebInspector.FPSInstrument.supported() || this._viewMode === mode)
            return;

        this._viewMode = mode;
        this._viewModeNavigationBar.selectedNavigationItem = this._viewMode;

        if (this._viewMode === WebInspector.TimelineSidebarPanel.ViewMode.Timelines) {
            this._timelinesTreeOutline.hidden = false;
            this._frameSelectionChartSection.collapsed = true;
        } else {
            this._timelinesTreeOutline.hidden = true;
            this._frameSelectionChartSection.collapsed = false;
        }

        if (selectedByUser) {
            var timelineType = this._previousSelectedTimelineType;
            if (this._viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames) {
                this._previousSelectedTimelineType = this._timelinesTreeOutline.selectedTreeElement ? this._timelinesTreeOutline.selectedTreeElement.representedObject.type : null;
                timelineType = WebInspector.TimelineRecord.Type.RenderingFrame;
            }

            if (timelineType) {
                console.assert(this._displayedRecording.timelines.has(timelineType), timelineType);
                this.showTimelineViewForTimeline(this._displayedRecording.timelines.get(timelineType));
            } else
                this.showTimelineOverview();
        }

        this._timelineCountChanged();

        this.updateFilter();
    }

    _frameSelectionLegendItemChecked(event)
    {
        if (event.data.checked)
            this._renderingFrameTaskFilter.delete(event.data.id);
        else
            this._renderingFrameTaskFilter.add(event.data.id);

        this.updateFilter();
    }

    _refreshFrameSelectionChart()
    {
        if (!this.visible)
            return;

        function getSelectedRecords()
        {
            console.assert(this._displayedRecording);
            console.assert(this._displayedRecording.timelines.has(WebInspector.TimelineRecord.Type.RenderingFrame), "Cannot find rendering frames timeline in displayed recording");

            var timeline = this._displayedRecording.timelines.get(WebInspector.TimelineRecord.Type.RenderingFrame);
            var selectedRecords = [];
            for (var record of timeline.records) {
                console.assert(record instanceof WebInspector.RenderingFrameTimelineRecord);
                // If this frame is completely before the bounds of the graph, skip this record.
                if (record.frameIndex < this._startFrameIndex)
                    continue;

                // If this record is completely after the end time, break out now.
                // Records are sorted, so all records after this will be beyond the end time too.
                if (record.frameIndex > this._endFrameIndex)
                    break;

                selectedRecords.push(record);
            }

            return selectedRecords;
        }

        let records = getSelectedRecords.call(this);
        for (let key in WebInspector.RenderingFrameTimelineRecord.TaskType) {
            let taskType = WebInspector.RenderingFrameTimelineRecord.TaskType[key];
            let value = records.reduce(function(previousValue, currentValue) { return previousValue + currentValue.durationForTask(taskType); }, 0);
            this._frameSelectionChartRow.setItemValue(taskType, value);
        }

        if (!records.length) {
            this._frameSelectionChartRow.title = WebInspector.UIString("Frames: None Selected");
            return;
        }

        var firstRecord = records[0];
        var lastRecord = records.lastValue;

        if (records.length > 1) {
            this._frameSelectionChartRow.title = WebInspector.UIString("Frames: %d \u2013 %d (%s \u2013 %s)").format(firstRecord.frameNumber, lastRecord.frameNumber,
                Number.secondsToString(firstRecord.startTime), Number.secondsToString(lastRecord.endTime));
        } else {
            this._frameSelectionChartRow.title = WebInspector.UIString("Frame: %d (%s \u2013 %s)").format(firstRecord.frameNumber,
                Number.secondsToString(firstRecord.startTime), Number.secondsToString(lastRecord.endTime));
        }
    }

    // These methods are only used when ReplayAgent is available.

    _updateReplayInterfaceVisibility()
    {
        var shouldShowReplayInterface = !!(window.ReplayAgent && WebInspector.showReplayInterfaceSetting.value);

        this._statusBarElement.classList.toggle(WebInspector.TimelineSidebarPanel.HiddenStyleClassName, shouldShowReplayInterface);
        this._replayNavigationBar.element.classList.toggle(WebInspector.TimelineSidebarPanel.HiddenStyleClassName, !shouldShowReplayInterface);
    }

    _contextMenuNavigationBarOrStatusBar()
    {
        if (!window.ReplayAgent)
            return;

        let toggleReplayInterface = () => {
            WebInspector.showReplayInterfaceSetting.value = !WebInspector.showReplayInterfaceSetting.value;
        };

        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);
        if (WebInspector.showReplayInterfaceSetting.value)
            contextMenu.appendItem(WebInspector.UIString("Hide Replay Controls"), toggleReplayInterface);
        else
            contextMenu.appendItem(WebInspector.UIString("Show Replay Controls"), toggleReplayInterface);
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

WebInspector.TimelineSidebarPanel.ViewMode = {
    Timelines: "timeline-sidebar-panel-view-mode-timelines",
    RenderingFrames: "timeline-sidebar-panel-view-mode-frames"
};

WebInspector.TimelineSidebarPanel.HiddenStyleClassName = "hidden";
WebInspector.TimelineSidebarPanel.StatusBarStyleClass = "status-bar";
WebInspector.TimelineSidebarPanel.RecordGlyphStyleClass = "record-glyph";
WebInspector.TimelineSidebarPanel.RecordGlyphRecordingStyleClass = "recording";
WebInspector.TimelineSidebarPanel.RecordGlyphRecordingForcedStyleClass = "forced";
WebInspector.TimelineSidebarPanel.RecordStatusStyleClass = "record-status";
WebInspector.TimelineSidebarPanel.TitleBarStyleClass = "title-bar";
WebInspector.TimelineSidebarPanel.TitleBarTextStyleClass = "title-bar-text";
WebInspector.TimelineSidebarPanel.TitleBarScopeBarStyleClass = "title-bar-scope-bar";
WebInspector.TimelineSidebarPanel.TimelinesTitleBarStyleClass = "timelines";
WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass = "timeline-events";
WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass = "timelines-content";
WebInspector.TimelineSidebarPanel.LargeIconStyleClass = "large";
WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass = "stopwatch-icon";
WebInspector.TimelineSidebarPanel.TimelineRecordingContentViewShowingStyleClass = "timeline-recording-content-view-showing";

WebInspector.TimelineSidebarPanel.ShowingTimelineRecordingContentViewCookieKey = "timeline-sidebar-panel-showing-timeline-recording-content-view";
WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey = "timeline-sidebar-panel-selected-timeline-view-identifier";
WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue = "overview";
