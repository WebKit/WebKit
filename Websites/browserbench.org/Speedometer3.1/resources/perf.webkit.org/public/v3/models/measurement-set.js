'use strict';

class MeasurementSet {
    _platformId;
    _metricId;
    _lastModified;
    _sortedClusters = [];
    _primaryClusterEndTime = null;
    _clusterCount = null;
    _clusterStart = null;
    _clusterSize = null;
    _allFetches = new Map;
    _callbackMap = new Map;
    _primaryClusterPromise = null;
    _segmentationCache = new Map;

    constructor(platformId, metricId, lastModified)
    {
        this._platformId = platformId;
        this._metricId = metricId;
        this._lastModified = +lastModified;
    }

    platformId() { return this._platformId; }
    metricId() { return this._metricId; }

    static findSet(platformId, metricId, lastModified)
    {
        if (!this._setMap)
            this._setMap = new Map;
        const key = `${platformId}-${metricId}`;
        let measurementSet = this._setMap.get(key);
        if (!measurementSet) {
            measurementSet = new MeasurementSet(platformId, metricId, lastModified);
            this._setMap.set(key, measurementSet);
        }
        return measurementSet;
    }

    findClusters(startTime, endTime)
    {
        const clusterStart = this._clusterStart;
        const clusterSize = this._clusterSize;

        const clusters = [];
        let clusterEnd = clusterStart + Math.floor(Math.max(0, startTime - clusterStart) / clusterSize) * clusterSize;

        const lastClusterEndTime = this._primaryClusterEndTime;
        const firstClusterEndTime = lastClusterEndTime - clusterSize * (this._clusterCount - 1);
        do {
            clusterEnd += clusterSize;
            if (firstClusterEndTime <= clusterEnd && clusterEnd <= this._primaryClusterEndTime)
                clusters.push(clusterEnd);
        } while (clusterEnd < endTime);

        return clusters;
    }

    async fetchBetween(startTime, endTime, callback, noCache)
    {
        if (noCache) {
            this._primaryClusterPromise = null;
            this._allFetches = new Map;
        }
        if (!this._primaryClusterPromise)
            this._primaryClusterPromise = this._fetchPrimaryCluster(noCache);

        if (callback)
            this._primaryClusterPromise.catch(callback);

        await this._primaryClusterPromise;

        this._allFetches.set(this._primaryClusterEndTime, this._primaryClusterPromise);
        return Promise.all(this.findClusters(startTime, endTime).map((clusterEndTime) => this._ensureClusterPromise(clusterEndTime, callback)));
    }

    _ensureClusterPromise(clusterEndTime, callback)
    {
        let callbackSet = this._callbackMap.get(clusterEndTime);
        if (!callbackSet) {
            callbackSet = new Set;
            this._callbackMap.set(clusterEndTime, callbackSet);
        }
        if (callback)
            callbackSet.add(callback);

        let promise = this._allFetches.get(clusterEndTime);
        if (promise) {
            if (callback)
                promise.then(callback, callback);
            return promise;
        }

        promise = this._fetchSecondaryCluster(clusterEndTime);
        for (const existingCallback of callbackSet)
            promise.then(existingCallback, existingCallback);
        this._allFetches.set(clusterEndTime, promise);
        return promise;
    }

    _urlForCache(clusterEndTime = null /* primary */)
    {
        const suffix = clusterEndTime ? '-' + +clusterEndTime : '';
        return `/data/measurement-set-${this._platformId}-${this._metricId}${suffix}.json`;
    }

    async _fetchPrimaryCluster(noCache = false)
    {
        let data = null;
        if (!noCache) {
            try {
                data = await RemoteAPI.getJSONWithStatus(this._urlForCache());
                if (+data['lastModified'] < this._lastModified)
                    data = null;
            } catch (error) {
                if (error != 404)
                    throw error;
            }
        }

        if (!data) // Request the latest JSON if noCache was true or the cached JSON was outdated.
            data = await RemoteAPI.getJSONWithStatus(`/api/measurement-set?platform=${this._platformId}&metric=${this._metricId}`);

        this._primaryClusterEndTime = data['endTime'];
        this._clusterCount = data['clusterCount'];
        this._clusterStart = data['clusterStart'];
        this._clusterSize = data['clusterSize'];
        this._addFetchedCluster(new MeasurementCluster(data));
    }

