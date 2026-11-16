
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

    currentIndicator()
    {
        var id = this._indicatorID;
        if (!id)
            return null;

        if (!this._sampledTimeSeriesData)
            return null;

        for (var view of this._sampledTimeSeriesData) {
            if (!view)
                continue;
            let point = view.findById(id);
            if (!point)
                continue;
            return {view, point, isLocked: this._indicatorIsLocked};
        }
        return null;
    }

    currentSelection() { return this._selectionTimeRange; }

    selectedPoints(type)
    {
        const selection = this._selectionTimeRange;
        const data = this.sampledTimeSeriesData(type);
        return selection && data ? data.viewTimeRange(selection[0], selection[1]) : null;
    }

    firstSelectedPoint(type)
    {
        const selection = this._selectionTimeRange;
        const data = this.sampledTimeSeriesData(type);
        return selection && data ? data.firstPointInTimeRange(selection[0], selection[1]) : null;
    }

    referencePoints(type)
    {
        const selection = this.currentSelection();
        if (selection) {
            const view = this.selectedPoints('current');
            if (!view)
                return null;
            const firstPoint = view.lastPoint();
            const lastPoint = view.firstPoint();
            if (!firstPoint)
                return null;
            return {view, currentPoint: firstPoint, previousPoint: firstPoint != lastPoint ? lastPoint : null};
        } else  {
            const indicator = this.currentIndicator();
            if (!indicator)
                return null;
            return {view: indicator.view, currentPoint: indicator.point, previousPoint: indicator.view.previousPoint(indicator.point)};
        }
        return null;
    }

    setIndicator(id, shouldLock)
    {
        var selectionDidChange = !!this._sampledTimeSeriesData;

        this._indicatorID = id;
        this._indicatorIsLocked = shouldLock;

        this._lastMouseDownLocation = null;
        this._selectionTimeRange = null;
        this._forceRender = true;

        if (selectionDidChange)
            this._notifySelectionChanged(false);
    }

    moveLockedIndicatorWithNotification(forward)
    {
        const indicator = this.currentIndicator();
        if (!indicator || !indicator.isLocked)
            return false;
        console.assert(!this._selectionTimeRange);

        const constrainedView = indicator.view.viewTimeRange(this._startTime, this._endTime);
        const newPoint = forward ? constrainedView.nextPoint(indicator.point) : constrainedView.previousPoint(indicator.point);
        if (!newPoint || this._indicatorID == newPoint.id)
            return false;

        this._indicatorID = newPoint.id;
        this._lastMouseDownLocation = null;
        this._forceRender = true;

        this.enqueueToRender();
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

        this._annotationLabel = this.content('annotation-label');
        this._zoomButton = this.content('zoom-button');

        this._zoomButton.onclick = (event) => {
            event.preventDefault();
            this.dispatchAction('zoom', this._selectionTimeRange);
        }

        return canvas;
    }

    static htmlTemplate()
    {
        return `
            <a href="#" title="Zoom" id="zoom-button" style="display:none;">
                <svg viewBox="0 0 100 100">
                    <g stroke-width="0" stroke="none">
                        <polygon points="25,25 5,50 25,75"/>
                        <polygon points="75,25 95,50 75,75"/>
                    </g>
                    <line x1="20" y1="50" x2="80" y2="50" stroke-width="10"></line>
                </svg>
            </a>
            <span id="annotation-label" style="display:none;"></span>
        `;
    }

    static cssTemplate()
    {
        return TimeSeriesChart.cssTemplate() + `
            #zoom-button {
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

            #annotation-label {
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
        if (this._startOrContinueDragging(cursorLocation, false) || this._selectionTimeRange)
            return;

        if (this._indicatorIsLocked)
            return;

        var oldIndicatorID = this._indicatorID;

        var newAnnotation = this._findAnnotation(cursorLocation);
        var newIndicatorID = null;
        if (this._currentAnnotation)
            newIndicatorID = null;
        else
            newIndicatorID = this._findClosestPoint(cursorLocation);

        this._forceRender = true;
        this.enqueueToRender();

        if (this._currentAnnotation == newAnnotation && this._indicatorID == newIndicatorID)
            return;

        this._currentAnnotation = newAnnotation;
        this._indicatorID = newIndicatorID;

        this._notifyIndicatorChanged();
    }

    _mouseLeave(event)
    {
        if (this._selectionTimeRange || this._indicatorIsLocked || !this._indicatorID)
            return;

        this._indicatorID = null;
        this._forceRender = true;
        this.enqueueToRender();
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
                this._notifySelectionChanged(true);
                this._mouseMove(event);
            }
            return;
        }

        this._lastMouseDownLocation = null;

        var cursorLocation = {x: event.offsetX, y: event.offsetY};
        var annotation = this._findAnnotation(cursorLocation);
        if (annotation) {
            this.dispatchAction('annotationClick', annotation);
            return;
        }

        this._indicatorIsLocked = !this._indicatorIsLocked;
        this._indicatorID = this._findClosestPoint(cursorLocation);
        this._forceRender = true;
        this.enqueueToRender();

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
        this.enqueueToRender();

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
        setTimeout(() => this._didEndDrag = false, 0);
    }

    _notifyIndicatorChanged()
    {
        this.dispatchAction('indicatorChange', this._indicatorID, this._indicatorIsLocked);
    }

    _notifySelectionChanged(didEndDrag)
    {
        this.dispatchAction('selectionChange', this._selectionTimeRange, didEndDrag);
    }

    _findAnnotation(cursorLocation)
    {
        if (!this._annotations)
            return null;

        for (const item of this._annotations) {
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
        return metrics;
    }

    _sampleTimeSeries(data, maximumNumberOfPoints, excludedPoints)
    {
        if (this._indicatorID)
            excludedPoints.add(this._indicatorID);
        return super._sampleTimeSeries(data, maximumNumberOfPoints, excludedPoints);
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

        const indicator = this.currentIndicator();
        const indicatorOptions = (indicator && indicator.isLocked ? this._options.lockedIndicator : null) || this._options.indicator;
        if (indicator && indicatorOptions) {
            context.fillStyle = indicatorOptions.fillStyle || indicatorOptions.lineStyle;
            context.strokeStyle = indicatorOptions.lineStyle;
            context.lineWidth = indicatorOptions.lineWidth;

            const x = metrics.timeToX(indicator.point.time);
            const y = metrics.valueToY(indicator.point.value);

            context.beginPath();
            context.moveTo(x, metrics.chartY);
            context.lineTo(x, metrics.chartY + metrics.chartHeight);
            context.stroke();

            this._fillCircle(context, x, y, indicatorOptions.pointRadius);
            context.stroke();
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
            if (this._renderedSelection && this._options.zoomButton
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

ComponentBase.defineElement('interactive-time-series-chart', InteractiveTimeSeriesChart);
