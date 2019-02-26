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

WI.CPUUsageIndicatorView = class CPUUsageIndicatorView extends WI.View
{
    constructor(delegate)
    {
        super();

        this.element.classList.add("cpu-usage-indicator-view");

        this._detailsElement = this.element.appendChild(document.createElement("div"));
        this._detailsElement.classList.add("details");

        this._graphElement = this.element.appendChild(document.createElement("div"));
        this._graphElement.classList.add("graph");

        this._chart = new WI.RangeChart;
        this.addSubview(this._chart);
        this._graphElement.appendChild(this._chart.element);
    }

    // Public

    get chart() { return this._chart; }

    clear()
    {
        this._chart.clear();
        this._chart.needsLayout();
    }

    updateChart(samples, size, visibleEndTime, xScale)
    {
        console.assert(size instanceof WI.Size);

        this._chart.clear();
        this._chart.size = size;
        this._chart.needsLayout();

        if (!samples.length)
            return;

        // Coalesce ranges of samples.
        let ranges = [];
        let currentRange = null;
        let currentSampleType = undefined;
        for (let i = 0; i < samples.length; ++i) {
            // Back to idle, close any current chunk.
            let type = samples[i];
            if (!type) {
                if (currentRange) {
                    ranges.push(currentRange);
                    currentRange = null;
                    currentSampleType = undefined;
                }
                continue;
            }

            // Expand existing chunk.
            if (type === currentSampleType) {
                currentRange.endIndex = i;
                continue;
            }

            // If type changed, close current chunk.
            if (currentSampleType) {
                ranges.push(currentRange);
                currentRange = null;
                currentSampleType = undefined;
            }

            // Start a new chunk.
            console.assert(!currentRange);
            console.assert(!currentSampleType);
            currentRange = {type, startIndex: i, endIndex: i};
            currentSampleType = type;
        }

        for (let {type, startIndex, endIndex} of ranges) {
            let startX = xScale(startIndex);
            let endX = xScale(endIndex + 1);
            let width = endX - startX;
            this._chart.addRange(startX, width, type);
        }
    }
};
