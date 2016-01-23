/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2015 University of Washington.
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

WebInspector.TimelineRecordingContentView = class TimelineRecordingContentView extends WebInspector.ContentView
{
    constructor(recording, extraArguments)
    {
        console.assert(extraArguments);
        console.assert(extraArguments.timelineSidebarPanel instanceof WebInspector.TimelineSidebarPanel);

        super(recording);

        this._recording = recording;
        this._timelineSidebarPanel = extraArguments.timelineSidebarPanel;

        this.element.classList.add("timeline-recording");

        this._linearTimelineOverview = new WebInspector.LinearTimelineOverview(this._recording);
        this._linearTimelineOverview.addEventListener(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);

        this._renderingFrameTimelineOverview = new WebInspector.RenderingFrameTimelineOverview(this._recording);
        this._renderingFrameTimelineOverview.addEventListener(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);
        this._renderingFrameTimelineOverview.addEventListener(WebInspector.TimelineOverview.Event.RecordSelected, this._recordSelected, this);

        this._currentTimelineOverview = this._linearTimelineOverview;

        this.addSubview(this._currentTimelineOverview);

        this._contentViewContainer = new WebInspector.ContentViewContainer;
        this._contentViewContainer.addEventListener(WebInspector.ContentViewContainer.Event.CurrentContentViewDidChange, this._currentContentViewDidChange, this);
        this.addSubview(this._contentViewContainer);

        this._clearTimelineNavigationItem = new WebInspector.ButtonNavigationItem("clear-timeline", WebInspector.UIString("Clear Timeline"), "Images/NavigationItemTrash.svg", 15, 15);
        this._clearTimelineNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._clearTimeline, this);

        this._overviewTimelineView = new WebInspector.OverviewTimelineView(recording, {timelineSidebarPanel: this._timelineSidebarPanel});
        this._overviewTimelineView.secondsPerPixel = this._linearTimelineOverview.secondsPerPixel;

        this._timelineViewMap = new Map;
        this._pathComponentMap = new Map;

        this._updating = false;
        this._currentTime = NaN;
        this._lastUpdateTimestamp = NaN;
        this._startTimeNeedsReset = true;
        this._renderingFrameTimeline = null;

        this._recording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentAdded, this._instrumentAdded, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentRemoved, this._instrumentRemoved, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.Reset, this._recordingReset, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.Unloaded, this._recordingUnloaded, this);

        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);

        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerPaused, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerResumed, this);

        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._contentViewSelectionPathComponentDidChange, this);
        WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange, this._contentViewSupplementalRepresentedObjectsDidChange, this);

        for (let instrument of this._recording.instruments)
            this._instrumentAdded(instrument);

        this.showOverviewTimelineView();
    }

    // Public

    showOverviewTimelineView()
    {
        this._contentViewContainer.showContentView(this._overviewTimelineView);
    }

    showTimelineViewForTimeline(timeline)
    {
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._timelineViewMap.has(timeline), timeline);
        if (!this._timelineViewMap.has(timeline))
            return;

        this._contentViewContainer.showContentView(this._timelineViewMap.get(timeline));
    }

    get supportsSplitContentBrowser()
    {
        // The layout of the overview and split content browser don't work well.
        return false;
    }

    get selectionPathComponents()
    {
        if (!this._contentViewContainer.currentContentView)
            return [];

        var pathComponents = this._contentViewContainer.currentContentView.selectionPathComponents || [];
        var representedObject = this._contentViewContainer.currentContentView.representedObject;
        if (representedObject instanceof WebInspector.Timeline)
            pathComponents.unshift(this._pathComponentMap.get(representedObject));
        return pathComponents;
    }

    get supplementalRepresentedObjects()
    {
        if (!this._contentViewContainer.currentContentView)
            return [];
        return this._contentViewContainer.currentContentView.supplementalRepresentedObjects;
    }

    get navigationItems()
    {
        return [this._clearTimelineNavigationItem];
    }

    get handleCopyEvent()
    {
        var currentContentView = this._contentViewContainer.currentContentView;
        return currentContentView && typeof currentContentView.handleCopyEvent === "function" ? currentContentView.handleCopyEvent.bind(currentContentView) : null;
    }

    get supportsSave()
    {
        var currentContentView = this._contentViewContainer.currentContentView;
        return currentContentView && currentContentView.supportsSave;
    }

    get saveData()
    {
        var currentContentView = this._contentViewContainer.currentContentView;
        return currentContentView && currentContentView.saveData || null;
    }

    get currentTimelineView()
    {
        var contentView = this._contentViewContainer.currentContentView;
        return (contentView instanceof WebInspector.TimelineView) ? contentView : null;
    }

    shown()
    {
        this._currentTimelineOverview.shown();
        this._contentViewContainer.shown();
        this._clearTimelineNavigationItem.enabled = !this._recording.readonly;

        this._currentContentViewDidChange();

        if (!this._updating && WebInspector.timelineManager.activeRecording === this._recording && WebInspector.timelineManager.isCapturing())
            this._startUpdatingCurrentTime(this._currentTime);
    }

    hidden()
    {
        this._currentTimelineOverview.hidden();
        this._contentViewContainer.hidden();

        if (this._updating)
            this._stopUpdatingCurrentTime();
    }

    closed()
    {
        this._contentViewContainer.closeAllContentViews();

        this._recording.removeEventListener(null, null, this);

        WebInspector.timelineManager.removeEventListener(null, null, this);
        WebInspector.debuggerManager.removeEventListener(null, null, this);
        WebInspector.ContentView.removeEventListener(null, null, this);
    }

    canGoBack()
    {
        return this._contentViewContainer.canGoBack();
    }

    canGoForward()
    {
        return this._contentViewContainer.canGoForward();
    }

    goBack()
    {
        this._contentViewContainer.goBack();
    }

    goForward()
    {
        this._contentViewContainer.goForward();
    }

    saveToCookie(cookie)
    {
        cookie.type = WebInspector.ContentViewCookieType.Timelines;

        var currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView || currentContentView === this._overviewTimelineView)
            cookie[WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey] = WebInspector.TimelineRecordingContentView.OverviewTimelineViewCookieValue;
        else if (currentContentView.representedObject instanceof WebInspector.Timeline)
            cookie[WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey] = this.currentTimelineView.representedObject.type;
    }

    restoreFromCookie(cookie)
    {
        var timelineType = cookie[WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey];
        if (timelineType === WebInspector.TimelineRecordingContentView.OverviewTimelineViewCookieValue)
            this.showOverviewTimelineView();
        else
            this.showTimelineViewForTimeline(this.representedObject.timelines.get(timelineType));
    }

    filterDidChange()
    {
        if (!this.currentTimelineView)
            return;

        this.currentTimelineView.filterDidChange();
    }

    recordWasFiltered(record, filtered)
    {
        if (!this.currentTimelineView)
            return;

        this._currentTimelineOverview.recordWasFiltered(this.currentTimelineView.representedObject, record, filtered);
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        if (this.currentTimelineView && !this.currentTimelineView.matchTreeElementAgainstCustomFilters(treeElement))
            return false;

        var startTime = this._currentTimelineOverview.selectionStartTime;
        var endTime = startTime + this._currentTimelineOverview.selectionDuration;
        var currentTime = this._currentTime || this._recording.startTime;

        if (this._timelineSidebarPanel.viewMode === WebInspector.TimelineSidebarPanel.ViewMode.RenderingFrames) {
            console.assert(this._renderingFrameTimeline);

            if (this._renderingFrameTimeline && this._renderingFrameTimeline.records.length) {
                var records = this._renderingFrameTimeline.records;
                var startIndex = this._currentTimelineOverview.timelineRuler.snapInterval ? startTime : Math.floor(startTime);
                if (startIndex >= records.length)
                    return false;

                var endIndex = this._currentTimelineOverview.timelineRuler.snapInterval ? endTime - 1: Math.floor(endTime);
                endIndex = Math.min(endIndex, records.length - 1);
                console.assert(startIndex <= endIndex, startIndex);

                startTime = records[startIndex].startTime;
                endTime = records[endIndex].endTime;
            }
        }

        function checkTimeBounds(itemStartTime, itemEndTime)
        {
            itemStartTime = itemStartTime || currentTime;
            itemEndTime = itemEndTime || currentTime;

            return startTime <= itemEndTime && itemStartTime <= endTime;
        }

        if (treeElement instanceof WebInspector.ResourceTreeElement) {
            var resource = treeElement.resource;
            return checkTimeBounds(resource.requestSentTimestamp, resource.finishedOrFailedTimestamp);
        }

        if (treeElement instanceof WebInspector.SourceCodeTimelineTreeElement) {
            var sourceCodeTimeline = treeElement.sourceCodeTimeline;

            // Do a quick check of the timeline bounds before we check each record.
            if (!checkTimeBounds(sourceCodeTimeline.startTime, sourceCodeTimeline.endTime))
                return false;

            for (var record of sourceCodeTimeline.records) {
                if (checkTimeBounds(record.startTime, record.endTime))
                    return true;
            }

            return false;
        }

        if (treeElement instanceof WebInspector.ProfileNodeTreeElement) {
            var profileNode = treeElement.profileNode;
            if (checkTimeBounds(profileNode.startTime, profileNode.endTime))
                return true;

            return false;
        }

        if (treeElement instanceof WebInspector.TimelineRecordTreeElement) {
            var record = treeElement.record;
            return checkTimeBounds(record.startTime, record.endTime);
        }

        console.error("Unknown TreeElement, can't filter by time.");
        return true;
    }

    // Protected

    layout()
    {
        this._currentTimelineOverview.updateLayoutForResize();

        let currentContentView = this._contentViewContainer.currentContentView;
        if (currentContentView && currentContentView.updateLayoutForResize)
            currentContentView.updateLayoutForResize();
    }

    // Private

    _currentContentViewDidChange(event)
    {
        let newTimelineOverview;
        let timelineView = this.currentTimelineView;
        if (timelineView && timelineView.representedObject.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            newTimelineOverview = this._renderingFrameTimelineOverview;
        else
            newTimelineOverview = this._linearTimelineOverview;

        if (newTimelineOverview !== this._currentTimelineOverview) {
            this._currentTimelineOverview.hidden();

            this.replaceSubview(this._currentTimelineOverview, newTimelineOverview);

            this._currentTimelineOverview = newTimelineOverview;
            this._currentTimelineOverview.shown();

            this._updateTimelineOverviewHeight();
        }

        if (timelineView) {
            this._timelineSidebarPanel.contentTreeOutline = timelineView.navigationSidebarTreeOutline;
            this._timelineSidebarPanel.contentTreeOutlineLabel = timelineView.navigationSidebarTreeOutlineLabel;
            this._timelineSidebarPanel.contentTreeOutlineScopeBar = timelineView.navigationSidebarTreeOutlineScopeBar;

            timelineView.startTime = newTimelineOverview.selectionStartTime;
            timelineView.endTime = newTimelineOverview.selectionStartTime + newTimelineOverview.selectionDuration;
            timelineView.currentTime = this._currentTime;
        }

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    }

    _pathComponentSelected(event)
    {
        this._timelineSidebarPanel.showTimelineViewForTimeline(event.data.pathComponent.representedObject);
    }

    _contentViewSelectionPathComponentDidChange(event)
    {
        if (event.target !== this._contentViewContainer.currentContentView)
            return;

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        if (this.currentTimelineView === this._overviewTimelineView)
            return;

        var recordPathComponent = this.selectionPathComponents.find(function(element) { return element.representedObject instanceof WebInspector.TimelineRecord; });
        var record = recordPathComponent ? recordPathComponent.representedObject : null;
        this._currentTimelineOverview.selectRecord(event.target.representedObject, record);
    }

    _contentViewSupplementalRepresentedObjectsDidChange(event)
    {
        if (event.target !== this._contentViewContainer.currentContentView)
            return;
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange);
    }

    _update(timestamp)
    {
        if (this._waitingToResetCurrentTime) {
            requestAnimationFrame(this._updateCallback);
            return;
        }

        var startTime = this._recording.startTime;
        var currentTime = this._currentTime || startTime;
        var endTime = this._recording.endTime;
        var timespanSinceLastUpdate = (timestamp - this._lastUpdateTimestamp) / 1000 || 0;

        currentTime += timespanSinceLastUpdate;

        this._updateTimes(startTime, currentTime, endTime);

        // Only stop updating if the current time is greater than the end time, or the end time is NaN.
        // The recording end time will be NaN if no records were added.
        if (!this._updating && (currentTime >= endTime || isNaN(endTime))) {
            if (this.visible)
                this._lastUpdateTimestamp = NaN;
            return;
        }

        this._lastUpdateTimestamp = timestamp;

        requestAnimationFrame(this._updateCallback);
    }

    _updateTimes(startTime, currentTime, endTime)
    {
        if (this._startTimeNeedsReset && !isNaN(startTime)) {
            var selectionOffset = this._linearTimelineOverview.selectionStartTime - this._linearTimelineOverview.startTime;

            this._linearTimelineOverview.startTime = startTime;
            this._linearTimelineOverview.selectionStartTime = startTime + selectionOffset;

            this._overviewTimelineView.zeroTime = startTime;
            for (var timelineView of this._timelineViewMap.values())
                timelineView.zeroTime = startTime;

            this._startTimeNeedsReset = false;
        }

        this._linearTimelineOverview.endTime = Math.max(endTime, currentTime);

        this._currentTime = currentTime;
        this._linearTimelineOverview.currentTime = currentTime;
        if (this.currentTimelineView)
            this.currentTimelineView.currentTime = currentTime;

        if (this._renderingFrameTimeline) {
            var currentFrameNumber = 0;
            if (this._renderingFrameTimeline.records.length)
                currentFrameNumber = this._renderingFrameTimeline.records.lastValue.frameNumber;

            this._renderingFrameTimelineOverview.currentTime = this._renderingFrameTimelineOverview.endTime = currentFrameNumber;
        }

        this._timelineSidebarPanel.updateFilter();

        // Force a layout now since we are already in an animation frame and don't need to delay it until the next.
        this._currentTimelineOverview.updateLayoutIfNeeded();
        if (this.currentTimelineView)
            this.currentTimelineView.updateLayoutIfNeeded();
    }

    _startUpdatingCurrentTime(startTime)
    {
        console.assert(!this._updating);
        if (this._updating)
            return;

        if (typeof startTime === "number")
            this._currentTime = startTime;
        else if (!isNaN(this._currentTime)) {
            // This happens when you stop and later restart recording.
            // COMPATIBILITY (iOS 9): Timeline.recordingStarted events did not include a timestamp.
            // We likely need to jump into the future to a better current time which we can
            // ascertained from a new incoming timeline record, so we wait for a Timeline to update.
            console.assert(!this._waitingToResetCurrentTime);
            this._waitingToResetCurrentTime = true;
            this._recording.addEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
        }

        this._updating = true;

        if (!this._updateCallback)
            this._updateCallback = this._update.bind(this);

        requestAnimationFrame(this._updateCallback);
    }

    _stopUpdatingCurrentTime()
    {
        console.assert(this._updating);
        this._updating = false;

        if (this._waitingToResetCurrentTime) {
            // Did not get any event while waiting for the current time, but we should stop waiting.
            this._recording.removeEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
            this._waitingToResetCurrentTime = false;
        }
    }

    _capturingStarted(event)
    {
        if (!this._updating)
            this._startUpdatingCurrentTime(event.data.startTime);
    }

    _capturingStopped(event)
    {
        if (this._updating)
            this._stopUpdatingCurrentTime();
    }

    _debuggerPaused(event)
    {
        if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Replaying)
            return;

        if (this._updating)
            this._stopUpdatingCurrentTime();
    }

    _debuggerResumed(event)
    {
        if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Replaying)
            return;

        if (!this._updating)
            this._startUpdatingCurrentTime();
    }

    _recordingTimesUpdated(event)
    {
        if (!this._waitingToResetCurrentTime)
            return;

        // COMPATIBILITY (iOS 9): Timeline.recordingStarted events did not include a new startTime.
        // Make the current time be the start time of the last added record. This is the best way
        // currently to jump to the right period of time after recording starts.

        for (var timeline of this._recording.timelines.values()) {
            var lastRecord = timeline.records.lastValue;
            if (!lastRecord)
                continue;
            this._currentTime = Math.max(this._currentTime, lastRecord.startTime);
        }

        this._recording.removeEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
        this._waitingToResetCurrentTime = false;
    }

    _clearTimeline(event)
    {
        if (WebInspector.timelineManager.activeRecording === this._recording && WebInspector.timelineManager.isCapturing())
            WebInspector.timelineManager.stopCapturing();

        this._recording.reset();
    }

    _updateTimelineOverviewHeight()
    {
        const rulerHeight = 29;
        const timelineHeight = 36;
        const renderingFramesTimelineHeight = 108;

        let overviewHeight;
        if (this.currentTimelineView && this.currentTimelineView.representedObject.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            overviewHeight = renderingFramesTimelineHeight;
        else {
            let timelineCount = this._timelineViewMap.size;
            if (this._renderingFrameTimeline)
                timelineCount--;

            overviewHeight = timelineCount * timelineHeight;
        }

        let styleValue = (overviewHeight + rulerHeight) + "px";
        this._currentTimelineOverview.element.style.height = styleValue;
        this._contentViewContainer.element.style.top = styleValue;
    }

    _instrumentAdded(instrumentOrEvent)
    {
        let instrument = instrumentOrEvent instanceof WebInspector.Instrument ? instrumentOrEvent : instrumentOrEvent.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._recording.timelineForInstrument(instrument);
        console.assert(!this._timelineViewMap.has(timeline), timeline);

        this._timelineViewMap.set(timeline, WebInspector.ContentView.createFromRepresentedObject(timeline, {timelineSidebarPanel: this._timelineSidebarPanel}));
        if (timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            this._renderingFrameTimeline = timeline;

        let displayName = WebInspector.TimelineTabContentView.displayNameForTimeline(timeline);
        let iconClassName = WebInspector.TimelineTabContentView.iconClassNameForTimeline(timeline);
        let pathComponent = new WebInspector.HierarchicalPathComponent(displayName, iconClassName, timeline);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
        this._pathComponentMap.set(timeline, pathComponent);

        this._timelineCountChanged();
    }

    _instrumentRemoved(event)
    {
        let instrument = event.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument);

        let timeline = this._recording.timelineForInstrument(instrument);
        console.assert(this._timelineViewMap.has(timeline), timeline);

        let timelineView = this._timelineViewMap.take(timeline);
        if (this.currentTimelineView === timelineView)
            this.showOverviewTimelineView();
        if (timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            this._renderingFrameTimeline = null;

        this._pathComponentMap.delete(timeline);

        this._timelineCountChanged();
    }

    _timelineCountChanged()
    {
        var previousPathComponent = null;
        for (var pathComponent of this._pathComponentMap.values()) {
            if (previousPathComponent) {
                previousPathComponent.nextSibling = pathComponent;
                pathComponent.previousSibling = previousPathComponent;
            }

            previousPathComponent = pathComponent;
        }

        this._updateTimelineOverviewHeight();
    }

    _recordingReset(event)
    {
        this._currentTime = NaN;

        if (!this._updating) {
            // Force the time ruler and views to reset to 0.
            this._startTimeNeedsReset = true;
            this._updateTimes(0, 0, 0);
        }

        this._lastUpdateTimestamp = NaN;
        this._startTimeNeedsReset = true;

        this._recording.removeEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
        this._waitingToResetCurrentTime = false;

        this._linearTimelineOverview.reset();
        this._renderingFrameTimelineOverview.reset();
        this._overviewTimelineView.reset();
        for (var timelineView of this._timelineViewMap.values())
            timelineView.reset();
    }

    _recordingUnloaded(event)
    {
        console.assert(!this._updating);

        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);
    }

    _timeRangeSelectionChanged(event)
    {
        if (this.currentTimelineView) {
            this.currentTimelineView.startTime = this._currentTimelineOverview.selectionStartTime;
            this.currentTimelineView.endTime = this._currentTimelineOverview.selectionStartTime + this._currentTimelineOverview.selectionDuration;

            if (this.currentTimelineView.representedObject.type === WebInspector.TimelineRecord.Type.RenderingFrame)
                this._updateFrameSelection();
        }

        // Delay until the next frame to stay in sync with the current timeline view's time-based layout changes.
        requestAnimationFrame(function() {
            var selectedTreeElement = this.currentTimelineView && this.currentTimelineView.navigationSidebarTreeOutline ? this.currentTimelineView.navigationSidebarTreeOutline.selectedTreeElement : null;
            var selectionWasHidden = selectedTreeElement && selectedTreeElement.hidden;

            this._timelineSidebarPanel.updateFilter();

            if (selectedTreeElement && selectedTreeElement.hidden !== selectionWasHidden)
                this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        }.bind(this));
    }

    _recordSelected(event)
    {
        var timelineView = this._timelineViewMap.get(event.data.timeline);
        console.assert(timelineView === this.currentTimelineView, timelineView);
        if (timelineView !== this.currentTimelineView)
            return;

        var selectedTreeElement = this.currentTimelineView.navigationSidebarTreeOutline.selectedTreeElement;
        if (!event.data.record) {
            if (selectedTreeElement)
                selectedTreeElement.deselect();
            return;
        }

        var treeElement = this.currentTimelineView.navigationSidebarTreeOutline.findTreeElement(event.data.record);
        console.assert(treeElement, "Timeline view has no tree element for record selected in timeline overview.", timelineView, event.data.record);
        if (!treeElement || treeElement.selected)
            return;

        // Don't select the record's tree element if one of it's children is already selected.
        if (selectedTreeElement && selectedTreeElement.hasAncestor(treeElement))
            return;

        treeElement.revealAndSelect(false, false, false, true);
    }

    _updateFrameSelection()
    {
        console.assert(this._renderingFrameTimeline);
        if (!this._renderingFrameTimeline)
            return;

        var startIndex = this._renderingFrameTimelineOverview.selectionStartTime;
        var endIndex = startIndex + this._renderingFrameTimelineOverview.selectionDuration - 1;
        this._timelineSidebarPanel.updateFrameSelection(startIndex, endIndex);
    }
};

WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey = "timeline-recording-content-view-selected-timeline-type";
WebInspector.TimelineRecordingContentView.OverviewTimelineViewCookieValue = "timeline-recording-content-view-overview-timeline-view";
