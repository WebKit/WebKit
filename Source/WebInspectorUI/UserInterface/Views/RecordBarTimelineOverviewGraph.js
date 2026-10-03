/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    distribution and/or other materials provided with the distribution.
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

WI.RecordBarTimelineOverviewGraph = class RecordBarTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timelineOverview)
    {
        super(timelineOverview);

        this._usesSelectedRecordBar = true;
        this._recordBarGeometries = [];
        this._lastSelectedRecordInLayout = null;

        this.element.addEventListener("click", this._handleRecordBarClick.bind(this));
    }

    // Public

    reset()
    {
        super.reset();

        this.selectedRecordBar = null;
        this._recordBarGeometries = [];
        this._lastSelectedRecordInLayout = null;
    }

    // Protected

    clearCanvas()
    {
        super.clearCanvas();

        this.selectedRecordBar = null;
        this._recordBarGeometries = [];
        this._lastSelectedRecordInLayout = this.selectedRecord;
    }

    beginRecordBarLayout()
    {
        this._recordBarGeometries = [];
    }

    addRecordBars(records, y, height, cornerRadius, roundAllCorners)
    {
        WI.TimelineRecordBar.createCombinedBars(records, this.secondsPerPixel, this, (records, renderMode) => {
            let geometry = this._createRecordBarGeometry(records, renderMode, y, height, cornerRadius, roundAllCorners);
            if (geometry)
                this._recordBarGeometries.push(geometry);
        });
    }

    drawCanvas(context)
    {
        let selectedRecordBar = null;
        if (this._usesSelectedRecordBar && this.selectedRecord)
            selectedRecordBar = this._recordBarGeometries.find((recordBar) => recordBar.records.includes(this.selectedRecord)) || null;
        this.selectedRecordBar = selectedRecordBar;
        this._lastSelectedRecordInLayout = this.selectedRecord;

        let focusedSelection = this.selected && !!this.element.closest(":focus");
        let selectedStyles = { // Keep this in sync with `.timeline-record-bar.selected > .segment`.
            fill: this.cssVariableValue("--selected-text-background-color"),
            stroke: this.cssVariableValue("--glyph-color-active"),
        };
        const focusedSelectedStyles = { // Keep this in sync with `:focus .selected .timeline-record-bar:not(.has-custom-children) > .segment`.
            fill: "white",
            stroke: "transparent",
        };
        const focusedSelectedInactiveStyles = { // Keep this in sync with `:focus .selected .timeline-record-bar:not(.has-custom-children) > .segment.inactive`.
            fill: "hsl(215, 63%, 85%)",
            stroke: "transparent",
        };

        for (let recordBar of this._recordBarGeometries) {
            let record = recordBar.records[0];
            for (let segment of recordBar.segments) {
                let styles;
                if (recordBar.selected)
                    styles = focusedSelection ? (segment.inactive ? focusedSelectedInactiveStyles : focusedSelectedStyles) : selectedStyles;
                else {
                    let styleKey = this._styleKeyForRecordBar(record, segment.inactive);
                    styles = WI.RecordBarTimelineOverviewGraph.EventStyles[styleKey];
                }

                let {fill, stroke} = styles;
                this._drawRecordBarSegment(context, segment, recordBar.y, recordBar.height, recordBar.cornerRadius, fill, stroke);

                if (!segment.inactive && (record.type === WI.TimelineRecord.Type.Network || (record.type === WI.TimelineRecord.Type.Media && segment.roundEnd)))
                    context.clearRect(segment.x + segment.width, recordBar.y, 1, recordBar.height);
            }
        }
    }

    updateSelectedRecord()
    {
        super.updateSelectedRecord();

        if (!this._usesSelectedRecordBar) {
            this.selectedRecordBar = null;
            return;
        }

        if (this._lastSelectedRecordInLayout === this.selectedRecord)
            return;

        this.selectedRecordBar = this.selectedRecord ? this._recordBarGeometries.find((recordBar) => recordBar.records.includes(this.selectedRecord)) || null : null;
        this.needsLayout();
    }

    // Private

    _createRecordBarGeometry(records, renderMode, y, height, cornerRadius, roundAllCorners)
    {
        if (isNaN(this.secondsPerPixel) || !records.length)
            return null;

        let firstRecord = records[0];
        let barStartTime = firstRecord.startTime;
        if (isNaN(barStartTime))
            return null;

        let barEndTime = records.reduce((previousValue, currentValue) => Math.max(previousValue, currentValue.endTime), 0);
        if (barStartTime > this.currentTime || barEndTime < this.startTime || barStartTime > this.endTime)
            return null;

        let barUnfinished = isNaN(barEndTime) || barEndTime >= this.currentTime;
        if (barUnfinished)
            barEndTime = this.currentTime;

        let duration = barEndTime - barStartTime;
        let x = (barStartTime - this.startTime) / this.secondsPerPixel;
        let width = duration / this.secondsPerPixel;
        let segments = [];

        let addSegment = (startTime, endTime, inactive, roundStart, roundEnd) => {
            segments.push({
                x: (startTime - this.startTime) / this.secondsPerPixel,
                width: Math.max((endTime - startTime) / this.secondsPerPixel, WI.TimelineRecordBar.MinimumWidthPixels),
                inactive,
                roundStart: roundAllCorners || roundStart,
                roundEnd: roundAllCorners || roundEnd,
                strokeEnd: roundEnd,
            });
        };

        if (!firstRecord.usesActiveStartTime) {
            if (renderMode === WI.TimelineRecordBar.RenderMode.InactiveOnly)
                return null;

            addSegment(barStartTime, barEndTime, false, true, !barUnfinished);
        } else {
            let barActiveStartTime;
            if (renderMode === WI.TimelineRecordBar.RenderMode.ActiveOnly)
                barActiveStartTime = records.reduce((previousValue, currentValue) => Math.min(previousValue, currentValue.activeStartTime), Infinity);
            else
                barActiveStartTime = records.reduce((previousValue, currentValue) => Math.max(previousValue, currentValue.activeStartTime), 0);

            let inactiveUnfinished = isNaN(barActiveStartTime) || barActiveStartTime >= this.currentTime;
            if (inactiveUnfinished)
                barActiveStartTime = this.currentTime;
            else if (renderMode === WI.TimelineRecordBar.RenderMode.Normal) {
                let minimumSegmentDuration = this.secondsPerPixel * WI.TimelineRecordBar.MinimumWidthPixels;
                if (barActiveStartTime - barStartTime < minimumSegmentDuration)
                    barActiveStartTime = barStartTime;
            }

            let showInactiveSegment = barActiveStartTime > barStartTime;
            if (showInactiveSegment && renderMode !== WI.TimelineRecordBar.RenderMode.ActiveOnly)
                addSegment(barStartTime, barActiveStartTime, true, true, false);

            if (!inactiveUnfinished && renderMode !== WI.TimelineRecordBar.RenderMode.InactiveOnly)
                addSegment(barActiveStartTime, barEndTime, false, !showInactiveSegment, true);
        }

        if (!segments.length)
            return null;

        return {
            records,
            renderMode,
            x,
            width,
            duration,
            y,
            height,
            cornerRadius,
            segments,
            selected: false,
        };
    }

    _drawRecordBarSegment(context, segment, y, height, cornerRadius, fillStyle, strokeStyle)
    {
        let left = segment.x + 0.5;
        let right = segment.x + segment.width - 0.5;
        let top = y + 0.5;
        let bottom = y + height - 0.5;
        let maximumRadius = Math.min((right - left) / 2, (bottom - top) / 2);
        let startRadius = segment.roundStart ? Math.min(cornerRadius, maximumRadius) : 0;
        let endRadius = segment.roundEnd ? Math.min(cornerRadius, maximumRadius) : 0;

        context.beginPath();
        context.moveTo(left + startRadius, top);
        context.lineTo(right - endRadius, top);
        context.quadraticCurveTo(right, top, right, top + endRadius);
        context.lineTo(right, bottom - endRadius);
        context.quadraticCurveTo(right, bottom, right - endRadius, bottom);
        context.lineTo(left + startRadius, bottom);
        context.quadraticCurveTo(left, bottom, left, bottom - startRadius);
        context.lineTo(left, top + startRadius);
        context.quadraticCurveTo(left, top, left + startRadius, top);
        context.closePath();

        context.fillStyle = fillStyle;
        context.fill();

        if (strokeStyle === "transparent")
            return;

        context.strokeStyle = strokeStyle;
        context.lineWidth = 1;
        if (segment.strokeEnd) {
            context.stroke();
            return;
        }

        context.beginPath();
        context.moveTo(right, top + endRadius);
        context.quadraticCurveTo(right, top, right - endRadius, top);
        context.lineTo(left + startRadius, top);
        context.quadraticCurveTo(left, top, left, top + startRadius);
        context.lineTo(left, bottom - startRadius);
        context.quadraticCurveTo(left, bottom, left + startRadius, bottom);
        context.lineTo(right - endRadius, bottom);
        context.quadraticCurveTo(right, bottom, right, bottom - endRadius);
        context.stroke();
    }

    _styleKeyForRecordBar(record, inactive)
    {
        switch (record.type) {
        case WI.TimelineRecord.Type.Network:
            return inactive ? "network-inactive" : "network";

        case WI.TimelineRecord.Type.Layout:
            switch (record.eventType) {
            case WI.LayoutTimelineRecord.EventType.InvalidateStyles:
            case WI.LayoutTimelineRecord.EventType.RecalculateStyles:
                return "style";

            case WI.LayoutTimelineRecord.EventType.Paint:
            case WI.LayoutTimelineRecord.EventType.Composite:
                return "paint";

            case WI.LayoutTimelineRecord.EventType.FirstContentfulPaint:
            case WI.LayoutTimelineRecord.EventType.LargestContentfulPaint:
                return "milestone";
            }
            return "layout";

        case WI.TimelineRecord.Type.Script:
            return record.isGarbageCollection() ? "garbage-collection" : "script";

        case WI.TimelineRecord.Type.Media:
            return "media";
        }

        return "default";
    }

    _handleRecordBarClick(event)
    {
        let {x, y} = this.canvasPositionForEvent(event);

        let recordBar = null;
        for (let i = this._recordBarGeometries.length - 1; i >= 0; --i) {
            let candidate = this._recordBarGeometries[i];
            let hitStart = candidate.x;
            let hitEnd = candidate.x + candidate.width;
            for (let segment of candidate.segments) {
                hitStart = Math.min(hitStart, segment.x);
                hitEnd = Math.max(hitEnd, segment.x + segment.width);
            }

            if (x >= hitStart && x <= hitEnd && y >= candidate.y && y <= candidate.y + candidate.height) {
                recordBar = candidate;
                break;
            }
        }
        if (!recordBar)
            return;

        // Ensure that the container "click" listener added by `WI.TimelineOverview` isn't called.
        event.__timelineRecordClickEventHandled = true;

        let record = this._recordForRecordBarAtPosition(recordBar, x);
        if (record)
            this.timelineRecordBarClicked(record);
    }

    _recordForRecordBarAtPosition(recordBar, x)
    {
        if (!recordBar.duration)
            return null;

        if (recordBar.records.length === 1)
            return recordBar.records[0];

        let relativeMouseX = Number.constrain((x - recordBar.x) / recordBar.width, 0, 1);
        let targetRecordTime = recordBar.records[0].startTime + (recordBar.duration * relativeMouseX);
        let closestRecord = null;
        let closestRecordTimeDelta = Infinity;
        for (let record of recordBar.records) {
            if (record.children.length)
                continue;

            if (targetRecordTime >= record.startTime && targetRecordTime <= record.endTime)
                return record;

            let timeBetweenRecordAndTargetTime = Math.min(Math.abs(record.startTime - targetRecordTime), Math.abs(record.endTime - targetRecordTime));
            if (timeBetweenRecordAndTargetTime > closestRecordTimeDelta)
                break;

            closestRecord = record;
            closestRecordTimeDelta = timeBetweenRecordAndTargetTime;
        }

        console.assert(closestRecord, recordBar, x);
        return closestRecord;
    }
};

