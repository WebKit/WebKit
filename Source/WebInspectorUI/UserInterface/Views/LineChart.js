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

// LineChart creates a single filled line chart.
//
// Initialize the chart with a size. You can then include a new point
// in the line chart by providing an (x, y) point via `addPoint`.
//
// SVG:
//
// - There is a single path for line.
//
//  <div class="line-chart">
//      <svg width="800" height="75" viewbox="0 0 800 75">
//          <path d="..."/>
//      </svg>
//  </div>

WI.LineChart = class LineChart
{
    constructor(size)
    {
        this._element = document.createElement("div");
        this._element.classList.add("line-chart");

        this._chartElement = this._element.appendChild(createSVGElement("svg"));
        this._pathElement = this._chartElement.appendChild(createSVGElement("path"));

        this._points = [];
        this.size = size;
    }

    // Public

    get element() { return this._element; }
    get points() { return this._points; }

    get size()
    {
        return this._size;
    }

    set size(size)
    {
        this._size = size;

        this._chartElement.setAttribute("width", size.width);
        this._chartElement.setAttribute("height", size.height);
        this._chartElement.setAttribute("viewbox", `0 0 ${size.width} ${size.height}`);
    }

    addPoint(x, y)
    {
        this._points.push({x, y});
    }

    clear()
    {
        this._points = [];
    }

    needsLayout()
    {
        if (this._scheduledLayoutUpdateIdentifier)
            return;

        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(this.updateLayout.bind(this));
    }

    updateLayout()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            this._scheduledLayoutUpdateIdentifier = undefined;
        }

        let pathComponents = [];
        pathComponents.push(`M 0 ${this._size.height}`);
        for (let point of this._points)
            pathComponents.push(`L ${point.x} ${point.y}`);

        let lastX = this._points.length ? this._points.lastValue.x : 0;
        pathComponents.push(`L ${lastX} ${this._size.height}`);
        pathComponents.push("Z");

        let pathString = pathComponents.join(" ");
        this._pathElement.setAttribute("d", pathString);
    }
};
