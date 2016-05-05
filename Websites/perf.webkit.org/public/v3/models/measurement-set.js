'use strict';

if (!Array.prototype.includes)
    Array.prototype.includes = function (value) { return this.indexOf(value) >= 0; }

class MeasurementSet {
    constructor(platformId, metricId, lastModified)
    {
        this._platformId = platformId;
        this._metricId = metricId;
        this._lastModified = +lastModified;

        this._sortedClusters = [];
        this._primaryClusterEndTime = null;
        this._clusterCount = null;
        this._clusterStart = null;
        this._clusterSize = null;
        this._allFetches = {};
        this._primaryClusterPromise = null;
    }

    platformId() { return this._platformId; }

    static findSet(platformId, metricId, lastModified)
    {
        if (!this._set)
            this._set = {};
        var key = platformId + '-' + metricId;
        if (!this._set[key])
            this._set[key] = new MeasurementSet(platformId, metricId, lastModified);
        return this._set[key];
    }

    findClusters(startTime, endTime)
    {
        var clusterStart = this._clusterStart;
        var clusterSize = this._clusterSize;

        function computeClusterStart(time) {
            var diff = time - clusterStart;
            return clusterStart + Math.floor(diff / clusterSize) * clusterSize;            
        }

        var clusters = [];
        var clusterEnd = computeClusterStart(startTime);

        var lastClusterEndTime = this._primaryClusterEndTime;
        var firstClusterEndTime = lastClusterEndTime - clusterStart * this._clusterCount;
        do {
            clusterEnd += clusterSize;
            if (firstClusterEndTime <= clusterEnd && clusterEnd <= this._primaryClusterEndTime)
                clusters.push(clusterEnd);
        } while (clusterEnd < endTime);

        return clusters;
    }

    fetchBetween(startTime, endTime, callback)
    {
        if (!this._primaryClusterPromise)
            this._primaryClusterPromise = this._fetchPrimaryCluster();
        var self = this;
        this._primaryClusterPromise.catch(callback);
        return this._primaryClusterPromise.then(function () {
            var promiseList = [];
            self.findClusters(startTime, endTime).map(function (clusterEndTime) {
                if(!self._allFetches[clusterEndTime])
                    self._allFetches[clusterEndTime] = self._fetchSecondaryCluster(clusterEndTime);
                self._allFetches[clusterEndTime].then(callback, callback);
                promiseList.push(self._allFetches[clusterEndTime]);
            });
            return Promise.all(promiseList);
        });
    }

    _constructUrl(useCache, clusterEndTime)
    {
        if (!useCache) {
            return `../api/measurement-set?platform=${this._platformId}&metric=${this._metricId}`;
        }
        var url;
        url = `../data/measurement-set-${this._platformId}-${this._metricId}`;
        if (clusterEndTime)
            url += '-' + +clusterEndTime;
        url += '.json';
        return url;
    }

    _fetchPrimaryCluster() {
        var self = this;
        return RemoteAPI.getJSONWithStatus(self._constructUrl(true, null)).then(function (data) {
            if (+data['lastModified'] < self._lastModified)
                return RemoteAPI.getJSONWithStatus(self._constructUrl(false, null));
            return data;
        }).catch(function (error) {
            if(error == 404)
                return RemoteAPI.getJSONWithStatus(self._constructUrl(false, null));
            return Promise.reject(error);
        }).then(function (data) {
            self._didFetchJSON(true, data);
            self._allFetches[self._primaryClusterEndTime] = self._primaryClusterPromise;
        });
    }

    _fetchSecondaryCluster(endTime) {
        var self = this;
        return RemoteAPI.getJSONWithStatus(self._constructUrl(true, endTime)).then(function (data) {
            self._didFetchJSON(false, data);
        });
    }

    _didFetchJSON(isPrimaryCluster, response, clusterEndTime)
    {
        console.assert(isPrimaryCluster);

        if (isPrimaryCluster) {
            this._primaryClusterEndTime = response['endTime'];
            this._clusterCount = response['clusterCount'];
            this._clusterStart = response['clusterStart'];
            this._clusterSize = response['clusterSize'];
        } else
            console.assert(this._primaryClusterEndTime);

        this._addFetchedCluster(new MeasurementCluster(response));
    }

    _addFetchedCluster(cluster)
    {
        this._sortedClusters.push(cluster);
        this._sortedClusters = this._sortedClusters.sort(function (c1, c2) {
            return c1.startTime() - c2.startTime();
        });
    }

    hasFetchedRange(startTime, endTime)
    {
        console.assert(startTime < endTime);
        var hasHole = false;
        var previousEndTime = null;
        for (var cluster of this._sortedClusters) {
            if (cluster.startTime() < startTime && startTime < cluster.endTime())
                hasHole = false;
            if (previousEndTime !== null && previousEndTime != cluster.startTime())
                hasHole = true;
            if (cluster.startTime() < endTime && endTime < cluster.endTime())
                break;
            previousEndTime = cluster.endTime();
        }
        return !hasHole;
    }

    fetchedTimeSeries(configType, includeOutliers, extendToFuture)
    {
        Instrumentation.startMeasuringTime('MeasurementSet', 'fetchedTimeSeries');

        // FIXME: Properly construct TimeSeries.
        var series = new TimeSeries([]);
        var idMap = {};
        for (var cluster of this._sortedClusters)
            cluster.addToSeries(series, configType, includeOutliers, idMap);

        if (extendToFuture)
            series.extendToFuture();

        Instrumentation.endMeasuringTime('MeasurementSet', 'fetchedTimeSeries');

        return series;
    }
}

if (typeof module != 'undefined')
    module.exports.MeasurementSet = MeasurementSet;
