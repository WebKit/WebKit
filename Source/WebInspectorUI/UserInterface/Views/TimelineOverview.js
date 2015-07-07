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

WebInspector.TimelineOverview = function(identifier, timelineRecording, minimumDurationPerPixel, maximumDurationPerPixel, defaultSettingsValues)
{
    // FIXME: Convert this to a WebInspector.Object subclass, and call super().
    // WebInspector.Object.call(this);

    this._recording = timelineRecording;
    this._recording.addEventListener(WebInspector.TimelineRecording.Event.TimelineAdded, this._timelineAdded, this);
    this._recording.addEventListener(WebInspector.TimelineRecording.Event.TimelineRemoved, this._timelineRemoved, this);

    this._element = document.createElement("div");
    this._element.classList.add("timeline-overview", identifier);
    this._element.addEventListener("wheel", this._handleWheelEvent.bind(this));

    this._graphsContainerElement = document.createElement("div");
    this._graphsContainerElement.className = "graphs-container";
    this._element.appendChild(this._graphsContainerElement);

    this._timelineOverviewGraphsMap = new Map;

    this._timelineRuler = new WebInspector.TimelineRuler;
    this._timelineRuler.allowsClippedLabels = true;
    this._timelineRuler.allowsTimeRangeSelection = true;
    this._timelineRuler.addEventListener(WebInspector.TimelineRuler.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);
    this._element.appendChild(this._timelineRuler.element);

    this._currentTimeMarker = new WebInspector.TimelineMarker(0, WebInspector.TimelineMarker.Type.CurrentTime);
    this._timelineRuler.addMarker(this._currentTimeMarker);

    this._scrollContainerElement = document.createElement("div");
    this._scrollContainerElement.className = "scroll-container";
    this._scrollContainerElement.addEventListener("scroll", this._handleScrollEvent.bind(this));
    this._element.appendChild(this._scrollContainerElement);

    this._scrollWidthSizer = document.createElement("div");
    this._scrollWidthSizer.className = "scroll-width-sizer";
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

    this.selectionStartTime = this._selectionStartValueSetting.value;
    this.selectionDuration = this._selectionDurationSetting.value;

    for (var timeline of this._recording.timelines.values())
        this._timelineAdded(timeline);

    if (!WebInspector.timelineManager.isCapturingPageReload())
        this._resetSelection();
};

WebInspector.TimelineOverview.ScrollDeltaDenominator = 500;

WebInspector.TimelineOverview.Event = {
    TimeRangeSelectionChanged: "timeline-overview-time-range-selection-changed"
};

