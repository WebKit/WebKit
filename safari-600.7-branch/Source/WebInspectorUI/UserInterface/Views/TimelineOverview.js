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

WebInspector.TimelineOverview = function(timelineOverviewGraphsMap)
{
    WebInspector.Object.call(this);

    this._element = document.createElement("div");
    this._element.className = WebInspector.TimelineOverview.StyleClassName;
    this._element.addEventListener("wheel", this._handleWheelEvent.bind(this));

    this._graphsContainer = document.createElement("div");
    this._graphsContainer.className = WebInspector.TimelineOverview.GraphsContainerStyleClassName;
    this._element.appendChild(this._graphsContainer);

    this._timelineOverviewGraphsMap = timelineOverviewGraphsMap;

    for (var timelineOverviewGraph of this._timelineOverviewGraphsMap.values()) {
        timelineOverviewGraph.timelineOverview = this;
        this._graphsContainer.appendChild(timelineOverviewGraph.element);
    }

    this._timelineRuler = new WebInspector.TimelineRuler;
    this._timelineRuler.allowsClippedLabels = true;
    this._timelineRuler.allowsTimeRangeSelection = true;
    this._timelineRuler.addEventListener(WebInspector.TimelineRuler.Event.TimeRangeSelectionChanged, this._timeRangeSelectionChanged, this);
    this._element.appendChild(this._timelineRuler.element);

    this._currentTimeMarker = new WebInspector.TimelineMarker(0, WebInspector.TimelineMarker.Type.CurrentTime);
    this._timelineRuler.addMarker(this._currentTimeMarker);

    this._scrollContainer = document.createElement("div");
    this._scrollContainer.className = WebInspector.TimelineOverview.ScrollContainerStyleClassName;
    this._scrollContainer.addEventListener("scroll", this._handleScrollEvent.bind(this));
    this._element.appendChild(this._scrollContainer);

    this._scrollWidthSizer = document.createElement("div");
    this._scrollWidthSizer.className = WebInspector.TimelineOverview.ScrollWidthSizerStyleClassName;
    this._scrollContainer.appendChild(this._scrollWidthSizer);

    this._secondsPerPixelSetting = new WebInspector.Setting("timeline-overview-seconds-per-pixel", 0.01);
    this._selectionStartTimeSetting = new WebInspector.Setting("timeline-overview-selection-start-time", 0);
    this._selectionDurationSetting = new WebInspector.Setting("timeline-overview-selection-duration", 5);

    this._startTime = 0;
    this._currentTime = 0;
    this._endTime = 0;
    this._secondsPerPixel = this._secondsPerPixelSetting.value;
    this._scrollStartTime = 0;
    this._cachedScrollContainerWidth = NaN;

    this.selectionStartTime = this._selectionStartTimeSetting.value;
    this.selectionDuration = this._selectionDurationSetting.value;
};

WebInspector.TimelineOverview.StyleClassName = "timeline-overview";
WebInspector.TimelineOverview.GraphsContainerStyleClassName = "graphs-container";
WebInspector.TimelineOverview.ScrollContainerStyleClassName = "scroll-container";
WebInspector.TimelineOverview.ScrollWidthSizerStyleClassName = "scroll-width-sizer";
WebInspector.TimelineOverview.MinimumSecondsPerPixel = 0.001;
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
        return this._secondsPerPixel;
    },

    set secondsPerPixel(x)
    {
        x = Math.max(WebInspector.TimelineOverview.MinimumSecondsPerPixel, x);

        if (this._secondsPerPixel === x)
            return;

        this._secondsPerPixel = x;
        this._secondsPerPixelSetting.value = x;

        this._needsLayout();
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
            this._cachedScrollContainerWidth = this._scrollContainer.offsetWidth;
            console.assert(this._cachedScrollContainerWidth > 0);
        }

        return this._cachedScrollContainerWidth * this._secondsPerPixel;
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
        var newWidth = Math.ceil(duration / this._secondsPerPixel);

        // Update all relevant elements to the new required width.
        this._updateElementWidth(this._scrollWidthSizer, newWidth);

        this._currentTimeMarker.time = this._currentTime;

        if (this._revealCurrentTime) {
            this.revealMarker(this._currentTimeMarker);
            delete this._revealCurrentTime;
        }

        const visibleDuration = this.visibleDuration;

        // Clamp the scroll start time to match what the scroll bar would allow.
        var scrollStartTime = Math.min(this._scrollStartTime, this._endTime - visibleDuration);
        scrollStartTime = Math.max(this._startTime, scrollStartTime);

        this._timelineRuler.zeroTime = this._startTime;
        this._timelineRuler.startTime = scrollStartTime;
        this._timelineRuler.secondsPerPixel = this._secondsPerPixel;

        if (!this._dontUpdateScrollLeft) {
            this._ignoreNextScrollEvent = true;
            this._scrollContainer.scrollLeft = Math.ceil((scrollStartTime - this._startTime) / this._secondsPerPixel);
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

    // Private

    _updateElementWidth: function(element, newWidth)
    {
        var currentWidth = parseInt(element.style.width);
        if (currentWidth !== newWidth)
            element.style.width = newWidth + "px";
    },

    _needsLayout: function()
    {
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

        var scrollOffset = this._scrollContainer.scrollLeft;
        this.scrollStartTime = this._startTime + (scrollOffset * this._secondsPerPixel);

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

            this._scrollContainer.dispatchEvent(newWheelEvent);
            return;
        }

        // Remember the mouse position in time.
        var mouseOffset = event.pageX - this._element.totalOffsetLeft;
        var mousePositionTime = this._scrollStartTime + (mouseOffset * this._secondsPerPixel);
        var deviceDirection = event.webkitDirectionInvertedFromDevice ? 1 : -1;

        this.secondsPerPixel += event.deltaY * (this._secondsPerPixel / WebInspector.TimelineOverview.ScrollDeltaDenominator) * deviceDirection;

        // Center the zoom around the mouse based on the remembered mouse position time.
        this.scrollStartTime = mousePositionTime - (mouseOffset * this._secondsPerPixel);

        event.preventDefault();
        event.stopPropagation();
    },

    _timeRangeSelectionChanged: function(event)
    {
        this._selectionStartTimeSetting.value = this.selectionStartTime - this._startTime;
        this._selectionDurationSetting.value = this.selectionDuration;

        this.dispatchEventToListeners(WebInspector.TimelineOverview.Event.TimeRangeSelectionChanged);
    }
};
