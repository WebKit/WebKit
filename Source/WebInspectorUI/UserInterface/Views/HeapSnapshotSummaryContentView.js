/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.HeapSnapshotSummaryContentView = class HeapSnapshotSummaryContentView extends WebInspector.ContentView
{
    constructor(heapSnapshot, extraArguments)
    {
        console.assert(heapSnapshot instanceof WebInspector.HeapSnapshotProxy || heapSnapshot instanceof WebInspector.HeapSnapshotDiffProxy);

        super(heapSnapshot);

        this._heapSnapshot = heapSnapshot;

        // FIXME: Show/hide internal objects.
        let {showInternalObjects} = extraArguments;

        this.element.classList.add("heap-snapshot-summary");

        let contentElement = this.element.appendChild(document.createElement("div"));
        contentElement.classList.add("content");

        let overviewElement = contentElement.appendChild(document.createElement("div"));
        overviewElement.classList.add("overview");

        function createChartContainer(parentElement, subtitle, tooltip)
        {
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

        const circleChartConfig = {size: 120, innerRadiusRatio: 0.5};

        let classSizeBreakdownTooltip = WebInspector.UIString("Breakdown of total allocation size by class");
        let classSizeBreakdownChartContainerElement = createChartContainer(overviewElement, WebInspector.UIString("Class Size Breakdown"), classSizeBreakdownTooltip);
        this._classSizeBreakdownCircleChart = new WebInspector.CircleChart(circleChartConfig);
        classSizeBreakdownChartContainerElement.appendChild(this._classSizeBreakdownCircleChart.element);
        this._classSizeBreakdownLegendElement = classSizeBreakdownChartContainerElement.appendChild(document.createElement("div"));
        this._classSizeBreakdownLegendElement.classList.add("legend", "class-breakdown");

        let secondOverviewElement = contentElement.appendChild(document.createElement("div"));
        secondOverviewElement.classList.add("overview");

        let classCountBreakdownTooltip = WebInspector.UIString("Breakdown of object counts by class");
        let classCountBreakdownChartContainerElement = createChartContainer(secondOverviewElement, WebInspector.UIString("Class Count Breakdown"), classCountBreakdownTooltip);
        this._classCountBreakdownCircleChart = new WebInspector.CircleChart(circleChartConfig);
        classCountBreakdownChartContainerElement.appendChild(this._classCountBreakdownCircleChart.element);
        this._classCountBreakdownLegendElement = classCountBreakdownChartContainerElement.appendChild(document.createElement("div"));
        this._classCountBreakdownLegendElement.classList.add("legend", "count-breakdown");

        let thirdOverviewElement = contentElement.appendChild(document.createElement("div"));
        thirdOverviewElement.classList.add("overview");

        let allocationSizeBreakdownTooltip = WebInspector.UIString("Breakdown of allocations into size classes");
        let allocationSizeBreakdownChartContainerElement = createChartContainer(thirdOverviewElement, WebInspector.UIString("Allocation Size Breakdown"), allocationSizeBreakdownTooltip);
        this._allocationSizeBreakdownCircleChart = new WebInspector.CircleChart(circleChartConfig);
        allocationSizeBreakdownChartContainerElement.appendChild(this._allocationSizeBreakdownCircleChart.element);
        this._allocationSizeBreakdownLegendElement = allocationSizeBreakdownChartContainerElement.appendChild(document.createElement("div"));
        this._allocationSizeBreakdownLegendElement.classList.add("legend", "allocation-size-breakdown");

        function appendLegendRow(legendElement, swatchClass, label, subtitle, tooltip) {
            let rowElement = legendElement.appendChild(document.createElement("div"));
            rowElement.classList.add("row");
            let swatchElement = rowElement.appendChild(document.createElement("div"));
            swatchElement.classList.add("swatch", swatchClass);
            let labelElement = rowElement.appendChild(document.createElement("p"));
            labelElement.classList.add("label");
            labelElement.textContent = label;
            let sizeElement = rowElement.appendChild(document.createElement("p"));
            sizeElement.classList.add("size");
            sizeElement.textContent = subtitle;
            if (tooltip)
                rowElement.title = tooltip;
        }

        function appendEmptyMessage(legendElement, text) {
            let emptyElement = legendElement.appendChild(document.createElement("div"));
            emptyElement.classList.add("empty");
            emptyElement.textContent = text;
        }

        // Largest classes by size.
        let categoriesBySize = [...this._heapSnapshot.categories.values()].sort((a, b) => b.size - a.size);
        let categoriesBySizeLimit = Math.min(categoriesBySize.length, 5);
        if (categoriesBySizeLimit) {
            let topClassSizes = categoriesBySize.splice(0, categoriesBySizeLimit);
            let otherClassSize = categoriesBySize.reduce((sum, category) => sum += category.size, 0);

            let segments = [];
            let values = [];

            for (let i = 0; i < categoriesBySizeLimit; ++i) {
                let topClassSize = topClassSizes[i];
                let segmentName = "top-class-" + (i + 1);
                segments.push(segmentName);
                values.push(topClassSize.size);
                appendLegendRow.call(this, this._classSizeBreakdownLegendElement, segmentName, topClassSize.className, Number.bytesToString(topClassSize.size));
            }

            if (otherClassSize) {
                let otherSegmentName = "other";
                segments.push(otherSegmentName);
                values.push(otherClassSize);
                appendLegendRow.call(this, this._classSizeBreakdownLegendElement, otherSegmentName, WebInspector.UIString("Other"), Number.bytesToString(otherClassSize));
            }

            this._classSizeBreakdownCircleChart.segments = segments;
            this._classSizeBreakdownCircleChart.values = values;
            this._classSizeBreakdownCircleChart.updateLayout();

            let totalSizeElement = this._classSizeBreakdownCircleChart.centerElement.appendChild(document.createElement("div"));
            totalSizeElement.classList.add("total-size");
            totalSizeElement.appendChild(document.createElement("span")); // firstChild
            totalSizeElement.appendChild(document.createElement("br"));
            totalSizeElement.appendChild(document.createElement("span")); // lastChild
            let totalSize = Number.bytesToString(this._heapSnapshot.totalSize).split(/\s+/);
            totalSizeElement.firstChild.textContent = totalSize[0];
            totalSizeElement.lastChild.textContent = totalSize[1];
            totalSizeElement.title = WebInspector.UIString("Total size of heap");
        } else
            appendEmptyMessage.call(this, this._classSizeBreakdownLegendElement, WebInspector.UIString("No objects"));

        // Largest classes by count.
        let categoriesByCount = [...this._heapSnapshot.categories.values()].sort((a, b) => b.count - a.count);
        let categoriesByCountLimit = Math.min(categoriesByCount.length, 5);
        if (categoriesByCountLimit) {
            let topClassCounts = categoriesByCount.splice(0, categoriesByCountLimit);
            let otherClassCount = categoriesByCount.reduce((sum, category) => sum += category.count, 0);

            let segments = [];
            let values = [];

            for (let i = 0; i < categoriesByCountLimit; ++i) {
                let topClassCount = topClassCounts[i];
                let segmentName = "top-class-" + (i + 1);
                segments.push(segmentName);
                values.push(topClassCount.count);
                appendLegendRow.call(this, this._classCountBreakdownLegendElement, segmentName, topClassCount.className, topClassCount.count);
            }

            if (otherClassCount) {
                let otherSegmentName = "other";
                segments.push(otherSegmentName);
                values.push(otherClassCount);
                appendLegendRow.call(this, this._classCountBreakdownLegendElement, otherSegmentName, WebInspector.UIString("Other"), otherClassCount);
            }

            this._classCountBreakdownCircleChart.segments = segments;
            this._classCountBreakdownCircleChart.values = values;
            this._classCountBreakdownCircleChart.updateLayout();

            let totalCountElement = this._classCountBreakdownCircleChart.centerElement.appendChild(document.createElement("div"));
            totalCountElement.classList.add("total-count");
            totalCountElement.textContent = this._heapSnapshot.totalObjectCount;
            totalCountElement.title = WebInspector.UIString("Total object count");
        } else
            appendEmptyMessage.call(this, this._classCountBreakdownLegendElement, WebInspector.UIString("No objects"));

        // Allocation size groups.
        const smallAllocationSize = 48;
        const mediumAllocationSize = 128;
        const largeAllocationSize = 512;

        this._heapSnapshot.allocationBucketCounts([smallAllocationSize, mediumAllocationSize, largeAllocationSize], (results) => {
            let [small, medium, large, veryLarge] = results;

            if (small + medium + large + veryLarge) {
                appendLegendRow.call(this, this._allocationSizeBreakdownLegendElement, "small", WebInspector.UIString("Small %s").format(Number.bytesToString(smallAllocationSize)), small);
                appendLegendRow.call(this, this._allocationSizeBreakdownLegendElement, "medium", WebInspector.UIString("Medium %s").format(Number.bytesToString(mediumAllocationSize)), medium);
                appendLegendRow.call(this, this._allocationSizeBreakdownLegendElement, "large", WebInspector.UIString("Large %s").format(Number.bytesToString(largeAllocationSize)), large);
                appendLegendRow.call(this, this._allocationSizeBreakdownLegendElement, "very-large", WebInspector.UIString("Very Large"), veryLarge);

                this._allocationSizeBreakdownCircleChart.segments = ["small", "medium", "large", "very-large"];
                this._allocationSizeBreakdownCircleChart.values = [small, medium, large, veryLarge];
                this._allocationSizeBreakdownCircleChart.updateLayout();

                let averageAllocationSizeElement = this._allocationSizeBreakdownCircleChart.centerElement.appendChild(document.createElement("div"));
                averageAllocationSizeElement.classList.add("average-allocation-size");
                averageAllocationSizeElement.textContent = Number.bytesToString(this._heapSnapshot.totalSize / this._heapSnapshot.totalObjectCount);
                averageAllocationSizeElement.title = WebInspector.UIString("Average allocation size");
            } else
                appendEmptyMessage.call(this, this._allocationSizeBreakdownLegendElement, WebInspector.UIString("No objects"));
        });
    }

    // Protected

    layout()
    {
        // Nothing to change.
    }
};
