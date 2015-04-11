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

WebInspector.ChartDetailsSectionRow = class ChartDetailsSectionRow extends WebInspector.DetailsSectionRow
{
    constructor(formatValueCallback)
    {
        super(WebInspector.UIString("No Chart Available"));

        this.element.classList.add("chart");

        this._legendGroup = new WebInspector.DetailsSectionGroup;
        this._formatValueCallback = formatValueCallback;
        this._chartItems = [];
        this._innerLabel = "";
        this._innerRadius = 0.5;
        this._innerLabelFontSize = 11;
        this._shadowColor = "rgba(0, 0, 0, 0.5)";
        this._total = 0;

        this._refresh();
    }

    // Public

    get legendGroup()
    {
        return this._legendGroup;
    }

    set innerLabel(x)
    {
        if (this._innerLabel === x)
            return;

        this._innerLabel = x;

        this._refresh();
    }

    addChartValue(label, value, color)
    {
        console.assert(value >= 0);
        if (value < 0)
            return;

        this._chartItems.push({label, value, color});
        this._total += value

        this._refresh();

        if (!this._legendGroup)
            return;

        var rows = this._legendGroup.rows;
        var formattedValue = this._formatValueCallback ? this._formatValueCallback(value) : value;
        rows.push(new WebInspector.ChartDetailsSectionLegendRow(label, formattedValue, color));
        this._legendGroup.rows = rows;
    }

    clearChart()
    {
        this._chartItems = [];
        this._total = 0;

        this._refresh();

        if (!this._legendGroup)
            return;

        this._legendGroup.rows = [];
    }

    // Private

    _refresh()
    {
        if (!this._chartItems.length) {
            this._canvas = null;
            this.showEmptyMessage();
            return;
        }

        if (!this._canvas) {
            this.hideEmptyMessage();

            this._canvas = document.createElement("canvas");
            this.element.appendChild(this._canvas);

            this._canvas.width = this._canvas.offsetWidth * window.devicePixelRatio;
            this._canvas.height = this._canvas.offsetHeight * window.devicePixelRatio;
        }

        var context = this._canvas.getContext("2d");
        context.clearRect(0, 0, this._canvas.width, this._canvas.height);

        var centerX = Math.floor(this._canvas.width / 2);
        var centerY = Math.floor(this._canvas.height / 2);
        var radius = Math.floor(Math.min(centerX, centerY) * 0.96);   // Add a small margin to prevent clipping of the chart shadow.
        var innerRadius = Math.floor(radius * this._innerRadius);
        var startAngle = 1.5 * Math.PI;
        var endAngle = startAngle;

        function drawSlice(x, y, startAngle, endAngle, color)
        {
            context.beginPath();
            context.moveTo(x, y);
            context.arc(x, y, radius, startAngle, endAngle, false);
            if (innerRadius > 0)
                context.arc(x, y, innerRadius, endAngle, startAngle, true);
            context.fillStyle = color;
            context.fill();
        }

        context.save();
        context.shadowBlur = 2 * window.devicePixelRatio;
        context.shadowOffsetY = window.devicePixelRatio;
        context.shadowColor = this._shadowColor;
        drawSlice(centerX, centerY, 0, 2.0 * Math.PI, "rgb(255, 255, 255)");
        context.restore();

        for (var item of this._chartItems) {
            endAngle += (item.value / this._total) * 2.0 * Math.PI;
            drawSlice(centerX, centerY, startAngle, endAngle, item.color);
            startAngle = endAngle;
        }

        if (this._innerLabel) {
            context.font = (this._innerLabelFontSize * window.devicePixelRatio) + "px sans-serif";
            var metrics = context.measureText(this._innerLabel);
            var offsetX = centerX - metrics.width / 2;
            context.fillStyle = "rgb(68, 68, 68)";
            context.fillText(this._innerLabel, offsetX, centerY);
        }
    }
};
