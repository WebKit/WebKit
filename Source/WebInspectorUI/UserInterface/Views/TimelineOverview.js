/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
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

WebInspector.TimelineOverview = class TimelineOverview extends WebInspector.View
{
    constructor(timelineRecording)
    {
        super();

        console.assert(timelineRecording instanceof WebInspector.TimelineRecording);

        this._timelinesViewModeSettings = this._createViewModeSettings(WebInspector.TimelineOverview.ViewMode.Timelines, 0.0001, 60, 0.01, 0, 15);

        if (WebInspector.FPSInstrument.supported()) {
            let minimumDurationPerPixel = 1 / WebInspector.TimelineRecordFrame.MaximumWidthPixels;
            let maximumDurationPerPixel = 1 / WebInspector.TimelineRecordFrame.MinimumWidthPixels;
            this._renderingFramesViewModeSettings = this._createViewModeSettings(WebInspector.TimelineOverview.ViewMode.RenderingFrames, minimumDurationPerPixel, maximumDurationPerPixel, minimumDurationPerPixel, 0, 100);
        }

        this._recording = timelineRecording;
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentAdded, this._instrumentAdded, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentRemoved, this._instrumentRemoved, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.MarkerAdded, this._markerAdded, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.Reset, this._recordingReset, this);

        this.element.classList.add("timeline-overview");
        this.element.addEventListener("wheel", this._handleWheelEvent.bind(this));
        this.element.addEventListener("gesturestart", this._handleGestureStart.bind(this));
        this.element.addEventListener("gesturechange", this._handleGestureChange.bind(this));
        this.element.addEventListener("gestureend", this._handleGestureEnd.bind(this));

        this._graphsContainerView = new WebInspector.View;
        this._graphsContainerView.element.classList.add("graphs-container");
        this.addSubview(this._graphsContainerView);

        this._overviewGraphsByTypeMap = new Map;

        this._timelinesTreeOutline = new WebInspector.TreeOutline;
        this._timelinesTreeOutline.element.classList.add("timelines");
        this._timelinesTreeOutline.disclosureButtons = false;
        this._timelinesTreeOutline.large = true;
        this._timelinesTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._timelinesTreeSelectionDidChange, this);
        this.element.appendChild(this._timelinesTreeOutline.element);

        this._treeElementsByTypeMap = new Map;

        this._timelineRuler = new WebInspector.TimelineRuler;
        this._timelineRuler.allowsClippedLabels = true;
        this._timelineRuler.allowsTimeRangeSelection = true;
        this._timelineRuler.element.addEventListener("mousedown", this._timelineRulerMouseDown.bind(this));
        this._timelineRuler.element.addEventListener("click", this._timelineRulerMouseClicked.bind(this));
        this._timelineRuler.addEventListener(WebInspector.TimelineRuler.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);
        this.addSubview(this._timelineRuler);

        this._currentTimeMarker = new WebInspector.TimelineMarker(0, WebInspector.TimelineMarker.Type.CurrentTime);
        this._timelineRuler.addMarker(this._currentTimeMarker);

        this._scrollContainerElement = document.createElement("div");
        this._scrollContainerElement.classList.add("scroll-container");
        this._scrollContainerElement.addEventListener("scroll", this._handleScrollEvent.bind(this));
        this.element.appendChild(this._scrollContainerElement);

        this._scrollWidthSizer = document.createElement("div");
        this._scrollWidthSizer.classList.add("scroll-width-sizer");
        this._scrollContainerElement.appendChild(this._scrollWidthSizer);

        this._startTime = 0;
        this._currentTime = 0;
        this._revealCurrentTime = false;
        this._endTime = 0;
        this._pixelAlignDuration = false;
        this._mouseWheelDelta = 0;
        this._cachedScrollContainerWidth = NaN;
        this._timelineRulerSelectionChanged = false;
        this._viewMode = WebInspector.TimelineOverview.ViewMode.Timelines;
        this._selectedTimeline = null;;

        for (let instrument of this._recording.instruments)
            this._instrumentAdded(instrument);

        if (!WebInspector.timelineManager.isCapturingPageReload())
            this._resetSelection();

        this._viewModeDidChange();
    }

    // Public

    get selectedTimeline()
    {
        return this._selectedTimeline;
    }

    set selectedTimeline(x)
    {
        if (this._selectedTimeline === x)
            return;

        this._selectedTimeline = x;
        if (this._selectedTimeline) {
            let treeElement = this._treeElementsByTypeMap.get(this._selectedTimeline.type);
            console.assert(treeElement, "Missing tree element for timeline", this._selectedTimeline);

            let omitFocus = true;
            let wasSelectedByUser = false;
            treeElement.select(omitFocus, wasSelectedByUser);
        } else if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();
    }

    get viewMode()
    {
        return this._viewMode;
    }

    set viewMode(x)
    {
        if (this._viewMode === x)
            return;

        this._viewMode = x;
        this._viewModeDidChange();
    }

    get startTime()
    {
        return this._startTime;
    }

    set startTime(x)
    {
        x = x || 0;

        if (this._startTime === x)
            return;

        if (this._viewMode !== WebInspector.TimelineOverview.ViewMode.RenderingFrames) {
            let selectionOffset = this.selectionStartTime - this._startTime;
            this.selectionStartTime = selectionOffset + x;
        }

        this._startTime = x;

        this.needsLayout();
    }

    get currentTime()
    {
        return this._currentTime;
    }

    set currentTime(x)
    {
        x = x || 0;

        if (this._currentTime === x)
            return;

        this._currentTime = x;
        this._revealCurrentTime = true;

        this.needsLayout();
    }

    get secondsPerPixel()
    {
        return this._currentSettings.durationPerPixelSetting.value;
    }

    set secondsPerPixel(x)
    {
        x = Math.min(this._currentSettings.maximumDurationPerPixel, Math.max(this._currentSettings.minimumDurationPerPixel, x));

        if (this.secondsPerPixel === x)
            return;

        if (this._pixelAlignDuration) {
            x = 1 / Math.round(1 / x);
            if (this.secondsPerPixel === x)
                return;
        }

        this._currentSettings.durationPerPixelSetting.value = x;

        this.needsLayout();
    }

    get pixelAlignDuration()
    {
        return this._pixelAlignDuration;
    }

    set pixelAlignDuration(x)
    {
        if (this._pixelAlignDuration === x)
            return;

        this._mouseWheelDelta = 0;
        this._pixelAlignDuration = x;
        if (this._pixelAlignDuration)
            this.secondsPerPixel = 1 / Math.round(1 / this.secondsPerPixel);
    }

    get endTime()
    {
        return this._endTime;
    }

    set endTime(x)
    {
        x = x || 0;

        if (this._endTime === x)
            return;

        this._endTime = x;

        this.needsLayout();
    }

    get scrollStartTime()
    {
        return this._currentSettings.scrollStartTime;
    }

    set scrollStartTime(x)
    {
        x = x || 0;

        if (this.scrollStartTime === x)
            return;

        this._currentSettings.scrollStartTime = x;

        this.needsLayout();
    }

    get scrollContainerWidth()
    {
        return this._cachedScrollContainerWidth;
    }

    get visibleDuration()
    {
        if (isNaN(this._cachedScrollContainerWidth)) {
            this._cachedScrollContainerWidth = this._scrollContainerElement.offsetWidth;
            if (!this._cachedScrollContainerWidth)
                this._cachedScrollContainerWidth = NaN;
        }

        return this._cachedScrollContainerWidth * this.secondsPerPixel;
    }

    get selectionStartTime()
    {
        return this._timelineRuler.selectionStartTime;
    }

    set selectionStartTime(x)
    {
        x = x || 0;

        if (this._timelineRuler.selectionStartTime === x)
            return;

        let selectionDuration = this.selectionDuration;
        this._timelineRuler.selectionStartTime = x;
        this._timelineRuler.selectionEndTime = x + selectionDuration;
    }

    get selectionDuration()
    {
        return this._timelineRuler.selectionEndTime - this._timelineRuler.selectionStartTime;
    }

    set selectionDuration(x)
    {
        x = Math.max(this._timelineRuler.minimumSelectionDuration, x);

        this._timelineRuler.selectionEndTime = this._timelineRuler.selectionStartTime + x;
    }

    get height()
    {
        let height = 0;
        for (let overviewGraph of this._overviewGraphsByTypeMap.values()) {
            if (overviewGraph.visible)
                height += overviewGraph.height;
        }
        return height;
    }

    get visible()
    {
        return this._visible;
    }

    shown()
    {
        this._visible = true;

        for (let [type, overviewGraph] of this._overviewGraphsByTypeMap) {
            if (this._canShowTimelineType(type))
                overviewGraph.shown();
        }

        this.updateLayout(WebInspector.View.LayoutReason.Resize);
    }

    hidden()
    {
        this._visible = false;

        for (let overviewGraph of this._overviewGraphsByTypeMap.values())
            overviewGraph.hidden();
    }

    reset()
    {
        for (let overviewGraph of this._overviewGraphsByTypeMap.values())
            overviewGraph.reset();

        this._mouseWheelDelta = 0;

        this._resetSelection();
    }

    revealMarker(marker)
    {
        this.scrollStartTime = marker.time - (this.visibleDuration / 2);
    }

    recordWasFiltered(timeline, record, filtered)
    {
        let overviewGraph = this._overviewGraphsByTypeMap.get(timeline.type);
        console.assert(overviewGraph, "Missing overview graph for timeline type " + timeline.type);
        if (!overviewGraph)
            return;

        console.assert(overviewGraph.visible, "Record filtered in hidden overview graph", record);

        overviewGraph.recordWasFiltered(record, filtered);
    }

    selectRecord(timeline, record)
    {
        let overviewGraph = this._overviewGraphsByTypeMap.get(timeline.type);
        console.assert(overviewGraph, "Missing overview graph for timeline type " + timeline.type);
        if (!overviewGraph)
            return;

        console.assert(overviewGraph.visible, "Record selected in hidden overview graph", record);

        overviewGraph.selectedRecord = record;
    }

    updateLayoutIfNeeded()
    {
        if (this.layoutPending) {
            super.updateLayoutIfNeeded();
            return;
        }

        this._timelineRuler.updateLayoutIfNeeded();

        for (let overviewGraph of this._overviewGraphsByTypeMap.values()) {
            if (overviewGraph.visible)
                overviewGraph.updateLayoutIfNeeded();
        }
    }

    // Protected

    get timelineRuler()
    {
        return this._timelineRuler;
    }

    layout(layoutReason)
    {
        if (layoutReason === WebInspector.View.LayoutReason.Resize)
            this._cachedScrollContainerWidth = NaN;

        let startTime = this._startTime;
        let endTime = this._endTime;
        let currentTime = this._currentTime;
        if (this._viewMode === WebInspector.TimelineOverview.ViewMode.RenderingFrames) {
            let renderingFramesTimeline = this._recording.timelines.get(WebInspector.TimelineRecord.Type.RenderingFrame);
            console.assert(renderingFramesTimeline, "Recoring missing rendering frames timeline");

            startTime = 0;
            endTime = renderingFramesTimeline.records.length;
            currentTime = endTime;
        }

        // Calculate the required width based on the duration and seconds per pixel.
        let duration = endTime - startTime;
        let newWidth = Math.ceil(duration / this.secondsPerPixel);

        // Update all relevant elements to the new required width.
        this._updateElementWidth(this._scrollWidthSizer, newWidth);

        this._currentTimeMarker.time = currentTime;

        if (this._revealCurrentTime) {
            this.revealMarker(this._currentTimeMarker);
            this._revealCurrentTime = false;
        }

        const visibleDuration = this.visibleDuration;

        // Clamp the scroll start time to match what the scroll bar would allow.
        let scrollStartTime = Math.min(this.scrollStartTime, endTime - visibleDuration);
        scrollStartTime = Math.max(startTime, scrollStartTime);

        this._timelineRuler.zeroTime = startTime;
        this._timelineRuler.startTime = scrollStartTime;
        this._timelineRuler.secondsPerPixel = this.secondsPerPixel;

        if (!this._dontUpdateScrollLeft) {
            this._ignoreNextScrollEvent = true;
            let scrollLeft = Math.ceil((scrollStartTime - startTime) / this.secondsPerPixel);
            if (scrollLeft)
                this._scrollContainerElement.scrollLeft = scrollLeft;
        }

        for (let overviewGraph of this._overviewGraphsByTypeMap.values()) {
            if (!overviewGraph.visible)
                continue;

            overviewGraph.zeroTime = startTime;
            overviewGraph.startTime = scrollStartTime;
            overviewGraph.currentTime = currentTime;
            overviewGraph.endTime = scrollStartTime + visibleDuration;
        }
    }

    // Private

    _updateElementWidth(element, newWidth)
    {
        var currentWidth = parseInt(element.style.width);
        if (currentWidth !== newWidth)
            element.style.width = newWidth + "px";
    }

    _handleScrollEvent(event)
    {
        if (this._ignoreNextScrollEvent) {
            this._ignoreNextScrollEvent = false;
            return;
        }

        this._dontUpdateScrollLeft = true;

        let scrollOffset = this._scrollContainerElement.scrollLeft;
        this.scrollStartTime = this._startTime + (scrollOffset * this.secondsPerPixel);

        // Force layout so we can update with the scroll position synchronously.
        this.updateLayoutIfNeeded();

        this._dontUpdateScrollLeft = false;
    }

    _handleWheelEvent(event)
    {
        // Ignore cloned events that come our way, we already handled the original.
        if (event.__cloned)
            return;

        // Ignore wheel events while handing gestures.
        if (this._handlingGesture)
            return;

        // Require twice the vertical delta to overcome horizontal scrolling. This prevents most
        // cases of inadvertent zooming for slightly diagonal scrolls.
        if (Math.abs(event.deltaX) >= Math.abs(event.deltaY) * 0.5) {
            // Clone the event to dispatch it on the scroll container. Mark it as cloned so we don't get into a loop.
            let newWheelEvent = new event.constructor(event.type, event);
            newWheelEvent.__cloned = true;

            this._scrollContainerElement.dispatchEvent(newWheelEvent);
            return;
        }

        // Remember the mouse position in time.
        let mouseOffset = event.pageX - this.element.totalOffsetLeft;
        let mousePositionTime = this._currentSettings.scrollStartTime + (mouseOffset * this.secondsPerPixel);
        let deviceDirection = event.webkitDirectionInvertedFromDevice ? 1 : -1;
        let delta = event.deltaY * (this.secondsPerPixel / WebInspector.TimelineOverview.ScrollDeltaDenominator) * deviceDirection;

        // Reset accumulated wheel delta when direction changes.
        if (this._pixelAlignDuration && (delta < 0 && this._mouseWheelDelta >= 0 || delta >= 0 && this._mouseWheelDelta < 0))
            this._mouseWheelDelta = 0;

        let previousDurationPerPixel = this.secondsPerPixel;
        this._mouseWheelDelta += delta;
        this.secondsPerPixel += this._mouseWheelDelta;

        if (this.secondsPerPixel === this._currentSettings.minimumDurationPerPixel && delta < 0 || this.secondsPerPixel === this._currentSettings.maximumDurationPerPixel && delta >= 0)
            this._mouseWheelDelta = 0;
        else
            this._mouseWheelDelta = previousDurationPerPixel + this._mouseWheelDelta - this.secondsPerPixel;

        // Center the zoom around the mouse based on the remembered mouse position time.
        this.scrollStartTime = mousePositionTime - (mouseOffset * this.secondsPerPixel);

        event.preventDefault();
        event.stopPropagation();
    }

    _handleGestureStart(event)
    {
        if (this._handlingGesture) {
            // FIXME: <https://webkit.org/b/151068> [Mac] Unexpected gesturestart events when already handling gesture
            return;
        }

        let mouseOffset = event.pageX - this.element.totalOffsetLeft;
        let mousePositionTime = this._currentSettings.scrollStartTime + (mouseOffset * this.secondsPerPixel);

        this._handlingGesture = true;
        this._gestureStartStartTime = mousePositionTime;
        this._gestureStartDurationPerPixel = this.secondsPerPixel;

        event.preventDefault();
        event.stopPropagation();
    }

    _handleGestureChange(event)
    {
        // Cap zooming out at 5x.
        let scale = Math.max(1/5, event.scale);

        let mouseOffset = event.pageX - this.element.totalOffsetLeft;
        let newSecondsPerPixel = this._gestureStartDurationPerPixel / scale;

        this.secondsPerPixel = newSecondsPerPixel;
        this.scrollStartTime = this._gestureStartStartTime - (mouseOffset * this.secondsPerPixel);

        event.preventDefault();
        event.stopPropagation();
    }

    _handleGestureEnd(event)
    {
        this._handlingGesture = false;
        this._gestureStartStartTime = NaN;
        this._gestureStartDurationPerPixel = NaN;
    }

    _instrumentAdded(instrumentOrEvent)
    {
        let instrument = instrumentOrEvent instanceof WebInspector.Instrument ? instrumentOrEvent : instrumentOrEvent.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._recording.timelineForInstrument(instrument);
        console.assert(!this._overviewGraphsByTypeMap.has(timeline.type), timeline);
        console.assert(!this._treeElementsByTypeMap.has(timeline.type), timeline);

        let overviewGraph = WebInspector.TimelineOverviewGraph.createForTimeline(timeline, this);
        overviewGraph.addEventListener(WebInspector.TimelineOverviewGraph.Event.RecordSelected, this._recordSelected, this);
        this._overviewGraphsByTypeMap.set(timeline.type, overviewGraph);

        this._graphsContainerView.addSubview(overviewGraph);

        let displayName = WebInspector.TimelineTabContentView.displayNameForTimeline(timeline);
        let iconClassName = WebInspector.TimelineTabContentView.iconClassNameForTimeline(timeline);
        let genericClassName = WebInspector.TimelineTabContentView.genericClassNameForTimeline(timeline);
        let treeElement = new WebInspector.GeneralTreeElement([iconClassName, genericClassName], displayName, null, timeline);
        let tooltip = WebInspector.UIString("Close %s timeline view").format(displayName);
        let button = new WebInspector.TreeElementStatusButton(useSVGSymbol("Images/CloseLarge.svg", "close-button", tooltip));
        button.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, () => { treeElement.deselect(); });
        treeElement.status = button.element;

        this._timelinesTreeOutline.appendChild(treeElement);
        treeElement.element.style.height = overviewGraph.height + "px";

        this._treeElementsByTypeMap.set(timeline.type, treeElement);

        if (!this._canShowTimelineType(timeline.type)) {
            overviewGraph.hidden();
            treeElement.hidden = true;
        }
    }

    _instrumentRemoved(event)
    {
        let instrument = event.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._recording.timelineForInstrument(instrument);
        console.assert(this._overviewGraphsByTypeMap.has(timeline.type), timeline);

        let overviewGraph = this._overviewGraphsByTypeMap.take(timeline.type);
        overviewGraph.removeEventListener(WebInspector.TimelineOverviewGraph.Event.RecordSelected, this._recordSelected, this);
        this._graphsContainerView.removeSubview(overviewGraph);

        let treeElement = this._treeElementsByTypeMap.take(timeline.type);
        let shouldSuppressOnDeselect = false;
        let shouldSuppressSelectSibling = true;
        this._timelinesTreeOutline.removeChild(treeElement, shouldSuppressOnDeselect, shouldSuppressSelectSibling);
    }

    _markerAdded(event)
    {
        this._timelineRuler.addMarker(event.data.marker);
    }

    _timelineRulerMouseDown(event)
    {
        this._timelineRulerSelectionChanged = false;
    }

    _timelineRulerMouseClicked(event)
    {
        if (this._timelineRulerSelectionChanged)
            return;

        for (let overviewGraph of this._overviewGraphsByTypeMap.values()) {
            let graphRect = overviewGraph.element.getBoundingClientRect();
            if (!(event.pageX >= graphRect.left && event.pageX <= graphRect.right && event.pageY >= graphRect.top && event.pageY <= graphRect.bottom))
                continue;

            // Clone the event to dispatch it on the overview graph element.
            let newClickEvent = new event.constructor(event.type, event);
            overviewGraph.element.dispatchEvent(newClickEvent);
            return;
        }
    }

    _timeRangeSelectionChanged(event)
    {
        this._timelineRulerSelectionChanged = true;

        let startTime = this._viewMode === WebInspector.TimelineOverview.ViewMode.Timelines ? this._startTime : 0;
        this._currentSettings.selectionStartValueSetting.value = this.selectionStartTime - startTime;
        this._currentSettings.selectionDurationSetting.value = this.selectionDuration;

        this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged);
    }

    _recordSelected(event)
    {
        for (let [type, overviewGraph] of this._overviewGraphsByTypeMap) {
            if (overviewGraph !== event.target)
                continue;

            let timeline = this._recording.timelines.get(type);
            console.assert(timeline, "Timeline recording missing timeline type", type);
            this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.RecordSelected, {timeline, record: event.data.record});
            return;
        }
    }

    _resetSelection()
    {
        function reset(settings)
        {
            settings.durationPerPixelSetting.reset();
            settings.selectionStartValueSetting.reset();
            settings.selectionDurationSetting.reset();
        }

        reset(this._timelinesViewModeSettings);
        if (this._renderingFramesViewModeSettings)
            reset(this._renderingFramesViewModeSettings);

        this.secondsPerPixel = this._currentSettings.durationPerPixelSetting.value;
        this.selectionStartTime = this._currentSettings.selectionStartValueSetting.value;
        this.selectionDuration = this._currentSettings.selectionDurationSetting.value;
    }

    _recordingReset(event)
    {
        this._timelineRuler.clearMarkers();
        this._timelineRuler.addMarker(this._currentTimeMarker);
    }

    _canShowTimelineType(type)
    {
        let timelineViewMode = WebInspector.TimelineOverview.ViewMode.Timelines;
        if (type === WebInspector.TimelineRecord.Type.RenderingFrame)
            timelineViewMode = WebInspector.TimelineOverview.ViewMode.RenderingFrames;

        return timelineViewMode === this._viewMode;
    }

    _viewModeDidChange()
    {
        let startTime = 0;
        let isRenderingFramesMode = this._viewMode === WebInspector.TimelineOverview.ViewMode.RenderingFrames;
        if (isRenderingFramesMode) {
            this._timelineRuler.minimumSelectionDuration = 1;
            this._timelineRuler.snapInterval = 1;
            this._timelineRuler.formatLabelCallback = (value) => value.toFixed(0);
        } else {
            this._timelineRuler.minimumSelectionDuration = 0.01;
            this._timelineRuler.snapInterval = NaN;
            this._timelineRuler.formatLabelCallback = null;

            startTime = this._startTime;
        }

        this.pixelAlignDuration = isRenderingFramesMode;
        this.selectionStartTime = this._currentSettings.selectionStartValueSetting.value + startTime;
        this.selectionDuration = this._currentSettings.selectionDurationSetting.value;

        for (let [type, overviewGraph] of this._overviewGraphsByTypeMap) {
            let treeElement = this._treeElementsByTypeMap.get(type);
            console.assert(treeElement, "Missing tree element for timeline type", type)

            treeElement.hidden = !this._canShowTimelineType(type);
            if (treeElement.hidden)
                overviewGraph.hidden();
            else
                overviewGraph.shown();
        }

        this.element.classList.toggle("frames", isRenderingFramesMode);

        this.updateLayout(WebInspector.View.LayoutReason.Resize);
    }

    _createViewModeSettings(viewMode, minimumDurationPerPixel, maximumDurationPerPixel, durationPerPixel, selectionStartValue, selectionDuration)
    {
        durationPerPixel = Math.min(maximumDurationPerPixel, Math.max(minimumDurationPerPixel, durationPerPixel));

        let durationPerPixelSetting = new WebInspector.Setting(viewMode + "-duration-per-pixel", durationPerPixel);
        let selectionStartValueSetting = new WebInspector.Setting(viewMode + "-selection-start-value", selectionStartValue);
        let selectionDurationSetting = new WebInspector.Setting(viewMode + "-selection-duration", selectionDuration);

        return {
            scrollStartTime: 0,
            minimumDurationPerPixel,
            maximumDurationPerPixel,
            durationPerPixelSetting,
            selectionStartValueSetting,
            selectionDurationSetting
        };
    }

    get _currentSettings()
    {
        return this._viewMode === WebInspector.TimelineOverview.ViewMode.Timelines ? this._timelinesViewModeSettings : this._renderingFramesViewModeSettings;
    }

    _timelinesTreeSelectionDidChange()
    {
        let treeElement = this._timelinesTreeOutline.selectedTreeElement;
        let timeline = null;
        if (treeElement) {
            timeline = treeElement.representedObject;
            console.assert(timeline instanceof WebInspector.Timeline, timeline);
            console.assert(this._recording.timelines.get(timeline.type) === timeline, timeline);
        }

        this._selectedTimeline = timeline;
        this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.TimelineSelected);
    }
};

WebInspector.TimelineOverview.ScrollDeltaDenominator = 500;

WebInspector.TimelineOverview.ViewMode = {
    Timelines: "timeline-overview-view-mode-timelines",
    RenderingFrames: "timeline-overview-view-mode-rendering-frames"
};

WebInspector.TimelineOverview.Event = {
    RecordSelected: "timeline-overview-record-selected",
    TimelineSelected: "timeline-overview-timeline-selected",
    TimeRangeSelectionChanged: "timeline-overview-time-range-selection-changed"
};
