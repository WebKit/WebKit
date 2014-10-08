// We don't use DS.Model for these object types because we can't afford to process millions of them.

FetchCommitsForTimeRange = function (repository, from, to)
{
    var url = '../api/commits/' + repository.get('id') + '/' + from + '-' + to;
    console.log('Fetching ' + url)
    return new Ember.RSVP.Promise(function (resolve, reject) {
        $.getJSON(url, function (data) {
            if (data.status != 'OK') {
                reject(data.status);
                return;
            }
            resolve(data.commits);
        }).fail(function (xhr, status, error) {
            reject(xhr.status + (error ? ', ' + error : ''));
        })
    });
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

    for (var repositoryName in revisions) {
        var commitTimeOrUndefined = revisions[repositoryName][1]; // e.g. ["162190", 1389945046000]
        if (latestTime < commitTimeOrUndefined)
            latestTime = commitTimeOrUndefined;
    }
    this._latestCommitTime = latestTime !== -1 ? new Date(latestTime) : null;
    this._buildTime = new Date(this._raw['buildTime']);
    this._confidenceInterval = undefined;
    this._formattedRevisions = undefined;
}

Measurement.prototype.formattedRevisions = function (previousMeasurement)
{
    var revisions = this._raw['revisions'];
    var previousRevisions = previousMeasurement ? previousMeasurement._raw['revisions'] : null;
    var formattedRevisions = {};
    for (var repositoryName in revisions) {
        var currentRevision = revisions[repositoryName][0];
        var commitTimeInPOSIX = revisions[repositoryName][1];

        var previousRevision = previousRevisions ? previousRevisions[repositoryName][0] : null;

        var formatttedRevision = this._formatRevisionRange(previousRevision, currentRevision);
        if (commitTimeInPOSIX)
            formatttedRevision['commitTime'] = new Date(commitTimeInPOSIX);
        formattedRevisions[repositoryName] = formatttedRevision;
    }

    return formattedRevisions;
}

Measurement.prototype._formatRevisionRange = function (previousRevision, currentRevision)
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
        formattedCurrentHash = currentRevision.substring(0, 8);
        if (previousRevision)
            label = previousRevision.substring(0, 8) + '..' + formattedCurrentHash;
        else
            label = 'At ' + formattedCurrentHash;
    }

    if (!label) {
        if (previousRevision)
            label = revisionPrefix + previousRevision + revisionDelimiter + revisionPrefix + currentRevision;
        else
            label = 'At ' + revisionPrefix + currentRevision;
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

function RunsData(rawData)
{
    this._measurements = rawData.map(function (run) { return new Measurement(run); });
}

RunsData.prototype.count = function ()
{
    return this._measurements.length;
}

RunsData.prototype.timeSeriesByCommitTime = function ()
{
    return new TimeSeries(this._measurements.map(function (measurement) {
        var confidenceInterval = measurement.confidenceInterval();
        return {
            measurement: measurement,
            time: measurement.latestCommitTime(),
            value: measurement.mean(),
            interval: measurement.confidenceInterval(),
        };
    }));
}

RunsData.prototype.timeSeriesByBuildTime = function ()
{
    return new TimeSeries(this._measurements.map(function (measurement) {
        return {
            measurement: measurement,
            time: measurement.buildTime(),
            value: measurement.mean(),
            interval: measurement.confidenceInterval(),
        };
    }));
}

// FIXME: We need to devise a way to fetch runs in multiple chunks so that
// we don't have to fetch the entire time series to just show the last 3 days.
RunsData.fetchRuns = function (platformId, metricId)
{
    var filename = platformId + '-' + metricId + '.json';

    return new Ember.RSVP.Promise(function (resolve, reject) {
        $.getJSON('../api/runs/' + filename, function (data) {
            if (data.status != 'OK') {
                reject(data.status);
                return;
            }
            delete data.status;

            for (var config in data)
                data[config] = new RunsData(data[config]);

            resolve(data);
        }).fail(function (xhr, status, error) {
            reject(xhr.status + (error ? ', ' + error : ''));
        })
    });
}

function TimeSeries(series)
{
    this._series = series.sort(function (a, b) { return a.time - b.time; });
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

TimeSeries.prototype.minMaxForTimeRange = function (startTime, endTime)
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

TimeSeries.prototype.previousPoint = function (point)
{
    if (!point.seriesIndex)
        return null;
    return this._series[point.seriesIndex - 1];
}
