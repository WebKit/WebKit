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

// StackedAreaChart creates a area chart stratified with sections.
//
// Initialize the chart with a size.
// To populate with data, first initialize the sections. The class names you
// provide for the segments will allow you to style them. You can then include
// a new set of (x, [y1, y2, y3]) points in the chart via `addPointSet`. The
// order of `y` values must be in the same order as the sections.
//
// SVG:
//
// - There is a single path for each stratification.
// - Each path extends all the way down to the bottom, they are layered such
//   that the paths for lower sections overlap the paths for upper sections.
//
//  <div class="stacked-area-chart">
//      <svg viewBox="0 0 1000 100">
//          <path class="section-class-name-2" d="..."/>
//          <path class="section-class-name-1" d="..."/>
//      </svg>
//  </div>

WI.StackedAreaChart = class StackedAreaChart extends WI.View
{
    constructor()
    {
        super();

        this.element.classList.add("stacked-area-chart");

        this._chartElement = this.element.appendChild(createSVGElement("svg"));
        this._chartElement.setAttribute("preserveAspectRatio", "none");

        this._pathElements = [];

        this._points = [];
        this._size = null;
    }

    // Public

    get size()
    {
        return this._size;
    }

    set size(size)
    {
        if (this._size && this._size.equals(size))
            return;

        this._size = size;

        this._chartElement.setAttribute("viewBox", `0 0 ${size.width} ${size.height}`);

        this.needsLayout();
    }

    initializeSections(sectionClassNames)
    {
        console.assert(!this._pathElements.length, "Should not initialize multiple times");

        sectionClassNames.reverse();

        for (let i = 0; i < sectionClassNames.length; ++i) {
            let pathElement = this._chartElement.appendChild(createSVGElement("path"));
            pathElement.classList.add(sectionClassNames[i]);
            this._pathElements.push(pathElement);
        }

        this._pathElements.reverse();
    }

    addPointSet(x, ys)
    {
        console.assert(ys.length === this._pathElements.length);
        this._points.push({x, ys});
    }

    clear()
    {
        this._points = [];
    }

    // Protected

    layout()
    {
        super.layout();

        if (this.layoutReason === WI.View.LayoutReason.Resize)
            return;

        if (!this._size)
            return;

        let pathComponents = [];
        let length = this._pathElements.length;

        for (let i = 0; i < length; ++i)
            pathComponents[i] = [`M 0 ${this._size.height}`];

        for (let {x, ys} of this._points) {
            for (let i = 0; i < length; ++i)
                pathComponents[i].push(`L ${x} ${ys[i]}`);
        }

        let lastX = this._points.length ? this._points.lastValue.x : 0;
        for (let i = 0; i < length; ++i) {
            pathComponents[i].push(`L ${lastX} ${this._size.height}`);
            pathComponents[i].push("Z");
            let pathString = pathComponents[i].join(" ");
            this._pathElements[i].setAttribute("d", pathString);
        }
    }
};
