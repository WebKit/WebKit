
class TimeSeriesChart extends ComponentBase {
    constructor(sourceList, options)
    {
        super();
        this._canvas = null;
        this._sourceList = sourceList;
        this._trendLines = null;
        this._options = options;
        this._fetchedTimeSeries = null;
        this._sampledTimeSeriesData = null;
        this._renderedTrendLines = false;
        this._valueRangeCache = null;
        this._annotations = null;
        this._annotationRows = null;
        this._startTime = null;
        this._endTime = null;
        this._width = null;
        this._height = null;
        this._contextScaleX = 1;
        this._contextScaleY = 1;
        this._rem = null;
    }

    _ensureCanvas()
    {
        if (!this._canvas) {
            this._canvas = this._createCanvas();
            this._canvas.style.display = 'block';
            this._canvas.style.position = 'absolute';
            this._canvas.style.left = '0px';
            this._canvas.style.top = '0px';
            this.content().appendChild(this._canvas);
        }
        return this._canvas;
    }

    static cssTemplate()
    {
        return `

        :host {
            display: block !important;
            position: relative !important;
        }

        `;
    }

    static get enqueueToRenderOnResize() { return true; }

    _createCanvas()
    {
        return document.createElement('canvas');
    }

    setDomain(startTime, endTime)
    {
        console.assert(startTime < endTime, 'startTime must be before endTime');
        this._startTime = startTime;
        this._endTime = endTime;
        this._fetchedTimeSeries = null;
        this.fetchMeasurementSets(false);
    }

    setSourceList(sourceList)
    {
        this._sourceList = sourceList;
        this._fetchedTimeSeries = null;
        this.fetchMeasurementSets(false);
    }

    sourceList() { return this._sourceList; }

    clearTrendLines()
    {
        this._trendLines = null;
        this._renderedTrendLines = false;
        this.enqueueToRender();
    }

    setTrendLine(sourceIndex, trendLine)
    {
        if (this._trendLines)
            this._trendLines = this._trendLines.slice(0);
        else
            this._trendLines = [];
        this._trendLines[sourceIndex] = trendLine;
        this._renderedTrendLines = false;
        this.enqueueToRender();
    }

    fetchMeasurementSets(noCache)
    {
        var fetching = false;
        for (var source of this._sourceList) {
            if (source.measurementSet) {
                if (!noCache && source.measurementSet.hasFetchedRange(this._startTime, this._endTime))
                    continue;
                source.measurementSet.fetchBetween(this._startTime, this._endTime, this._didFetchMeasurementSet.bind(this, source.measurementSet), noCache);
                fetching = true;
            }

        }
        this._sampledTimeSeriesData = null;
        this._valueRangeCache = null;
        this._annotationRows = null;
        if (!fetching)
            this.enqueueToRender();
    }

    _didFetchMeasurementSet(set)
    {
        this._fetchedTimeSeries = null;
        this._sampledTimeSeriesData = null;
        this._valueRangeCache = null;
        this._annotationRows = null;

        this.enqueueToRender();
    }

    // FIXME: Figure out a way to make this readonly.
    sampledTimeSeriesData(type)
    {
        if (!this._sampledTimeSeriesData)
            return null;
        for (var i = 0; i < this._sourceList.length; i++) {
            if (this._sourceList[i].type == type)
                return this._sampledTimeSeriesData[i];
        }
        return null;
    }

    referencePoints(type)
    {
        const view = this.sampledTimeSeriesData(type);
        if (!view || !this._startTime || !this._endTime)
            return null;
        const point = view.lastPointInTimeRange(this._startTime, this._endTime);
        if (!point)
            return null;
        return {view, currentPoint: point, previousPoint: null};
    }

    setAnnotations(annotations)
    {
        this._annotations = annotations;
        this._annotationRows = null;

        this.enqueueToRender();
    }

    render()
    {
        if (!this._startTime || !this._endTime)
            return;

        var canvas = this._ensureCanvas();

        var metrics = this._layout();
        if (!metrics.doneWork)
            return;

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'render');

        var context = canvas.getContext('2d');
        context.scale(this._contextScaleX, this._contextScaleY);

        context.clearRect(0, 0, this._width, this._height);

