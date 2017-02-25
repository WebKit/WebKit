
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
                const indicator = this._chart.currentIndicator();
                if (indicator) {
                    currentPoint = indicator.point;
                    previousPoint = indicator.view.previousPoint(currentPoint);
                }
            }
        } else {
            var data = this._chart.sampledTimeSeriesData('current');
            if (!data)
                return false;
            if (data.length())
                currentPoint = data.lastPoint();
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
        console.assert(currentPoint);
        const baselineView = chart.sampledTimeSeriesData('baseline');
        const targetView = chart.sampledTimeSeriesData('target');

        const formatter = metric.makeFormatter(3);
        const deltaFormatter = metric.makeFormatter(2, true);
        const smallerIsBetter = metric.isSmallerBetter();

        const labelForDiff = (diff, referencePoint, name, comparison) => {
            const relativeDiff = Math.abs(diff * 100).toFixed(1);
            const referenceValue = referencePoint ? ` (${formatter(referencePoint.value)})` : '';
            return `${relativeDiff}% ${comparison} than ${name}${referenceValue}`;
        };

        const baselinePoint = baselineView ? baselineView.lastPointInTimeRange(0, currentPoint.time) : null;
        const targetPoint = targetView ? targetView.lastPointInTimeRange(0, currentPoint.time) : null;

        const diffFromBaseline = baselinePoint ? (currentPoint.value - baselinePoint.value) / baselinePoint.value : undefined;
        const diffFromTarget = targetPoint ? (currentPoint.value - targetPoint.value) / targetPoint.value : undefined;

        let label = null;
        let comparison = null;

        if (diffFromBaseline !== undefined && diffFromTarget !== undefined) {
            if (diffFromBaseline > 0 == smallerIsBetter) {
                comparison = 'worse';
                label = labelForDiff(diffFromBaseline, baselinePoint, 'baseline', comparison);
            } else if (diffFromTarget < 0 == smallerIsBetter) {
                comparison = 'better';
                label = labelForDiff(diffFromTarget, targetPoint, 'target', comparison);
            } else
                label = labelForDiff(diffFromTarget, targetPoint, 'target', 'until');
        } else if (diffFromBaseline !== undefined) {
            comparison = diffFromBaseline > 0 == smallerIsBetter ? 'worse' : 'better';
            label = labelForDiff(diffFromBaseline, baselinePoint, 'baseline', comparison);
        } else if (diffFromTarget !== undefined) {
            comparison = diffFromTarget < 0 == smallerIsBetter ? 'better' : 'worse';
            label = labelForDiff(diffFromTarget, targetPoint, 'target', comparison);
        }

        let valueDelta = null;
        let relativeDelta = null;
        if (previousPoint) {
            valueDelta = deltaFormatter(currentPoint.value - previousPoint.value);
            relativeDelta = (currentPoint.value - previousPoint.value) / previousPoint.value;
            relativeDelta = (relativeDelta * 100).toFixed(0) + '%';
        }

        return {className: comparison, label, currentValue: formatter(currentPoint.value), valueDelta, relativeDelta};
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