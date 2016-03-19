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
        rawMeasurements.forEach(function (row) {
            var id = self._adaptor.extractId(row);
            if (id in idMap)
                return;
            if (row[self._markedOutlierIndex] && !includeOutliers)
                return;

            idMap[id] = true;

            series.append(self._adaptor.applyTo(row));
        });
    }
}

if (typeof module != 'undefined')
    module.exports.MeasurementCluster = MeasurementCluster;
