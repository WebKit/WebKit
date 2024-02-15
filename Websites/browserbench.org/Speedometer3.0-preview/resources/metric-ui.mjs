import { Metric } from "./metric.mjs";

export const COLORS = Object.freeze(["blue", "blue-light", "green-light", "green", "yellow", "orange", "red", "magenta", "violet", "purple", "blue-dark", "green-dark", "ochre", "rust"]);

export function renderMetricView(viewParams) {
    let { metrics, width = 500, trackHeight = 20, subMetricMargin = 35, title = "", colors = COLORS } = viewParams;
    // Make sure subMetricMargin is set for use in renderSubMetrics.
    viewParams.subMetricMargin = subMetricMargin;
    const scatterPlotParams = { width, trackHeight, colors };

    scatterPlotParams.xAxisPositiveOnly = false;
    scatterPlotParams.xAxisShowZero = true;
    scatterPlotParams.values = prepareScatterPlotValues(metrics, true);
    scatterPlotParams.unit = "%";
    scatterPlotParams.xAxisLabel = "Spread Normalized";
    const normalizedScatterPlot = renderScatterPlot(scatterPlotParams);

    scatterPlotParams.xAxisPositiveOnly = true;
    scatterPlotParams.xAxisShowZero = false;
    scatterPlotParams.values = prepareScatterPlotValues(metrics, false);
    scatterPlotParams.unit = metrics[0].unit;
    scatterPlotParams.xAxisLabel = metrics[0].unit;
    const absoluteScatterPlot = renderScatterPlot(scatterPlotParams);

    const legend = metrics
        .map(
            (metric, i) => `
                <tr >
                    <td class="${colors[i % colors.length]} no-select" >●</td>
                    <td class="label">${metric.shortName}</td>
                    <td class="number">${metric.mean.toFixed(2)}</td>
                    <td>±</td>
                    <td>${metric.deltaString}</td>
                    <td>${metric.unit}</td>
                </tr>`
        )
        .join("");
    return `
        <dl class="metric">
            <dt><h3>${title}<h3></dt>
            <dd>
                <div class="metric-chart"">
                    <div onclick="document.body.classList.toggle('relative-charts')">
                        <div class="metric-chart-absolute">
                            ${absoluteScatterPlot}
                        </div>
                        <div class="metric-chart-relative">
                            ${normalizedScatterPlot}
                        </div>
                    </div>
                    <table class="chart chart-legend">${legend}</table>
                </div>
                ${renderSubMetrics(viewParams)}
            </dd>
        </dl>
    `;
}

function renderSubMetrics(viewParams) {
    const { metrics, width, subMetricMargin, colors = COLORS, renderChildren = true } = viewParams;
    const valuesTable = `
            <label class="details-toggle">
                <input type="checkbox" 
                        onclick="this.parentNode.nextElementSibling.classList.toggle('visible')" />
                Table
            </label>
            <div class="submetrics">
                ${renderMetricsTable(metrics)}
            </div>`;
    const hasChildMetric = metrics.length > 0 && metrics[0].children.length > 0;
    if (!hasChildMetric || !renderChildren)
        return valuesTable;

    const subMetricWidth = width - subMetricMargin;
    const childColors = [...colors];

    const subMetrics = metrics
        .map((metric) => {
            // Rotate colors to get different colors for sub-plots.
            for (let i = 0; i < metric.children.length; i++) {
                const color = childColors.pop();
                childColors.unshift(color);
            }
            const subMetricParams = {
                ...viewParams,
                parentMetric: metric,
                metrics: metric.children,
                title: metric.name,
                width: subMetricWidth,
                colors: childColors,
            };
            return renderMetricView(subMetricParams);
        })
        .join("");
    return `${valuesTable}
        <label class="details-toggle">
            <input type="checkbox" 
                    onclick="this.parentNode.nextElementSibling.classList.toggle('visible')" />
            Submetrics 
        </label>
        <div class="submetrics">
            ${subMetrics}
        </div>
    `;
}

