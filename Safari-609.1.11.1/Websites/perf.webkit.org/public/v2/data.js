// We don't use DS.Model for these object types because we can't afford to process millions of them.

if (!Array.prototype.find) {
    Array.prototype.find = function (callback) {
        for (var item of this) {
            if (callback(item))
                return item;
        }
        return undefined;
    }
}

var PrivilegedAPI = {
    _token: null,
    _expiration: null,
    _maxNetworkLatency: 3 * 60 * 1000 /* 3 minutes */,
};

PrivilegedAPI.sendRequest = function (url, parameters)
{
    return this._generateTokenInServerIfNeeded().then(function (token) {
        return PrivilegedAPI._post(url, $.extend({token: token}, parameters));
    });
}

PrivilegedAPI._generateTokenInServerIfNeeded = function ()
{
    var self = this;
    return new Ember.RSVP.Promise(function (resolve, reject) {
        if (self._token && self._expiration > Date.now() + self._maxNetworkLatency)
            resolve(self._token);

        PrivilegedAPI._post('generate-csrf-token')
            .then(function (result, reject) {
                self._token = result['token'];
                self._expiration = new Date(result['expiration']);
                resolve(self._token);
            }).catch(reject);
    });
}

PrivilegedAPI._post = function (url, parameters)
{
    return new Ember.RSVP.Promise(function (resolve, reject) {
        $.ajax({
            url: '../privileged-api/' + url,
            type: 'POST',
            contentType: 'application/json',
            data: parameters ? JSON.stringify(parameters) : '{}',
            dataType: 'json',
        }).done(function (data) {
            if (data.status != 'OK') {
                console.log('PrivilegedAPI failed', data);
                reject(data.status);
            } else
                resolve(data);
        }).fail(function (xhr, status, error) {
            reject(xhr.status + (error ? ', ' + error : '') + '\n\nWith response:\n' + xhr.responseText);
        });
    });
}

var CommitLogs = {
    _cachedCommitsByRepository: {}
};

CommitLogs.fetchCommits = function (repository, from, to, keyword)
{
    var params = [];
    if (from && to) {
        params.push(['from', from]);
        params.push(['to', to]);
    }
    if (keyword)
        params.push(['keyword', keyword]);

    // FIXME: We should be able to use the cache if all commits in the range have been cached.
    var useCache = from && to && !keyword;

    var url = '../api/commits/' + repository + '/?' + params.map(function (keyValue) {
        return encodeURIComponent(keyValue[0]) + '=' + encodeURIComponent(keyValue[1]);
    }).join('&');

    if (useCache) {
        var cachedCommitsForRange = CommitLogs._cachedCommitsBetween(repository, from, to);
        if (cachedCommitsForRange)
            return new Ember.RSVP.Promise(function (resolve) { resolve(cachedCommitsForRange); });
    }

    return new Ember.RSVP.Promise(function (resolve, reject) {
        $.getJSON(url, function (data) {
            if (data.status != 'OK') {
                reject(data.status);
                return;
            }

            var fetchedCommits = data.commits;
            fetchedCommits.forEach(function (commit) { commit.time = new Date(commit.time); });

            if (useCache)
                CommitLogs._cacheConsecutiveCommits(repository, from, to, fetchedCommits);

            resolve(fetchedCommits);
        }).fail(function (xhr, status, error) {
            reject(xhr.status + (error ? ', ' + error : ''));
        })
    });
}

CommitLogs._cachedCommitsBetween = function (repository, from, to)
{
    var cachedCommits = this._cachedCommitsByRepository[repository];
    if (!cachedCommits)
        return null;

    var startCommit = cachedCommits.commitsByRevision[from];
    var endCommit = cachedCommits.commitsByRevision[to];
    if (!startCommit || !endCommit)
        return null;

    return cachedCommits.commitsByTime.slice(startCommit.cacheIndex, endCommit.cacheIndex + 1);
}

