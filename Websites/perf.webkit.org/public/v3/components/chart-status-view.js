
class ChartStatusView extends ComponentBase {

    constructor(metric, chart)
    {
        super('chart-status');
        this._metric = metric;
        this._chart = chart;

        this._usedSelection = null;
        this._usedCurrentPoint = null;
        this._usedPreviousPoint = null;

        this._currentValue = null;
        this._comparisonClass = null;
        this._comparisonLabel = null;

        this._renderedCurrentValue = null;
        this._renderedComparisonClass = null;
        this._renderedComparisonLabel = null;
    }

    render()
    {
        this.updateStatusIfNeeded();

        if (this._renderedCurrentValue == this._currentValue
            && this._renderedComparisonClass == this._comparisonClass
            && this._renderedComparisonLabel == this._comparisonLabel)
            return;

        this._renderedCurrentValue = this._currentValue;
        this._renderedComparisonClass = this._comparisonClass;
        this._renderedComparisonLabel = this._comparisonLabel;

        this.content().querySelector('.chart-status-current-value').textContent = this._currentValue || '';
        var comparison = this.content().querySelector('.chart-status-comparison');
        comparison.className = 'chart-status-comparison ' + (this._comparisonClass || '');
        comparison.textContent = this._comparisonLabel;
    }

    updateStatusIfNeeded()
    {
        var currentPoint;
        var previousPoint;

        if (this._chart instanceof InteractiveTimeSeriesChart) {
            var selection = this._chart.currentSelection();
            if (selection && this._usedSelection == selection)
                return false;

            if (selection) {
                const view = this._chart.selectedPoints('current');
                if (!view)
                    return false;

                if (view && view.length() > 1) {
                    this._usedSelection = selection;
                    currentPoint = view.lastPoint();
                    previousPoint = view.firstPoint();
                }
            } else  {
                currentPoint = this._chart.currentPoint();
                previousPoint = this._chart.currentPoint(-1);
            }
        } else {
            var data = this._chart.sampledTimeSeriesData('current');
            if (!data)
                return false;
            if (data.length)
                currentPoint = data[data.length - 1];
        }

        if (currentPoint == this._usedCurrentPoint && previousPoint == this._usedPreviousPoint)
            return false;

        this._usedCurrentPoint = currentPoint;
        this._usedPreviousPoint = previousPoint;

        this.computeChartStatusLabels(currentPoint, previousPoint);

        return true;
    }

    computeChartStatusLabels(currentPoint, previousPoint)
    {
        var status = currentPoint ? this._computeChartStatus(this._metric, this._chart, currentPoint, previousPoint) : null;
        if (status) {
            this._currentValue = status.currentValue;
            if (previousPoint)
                this._currentValue += ` (${status.valueDelta} / ${status.relativeDelta})`;
            this._comparisonClass = status.className;
            this._comparisonLabel = status.label;
        } else {
            this._currentValue = null;
            this._comparisonClass = null;
            this._comparisonLabel = null;
        }
    }

    _computeChartStatus(metric, chart, currentPoint, previousPoint)
    {
        var currentTimeSeriesData = chart.sampledTimeSeriesData('current');
        var baselineTimeSeriesData = chart.sampledTimeSeriesData('baseline');
        var targetTimeSeriesData = chart.sampledTimeSeriesData('target');
        if (!currentTimeSeriesData)
            return null;

        var formatter = metric.makeFormatter(3);
        var deltaFormatter = metric.makeFormatter(2, true);

        if (!currentPoint)
            currentPoint = currentTimeSeriesData[currentTimeSeriesData.length - 1];

        var baselinePoint = this._findLastPointPriorToTime(currentPoint, baselineTimeSeriesData);
        var diffFromBaseline = baselinePoint !== undefined ? (currentPoint.value - baselinePoint.value) / baselinePoint.value : undefined;

        var targetPoint = this._findLastPointPriorToTime(currentPoint, targetTimeSeriesData);
        var diffFromTarget = targetPoint !== undefined ? (currentPoint.value - targetPoint.value) / targetPoint.value : undefined;

        var label = '';
        var className = '';

        function labelForDiff(diff, referencePoint, name, comparison)
        {
            var relativeDiff = Math.abs(diff * 100).toFixed(1);
            var referenceValue = referencePoint ? ` (${formatter(referencePoint.value)})` : '';
            return `${relativeDiff}% ${comparison} ${name}${referenceValue}`;
        }

        var smallerIsBetter = metric.isSmallerBetter();
        if (diffFromBaseline !== undefined && diffFromTarget !== undefined) {
            if (diffFromBaseline > 0 == smallerIsBetter) {
                label = labelForDiff(diffFromBaseline, baselinePoint, 'baseline', 'worse than');
                className = 'worse';
            } else if (diffFromTarget < 0 == smallerIsBetter) {
                label = labelForDiff(diffFromTarget, targetPoint, 'target', 'better than');
                className = 'better';
            } else
                label = labelForDiff(diffFromTarget, targetPoint, 'target', 'until');
        } else if (diffFromBaseline !== undefined) {
            className = diffFromBaseline > 0 == smallerIsBetter ? 'worse' : 'better';
            label = labelForDiff(diffFromBaseline, baselinePoint, 'baseline', className + ' than');
        } else if (diffFromTarget !== undefined) {
            className = diffFromTarget < 0 == smallerIsBetter ? 'better' : 'worse';
            label = labelForDiff(diffFromTarget, targetPoint, 'target', className + ' than');
        }

        var valueDelta = null;
        var relativeDelta = null;
        if (previousPoint) {
            valueDelta = deltaFormatter(currentPoint.value - previousPoint.value);
            var relativeDelta = (currentPoint.value - previousPoint.value) / previousPoint.value;
            relativeDelta = (relativeDelta * 100).toFixed(0) + '%';
        }
        return {
            className: className,
            label: label,
            currentValue: formatter(currentPoint.value),
            valueDelta: valueDelta,
            relativeDelta: relativeDelta,
        };
    }

    _findLastPointPriorToTime(currentPoint, timeSeriesData)
    {
        if (!currentPoint || !timeSeriesData || !timeSeriesData.length)
            return undefined;

        var i = 0;
        while (i < timeSeriesData.length - 1 && timeSeriesData[i + 1].time < currentPoint.time)
            i++;
        return timeSeriesData[i];
    }

    static htmlTemplate()
    {
        return `
            <div>
                <span class="chart-status-current-value"></span>
                <span class="chart-status-comparison"></span>
            </div>`;
    }

    static cssTemplate()
    {
        return `
            .chart-status-current-value {
                padding-right: 0.5rem;
            }

            .chart-status-comparison.worse {
                color: #c33;
            }

            .chart-status-comparison.better {
                color: #33c;
            }`;
    }
}