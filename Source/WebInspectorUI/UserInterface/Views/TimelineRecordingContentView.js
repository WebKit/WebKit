/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.TimelineRecordingContentView = function(recording)
{
    WebInspector.ContentView.call(this, recording);

    this._recording = recording;

    this.element.classList.add(WebInspector.TimelineRecordingContentView.StyleClassName);

    this._timelineOverview = new WebInspector.TimelineOverview(this._recording);
    this._timelineOverview.addEventListener(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);
    this.element.appendChild(this._timelineOverview.element);

    this._contentViewContainer = new WebInspector.ContentViewContainer();
    this._contentViewContainer.addEventListener(WebInspector.ContentViewContainer.Event.CurrentContentViewDidChange, this._currentContentViewDidChange, this);
    this.element.appendChild(this._contentViewContainer.element);

    var trashImage;
    if (WebInspector.Platform.isLegacyMacOS)
        trashImage = {src: "Images/Legacy/NavigationItemTrash.svg", width: 16, height: 16};
    else
        trashImage = {src: "Images/NavigationItemTrash.svg", width: 15, height: 15};

    this._clearTimelineNavigationItem = new WebInspector.ButtonNavigationItem("clear-timeline", WebInspector.UIString("Clear Timeline"), trashImage.src, trashImage.width, trashImage.height);
    this._clearTimelineNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._clearTimeline, this);

    this._overviewTimelineView = new WebInspector.OverviewTimelineView(recording);
    this._overviewTimelineView.secondsPerPixel = this._timelineOverview.secondsPerPixel;

    this._timelineViewMap = new Map;
    this._pathComponentMap = new Map;

    this._updating = false;
    this._currentTime = NaN;
    this._lastUpdateTimestamp = NaN;
    this._startTimeNeedsReset = true;

    this._recording.addEventListener(WebInspector.TimelineRecording.Event.TimelineAdded, this._timelineAdded, this);
    this._recording.addEventListener(WebInspector.TimelineRecording.Event.TimelineRemoved, this._timelineRemoved, this);
    this._recording.addEventListener(WebInspector.TimelineRecording.Event.Reset, this._recordingReset, this);
    this._recording.addEventListener(WebInspector.TimelineRecording.Event.Unloaded, this._recordingUnloaded, this);

    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);

    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerPaused, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerResumed, this);

    WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._contentViewSelectionPathComponentDidChange, this);
    WebInspector.ContentView.addEventListener(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange, this._contentViewSupplementalRepresentedObjectsDidChange, this);

    for (var timeline of this._recording.timelines.values())
        this._timelineAdded(timeline);

    this.showOverviewTimelineView();
};

WebInspector.TimelineRecordingContentView.StyleClassName = "timeline-recording";

WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey = "timeline-recording-content-view-selected-timeline-type";
WebInspector.TimelineRecordingContentView.OverviewTimelineViewCookieValue = "timeline-recording-content-view-overview-timeline-view";