CommitLogs._cacheConsecutiveCommits = function (repository, from, to, consecutiveCommits)
{
    var cachedCommits = this._cachedCommitsByRepository[repository];
    if (!cachedCommits) {
        cachedCommits = {commitsByRevision: {}, commitsByTime: []};
        this._cachedCommitsByRepository[repository] = cachedCommits;
    }

    consecutiveCommits.forEach(function (commit) {
        if (cachedCommits.commitsByRevision[commit.revision])
            return;
        cachedCommits.commitsByRevision[commit.revision] = commit;
        cachedCommits.commitsByTime.push(commit);
    });

    cachedCommits.commitsByTime.sort(function (a, b) { return a.time && b.time ? (a.time - b.time) : (a.order - b.order); });
    cachedCommits.commitsByTime.forEach(function (commit, index) { commit.cacheIndex = index; });
}


function Measurement(rawData)
{
    this._raw = rawData;

    var latestTime = -1;
    var revisions = this._raw['revisions'];
    // FIXME: Fix this in the server side.
    if (Array.isArray(revisions))
        revisions = {};
    this._raw['revisions'] = revisions;

    for (var repositoryId in revisions) {
        var commitTimeOrUndefined = revisions[repositoryId][1]; // e.g. ["162190", 1389945046000]
        if (commitTimeOrUndefined && latestTime < commitTimeOrUndefined)
            latestTime = commitTimeOrUndefined;
    }
    this._latestCommitTime = latestTime !== -1 ? new Date(latestTime) : null;
    this._buildTime = new Date(this._raw['buildTime']);
    this._confidenceInterval = undefined;
    this._formattedRevisions = undefined;
}

Measurement.prototype.revisionForRepository = function (repositoryId)
{
    var revisions = this._raw['revisions'];
    var rawData = revisions[repositoryId];
    return rawData ? rawData[0] : null;
}

Measurement.prototype.commitTimeForRepository = function (repositoryId)
{
    var revisions = this._raw['revisions'];
    var rawData = revisions[repositoryId];
    return rawData ? new Date(rawData[1]) : null;
}

Measurement.prototype.formattedRevisions = function (previousMeasurement)
{
    var revisions = this._raw['revisions'];
    var previousRevisions = previousMeasurement ? previousMeasurement._raw['revisions'] : null;
    var formattedRevisions = {};
    for (var repositoryId in revisions) {
        var currentRevision = revisions[repositoryId][0];
        var previousRevision = previousRevisions && previousRevisions[repositoryId] ? previousRevisions[repositoryId][0] : null;
        var formatttedRevision = Measurement.formatRevisionRange(currentRevision, previousRevision);
        formattedRevisions[repositoryId] = formatttedRevision;
    }

    return formattedRevisions;
}

Measurement.formatRevisionRange = function (currentRevision, previousRevision)
{
    var revisionChanged = false;
    if (previousRevision == currentRevision)
        previousRevision = null;
    else
        revisionChanged = true;

    var revisionPrefix = '';
    var revisionDelimiter = '-';
    var label = '';
    if (parseInt(currentRevision) == currentRevision) { // e.g. r12345.
        currentRevision = parseInt(currentRevision);
        revisionPrefix = 'r';
        if (previousRevision)
            previousRevision = (parseInt(previousRevision) + 1);
    } else if (currentRevision.indexOf(' ') >= 0) // e.g. 10.9 13C64.
        revisionDelimiter = ' - ';
    else if (currentRevision.length == 40) { // e.g. git hash
        var formattedCurrentHash = currentRevision.substring(0, 8);
        if (previousRevision)
            label = previousRevision.substring(0, 8) + '..' + formattedCurrentHash;
        else
            label = formattedCurrentHash;
    }

    if (!label) {
        if (previousRevision)
            label = revisionPrefix + previousRevision + revisionDelimiter + revisionPrefix + currentRevision;
        else
            label = revisionPrefix + currentRevision;
    }

    return {
        label: label,
        previousRevision: previousRevision,
        currentRevision: currentRevision,
        revisionChanged: revisionChanged
    };
}

Measurement.prototype.id = function ()
{
    return this._raw['id'];
}

Measurement.prototype.mean = function()
{
    return this._raw['mean'];
}

Measurement.prototype.confidenceInterval = function()
{
    if (this._confidenceInterval === undefined) {
        var delta = Statistics.confidenceIntervalDelta(0.95, this._raw["iterationCount"], this._raw["sum"], this._raw["squareSum"]);
        var mean = this.mean();
        this._confidenceInterval = isNaN(delta) ? null : [mean - delta, mean + delta];
    }
    return this._confidenceInterval;
}