        context.font = metrics.fontSize + 'px sans-serif';
        const axis = this._options.axis;
        if (axis) {
            context.strokeStyle = axis.gridStyle;
            context.lineWidth = 1 / this._contextScaleY;
            this._renderXAxis(context, metrics, this._startTime, this._endTime);
            this._renderYAxis(context, metrics, this._valueRangeCache[0], this._valueRangeCache[1]);
        }

        context.save();

        context.beginPath();
        context.rect(metrics.chartX, metrics.chartY, metrics.chartWidth, metrics.chartHeight);
        context.clip();

        this._renderChartContent(context, metrics);

        context.restore();

        context.setTransform(1, 0, 0, 1, 0, 0);

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'render');
    }

    _layout()
    {
        // FIXME: We should detect changes in _options and _sourceList.
        // FIXME: We should consider proactively preparing time series caches to avoid jaggy scrolling.
        var doneWork = this._updateCanvasSizeIfClientSizeChanged();
        var metrics = this._computeHorizontalRenderingMetrics();
        doneWork |= this._ensureSampledTimeSeries(metrics);
        doneWork |= this._ensureTrendLines();
        doneWork |= this._ensureValueRangeCache();
        this._computeVerticalRenderingMetrics(metrics);
        doneWork |= this._layoutAnnotationBars(metrics);
        metrics.doneWork = doneWork;
        return metrics;
    }

    _computeHorizontalRenderingMetrics()
    {
        // FIXME: We should detect when rem changes.
        if (!this._rem)
            this._rem = parseFloat(getComputedStyle(document.documentElement).fontSize);

        var timeDiff = this._endTime - this._startTime;
        var startTime = this._startTime;

        const axis = this._options.axis || {};
        const fontSize = (axis.fontSize || 1) * this._rem;
        const chartX = (axis.yAxisWidth || 0) * fontSize;
        var chartY = 0;
        var chartWidth = this._width - chartX;
        var chartHeight = this._height - (axis.xAxisHeight || 0) * fontSize;

        if (axis.xAxisEndPadding)
            timeDiff += axis.xAxisEndPadding / (chartWidth / timeDiff);

        return {
            xToTime: function (x)
            {
                var time = (x - chartX) / (chartWidth / timeDiff) + +startTime;
                console.assert(Math.abs(x - this.timeToX(time)) < 1e-6);
                return time;
            },
            timeToX: function (time) { return (chartWidth / timeDiff) * (time - startTime) + chartX; },
            valueToY: function (value)
            {
                return ((chartHeight - this.annotationHeight) / this.valueDiff) * (this.endValue - value) + chartY;
            },
            chartX: chartX,
            chartY: chartY,
            chartWidth: chartWidth,
            chartHeight: chartHeight,
            annotationHeight: 0, // Computed later in _layoutAnnotationBars.
            fontSize: fontSize,
            valueDiff: 0,
            endValue: 0,
        };
    }

    _computeVerticalRenderingMetrics(metrics)
    {
        var minValue = this._valueRangeCache[0];
        var maxValue = this._valueRangeCache[1];
        var valueDiff = maxValue - minValue;
        var valueMargin = valueDiff * 0.05;
        var endValue = maxValue + valueMargin;
        var valueDiffWithMargin = valueDiff + valueMargin * 2;

        metrics.valueDiff = valueDiffWithMargin;
        metrics.endValue = endValue;
    }

    _layoutAnnotationBars(metrics)
    {
        if (!this._annotations || !this._options.annotations)
            return false;

        var barHeight = this._options.annotations.barHeight;
        var barSpacing = this._options.annotations.barSpacing;

        if (this._annotationRows) {
            metrics.annotationHeight = this._annotationRows.length * (barHeight + barSpacing);
            return false;
        }

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'layoutAnnotationBars');

        var minWidth = this._options.annotations.minWidth;

        // (1) Expand the width of each bar to hit the minimum width and sort them by left edges.
        this._annotations.forEach(function (annotation) {
            var x1 = metrics.timeToX(annotation.startTime);
            var x2 = metrics.timeToX(annotation.endTime);
            if (x2 - x1 < minWidth) {
                x1 -= minWidth / 2;
                x2 += minWidth / 2;
            }
            annotation.x = x1;
            annotation.width = x2 - x1;
        });
        var sortedAnnotations = this._annotations.sort(function (a, b) { return a.x - b.x });

        // (2) For each bar, find the first row in which the last bar's right edge appear
        // on the left of the bar as each row contains non-overlapping bars in the acending x order.
        var rows = [];
        sortedAnnotations.forEach(function (currentItem) {
            for (var rowIndex = 0; rowIndex < rows.length; rowIndex++) {
                var currentRow = rows[rowIndex];
                var lastItem = currentRow[currentRow.length - 1];
                if (lastItem.x + lastItem.width + minWidth < currentItem.x) {
                    currentRow.push(currentItem);
                    return;
                }
            }
            rows.push([currentItem]);
        });

        this._annotationRows = rows;
        for (var rowIndex = 0; rowIndex < rows.length; rowIndex++) {
            for (var annotation of rows[rowIndex]) {
                annotation.y = metrics.chartY + metrics.chartHeight - (rows.length - rowIndex) * (barHeight + barSpacing);
                annotation.height = barHeight;
            }
        }

        metrics.annotationHeight = rows.length * (barHeight + barSpacing);

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'layoutAnnotationBars');

        return true;
    }

    _renderXAxis(context, metrics, startTime, endTime)
    {
        var typicalWidth = context.measureText('12/31 x').width;
        var maxXAxisLabels = Math.floor(metrics.chartWidth / typicalWidth);
        var xAxisGrid = TimeSeriesChart.computeTimeGrid(startTime, endTime, maxXAxisLabels);

        for (var item of xAxisGrid) {
            context.beginPath();
            var x = metrics.timeToX(item.time);
            context.moveTo(x, metrics.chartY);
            context.lineTo(x, metrics.chartY + metrics.chartHeight);
            context.stroke();
        }

        if (!this._options.axis.xAxisHeight)
            return;

        var rightEdgeOfPreviousItem = 0;
        for (var item of xAxisGrid) {
            var xCenter = metrics.timeToX(item.time);
            var width = context.measureText(item.label).width;
            var x = xCenter - width / 2;
            if (x + width > metrics.chartX + metrics.chartWidth) {
                x = metrics.chartX + metrics.chartWidth - width;
                if (x <= rightEdgeOfPreviousItem)
                    break;
            }
            rightEdgeOfPreviousItem = x + width;
            context.fillText(item.label, x, metrics.chartY + metrics.chartHeight + metrics.fontSize);
        }
    }

    _renderYAxis(context, metrics, minValue, maxValue)
    {
        const maxYAxisLabels = Math.floor(metrics.chartHeight / metrics.fontSize / 2);
        const yAxisGrid = TimeSeriesChart.computeValueGrid(minValue, maxValue, maxYAxisLabels, this._options.axis.valueFormatter);

        for (let item of yAxisGrid) {
            context.beginPath();
            const y = metrics.valueToY(item.value);
            context.moveTo(metrics.chartX, y);
            context.lineTo(metrics.chartX + metrics.chartWidth, y);
            context.stroke();
        }

        if (!this._options.axis.yAxisWidth)
            return;

        for (let item of yAxisGrid) {
            const x = (metrics.chartX - context.measureText(item.label).width) / 2;

            let y = metrics.valueToY(item.value) + metrics.fontSize / 2.5;
            if (y < metrics.fontSize)
                y = metrics.fontSize;

            context.fillText(item.label, x, y);
        }
    }

    _renderChartContent(context, metrics)
    {
        context.lineJoin = 'round';
        for (var i = 0; i < this._sourceList.length; i++) {
            var source = this._sourceList[i];
            var series = this._sampledTimeSeriesData[i];
            if (series)
                this._renderTimeSeries(context, metrics, source, series, this._trendLines && this._trendLines[i] ? 'background' : '');
        }

        for (var i = 0; i < this._sourceList.length; i++) {
            var source = this._sourceList[i];
            var trendLine = this._trendLines ? this._trendLines[i] : null;
            if (series && trendLine)
                this._renderTimeSeries(context, metrics, source, trendLine, 'foreground');
        }

        if (!this._annotationRows)
            return;

        for (var row of this._annotationRows) {
            for (var bar of row) {
                if (bar.x > this.chartWidth || bar.x + bar.width < 0)
                    continue;
                context.fillStyle = bar.fillStyle;
                context.fillRect(bar.x, bar.y, bar.width, bar.height);
            }
        }
    }

    _renderTimeSeries(context, metrics, source, series, layerName)
    {
        for (let point = series.firstPoint(); point; point = series.nextPoint(point)) {
            point.x = metrics.timeToX(point.time);
            point.y = metrics.valueToY(point.value);
        }

        if (source.intervalStyle) {
            context.strokeStyle = source.intervalStyle;
            context.fillStyle = source.intervalStyle;
            context.lineWidth = source.intervalWidth;

            context.beginPath();
            var width = 1;
            for (var i = 0; i < series.length; i++) {
                var point = series[i];
                var interval = point.interval;
                var value = interval ? interval[0] : point.value;
                context.lineTo(point.x - width, metrics.valueToY(value));
                context.lineTo(point.x + width, metrics.valueToY(value));
            }
            for (var i = series.length - 1; i >= 0; i--) {
                var point = series[i];
                var interval = point.interval;
                var value = interval ? interval[1] : point.value;
                context.lineTo(point.x + width, metrics.valueToY(value));
                context.lineTo(point.x - width, metrics.valueToY(value));
            }
            context.fill();
        }

        context.strokeStyle = this._sourceOptionWithFallback(source, layerName + 'LineStyle', 'lineStyle');
        context.lineWidth = this._sourceOptionWithFallback(source, layerName + 'LineWidth', 'lineWidth');
        context.beginPath();
        for (let point = series.firstPoint(); point; point = series.nextPoint(point))
            context.lineTo(point.x, point.y);
        context.stroke();

        context.fillStyle = this._sourceOptionWithFallback(source, layerName + 'PointStyle', 'pointStyle');
        var radius = this._sourceOptionWithFallback(source, layerName + 'PointRadius', 'pointRadius');
        if (radius) {
            for (let point = series.firstPoint(); point; point = series.nextPoint(point))
                this._fillCircle(context, point.x, point.y, radius);
        }
    }

    _sourceOptionWithFallback(option, preferred, fallback)
    {
        return preferred in option ? option[preferred] : option[fallback];
    }

    _fillCircle(context, cx, cy, radius)
    {
        context.beginPath();
        context.arc(cx, cy, radius, 0, 2 * Math.PI);
        context.fill();
    }

    _ensureFetchedTimeSeries()
    {
        if (this._fetchedTimeSeries)
            return false;

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'ensureFetchedTimeSeries');

        this._fetchedTimeSeries = this._sourceList.map(function (source) {
            return source.measurementSet.fetchedTimeSeries(source.type, source.includeOutliers, source.extendToFuture);
        });

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'ensureFetchedTimeSeries');

        return true;
    }

    _ensureSampledTimeSeries(metrics)
    {
        if (this._sampledTimeSeriesData)
            return false;

        this._ensureFetchedTimeSeries();

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'ensureSampledTimeSeries');

        const startTime = this._startTime;
        const endTime = this._endTime;
        this._sampledTimeSeriesData = this._sourceList.map((source, sourceIndex) => {
            const timeSeries = this._fetchedTimeSeries[sourceIndex];
            if (!timeSeries)
                return null;

            const pointAfterStart = timeSeries.findPointAfterTime(startTime);
            const pointBeforeStart = (pointAfterStart ? timeSeries.previousPoint(pointAfterStart) : null) || timeSeries.firstPoint();
            const pointAfterEnd = timeSeries.findPointAfterTime(endTime) || timeSeries.lastPoint();
            if (!pointBeforeStart || !pointAfterEnd)
                return null;

            // FIXME: Move this to TimeSeries.prototype.
            const view = timeSeries.viewBetweenPoints(pointBeforeStart, pointAfterEnd);
            if (!source.sampleData)
                return view;

            // A chart with X px width shouldn't have more than 2X / <radius-of-points> data points.
            const viewWidth = Math.min(metrics.chartWidth, metrics.timeToX(pointAfterEnd.time) - metrics.timeToX(pointBeforeStart.time));
            const maximumNumberOfPoints = 2 * viewWidth / (source.pointRadius || 2);

            return this._sampleTimeSeries(view, maximumNumberOfPoints, new Set);
        });

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'ensureSampledTimeSeries');

        this.dispatchAction('dataChange');

        return true;
    }

    _sampleTimeSeries(view, maximumNumberOfPoints, excludedPoints)
    {

        if (view.length() < 2 || maximumNumberOfPoints >= view.length() || maximumNumberOfPoints < 1)
            return view;

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'sampleTimeSeries');

        let ranks = new Array(view.length());
        let i = 0;
        for (let point of view) {
            let previousPoint = view.previousPoint(point) || point;
            let nextPoint = view.nextPoint(point) || point;
            ranks[i] = nextPoint.time - previousPoint.time;
            i++;
        }

        const sortedRanks = ranks.slice(0).sort((a, b) => b - a);
        const minimumRank = sortedRanks[Math.floor(maximumNumberOfPoints)];
        const sampledData = view.filter((point, i) => {
            return excludedPoints.has(point.id) || ranks[i] >= minimumRank;
        });

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'sampleTimeSeries');

        Instrumentation.reportMeasurement('TimeSeriesChart', 'samplingRatio', '%', sampledData.length() / view.length() * 100);

        return sampledData;
    }

    _ensureTrendLines()
    {
        if (this._renderedTrendLines)
            return false;
        this._renderedTrendLines = true;
        return true;
    }

    _ensureValueRangeCache()
    {
        if (this._valueRangeCache)
            return false;

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'valueRangeCache');
        var startTime = this._startTime;
        var endTime = this._endTime;

        var min;
        var max;
        for (var seriesData of this._sampledTimeSeriesData) {
            if (!seriesData)
                continue;
            for (let point = seriesData.firstPoint(); point; point = seriesData.nextPoint(point)) {
                var minCandidate = point.interval ? point.interval[0] : point.value;
                var maxCandidate = point.interval ? point.interval[1] : point.value;
                min = (min === undefined) ? minCandidate : Math.min(min, minCandidate);
                max = (max === undefined) ? maxCandidate : Math.max(max, maxCandidate);
            }
        }
        if (min == max)
            max = max * 1.1;
        this._valueRangeCache = [min, max];
        Instrumentation.endMeasuringTime('TimeSeriesChart', 'valueRangeCache');

        return true;
    }

    _updateCanvasSizeIfClientSizeChanged()
    {
        var canvas = this._ensureCanvas();

        var newWidth = this.element().clientWidth;
        var newHeight = this.element().clientHeight;

        if (!newWidth || !newHeight || (newWidth == this._width && newHeight == this._height))
            return false;

        var scale = window.devicePixelRatio;
        canvas.width = newWidth * scale;
        canvas.height = newHeight * scale;
        canvas.style.width = newWidth + 'px';
        canvas.style.height = newHeight + 'px';
        this._contextScaleX = scale;
        this._contextScaleY = scale;
        this._width = newWidth;
        this._height = newHeight;
        this._sampledTimeSeriesData = null;
        this._annotationRows = null;

        return true;
    }

    static computeTimeGrid(min, max, maxLabels)
    {
        const diffPerLabel = (max - min) / maxLabels;

        let iterator;
        for (iterator of this._timeIterators()) {
            if (iterator.diff >= diffPerLabel)
                break;
        }
        console.assert(iterator);

        const currentTime = new Date(min);
        currentTime.setUTCMilliseconds(0);
        currentTime.setUTCSeconds(0);
        currentTime.setUTCMinutes(0);
        iterator.next(currentTime);

        const fitsInOneDay = max - min < 24 * 3600 * 1000;

        let result = [];

        let previousDate = null;
        let previousMonth = null;
        while (currentTime <= max && result.length < maxLabels) {
            const time = new Date(currentTime);
            const month = time.getUTCMonth() + 1;
            const date = time.getUTCDate();
            const hour = time.getUTCHours();
            const hourLabel = ((hour % 12) || 12) + (hour >= 12 ? 'PM' : 'AM');

            iterator.next(currentTime);

            let label;
            const isMidnight = !hour;
            if ((date == previousDate && month == previousMonth) || ((!isMidnight || previousDate == null) && fitsInOneDay))
                label = hourLabel;
            else {
                label = `${month}/${date}`;
                if (!isMidnight && currentTime.getUTCDate() != date)
                    label += ' ' + hourLabel;
            }

            result.push({time: time, label: label});

            previousDate = date;
            previousMonth = month;
        }

        console.assert(result.length <= maxLabels);

        return result;
    }

    static _timeIterators()
    {
        var HOUR = 3600 * 1000;
        var DAY = 24 * HOUR;
        return [
            {
                diff: 2 * HOUR,
                next: function (date) {
                    if (date.getUTCHours() >= 22) {
                        date.setUTCHours(0);
                        date.setUTCDate(date.getUTCDate() + 1);
                    } else
                        date.setUTCHours(Math.floor(date.getUTCHours() / 2) * 2 + 2);
                },
            },
            {
                diff: 12 * HOUR,
                next: function (date) {
                    if (date.getUTCHours() >= 12) {
                        date.setUTCHours(0);
                        date.setUTCDate(date.getUTCDate() + 1);
                    } else
                        date.setUTCHours(12);
                },
            },
            {
                diff: DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    date.setUTCDate(date.getUTCDate() + 1);
                }
            },
            {
                diff: 2 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    date.setUTCDate(date.getUTCDate() + 2);
                }
            },
            {
                diff: 7 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    if (date.getUTCDay())
                        date.setUTCDate(date.getUTCDate() + (7 - date.getUTCDay()));
                    else
                        date.setUTCDate(date.getUTCDate() + 7);
                }
            },
            {
                diff: 16 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    if (date.getUTCDate() >= 15) {
                        date.setUTCMonth(date.getUTCMonth() + 1);
                        date.setUTCDate(1);
                    } else
                        date.setUTCDate(15);
                }
            },
            {
                diff: 31 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    const dayOfMonth = date.getUTCDate();
                    if (dayOfMonth > 1 && dayOfMonth < 15)
                        date.setUTCDate(15);
                    else {
                        if (dayOfMonth != 15)
                            date.setUTCDate(1);
                        date.setUTCMonth(date.getUTCMonth() + 1);
                    }
                }
            },
            {
                diff: 60 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    date.setUTCDate(1);
                    date.setUTCMonth(date.getUTCMonth() + 2);
                }
            },
            {
                diff: 90 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    date.setUTCDate(1);
                    date.setUTCMonth(date.getUTCMonth() + 3);
                }
            },
            {
                diff: 120 * DAY,
                next: function (date) {
                    date.setUTCHours(0);
                    date.setUTCDate(1);
                    date.setUTCMonth(date.getUTCMonth() + 4);
                }
            },
        ];
    }

    static computeValueGrid(min, max, maxLabels, formatter)
    {
        const diff = max - min;
        if (!diff)
            return [];

        const diffPerLabel = diff / maxLabels;

        // First, reduce the diff between 1 and 1000 using a power of 1000 or 1024.
        // FIXME: Share this code with Metric.makeFormatter.
        const maxAbsValue = Math.max(Math.abs(min), Math.abs(max));
        let scalingFactor = 1;
        const divisor = formatter.divisor;
        while (maxAbsValue * scalingFactor < 1)
            scalingFactor *= formatter.divisor;
        while (maxAbsValue * scalingFactor > divisor)
            scalingFactor /= formatter.divisor;
        const scaledDiff = diffPerLabel * scalingFactor;

        // Second, compute the smallest number greater than the scaled diff
        // which is a product of a power of 10, 2, and 5.
        // These numbers are all factors of the decimal numeral system, 10.
        const digitsInScaledDiff = Math.ceil(Math.log(scaledDiff) / Math.log(10));
        let step = Math.pow(10, digitsInScaledDiff);
        if (step / 5 >= scaledDiff)
            step /= 5; // The most significant digit is 2
        else if (step / 2 >= scaledDiff)
            step /= 2 // The most significant digit is 5
        step /= scalingFactor;

        const gridValues = [];
        let currentValue = Math.ceil(min / step) * step;
        while (currentValue <= max) {
            let unscaledValue = currentValue;
            gridValues.push({value: unscaledValue, label: formatter(unscaledValue, maxAbsValue)});
            currentValue += step;
        }

        return gridValues;
    }
}

ComponentBase.defineElement('time-series-chart', TimeSeriesChart);