function renderMetricsTable(metrics, min, max) {
    let numRows = 0;
    let columnHeaders = "";
    let commonPrefixes = metrics[0].name.split(Metric.separator);
    for (const metric of metrics) {
        const prefixes = metric.name.split(Metric.separator);
        for (let i = commonPrefixes.length - 1; i >= 0; i--) {
            if (commonPrefixes[i] !== prefixes[i])
                commonPrefixes.pop();
        }

    }
    const commonPrefix = commonPrefixes.join(Metric.separator);
    let commonPrefixHeader = "";
    if (commonPrefix) {
        commonPrefixHeader = `
            <tr>
                <td></td>
                <td colspan="${metrics.length}" class="prefix">${commonPrefix}</td>
            </tr>`;
    }
    for (const metric of metrics) {
        const name = metric.name.substring(commonPrefix.length);
        columnHeaders += `<th>${name} [${metric.unit}]</th>`;
        numRows = Math.max(metric.values.length, numRows);
    }

    let body = "";
    for (let row = 0; row < numRows; row++) {
        let columns = "";
        for (const metric of metrics) {
            const value = metric.values[row];
            if (value === undefined)
                continue;
            const delta = metric.max - metric.min;
            const percent = Math.max(Math.min((value - metric.min) / delta, 1), 0) * 100;
            const percentGradient = `background: linear-gradient(90deg, var(--foreground-alpha) ${percent}%, rgba(0,0,0,0) ${percent}%);`;
            columns += `<td style="${percentGradient}">${value.toFixed(2)}</td>`;
        }
        body += `<tr>
            <td>${row}</td>
            ${columns}
        </tr>`;
    }
    return `<table class="metrics-table" >
        <thead onclick="this.classList.toggle('nowrap')" >
            ${commonPrefixHeader}
            <tr>
                <th>Iteration</th>
                ${columnHeaders}
            </tr>
        </thead>
        <tbody>
        ${body}
        <tbody>
    </table>`;
}

function prepareScatterPlotValues(metrics, normalize = true) {
    let points = [];
    // Arrange child-metrics values in a single coordinate system:
    // - metric 1: x values are in range [0, 1]
    // - metric 2: y values are in range [1, 2]
    // - ...
    // This way each metric data point is on a separate track in the scatter
    // plot.
    // If normalize == true:
    //   All x values are normalized by the mean of each metric and
    //   centered on 0.
    //   Example: [90ms, 100ms, 110ms] =>  [-10%, 0%, +10%]
    const toPercent = 100;
    let unit;
    for (let metricIndex = 0; metricIndex < metrics.length; metricIndex++) {
        const metric = metrics[metricIndex];
        // If the mean is 0 we can't normalize values properly.
        const mean = metric.mean || 1;
        if (!unit)
            unit = metric.unit;
        else if (unit !== metric.unit)
            throw new Error("All metrics must have the same unit.");
        let width = metric.delta || 1;
        let center = mean;
        if (normalize) {
            width = (metric.delta / mean) * toPercent;
            center = 0;
        }
        const left = center - width / 2;
        const y = metricIndex;
        const label = `Mean: ${metric.valueString}\n` + `Min: ${metric.min.toFixed(2)}${unit}\n` + `Max: ${metric.max.toFixed(2)}${unit}`;
        const rect = [left, y, label, width];
        // Add data for individual points:
        points.push(rect);
        const values = metric.values;
        const length = values.length;
        for (let i = 0; i < length; i++) {
            const value = values[i];
            let x = value;
            let normalized = (value / mean - 1) * toPercent;
            if (normalize)
                x = normalized;
            const sign = normalized < 0 ? "-" : "+";
            normalized = Math.abs(normalized);
            // Each value is mapped to a y-coordinate in the range of [metricIndex, metricIndex + 1]
            const valueOffsetY = length === 1 ? 0.5 : i / length;
            const y = metricIndex + valueOffsetY;
            let label = `Iteration ${i}: ${value.toFixed(3)}${unit}\n` + `Normalized: ${metric.mean.toFixed(3)}${unit} ${sign} ${normalized.toFixed(2)}%`;
            const point = [x, y, label];
            points.push(point);
        }
    }
    return points;
}

