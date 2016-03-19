'use strict';

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

    applyToAnalysisResults(row)
    {
        var adaptedRow = this.applyTo(row);
        adaptedRow.metricId = row[this._metricIndex];
        adaptedRow.configType = row[this._configTypeIndex];
        return adaptedRow;
    }

    applyTo(row)
    {
        var id = row[this._idIndex];
        var mean = row[this._meanIndex];
        var sum = row[this._sumIndex];
        var squareSum = row[this._squareSumIndex];
        var revisionList = row[this._revisionsIndex];
        var buildId = row[this._buildIndex];
        var builderId = row[this._builderIndex];
        var buildNumber = row[this._buildNumberIndex];
        var buildTime = row[this._buildTimeIndex];
        var self = this;
        return {
            id: id,
            buildId: buildId,
            metricId: null,
            configType: null,
            rootSet: function () { return MeasurementRootSet.ensureSingleton(id, revisionList); },
            build: function () { return new Build(buildId, Builder.findById(builderId), buildNumber, buildTime); },
            time: row[this._commitTimeIndex],
            value: mean,
            sum: sum,
            squareSum: squareSum,
            iterationCount: row[this._countIndex],
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

    static computeConfidenceInterval(iterationCount, mean, sum, squareSum)
    {
        var delta = Statistics.confidenceIntervalDelta(0.95, iterationCount, sum, squareSum);
        return isNaN(delta) ? null : [mean - delta, mean + delta];
    }
}

if (typeof module != 'undefined')
    module.exports.MeasurementAdaptor = MeasurementAdaptor;
