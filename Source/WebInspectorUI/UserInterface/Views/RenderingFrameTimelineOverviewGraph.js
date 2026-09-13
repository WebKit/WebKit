/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WI.RenderingFrameTimelineOverviewGraph = class RenderingFrameTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        super(timelineOverview);

        this.element.classList.add("rendering-frame");
        this.element.addEventListener("click", this._mouseClicked.bind(this));

        this._renderingFrameTimeline = timeline;
        this._renderingFrameTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._timelineRecordAdded, this);

        this._selectedFrameMarker = document.createElement("div");
        this._selectedFrameMarker.classList.add("frame-marker");

        this._cachedDisplayDataForRecord = new WeakMap;
        this._frameGeometries = [];
        this._lastSelectedRecordInLayout = null;
        this._graphHeightSeconds = NaN;
        this._framesPerSecondDividerMap = new Map;

        this.reset();
    }

    // Public

    get graphHeightSeconds()
    {
        if (!isNaN(this._graphHeightSeconds))
            return this._graphHeightSeconds;

        var maximumFrameDuration = this._renderingFrameTimeline.records.reduce(function(previousValue, currentValue) {
            return Math.max(previousValue, currentValue.duration);
        }, 0);

        this._graphHeightSeconds = maximumFrameDuration * 1.1; // Add 10% margin above frames.
        this._graphHeightSeconds = Math.min(this._graphHeightSeconds, WI.RenderingFrameTimelineOverviewGraph.MaximumGraphHeightSeconds);
        this._graphHeightSeconds = Math.max(this._graphHeightSeconds, WI.RenderingFrameTimelineOverviewGraph.MinimumGraphHeightSeconds);
        return this._graphHeightSeconds;
    }

    reset()
    {
        super.reset();

        this.selectedRecord = null;

        this._selectedFrameMarker.remove();
        this._cachedDisplayDataForRecord = new WeakMap;
        this._frameGeometries = [];
        this._lastSelectedRecordInLayout = null;

        this._graphHeightSeconds = NaN;

        for (let divider of this._framesPerSecondDividerMap.values())
            divider.remove();
        this._framesPerSecondDividerMap.clear();
    }

    recordWasFiltered(record, filtered)
    {
        super.recordWasFiltered(record, filtered);

        if (!(record instanceof WI.RenderingFrameTimelineRecord))
            return;

        record[WI.RenderingFrameTimelineOverviewGraph.RecordWasFilteredSymbol] = filtered;

        // Update the canvas if the frame is within the visible range.
        const startIndex = Math.floor(this.startTime);
        const endIndex = Math.min(Math.floor(this.endTime), this._renderingFrameTimeline.records.length - 1);
        if (record.frameIndex < startIndex || record.frameIndex > endIndex)
            return;

        this.needsLayout();
    }

    // Protected

    get height()
    {
        return 108;
    }

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        this._frameGeometries = [];
        this._lastSelectedRecordInLayout = this.selectedRecord;

        let graphWidth = this.timelineOverview.scrollContainerWidth;
        if (isNaN(graphWidth)) {
            this.clearCanvas();
            return;
        }

        let records = this._renderingFrameTimeline.records;
        if (!records.length) {
            this.clearCanvas();
            this._updateFrameMarker();
            return;
        }

        let startIndex = Math.max(Math.floor(this.startTime), 0);
        let endIndex = Math.min(Math.floor(this.endTime), records.length - 1);
        let frameWidth = 1 / this.timelineOverview.secondsPerPixel;
        let canvasHeight = this.canvasHeight;

        for (let i = startIndex; i <= endIndex; ++i) {
            let record = records[i];
            let displayData = this._displayDataForRecord(record, canvasHeight);
            this._frameGeometries.push({
                record,
                displayData,
                x: (record.frameIndex - this.startTime) / this.timelineOverview.secondsPerPixel,
                width: frameWidth,
                height: displayData.frameDuration / this.graphHeightSeconds * canvasHeight,
                filtered: record[WI.RenderingFrameTimelineOverviewGraph.RecordWasFilteredSymbol] || false,
                selected: record === this.selectedRecord,
            });
        }

        this.updateCanvas();

        this._updateDividers();
        this._updateFrameMarker();
    }

    updateSelectedRecord()
    {
        if (!this.selectedRecord) {
            this._updateFrameMarker();

            if (this._lastSelectedRecordInLayout)
                this.needsLayout();
            return;
        }

        const visibleDuration = this.timelineOverview.visibleDuration;
        const frameIndex = this.selectedRecord.frameIndex;

        // Reveal a newly selected record if it's outside the visible range.
        if (frameIndex < Math.ceil(this.timelineOverview.scrollStartTime) || frameIndex >= this.timelineOverview.scrollStartTime + visibleDuration) {
            var scrollStartTime = frameIndex;
            if (!this._lastSelectedRecordInLayout || Math.abs(this._lastSelectedRecordInLayout.frameIndex - this.selectedRecord.frameIndex) > 1) {
                scrollStartTime -= Math.floor(visibleDuration / 2);
                scrollStartTime = Math.max(Math.min(scrollStartTime, this.timelineOverview.endTime), this.timelineOverview.startTime);
            }

            this.timelineOverview.scrollStartTime = scrollStartTime;
            return;
        }

        this._updateFrameMarker();

        if (this._lastSelectedRecordInLayout !== this.selectedRecord)
            this.needsLayout();
    }

    drawCanvas(context)
    {
        let selectedFillStyle = document.body.classList.contains("window-inactive") ? "hsl(0, 0%, 96%)" : this.cssVariableValue("--selected-text-background-color");
        let tallFrameMaskGradient = null;
        let canvasHeight = this.canvasHeight;

        for (let frame of this._frameGeometries) {
            let x = frame.x + 1;
            let width = Math.max(frame.width - 1, 0);
            if (frame.selected) {
                context.fillStyle = selectedFillStyle;
                context.fillRect(x, 0, width, canvasHeight - 1);
            }

            context.save();
            if (frame.filtered)
                context.globalAlpha = 0.35;

            let bottom = canvasHeight - 1;
            for (let i = 0; i < frame.displayData.segments.length; ++i) {
                let segment = frame.displayData.segments[i];
                let segmentHeight = segment.duration / this.graphHeightSeconds * canvasHeight;
                let top = bottom - segmentHeight;
                let styles = WI.RecordBarTimelineOverviewGraph.EventStyles[segment.styleKey];

                context.fillStyle = styles?.fill || "hsl(0, 0%, 90%)";
                context.fillRect(x, top, width, segmentHeight);

                if (width > 1 && segmentHeight > 1) {
                    let left = x + 0.5;
                    let right = x + width - 0.5;
                    let strokeTop = top + 0.5;
                    let strokeBottom = bottom - 0.5;

                    context.beginPath();
                    context.moveTo(left, strokeBottom);
                    context.lineTo(left, strokeTop);
                    context.lineTo(right, strokeTop);
                    context.lineTo(right, strokeBottom);
                    if (!i)
                        context.lineTo(left, strokeBottom);

                    context.strokeStyle = styles?.stroke || "hsl(0, 0%, 82%)";
                    context.lineWidth = 1;
                    context.stroke();
                }

                bottom = top;
            }

            if (frame.selected) {
                context.fillStyle = "white";
                context.fillRect(frame.x, canvasHeight - frame.height - 2, frame.width, 1);
            }
            context.restore();

            if (frame.height / canvasHeight >= 0.95) {
                context.save();
                context.beginPath();
                context.rect(frame.x, 0, frame.width, canvasHeight);
                context.clip();
                context.globalCompositeOperation = "destination-in";

                if (!tallFrameMaskGradient) {
                    tallFrameMaskGradient = context.createLinearGradient(0, 0, 0, canvasHeight * 0.1);
                    tallFrameMaskGradient.addColorStop(0, "transparent");
                    tallFrameMaskGradient.addColorStop(1, "black");
                }
                context.fillStyle = tallFrameMaskGradient;
                context.fillRect(frame.x, 0, frame.width, canvasHeight);
                context.restore();
            }
        }
    }

    // Private

    _displayDataForRecord(record, canvasHeight)
    {
        let graphHeightSeconds = this.graphHeightSeconds;
        let displayData = this._cachedDisplayDataForRecord.get(record);
        if (displayData && displayData.graphHeightSeconds === graphHeightSeconds && displayData.canvasHeight === canvasHeight)
            return displayData;

        let secondsPerBlock = (graphHeightSeconds / canvasHeight) * 3;
        let segments = [];
        let invisibleSegments = [];
        let currentSegment = null;

        function updateDurationRemainder(segment) {
            if (segment.duration <= secondsPerBlock) {
                segment.remainder = 0;
                return;
            }

            let roundedDuration = Math.roundTo(segment.duration, secondsPerBlock);
            segment.remainder = Math.max(segment.duration - roundedDuration, 0);
        }

        function pushCurrentSegment() {
            updateDurationRemainder(currentSegment);
            segments.push(currentSegment);
            if (currentSegment.duration < secondsPerBlock)
                invisibleSegments.push({segment: currentSegment, index: segments.length - 1});

            currentSegment = null;
        }

        // Frame segments aren't shown at arbitrary pixel heights, but are divided into blocks of pixels.
        // One block represents the minimum displayable duration of a rendering frame, in seconds.
        // Contiguous tasks less than a block high are grouped until the minimum is met or a task meeting the minimum is found.
        // The group is then added to the list of segment candidates.
        // Large tasks (i.e., one block or more) are simply added to the candidate list instead of being grouped with other tasks.
        for (let key in WI.RenderingFrameTimelineRecord.TaskType) {
            let taskType = WI.RenderingFrameTimelineRecord.TaskType[key];
            let duration = record.durationForTask(taskType);
            if (duration === 0)
                continue;

            if (currentSegment && duration >= secondsPerBlock)
                pushCurrentSegment();

            currentSegment ||= {styleKey: null, longestTaskDuration: 0, duration: 0, remainder: 0};
            currentSegment.duration += duration;
            if (duration > currentSegment.longestTaskDuration) {
                currentSegment.styleKey = key.toLowerCase();
                currentSegment.longestTaskDuration = duration;
            }

            if (currentSegment.duration >= secondsPerBlock)
                pushCurrentSegment();
        }

        if (currentSegment)
            pushCurrentSegment();

        // A frame consisting of a single segment is always visible.
        if (segments.length === 1) {
            segments[0].duration = Math.max(segments[0].duration, secondsPerBlock);
            invisibleSegments = [];
        }

        // Handle any groups that are still beneath the minimum displayable duration.
        // Each sub-block task has one or two adjacent display segments greater than one block.
        // The rounded-off time from these tasks is added to the sub-block if it's sufficient to create a full block.
        // Otherwise, the task is merged with an adjacent segment.
        invisibleSegments.sort((a, b) => a.segment.duration - b.segment.duration);

        for (let item of invisibleSegments) {
            let segment = item.segment;
            let previousSegment = item.index > 0 ? segments[item.index - 1] : null;
            let nextSegment = item.index < segments.length - 1 ? segments[item.index + 1] : null;
            console.assert(previousSegment || nextSegment, "Invisible segment should have at least one adjacent visible segment.", record, item, segments);

            // Try to increase the segment's size to exactly one block by taking time from neighboring segments.
            // If there are two neighbors the one with greater subblock duration is borrowed from first.
            let adjacentSegments;
            let availableDuration;
            if (previousSegment && nextSegment) {
                adjacentSegments = previousSegment.remainder > nextSegment.remainder ? [previousSegment, nextSegment] : [nextSegment, previousSegment];
                availableDuration = previousSegment.remainder + nextSegment.remainder;
            } else {
                adjacentSegments = [previousSegment || nextSegment];
                availableDuration = adjacentSegments[0].remainder;
            }

            if (availableDuration < (secondsPerBlock - segment.duration)) {
                // Merge with largest adjacent segment.
                let targetSegment;
                if (previousSegment && nextSegment)
                    targetSegment = previousSegment.duration > nextSegment.duration ? previousSegment : nextSegment;
                else
                    targetSegment = previousSegment || nextSegment;

                targetSegment.duration += segment.duration;
                updateDurationRemainder(targetSegment);
                continue;
            }

            adjacentSegments.forEach(function(adjacentSegment) {
                if (segment.duration >= secondsPerBlock)
                    return;
                let remainder = Math.min(secondsPerBlock - segment.duration, adjacentSegment.remainder);
                segment.duration += remainder;
                adjacentSegment.remainder -= remainder;
            });
        }

        // Round visible segments to the nearest block and compute the rounded frame duration.
        let frameDuration = 0;
        segments = segments.filter(function(segment) {
            if (segment.duration < secondsPerBlock)
                return false;
            segment.duration = Math.roundTo(segment.duration, secondsPerBlock);
            frameDuration += segment.duration;
            return true;
        });

        displayData = {frameDuration, segments, graphHeightSeconds, canvasHeight};
        this._cachedDisplayDataForRecord.set(record, displayData);
        return displayData;
    }

    _timelineRecordAdded(event)
    {
        this._graphHeightSeconds = NaN;

        this.needsLayout();
    }

    _updateDividers()
    {
        if (this.graphHeightSeconds === 0)
            return;

        let overviewGraphHeight = this.canvasHeight;

        function createDividerAtPosition(framesPerSecond)
        {
            var secondsPerFrame = 1 / framesPerSecond;
            var dividerTop = 1 - secondsPerFrame / this.graphHeightSeconds;
            if (dividerTop < 0.01 || dividerTop >= 1)
                return;

            var divider = this._framesPerSecondDividerMap.get(framesPerSecond);
            if (!divider) {
                divider = document.createElement("div");
                divider.classList.add("divider");

                var label = document.createElement("div");
                label.classList.add("label");
                label.innerText = WI.UIString("%d FPS").format(framesPerSecond);
                divider.appendChild(label);

                this.element.appendChild(divider);

                this._framesPerSecondDividerMap.set(framesPerSecond, divider);
            }

            divider.style.marginTop = (dividerTop * overviewGraphHeight).toFixed(2) + "px";
        }

        createDividerAtPosition.call(this, 60);
        createDividerAtPosition.call(this, 30);
    }

    _updateFrameMarker()
    {
        if (!this.selectedRecord) {
            this._selectedFrameMarker.remove();
            return;
        }

        let visibleDuration = this.timelineOverview.visibleDuration;
        if (!visibleDuration) {
            this._selectedFrameMarker.remove();
            return;
        }

        var frameWidth = 1 / this.timelineOverview.secondsPerPixel;
        this._selectedFrameMarker.style.width = frameWidth + "px";

        var markerLeftPosition = this.selectedRecord.frameIndex - this.startTime;
        let property = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? "right" : "left";
        this._selectedFrameMarker.style.setProperty(property, ((markerLeftPosition / visibleDuration) * 100).toFixed(2) + "%");

        if (!this._selectedFrameMarker.parentElement)
            this.element.appendChild(this._selectedFrameMarker);
    }

    _mouseClicked(event)
    {
        let position = this.canvasPositionForEvent(event);

        let frameIndex = Math.floor(position.x * this.timelineOverview.secondsPerPixel + this.startTime);
        if (isNaN(frameIndex) || frameIndex < 0 || frameIndex >= this._renderingFrameTimeline.records.length)
            return;

        let newSelectedRecord = this._renderingFrameTimeline.records[frameIndex];
        if (newSelectedRecord[WI.RenderingFrameTimelineOverviewGraph.RecordWasFilteredSymbol])
            return;

        // Ensure that the container "click" listener added by `WI.TimelineOverview` isn't called.
        event.__timelineRecordClickEventHandled = true;

        if (this.selectedRecord === newSelectedRecord)
            return;

        if (frameIndex >= this.timelineOverview.selectionStartTime && frameIndex < this.timelineOverview.selectionStartTime + this.timelineOverview.selectionDuration) {
            this.selectedRecord = newSelectedRecord;
            return;
        }

        // Clicking a frame outside the current ruler selection changes the selection to include the frame.
        this.selectedRecord = newSelectedRecord;
        this.timelineOverview.selectionStartTime = frameIndex;
        this.timelineOverview.selectionDuration = 1;
    }
};

WI.RenderingFrameTimelineOverviewGraph.RecordWasFilteredSymbol = Symbol("rendering-frame-overview-graph-record-was-filtered");

WI.RenderingFrameTimelineOverviewGraph.MaximumGraphHeightSeconds = 0.037;
WI.RenderingFrameTimelineOverviewGraph.MinimumGraphHeightSeconds = 0.0185;