WebInspector.TimelineOverview.prototype = {
    constructor: WebInspector.TimelineOverview,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    get startTime()
    {
        return this._startTime;
    },

    set startTime(x)
    {
        if (this._startTime === x)
            return;

        this._startTime = x || 0;

        this._needsLayout();
    },

    get currentTime()
    {
        return this._currentTime;
    },

    set currentTime(x)
    {
        if (this._currentTime === x)
            return;

        this._currentTime = x || 0;
        this._revealCurrentTime = true;

        this._needsLayout();
    },

    get secondsPerPixel()
    {
        return this._durationPerPixel;
    },

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

        this._needsLayout();
    },

    get pixelAlignDuration()
    {
        return this._pixelAlignDuration;
    },

    set pixelAlignDuration(x)
    {
        if (this._pixelAlignDuration === x)
            return;

        this._mouseWheelDelta = 0;
        this._pixelAlignDuration = x;
        if (this._pixelAlignDuration)
            this.secondsPerPixel = 1 / Math.round(1 / this._durationPerPixel);
    },

    get endTime()
    {
        return this._endTime;
    },

    set endTime(x)
    {
        if (this._endTime === x)
            return;

        this._endTime = x || 0;

        this._needsLayout();
    },

    get scrollStartTime()
    {
        return this._scrollStartTime;
    },

    set scrollStartTime(x)
    {
        if (this._scrollStartTime === x)
            return;

        this._scrollStartTime = x || 0;

        this._needsLayout();
    },

    get visibleDuration()
    {
        if (isNaN(this._cachedScrollContainerWidth)) {
            this._cachedScrollContainerWidth = this._scrollContainerElement.offsetWidth;
            if (!this._cachedScrollContainerWidth)
                this._cachedScrollContainerWidth = NaN;
        }

        return this._cachedScrollContainerWidth * this._durationPerPixel;
    },

    get selectionStartTime()
    {
        return this._timelineRuler.selectionStartTime;
    },

    set selectionStartTime(x)
    {
        x = x || 0;

        var selectionDuration = this.selectionDuration;
        this._timelineRuler.selectionStartTime = x;
        this._timelineRuler.selectionEndTime = x + selectionDuration;
    },

    get selectionDuration()
    {
        return this._timelineRuler.selectionEndTime - this._timelineRuler.selectionStartTime;
    },

    set selectionDuration(x)
    {
        x = Math.max(WebInspector.TimelineRuler.MinimumSelectionTimeRange, x);

        this._timelineRuler.selectionEndTime = this._timelineRuler.selectionStartTime + x;
    },

    get visible()
    {
        return this._visible;
    },

    shown: function()
    {
        this._visible = true;

        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.shown();

        this.updateLayout();
    },

    hidden: function()
    {
        this._visible = false;

        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.hidden();
    },

    reset: function()
    {
        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.reset();

        this._mouseWheelDelta = 0;

        this._resetSelection();
    },

    addMarker: function(marker)
    {
        this._timelineRuler.addMarker(marker);
    },

    revealMarker: function(marker)
    {
        this.scrollStartTime = marker.time - (this.visibleDuration / 2);
    },

    updateLayoutForResize: function()
    {
        this._cachedScrollContainerWidth = NaN;
        this.updateLayout();
    },

    updateLayout: function()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            delete this._scheduledLayoutUpdateIdentifier;
        }

        // Calculate the required width based on the duration and seconds per pixel.
        var duration = this._endTime - this._startTime;
        var newWidth = Math.ceil(duration / this._durationPerPixel);

        // Update all relevant elements to the new required width.
        this._updateElementWidth(this._scrollWidthSizer, newWidth);

        this._currentTimeMarker.time = this._currentTime;

        if (this._revealCurrentTime) {
            this.revealMarker(this._currentTimeMarker);
            this._revealCurrentTime = false;
        }

        const visibleDuration = this.visibleDuration;

        // Clamp the scroll start time to match what the scroll bar would allow.
        var scrollStartTime = Math.min(this._scrollStartTime, this._endTime - visibleDuration);
        scrollStartTime = Math.max(this._startTime, scrollStartTime);

        this._timelineRuler.zeroTime = this._startTime;
        this._timelineRuler.startTime = scrollStartTime;
        this._timelineRuler.secondsPerPixel = this._durationPerPixel;

        if (!this._dontUpdateScrollLeft) {
            this._ignoreNextScrollEvent = true;
            this._scrollContainerElement.scrollLeft = Math.ceil((scrollStartTime - this._startTime) / this._durationPerPixel);
        }

        this._timelineRuler.updateLayout();

        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values()) {
            timelineOverviewGraph.zeroTime = this._startTime;
            timelineOverviewGraph.startTime = scrollStartTime;
            timelineOverviewGraph.currentTime = this._currentTime;
            timelineOverviewGraph.endTime = scrollStartTime + visibleDuration;
            timelineOverviewGraph.updateLayout();
        }
    },

    updateLayoutIfNeeded: function()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            this.updateLayout();
            return;
        }

        this._timelineRuler.updateLayoutIfNeeded();

        for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values())
            timelineOverviewGraph.updateLayoutIfNeeded();
    },

    // Protected

    get timelineRuler()
    {
        return this._timelineRuler;
    },

    canShowTimeline: function(timeline)
    {
        // Implemented by subclasses.
        console.error("Needs to be implemented by a subclass.");
    },

    // Private

    _updateElementWidth: function(element, newWidth)
    {
        var currentWidth = parseInt(element.style.width);
        if (currentWidth !== newWidth)
            element.style.width = newWidth + "px";
    },

    _needsLayout: function()
    {
        if (!this._visible)
            return;

        if (this._scheduledLayoutUpdateIdentifier)
            return;

        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(this.updateLayout.bind(this));
    },

    _handleScrollEvent: function(event)
    {
        if (this._ignoreNextScrollEvent) {
            delete this._ignoreNextScrollEvent;
            return;
        }

        this._dontUpdateScrollLeft = true;

        var scrollOffset = this._scrollContainerElement.scrollLeft;
        this.scrollStartTime = this._startTime + (scrollOffset * this._durationPerPixel);

        // Force layout so we can update with the scroll position synchronously.
        this.updateLayoutIfNeeded();

        delete this._dontUpdateScrollLeft;
    },

    _handleWheelEvent: function(event)
    {
        // Ignore cloned events that come our way, we already handled the original.
        if (event.__cloned)
            return;

        // Require twice the vertical delta to overcome horizontal scrolling. This prevents most
        // cases of inadvertent zooming for slightly diagonal scrolls.
        if (Math.abs(event.deltaX) >= Math.abs(event.deltaY) * 0.5) {
            // Clone the event to dispatch it on the scroll container. Mark it as cloned so we don't get into a loop.
            var newWheelEvent = new event.constructor(event.type, event);
            newWheelEvent.__cloned = true;

            this._scrollContainerElement.dispatchEvent(newWheelEvent);
            return;
        }

        // Remember the mouse position in time.
        var mouseOffset = event.pageX - this._element.totalOffsetLeft;
        var mousePositionTime = this._scrollStartTime + (mouseOffset * this._durationPerPixel);
        var deviceDirection = event.webkitDirectionInvertedFromDevice ? 1 : -1;
        var delta = event.deltaY * (this._durationPerPixel / WebInspector.TimelineOverview.ScrollDeltaDenominator) * deviceDirection;

        // Reset accumulated wheel delta when direction changes.
        if (this._pixelAlignDuration && (delta < 0 && this._mouseWheelDelta >= 0 || delta >= 0 && this._mouseWheelDelta < 0))
            this._mouseWheelDelta = 0;

        var previousDurationPerPixel = this._durationPerPixel;
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
    },

    _timelineAdded: function(timelineOrEvent)
    {
        var timeline = timelineOrEvent;
        if (!(timeline instanceof WebInspector.Timeline))
            timeline = timelineOrEvent.data.timeline;

        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(!this._timelineOverviewGraphsMap.has(timeline), timeline);
        if (!this.canShowTimeline(timeline))
            return;

        var overviewGraph = new WebInspector.TimelineOverviewGraph(timeline);
        overviewGraph.timelineOverview = this;
        this._timelineOverviewGraphsMap.set(timeline, overviewGraph);
        this._graphsContainerElement.appendChild(overviewGraph.element);
    },

    _timelineRemoved: function(event)
    {
        var timeline = event.data.timeline;
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        if (!this.canShowTimeline(timeline))
            return;

        console.assert(this._timelineOverviewGraphsMap.has(timeline), timeline);

        var overviewGraph = this._timelineOverviewGraphsMap.take(timeline);
        overviewGraph.timelineOverview = null;
        this._graphsContainerElement.removeChild(overviewGraph.element);
    },

    _timeRangeSelectionChanged: function(event)
    {
        this._selectionStartValueSetting.value = this.selectionStartTime - this._startTime;
        this._selectionDurationSetting.value = this.selectionDuration;

        this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged);
    },

    _resetSelection: function()
    {
        this.secondsPerPixel = this._defaultSettingsValues.durationPerPixel;
        this.selectionStartTime = this._defaultSettingsValues.selectionStartValue;
        this.selectionDuration = this._defaultSettingsValues.selectionDuration;
    },
};
