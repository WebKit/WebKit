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

WebInspector.TimelineOverview = class TimelineOverview extends WebInspector.View
{
    constructor(identifier, timelineRecording, minimumDurationPerPixel, maximumDurationPerPixel, defaultSettingsValues)
    {
        super();

        console.assert(timelineRecording instanceof WebInspector.TimelineRecording);

        this._recording = timelineRecording;
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentAdded, this._instrumentAdded, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.InstrumentRemoved, this._instrumentRemoved, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.MarkerAdded, this._markerAdded, this);
        this._recording.addEventListener(WebInspector.TimelineRecording.Event.Reset, this._recordingReset, this);

        this.element.classList.add("timeline-overview", identifier);
        this.element.addEventListener("wheel", this._handleWheelEvent.bind(this));
        this.element.addEventListener("gesturestart", this._handleGestureStart.bind(this));
        this.element.addEventListener("gesturechange", this._handleGestureChange.bind(this));
        this.element.addEventListener("gestureend", this._handleGestureEnd.bind(this));

        this._graphsContainerView = new WebInspector.View;
        this._graphsContainerView.element.classList.add("graphs-container");
        this.addSubview(this._graphsContainerView);

        this._timelineOverviewGraphsMap = new Map;

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

        this._defaultSettingsValues = defaultSettingsValues;
        this._durationPerPixelSetting = new WebInspector.Setting(identifier + "-timeline-overview-duration-per-pixel", this._defaultSettingsValues.durationPerPixel);
        this._selectionStartValueSetting = new WebInspector.Setting(identifier + "-timeline-overview-selection-start-value", this._defaultSettingsValues.selectionStartValue);
        this._selectionDurationSetting = new WebInspector.Setting(identifier + "-timeline-overview-selection-duration", this._defaultSettingsValues.selectionDuration);

        this._startTime = 0;
        this._currentTime = 0;
        this._revealCurrentTime = false;
        this._endTime = 0;
        this._minimumDurationPerPixel = minimumDurationPerPixel;
        this._maximumDurationPerPixel = maximumDurationPerPixel;
        this._durationPerPixel = Math.min(this._maximumDurationPerPixel, Math.max(this._minimumDurationPerPixel, this._durationPerPixelSetting.value));
        this._pixelAlignDuration = false;
        this._mouseWheelDelta = 0;
        this._scrollStartTime = 0;
        this._cachedScrollContainerWidth = NaN;
        this._timelineRulerSelectionChanged = false;

        this.selectionStartTime = this._selectionStartValueSetting.value;
        this.selectionDuration = this._selectionDurationSetting.value;

        for (let instrument of this._recording.instruments)
            this._instrumentAdded(instrument);

        if (!WebInspector.timelineManager.isCapturingPageReload())
            this._resetSelection();
    }

    // Public

    get startTime()
    {
        return this._startTime;
    }

    set startTime(x)
    {
        x = x || 0;

        if (this._startTime === x)
            return;

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
        return this._durationPerPixel;
    }

    set secondsPerPixel(x)
    {
        x = Math.min(this._maximumDurationPerPixel, Math.max(this._minimumDurationPerPixel, x));

        if (this._durationPerPixel === x)
            return;

        if (this._pixelAlignDuration) {
            x = 1 / Math.round(1 / x);
            if (this._durationPerPixel === x)
                return;
        }

        this._durationPerPixel = x;
        this._durationPerPixelSetting.value = x;

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
            this.secondsPerPixel = 1 / Math.round(1 / this._durationPerPixel);
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
        return this._scrollStartTime;
    }

    set scrollStartTime(x)
    {
        x = x || 0;

        if (this._scrollStartTime === x)
            return;

        this._scrollStartTime = x;

        this.needsLayout();
    }

    get visibleDuration()
    {
        if (isNaN(this._cachedScrollContainerWidth)) {
            this._cachedScrollContainerWidth = this._scrollContainerElement.offsetWidth;
            if (!this._cachedScrollContainerWidth)
                this._cachedScrollContainerWidth = NaN;
        }

        return this._cachedScrollContainerWidth * this._durationPerPixel;
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
        for (let graph of this._timelineOverviewGraphsMap.values())
            height += graph.height;
        return height;
    }

    get visible()
    {
        return this._visible;
    }

    shown()
    {
        this._visible = true;

        this._timelineRuler.resize()

        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.shown();

        this.updateLayout();
    }

    hidden()
    {
        this._visible = false;

        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.hidden();
    }

    reset()
    {
        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.reset();

        this._mouseWheelDelta = 0;

        this._resetSelection();
    }

    revealMarker(marker)
    {
        this.scrollStartTime = marker.time - (this.visibleDuration / 2);
    }

    recordWasFiltered(timeline, record, filtered)
    {
        console.assert(this.canShowTimeline(timeline), timeline);

        let overviewGraph = this._timelineOverviewGraphsMap.get(timeline);
        console.assert(overviewGraph, "Missing overview graph for timeline type " + timeline.type);
        if (!overviewGraph)
            return;

        overviewGraph.recordWasFiltered(record, filtered);
    }

    selectRecord(timeline, record)
    {
        console.assert(this.canShowTimeline(timeline), timeline);

        var overviewGraph = this._timelineOverviewGraphsMap.get(timeline);
        console.assert(overviewGraph, "Missing overview graph for timeline type " + timeline.type);
        if (!overviewGraph)
            return;

        overviewGraph.selectedRecord = record;
    }

    updateLayoutForResize()
    {
        this._cachedScrollContainerWidth = NaN;
        this._timelineRuler.resize()
        this.updateLayout();
    }

    updateLayoutIfNeeded()
    {
        if (this.layoutPending) {
            super.updateLayoutIfNeeded();
            return;
        }

        this._timelineRuler.updateLayoutIfNeeded();

        for (let timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.updateLayoutIfNeeded();
    }

    // Protected

    get timelineRuler()
    {
        return this._timelineRuler;
    }

    canShowTimeline(timeline)
    {
        // Implemented by subclasses.
        console.error("Needs to be implemented by a subclass.");
    }

    layout()
    {
        // Calculate the required width based on the duration and seconds per pixel.
        let duration = this._endTime - this._startTime;
        let newWidth = Math.ceil(duration / this._durationPerPixel);

        // Update all relevant elements to the new required width.
        this._updateElementWidth(this._scrollWidthSizer, newWidth);

        this._currentTimeMarker.time = this._currentTime;

        if (this._revealCurrentTime) {
            this.revealMarker(this._currentTimeMarker);
            this._revealCurrentTime = false;
        }

        const visibleDuration = this.visibleDuration;

        // Clamp the scroll start time to match what the scroll bar would allow.
        let scrollStartTime = Math.min(this._scrollStartTime, this._endTime - visibleDuration);
        scrollStartTime = Math.max(this._startTime, scrollStartTime);

        this._timelineRuler.zeroTime = this._startTime;
        this._timelineRuler.startTime = scrollStartTime;
        this._timelineRuler.secondsPerPixel = this._durationPerPixel;

        if (!this._dontUpdateScrollLeft) {
            this._ignoreNextScrollEvent = true;
            let scrollLeft = Math.ceil((scrollStartTime - this._startTime) / this._durationPerPixel);
            if (scrollLeft)
                this._scrollContainerElement.scrollLeft = scrollLeft;
        }

        for (let timelineOverviewGraph of this._timelineOverviewGraphsMap.values()) {
            timelineOverviewGraph.zeroTime = this._startTime;
            timelineOverviewGraph.startTime = scrollStartTime;
            timelineOverviewGraph.currentTime = this._currentTime;
            timelineOverviewGraph.endTime = scrollStartTime + visibleDuration;
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

        var scrollOffset = this._scrollContainerElement.scrollLeft;
        this.scrollStartTime = this._startTime + (scrollOffset * this._durationPerPixel);

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
        let mousePositionTime = this._scrollStartTime + (mouseOffset * this._durationPerPixel);
        let deviceDirection = event.webkitDirectionInvertedFromDevice ? 1 : -1;
        let delta = event.deltaY * (this._durationPerPixel / WebInspector.TimelineOverview.ScrollDeltaDenominator) * deviceDirection;

        // Reset accumulated wheel delta when direction changes.
        if (this._pixelAlignDuration && (delta < 0 && this._mouseWheelDelta >= 0 || delta >= 0 && this._mouseWheelDelta < 0))
            this._mouseWheelDelta = 0;

        let previousDurationPerPixel = this._durationPerPixel;
        this._mouseWheelDelta += delta;
        this.secondsPerPixel += this._mouseWheelDelta;

        if (this._durationPerPixel === this._minimumDurationPerPixel && delta < 0 || this._durationPerPixel === this._maximumDurationPerPixel && delta >= 0)
            this._mouseWheelDelta = 0;
        else
            this._mouseWheelDelta = previousDurationPerPixel + this._mouseWheelDelta - this._durationPerPixel;

        // Center the zoom around the mouse based on the remembered mouse position time.
        this.scrollStartTime = mousePositionTime - (mouseOffset * this._durationPerPixel);

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
        let mousePositionTime = this._scrollStartTime + (mouseOffset * this._durationPerPixel);

        this._handlingGesture = true;
        this._gestureStartStartTime = mousePositionTime;
        this._gestureStartDurationPerPixel = this._durationPerPixel;

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
        this.scrollStartTime = this._gestureStartStartTime - (mouseOffset * this._durationPerPixel);

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
        console.assert(!this._timelineOverviewGraphsMap.has(timeline), timeline);
        if (!this.canShowTimeline(timeline))
            return;

        let overviewGraph = WebInspector.TimelineOverviewGraph.createForTimeline(timeline, this);
        overviewGraph.addEventListener(WebInspector.TimelineOverviewGraph.Event.RecordSelected, this._recordSelected, this);
        this._timelineOverviewGraphsMap.set(timeline, overviewGraph);

        this._graphsContainerView.addSubview(overviewGraph);
    }

    _instrumentRemoved(event)
    {
        let instrument = event.data.instrument;
        console.assert(instrument instanceof WebInspector.Instrument, instrument);

        let timeline = this._recording.timelineForInstrument(instrument);
        if (!this.canShowTimeline(timeline))
            return;

        console.assert(this._timelineOverviewGraphsMap.has(timeline), timeline);

        let overviewGraph = this._timelineOverviewGraphsMap.take(timeline);
        overviewGraph.removeEventListener(WebInspector.TimelineOverviewGraph.Event.RecordSelected, this._recordSelected, this);
        this._graphsContainerView.removeSubview(overviewGraph);
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

        for (var overviewGraph of this._timelineOverviewGraphsMap.values()) {
            var graphRect = overviewGraph.element.getBoundingClientRect();
            if (!(event.pageX >= graphRect.left && event.pageX <= graphRect.right && event.pageY >= graphRect.top && event.pageY <= graphRect.bottom))
                continue;

            // Clone the event to dispatch it on the overview graph element.
            var newClickEvent = new event.constructor(event.type, event);
            overviewGraph.element.dispatchEvent(newClickEvent);
            return;
        }
    }

    _timeRangeSelectionChanged(event)
    {
        this._timelineRulerSelectionChanged = true;
        this._selectionStartValueSetting.value = this.selectionStartTime - this._startTime;
        this._selectionDurationSetting.value = this.selectionDuration;

        this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged);
    }

    _recordSelected(event)
    {
        for (var [timeline, overviewGraph] of this._timelineOverviewGraphsMap) {
            if (overviewGraph !== event.target)
                continue;

            this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.RecordSelected, {timeline, record: event.data.record});
            return;
        }
    }

    _resetSelection()
    {
        this.secondsPerPixel = this._defaultSettingsValues.durationPerPixel;
        this.selectionStartTime = this._defaultSettingsValues.selectionStartValue;
        this.selectionDuration = this._defaultSettingsValues.selectionDuration;
    }

    _recordingReset(event)
    {
        this._timelineRuler.clearMarkers();
        this._timelineRuler.addMarker(this._currentTimeMarker);
    }
};

WebInspector.TimelineOverview.ScrollDeltaDenominator = 500;

WebInspector.TimelineOverview.Event = {
    RecordSelected: "timeline-overview-record-selected",
    TimeRangeSelectionChanged: "timeline-overview-time-range-selection-changed"
};
