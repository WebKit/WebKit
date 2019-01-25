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

// ColumnChart creates a single filled line chart.
//
// Initialize the chart with a size. You can then include a new bar
// in the bar chart by providing an (x, y, w, h) via `addBar`.
//
// SVG:
//
// - There is a single rect for each bar.
//
//  <div class="line-chart">
//      <svg width="800" height="75" viewbox="0 0 800 75">
//          <rect width="<w>" height="<h>" transform="translate(<x>, <y>)" />
//          <rect width="<w>" height="<h>" transform="translate(<x>, <y>)" />
//      </svg>
//  </div>

WI.ColumnChart = class ColumnChart
{
    constructor(size)
    {
        this._element = document.createElement("div");
        this._element.classList.add("bar-chart");

        this._svgElement = this._element.appendChild(createSVGElement("svg"));

        this._bars = [];
        this.size = size;
    }

    // Public

    get element() { return this._element; }
    get bars() { return this._bars; }

    get size()
    {
        return this._size;
    }

    set size(size)
    {
        this._size = size;

        this._svgElement.setAttribute("width", size.width);
        this._svgElement.setAttribute("height", size.height);
        this._svgElement.setAttribute("viewbox", `0 0 ${size.width} ${size.height}`);
    }

    addBar(x, y, width, height)
    {
        this._bars.push({x, y, width, height});
    }

    clear()
    {
        this._bars = [];
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

        this._svgElement.removeChildren();

        for (let {x, y, width, height} of this._bars) {
            let rect = this._svgElement.appendChild(createSVGElement("rect"));
            rect.setAttribute("width", width);
            rect.setAttribute("height", height);
            rect.setAttribute("transform", `translate(${x}, ${y})`);
        }
    }
};
