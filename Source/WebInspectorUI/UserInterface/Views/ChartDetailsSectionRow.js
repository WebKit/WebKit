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

        this._svgFiltersElement = document.createElement("svg");
        this._svgFiltersElement.classList.add("defs-only");
        this.element.append(this._svgFiltersElement);

        this._checkboxStyleElement = document.createElement("style");
        this._checkboxStyleElement.id = "checkbox-styles";
        document.getElementsByTagName("head")[0].append(this._checkboxStyleElement);
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

        this._createLegend();
        this._refresh();
    }

    // Private

    _addCheckboxColorFilter(id, r, g, b)
    {
        for (let i = 0; i < this._svgFiltersElement.childNodes.length; ++i) {
            if (this._svgFiltersElement.childNodes[i].id === id)
                return;
        }

        r /= 255;
        b /= 255;
        g /= 255;

        // Create an svg:filter element that approximates "background-blend-mode: color", for grayscale input.
        let filterElement = createSVGElement("filter");
        filterElement.id = id;
        filterElement.setAttribute("color-interpolation-filters", "sRGB");

        let values = [1 - r, 0, 0, 0, r,
                      1 - g, 0, 0, 0, g,
                      1 - b, 0, 0, 0, b,
                      0,     0, 0, 1, 0];

        let colorMatrixPrimitive = createSVGElement("feColorMatrix");
        colorMatrixPrimitive.setAttribute("type", "matrix");
        colorMatrixPrimitive.setAttribute("values", values.join(" "));

        function createGammaPrimitive(tagName, value)
        {
            let gammaPrimitive = createSVGElement(tagName);
            gammaPrimitive.setAttribute("type", "gamma");
            gammaPrimitive.setAttribute("value", value);
            return gammaPrimitive;
        }

        let componentTransferPrimitive = createSVGElement("feComponentTransfer");
        componentTransferPrimitive.append(createGammaPrimitive("feFuncR", 1.2), createGammaPrimitive("feFuncG", 1.2), createGammaPrimitive("feFuncB", 1.2));
        filterElement.append(colorMatrixPrimitive, componentTransferPrimitive);

        this._svgFiltersElement.append(filterElement);

        let styleSheet = this._checkboxStyleElement.sheet;
        styleSheet.insertRule(".details-section > .content > .group > .row.chart > .chart-content > .legend > .legend-item > label > input[type=checkbox]." + id + " { filter: grayscale(1) url(#" + id + ") }", 0);
    }

    _createLegend()
    {
        this._legendElement.removeChildren();

        for (let item of this._items) {
            let labelElement = document.createElement("label");
            let keyElement;
            if (item.checkbox) {
                let className = item.id.toLowerCase();
                let rgb = item.color.substring(4, item.color.length - 1).replace(/ /g, "").split(",");
                if (rgb[0] === rgb[1] && rgb[1] === rgb[2])
                    rgb[0] = rgb[1] = rgb[2] = Math.min(160, rgb[0]);

                keyElement = document.createElement("input");
                keyElement.type = "checkbox";
                keyElement.classList.add(className);
                keyElement.checked = item.checked || true;
                keyElement[WebInspector.ChartDetailsSectionRow.DataItemIdSymbol] = item.id;

                keyElement.addEventListener("change", this._legendItemCheckboxValueChanged.bind(this));

                this._addCheckboxColorFilter(className, rgb[0], rgb[1], rgb[2]);
            } else {
                keyElement = document.createElement("div");
                keyElement.classList.add("color-key");
                keyElement.style.backgroundColor = item.color;
            }

            labelElement.append(keyElement, item.label);

            let valueElement = document.createElement("div");
            valueElement.classList.add("value");

            if (this._delegate && typeof this._delegate.formatChartValue === "function")
                valueElement.textContent = this._delegate.formatChartValue(item.value);
            else
                valueElement.textContent = item.value;

            let legendItemElement = document.createElement("div");
            legendItemElement.classList.add("legend-item");
            legendItemElement.append(labelElement, valueElement);

            this._legendElement.append(legendItemElement);
        }
    }

    _legendItemCheckboxValueChanged(event)
    {
        let checkbox = event.target;
        let id = checkbox[WebInspector.ChartDetailsSectionRow.DataItemIdSymbol];
        this.dispatchEventToListeners(WebInspector.ChartDetailsSectionRow.Event.LegendItemChecked, {id, checked: checkbox.checked});
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

WebInspector.ChartDetailsSectionRow.DataItemIdSymbol = Symbol("chart-details-section-row-data-item-id");

WebInspector.ChartDetailsSectionRow.Event = {
    LegendItemChecked: "chart-details-section-row-legend-item-checked"
};
