
class InteractiveTimeSeriesChart extends TimeSeriesChart {
    constructor(sourceList, options)
    {
        super(sourceList, options);
        this._indicatorID = null;
        this._indicatorIsLocked = false;
        this._currentAnnotation = null;
        this._forceRender = false;
        this._lastMouseDownLocation = null;
        this._dragStarted = false;
        this._didEndDrag = false;
        this._selectionTimeRange = null;
        this._renderedSelection = null;
        this._annotationLabel = null;
        this._renderedAnnotation = null;
    }

    currentPoint(diff)
    {
        if (!this._sampledTimeSeriesData)
            return null;

        var id = this._indicatorID;
        if (!id)
            return null;

        for (var data of this._sampledTimeSeriesData) {
            if (!data)
                continue;
            var index = data.findIndex(function (point) { return point.id == id; });
            if (index < 0)
                continue;
            if (diff)
                index += diff;
            return data[Math.min(Math.max(0, index), data.length)];
        }
        return null;
    }

    currentSelection() { return this._selectionTimeRange; }

    setIndicator(id, shouldLock)
    {
        var selectionDidChange = !!this._sampledTimeSeriesData;

        this._indicatorID = id;
        this._indicatorIsLocked = shouldLock;

        this._lastMouseDownLocation = null;
        this._selectionTimeRange = null;
        this._forceRender = true;

        if (selectionDidChange)
            this._notifySelectionChanged();
    }

    moveLockedIndicatorWithNotification(forward)
    {
        if (!this._indicatorID || !this._indicatorIsLocked)
            return false;

        console.assert(!this._selectionTimeRange);

        var point = this.currentPoint(forward ? 1 : -1);
        if (!point || this._indicatorID == point.id)
            return false;

        this._indicatorID = point.id;
        this._lastMouseDownLocation = null;
        this._forceRender = true;

        this._notifyIndicatorChanged();
    }

    setSelection(newSelectionTimeRange)
    {
        var indicatorDidChange = !!this._indicatorID;
        this._indicatorID = null;
        this._indicatorIsLocked = false;

        this._lastMouseDownLocation = null;
        this._selectionTimeRange = newSelectionTimeRange;
        this._forceRender = true;

        if (indicatorDidChange)
            this._notifyIndicatorChanged();
    }

    _createCanvas()
    {
        var canvas = super._createCanvas();
        canvas.addEventListener('mousemove', this._mouseMove.bind(this));
        canvas.addEventListener('mouseleave', this._mouseLeave.bind(this));
        canvas.addEventListener('mousedown', this._mouseDown.bind(this));
        window.addEventListener('mouseup', this._mouseUp.bind(this));
        canvas.addEventListener('click', this._click.bind(this));

        this._annotationLabel = this.content().querySelector('.time-series-chart-annotation-label');
        this._zoomButton = this.content().querySelector('.time-series-chart-zoom-button');

        var self = this;
        this._zoomButton.onclick = function (event) {
            event.preventDefault();
            if (self._options.selection && self._options.selection.onzoom)
                self._options.selection.onzoom(self._selectionTimeRange);
        }

        return canvas;
    }

    static htmlTemplate()
    {
        return `
            <a href="#" title="Zoom" class="time-series-chart-zoom-button" style="display:none;">
                <svg viewBox="0 0 100 100">
                    <g stroke-width="0" stroke="none">
                        <polygon points="25,25 5,50 25,75"/>
                        <polygon points="75,25 95,50 75,75"/>
                    </g>
                    <line x1="20" y1="50" x2="80" y2="50" stroke-width="10"></line>
                </svg>
            </a>
            <span class="time-series-chart-annotation-label" style="display:none;"></span>
        `;
    }

    static cssTemplate()
    {
        return TimeSeriesChart.cssTemplate() + `
            .time-series-chart-zoom-button {
                position: absolute;
                left: 0;
                top: 0;
                width: 1rem;
                height: 1rem;
                display: block;
                background: rgba(255, 255, 255, 0.8);
                -webkit-backdrop-filter: blur(0.3rem);
                stroke: #666;
                fill: #666;
                border: solid 1px #ccc;
                border-radius: 0.2rem;
                z-index: 20;
            }

            .time-series-chart-annotation-label {
                position: absolute;
                left: 0;
                top: 0;
                display: inline;
                background: rgba(255, 255, 255, 0.8);
                -webkit-backdrop-filter: blur(0.5rem);
                color: #000;
                border: solid 1px #ccc;
                border-radius: 0.2rem;
                padding: 0.2rem;
                font-size: 0.8rem;
                font-weight: normal;
                line-height: 0.9rem;
                z-index: 10;
                max-width: 15rem;
            }
        `;
    }

