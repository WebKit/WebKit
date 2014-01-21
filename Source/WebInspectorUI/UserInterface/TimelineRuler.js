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

WebInspector.TimelineRuler = function()
{
    WebInspector.Object.call(this);

    this._element = document.createElement("div");
    this._element.className = WebInspector.TimelineRuler.StyleClassName;

    this._headerElement = document.createElement("div");
    this._headerElement.className = WebInspector.TimelineRuler.HeaderElementStyleClassName;
    this._element.appendChild(this._headerElement);

    this._markersElement = document.createElement("div");
    this._markersElement.className = WebInspector.TimelineRuler.MarkersElementStyleClassName;
    this._element.appendChild(this._markersElement);

    this._zeroTime = 0;
    this._startTime = 0;
    this._endTime = 0;
    this._duration = NaN;
    this._secondsPerPixel = 0;
    this._endTimePinned = false;
    this._allowsClippedLabels = false;

    this._markerElementMap = new Map;
}

WebInspector.TimelineRuler.MinimumLeftDividerSpacing = 48;
WebInspector.TimelineRuler.MinimumDividerSpacing = 64;

WebInspector.TimelineRuler.StyleClassName = "timeline-ruler";
WebInspector.TimelineRuler.HeaderElementStyleClassName = "header";
WebInspector.TimelineRuler.DividerElementStyleClassName = "divider";
WebInspector.TimelineRuler.DividerLabelElementStyleClassName = "label";

WebInspector.TimelineRuler.MarkersElementStyleClassName = "markers";
WebInspector.TimelineRuler.BaseMarkerElementStyleClassName = "marker";

