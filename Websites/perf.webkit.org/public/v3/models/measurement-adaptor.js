
class MeasurementAdaptor {
    constructor(formatMap)
    {
        var nameMap = {};
        formatMap.forEach(function (key, index) {
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
        this._metricIndex = nameMap['metric'];
        this._configTypeIndex = nameMap['configType'];
    }

    extractId(row)
    {
        return row[this._idIndex];
    }

    adoptToAnalysisResults(row)
    {
        var id = row[this._idIndex];
        var mean = row[this._meanIndex];
        var sum = row[this._sumIndex];
        var squareSum = row[this._squareSumIndex];
        var iterationCount = row[this._countIndex];
        var revisionList = row[this._revisionsIndex];
        var buildId = row[this._buildIndex];
        var metricId = row[this._metricIndex];
        var configType = row[this._configTypeIndex];
        var self = this;
        return {
            id: id,
            buildId: buildId,
            metricId: metricId,
            configType: configType,
            rootSet: function () { return MeasurementRootSet.ensureSingleton(id, revisionList); },
            build: function () {
                var builder = Builder.findById(row[self._builderIndex]);
                return new Build(id, builder, row[self._buildNumberIndex]);
            },
            value: mean,
            sum: sum,
            squareSum,
            iterationCount: iterationCount,
            interval: MeasurementAdaptor.computeConfidenceInterval(row[this._countIndex], mean, sum, squareSum)
        };
    }

    static aggregateAnalysisResults(results)
    {
        var totalSum = 0;
        var totalSquareSum = 0;
        var totalIterationCount = 0;
        var means = [];
        for (var result of results) {
            means.push(result.value);
            totalSum += result.sum;
            totalSquareSum += result.squareSum;
            totalIterationCount += result.iterationCount;
        }
        var mean = totalSum / totalIterationCount;
        var interval;
        try {
            interval = this.computeConfidenceInterval(totalIterationCount, mean, totalSum, totalSquareSum)
        } catch (error) {
            interval = this.computeConfidenceInterval(results.length, mean, Statistics.sum(means), Statistics.squareSum(means));
        }
        return { value: mean, interval: interval };
    }

    adoptToSeries(row, series, seriesIndex)
    {
        var id = row[this._idIndex];
        var mean = row[this._meanIndex];
        var sum = row[this._sumIndex];
        var squareSum = row[this._squareSumIndex];
        var revisionList = row[this._revisionsIndex];
        var self = this;
        return {
            id: id,
            measurement: function () {
                // Create a new Measurement class that doesn't require mimicking what runs.php generates.
                var revisionsMap = {};
                for (var revisionRow of revisionList)
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
            rootSet: function () { return MeasurementRootSet.ensureSingleton(id, revisionList); },
            series: series,
            seriesIndex: seriesIndex,
            time: row[this._commitTimeIndex],
            value: mean,
            interval: MeasurementAdaptor.computeConfidenceInterval(row[this._countIndex], mean, sum, squareSum)
        };
    }

    static computeConfidenceInterval(iterationCount, mean, sum, squareSum)
    {
        var delta = Statistics.confidenceIntervalDelta(0.95, iterationCount, sum, squareSum);
        return isNaN(delta) ? null : [mean - delta, mean + delta];
    }
}
