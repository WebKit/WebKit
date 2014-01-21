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

WebInspector.TimelineOverview = function()
{
    WebInspector.Object.call(this);

    this._element = document.createElement("div");
    this._element.className = WebInspector.TimelineOverview.StyleClassName;

    this._element.addEventListener("wheel", this._handleWheelEvent.bind(this));

    this._scrollContainer = document.createElement("div");
    this._scrollContainer.className = WebInspector.TimelineOverview.ScrollContainerStyleClassName;
    this._element.appendChild(this._scrollContainer);

    this._timelineRuler = new WebInspector.TimelineRuler;
    this._timelineRuler.allowsClippedLabels = true;
    this._timelineRuler.allowsTimeRangeSelection = true;
    this._scrollContainer.appendChild(this._timelineRuler.element);

    this._endTime = 0;

    this.startTime = 0;
    this.secondsPerPixel = 0.0025;
    this.selectionDuration = 5;
};

WebInspector.TimelineOverview.StyleClassName = "timeline-overview";
WebInspector.TimelineOverview.ScrollContainerStyleClassName = "scroll-container";
WebInspector.TimelineOverview.MinimumSecondsPerPixel = 0.001;
WebInspector.TimelineOverview.ScrollDeltaDenominator = 500;

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
        return this._timelineRuler.startTime;
    },

    set startTime(x)
    {
        if (this._timelineRuler.startTime === x)
            return;

        this._timelineRuler.zeroTime = x;
        this._timelineRuler.startTime = x;

        this._needsLayout();
    },

    get secondsPerPixel()
    {
        return this._timelineRuler.secondsPerPixel;
    },

    set secondsPerPixel(x)
    {
        if (this._timelineRuler.secondsPerPixel === x)
            return;

        this._timelineRuler.secondsPerPixel = Math.max(WebInspector.TimelineOverview.MinimumSecondsPerPixel, x);

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
        var markerElement = this._timelineRuler.elementForMarker(marker);
        if (!markerElement)
            return;
        markerElement.scrollIntoViewIfNeeded(true);
    },

    updateLayout: function()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            delete this._scheduledLayoutUpdateIdentifier;
        }

        // Calculate the required width based on the duration and seconds per pixel.
        var duration = this.endTime - this.startTime;
        var newWidth = Math.ceil(duration / this.secondsPerPixel);

        // Update all relevant elements to the new required width.
        this._updateElementWidth(this._timelineRuler.element, newWidth);

        // Update the time ruler layout now that its width has changed.
        this._timelineRuler.updateLayout();
    },

    // Private

    _updateElementWidth: function(element, newWidth)
    {
        var currentWidth = parseFloat(element.style.width).toFixed(0);
        if (currentWidth !== newWidth)
            element.style.width = newWidth + "px";
    },

    _needsLayout: function()
    {
        if (this._scheduledLayoutUpdateIdentifier)
            return;
        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(this.updateLayout.bind(this));
    },

    _handleWheelEvent: function(event)
    {
        // Require twice the vertical delta to overcome horizontal scrolling. This prevents most
        // cases of inadvertent zooming for slightly diagonal scrolls.
        if (Math.abs(event.deltaX) >= Math.abs(event.deltaY) * 0.5)
            return;

        // Remember the mouse position in time.
        var mouseOffset = event.pageX - this._scrollContainer.totalOffsetLeft;
        var scrollOffset = this._scrollContainer.scrollLeft;
        var mousePositionTime = (scrollOffset + mouseOffset) * this.secondsPerPixel;
        var deviceDirection = event.webkitDirectionInvertedFromDevice ? 1 : -1;

        this.secondsPerPixel += event.deltaY * (this._secondsPerPixel / WebInspector.TimelineOverview.ScrollDeltaDenominator) * deviceDirection;

        // Force layout so we can update the scroll position synchronously.
        this.updateLayout();

        // Center the zoom around the mouse based on the remembered mouse position time.
        this._scrollContainer.scrollLeft = Math.max(0, Math.round((mousePositionTime / this.secondsPerPixel) - mouseOffset));

        event.preventDefault();
        event.stopPropagation();
    }
};
