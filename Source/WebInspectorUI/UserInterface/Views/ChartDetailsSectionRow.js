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
    constructor(delegate)
    {
        super(WebInspector.UIString("No Chart Available"));

        this.element.classList.add("chart");

        this._titleElement = document.createElement("div");
        this._titleElement.className = "title";
        this.element.appendChild(this._titleElement);

        var chartContentElement = document.createElement("div");
        chartContentElement.className = "chart-content";
        this.element.appendChild(chartContentElement);

        this._canvas = document.createElement("canvas");
        this._canvas.className = "chart";
        chartContentElement.appendChild(this._canvas);

        this._legendElement = document.createElement("div");
        this._legendElement.className = "legend";
        chartContentElement.appendChild(this._legendElement);

        this._delegate = delegate;
        this._items = [];
        this._title = "";
        this._innerLabel = "";
        this._innerRadius = 0;
        this._innerLabelFontSize = 11;
        this._shadowColor = "rgba(0, 0, 0, 0.6)";
        this._total = 0;
    }

    // Public

    set title(title)
    {
        if (this._title === title)
            return;

        this._title = title;
        this._titleElement.textContent = title;
    }

    set innerLabel(label)
    {
        if (this._innerLabel === label)
            return;

        this._innerLabel = label;

        this._refresh();
    }

    set innerRadius(radius)
    {
        if (this._innerRadius === radius)
            return;

        this._innerRadius = radius;

        this._refresh();
    }

    get total()
    {
        return this._total;
    }

    set data(items)
    {
        if (!(items instanceof Array))
            items = [items];

        items = items.filter(function(item) { return item.value >= 0; });
        if (!this._items.length && !items.length)
            return;

        if (this._items.length === items.length && this._items.every(function(item, index) { return JSON.stringify(item) === JSON.stringify(items[index]); }))
            return;

        this._items = items;
        this._total = this._items.reduce(function(previousValue, currentValue) { return previousValue + currentValue.value; }, 0);;

        this._legendElement.removeChildren();
        this._items.forEach(function(item) { this._legendElement.appendChild(this._createLegendItem(item)); }, this);

        this._refresh();
    }

    // Private

    _createLegendItem(item)
    {
        var colorSwatchElement = document.createElement("div");
        colorSwatchElement.className = "color-swatch";
        colorSwatchElement.style.backgroundColor = item.color;

        var labelElement = document.createElement("div");
        labelElement.className = "label";
        labelElement.appendChild(colorSwatchElement);
        labelElement.appendChild(document.createTextNode(item.label));

        var valueElement = document.createElement("div");
        valueElement.className = "value";

        if (this._delegate && typeof this._delegate.formatChartValue === "function")
            valueElement.textContent = this._delegate.formatChartValue(item.value);
        else
            valueElement.textContent = item.value;

        var legendItemElement = document.createElement("div");
        legendItemElement.className = "legend-item";
        legendItemElement.appendChild(labelElement);
        legendItemElement.appendChild(valueElement);

        return legendItemElement;
    }

    _refresh()
    {
        var width = this._canvas.clientWidth * window.devicePixelRatio;
        var height = this._canvas.clientHeight * window.devicePixelRatio;
        this._canvas.width = width;
        this._canvas.height = height;

        var context = this._canvas.getContext("2d");
        context.clearRect(0, 0, width, height);

        var x = Math.floor(width / 2);
        var y = Math.floor(height / 2);
        var radius = Math.floor(Math.min(x, y) * 0.96);   // Add a small margin to prevent clipping of the chart shadow.
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
        drawSlice(x, y, 0, 2.0 * Math.PI, "rgb(242, 242, 242)");
        context.restore();

        for (var item of this._items) {
            if (item.value === 0)
                continue;
            endAngle += (item.value / this._total) * 2.0 * Math.PI;
            drawSlice(x, y, startAngle, endAngle, item.color);
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