WebInspector.TimelineRuler.prototype = {
    constructor: WebInspector.TimelineRuler,

    // Public

    get element()
    {
        return this._element;
    },

    get headerElement()
    {
        return this._headerElement;
    },

    get allowsClippedLabels()
    {
        return this._allowsClippedLabels
    },

    set allowsClippedLabels(x)
    {
        if (this._allowsClippedLabels === x)
            return;

        this._allowsClippedLabels = x || false;

        this._needsLayout();
    },

    get zeroTime()
    {
        return this._zeroTime;
    },

    set zeroTime(x)
    {
        if (this._zeroTime === x)
            return;

        this._zeroTime = x || 0;

        this._needsLayout();
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

        if (!isNaN(this._duration))
            this._endTime = this._startTime + this._duration;

        this._needsLayout();
    },

    get duration()
    {
        if (!isNaN(this._duration))
            return this._duration;
        return this.endTime - this.startTime;
    },

    set duration(x)
    {
        if (this._duration === x)
            return;

        this._duration = x || NaN;

        if (!isNaN(this._duration)) {
            this._endTime = this._startTime + this._duration;
            this._endTimePinned = true;
        } else
            this._endTimePinned = false;

        this._needsLayout();
    },

    get endTime()
    {
        if (!this._endTimePinned && this._scheduledLayoutUpdateIdentifier)
            this._recalculate();
        return this._endTime;
    },

    set endTime(x)
    {
        if (this._endTime === x)
            return;

        this._endTime = x || 0;
        this._endTimePinned = true;

        this._needsLayout();
    },

    get secondsPerPixel()
    {
        if (this._scheduledLayoutUpdateIdentifier)
            this._recalculate();
        return this._secondsPerPixel;
    },

    set secondsPerPixel(x)
    {
        if (this._secondsPerPixel === x)
            return;

        this._secondsPerPixel = x || 0;
        this._endTimePinned = false;
        this._currentSliceTime = 0;

        this._needsLayout();
    },

    addMarker: function(marker)
    {
        console.assert(marker instanceof WebInspector.TimelineMarker);

        if (this._markerElementMap.has(marker))
            return;

        marker.addEventListener(WebInspector.TimelineMarker.Event.TimeChanged, this._timelineMarkerTimeChanged, this);

        var markerElement = document.createElement("div");
        markerElement.classList.add(WebInspector.TimelineRuler.BaseMarkerElementStyleClassName);
        markerElement.classList.add(marker.type);

        this._markerElementMap.set(marker, markerElement);

        this._needsMarkerLayout();
    },

    elementForMarker: function(marker)
    {
        return this._markerElementMap.get(marker) || null;
    },

    updateLayout: function()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            delete this._scheduledLayoutUpdateIdentifier;
        }

        var visibleWidth = this._recalculate();
        if (visibleWidth <= 0)
            return;

        var duration = this.duration;

        var pixelsPerSecond = visibleWidth / duration;

        // Calculate a divider count based on the maximum allowed divider density.
        var dividerCount = Math.round(visibleWidth / WebInspector.TimelineRuler.MinimumDividerSpacing);

        if (this._endTimePinned || !this._currentSliceTime) {
            // Calculate the slice time based on the rough divider count and the time span.
            var sliceTime = duration / dividerCount;

            // Snap the slice time to a nearest number (e.g. 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, etc.)
            sliceTime = Math.pow(10, Math.ceil(Math.log(sliceTime) / Math.LN10));
            if (sliceTime * pixelsPerSecond >= 5 * WebInspector.TimelineRuler.MinimumDividerSpacing)
                sliceTime = sliceTime / 5;
            if (sliceTime * pixelsPerSecond >= 2 * WebInspector.TimelineRuler.MinimumDividerSpacing)
                sliceTime = sliceTime / 2;

            this._currentSliceTime = sliceTime;
        } else {
            // Reuse the last slice time since the time duration does not scale to fit when the end time isn't pinned.
            var sliceTime = this._currentSliceTime;
        }

        var firstDividerTime = (Math.ceil((this._startTime - this._zeroTime) / sliceTime) * sliceTime) + this._zeroTime;
        var lastDividerTime = this._endTime;

        // Calculate the divider count now based on the final slice time.
        dividerCount = Math.ceil((lastDividerTime - firstDividerTime) / sliceTime);

        // Make an extra divider in case the last one is partially visible.
        if (!this._endTimePinned)
            ++dividerCount;

        var markerDividers = this._markersElement.querySelectorAll("." + WebInspector.TimelineRuler.DividerElementStyleClassName);

        var dividerElement = this._headerElement.firstChild;

        for (var i = 0; i <= dividerCount; ++i) {
            if (!dividerElement) {
                dividerElement = document.createElement("div");
                dividerElement.className = WebInspector.TimelineRuler.DividerElementStyleClassName;
                this._headerElement.appendChild(dividerElement);

                var labelElement = document.createElement("div");
                labelElement.className = WebInspector.TimelineRuler.DividerLabelElementStyleClassName;
                dividerElement._labelElement = labelElement;
                dividerElement.appendChild(labelElement);
            }

            var markerDividerElement = markerDividers[i];
            if (!markerDividerElement) {
                markerDividerElement = document.createElement("div");
                markerDividerElement.className = WebInspector.TimelineRuler.DividerElementStyleClassName;
                this._markersElement.appendChild(markerDividerElement);
            }

            var dividerTime = firstDividerTime + (sliceTime * i);

            var newLeftPosition = (dividerTime - this._startTime) / duration;

            if (!this._allowsClippedLabels) {
                // Don't allow dividers under 0% where they will be completely hidden.
                if (newLeftPosition < 0)
                    continue;

                // When over 100% it is time to stop making/updating dividers.
                if (newLeftPosition > 1)
                    break;

                // Don't allow the left-most divider spacing to be so tight it clips.
                if ((newLeftPosition * visibleWidth) < WebInspector.TimelineRuler.MinimumLeftDividerSpacing)
                    continue;
            }

            this._updateLeftPositionOfElement(dividerElement, newLeftPosition, visibleWidth);
            this._updateLeftPositionOfElement(markerDividerElement, newLeftPosition, visibleWidth);

            dividerElement._labelElement.textContent = isNaN(dividerTime) ? "" : Number.secondsToString(dividerTime - this._zeroTime, true);
            dividerElement = dividerElement.nextSibling;
        }

        // Remove extra dividers.
        while (dividerElement) {
            var nextDividerElement = dividerElement.nextSibling;
            dividerElement.remove();
            dividerElement = nextDividerElement;
        }

        for (; i < markerDividers.length; ++i)
            markerDividers[i].remove();

        this._updateMarkers(visibleWidth, duration);
    },

    // Private

    _needsLayout: function()
    {
        if (this._scheduledLayoutUpdateIdentifier)
            return;

        if (this._scheduledMarkerLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledMarkerLayoutUpdateIdentifier);
            delete this._scheduledMarkerLayoutUpdateIdentifier;
        }

        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(this.updateLayout.bind(this));
    },

    _needsMarkerLayout: function()
    {
        // If layout is scheduled, abort since markers will be updated when layout happens.
        if (this._scheduledLayoutUpdateIdentifier)
            return;

        if (this._scheduledMarkerLayoutUpdateIdentifier)
            return;

        function update()
        {
            delete this._scheduledMarkerLayoutUpdateIdentifier;

            var visibleWidth = this._headerElement.clientWidth;
            if (visibleWidth <= 0)
                return;

            this._updateMarkers(visibleWidth, this.duration);
        }

        this._scheduledMarkerLayoutUpdateIdentifier = requestAnimationFrame(update.bind(this));
    },

    _recalculate: function()
    {
        var visibleWidth = this._headerElement.clientWidth;
        if (visibleWidth <= 0)
            return 0;

        if (this._endTimePinned)
            var duration = this._endTime - this._startTime;
        else
            var duration = visibleWidth * this._secondsPerPixel;

        this._secondsPerPixel = duration / visibleWidth;

        if (!this._endTimePinned)
            this._endTime = this._startTime + (visibleWidth * this._secondsPerPixel);

        return visibleWidth;
    },

    _updateLeftPositionOfElement: function(element, newLeftPosition, visibleWidth)
    {
        newLeftPosition *= this._endTimePinned ? 100 : visibleWidth;
        newLeftPosition = newLeftPosition.toFixed(2);

        var currentLeftPosition = parseFloat(element.style.left).toFixed(2);
        if (currentLeftPosition !== newLeftPosition)
            element.style.left = newLeftPosition + (this._endTimePinned ? "%" : "px");
    },

    _updateMarkers: function(visibleWidth, duration)
    {
        this._markerElementMap.forEach(function(markerElement, marker) {
            var newLeftPosition = (marker.time - this._startTime) / duration;

            this._updateLeftPositionOfElement(markerElement, newLeftPosition, visibleWidth);

            if (!markerElement.parentNode)
                this._markersElement.appendChild(markerElement);
        }, this);
    },

    _timelineMarkerTimeChanged: function()
    {
        this._needsMarkerLayout();
    }
}

WebInspector.TimelineRuler.prototype.__proto__ = WebInspector.Object.prototype;
