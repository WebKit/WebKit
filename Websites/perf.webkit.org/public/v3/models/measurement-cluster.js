
class MeasurementCluster {
    constructor(response)
    {
        this._response = response;
        this._idMap = {};

        var nameMap = {};
        response['formatMap'].forEach(function (key, index) {
            nameMap[key] = index;
        });

        this._idIndex = nameMap['id'];
        this._commitTimeIndex = nameMap['commitTime'];
        this._countIndex = nameMap['iterationCount'];
        this._meanIndex = nameMap['mean'];
        this._sumIndex = nameMap['sum'];
        this._squareSumIndex = nameMap['squareSum'];
        this._markedOutlierIndex = nameMap['markedOutlier'];
        this._revisionsIndex = nameMap['revisions'];
        this._buildIndex = nameMap['build'];
        this._buildTimeIndex = nameMap['buildTime'];
        this._buildNumberIndex = nameMap['buildNumber'];
        this._builderIndex = nameMap['builder'];
    }

    startTime() { return this._response['startTime']; }

    addToSeries(series, configType, includeOutliers, idMap)
    {
        var rawMeasurements = this._response['configurations'][configType];
        if (!rawMeasurements)
            return;

        var self = this;
        rawMeasurements.forEach(function (row) {
            var id = row[self._idIndex];
            if (id in idMap)
                return;
            if (row[self._markedOutlierIndex] && !includeOutliers)
                return;

            idMap[id] = true;

            var mean = row[self._meanIndex];
            var sum = row[self._sumIndex];
            var squareSum = row[self._squareSumIndex];
            series._series.push({
                id: id,
                _rawMeasurement: row,
                measurement: function () {
                    // Create a new Measurement class that doesn't require mimicking what runs.php generates.
                    var revisionsMap = {};
                    for (var revisionRow of row[self._revisionsIndex])
                        revisionsMap[revisionRow[0]] = revisionRow.slice(1);
                    return new Measurement({
                        id: id,
                        mean: mean,
                        sum: sum,
                        squareSum: squareSum,
                        revisions: revisionsMap,
                        build: row[self._buildIndex],
                        buildTime: row[self._buildTimeIndex],
                        buildNumber: row[self._buildNumberIndex],
                        builder: row[self._builderIndex],
                    });
                },
                series: series,
                seriesIndex: series._series.length,
                time: row[self._commitTimeIndex],
                value: mean,
                interval: self._computeConfidenceInterval(row[self._countIndex], mean, sum, squareSum)
            });            
        });
    }

    _computeConfidenceInterval(iterationCount, mean, sum, squareSum)
    {
        var delta = Statistics.confidenceIntervalDelta(0.95, iterationCount, sum, squareSum);
        return isNaN(delta) ? null : [mean - delta, mean + delta];
    }
}
