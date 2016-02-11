
class MeasurementSet {
    constructor(platformId, metricId, lastModified)
    {
        this._platformId = platformId;
        this._metricId = metricId;
        this._lastModified = +lastModified;

        this._waitingForPrimaryCluster = null;
        this._fetchedPrimary = false;
        this._endTimeToCallback = {};

        this._sortedClusters = [];
        this._primaryClusterEndTime = null;
        this._clusterCount = null;
        this._clusterStart = null;
        this._clusterSize = null;
    }

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
        console.assert(clusterStart && clusterSize);

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
        if (!this._fetchedPrimary) {
            var primaryFetchHadFailed = this._waitingForPrimaryCluster === false;
            if (primaryFetchHadFailed) {
                callback();
                return;
            }

            var shouldStartPrimaryFetch = !this._waitingForPrimaryCluster;
            if (shouldStartPrimaryFetch)
                this._waitingForPrimaryCluster = [];

            this._waitingForPrimaryCluster.push({startTime: startTime, endTime: endTime, callback: callback});

            if (shouldStartPrimaryFetch)
                this._fetch(null, true);

            return;
        }

        this._fetchSecondaryClusters(startTime, endTime, callback);
    }

    _fetchSecondaryClusters(startTime, endTime, callback)
    {
        console.assert(this._fetchedPrimary);
        console.assert(this._clusterStart && this._clusterSize);
        console.assert(this._sortedClusters.length);

        var clusters = this.findClusters(startTime, endTime);
        var shouldInvokeCallackNow = false;
        for (var endTime of clusters) {
            var isPrimaryCluster = endTime == this._primaryClusterEndTime;
            var shouldStartFetch = !isPrimaryCluster && !(endTime in this._endTimeToCallback);
            if (shouldStartFetch)
                this._endTimeToCallback[endTime] = [];

            var callbackList = this._endTimeToCallback[endTime];
            if (isPrimaryCluster || callbackList === true)
                shouldInvokeCallackNow = true;
            else if (!callbackList.includes(callback))
                callbackList.push(callback);

            if (shouldStartFetch) {
                console.assert(!shouldInvokeCallackNow);
                this._fetch(endTime, true);
            }
        }

        if (shouldInvokeCallackNow)
            callback();
    }

    _fetch(clusterEndTime, useCache)
    {
        console.assert(!clusterEndTime || useCache);

        var url;
        if (useCache) {
            url = `../data/measurement-set-${this._platformId}-${this._metricId}`;
            if (clusterEndTime)
                url += '-' + +clusterEndTime;
            url += '.json';
        } else
            url = `../api/measurement-set?platform=${this._platformId}&metric=${this._metricId}`;

        var self = this;

        return getJSONWithStatus(url).then(function (data) {
            if (!clusterEndTime && useCache && +data['lastModified'] < self._lastModified)
                self._fetch(clusterEndTime, false);
            else
                self._didFetchJSON(!clusterEndTime, data);
        }, function (error, xhr) {
            if (!clusterEndTime && error == 404 && useCache)
                self._fetch(clusterEndTime, false);
            else
                self._failedToFetchJSON(clusterEndTime, error);
        });
    }

    _didFetchJSON(isPrimaryCluster, response, clusterEndTime)
    {
        console.assert(isPrimaryCluster || this._fetchedPrimary);

        if (isPrimaryCluster) {
            this._primaryClusterEndTime = response['endTime'];
            this._clusterCount = response['clusterCount'];
            this._clusterStart = response['clusterStart'];
            this._clusterSize = response['clusterSize'];
        } else
            console.assert(this._primaryClusterEndTime);

        this._addFetchedCluster(new MeasurementCluster(response));

        console.assert(this._waitingForPrimaryCluster);
        if (!isPrimaryCluster) {
            this._invokeCallbacks(response.endTime);
            return;
        }
        console.assert(this._waitingForPrimaryCluster instanceof Array);

        this._fetchedPrimary = true;
        for (var entry of this._waitingForPrimaryCluster)
            this._fetchSecondaryClusters(entry.startTime, entry.endTime, entry.callback);
        this._waitingForPrimaryCluster = true;
    }

    _failedToFetchJSON(clusterEndTime, error)
    {
        if (clusterEndTime) {
            this._invokeCallbacks(clusterEndTime);
            return;
        }

        console.assert(!this._fetchedPrimary);
        console.assert(this._waitingForPrimaryCluster instanceof Array);
        for (var entry of this._waitingForPrimaryCluster)
            entry.callback();
        this._waitingForPrimaryCluster = false;
    }

    _invokeCallbacks(clusterEndTime)
    {
        var callbackList = this._endTimeToCallback[clusterEndTime];
        for (var callback of callbackList)
            callback();
        this._endTimeToCallback[clusterEndTime] = true;
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

        if (extendToFuture && series._series.length) {
            var lastPoint = series._series[series._series.length - 1];
            series._series.push({
                series: series,
                seriesIndex: series._series.length,
                measurement: null,
                time: Date.now() + 365 * 24 * 3600 * 1000,
                value: lastPoint.value,
                interval: lastPoint.interval,
            });
        }

        Instrumentation.endMeasuringTime('MeasurementSet', 'fetchedTimeSeries');

        return series;
    }
}

TimeSeries.prototype.findById = function (id)
{
    return this._series.find(function (point) { return point.id == id });
}

TimeSeries.prototype.dataBetweenPoints = function (firstPoint, lastPoint)
{
    var data = this._series;
    var filteredData = [];
    for (var i = firstPoint.seriesIndex; i <= lastPoint.seriesIndex; i++) {
        if (!data[i].markedOutlier)
            filteredData.push(data[i]);
    }
    return filteredData;
}

TimeSeries.prototype.firstPoint = function ()
{
    if (!this._series || !this._series.length)
        return null;
    return this._series[0];
}
