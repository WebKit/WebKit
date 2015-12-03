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
    constructor(delegate, chartSize, innerRadiusRatio)
    {
        super(WebInspector.UIString("No Chart Available"));

        innerRadiusRatio = innerRadiusRatio || 0;
        console.assert(chartSize > 0, chartSize);
        console.assert(innerRadiusRatio >= 0 && innerRadiusRatio < 1, innerRadiusRatio);

        this.element.classList.add("chart");

        this._titleElement = document.createElement("div");
        this._titleElement.className = "title";
        this.element.appendChild(this._titleElement);

        var chartContentElement = document.createElement("div");
        chartContentElement.className = "chart-content";
        this.element.appendChild(chartContentElement);

        this._chartElement = createSVGElement("svg");
        chartContentElement.appendChild(this._chartElement);

        this._legendElement = document.createElement("div");
        this._legendElement.className = "legend";
        chartContentElement.appendChild(this._legendElement);

        this._delegate = delegate;
        this._items = new Map;
        this._title = "";
        this._chartSize = chartSize;
        this._radius = (this._chartSize / 2) - 1;   // Subtract one to accomodate chart stroke width.
        this._innerRadius = innerRadiusRatio ? Math.floor(this._radius * innerRadiusRatio) : 0;
        this._total = 0;

        this._svgFiltersElement = document.createElement("svg");
        this._svgFiltersElement.classList.add("defs-only");
        this.element.appendChild(this._svgFiltersElement);

        this._checkboxStyleElement = document.createElement("style");
        this._checkboxStyleElement.id = "checkbox-styles";
        document.getElementsByTagName("head")[0].appendChild(this._checkboxStyleElement);

        function createEmptyChartPathData(c, r1, r2)
        {
            var a1 = 0;
            var a2 = Math.PI * 1.9999;
            var x1 = c + Math.cos(a1) * r1,
                y1 = c + Math.sin(a1) * r1,
                x2 = c + Math.cos(a2) * r1,
                y2 = c + Math.sin(a2) * r1,
                x3 = c + Math.cos(a2) * r2,
                y3 = c + Math.sin(a2) * r2,
                x4 = c + Math.cos(a1) * r2,
                y4 = c + Math.sin(a1) * r2;
            return [
                "M", x1, y1,                    // Starting position.
                "A", r1, r1, 0, 1, 1, x2, y2,   // Draw outer arc.
                "Z",                            // Close path.
                "M", x3, y3,                    // Starting position.
                "A", r2, r2, 0, 1, 0, x4, y4,   // Draw inner arc.
                "Z"                             // Close path.
            ].join(" ");
        }

        this._emptyChartPath = createSVGElement("path");
        this._emptyChartPath.setAttribute("d", createEmptyChartPathData(this._chartSize / 2, this._radius, this._innerRadius));
        this._emptyChartPath.classList.add("empty-chart");
        this._chartElement.appendChild(this._emptyChartPath);
    }

    // Public

    get chartSize()
    {
        return this._chartSize;
    }

    set title(title)
    {
        if (this._title === title)
            return;

        this._title = title;
        this._titleElement.textContent = title;
    }

    get total()
    {
        return this._total;
    }

    addItem(id, label, value, color, checkbox, checked)
    {
        console.assert(!this._items.has(id), "Already added item with id: " + id);
        if (this._items.has(id))
            return;

        console.assert(value >= 0, "Value cannot be negative.");
        if (value < 0)
            return;

        this._items.set(id, {label, value, color, checkbox, checked});
        this._total += value;

        this._needsLayout();
    }

    setItemValue(id, value)
    {
        var item = this._items.get(id);
        console.assert(item, "Cannot set value for invalid item id: " + id);
        if (!item)
            return;

        console.assert(value >= 0, "Value cannot be negative.");
        if (value < 0)
            return;

        if (item.value === value)
            return;

        this._total += value - item.value;
        item.value = value;

        this._needsLayout();
    }

    clearItems()
    {
        for (var item of this._items.values()) {
            var path = item[WebInspector.ChartDetailsSectionRow.ChartSegmentPathSymbol];
            if (path)
                path.remove();
        }

        this._total = 0;
        this._items.clear();

        this._needsLayout();
    }

    // Private

    _addCheckboxColorFilter(id, r, g, b)
    {
        for (var i = 0; i < this._svgFiltersElement.childNodes.length; ++i) {
            if (this._svgFiltersElement.childNodes[i].id === id)
                return;
        }

        r /= 255;
        b /= 255;
        g /= 255;

        // Create an svg:filter element that approximates "background-blend-mode: color", for grayscale input.
        var filterElement = createSVGElement("filter");
        filterElement.id = id;
        filterElement.setAttribute("color-interpolation-filters", "sRGB");

        var values = [1 - r, 0, 0, 0, r,
                      1 - g, 0, 0, 0, g,
                      1 - b, 0, 0, 0, b,
                      0,     0, 0, 1, 0];

        var colorMatrixPrimitive = createSVGElement("feColorMatrix");
        colorMatrixPrimitive.setAttribute("type", "matrix");
        colorMatrixPrimitive.setAttribute("values", values.join(" "));

        function createGammaPrimitive(tagName, value)
        {
            var gammaPrimitive = createSVGElement(tagName);
            gammaPrimitive.setAttribute("type", "gamma");
            gammaPrimitive.setAttribute("exponent", value);
            return gammaPrimitive;
        }

        var componentTransferPrimitive = createSVGElement("feComponentTransfer");
        componentTransferPrimitive.appendChild(createGammaPrimitive("feFuncR", 1.4));
        componentTransferPrimitive.appendChild(createGammaPrimitive("feFuncG", 1.4));
        componentTransferPrimitive.appendChild(createGammaPrimitive("feFuncB", 1.4));
        filterElement.appendChild(colorMatrixPrimitive);
        filterElement.appendChild(componentTransferPrimitive);

        this._svgFiltersElement.appendChild(filterElement);

        var styleSheet = this._checkboxStyleElement.sheet;
        styleSheet.insertRule(".details-section > .content > .group > .row.chart > .chart-content > .legend > .legend-item > label > input[type=checkbox]." + id + " { filter: grayscale(1) url(#" + id + ") }", 0);
    }

    _updateLegend()
    {
        if (!this._items.size) {
            this._legendElement.removeChildren();
            return;
        }

        function formatItemValue(item)
        {
            if (this._delegate && typeof this._delegate.formatChartValue === "function")
                return this._delegate.formatChartValue(item.value);
            return item.value;
        }

        for (var [id, item] of this._items) {
            if (item[WebInspector.ChartDetailsSectionRow.LegendItemValueElementSymbol]) {
                var valueElement = item[WebInspector.ChartDetailsSectionRow.LegendItemValueElementSymbol];
                valueElement.textContent = formatItemValue.call(this, item);
                continue;
            }

            var labelElement = document.createElement("label");
            var keyElement;
            if (item.checkbox) {
                var className = id.toLowerCase();
                var rgb = item.color.substring(4, item.color.length - 1).replace(/ /g, "").split(",");
                if (rgb[0] === rgb[1] && rgb[1] === rgb[2])
                    rgb[0] = rgb[1] = rgb[2] = Math.min(160, rgb[0]);

                keyElement = document.createElement("input");
                keyElement.type = "checkbox";
                keyElement.classList.add(className);
                keyElement.checked = item.checked;
                keyElement[WebInspector.ChartDetailsSectionRow.DataItemIdSymbol] = id;

                keyElement.addEventListener("change", this._legendItemCheckboxValueChanged.bind(this));

                this._addCheckboxColorFilter(className, rgb[0], rgb[1], rgb[2]);
            } else {
                keyElement = document.createElement("div");
                keyElement.classList.add("color-key");
                keyElement.style.backgroundColor = item.color;
            }

            labelElement.appendChild(keyElement);
            labelElement.appendChild(document.createTextNode(item.label));

            var valueElement = document.createElement("div");
            valueElement.classList.add("value");
            valueElement.textContent = formatItemValue.call(this, item);

            item[WebInspector.ChartDetailsSectionRow.LegendItemValueElementSymbol] = valueElement;

            var legendItemElement = document.createElement("div");
            legendItemElement.classList.add("legend-item");
            legendItemElement.appendChild(labelElement);
            legendItemElement.appendChild(valueElement);

            this._legendElement.appendChild(legendItemElement);
        }
    }

    _legendItemCheckboxValueChanged(event)
    {
        var checkbox = event.target;
        var id = checkbox[WebInspector.ChartDetailsSectionRow.DataItemIdSymbol];
        this.dispatchEventToListeners(WebInspector.ChartDetailsSectionRow.Event.LegendItemChecked, {id, checked: checkbox.checked});
    }

    _needsLayout()
    {
        if (this._scheduledLayoutUpdateIdentifier)
            return;

        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(this._updateLayout.bind(this));
    }

    _updateLayout()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            this._scheduledLayoutUpdateIdentifier = undefined;
        }

        this._updateLegend();

        this._chartElement.setAttribute("width", this._chartSize);
        this._chartElement.setAttribute("height", this._chartSize);
        this._chartElement.setAttribute("viewbox", "0 0 " + this._chartSize + " " + this._chartSize);

        function createSegmentPathData(c, a1, a2, r1, r2)
        {
            var largeArcFlag = ((a2 - a1) % (Math.PI * 2)) > Math.PI ? 1 : 0;
            var x1 = c + Math.cos(a1) * r1,
                y1 = c + Math.sin(a1) * r1,
                x2 = c + Math.cos(a2) * r1,
                y2 = c + Math.sin(a2) * r1,
                x3 = c + Math.cos(a2) * r2,
                y3 = c + Math.sin(a2) * r2,
                x4 = c + Math.cos(a1) * r2,
                y4 = c + Math.sin(a1) * r2;
            return [
                "M", x1, y1,                                // Starting position.
                "A", r1, r1, 0, largeArcFlag, 1, x2, y2,    // Draw outer arc.
                "L", x3, y3,                                // Connect outer and innner arcs.
                "A", r2, r2, 0, largeArcFlag, 0, x4, y4,    // Draw inner arc.
                "Z"                                         // Close path.
            ].join(" ");
        }

        // Balance item values so that all non-zero chart segments are visible.
        var minimumDisplayValue = this._total * 0.015;

        var items = [];
        for (var item of this._items.values()) {
            item.displayValue = item.value ? Math.max(minimumDisplayValue, item.value) : 0;
            if (item.displayValue)
                items.push(item);
        }

        if (items.length > 1) {
            items.sort(function(a, b) { return a.value - b.value; });

            var largeItemCount = items.length;
            var totalAdjustedValue = 0;
            for (var item of items) {
                if (item.value < minimumDisplayValue) {
                    totalAdjustedValue += minimumDisplayValue - item.value;
                    largeItemCount--;
                    continue;
                }

                if (!totalAdjustedValue || !largeItemCount)
                    break;

                var donatedValue = totalAdjustedValue / largeItemCount;
                if (item.displayValue - donatedValue >= minimumDisplayValue) {
                    item.displayValue -= donatedValue;
                    totalAdjustedValue -= donatedValue;
                }

                largeItemCount--;
            }
        }

        var center = this._chartSize / 2;
        var startAngle = -Math.PI / 2;
        var endAngle = 0;
        for (var [id, item] of this._items) {
            var path = item[WebInspector.ChartDetailsSectionRow.ChartSegmentPathSymbol];
            if (!path) {
                path = createSVGElement("path");
                path.classList.add("chart-segment");
                path.setAttribute("fill", item.color);
                this._chartElement.appendChild(path);

                item[WebInspector.ChartDetailsSectionRow.ChartSegmentPathSymbol] = path;
            }

            if (!item.value) {
                path.classList.add("hidden");
                continue;
            }

            var angle = (item.displayValue / this._total) * Math.PI * 2;
            endAngle = startAngle + angle;

            path.setAttribute("d", createSegmentPathData(center, startAngle, endAngle, this._radius, this._innerRadius));
            path.classList.remove("hidden");

            startAngle = endAngle;
        }
    }
};

WebInspector.ChartDetailsSectionRow.DataItemIdSymbol = Symbol("chart-details-section-row-data-item-id");
WebInspector.ChartDetailsSectionRow.ChartSegmentPathSymbol = Symbol("chart-details-section-row-chart-segment-path");
WebInspector.ChartDetailsSectionRow.LegendItemValueElementSymbol = Symbol("chart-details-section-row-legend-item-value-element");

WebInspector.ChartDetailsSectionRow.Event = {
    LegendItemChecked: "chart-details-section-row-legend-item-checked"
};