Measurement.prototype.latestCommitTime = function()
{
    return this._latestCommitTime || this._buildTime;
}

Measurement.prototype.buildId = function()
{
    return this._raw['build'];
}

Measurement.prototype.buildNumber = function ()
{
    return this._raw['buildNumber'];
}

Measurement.prototype.builderId = function ()
{
    return this._raw['builder'];
}

Measurement.prototype.buildTime = function()
{
    return this._buildTime;
}

Measurement.prototype.formattedBuildTime = function ()
{
    return Measurement._formatDate(this.buildTime());
}

Measurement._formatDate = function (date)
{
    return date.toISOString().replace('T', ' ').replace(/\.\d+Z$/, '');
}

Measurement.prototype.bugs = function ()
{
    return this._raw['bugs'];
}

Measurement.prototype.hasBugs = function ()
{
    var bugs = this.bugs();
    return bugs && Object.keys(bugs).length;
}

Measurement.prototype.markedOutlier = function ()
{
    return this._raw['markedOutlier'];
}

Measurement.prototype.setMarkedOutlier = function (markedOutlier)
{
    var params = {'run': this.id(), 'markedOutlier': markedOutlier};
    return PrivilegedAPI.sendRequest('update-run-status', params).then(function (data) {
    }, function (error) {
        alert('Failed to update the outlier status: ' + error);
    });
}

function RunsData(rawData)
{
    this._measurements = rawData.map(function (run) { return new Measurement(run); });
}

RunsData.prototype.count = function ()
{
    return this._measurements.length;
}

RunsData.prototype.timeSeriesByCommitTime = function (includeOutliers, extendToFuture)
{
    return this._timeSeriesByTimeInternal(true, includeOutliers, extendToFuture);
}

RunsData.prototype.timeSeriesByBuildTime = function (includeOutliers, extendToFuture)
{
    return this._timeSeriesByTimeInternal(false, includeOutliers, extendToFuture);
}

RunsData.prototype._timeSeriesByTimeInternal = function (useCommitType, includeOutliers, extendToFuture)
{
    var series = new Array();
    var seriesIndex = 0;
    for (var measurement of this._measurements) {
        if (measurement.markedOutlier() && !includeOutliers)
            continue;
        series.push({
            measurement: measurement,
            time: useCommitType ? measurement.latestCommitTime() : measurement.buildTime(),
            secondaryTime: measurement.buildTime(),
            value: measurement.mean(),
            interval: measurement.confidenceInterval(),
            markedOutlier: measurement.markedOutlier(),
        });
    }
    return new TimeSeries(series, extendToFuture);
}

// FIXME: We need to devise a way to fetch runs in multiple chunks so that
// we don't have to fetch the entire time series to just show the last 3 days.
RunsData.fetchRuns = function (platformId, metricId, testGroupId, useCache)
{
    var url = this.pathForFetchingRuns(platformId, metricId, testGroupId, useCache);
    return new Ember.RSVP.Promise(function (resolve, reject) {
        $.getJSON(url, function (response) {
            if (response.status != 'OK') {
                reject(response.status);
                return;
            }
            resolve(RunsData.createRunsDataInResponse(response));
        }).fail(function (xhr, status, error) {
            if (xhr.status == 404 && useCache)
                resolve(null);
            else
                reject(xhr.status + (error ? ', ' + error : ''));
        })
    });
}

RunsData.pathForFetchingRuns = function (platformId, metricId, testGroupId, useCache)
{
    var path = useCache ? '/data/' : '/api/runs/';

    path += platformId + '-' + metricId + '.json';
    if (testGroupId)
        path += '?testGroup=' + testGroupId;

    return path;
}

RunsData.createRunsDataInResponse = function (response)
{
    delete response.status;

    var data = response.configurations;
    for (var config in data)
        data[config] = new RunsData(data[config]);

    if (response.lastModified)
        response.lastModified = new Date(response.lastModified);

    return response;
}

// FIXME: It was a mistake to put this in the client side. We should put this back in the JSON.
RunsData.unitFromMetricName = function (metricName)
{
    var suffix = metricName.match('([A-z][a-z]+|FrameRate)$')[0];
    var unit = {
        'FrameRate': 'fps',
        'Runs': '/s',
        'Time': 'ms',
        'Duration': 'ms',
        'Malloc': 'B',
        'Heap': 'B',
        'Allocations': 'B',
        'Size': 'B',
        'Score': 'pt',
    }[suffix];
    return unit;
}