    _mouseMove(event)
    {
        var cursorLocation = {x: event.offsetX, y: event.offsetY};
        if (this._startOrContinueDragging(cursorLocation) || this._selectionTimeRange)
            return;

        if (this._indicatorIsLocked)
            return;

        var oldIndicatorID = this._indicatorID;

        this._currentAnnotation = this._findAnnotation(cursorLocation);
        if (this._currentAnnotation)
            this._indicatorID = null;
        else
            this._indicatorID = this._findClosestPoint(cursorLocation);

        this._forceRender = true;
        this._notifyIndicatorChanged();
    }

    _mouseLeave(event)
    {
        if (this._selectionTimeRange || this._indicatorIsLocked || !this._indicatorID)
            return;

        this._indicatorID = null;
        this._forceRender = true;
        this._notifyIndicatorChanged();
    }

    _mouseDown(event)
    {
        this._lastMouseDownLocation = {x: event.offsetX, y: event.offsetY};
    }

    _mouseUp(event)
    {
        if (this._dragStarted)
            this._endDragging({x: event.offsetX, y: event.offsetY});
    }

    _click(event)
    {
        if (this._selectionTimeRange) {
            if (!this._didEndDrag) {
                this._lastMouseDownLocation = null;
                this._selectionTimeRange = null;
                this._forceRender = true;
                this._notifySelectionChanged(true);
                this._mouseMove(event);
            }
            return;
        }

        this._lastMouseDownLocation = null;

        var cursorLocation = {x: event.offsetX, y: event.offsetY};
        var annotation = this._findAnnotation(cursorLocation);
        if (annotation) {
            if (this._options.annotations.onclick)
                this._options.annotations.onclick(annotation);
            return;
        }

        this._indicatorIsLocked = !this._indicatorIsLocked;
        this._indicatorID = this._findClosestPoint(cursorLocation);
        this._forceRender = true;

        this._notifyIndicatorChanged();
    }

    _startOrContinueDragging(cursorLocation, didEndDrag)
    {
        var mouseDownLocation = this._lastMouseDownLocation;
        if (!mouseDownLocation || !this._options.selection)
            return false;

        var xDiff = mouseDownLocation.x - cursorLocation.x;
        var yDiff = mouseDownLocation.y - cursorLocation.y;
        if (!this._dragStarted && xDiff * xDiff + yDiff * yDiff < 10)
            return false;
        this._dragStarted = true;

        var indicatorDidChange = !!this._indicatorID;
        this._indicatorID = null;
        this._indicatorIsLocked = false;

        var metrics = this._layout();

        var oldSelection = this._selectionTimeRange;
        if (!didEndDrag) {
            var selectionStart = Math.min(mouseDownLocation.x, cursorLocation.x);
            var selectionEnd = Math.max(mouseDownLocation.x, cursorLocation.x);
            this._selectionTimeRange = [metrics.xToTime(selectionStart), metrics.xToTime(selectionEnd)];
        }
        this._forceRender = true;

        if (indicatorDidChange)
            this._notifyIndicatorChanged();

        var selectionDidChange = !oldSelection ||
            oldSelection[0] != this._selectionTimeRange[0] || oldSelection[1] != this._selectionTimeRange[1];
        if (selectionDidChange || didEndDrag)
            this._notifySelectionChanged(didEndDrag);

        return true;
    }

    _endDragging(cursorLocation)
    {
        if (!this._startOrContinueDragging(cursorLocation, true))
            return;
        this._dragStarted = false;
        this._lastMouseDownLocation = null;
        this._didEndDrag = true;
        var self = this;
        setTimeout(function () { self._didEndDrag = false; }, 0);
    }

    _notifyIndicatorChanged()
    {
        if (this._options.indicator && this._options.indicator.onchange)
            this._options.indicator.onchange(this._indicatorID, this._indicatorIsLocked);
    }

    _notifySelectionChanged(didEndDrag)
    {
        if (this._options.selection && this._options.selection.onchange)
            this._options.selection.onchange(this._selectionTimeRange, didEndDrag);
    }

    _findAnnotation(cursorLocation)
    {
        if (!this._annotations)
            return null;

        for (var item of this._annotations) {
            if (item.x <= cursorLocation.x && cursorLocation.x <= item.x + item.width
                && item.y <= cursorLocation.y && cursorLocation.y <= item.y + item.height)
                return item;
        }
        return null;
    }

