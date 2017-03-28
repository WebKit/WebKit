
class AnalysisResults {
    constructor()
    {
        this._metricToBuildMap = {};
    }

    find(buildId, metricId)
    {
        const map = this._metricToBuildMap[metricId];
        if (!map)
            return null;
        return map[buildId];
    }

    add(measurement)
    {
        console.assert(measurement.configType == 'current');
        const metricId = measurement.metricId;
        if (!(metricId in this._metricToBuildMap))
            this._metricToBuildMap[metricId] = {};
        var map = this._metricToBuildMap[metricId];
        console.assert(!map[measurement.buildId]);
        map[measurement.buildId] = measurement;
    }

    viewForMetric(metric)
    {
        console.assert(metric instanceof Metric);
        return new AnalysisResultsView(this, metric);
    }

    static fetch(taskId)
    {
        taskId = parseInt(taskId);
        return RemoteAPI.getJSONWithStatus(`../api/measurement-set?analysisTask=${taskId}`).then(function (response) {

            Instrumentation.startMeasuringTime('AnalysisResults', 'fetch');

            const adaptor = new MeasurementAdaptor(response['formatMap']);
            const results = new AnalysisResults;
            for (const rawMeasurement of response['measurements'])
                results.add(adaptor.applyToAnalysisResults(rawMeasurement));

            Instrumentation.endMeasuringTime('AnalysisResults', 'fetch');

            return results;
        });
    }
}

class AnalysisResultsView {
    constructor(analysisResults, metric)
    {
        console.assert(analysisResults instanceof AnalysisResults);
        console.assert(metric instanceof Metric);
        this._results = analysisResults;
        this._metric = metric;
    }

    metric() { return this._metric; }

    resultForBuildId(buildId)
    {
        return this._results.find(buildId, this._metric.id());
    }
}