    async _fetchSecondaryCluster(endTime)
    {
        console.assert(endTime);
        console.assert(this._primaryClusterEndTime);
        const data = await RemoteAPI.getJSONWithStatus(this._urlForCache(endTime));
        this._addFetchedCluster(new MeasurementCluster(data));
    }

    _addFetchedCluster(cluster)
    {
        for (let clusterIndex = 0; clusterIndex < this._sortedClusters.length; clusterIndex++) {
            const startTime = this._sortedClusters[clusterIndex].startTime();
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
        for (const cluster of this._sortedClusters) {
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
        const series = new TimeSeries();
        const idMap = {};
        for (const cluster of this._sortedClusters)
            cluster.addToSeries(series, configType, includeOutliers, idMap);

        if (extendToFuture)
            series.extendToFuture();

        Instrumentation.endMeasuringTime('MeasurementSet', 'fetchedTimeSeries');

        return series;
    }

    async fetchSegmentation(segmentationName, parameters, configType, includeOutliers, extendToFuture)
    {
        let cacheMap = this._segmentationCache.get(configType);
        if (!cacheMap) {
            cacheMap = new WeakMap;
            this._segmentationCache.set(configType, cacheMap);
        }

        const timeSeries = new TimeSeries;
        const idMap = {};
        const promises = [];
        for (const cluster of this._sortedClusters) {
            const clusterStart = timeSeries.length();
            cluster.addToSeries(timeSeries, configType, includeOutliers, idMap);
            const clusterEnd = timeSeries.length();
            promises.push(this._cachedClusterSegmentation(segmentationName, parameters, cacheMap,
                cluster, timeSeries, clusterStart, clusterEnd, idMap));
        }
        if (!timeSeries.length())
            return Promise.resolve(null);

        const clusterSegmentations = await Promise.all(promises);

        const segmentationSeries = [];
        const addSegmentMergingIdenticalSegments = (startingPoint, endingPoint) => {
            const value = Statistics.mean(timeSeries.valuesBetweenRange(startingPoint.seriesIndex, endingPoint.seriesIndex));
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
    }

    async _cachedClusterSegmentation(segmentationName, parameters, cacheMap, cluster, timeSeries, clusterStart, clusterEnd, idMap)
    {
        const cache = cacheMap.get(cluster);
        if (cache && this._validateSegmentationCache(cache, segmentationName, parameters)) {
            const segmentationByIndex = new Array(cache.segmentation.length);
            for (let i = 0; i < cache.segmentation.length; i++) {
                const id = cache.segmentation[i];
                if (!(id in idMap))
                    return null;
                segmentationByIndex[i] = idMap[id];
            }
            return Promise.resolve(segmentationByIndex);
        }

        const clusterValues = timeSeries.valuesBetweenRange(clusterStart, clusterEnd);
        const segmentationInClusterIndex = await this._invokeSegmentationAlgorithm(segmentationName, parameters, clusterValues);
        // Remove cluster start/end as segmentation points. Otherwise each cluster will be placed into its own segment. 
        const segmentation = segmentationInClusterIndex.slice(1, -1).map((index) => clusterStart + index);
        cacheMap.set(cluster, {segmentationName,
            segmentationParameters: parameters.slice(),
            segmentation: segmentation.map((index) => timeSeries.findPointByIndex(index).id)});
        return segmentation;
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

    async _invokeSegmentationAlgorithm(segmentationName, parameters, timeSeriesValues)
    {
        const args = [timeSeriesValues].concat(parameters || []);

        const timeSeriesIsShortEnoughForSyncComputation = timeSeriesValues.length < 100;
        if (timeSeriesIsShortEnoughForSyncComputation || !AsyncTask.isAvailable()) {
            Instrumentation.startMeasuringTime('_invokeSegmentationAlgorithm', 'syncSegmentation');
            const segmentation = Statistics[segmentationName].apply(timeSeriesValues, args);
            Instrumentation.endMeasuringTime('_invokeSegmentationAlgorithm', 'syncSegmentation');
            return Promise.resolve(segmentation);
        }

        const task = new AsyncTask(segmentationName, args);
        const response = await task.execute();
        Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm', 'workerStartLatency', 'ms', response.startLatency);
        Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm', 'workerTime', 'ms', response.workerTime);
        Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm', 'totalTime', 'ms', response.totalTime);
        return response.result;
    }

}

if (typeof module != 'undefined')
    module.exports.MeasurementSet = MeasurementSet;