WebInspector.TimelineRecordingContentView.prototype = {
    constructor: WebInspector.TimelineRecordingContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    showOverviewTimelineView: function()
    {
        this._contentViewContainer.showContentView(this._overviewTimelineView);
    },

    showTimelineViewForTimeline: function(timeline)
    {
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._timelineViewMap.has(timeline), timeline);
        if (!this._timelineViewMap.has(timeline))
            return;

        this._contentViewContainer.showContentView(this._timelineViewMap.get(timeline));
    },

    get allowedNavigationSidebarPanels()
    {
        return [WebInspector.timelineSidebarPanel.identifier];
    },

    get supportsSplitContentBrowser()
    {
        // The layout of the overview and split content browser don't work well.
        return false;
    },

    get selectionPathComponents()
    {
        if (!this._contentViewContainer.currentContentView)
            return [];

        var pathComponents = this._contentViewContainer.currentContentView.selectionPathComponents || [];
        var representedObject = this._contentViewContainer.currentContentView.representedObject;
        if (representedObject instanceof WebInspector.Timeline)
            pathComponents.unshift(this._pathComponentMap.get(representedObject));
        return pathComponents;
    },

    get supplementalRepresentedObjects()
    {
        if (!this._contentViewContainer.currentContentView)
            return [];
        return this._contentViewContainer.currentContentView.supplementalRepresentedObjects;
    },

    get navigationItems()
    {
        return [this._clearTimelineNavigationItem];
    },

    get handleCopyEvent()
    {
        var currentContentView = this._contentViewContainer.currentContentView;
        return currentContentView && typeof currentContentView.handleCopyEvent === "function" ? currentContentView.handleCopyEvent.bind(currentContentView) : null;
    },

    get supportsSave()
    {
        var currentContentView = this._contentViewContainer.currentContentView;
        return currentContentView && currentContentView.supportsSave;
    },

    get saveData()
    {
        var currentContentView = this._contentViewContainer.currentContentView;
        return currentContentView && currentContentView.saveData || null;
    },

    get currentTimelineView()
    {
        var contentView = this._contentViewContainer.currentContentView;
        return (contentView instanceof WebInspector.TimelineView) ? contentView : null;
    },

    shown: function()
    {
        this._timelineOverview.shown();
        this._contentViewContainer.shown();
        this._clearTimelineNavigationItem.enabled = this._recording.isWritable();

        if (!this._updating && WebInspector.timelineManager.activeRecording === this._recording && WebInspector.timelineManager.isCapturing())
            this._startUpdatingCurrentTime();
    },

    hidden: function()
    {
        this._timelineOverview.hidden();
        this._contentViewContainer.hidden();

        if (this._updating)
            this._stopUpdatingCurrentTime();
    },

    closed: function()
    {
        this._contentViewContainer.closeAllContentViews();

        WebInspector.ContentView.removeEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._contentViewSelectionPathComponentDidChange, this);
        WebInspector.ContentView.removeEventListener(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange, this._contentViewSupplementalRepresentedObjectsDidChange, this);
    },

    canGoBack: function()
    {
        return this._contentViewContainer.canGoBack();
    },

    canGoForward: function()
    {
        return this._contentViewContainer.canGoForward();
    },

    goBack: function()
    {
        this._contentViewContainer.goBack();
    },

    goForward: function()
    {
        this._contentViewContainer.goForward();
    },

    updateLayout: function()
    {
        this._timelineOverview.updateLayoutForResize();

        var currentContentView = this._contentViewContainer.currentContentView;
        if (currentContentView)
            currentContentView.updateLayout();
    },

    saveToCookie: function(cookie)
    {
        cookie.type = WebInspector.ContentViewCookieType.Timelines;

        var currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView || currentContentView === this._overviewTimelineView)
            cookie[WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey] = WebInspector.TimelineRecordingContentView.OverviewTimelineViewCookieValue;
        else if (currentContentView.representedObject instanceof WebInspector.Timeline)
            cookie[WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey] = this.currentTimelineView.representedObject.type;
    },

    restoreFromCookie: function(cookie)
    {
        var timelineType = cookie[WebInspector.TimelineRecordingContentView.SelectedTimelineTypeCookieKey];

        if (timelineType === WebInspector.TimelineRecordingContentView.OverviewTimelineViewCookieValue)
            this.showOverviewTimelineView();
        else
            this.showTimelineViewForTimeline(this.representedObject.timelines.get(timelineType));
    },

    filterDidChange: function()
    {
        if (!this.currentTimelineView)
            return;

        this.currentTimelineView.filterDidChange();
    },

    matchTreeElementAgainstCustomFilters: function(treeElement)
    {
        if (this.currentTimelineView && !this.currentTimelineView.matchTreeElementAgainstCustomFilters(treeElement))
            return false;

        var startTime = this._timelineOverview.selectionStartTime;
        var endTime = this._timelineOverview.selectionStartTime + this._timelineOverview.selectionDuration;
        var currentTime = this._currentTime || this._recording.startTime;

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
            for (var call of profileNode.calls) {
                if (checkTimeBounds(call.startTime, call.endTime))
                    return true;
            }

            return false;
        }

        if (treeElement instanceof WebInspector.TimelineRecordTreeElement) {
            var record = treeElement.record;
            return checkTimeBounds(record.startTime, record.endTime);
        }

        console.error("Unknown TreeElement, can't filter by time.");
        return true;
    },

    // Private

    _currentContentViewDidChange: function(event)
    {
        var timelineView = this.currentTimelineView;
        if (timelineView) {
            WebInspector.timelineSidebarPanel.contentTreeOutline = timelineView.navigationSidebarTreeOutline;
            WebInspector.timelineSidebarPanel.contentTreeOutlineLabel = timelineView.navigationSidebarTreeOutlineLabel;

            timelineView.startTime = this._timelineOverview.selectionStartTime;
            timelineView.endTime = this._timelineOverview.selectionStartTime + this._timelineOverview.selectionDuration;
            timelineView.currentTime = this._currentTime;
        }

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NavigationItemsDidChange);
    },

    _pathComponentSelected: function(event)
    {
        WebInspector.timelineSidebarPanel.showTimelineViewForTimeline(event.data.pathComponent.representedObject);
    },

    _contentViewSelectionPathComponentDidChange: function(event)
    {
        if (event.target !== this._contentViewContainer.currentContentView)
            return;
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    },

    _contentViewSupplementalRepresentedObjectsDidChange: function(event)
    {
        if (event.target !== this._contentViewContainer.currentContentView)
            return;
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange);
    },

    _update: function(timestamp)
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

        // Only stop updating if the current time is greater than the end time.
        if (!this._updating && currentTime >= endTime) {
            this._lastUpdateTimestamp = NaN;
            return;
        }

        this._lastUpdateTimestamp = timestamp;

        requestAnimationFrame(this._updateCallback);
    },

    _updateTimes: function(startTime, currentTime, endTime)
    {
        if (this._startTimeNeedsReset && !isNaN(startTime)) {
            var selectionOffset = this._timelineOverview.selectionStartTime - this._timelineOverview.startTime;

            this._timelineOverview.startTime = startTime;
            this._timelineOverview.selectionStartTime = startTime + selectionOffset;

            this._overviewTimelineView.zeroTime = startTime;
            for (var timelineView of this._timelineViewMap.values())
                timelineView.zeroTime = startTime;

            delete this._startTimeNeedsReset;
        }

        this._timelineOverview.endTime = Math.max(endTime, currentTime);

        this._currentTime = currentTime;
        this._timelineOverview.currentTime = currentTime;
        if (this.currentTimelineView)
            this.currentTimelineView.currentTime = currentTime;

        WebInspector.timelineSidebarPanel.updateFilter();

        // Force a layout now since we are already in an animation frame and don't need to delay it until the next.
        this._timelineOverview.updateLayoutIfNeeded();
        if (this.currentTimelineView)
            this.currentTimelineView.updateLayoutIfNeeded();
    },

    _startUpdatingCurrentTime: function()
    {
        console.assert(!this._updating);
        if (this._updating)
            return;

        if (!isNaN(this._currentTime)) {
            // We have a current time already, so we likely need to jump into the future to a better current time.
            // This happens when you stop and later restart recording.
            console.assert(!this._waitingToResetCurrentTime);
            this._waitingToResetCurrentTime = true;
            this._recording.addEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
        }

        this._updating = true;

        if (!this._updateCallback)
            this._updateCallback = this._update.bind(this);

        requestAnimationFrame(this._updateCallback);
    },

    _stopUpdatingCurrentTime: function()
    {
        console.assert(this._updating);
        this._updating = false;

        if (this._waitingToResetCurrentTime) {
            // Did not get any event while waiting for the current time, but we should stop waiting.
            this._recording.removeEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
            this._waitingToResetCurrentTime = false;
        }
    },

    _capturingStarted: function(event)
    {
        this._startUpdatingCurrentTime();
    },

    _capturingStopped: function(event)
    {
        if (this._updating)
            this._stopUpdatingCurrentTime();
    },

    _debuggerPaused: function(event)
    {
        if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Replaying)
            return;

        this._stopUpdatingCurrentTime();
    },

    _debuggerResumed: function(event)
    {
        if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Replaying)
            return;

        this._startUpdatingCurrentTime();
    },

    _recordingTimesUpdated: function(event)
    {
        if (!this._waitingToResetCurrentTime)
            return;

        // Make the current time be the start time of the last added record. This is the best way
        // currently to jump to the right period of time after recording starts.
        // FIXME: If no activity is happening we can sit for a while until a record is added.
        // We might want to have the backend send a "start" record to get current time moving.

        for (var timeline of this._recording.timelines.values()) {
            var lastRecord = timeline.records.lastValue;
            if (!lastRecord)
                continue;
            this._currentTime = Math.max(this._currentTime, lastRecord.startTime);
        }

        this._recording.removeEventListener(WebInspector.TimelineRecording.Event.TimesUpdated, this._recordingTimesUpdated, this);
        this._waitingToResetCurrentTime = false;
    },

    _clearTimeline: function(event)
    {
        this._recording.reset();
    },

    _timelineAdded: function(timelineOrEvent)
    {
        var timeline = timelineOrEvent;
        if (!(timeline instanceof WebInspector.Timeline))
            timeline = timelineOrEvent.data.timeline;

        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(!this._timelineViewMap.has(timeline), timeline);

        this._timelineViewMap.set(timeline, new WebInspector.ContentView(timeline));

        var pathComponent = new WebInspector.HierarchicalPathComponent(timeline.displayName, timeline.iconClassName, timeline);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
        this._pathComponentMap.set(timeline, pathComponent);

        this._timelineCountChanged();
    },

    _timelineRemoved: function(event)
    {
        var timeline = event.data.timeline;
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(this._timelineViewMap.has(timeline), timeline);

        var timelineView = this._timelineViewMap.take(timeline);
        if (this.currentTimelineView === timelineView)
            this.showOverviewTimelineView();

        this._pathComponentMap.delete(timeline);

        this._timelineCountChanged();
    },

    _timelineCountChanged: function()
    {
        var previousPathComponent = null;
        for (var pathComponent of this._pathComponentMap.values()) {
            if (previousPathComponent) {
                previousPathComponent.nextSibling = pathComponent;
                pathComponent.previousSibling = previousPathComponent;
            }

            previousPathComponent = pathComponent;
        }

        var timelineCount = this._recording.timelines.size;
        const timelineHeight = 36;
        const extraOffset = 22;
        this._timelineOverview.element.style.height = (timelineCount * timelineHeight + extraOffset) + "px";
        this._contentViewContainer.element.style.top = (timelineCount * timelineHeight + extraOffset) + "px";
    },

    _recordingReset: function(event)
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
    },

    _recordingUnloaded: function(event)
    {
        console.assert(!this._updating);

        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.Event.CapturingStarted, this._capturingStarted, this);
        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);
    },

    _timeRangeSelectionChanged: function(event)
    {
        if (this.currentTimelineView) {
            this.currentTimelineView.startTime = this._timelineOverview.selectionStartTime;
            this.currentTimelineView.endTime = this._timelineOverview.selectionStartTime + this._timelineOverview.selectionDuration;
        }

        // Delay until the next frame to stay in sync with the current timeline view's time-based layout changes.
        requestAnimationFrame(function() {
            var selectedTreeElement = this.currentTimelineView && this.currentTimelineView.navigationSidebarTreeOutline ? this.currentTimelineView.navigationSidebarTreeOutline.selectedTreeElement : null;
            var selectionWasHidden = selectedTreeElement && selectedTreeElement.hidden;

            WebInspector.timelineSidebarPanel.updateFilter();

            if (selectedTreeElement && selectedTreeElement.hidden !== selectionWasHidden)
                this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
        }.bind(this));
    }
};
