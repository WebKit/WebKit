
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
            currentPoint = this._chart.currentPoint();

            var selection = this._chart.currentSelection();
            if (selection && this._usedSelection == selection)
                return false;

            if (selection) {
                var data = this._chart.sampledDataBetween('current', selection[0], selection[1]);
                if (!data)
                    return false;
                this._usedSelection = selection;

                if (data && data.length > 1) {
                    currentPoint = data[data.length - 1];
                    previousPoint = data[0];
                }
            } else if (currentPoint)
                previousPoint = currentPoint.series.previousPoint(currentPoint);
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

        var diffFromBaseline = this._relativeDifferenceToLaterPointInTimeSeries(currentPoint, baselineTimeSeriesData);
        var diffFromTarget = this._relativeDifferenceToLaterPointInTimeSeries(currentPoint, targetTimeSeriesData);

        var label = '';
        var className = '';

        function labelForDiff(diff, name, comparison)
        {
            return Math.abs(diff * 100).toFixed(1) + '% ' + comparison + ' ' + name;
        }

        var smallerIsBetter = metric.isSmallerBetter();
        if (diffFromBaseline !== undefined && diffFromTarget !== undefined) {
            if (diffFromBaseline > 0 == smallerIsBetter) {
                label = labelForDiff(diffFromBaseline, 'baseline', 'worse than');
                className = 'worse';
            } else if (diffFromTarget < 0 == smallerIsBetter) {
                label = labelForDiff(diffFromBaseline, 'target', 'better than');
                className = 'better';
            } else
                label = labelForDiff(diffFromTarget, 'target', 'until');
        } else if (diffFromBaseline !== undefined) {
            className = diffFromBaseline > 0 == smallerIsBetter ? 'worse' : 'better';
            label = labelForDiff(diffFromBaseline, 'baseline', className + ' than');
        } else if (diffFromTarget !== undefined) {
            className = diffFromTarget < 0 == smallerIsBetter ? 'better' : 'worse';
            label = labelForDiff(diffFromTarget, 'target', className + ' than');
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

    _relativeDifferenceToLaterPointInTimeSeries(currentPoint, timeSeriesData)
    {
        if (!currentPoint || !timeSeriesData || !timeSeriesData.length)
            return undefined;

        // FIXME: We shouldn't be using the first data point to access the time series object.
        var referencePoint = timeSeriesData[0].series.findPointAfterTime(currentPoint.time);
        if (!referencePoint)
            return undefined;

        return (currentPoint.value - referencePoint.value) / referencePoint.value;
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