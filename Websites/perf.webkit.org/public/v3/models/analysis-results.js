
class AnalysisResults {
    constructor()
    {
        this._buildToMetricsMap = {};
    }

    find(buildId, metric)
    {
        var map = this._buildToMetricsMap[buildId];
        if (!map)
            return null;
        return map[metric.id()];
    }

    add(measurement)
    {
        console.assert(measurement.configType == 'current');
        if (!this._buildToMetricsMap[measurement.buildId])
            this._buildToMetricsMap[measurement.buildId] = {};
        var map = this._buildToMetricsMap[measurement.buildId];
        console.assert(!map[measurement.metricId]);
        map[measurement.metricId] = measurement;
    }

    static fetch(taskId)
    {
        taskId = parseInt(taskId);
        return getJSONWithStatus(`../api/measurement-set?analysisTask=${taskId}`).then(function (response) {

            Instrumentation.startMeasuringTime('AnalysisResults', 'fetch');

            var adaptor = new MeasurementAdaptor(response['formatMap']);
            var results = new AnalysisResults;
            for (var rawMeasurement of response['measurements'])
                results.add(adaptor.adoptToAnalysisResults(rawMeasurement));

            Instrumentation.endMeasuringTime('AnalysisResults', 'fetch');

            return results;
        });
    }
}
