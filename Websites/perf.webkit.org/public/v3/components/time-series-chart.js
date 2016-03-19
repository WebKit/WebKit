
class TimeSeriesChart extends ComponentBase {
    constructor(sourceList, options)
    {
        super('time-series-chart');
        this.element().style.display = 'block';
        this.element().style.position = 'relative';
        this._canvas = null;
        this._sourceList = sourceList;
        this._options = options;
        this._fetchedTimeSeries = null;
        this._sampledTimeSeriesData = null;
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

        if (this._options.updateOnRequestAnimationFrame) {
            if (!TimeSeriesChart._chartList)
                TimeSeriesChart._chartList = [];
            TimeSeriesChart._chartList.push(this);
            TimeSeriesChart._updateOnRAF();
        }
    }

    _ensureCanvas()
    {
        if (!this._canvas) {
            this._canvas = this._createCanvas();
            this._canvas.style.display = 'block';
            this._canvas.style.position = 'absolute';
            this._canvas.style.left = '0px';
            this._canvas.style.top = '0px';
            this._canvas.style.width = '100%';
            this._canvas.style.height = '100%';
            this.content().appendChild(this._canvas);
        }
        return this._canvas;
    }

    static cssTemplate() { return ''; }

    _createCanvas()
    {
        return document.createElement('canvas');
    }

    static _updateOnRAF()
    {
        var self = this;
        window.requestAnimationFrame(function ()
        {
            TimeSeriesChart._chartList.map(function (chart) { chart.render(); });
            self._updateOnRAF();
        });
    }

    setDomain(startTime, endTime)
    {
        console.assert(startTime < endTime, 'startTime must be before endTime');
        this._startTime = startTime;
        this._endTime = endTime;
        for (var source of this._sourceList) {
            if (source.measurementSet)
                source.measurementSet.fetchBetween(startTime, endTime, this._didFetchMeasurementSet.bind(this, source.measurementSet));
        }
        this._sampledTimeSeriesData = null;
        this._valueRangeCache = null;
        this._annotationRows = null;
    }

