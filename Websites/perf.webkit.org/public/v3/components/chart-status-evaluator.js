
class ChartStatusEvaluator {

    constructor(chart, metric)
    {
        this._chart = chart;
        this._computeStatus = new LazilyEvaluatedFunction((currentPoint, previousPoint, view) => {
            if (!currentPoint)
                return null;

            const baselineView = this._chart.sampledTimeSeriesData('baseline');
            const targetView = this._chart.sampledTimeSeriesData('target');
            return ChartStatusEvaluator.computeChartStatus(metric, currentPoint, previousPoint, view, baselineView, targetView);
        });
    }

    status()
    {
        const referencePoints = this._chart.referencePoints('current');
        const currentPoint = referencePoints ? referencePoints.currentPoint : null;
        const previousPoint = referencePoints ? referencePoints.previousPoint : null;
        const view = referencePoints ? referencePoints.view : null;
        return this._computeStatus.evaluate(currentPoint, previousPoint, view);
    }

    static computeChartStatus(metric, currentPoint, previousPoint, currentView, baselineView, targetView)
    {
        const formatter = metric.makeFormatter(3);
        const deltaFormatter = metric.makeFormatter(2, true);
        const smallerIsBetter = metric.isSmallerBetter();

        const labelForDiff = (diff, referencePoint, name, comparison) => {
            const relativeDiff = Math.abs(diff * 100).toFixed(1);
            const referenceValue = referencePoint ? ` (${formatter(referencePoint.value)})` : '';
            if (comparison != 'until')
                comparison += ' than';
            return `${relativeDiff}% ${comparison} ${name}${referenceValue}`;
        }

        const pointIsInCurrentSeries = baselineView != currentView && targetView != currentView;

        const baselinePoint = pointIsInCurrentSeries && baselineView ? baselineView.lastPointInTimeRange(0, currentPoint.time) : null;
        const targetPoint = pointIsInCurrentSeries && targetView ? targetView.lastPointInTimeRange(0, currentPoint.time) : null;

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
            label = labelForDiff(diffFromTarget, targetPoint, 'target', 'until');
        }

        let valueDelta = null;
        let relativeDelta = null;
        if (previousPoint) {
            valueDelta = deltaFormatter(currentPoint.value - previousPoint.value);
            relativeDelta = (currentPoint.value - previousPoint.value) / previousPoint.value;
            relativeDelta = (relativeDelta * 100).toFixed(0) + '%';
        }

        return {comparison, label, currentValue: formatter(currentPoint.value), valueDelta, relativeDelta};
    }

}
