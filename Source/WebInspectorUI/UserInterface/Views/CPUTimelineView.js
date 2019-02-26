/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.CPUTimelineView = class CPUTimelineView extends WI.TimelineView
{
    constructor(timeline, extraArguments)
    {
        console.assert(timeline.type === WI.TimelineRecord.Type.CPU, timeline);

        super(timeline, extraArguments);

        this._recording = extraArguments.recording;

        this.element.classList.add("cpu");

        timeline.addEventListener(WI.Timeline.Event.RecordAdded, this._cpuTimelineRecordAdded, this);
    }

    // Static

    static displayNameForSampleType(type)
    {
        switch (type) {
        case WI.CPUTimelineView.SampleType.Script:
            return WI.UIString("Script");
        case WI.CPUTimelineView.SampleType.Layout:
            return WI.UIString("Layout");
        case WI.CPUTimelineView.SampleType.Paint:
            return WI.UIString("Paint");
        case WI.CPUTimelineView.SampleType.Style:
            return WI.UIString("Style Resolution");
        }
        console.error("Unknown sample type", type);
    }

    static get cpuUsageViewHeight() { return 150; }
    static get threadCPUUsageViewHeight() { return 65; }
    static get indicatorViewHeight() { return 15; }

    // Public

    shown()
    {
        super.shown();

        if (this._timelineRuler)
            this._timelineRuler.updateLayout(WI.View.LayoutReason.Resize);
    }

    closed()
    {
        console.assert(this.representedObject instanceof WI.Timeline);
        this.representedObject.removeEventListener(null, null, this);
    }

    reset()
    {
        super.reset();

        this.clear();
    }

    clear()
    {
        if (!this.didInitialLayout)
            return;

        this._breakdownChart.clear();
        this._breakdownChart.needsLayout();
        this._clearBreakdownLegend();

        function clearUsageView(view) {
            view.clear();

            let markersElement = view.chart.element.querySelector(".markers");
            if (markersElement)
                markersElement.remove();
        }

        clearUsageView(this._cpuUsageView);
        clearUsageView(this._mainThreadUsageView);
        clearUsageView(this._webkitThreadUsageView);
        clearUsageView(this._unknownThreadUsageView);

        this._removeWorkerThreadViews();

        this._mainThreadWorkIndicatorView.clear();
    }

    // Protected

    get showsFilterBar() { return false; }

    get scrollableElements()
    {
        return [this.element];
    }

    initialLayout()
    {
        this.element.style.setProperty("--cpu-usage-stacked-view-height", CPUTimelineView.cpuUsageViewHeight + "px");
        this.element.style.setProperty("--cpu-usage-view-height", CPUTimelineView.threadCPUUsageViewHeight + "px");
        this.element.style.setProperty("--cpu-usage-indicator-view-height", CPUTimelineView.indicatorViewHeight + "px");

        let contentElement = this.element.appendChild(document.createElement("div"));
        contentElement.classList.add("content");

        let overviewElement = contentElement.appendChild(document.createElement("div"));
        overviewElement.classList.add("overview");

        function createChartContainer(parentElement, subtitle, tooltip) {
            let chartElement = parentElement.appendChild(document.createElement("div"));
            chartElement.classList.add("chart");

            let chartSubtitleElement = chartElement.appendChild(document.createElement("div"));
            chartSubtitleElement.classList.add("subtitle");
            chartSubtitleElement.textContent = subtitle;
            chartSubtitleElement.title = tooltip;

            let chartFlexContainerElement = chartElement.appendChild(document.createElement("div"));
            chartFlexContainerElement.classList.add("container");
            return chartFlexContainerElement;
        }

        function appendLegendRow(legendElement, sampleType) {
            let rowElement = legendElement.appendChild(document.createElement("div"));
            rowElement.classList.add("row");

            let swatchElement = rowElement.appendChild(document.createElement("div"));
            swatchElement.classList.add("swatch", sampleType);

            let valueContainer = rowElement.appendChild(document.createElement("div"));
            valueContainer.classList.add("value");

            let labelElement = valueContainer.appendChild(document.createElement("div"));
            labelElement.classList.add("label");
            labelElement.textContent = WI.CPUTimelineView.displayNameForSampleType(sampleType);

            let sizeElement = valueContainer.appendChild(document.createElement("div"));
            sizeElement.classList.add("size");

            return sizeElement;
        }

        let breakdownChartContainerElement = createChartContainer(overviewElement, WI.UIString("Main Thread"), WI.UIString("Breakdown of time spent on the main thread"));
        this._breakdownChart = new WI.CircleChart({size: 120, innerRadiusRatio: 0.5});
        this._breakdownChart.segments = Object.values(WI.CPUTimelineView.SampleType);
        this.addSubview(this._breakdownChart);
        breakdownChartContainerElement.appendChild(this._breakdownChart.element);

        this._breakdownLegendElement = breakdownChartContainerElement.appendChild(document.createElement("div"));
        this._breakdownLegendElement.classList.add("legend");

        this._breakdownLegendScriptElement = appendLegendRow(this._breakdownLegendElement, WI.CPUTimelineView.SampleType.Script);
        this._breakdownLegendLayoutElement = appendLegendRow(this._breakdownLegendElement, WI.CPUTimelineView.SampleType.Layout);
        this._breakdownLegendPaintElement = appendLegendRow(this._breakdownLegendElement, WI.CPUTimelineView.SampleType.Paint);
        this._breakdownLegendStyleElement = appendLegendRow(this._breakdownLegendElement, WI.CPUTimelineView.SampleType.Style);

        let detailsContainerElement = this._detailsContainerElement = contentElement.appendChild(document.createElement("div"));
        detailsContainerElement.classList.add("details");

        this._timelineRuler = new WI.TimelineRuler;
        this._timelineRuler.zeroTime = this.zeroTime;
        this._timelineRuler.startTime = this.startTime;
        this._timelineRuler.endTime = this.endTime;

        this.addSubview(this._timelineRuler);
        detailsContainerElement.appendChild(this._timelineRuler.element);

        // Cause the TimelineRuler to layout now so we will have some of its
        // important properties initialized for our layout.
        this._timelineRuler.updateLayout(WI.View.LayoutReason.Resize);

        let detailsSubtitleElement = detailsContainerElement.appendChild(document.createElement("div"));
        detailsSubtitleElement.classList.add("subtitle");
        detailsSubtitleElement.textContent = WI.UIString("CPU Usage");

        this._cpuUsageView = new WI.CPUUsageStackedView(WI.UIString("Total"));
        this.addSubview(this._cpuUsageView);
        this._detailsContainerElement.appendChild(this._cpuUsageView.element);

        this._mainThreadWorkIndicatorView = new WI.CPUUsageIndicatorView;
        this.addSubview(this._mainThreadWorkIndicatorView);
        this._detailsContainerElement.appendChild(this._mainThreadWorkIndicatorView.element);

        this._mainThreadWorkIndicatorView.chart.element.addEventListener("click", this._handleIndicatorClick.bind(this));

        let threadsSubtitleElement = detailsContainerElement.appendChild(document.createElement("div"));
        threadsSubtitleElement.classList.add("subtitle", "threads");
        threadsSubtitleElement.textContent = WI.UIString("Threads");

        this._mainThreadUsageView = new WI.CPUUsageView(WI.UIString("Main Thread"));
        this._mainThreadUsageView.element.classList.add("main-thread");
        this.addSubview(this._mainThreadUsageView);
        this._detailsContainerElement.appendChild(this._mainThreadUsageView.element);

        this._webkitThreadUsageView = new WI.CPUUsageView(WI.UIString("WebKit Threads"));
        this._webkitThreadUsageView.element.classList.add("non-main-thread");
        this.addSubview(this._webkitThreadUsageView);
        this._detailsContainerElement.appendChild(this._webkitThreadUsageView.element);

        this._unknownThreadUsageView = new WI.CPUUsageView(WI.UIString("Other Threads"));
        this._unknownThreadUsageView.element.classList.add("non-main-thread");
        this.addSubview(this._unknownThreadUsageView);
        this._detailsContainerElement.appendChild(this._unknownThreadUsageView.element);

        this._workerViews = [];
    }

    layout()
    {
        if (this.layoutReason === WI.View.LayoutReason.Resize)
            return;

        // Always update timeline ruler.
        this._timelineRuler.zeroTime = this.zeroTime;
        this._timelineRuler.startTime = this.startTime;
        this._timelineRuler.endTime = this.endTime;

        let secondsPerPixel = this._timelineRuler.secondsPerPixel;
        if (!secondsPerPixel)
            return;

        let graphStartTime = this.startTime;
        let graphEndTime = this.endTime;
        let visibleEndTime = Math.min(this.endTime, this.currentTime);

        let discontinuities = this._recording.discontinuitiesInTimeRange(graphStartTime, visibleEndTime);
        let originalDiscontinuities = discontinuities.slice();

        // Don't include the record before the graph start if the graph start is within a gap.
        let includeRecordBeforeStart = !discontinuities.length || discontinuities[0].startTime > graphStartTime;
        let visibleRecords = this.representedObject.recordsInTimeRange(graphStartTime, visibleEndTime, includeRecordBeforeStart);
        if (!visibleRecords.length || (visibleRecords.length === 1 && visibleRecords[0].endTime < graphStartTime)) {
            this.clear();
            return;
        }

        let samplingData = this._computeSamplingData(graphStartTime, visibleEndTime);
        let nonIdleSamplesCount = samplingData.samples.length - samplingData.samplesIdle;
        if (!nonIdleSamplesCount) {
            this._breakdownChart.clear();
            this._breakdownChart.needsLayout();
            this._clearBreakdownLegend();
        } else {
            let percentScript = samplingData.samplesScript / nonIdleSamplesCount;
            let percentLayout = samplingData.samplesLayout / nonIdleSamplesCount;
            let percentPaint = samplingData.samplesPaint / nonIdleSamplesCount;
            let percentStyle = samplingData.samplesStyle / nonIdleSamplesCount;

            this._breakdownLegendScriptElement.textContent = `${Number.percentageString(percentScript)} (${samplingData.samplesScript})`;
            this._breakdownLegendLayoutElement.textContent = `${Number.percentageString(percentLayout)} (${samplingData.samplesLayout})`;
            this._breakdownLegendPaintElement.textContent = `${Number.percentageString(percentPaint)} (${samplingData.samplesPaint})`;
            this._breakdownLegendStyleElement.textContent = `${Number.percentageString(percentStyle)} (${samplingData.samplesStyle})`;

            this._breakdownChart.values = [percentScript * 100, percentLayout * 100, percentPaint * 100, percentStyle * 100];
            this._breakdownChart.needsLayout();

            let centerElement = this._breakdownChart.centerElement;
            let samplesElement = centerElement.firstChild;
            if (!samplesElement) {
                samplesElement = centerElement.appendChild(document.createElement("div"));
                samplesElement.classList.add("samples");
                samplesElement.title = WI.UIString("Time spent on the main thread");
            }

            let millisecondsStringNoDecimal = WI.UIString("%.0fms").format(nonIdleSamplesCount);
            samplesElement.textContent = millisecondsStringNoDecimal;
        }

        let dataPoints = [];
        let workersDataMap = new Map;

        let max = -Infinity;
        let mainThreadMax = -Infinity;
        let webkitThreadMax = -Infinity;
        let unknownThreadMax = -Infinity;

        let min = Infinity;
        let mainThreadMin = Infinity;
        let webkitThreadMin = Infinity;
        let unknownThreadMin = Infinity;

        let average = 0;
        let mainThreadAverage = 0;
        let webkitThreadAverage = 0;
        let unknownThreadAverage = 0;

        for (let record of visibleRecords) {
            let time = record.startTime;
            let {usage, mainThreadUsage, workerThreadUsage, webkitThreadUsage, unknownThreadUsage} = record;

            if (discontinuities.length && discontinuities[0].endTime < time) {
                let startDiscontinuity = discontinuities.shift();
                let endDiscontinuity = startDiscontinuity;
                while (discontinuities.length && discontinuities[0].endTime < time)
                    endDiscontinuity = discontinuities.shift();

                if (dataPoints.length) {
                    let previousDataPoint = dataPoints.lastValue;
                    dataPoints.push({
                        time: startDiscontinuity.startTime,
                        mainThreadUsage: previousDataPoint.mainThreadUsage,
                        workerThreadUsage: previousDataPoint.workerThreadUsage,
                        webkitThreadUsage: previousDataPoint.webkitThreadUsage,
                        unknownThreadUsage: previousDataPoint.unknownThreadUsage,
                        usage: previousDataPoint.usage,
                    });
                }

                dataPoints.push({time: startDiscontinuity.startTime, mainThreadUsage: 0, workerThreadUsage: 0, webkitThreadUsage: 0, unknownThreadUsage: 0, usage: 0});
                dataPoints.push({time: endDiscontinuity.endTime, mainThreadUsage: 0, workerThreadUsage: 0, webkitThreadUsage: 0, unknownThreadUsage: 0, usage: 0});
                dataPoints.push({time: endDiscontinuity.endTime, mainThreadUsage, workerThreadUsage, webkitThreadUsage, unknownThreadUsage, usage});
            }

            dataPoints.push({time, mainThreadUsage, workerThreadUsage, webkitThreadUsage, unknownThreadUsage, usage});

            max = Math.max(max, usage);
            mainThreadMax = Math.max(mainThreadMax, mainThreadUsage);
            webkitThreadMax = Math.max(webkitThreadMax, webkitThreadUsage);
            unknownThreadMax = Math.max(unknownThreadMax, unknownThreadUsage);

            min = Math.min(min, usage);
            mainThreadMin = Math.min(mainThreadMin, mainThreadUsage);
            webkitThreadMin = Math.min(webkitThreadMin, webkitThreadUsage);
            unknownThreadMin = Math.min(unknownThreadMin, unknownThreadUsage);

            average += usage;
            mainThreadAverage += mainThreadUsage;
            webkitThreadAverage += webkitThreadUsage;
            unknownThreadAverage += unknownThreadUsage;

            if (record.workersData && record.workersData.length) {
                for (let {targetId, usage} of record.workersData) {
                    let workerData = workersDataMap.get(targetId);
                    if (!workerData) {
                        workerData = {
                            discontinuities: originalDiscontinuities.slice(),
                            recordsCount: 0,
                            dataPoints: [],
                            min: Infinity,
                            max: -Infinity,
                            average: 0
                        };

                        while (workerData.discontinuities.length && workerData.discontinuities[0].endTime <= graphStartTime)
                            workerData.discontinuities.shift();
                        workerData.dataPoints.push({time: graphStartTime, usage: 0});
                        workerData.dataPoints.push({time, usage: 0});
                        workersDataMap.set(targetId, workerData);
                    }

                    if (workerData.discontinuities.length && workerData.discontinuities[0].endTime < time) {
                        let startDiscontinuity = workerData.discontinuities.shift();
                        let endDiscontinuity = startDiscontinuity;
                        while (workerData.discontinuities.length && workerData.discontinuities[0].endTime < time)
                            endDiscontinuity = workerData.discontinuities.shift();
                        if (workerData.dataPoints.length) {
                            let previousDataPoint = workerData.dataPoints.lastValue;
                            workerData.dataPoints.push({time: startDiscontinuity.startTime, usage: previousDataPoint.usage});
                        }
                        workerData.dataPoints.push({time: startDiscontinuity.startTime, usage: 0});
                        workerData.dataPoints.push({time: endDiscontinuity.endTime, usage: 0});
                        workerData.dataPoints.push({time: endDiscontinuity.endTime, usage});
                    }

                    workerData.dataPoints.push({time, usage});
                    workerData.recordsCount += 1;
                    workerData.max = Math.max(workerData.max, usage);
                    workerData.min = Math.min(workerData.min, usage);
                    workerData.average += usage;
                }
            }
        }

        average /= visibleRecords.length;
        mainThreadAverage /= visibleRecords.length;
        webkitThreadAverage /= visibleRecords.length;
        unknownThreadAverage /= visibleRecords.length;

        for (let [workerId, workerData] of workersDataMap)
            workerData.average = workerData.average / workerData.recordsCount;

        // If the graph end time is inside a gap, the last data point should
        // only be extended to the start of the discontinuity.
        if (discontinuities.length)
            visibleEndTime = discontinuities[0].startTime;

        function removeGreaterThan(arr, max) {
            return arr.filter((x) => x <= max);
        }

        function markerValuesForMaxValue(max) {
            if (max < 1)
                return [0.5];
            if (max < 7)
                return removeGreaterThan([1, 3, 5], max);
            if (max < 12.5)
                return removeGreaterThan([5, 10], max);
            if (max < 20)
                return removeGreaterThan([5, 10, 15], max);
            if (max < 30)
                return removeGreaterThan([10, 20, 30], max);
            if (max < 50)
                return removeGreaterThan([15, 30, 45], max);
            if (max < 100)
                return removeGreaterThan([25, 50, 75], max);
            if (max < 200)
                return removeGreaterThan([50, 100, 150], max);
            if (max >= 200) {
                let hundreds = Math.floor(max / 100);
                let even = (hundreds % 2) === 0;
                if (even) {
                    let top = hundreds * 100;
                    let bottom = top / 2;
                    return [bottom, top];
                }
                let top = hundreds * 100;
                let bottom = 100;
                let mid = (top + bottom) / 2;
                return [bottom, mid, top];
            }
        }

        // Layout all graphs to the same time scale. The maximum value is
        // the maximum total CPU usage across all threads.
        let layoutMax = max;

        function layoutView(view, property, graphHeight, {dataPoints, min, max, average}) {
            if (min === Infinity)
                min = 0;
            if (max === -Infinity)
                max = 0;
            if (layoutMax === -Infinity)
                layoutMax = 0;

            let isAllThreadsGraph = property === null;

            let graphMax = layoutMax * 1.05;

            function xScale(time) {
                return (time - graphStartTime) / secondsPerPixel;
            }

            let size = new WI.Size(xScale(graphEndTime), graphHeight);

            function yScale(value) {
                return size.height - ((value / graphMax) * size.height);
            }

            view.updateChart(dataPoints, size, visibleEndTime, min, max, average, xScale, yScale, property);

            let markersElement = view.chart.element.querySelector(".markers");
            if (!markersElement) {
                markersElement = view.chart.element.appendChild(document.createElement("div"));
                markersElement.className = "markers";
            }
            markersElement.removeChildren();

            let markerValues;
            if (isAllThreadsGraph)
                markerValues = markerValuesForMaxValue(max);
            else {
                const minimumMarkerTextHeight = 17;
                let percentPerPixel = 1 / (graphHeight / layoutMax);
                if (layoutMax < 5) {
                    let minimumDisplayablePercentByTwo = Math.ceil((minimumMarkerTextHeight * percentPerPixel) / 2) * 2;
                    markerValues = [Math.max(minimumDisplayablePercentByTwo, Math.floor(max))];
                } else {
                    let minimumDisplayablePercentByFive = Math.ceil((minimumMarkerTextHeight * percentPerPixel) / 5) * 5;
                    markerValues = [Math.max(minimumDisplayablePercentByFive, Math.floor(max))];
                }
            }

            for (let value of markerValues) {
                let marginTop = yScale(value);

                let markerElement = markersElement.appendChild(document.createElement("div"));
                markerElement.style.marginTop = marginTop.toFixed(2) + "px";

                let labelElement = markerElement.appendChild(document.createElement("span"));
                labelElement.classList.add("label");
                const precision = 0;
                labelElement.innerText = Number.percentageString(value / 100, precision);
            }
        }

        layoutView(this._cpuUsageView, null, CPUTimelineView.cpuUsageViewHeight, {dataPoints, min, max, average});
        layoutView(this._mainThreadUsageView, "mainThreadUsage", CPUTimelineView.threadCPUUsageViewHeight, {dataPoints, min: mainThreadMin, max: mainThreadMax, average: mainThreadAverage});
        layoutView(this._webkitThreadUsageView, "webkitThreadUsage", CPUTimelineView.threadCPUUsageViewHeight, {dataPoints, min: webkitThreadMin, max: webkitThreadMax, average: webkitThreadAverage});
        layoutView(this._unknownThreadUsageView, "unknownThreadUsage", CPUTimelineView.threadCPUUsageViewHeight, {dataPoints, min: unknownThreadMin, max: unknownThreadMax, average: unknownThreadAverage});

        this._removeWorkerThreadViews();

        for (let [workerId, workerData] of workersDataMap) {
            let worker = WI.targetManager.targetForIdentifier(workerId);
            let displayName = worker ? worker.displayName : WI.UIString("Worker Thread");
            let workerView = new WI.CPUUsageView(displayName);
            workerView.element.classList.add("worker-thread");
            this.addSubview(workerView);
            this._detailsContainerElement.insertBefore(workerView.element, this._webkitThreadUsageView.element);
            this._workerViews.push(workerView);

            layoutView(workerView, "usage", CPUTimelineView.threadCPUUsageViewHeight, {dataPoints: workerData.dataPoints, min: workerData.min, max: workerData.max, average: workerData.average});
        }

        function xScaleIndicatorRange(sampleIndex) {
            return (sampleIndex / 1000) / secondsPerPixel;
        }

        let graphWidth = (graphEndTime - graphStartTime) / secondsPerPixel;
        let size = new WI.Size(graphWidth, CPUTimelineView.indicatorViewHeight);
        this._mainThreadWorkIndicatorView.updateChart(samplingData.samples, size, visibleEndTime, xScaleIndicatorRange);
    }

    // Private

    _computeSamplingData(startTime, endTime)
    {
        // Compute per-millisecond samples of what the main thread was doing.
        // We construct an array for every millisecond between the start and end time
        // and mark each millisecond with the best representation of the work that
        // was being done at that time. We start by populating the samples with
        // all of the script periods and then override with layout and rendering
        // samples. This means a forced layout would be counted as a layout:
        //
        // Initial:        [ ------, ------, ------, ------, ------ ]
        // Script Samples: [ ------, Script, Script, Script, ------ ]
        // Layout Samples: [ ------, Script, Layout, Script, ------ ]
        //
        // The undefined samples are considered Idle, but in actuality WebKit
        // may have been doing some work (such as hit testing / inspector protocol)
        // that is not included it in generic Timeline data. This just works with
        // with the data available to the frontend and is quite accurate for most
        // Main Thread activity.

        const includeRecordBeforeStart = true;

        let scriptTimeline = this._recording.timelineForRecordType(WI.TimelineRecord.Type.Script);
        let scriptRecords = scriptTimeline ? scriptTimeline.recordsInTimeRange(startTime, endTime, includeRecordBeforeStart) : [];
        scriptRecords = scriptRecords.filter((record) => {
            switch (record.eventType) {
            case WI.ScriptTimelineRecord.EventType.ScriptEvaluated:
            case WI.ScriptTimelineRecord.EventType.APIScriptEvaluated:
            case WI.ScriptTimelineRecord.EventType.ObserverCallback:
            case WI.ScriptTimelineRecord.EventType.EventDispatched:
            case WI.ScriptTimelineRecord.EventType.MicrotaskDispatched:
            case WI.ScriptTimelineRecord.EventType.TimerFired:
            case WI.ScriptTimelineRecord.EventType.AnimationFrameFired:
                // These event types define script entry/exits.
                return true;

            case WI.ScriptTimelineRecord.EventType.AnimationFrameRequested:
            case WI.ScriptTimelineRecord.EventType.AnimationFrameCanceled:
            case WI.ScriptTimelineRecord.EventType.TimerInstalled:
            case WI.ScriptTimelineRecord.EventType.TimerRemoved:
            case WI.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
            case WI.ScriptTimelineRecord.EventType.ConsoleProfileRecorded:
            case WI.ScriptTimelineRecord.EventType.GarbageCollected:
                // These event types have no time range, or are contained by the others.
                return false;

            default:
                console.error("Unhandled ScriptTimelineRecord.EventType", record.eventType);
                return false;
            }
        });

        let layoutTimeline = this._recording.timelineForRecordType(WI.TimelineRecord.Type.Layout);
        let layoutRecords = layoutTimeline ? layoutTimeline.recordsInTimeRange(startTime, endTime, includeRecordBeforeStart) : [];
        layoutRecords = layoutRecords.filter((record) => {
            switch (record.eventType) {
            case WI.LayoutTimelineRecord.EventType.RecalculateStyles:
            case WI.LayoutTimelineRecord.EventType.ForcedLayout:
            case WI.LayoutTimelineRecord.EventType.Layout:
            case WI.LayoutTimelineRecord.EventType.Paint:
            case WI.LayoutTimelineRecord.EventType.Composite:
                // These event types define layout and rendering entry/exits.
                return true;

            case WI.LayoutTimelineRecord.EventType.InvalidateStyles:
            case WI.LayoutTimelineRecord.EventType.InvalidateLayout:
                // These event types have no time range.
                return false;

            default:
                console.error("Unhandled LayoutTimelineRecord.EventType", record.eventType);
                return false;
            }
        });

        let millisecondStartTime = Math.round(startTime * 1000);
        let millisecondEndTime = Math.round(endTime * 1000);
        let millisecondDuration = millisecondEndTime - millisecondStartTime;

        let samples = new Array(millisecondDuration);

        function markRecordEntries(records, callback) {
            for (let record of records) {
                let recordStart = Math.round(record.startTime * 1000);
                let recordEnd = Math.round(record.endTime * 1000);
                if (recordStart > millisecondEndTime)
                    continue;
                if (recordEnd < millisecondStartTime)
                    continue;

                let offset = recordStart - millisecondStartTime;
                recordStart = Math.max(recordStart, millisecondStartTime);
                recordEnd = Math.min(recordEnd, millisecondEndTime);

                let value = callback(record);
                for (let t = recordStart; t <= recordEnd; ++t)
                    samples[t - millisecondStartTime] = value;
            }
        }

        markRecordEntries(scriptRecords, (record) => {
            return WI.CPUTimelineView.SampleType.Script;
        });

        markRecordEntries(layoutRecords, (record) => {
            switch (record.eventType) {
            case WI.LayoutTimelineRecord.EventType.RecalculateStyles:
                return WI.CPUTimelineView.SampleType.Style;
            case WI.LayoutTimelineRecord.EventType.ForcedLayout:
            case WI.LayoutTimelineRecord.EventType.Layout:
                return WI.CPUTimelineView.SampleType.Layout;
            case WI.LayoutTimelineRecord.EventType.Paint:
            case WI.LayoutTimelineRecord.EventType.Composite:
                return WI.CPUTimelineView.SampleType.Paint;
            }
        });

        let samplesIdle = 0;
        let samplesScript = 0;
        let samplesLayout = 0;
        let samplesPaint = 0;
        let samplesStyle = 0;
        for (let i = 0; i < samples.length; ++i) {
            switch (samples[i]) {
            case undefined:
                samplesIdle++;
                break;
            case WI.CPUTimelineView.SampleType.Script:
                samplesScript++;
                break;
            case WI.CPUTimelineView.SampleType.Layout:
                samplesLayout++;
                break;
            case WI.CPUTimelineView.SampleType.Paint:
                samplesPaint++;
                break;
            case WI.CPUTimelineView.SampleType.Style:
                samplesStyle++;
                break;
            }
        }

        return {
            samples,
            samplesIdle,
            samplesScript,
            samplesLayout,
            samplesPaint,
            samplesStyle,
        };
    }

    _removeWorkerThreadViews()
    {
        if (!this._workerViews.length)
            return;

        for (let view of this._workerViews)
            this.removeSubview(view);

        this._workerViews = [];
    }

    _clearBreakdownLegend()
    {
        this._breakdownLegendScriptElement.textContent = emDash;
        this._breakdownLegendLayoutElement.textContent = emDash;
        this._breakdownLegendPaintElement.textContent = emDash;
        this._breakdownLegendStyleElement.textContent = emDash;

        this._breakdownChart.centerElement.removeChildren();
    }

    _cpuTimelineRecordAdded(event)
    {
        let cpuTimelineRecord = event.data.record;
        console.assert(cpuTimelineRecord instanceof WI.CPUTimelineRecord);

        if (cpuTimelineRecord.startTime >= this.startTime && cpuTimelineRecord.endTime <= this.endTime)
            this.needsLayout();
    }

    _graphPositionForMouseEvent(event)
    {
        let svgElement = event.target.enclosingNodeOrSelfWithNodeName("svg");
        if (!svgElement)
            return NaN;

        let svgRect = svgElement.getBoundingClientRect();
        let position = event.pageX - svgRect.left;

        if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL)
            return svgRect.width - position;
        return position;
    }

    _handleIndicatorClick(event)
    {
        let clickPosition = this._graphPositionForMouseEvent(event);
        if (isNaN(clickPosition))
            return;

        let secondsPerPixel = this._timelineRuler.secondsPerPixel;
        let graphClickTime = clickPosition * secondsPerPixel;
        let graphStartTime = this.startTime;

        let clickStartTime = graphStartTime + graphClickTime;
        let clickEndTime = clickStartTime + secondsPerPixel;

        // Try at the exact clicked pixel.
        if (event.target.localName === "rect") {
            if (this._attemptSelectIndicatatorTimelineRecord(clickStartTime, clickEndTime))
                return;
            console.assert(false, "If the user clicked on a rect there should have been a record in this pixel range");
        }

        // Spiral out 4 pixels each side to try and select a nearby record.
        for (let i = 1, delta = 0; i <= 4; ++i) {
            delta += secondsPerPixel;
            if (this._attemptSelectIndicatatorTimelineRecord(clickStartTime - delta, clickStartTime))
                return;
            if (this._attemptSelectIndicatatorTimelineRecord(clickEndTime, clickEndTime + delta))
                return;
        }
    }

    _attemptSelectIndicatatorTimelineRecord(startTime, endTime)
    {
        let layoutTimeline = this._recording.timelineForRecordType(WI.TimelineRecord.Type.Layout);
        let layoutRecords = layoutTimeline ? layoutTimeline.recordsOverlappingTimeRange(startTime, endTime) : [];
        layoutRecords = layoutRecords.filter((record) => {
            switch (record.eventType) {
            case WI.LayoutTimelineRecord.EventType.RecalculateStyles:
            case WI.LayoutTimelineRecord.EventType.ForcedLayout:
            case WI.LayoutTimelineRecord.EventType.Layout:
            case WI.LayoutTimelineRecord.EventType.Paint:
            case WI.LayoutTimelineRecord.EventType.Composite:
                return true;
            case WI.LayoutTimelineRecord.EventType.InvalidateStyles:
            case WI.LayoutTimelineRecord.EventType.InvalidateLayout:
                return false;
            default:
                console.error("Unhandled LayoutTimelineRecord.EventType", record.eventType);
                return false;
            }
        });

        if (layoutRecords.length) {
            this._selectTimelineRecord(layoutRecords[0]);
            return true;
        }

        let scriptTimeline = this._recording.timelineForRecordType(WI.TimelineRecord.Type.Script);
        let scriptRecords = scriptTimeline ? scriptTimeline.recordsOverlappingTimeRange(startTime, endTime) : [];
        scriptRecords = scriptRecords.filter((record) => {
            switch (record.eventType) {
            case WI.ScriptTimelineRecord.EventType.ScriptEvaluated:
            case WI.ScriptTimelineRecord.EventType.APIScriptEvaluated:
            case WI.ScriptTimelineRecord.EventType.ObserverCallback:
            case WI.ScriptTimelineRecord.EventType.EventDispatched:
            case WI.ScriptTimelineRecord.EventType.MicrotaskDispatched:
            case WI.ScriptTimelineRecord.EventType.TimerFired:
            case WI.ScriptTimelineRecord.EventType.AnimationFrameFired:
                return true;
            case WI.ScriptTimelineRecord.EventType.AnimationFrameRequested:
            case WI.ScriptTimelineRecord.EventType.AnimationFrameCanceled:
            case WI.ScriptTimelineRecord.EventType.TimerInstalled:
            case WI.ScriptTimelineRecord.EventType.TimerRemoved:
            case WI.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
            case WI.ScriptTimelineRecord.EventType.ConsoleProfileRecorded:
            case WI.ScriptTimelineRecord.EventType.GarbageCollected:
                return false;
            default:
                console.error("Unhandled ScriptTimelineRecord.EventType", record.eventType);
                return false;
            }
        });

        if (scriptRecords.length) {
            this._selectTimelineRecord(scriptRecords[0]);
            return true;
        }

        return false;
    }

    _selectTimelineRecord(record)
    {
        this.dispatchEventToListeners(WI.TimelineView.Event.RecordWasSelected, {record});
    }
};

// NOTE: UI follows this order.
WI.CPUTimelineView.SampleType = {
    Script: "sample-type-script",
    Layout: "sample-type-layout",
    Paint: "sample-type-paint",
    Style: "sample-type-style",
};