    _didFetchMeasurementSet(set)
    {
        this._fetchedTimeSeries = null;
        this._sampledTimeSeriesData = null;
        this._valueRangeCache = null;
        this._annotationRows = null;
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

    sampledDataBetween(type, startTime, endTime)
    {
        var data = this.sampledTimeSeriesData(type);
        if (!data)
            return null;
        return data.filter(function (point) { return startTime <= point.time && point.time <= endTime; });
    }

    setAnnotations(annotations)
    {
        this._annotations = annotations;
        this._annotationRows = null;
    }

    render()
    {
        if (!this._startTime || !this._endTime)
            return;

        // FIXME: Also detect horizontal scrolling.
        var canvas = this._ensureCanvas();
        if (!TimeSeriesChart.isElementInViewport(canvas))
            return;

        var metrics = this._layout();
        if (!metrics.doneWork)
            return;

        Instrumentation.startMeasuringTime('TimeSeriesChart', 'render');

        var context = canvas.getContext('2d');
        context.scale(this._contextScaleX, this._contextScaleY);

        context.clearRect(0, 0, this._width, this._height);

        context.font = metrics.fontSize + 'px sans-serif';
        context.fillStyle = this._options.axis.fillStyle;
        context.strokeStyle = this._options.axis.gridStyle;
        context.lineWidth = 1 / this._contextScaleY;

        this._renderXAxis(context, metrics, this._startTime, this._endTime);
        this._renderYAxis(context, metrics, this._valueRangeCache[0], this._valueRangeCache[1]);

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

        var fontSize = this._options.axis.fontSize * this._rem;
        var chartX = this._options.axis.yAxisWidth * fontSize;
        var chartY = 0;
        var chartWidth = this._width - chartX;
        var chartHeight = this._height - this._options.axis.xAxisHeight * fontSize;

        if (this._options.axis.xAxisEndPadding)
            timeDiff += this._options.axis.xAxisEndPadding / (chartWidth / timeDiff);

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
        var maxYAxisLabels = Math.floor(metrics.chartHeight / metrics.fontSize / 2);
        var yAxisGrid = TimeSeriesChart.computeValueGrid(minValue, maxValue, maxYAxisLabels);

        for (var value of yAxisGrid) {
            context.beginPath();
            var y = metrics.valueToY(value);
            context.moveTo(metrics.chartX, y);
            context.lineTo(metrics.chartX + metrics.chartWidth, y);
            context.stroke();
        }

        if (!this._options.axis.yAxisWidth)
            return;

        for (var value of yAxisGrid) {
            var label = this._options.axis.valueFormatter(value);
            var x = (metrics.chartX - context.measureText(label).width) / 2;

            var y = metrics.valueToY(value) + metrics.fontSize / 2.5;
            if (y < metrics.fontSize)
                y = metrics.fontSize;

            context.fillText(label, x, y);
        }
    }

    _renderChartContent(context, metrics)
    {
        context.lineJoin = 'round';
        for (var i = 0; i < this._sourceList.length; i++) {
            var source = this._sourceList[i];
            var series = this._sampledTimeSeriesData[i];
            if (series)
                this._renderTimeSeries(context, metrics, source, series);
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

    _renderTimeSeries(context, metrics, source, series)
    {
        for (var point of series) {
            point.x = metrics.timeToX(point.time);
            point.y = metrics.valueToY(point.value);
        }

        context.strokeStyle = source.intervalStyle;
        context.fillStyle = source.intervalStyle;
        context.lineWidth = source.intervalWidth;
        for (var i = 0; i < series.length; i++) {
            var point = series[i];
            if (!point.interval)
                continue;
            context.beginPath();
            context.moveTo(point.x, metrics.valueToY(point.interval[0]))
            context.lineTo(point.x, metrics.valueToY(point.interval[1]));
            context.stroke();
        }

        context.strokeStyle = source.lineStyle;
        context.lineWidth = source.lineWidth;
        context.beginPath();
        for (var point of series)
            context.lineTo(point.x, point.y);
        context.stroke();

        context.fillStyle = source.pointStyle;
        var radius = source.pointRadius;
        for (var point of series)
            this._fillCircle(context, point.x, point.y, radius);
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

        var self = this;
        var startTime = this._startTime;
        var endTime = this._endTime;
        this._sampledTimeSeriesData = this._sourceList.map(function (source, sourceIndex) {
            var timeSeries = self._fetchedTimeSeries[sourceIndex];
            if (!timeSeries)
                return null;

            // A chart with X px width shouldn't have more than 2X / <radius-of-points> data points.
            var maximumNumberOfPoints = 2 * metrics.chartWidth / source.pointRadius;

            var pointAfterStart = timeSeries.findPointAfterTime(startTime);
            var pointBeforeStart = (pointAfterStart ? timeSeries.previousPoint(pointAfterStart) : null) || timeSeries.firstPoint();
            var pointAfterEnd = timeSeries.findPointAfterTime(endTime) || timeSeries.lastPoint();
            if (!pointBeforeStart || !pointAfterEnd)
                return null;

            // FIXME: Move this to TimeSeries.prototype.
            var filteredData = timeSeries.dataBetweenPoints(pointBeforeStart, pointAfterEnd);
            if (!source.sampleData)
                return filteredData;
            else
                return self._sampleTimeSeries(filteredData, maximumNumberOfPoints);
        });

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'ensureSampledTimeSeries');

        if (this._options.ondata)
            this._options.ondata();

        return true;
    }

    _sampleTimeSeries(data, maximumNumberOfPoints, exclusionPointID)
    {
        Instrumentation.startMeasuringTime('TimeSeriesChart', 'sampleTimeSeries');

        // FIXME: Do this in O(n) using quickselect: https://en.wikipedia.org/wiki/Quickselect
        function findMedian(list, startIndex, indexAfterEnd)
        {
            var sortedList = list.slice(startIndex, indexAfterEnd).sort(function (a, b) { return a.value - b.value; });
            return sortedList[Math.floor(sortedList.length / 2)];
        }

        var samplingSize = Math.ceil(data.length / maximumNumberOfPoints);

        var totalTimeDiff = data[data.length - 1].time - data[0].time;
        var timePerSample = totalTimeDiff / maximumNumberOfPoints;

        var sampledData = [];
        var lastIndex = data.length - 1;
        var i = 0;
        while (i <= lastIndex) {
            var startPoint = data[i];
            var j;
            for (j = i; j <= lastIndex; j++) {
                var endPoint = data[j];
                if (endPoint.id == exclusionPointID) {
                    j--;
                    break;
                }
                if (endPoint.time - startPoint.time >= timePerSample)
                    break;
            }
            if (i < j - 1) {
                sampledData.push(findMedian(data, i, j));
                i = j;
            } else {
                sampledData.push(startPoint);
                i++;
            }
        }

        Instrumentation.endMeasuringTime('TimeSeriesChart', 'sampleTimeSeries');

        Instrumentation.reportMeasurement('TimeSeriesChart', 'samplingRatio', '%', sampledData.length / data.length * 100);

        return sampledData;
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
            for (var point of seriesData) {
                var minCandidate = point.interval ? point.interval[0] : point.value;
                var maxCandidate = point.interval ? point.interval[1] : point.value;
                min = (min === undefined) ? minCandidate : Math.min(min, minCandidate);
                max = (max === undefined) ? maxCandidate : Math.max(max, maxCandidate);
            }
        }
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
        var diffPerLabel = (max - min) / maxLabels;

        var iterator;
        for (iterator of this._timeIterators()) {
            if (iterator.diff > diffPerLabel)
                break;
        }
        console.assert(iterator);

        var currentTime = new Date(min);
        currentTime.setUTCMilliseconds(0);
        currentTime.setUTCSeconds(0);
        currentTime.setUTCMinutes(0);
        iterator.next(currentTime);

        var result = [];

        var previousDate = null;
        var previousMonth = null;
        var previousHour = null;
        while (currentTime <= max) {
            var time = new Date(currentTime);
            var month = (time.getUTCMonth() + 1);
            var date = time.getUTCDate();
            var hour = time.getUTCHours();
            var hourLabel = (hour > 12 ? hour - 12 : hour) + (hour >= 12 ? 'PM' : 'AM');

            iterator.next(currentTime);

            var label;
            if (date == previousDate && month == previousMonth)
                label = hourLabel;
            else {
                label = `${month}/${date}`;
                if (hour && currentTime.getUTCDate() != date)
                    label += ' ' + hourLabel;
            }

            result.push({time: time, label: label});

            previousDate = date;
            previousMonth = month;
            previousHour = hour;
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
                    date.setUTCMonth(date.getUTCMonth() + 1);
                }
            },
        ];
    }

    static computeValueGrid(min, max, maxLabels)
    {
        var diff = max - min;
        var scalingFactor = 1;
        var diffPerLabel = diff / maxLabels;
        if (diffPerLabel < 1) {
            scalingFactor = Math.pow(10, Math.ceil(-Math.log(diffPerLabel) / Math.log(10)));
            min *= scalingFactor;
            max *= scalingFactor;
            diff *= scalingFactor;
            diffPerLabel *= scalingFactor;
        }
        diffPerLabel = Math.ceil(diffPerLabel);
        var numberOfDigitsToIgnore = Math.ceil(Math.log(diffPerLabel) / Math.log(10));
        var step = Math.pow(10, numberOfDigitsToIgnore);

        if (diff / (step / 5) < maxLabels) // 0.2, 0.4, etc...
            step /= 5;
        else if (diff / (step / 2) < maxLabels) // 0.5, 1, 1.5, etc...
            step /= 2;

        var gridValues = [];
        var currentValue = Math.ceil(min / step) * step;
        while (currentValue <= max) {
            gridValues.push(currentValue / scalingFactor);
            currentValue += step;
        }
        return gridValues;
    }
}