function renderScatterPlot({ values, width = 500, height, trackHeight, xAxisPositiveOnly = false, xAxisShowZero = false, xAxisLabel, unit = "", colors = COLORS }) {
    if (!height && !trackHeight)
        throw new Error("Either height or trackHeight must be specified");
    let xMin = Infinity;
    let xMax = 0;
    let yMin = Infinity;
    let yMax = 0;
    for (let value of values) {
        let [x, y] = value;
        xMin = Math.min(xMin, x);
        xMax = Math.max(xMax, x);
        yMin = Math.min(yMin, y);
        yMax = Math.max(yMax, y);
    }
    if (xAxisPositiveOnly)
        xMin = Math.max(xMin, 0);
    // Max delta of values across each axis:
    const trackCount = Math.ceil(yMax - yMin) || 1;
    const spreadX = xMax - xMin;
    // Axis + labels height:
    const axisHeight = 18;
    const axisMarginY = 4;
    const markerSize = 5;
    const trackMargin = 2;
    // Recalculate height:
    if (height)
        trackHeight = (height - axisHeight - axisMarginY) / trackCount;
    else
        height = trackCount * trackHeight + axisHeight + axisMarginY;

    // Horizontal axis position:
    const axisY = height - axisHeight + axisMarginY;
    const unitToPosX = width / spreadX;
    const unitToPosY = trackHeight - trackMargin - markerSize / 2;
    const points = values.map(renderValue).join("");
    let xAxisZeroLine = "";
    if (xAxisShowZero) {
        const xZeroPos = (0 - xMin) * unitToPosX;
        xAxisZeroLine = `<line x1="${xZeroPos}" x2="${xZeroPos}" y1="${0}" y2="${axisY}" class="axis"/>`;
    }
    return `
        <svg class="scatter-plot chart"
            width="${width}" height="${height}"
            viewBox="${`0 0 ${width} ${height}`}">
            <g class="horizontal-axis no-select">
                <line
                    x1="${0}" x2="${width}"
                    y1="${axisY - axisMarginY}" y2="${axisY - axisMarginY}"
                    class="axis" />
                <text y="${axisY}" x="0" text-anchor="start">${xMin.toFixed(2)}${unit}</text>
                <text y="${axisY}" x="${width / 2}" text-anchor="middle">${xAxisLabel}</text>
                <text y="${axisY}" x="${width}" text-anchor="end">${xMax.toFixed(2)}${unit}</text>
            </g>
            <defs>
                <g id="marker">
                    <circle r="${markerSize / 2}" />
                </g>
            </defs>
            <g class="values">
                ${xAxisZeroLine}
                ${points}
            </g>
        </svg>
    `;

    function renderValue(value) {
        const [rawX, rawY, label, rawWidth = 0] = value;
        const trackIndex = rawY | 0;
        const y = (rawY - yMin) * unitToPosY + markerSize * trackIndex;
        const cssClass = colors[trackIndex % colors.length];

        if (value.length <= 3) {
            // Render a simple marker:
            const x = (rawX - xMin) * unitToPosX;
            const adjustedY = y + markerSize / 2;
            return `
                <use href="#marker" x="${x}" y="${adjustedY}" class="marker ${cssClass}">
                    <title>${label}</title>
                </use>
            `;
        } else {
            // Render a rect with 4 input values:
            const x = (rawX - xMin) * unitToPosX + rawWidth / 2;
            const w = rawWidth * unitToPosX;
            const centerX = x + w / 2;
            const top = y;
            const height = trackHeight - trackMargin;
            const bottom = top + height;
            return `
            <g class="percentile ${cssClass}">
                <rect x="${x}" y="${top}" width="${w}" height="${height}">
                    <title>${label}</title>
                </rect>
                <line x1="${x}" x2="${x}" y1="${top}" y2="${bottom}" />
                <line
                    x1="${centerX}" x2="${centerX}"
                    y1="${top}"     y2="${bottom}"
                    stroke-dasharray="${height / 3}" />
                <line x1="${x + w}" x2="${x + w}" y1="${top}" y2="${bottom}" />
            </g>
        `;
        }
    }
}