RunsData.isSmallerBetter = function (unit)
{
    return unit != 'fps' && unit != '/s' && unit != 'pt';
}

// FIXME: Extending the baseline/target to the future should be a server-side configuration.
function TimeSeries(series, extendToFuture)
{
    this._series = series.sort(function (a, b) {
        var diff = a.time - b.time;
        return diff ? diff : a.secondaryTime - b.secondaryTime;
    });

    if (extendToFuture && this._series.length) {
        var lastPoint = this._series[this._series.length - 1];
        this._series.push({
            measurement: lastPoint.measurement,
            time: Date.now() + 24 * 3600 * 1000,
            secondaryTime: Date.now() + 24 * 3600 * 1000,
            value: lastPoint.value,
            interval: lastPoint.interval,
            markedOutlier: lastPoint.markedOutlier,
        });
    }

    var self = this;
    var min = undefined;
    var max = undefined;
    this._series.forEach(function (point, index) {
        point.series = self;
        point.seriesIndex = index;
        if (min === undefined || min > point.value)
            min = point.value;
        if (max === undefined || max < point.value)
            max = point.value;
    });
    this._min = min;
    this._max = max;
}

TimeSeries.prototype.findPointByIndex = function (index)
{
    if (!this._series || index < 0 || index >= this._series.length)
        return null;
    return this._series[index];
}

TimeSeries.prototype.findPointByBuild = function (buildId)
{
    return this._series.find(function (point) { return point.measurement.buildId() == buildId; })
}

TimeSeries.prototype.findPointByRevisions = function (revisions)
{
    return this._series.find(function (point, index) {
        for (var repositoryId in revisions) {
            if (point.measurement.revisionForRepository(repositoryId) != revisions[repositoryId])
                return false;
        }
        return true;
    });
}

TimeSeries.prototype.findPointByMeasurementId = function (measurementId)
{
    return this._series.find(function (point) { return point.measurement.id() == measurementId; });
}

TimeSeries.prototype.findPointAfterTime = function (time)
{
    return this._series.find(function (point) { return point.time >= time; });
}

TimeSeries.prototype.seriesBetweenPoints = function (startPoint, endPoint)
{
    if (typeof(startPoint.seriesIndex) != "number" || typeof(endPoint.seriesIndex) != "number")
        return null;
    return this._series.slice(startPoint.seriesIndex, endPoint.seriesIndex + 1);
}

TimeSeries.prototype.minMaxForTimeRange = function (startTime, endTime, ignoreOutlier)
{
    var data = this._series;
    var i = 0;
    if (startTime !== undefined) {
        for (i = 0; i < data.length; i++) {
            var point = data[i];
            if (point.time >= startTime)
                break;
        }
        if (i)
            i--;
    }
    var min = Number.MAX_VALUE;
    var max = Number.MIN_VALUE;
    for (; i < data.length; i++) {
        var point = data[i];
        if (point.isOutlier && ignoreOutlier)
            continue;
        var currentMin = point.interval ? point.interval[0] : point.value;
        var currentMax = point.interval ? point.interval[1] : point.value;

        if (currentMin < min)
            min = currentMin;
        if (currentMax > max)
            max = currentMax;

        if (point.time >= endTime)
            break;
    }
    return [min, max];
}

TimeSeries.prototype.series = function () { return this._series; }

TimeSeries.prototype.rawValues = function ()
{
    return this._series.map(function (point) { return point.value });
}

TimeSeries.prototype.lastPoint = function ()
{
    if (!this._series || !this._series.length)
        return null;
    return this._series[this._series.length - 1];
}

TimeSeries.prototype.previousPoint = function (point)
{
    if (!point.seriesIndex)
        return null;
    return this._series[point.seriesIndex - 1];
}

TimeSeries.prototype.nextPoint = function (point)
{
    if (!point.seriesIndex)
        return null;
    return this._series[point.seriesIndex + 1];
}

if (typeof module != 'undefined') {
    Statistics = require('../shared/statistics.js');
    module.exports.Measurement = Measurement;
    module.exports.RunsData = RunsData;
    module.exports.TimeSeries = TimeSeries;
}