WI.RecordBarTimelineOverviewGraph.EventStyles = {
    "default": {
        fill: "hsl(0, 0%, 88%)", // Keep this in sync with `--timeline-record-bar-default-fill-color`.
        stroke: "hsl(0, 0%, 78%)", // Keep this in sync with `--timeline-record-bar-default-stroke-color`.
    },
    "network": {
        fill: "hsl(207, 63%, 67%)", // Keep this in sync with `--timeline-record-bar-network-fill-color`.
        stroke: "hsl(202, 55%, 51%)", // Keep this in sync with `--timeline-record-bar-network-stroke-color`.
    },
    "network-inactive": {
        fill: "hsl(208, 66%, 79%)", // Keep this in sync with `--timeline-record-bar-network-inactive-fill-color`.
        stroke: "hsl(202, 57%, 68%)", // Keep this in sync with `--timeline-record-bar-network-inactive-stroke-color`.
    },
    "layout": {
        fill: "hsl(0, 65%, 75%)", // Keep this in sync with `--timeline-record-bar-layout-fill-color`.
        stroke: "hsl(0, 54%, 62%)", // Keep this in sync with `--timeline-record-bar-layout-stroke-color`.
    },
    "style": {
        fill: "hsl(23, 69%, 73%)", // Keep this in sync with `--timeline-record-bar-style-fill-color`.
        stroke: "hsl(11, 54%, 62%)", // Keep this in sync with `--timeline-record-bar-style-stroke-color`.
    },
    "paint": {
        fill: "hsl(76, 49%, 60%)", // Keep this in sync with `--timeline-record-bar-paint-fill-color`.
        stroke: "hsl(79, 45%, 51%)", // Keep this in sync with `--timeline-record-bar-paint-stroke-color`.
    },
    "milestone": {
        fill: "hsl(172, 48%, 75%)", // Keep this in sync with `--timeline-record-bar-milestone-fill-color`.
        stroke: "hsl(173, 46%, 45%)", // Keep this in sync with `--timeline-record-bar-milestone-stroke-color`.
    },
    "script": {
        fill: "hsl(269, 65%, 74%)", // Keep this in sync with `--timeline-record-bar-script-fill-color`.
        stroke: "hsl(273, 33%, 58%)", // Keep this in sync with `--timeline-record-bar-script-stroke-color`.
    },
    "garbage-collection": {
        fill: "hsl(0, 0%, 70%)", // Keep this in sync with `--timeline-record-bar-garbage-collection-fill-color`.
        stroke: "hsl(0, 0%, 55%)", // Keep this in sync with `--timeline-record-bar-garbage-collection-stroke-color`.
    },
    "media": {
        fill: "hsl(143, 24%, 66%)", // Keep this in sync with `--timeline-record-bar-media-fill-color`.
        stroke: "hsl(153, 24%, 51%)", // Keep this in sync with `--timeline-record-bar-media-stroke-color`.
    },
};
