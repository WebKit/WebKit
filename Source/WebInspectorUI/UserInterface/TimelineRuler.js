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
    this._selectionStartTime = 0;
    this._selectionEndTime = Infinity;
    this._endTimePinned = false;
    this._allowsClippedLabels = false;
    this._allowsTimeRangeSelection = false;

    this._markerElementMap = new Map;
}

WebInspector.TimelineRuler.MinimumLeftDividerSpacing = 48;
WebInspector.TimelineRuler.MinimumDividerSpacing = 64;

WebInspector.TimelineRuler.StyleClassName = "timeline-ruler";
WebInspector.TimelineRuler.AllowsTimeRangeSelectionStyleClassName = "allows-time-range-selection";
WebInspector.TimelineRuler.HeaderElementStyleClassName = "header";
WebInspector.TimelineRuler.DividerElementStyleClassName = "divider";
WebInspector.TimelineRuler.DividerLabelElementStyleClassName = "label";

WebInspector.TimelineRuler.MarkersElementStyleClassName = "markers";
WebInspector.TimelineRuler.BaseMarkerElementStyleClassName = "marker";
WebInspector.TimelineRuler.ShadedAreaElementStyleClassName = "shaded-area";
WebInspector.TimelineRuler.SelectionDragElementStyleClassName = "selection-drag";
WebInspector.TimelineRuler.SelectionHandleElementStyleClassName = "selection-handle";
WebInspector.TimelineRuler.LeftSelectionElementStyleClassName = "left";
WebInspector.TimelineRuler.RightSelectionElementStyleClassName = "right";
WebInspector.TimelineRuler.MinimumSelectionTimeRange = 0.1;

WebInspector.TimelineRuler.Event = {
    TimeRangeSelectionChanged: "time-ruler-time-range-selection-changed"
};

