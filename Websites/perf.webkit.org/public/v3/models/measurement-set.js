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
        this._callbackMap = new Map;
        this._primaryClusterPromise = null;
        this._segmentationCache = new Map;
    }

    platformId() { return this._platformId; }
    metricId() { return this._metricId; }

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

        var clusters = [];
        var clusterEnd = clusterStart + Math.floor(Math.max(0, startTime - clusterStart) / clusterSize) * clusterSize;

        var lastClusterEndTime = this._primaryClusterEndTime;
        var firstClusterEndTime = lastClusterEndTime - clusterSize * (this._clusterCount - 1);
        do {
            clusterEnd += clusterSize;
            if (firstClusterEndTime <= clusterEnd && clusterEnd <= this._primaryClusterEndTime)
                clusters.push(clusterEnd);
        } while (clusterEnd < endTime);

        return clusters;
    }

    fetchBetween(startTime, endTime, callback, noCache)
    {
        if (noCache) {
            this._primaryClusterPromise = null;
            this._allFetches = {};
        }
        if (!this._primaryClusterPromise)
            this._primaryClusterPromise = this._fetchPrimaryCluster(noCache);
        var self = this;
        if (callback)
            this._primaryClusterPromise.catch(callback);
        return this._primaryClusterPromise.then(function () {
            self._allFetches[self._primaryClusterEndTime] = self._primaryClusterPromise;
            return Promise.all(self.findClusters(startTime, endTime).map(function (clusterEndTime) {
                return self._ensureClusterPromise(clusterEndTime, callback);
            }));
        });
    }

    _ensureClusterPromise(clusterEndTime, callback)
    {
        if (!this._callbackMap.has(clusterEndTime))
            this._callbackMap.set(clusterEndTime, new Set);
        var callbackSet = this._callbackMap.get(clusterEndTime);
        if (callback)
            callbackSet.add(callback);

        var promise = this._allFetches[clusterEndTime];
        if (promise) {
            if (callback)
                promise.then(callback, callback);
        } else {
            promise = this._fetchSecondaryCluster(clusterEndTime);
            for (var existingCallback of callbackSet)
                promise.then(existingCallback, existingCallback);
            this._allFetches[clusterEndTime] = promise;
        }

        return promise;
    }

    _constructUrl(useCache, clusterEndTime)
    {
        if (!useCache) {
            return `/api/measurement-set?platform=${this._platformId}&metric=${this._metricId}`;
        }
        var url;
        url = `/data/measurement-set-${this._platformId}-${this._metricId}`;
        if (clusterEndTime)
            url += '-' + +clusterEndTime;
        url += '.json';
        return url;
    }

    _fetchPrimaryCluster(noCache)
    {
        var self = this;
        if (noCache) {
            return RemoteAPI.getJSONWithStatus(self._constructUrl(false, null)).then(function (data) {
                self._didFetchJSON(true, data);
            });
        }

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
        });
    }

    _fetchSecondaryCluster(endTime)
    {
        var self = this;
        return RemoteAPI.getJSONWithStatus(self._constructUrl(true, endTime)).then(function (data) {
            self._didFetchJSON(false, data);
        });
    }

    _didFetchJSON(isPrimaryCluster, response, clusterEndTime)
    {
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
        for (var clusterIndex = 0; clusterIndex < this._sortedClusters.length; clusterIndex++) {
            var startTime = this._sortedClusters[clusterIndex].startTime();
            if (cluster.startTime() <= startTime) {
                this._sortedClusters.splice(clusterIndex, startTime == cluster.startTime() ? 1 : 0, cluster);
                return;
            }
        }
        this._sortedClusters.push(cluster);
    }

    hasFetchedRange(startTime, endTime)
    {
        console.assert(startTime < endTime);
        let foundStart = false;
        let previousEndTime = null;
        endTime = Math.min(endTime, this._primaryClusterEndTime);
        for (var cluster of this._sortedClusters) {
            const containsStart = cluster.startTime() <= startTime && startTime <= cluster.endTime();
            const containsEnd = cluster.startTime() <= endTime && endTime <= cluster.endTime();
            const preceedingClusterIsMissing = previousEndTime !== null && previousEndTime != cluster.startTime();
            if (containsStart && containsEnd)
                return true;
            if (containsStart)
                foundStart = true;
            if (foundStart && preceedingClusterIsMissing)
                return false;
            if (containsEnd)
                return foundStart; // Return true iff there were not missing clusters from the one that contains startTime
            previousEndTime = cluster.endTime();
        }
        return false; // Didn't find a cluster that contains startTime or endTime
    }

    fetchedTimeSeries(configType, includeOutliers, extendToFuture)
    {
        Instrumentation.startMeasuringTime('MeasurementSet', 'fetchedTimeSeries');

        // FIXME: Properly construct TimeSeries.
        var series = new TimeSeries();
        var idMap = {};
        for (var cluster of this._sortedClusters)
            cluster.addToSeries(series, configType, includeOutliers, idMap);

        if (extendToFuture)
            series.extendToFuture();

        Instrumentation.endMeasuringTime('MeasurementSet', 'fetchedTimeSeries');

        return series;
    }

    fetchSegmentation(segmentationName, parameters, configType, includeOutliers, extendToFuture)
    {
        var cacheMap = this._segmentationCache.get(configType);
        if (!cacheMap) {
            cacheMap = new WeakMap;
            this._segmentationCache.set(configType, cacheMap);
        }

        var timeSeries = new TimeSeries;
        var idMap = {};
        var promises = [];
        for (var cluster of this._sortedClusters) {
            var clusterStart = timeSeries.length();
            cluster.addToSeries(timeSeries, configType, includeOutliers, idMap);
            var clusterEnd = timeSeries.length();
            promises.push(this._cachedClusterSegmentation(segmentationName, parameters, cacheMap,
                cluster, timeSeries, clusterStart, clusterEnd, idMap));
        }
        if (!timeSeries.length())
            return Promise.resolve(null);

        var self = this;
        return Promise.all(promises).then(function (clusterSegmentations) {
            var segmentationSeries = [];
            var addSegmentMergingIdenticalSegments = function (startingPoint, endingPoint) {
                var value = Statistics.mean(timeSeries.valuesBetweenRange(startingPoint.seriesIndex, endingPoint.seriesIndex));
                if (!segmentationSeries.length || value !== segmentationSeries[segmentationSeries.length - 1].value) {
                    segmentationSeries.push({value: value, time: startingPoint.time, seriesIndex: startingPoint.seriesIndex, interval: function () { return null; }});
                    segmentationSeries.push({value: value, time: endingPoint.time, seriesIndex: endingPoint.seriesIndex, interval: function () { return null; }});
                } else
                    segmentationSeries[segmentationSeries.length - 1].seriesIndex = endingPoint.seriesIndex;
            };

            let startingIndex = 0;
            for (const segmentation of clusterSegmentations) {
                for (const endingIndex of segmentation) {
                    addSegmentMergingIdenticalSegments(timeSeries.findPointByIndex(startingIndex), timeSeries.findPointByIndex(endingIndex));
                    startingIndex = endingIndex;
                }
            }
            if (extendToFuture)
                timeSeries.extendToFuture();
            addSegmentMergingIdenticalSegments(timeSeries.findPointByIndex(startingIndex), timeSeries.lastPoint());
            return segmentationSeries;
        });
    }

    _cachedClusterSegmentation(segmentationName, parameters, cacheMap, cluster, timeSeries, clusterStart, clusterEnd, idMap)
    {
        var cache = cacheMap.get(cluster);
        if (cache && this._validateSegmentationCache(cache, segmentationName, parameters)) {
            var segmentationByIndex = new Array(cache.segmentation.length);
            for (var i = 0; i < cache.segmentation.length; i++) {
                var id = cache.segmentation[i];
                if (!(id in idMap))
                    return null;
                segmentationByIndex[i] = idMap[id];
            }
            return Promise.resolve(segmentationByIndex);
        }

        var clusterValues = timeSeries.valuesBetweenRange(clusterStart, clusterEnd);
        return this._invokeSegmentationAlgorithm(segmentationName, parameters, clusterValues).then(function (segmentationInClusterIndex) {
            // Remove cluster start/end as segmentation points. Otherwise each cluster will be placed into its own segment. 
            var segmentation = segmentationInClusterIndex.slice(1, -1).map(function (index) { return clusterStart + index; });
            var cache = segmentation.map(function (index) { return timeSeries.findPointByIndex(index).id; });
            cacheMap.set(cluster, {segmentationName: segmentationName, segmentationParameters: parameters.slice(), segmentation: cache});
            return segmentation;
        });
    }

    _validateSegmentationCache(cache, segmentationName, parameters)
    {
        if (cache.segmentationName != segmentationName)
            return false;
        if (!!cache.segmentationParameters != !!parameters)
            return false;
        if (parameters) {
            if (parameters.length != cache.segmentationParameters.length)
                return false;
            for (var i = 0; i < parameters.length; i++) {
                if (parameters[i] != cache.segmentationParameters[i])
                    return false;
            }
        }
        return true;
    }

    _invokeSegmentationAlgorithm(segmentationName, parameters, timeSeriesValues)
    {
        var args = [timeSeriesValues].concat(parameters || []);

        var timeSeriesIsShortEnoughForSyncComputation = timeSeriesValues.length < 100;
        if (timeSeriesIsShortEnoughForSyncComputation || !AsyncTask.isAvailable()) {
            Instrumentation.startMeasuringTime('_invokeSegmentationAlgorithm', 'syncSegmentation');
            var segmentation = Statistics[segmentationName].apply(timeSeriesValues, args);
            Instrumentation.endMeasuringTime('_invokeSegmentationAlgorithm', 'syncSegmentation');
            return Promise.resolve(segmentation);
        }

        var task = new AsyncTask(segmentationName, args);
        return task.execute().then(function (response) {
            Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm', 'workerStartLatency', 'ms', response.startLatency);
            Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm', 'workerTime', 'ms', response.workerTime);
            Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm', 'totalTime', 'ms', response.totalTime);
            return response.result;
        });
    }

}

if (typeof module != 'undefined')
    module.exports.MeasurementSet = MeasurementSet;