    _findClosestPoint(cursorLocation)
    {
        Instrumentation.startMeasuringTime('InteractiveTimeSeriesChart', 'findClosestPoint');

        var metrics = this._layout();

        function weightedDistance(point) {
            var x = metrics.timeToX(point.time);
            var y = metrics.valueToY(point.value);
            var xDiff = cursorLocation.x - x;
            var yDiff = cursorLocation.y - y;
            return xDiff * xDiff + yDiff * yDiff / 16; // Favor horizontal affinity.
        }

        var minDistance;
        var minPoint = null;
        for (var i = 0; i < this._sampledTimeSeriesData.length; i++) {
            var series = this._sampledTimeSeriesData[i];
            var source = this._sourceList[i];
            if (!series || !source.interactive)
                continue;
            for (var point of series) {
                var distance = weightedDistance(point);
                if (minDistance === undefined || distance < minDistance) {
                    minDistance = distance;
                    minPoint = point;
                }
            }
        }

        Instrumentation.endMeasuringTime('InteractiveTimeSeriesChart', 'findClosestPoint');

        return minPoint ? minPoint.id : null;
    }

    _layout()
    {
        var metrics = super._layout();
        metrics.doneWork |= this._forceRender;
        this._forceRender = false;
        this._lastRenderigMetrics = metrics;
        return metrics;
    }

    _sampleTimeSeries(data, maximumNumberOfPoints, exclusionPointID)
    {
        console.assert(!exclusionPointID);
        return super._sampleTimeSeries(data, maximumNumberOfPoints, this._indicatorID);
    }

    _renderChartContent(context, metrics)
    {
        super._renderChartContent(context, metrics);

        Instrumentation.startMeasuringTime('InteractiveTimeSeriesChart', 'renderChartContent');

        context.lineJoin = 'miter';

        if (this._renderedAnnotation != this._currentAnnotation) {
            this._renderedAnnotation = this._currentAnnotation;

            var annotation = this._currentAnnotation;
            if (annotation) {
                var label = annotation.label;
                var spacing = this._options.annotations.minWidth;

                this._annotationLabel.textContent = label;
                if (this._annotationLabel.style.display != 'inline')
                    this._annotationLabel.style.display = 'inline';

                // Force a browser layout.
                var labelWidth = this._annotationLabel.offsetWidth;
                var labelHeight = this._annotationLabel.offsetHeight;

                var centerX = annotation.x + annotation.width / 2 - labelWidth / 2;
                var maxX = metrics.chartX + metrics.chartWidth - labelWidth - 2;

                var x = Math.round(Math.min(maxX, Math.max(metrics.chartX + 2, centerX)));
                var y = Math.floor(annotation.y - labelHeight - 1);

                // Use transform: translate to position the label to avoid triggering another browser layout.
                this._annotationLabel.style.transform = `translate(${x}px, ${y}px)`;
            } else
                this._annotationLabel.style.display = 'none';
        }

        var indicator = this._options.indicator;
        if (this._indicatorID && indicator) {
            context.fillStyle = indicator.lineStyle;
            context.strokeStyle = indicator.lineStyle;
            context.lineWidth = indicator.lineWidth;

            var point = this.currentPoint();
            if (point) {
                var x = metrics.timeToX(point.time);
                var y = metrics.valueToY(point.value);

                context.beginPath();
                context.moveTo(x, metrics.chartY);
                context.lineTo(x, metrics.chartY + metrics.chartHeight);
                context.stroke();

                this._fillCircle(context, x, y, indicator.pointRadius);
            }
        }

        var selectionOptions = this._options.selection;
        var selectionX2 = 0;
        var selectionY2 = 0;
        if (this._selectionTimeRange && selectionOptions) {
            context.fillStyle = selectionOptions.fillStyle;
            context.strokeStyle = selectionOptions.lineStyle;
            context.lineWidth = selectionOptions.lineWidth;

            var x1 = metrics.timeToX(this._selectionTimeRange[0]);
            var x2 = metrics.timeToX(this._selectionTimeRange[1]);
            context.beginPath();
            selectionX2 = x2;
            selectionY2 = metrics.chartHeight - selectionOptions.lineWidth;
            context.rect(x1, metrics.chartY + selectionOptions.lineWidth / 2,
                x2 - x1, metrics.chartHeight - selectionOptions.lineWidth);
            context.fill();
            context.stroke();
        }
    
        if (this._renderedSelection != selectionX2) {
            this._renderedSelection = selectionX2;
            if (this._renderedSelection && selectionOptions && selectionOptions.onzoom
                && selectionX2 > 0 && selectionX2 < metrics.chartX + metrics.chartWidth) {
                if (this._zoomButton.style.display)
                    this._zoomButton.style.display = null;

                this._zoomButton.style.left = Math.round(selectionX2 + metrics.fontSize / 4) + 'px';
                this._zoomButton.style.top = Math.floor(selectionY2 - metrics.fontSize * 1.5 - 2) + 'px';
            } else
                this._zoomButton.style.display = 'none';
        }

        Instrumentation.endMeasuringTime('InteractiveTimeSeriesChart', 'renderChartContent');
    }
}