WebInspector.TimelineRuler.prototype = {
    constructor: WebInspector.TimelineRuler,

    // Public

    get element()
    {
        return this._element;
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

    get allowsTimeRangeSelection()
    {
        return this._allowsTimeRangeSelection;
    },

    set allowsTimeRangeSelection(x)
    {
        if (this._allowsTimeRangeSelection === x)
            return;

        this._allowsTimeRangeSelection = x || false;

        if (x) {
            this._mouseDownEventListener = this._handleMouseDown.bind(this);
            this._element.addEventListener("mousedown", this._mouseDownEventListener);

            this._leftShadedAreaElement = document.createElement("div");
            this._leftShadedAreaElement.classList.add(WebInspector.TimelineRuler.ShadedAreaElementStyleClassName);
            this._leftShadedAreaElement.classList.add(WebInspector.TimelineRuler.LeftSelectionElementStyleClassName);

            this._rightShadedAreaElement = document.createElement("div");
            this._rightShadedAreaElement.classList.add(WebInspector.TimelineRuler.ShadedAreaElementStyleClassName);
            this._rightShadedAreaElement.classList.add(WebInspector.TimelineRuler.RightSelectionElementStyleClassName);

            this._leftSelectionHandleElement = document.createElement("div");
            this._leftSelectionHandleElement.classList.add(WebInspector.TimelineRuler.SelectionHandleElementStyleClassName);
            this._leftSelectionHandleElement.classList.add(WebInspector.TimelineRuler.LeftSelectionElementStyleClassName);
            this._leftSelectionHandleElement.addEventListener("mousedown", this._handleSelectionHandleMouseDown.bind(this));

            this._rightSelectionHandleElement = document.createElement("div");
            this._rightSelectionHandleElement.classList.add(WebInspector.TimelineRuler.SelectionHandleElementStyleClassName);
            this._rightSelectionHandleElement.classList.add(WebInspector.TimelineRuler.RightSelectionElementStyleClassName);
            this._rightSelectionHandleElement.addEventListener("mousedown", this._handleSelectionHandleMouseDown.bind(this));

            this._selectionDragElement = document.createElement("div");
            this._selectionDragElement.classList.add(WebInspector.TimelineRuler.SelectionDragElementStyleClassName);

            this._needsSelectionLayout();
        } else {
            this._element.removeEventListener("mousedown", this._mouseDownEventListener);
            delete this._mouseDownEventListener;

            this._leftShadedAreaElement.remove();
            this._rightShadedAreaElement.remove();
            this._leftSelectionHandleElement.remove();
            this._rightSelectionHandleElement.remove();
            this._selectionDragElement.remove();

            delete this._leftShadedAreaElement;
            delete this._rightShadedAreaElement;
            delete this._leftSelectionHandleElement;
            delete this._rightSelectionHandleElement;
            delete this._selectionDragElement;
        }
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

    get selectionStartTime()
    {
        return this._selectionStartTime;
    },

    set selectionStartTime(x)
    {
        if (this._selectionStartTime === x)
            return;

        this._selectionStartTime = x || 0;
        this._timeRangeSelectionChanged = true;

        this._needsSelectionLayout();
    },

    get selectionEndTime()
    {
        return this._selectionEndTime;
    },

    set selectionEndTime(x)
    {
        if (this._selectionEndTime === x)
            return;

        this._selectionEndTime = x || 0;
        this._timeRangeSelectionChanged = true;

        this._needsSelectionLayout();
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

            this._updatePositionOfElement(dividerElement, newLeftPosition, visibleWidth);
            this._updatePositionOfElement(markerDividerElement, newLeftPosition, visibleWidth);

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
        this._updateSelection(visibleWidth, duration);
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

        if (this._scheduledSelectionLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledSelectionLayoutUpdateIdentifier);
            delete this._scheduledSelectionLayoutUpdateIdentifier;
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

            var visibleWidth = this._element.clientWidth;
            if (visibleWidth <= 0)
                return;

            this._updateMarkers(visibleWidth, this.duration);
        }

        this._scheduledMarkerLayoutUpdateIdentifier = requestAnimationFrame(update.bind(this));
    },

    _needsSelectionLayout: function()
    {
        if (!this._allowsTimeRangeSelection)
            return;

        // If layout is scheduled, abort since the selection will be updated when layout happens.
        if (this._scheduledLayoutUpdateIdentifier)
            return;

        if (this._scheduledSelectionLayoutUpdateIdentifier)
            return;

        function update()
        {
            delete this._scheduledSelectionLayoutUpdateIdentifier;

            var visibleWidth = this._element.clientWidth;
            if (visibleWidth <= 0)
                return;

            this._updateSelection(visibleWidth, this.duration);
        }

        this._scheduledSelectionLayoutUpdateIdentifier = requestAnimationFrame(update.bind(this));
    },

    _recalculate: function()
    {
        var visibleWidth = this._element.clientWidth;
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

    _updatePositionOfElement: function(element, newPosition, visibleWidth, property)
    {
        property = property || "left";

        newPosition *= this._endTimePinned ? 100 : visibleWidth;
        newPosition = newPosition.toFixed(2);

        var currentPosition = parseFloat(element.style[property]).toFixed(2);
        if (currentPosition !== newPosition)
            element.style[property] = newPosition + (this._endTimePinned ? "%" : "px");
    },

    _updateMarkers: function(visibleWidth, duration)
    {
        if (this._scheduledMarkerLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledMarkerLayoutUpdateIdentifier);
            delete this._scheduledMarkerLayoutUpdateIdentifier;
        }

        this._markerElementMap.forEach(function(markerElement, marker) {
            var newLeftPosition = (marker.time - this._startTime) / duration;

            this._updatePositionOfElement(markerElement, newLeftPosition, visibleWidth);

            if (!markerElement.parentNode)
                this._markersElement.appendChild(markerElement);
        }, this);
    },

    _updateSelection: function(visibleWidth, duration)
    {
        if (this._scheduledSelectionLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledSelectionLayoutUpdateIdentifier);
            delete this._scheduledSelectionLayoutUpdateIdentifier;
        }

        this._element.classList.toggle(WebInspector.TimelineRuler.AllowsTimeRangeSelectionStyleClassName, this._allowsTimeRangeSelection);

        if (!this._allowsTimeRangeSelection)
            return;

        var newLeftPosition = Math.max(0, (this._selectionStartTime - this._startTime) / duration);
        this._updatePositionOfElement(this._leftShadedAreaElement, newLeftPosition, visibleWidth, "width");
        this._updatePositionOfElement(this._leftSelectionHandleElement, newLeftPosition, visibleWidth, "left");
        this._updatePositionOfElement(this._selectionDragElement, newLeftPosition, visibleWidth, "left");

        var newRightPosition = 1 - Math.min((this._selectionEndTime - this._startTime) / duration, 1);
        this._updatePositionOfElement(this._rightShadedAreaElement, newRightPosition, visibleWidth, "width");
        this._updatePositionOfElement(this._rightSelectionHandleElement, newRightPosition, visibleWidth, "right");
        this._updatePositionOfElement(this._selectionDragElement, newRightPosition, visibleWidth, "right");

        if (!this._selectionDragElement.parentNode) {
            this._element.appendChild(this._selectionDragElement);
            this._element.appendChild(this._leftShadedAreaElement);
            this._element.appendChild(this._leftSelectionHandleElement);
            this._element.appendChild(this._rightShadedAreaElement);
            this._element.appendChild(this._rightSelectionHandleElement);
        }

        if (this._timeRangeSelectionChanged)
            this._dispatchTimeRangeSelectionChangedEvent();
    },

    _dispatchTimeRangeSelectionChangedEvent: function()
    {
        delete this._timeRangeSelectionChanged;

        if (this._suppressTimeRangeSelectionChangedEvent)
            return;

        this.dispatchEventToListeners(WebInspector.TimelineRuler.Event.TimeRangeSelectionChanged);
    },

    _timelineMarkerTimeChanged: function()
    {
        this._needsMarkerLayout();
    },

    _handleMouseDown: function(event)
    {
        // Only handle left mouse clicks.
        if (event.button !== 0 || event.ctrlKey)
            return;

        this._selectionIsMove = event.target === this._selectionDragElement;
        this._suppressTimeRangeSelectionChangedEvent = !this._selectionIsMove;

        if (this._selectionIsMove)
            this._lastMousePosition = event.pageX;
        else
            this._mouseDownPosition = event.pageX - this._element.totalOffsetLeft;

        this._mouseMoveEventListener = this._handleMouseMove.bind(this);
        this._mouseUpEventListener = this._handleMouseUp.bind(this);

        // Register these listeners on the document so we can track the mouse if it leaves the ruler.
        document.addEventListener("mousemove", this._mouseMoveEventListener);
        document.addEventListener("mouseup", this._mouseUpEventListener);

        event.preventDefault();
        event.stopPropagation();
    },

    _handleMouseMove: function(event)
    {
        console.assert(event.button === 0);

        if (this._selectionIsMove) {
            var currentMousePosition = event.pageX;

            var offsetTime = (currentMousePosition - this._lastMousePosition) * this.secondsPerPixel;
            var selectionDuration = this.selectionEndTime - this.selectionStartTime;

            this.selectionStartTime = Math.max(this.startTime, Math.min(this.selectionStartTime + offsetTime, this.endTime - selectionDuration));
            this.selectionEndTime = this.selectionStartTime + selectionDuration;

            this._lastMousePosition = currentMousePosition;
        } else {
            var currentMousePosition = event.pageX - this._element.totalOffsetLeft;

            this.selectionStartTime = Math.max(this.startTime, this.startTime + (Math.min(currentMousePosition, this._mouseDownPosition) * this.secondsPerPixel));
            this.selectionEndTime = Math.min(this.startTime + (Math.max(currentMousePosition, this._mouseDownPosition) * this.secondsPerPixel), this.endTime);
        }

        this._updateSelection(this._element.clientWidth, this.duration);

        event.preventDefault();
        event.stopPropagation();
    },

    _handleMouseUp: function(event)
    {
        console.assert(event.button === 0);

        if (!this._selectionIsMove && this.selectionEndTime - this.selectionStartTime < WebInspector.TimelineRuler.MinimumSelectionTimeRange) {
            // The section is smaller than allowed, grow in the direction of the drag to meet the minumum.
            var currentMousePosition = event.pageX - this._element.totalOffsetLeft;
            if (currentMousePosition > this._mouseDownPosition) {
                this.selectionEndTime = Math.min(this.selectionStartTime + WebInspector.TimelineRuler.MinimumSelectionTimeRange, this.endTime);
                this.selectionStartTime = this.selectionEndTime - WebInspector.TimelineRuler.MinimumSelectionTimeRange;
            } else {
                this.selectionStartTime = Math.max(this.startTime, this.selectionEndTime - WebInspector.TimelineRuler.MinimumSelectionTimeRange);
                this.selectionEndTime = this.selectionStartTime + WebInspector.TimelineRuler.MinimumSelectionTimeRange
            }
        }

        delete this._suppressTimeRangeSelectionChangedEvent;

        this._dispatchTimeRangeSelectionChangedEvent();

        document.removeEventListener("mousemove", this._mouseMoveEventListener);
        document.removeEventListener("mouseup", this._mouseUpEventListener);

        delete this._mouseMovedEventListener;
        delete this._mouseUpEventListener;
        delete this._mouseDownPosition;
        delete this._lastMousePosition;
        delete this._selectionIsMove;

        event.preventDefault();
        event.stopPropagation();
    },

    _handleSelectionHandleMouseDown: function(event)
    {
        // Only handle left mouse clicks.
        if (event.button !== 0 || event.ctrlKey)
            return;

        this._dragHandleIsStartTime = event.target === this._leftSelectionHandleElement;
        this._mouseDownPosition = event.pageX - this._element.totalOffsetLeft;

        this._selectionHandleMouseMoveEventListener = this._handleSelectionHandleMouseMove.bind(this);
        this._selectionHandleMouseUpEventListener = this._handleSelectionHandleMouseUp.bind(this);

        // Register these listeners on the document so we can track the mouse if it leaves the ruler.
        document.addEventListener("mousemove", this._selectionHandleMouseMoveEventListener);
        document.addEventListener("mouseup", this._selectionHandleMouseUpEventListener);

        event.preventDefault();
        event.stopPropagation();
    },

    _handleSelectionHandleMouseMove: function(event)
    {
        console.assert(event.button === 0);

        var currentMousePosition = event.pageX - this._element.totalOffsetLeft;
        var currentTime = this.startTime + (currentMousePosition * this.secondsPerPixel);

        if (event.altKey && !event.ctrlKey && !event.metaKey && !event.shiftKey) {
            // Resize the selection on both sides when the Option keys is held down.
            if (this._dragHandleIsStartTime) {
                var timeDifference = currentTime - this.selectionStartTime;
                this.selectionStartTime = Math.max(this.startTime, Math.min(currentTime, this.selectionEndTime - WebInspector.TimelineRuler.MinimumSelectionTimeRange));
                this.selectionEndTime = Math.min(Math.max(this.selectionStartTime + WebInspector.TimelineRuler.MinimumSelectionTimeRange, this.selectionEndTime - timeDifference), this.endTime);
            } else {
                var timeDifference = currentTime - this.selectionEndTime;
                this.selectionEndTime = Math.min(Math.max(this.selectionStartTime + WebInspector.TimelineRuler.MinimumSelectionTimeRange, currentTime), this.endTime);
                this.selectionStartTime = Math.max(this.startTime, Math.min(this.selectionStartTime - timeDifference, this.selectionEndTime - WebInspector.TimelineRuler.MinimumSelectionTimeRange));
            }
        } else {
            // Resize the selection on side being dragged.
            if (this._dragHandleIsStartTime)
                this.selectionStartTime = Math.max(this.startTime, Math.min(currentTime, this.selectionEndTime - WebInspector.TimelineRuler.MinimumSelectionTimeRange));
            else
                this.selectionEndTime = Math.min(Math.max(this.selectionStartTime + WebInspector.TimelineRuler.MinimumSelectionTimeRange, currentTime), this.endTime);
        }

        this._updateSelection(this._element.clientWidth, this.duration);

        event.preventDefault();
        event.stopPropagation();
    },

    _handleSelectionHandleMouseUp: function(event)
    {
        console.assert(event.button === 0);

        document.removeEventListener("mousemove", this._selectionHandleMouseMoveEventListener);
        document.removeEventListener("mouseup", this._selectionHandleMouseUpEventListener);

        delete this._selectionHandleMouseMoveEventListener;
        delete this._selectionHandleMouseUpEventListener;
        delete this._dragHandleIsStartTime;
        delete this._mouseDownPosition;

        event.preventDefault();
        event.stopPropagation();
    }
}

WebInspector.TimelineRuler.prototype.__proto__ = WebInspector.Object.prototype;
