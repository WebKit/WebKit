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
    constructor(recording)
    {
        super(recording);

        this._recording = recording;

        this.element.classList.add("timeline-recording");

        this._timelineOverview = new WebInspector.TimelineOverview(this._recording, this);
        this._timelineOverview.addEventListener(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);
        this._timelineOverview.addEventListener(WebInspector.TimelineOverview.Event.RecordSelected, this._recordSelected, this);
        this._timelineOverview.addEventListener(WebInspector.TimelineOverview.Event.TimelineSelected, this._timelineSelected, this);
        this._timelineOverview.addEventListener(WebInspector.TimelineOverview.Event.EditingInstrumentsDidChange, this._editingInstrumentsDidChange, this);
        this.addSubview(this._timelineOverview);

        const disableBackForward = true;
        this._timelineContentBrowser = new WebInspector.ContentBrowser(null, this, disableBackForward);
        this._timelineContentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange, this._currentContentViewDidChange, this);

        this._entireRecordingPathComponent = this._createTimelineRangePathComponent(WebInspector.UIString("Entire Recording"));
        this._timelineSelectionPathComponent = this._createTimelineRangePathComponent();
        this._timelineSelectionPathComponent.previousSibling = this._entireRecordingPathComponent;
        this._selectedTimeRangePathComponent = this._entireRecordingPathComponent;

        this._filterBarNavigationItem = new WebInspector.FilterBarNavigationItem;
        this._filterBarNavigationItem.filterBar.placeholder = WebInspector.UIString("Filter Records");
        this._timelineContentBrowser.navigationBar.addNavigationItem(this._filterBarNavigationItem);
        this.addSubview(this._timelineContentBrowser);

        this._clearTimelineNavigationItem = new WebInspector.ButtonNavigationItem("clear-timeline", WebInspector.UIString("Clear Timeline"), "Images/NavigationItemTrash.svg", 15, 15);
        this._clearTimelineNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._clearTimeline, this);

        this._overviewTimelineView = new WebInspector.OverviewTimelineView(recording);
        this._overviewTimelineView.secondsPerPixel = this._timelineOverview.secondsPerPixel;

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
        this._timelineContentBrowser.showContentView(this._overviewTimelineView);
    }

    showTimelineViewForTimeline(timeline)
    {
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._timelineViewMap.has(timeline), timeline);
        if (!this._timelineViewMap.has(timeline))
            return;

        this._timelineContentBrowser.showContentView(this._timelineViewMap.get(timeline));
    }

    get supportsSplitContentBrowser()
    {
        // The layout of the overview and split content browser don't work well.
        return false;
    }

    get selectionPathComponents()
    {
        if (!this._timelineContentBrowser.currentContentView)
            return [];

        let pathComponents = [];
        let representedObject = this._timelineContentBrowser.currentContentView.representedObject;
        if (representedObject instanceof WebInspector.Timeline)
            pathComponents.push(this._pathComponentMap.get(representedObject));

        pathComponents.push(this._selectedTimeRangePathComponent);
        return pathComponents;
    }

    get supplementalRepresentedObjects()
    {
        if (!this._timelineContentBrowser.currentContentView)
            return [];
        return this._timelineContentBrowser.currentContentView.supplementalRepresentedObjects;
    }

    get navigationItems()
    {
        return [this._clearTimelineNavigationItem];
    }

    get handleCopyEvent()
    {
        let currentContentView = this._timelineContentBrowser.currentContentView;
        return currentContentView && typeof currentContentView.handleCopyEvent === "function" ? currentContentView.handleCopyEvent.bind(currentContentView) : null;
    }

    get supportsSave()
    {
        let currentContentView = this._timelineContentBrowser.currentContentView;
        return currentContentView && currentContentView.supportsSave;
    }

    get saveData()
    {
        let currentContentView = this._timelineContentBrowser.currentContentView;
        return currentContentView && currentContentView.saveData || null;
    }

    get currentTimelineView()
    {
        return this._timelineContentBrowser.currentContentView;
    }

    shown()
    {
        this._timelineOverview.shown();
        this._timelineContentBrowser.shown();
        this._clearTimelineNavigationItem.enabled = !this._recording.readonly && !isNaN(this._recording.startTime);

        this._currentContentViewDidChange();

        if (!this._updating && WebInspector.timelineManager.activeRecording === this._recording && WebInspector.timelineManager.isCapturing())
            this._startUpdatingCurrentTime(this._currentTime);
    }

    hidden()
    {
        this._timelineOverview.hidden();
        this._timelineContentBrowser.hidden();

        if (this._updating)
            this._stopUpdatingCurrentTime();
    }

    closed()
    {
        this._timelineContentBrowser.contentViewContainer.closeAllContentViews();

        this._recording.removeEventListener(null, null, this);

        WebInspector.timelineManager.removeEventListener(null, null, this);
        WebInspector.debuggerManager.removeEventListener(null, null, this);
        WebInspector.ContentView.removeEventListener(null, null, this);
    }

    canGoBack()
    {
        return this._timelineContentBrowser.canGoBack();
    }

    canGoForward()
    {
        return this._timelineContentBrowser.canGoForward();
    }

    goBack()
    {
        this._timelineContentBrowser.goBack();
    }

    goForward()
    {
        this._timelineContentBrowser.goForward();
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

        this._timelineOverview.recordWasFiltered(this.currentTimelineView.representedObject, record, filtered);
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        if (this.currentTimelineView && !this.currentTimelineView.matchTreeElementAgainstCustomFilters(treeElement))
            return false;

        let startTime = this._timelineOverview.selectionStartTime;
        let endTime = startTime + this._timelineOverview.selectionDuration;
        let currentTime = this._currentTime || this._recording.startTime;

        if (this._timelineOverview.viewMode === WebInspector.TimelineOverview.ViewMode.RenderingFrames) {
            console.assert(this._renderingFrameTimeline);

            if (this._renderingFrameTimeline && this._renderingFrameTimeline.records.length) {
                let records = this._renderingFrameTimeline.records;
                let startIndex = this._timelineOverview.timelineRuler.snapInterval ? startTime : Math.floor(startTime);
                if (startIndex >= records.length)
                    return false;

                let endIndex = this._timelineOverview.timelineRuler.snapInterval ? endTime - 1: Math.floor(endTime);
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

    // ContentBrowser delegate

    contentBrowserTreeElementForRepresentedObject(contentBrowser, representedObject)
    {
        if (!(representedObject instanceof WebInspector.Timeline) && !(representedObject instanceof WebInspector.TimelineRecording))
            return null;

        let iconClassName;
        let title;
        if (representedObject instanceof WebInspector.Timeline) {
            iconClassName = WebInspector.TimelineTabContentView.iconClassNameForTimelineType(representedObject.type);
            title = WebInspector.UIString("Details");
        } else {
            iconClassName = WebInspector.TimelineTabContentView.StopwatchIconStyleClass;
            title = WebInspector.UIString("Overview");
        }

        const hasChildren = false;
        return new WebInspector.GeneralTreeElement(iconClassName, title, representedObject, hasChildren);
    }

    // TimelineOverview delegate

    timelineOverviewUserSelectedRecord(timelineOverview, timelineRecord)
    {
        let timelineViewForRecord = null;
        for (let timelineView of this._timelineViewMap.values()) {
            if (timelineView.representedObject.type === timelineRecord.type) {
                timelineViewForRecord = timelineView;
                break;
            }
        }

        if (!timelineViewForRecord)
            return;

        this._timelineContentBrowser.showContentView(timelineViewForRecord);
        timelineViewForRecord.userSelectedRecordFromOverview(timelineRecord);
    }

    // Private

    _currentContentViewDidChange(event)
    {
        let newViewMode;
        let timelineView = this.currentTimelineView;
        if (timelineView && timelineView.representedObject.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            newViewMode = WebInspector.TimelineOverview.ViewMode.RenderingFrames;
        else
            newViewMode = WebInspector.TimelineOverview.ViewMode.Timelines;

        this._timelineOverview.viewMode = newViewMode;
        this._updateTimelineOverviewHeight();

        if (timelineView) {
            this._updateTimelineViewSelection(timelineView);
            timelineView.currentTime = this._currentTime;

            let timeline = null;
            if (timelineView.representedObject instanceof WebInspector.Timeline)
                timeline = timelineView.representedObject;

            this._timelineOverview.selectedTimeline = timeline;
        }

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    }

    _timelinePathComponentSelected(event)
    {
        let selectedTimeline = event.data.pathComponent.representedObject;
        this.showTimelineViewForTimeline(selectedTimeline);
    }

    _timeRangePathComponentSelected(event)
    {
        let selectedPathComponent = event.data.pathComponent;
        if (selectedPathComponent === this._selectedTimeRangePathComponent)
            return;

        let timelineRuler = this._timelineOverview.timelineRuler
        if (selectedPathComponent === this._entireRecordingPathComponent)
            timelineRuler.selectEntireRange();
        else {
            let timelineRange = selectedPathComponent.representedObject;
            timelineRuler.selectionStartTime = timelineRuler.zeroTime + timelineRange.startValue;
            timelineRuler.selectionEndTime = timelineRuler.zeroTime + timelineRange.endValue;
        }
    }

    _contentViewSelectionPathComponentDidChange(event)
    {
        if (event.target !== this._timelineContentBrowser.currentContentView)
            return;

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        if (this.currentTimelineView === this._overviewTimelineView)
            return;

        var recordPathComponent = this.selectionPathComponents.find(function(element) { return element.representedObject instanceof WebInspector.TimelineRecord; });
        var record = recordPathComponent ? recordPathComponent.representedObject : null;
        this._timelineOverview.selectRecord(event.target.representedObject, record);
    }

    _contentViewSupplementalRepresentedObjectsDidChange(event)
    {
        if (event.target !== this._timelineContentBrowser.currentContentView)
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
            this._timelineOverview.startTime = startTime;
            this._overviewTimelineView.zeroTime = startTime;
            for (var timelineView of this._timelineViewMap.values()) {
                if (timelineView.representedObject.type === WebInspector.TimelineRecord.Type.RenderingFrame)
                    continue;

                timelineView.zeroTime = startTime;
            }

            this._startTimeNeedsReset = false;
        }

        this._timelineOverview.endTime = Math.max(endTime, currentTime);

        this._currentTime = currentTime;
        this._timelineOverview.currentTime = currentTime;
        if (this.currentTimelineView)
            this.currentTimelineView.currentTime = currentTime;

        if (this._timelineOverview.timelineRuler.entireRangeSelected)
            this._updateTimelineViewSelection(this._overviewTimelineView);

        // Filter records on new recording times.
        // FIXME: <https://webkit.org/b/154924> Web Inspector: hook up grid row filtering in the new Timelines UI

        // Force a layout now since we are already in an animation frame and don't need to delay it until the next.
        this._timelineOverview.updateLayoutIfNeeded();
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
        this._clearTimelineNavigationItem.enabled = !this._recording.readonly;
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
        if (this._timelineOverview.editingInstruments)
            this._timelineOverview.element.style.height = "";
        else {
            const rulerHeight = 23;

            let styleValue = (rulerHeight + this._timelineOverview.height) + "px";
            this._timelineOverview.element.style.height = styleValue;
            this._timelineContentBrowser.element.style.top = styleValue;
        }
    }

    _instrumentAdded(instrumentOrEvent)
    {
        let instrument = instrumentOrEvent instanceof WebInspector.Instrument ? instrumentOrEvent : instrumentOrEvent.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._recording.timelineForInstrument(instrument);
        console.assert(!this._timelineViewMap.has(timeline), timeline);

        this._timelineViewMap.set(timeline, WebInspector.ContentView.createFromRepresentedObject(timeline, {recording: this._recording}));
        if (timeline.type === WebInspector.TimelineRecord.Type.RenderingFrame)
            this._renderingFrameTimeline = timeline;

        let displayName = WebInspector.TimelineTabContentView.displayNameForTimelineType(timeline.type);
        let iconClassName = WebInspector.TimelineTabContentView.iconClassNameForTimelineType(timeline.type);
        let pathComponent = new WebInspector.HierarchicalPathComponent(displayName, iconClassName, timeline);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._timelinePathComponentSelected, this);
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

        this._timelineOverview.reset();
        this._overviewTimelineView.reset();
        for (var timelineView of this._timelineViewMap.values())
            timelineView.reset();
        this._clearTimelineNavigationItem.enabled = false;
    }

    _recordingUnloaded(event)
    {
        console.assert(!this._updating);

        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);
    }

    _timeRangeSelectionChanged(event)
    {
        console.assert(this.currentTimelineView);
        if (!this.currentTimelineView)
            return;

        this._updateTimelineViewSelection(this.currentTimelineView);

        let selectedPathComponent;
        if (this._timelineOverview.timelineRuler.entireRangeSelected)
            selectedPathComponent = this._entireRecordingPathComponent;
        else {
            let timelineRange = this._timelineSelectionPathComponent.representedObject;
            timelineRange.startValue = this.currentTimelineView.startTime - this.currentTimelineView.zeroTime;
            timelineRange.endValue = this.currentTimelineView.endTime - this.currentTimelineView.zeroTime;

            this._updateTimeRangePathComponents();
            selectedPathComponent = this._timelineSelectionPathComponent;
        }

        if (this._selectedTimeRangePathComponent !== selectedPathComponent) {
            this._selectedTimeRangePathComponent = selectedPathComponent;
            this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        }

        // Delay until the next frame to stay in sync with the current timeline view's time-based layout changes.
        requestAnimationFrame(function() {
            var selectedTreeElement = this.currentTimelineView && this.currentTimelineView.navigationSidebarTreeOutline ? this.currentTimelineView.navigationSidebarTreeOutline.selectedTreeElement : null;
            var selectionWasHidden = selectedTreeElement && selectedTreeElement.hidden;

            // Filter records on new timeline selection.
            // FIXME: <https://webkit.org/b/154924> Web Inspector: hook up grid row filtering in the new Timelines UI

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

    _timelineSelected()
    {
        let timeline = this._timelineOverview.selectedTimeline;
        if (timeline)
            this.showTimelineViewForTimeline(timeline);
        else
            this.showOverviewTimelineView();
    }

    _updateTimeRangePathComponents()
    {
        let timelineRange = this._timelineSelectionPathComponent.representedObject;
        let startValue = timelineRange.startValue;
        let endValue = timelineRange.endValue;
        if (isNaN(startValue) || isNaN(endValue)) {
            this._entireRecordingPathComponent.nextSibling = null;
            return;
        }

        this._entireRecordingPathComponent.nextSibling = this._timelineSelectionPathComponent;

        let displayName;
        if (this._timelineOverview.viewMode === WebInspector.TimelineOverview.ViewMode.Timelines) {
            let selectionStart = Number.secondsToString(startValue, true);
            let selectionEnd = Number.secondsToString(endValue, true);
            displayName = WebInspector.UIString("%s \u2013 %s").format(selectionStart, selectionEnd);
        } else {
            startValue += 1; // Convert index to frame number.
            if (startValue === endValue)
                displayName = WebInspector.UIString("Frame %d").format(startValue);
            else
                displayName = WebInspector.UIString("Frames %d \u2013 %d").format(startValue, endValue);
        }

        this._timelineSelectionPathComponent.displayName = displayName;
        this._timelineSelectionPathComponent.title = displayName;
    }

    _createTimelineRangePathComponent(title)
    {
        let range = new WebInspector.TimelineRange(NaN, NaN);
        let pathComponent = new WebInspector.HierarchicalPathComponent(title || enDash, "time-icon", range);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._timeRangePathComponentSelected, this);

        return pathComponent;
    }

    _updateTimelineViewSelection(timelineView)
    {
        let timelineRuler = this._timelineOverview.timelineRuler;
        let entireRangeSelected = timelineRuler.entireRangeSelected;
        let endTime = this._timelineOverview.selectionStartTime + this._timelineOverview.selectionDuration;

        if (entireRangeSelected) {
            // Clamp selection to the end of the recording (with padding), so that OverviewTimelineView
            // displays an autosized graph without a lot of horizontal white space or tiny graph bars.
            if (isNaN(this._recording.endTime))
                endTime = this._currentTime;
            else
                endTime = Math.min(endTime, this._recording.endTime + timelineRuler.minimumSelectionDuration);
        }

        timelineView.startTime = this._timelineOverview.selectionStartTime;
        timelineView.endTime = endTime;
    }

    _editingInstrumentsDidChange(event)
    {
        let editingInstruments = this._timelineOverview.editingInstruments;
        this.element.classList.toggle(WebInspector.TimelineOverview.EditInstrumentsStyleClassName, editingInstruments);

        this._updateTimelineOverviewHeight();
    }
};
