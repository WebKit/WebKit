'use strict';

class MeasurementCluster {
    constructor(response)
    {
        this._response = response;
        this._adaptor = new MeasurementAdaptor(response['formatMap']);
    }

    startTime() { return this._response['startTime']; }
    endTime() { return this._response['endTime']; }

    addToSeries(series, configType, includeOutliers, idMap)
    {
        var rawMeasurements = this._response['configurations'][configType];
        if (!rawMeasurements)
            return;

        var self = this;
        for (var row of rawMeasurements) {
            var point = this._adaptor.applyTo(row);
            if (point.id in idMap || (!includeOutliers && point.isOutlier))
                continue;
            idMap[point.id] = true;
            point.cluster = this;
            series.append(point);
        }
    }
}

if (typeof module != 'undefined')
    module.exports.MeasurementCluster = MeasurementCluster;
