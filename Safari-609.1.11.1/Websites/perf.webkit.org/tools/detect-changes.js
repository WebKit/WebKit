#!/usr/local/bin/node

var fs = require('fs');
var http = require('http');
var https = require('https');
var data = require('../public/v2/data.js');
var RunsData = data.RunsData;
var Statistics = require('../public/shared/statistics.js');
var StatisticsStrategies = require('../public/v2/statistics-strategies.js');
var parseArguments = require('./js/parse-arguments.js').parseArguments;

// FIXME: We shouldn't use a global variable like this.
var settings;
function main(argv)
{
    var options = parseArguments(argv, [
        {name: '--server-config-json', required: true},
        {name: '--change-detection-config-json', required: true},
        {name: '--seconds-to-sleep', type: parseFloat, default: 1200},
    ]);
    if (!options)
        return;

    settings = JSON.parse(fs.readFileSync(options['--change-detection-config-json'], 'utf8'));
    settings.secondsToSleep = options['--seconds-to-sleep'];

    fetchManifestAndAnalyzeData(options['--server-config-json']);
}

function fetchManifestAndAnalyzeData(serverConfigJSON)
{
    loadServerConfig(serverConfigJSON);

    getJSON(settings.perfserver, '/data/manifest.json').then(function (manifest) {
        return mapInOrder(configurationsForTesting(manifest), analyzeConfiguration);
    }).catch(function (reason) {
        console.error('Failed to obtain the manifest file', reason);
    }).then(function () {
        console.log('');
        console.log('Sleeing for', settings.secondsToSleep, 'seconds');
        setTimeout(function () {
            fetchManifestAndAnalyzeData(serverConfigJSON);
        }, settings.secondsToSleep * 1000);
    });
}

function loadServerConfig(serverConfigJSON)
{
    var serverConfig = JSON.parse(fs.readFileSync(serverConfigJSON, 'utf8'));

    var server = serverConfig['server'];
    if ('auth' in server)
        server['auth'] = server['auth']['username'] + ':' + server['auth']['password'];

    settings.perfserver = server;
    settings.slave = serverConfig['slave'];
}

function mapInOrder(array, callback, startIndex)
{
    if (startIndex === undefined)
        startIndex = 0;
    if (startIndex >= array.length)
        return;

    var next = function () { return mapInOrder(array, callback, startIndex + 1); };
    var returnValue = callback(array[startIndex]);
    if (typeof(returnValue) === 'object' && returnValue instanceof Promise)
        return returnValue.then(next).catch(next);
    return next();
}

function configurationsForTesting(manifest)
{
    var configurations = [];
    for (var name in manifest.dashboards) {
        var dashboard = manifest.dashboards[name];
        for (var row of dashboard) {
            for (var cell of row) {
                if (cell instanceof Array) {
                    var platformId = parseInt(cell[0]);
                    var metricId = parseInt(cell[1]);
                    if (!isNaN(platformId) && !isNaN(metricId))
                        configurations.push({platformId: platformId, metricId: metricId});
                }
            }
        }
    }

    var platforms = manifest.all;
    for (var config of configurations) {
        var metric = manifest.metrics[config.metricId];

        var testPath = [];
        var id = metric.test;
        while (id) {
            var test = manifest.tests[id];
            testPath.push(test.name);
            id = test.parentId;
        }

        config.unit = RunsData.unitFromMetricName(metric.name);
        config.smallerIsBetter = RunsData.isSmallerBetter(config.unit);
        config.platformName = platforms[config.platformId].name;
        config.testName = testPath.reverse().join(' > ');
        config.fullTestName = config.testName + ':' + metric.name;
        config.repositories = manifest.repositories;
        if (metric.aggregator)
            config.fullTestName += ':' + metric.aggregator;
    }

    return configurations;
}

function analyzeConfiguration(config)
{
    var minTime = Date.now() - settings.maxDays * 24 * 3600 * 1000;

    console.log('');
    console.log('== Analyzing the last', settings.maxDays, 'days:', config.fullTestName, 'on', config.platformName, '==');

    return computeRangesForTesting(settings.perfserver, settings.strategies, config.platformId, config.metricId).then(function (ranges) {
        var filteredRanges = ranges.filter(function (range) { return range.endTime >= minTime && !range.overlappingAnalysisTasks.length; })
            .sort(function (a, b) { return a.endTime - b.endTime });

        var summary;
        var range;
        for (range of filteredRanges) {
            var summary = summarizeRange(config, range);
            console.log('Detected:', summary);
        }

        if (!range) {
            console.log('Nothing to analyze');
            return;
        }

        return createAnalysisTaskAndNotify(config, range, summary);
    });
}

function computeRangesForTesting(server, strategies, platformId, metricId)
{
    // FIXME: Store the segmentation strategy on the server side.
    // FIXME: Configure each strategy.
    var segmentationStrategy = findStrategyByLabel(StatisticsStrategies.MovingAverageStrategies, strategies.segmentation.label);
    if (!segmentationStrategy) {
        console.error('Failed to find the segmentation strategy: ' + strategies.segmentation.label);
        return;
    }

    var testRangeStrategy = findStrategyByLabel(StatisticsStrategies.TestRangeSelectionStrategies, strategies.testRange.label);
    if (!testRangeStrategy) {
        console.error('Failed to find the test range selection strategy: ' + strategies.testRange.label);
        return;
    }

    var currentPromise = getJSON(server, RunsData.pathForFetchingRuns(platformId, metricId)).then(function (response) {
        if (response.status != 'OK')
            throw response;
        return RunsData.createRunsDataInResponse(response).configurations.current;
    }, function (reason) {
        console.error('Failed to fetch the measurements:', reason);
    });

    var analysisTasksPromise = getJSON(server, '/api/analysis-tasks?platform=' + platformId + '&metric=' + metricId).then(function (response) {
        if (response.status != 'OK')
            throw response;
        return response.analysisTasks.filter(function (task) { return task.startRun && task.endRun; });
    }, function (reason) {
        console.error('Failed to fetch the analysis tasks:', reason);
    });

    return Promise.all([currentPromise, analysisTasksPromise]).then(function (results) {
        var currentTimeSeries = results[0].timeSeriesByCommitTime();
        var analysisTasks = results[1];
        var rawValues = currentTimeSeries.rawValues();
        var segmentedValues = StatisticsStrategies.executeStrategy(segmentationStrategy, rawValues);

        var ranges = StatisticsStrategies.executeStrategy(testRangeStrategy, rawValues, [segmentedValues]).map(function (range) {
            var startPoint = currentTimeSeries.findPointByIndex(range[0]);
            var endPoint = currentTimeSeries.findPointByIndex(range[1]);
            return {
                startIndex: range[0],
                endIndex: range[1],
                overlappingAnalysisTasks: [],
                startTime: startPoint.time,
                endTime: endPoint.time,
                relativeChangeInSegmentedValues: (segmentedValues[range[1]] - segmentedValues[range[0]]) / segmentedValues[range[0]],
                startMeasurement: startPoint.measurement,
                endMeasurement: endPoint.measurement,
            };
        });

        for (var task of analysisTasks) {
            var taskStartPoint = currentTimeSeries.findPointByMeasurementId(task.startRun);
            var taskEndPoint = currentTimeSeries.findPointByMeasurementId(task.endRun);
            for (var range of ranges) {
                var disjoint = range.endIndex < taskStartPoint.seriesIndex
                    || taskEndPoint.seriesIndex < range.startIndex;
                if (!disjoint)
                    range.overlappingAnalysisTasks.push(task);
            }
        }

        return ranges;
    });
}

function createAnalysisTaskAndNotify(config, range, summary)
{
    var segmentationStrategy = settings.strategies.segmentation.label;
    var testRangeStrategy = settings.strategies.testRange.label;

    var analysisTaskData = {
        name: summary,
        startRun: range.startMeasurement.id(),
        endRun: range.endMeasurement.id(),
        segmentationStrategy: segmentationStrategy,
        testRangeStrategy: testRangeStrategy,

        slaveName: settings.slave.name,
        slavePassword: settings.slave.password,
    };

    return postJSON(settings.perfserver, '/privileged-api/create-analysis-task', analysisTaskData).then(function (response) {
        if (response['status'] != 'OK')
            throw response;

        var analysisTaskId = response['taskId'];

        var title = '[' + config.testName + '][' + config.platformName + '] ' + summary;
        var analysisTaskURL = settings.perfserver.scheme + '://' + settings.perfserver.host + '/v2/#/analysis/task/' + analysisTaskId;
        var changeType = changeTypeForRange(config, range);
        // FIXME: Templatize this.
        var message = '<b>' + settings.notification.serviceName + '</b> detected a potential ' + changeType + ':<br><br>'
            + '<table border=1><caption>' + summary + '</caption><tbody>'
            + '<tr><th>Test</th><td>' + config.fullTestName + '</td></tr>'
            + '<tr><th>Platform</th><td>' + config.platformName + '</td></tr>'
            + '<tr><th>Algorithm</th><td>' + segmentationStrategy + '<br>' + testRangeStrategy + '</td></tr>'
            + '</table><br>'
            + '<a href="' + analysisTaskURL + '">Open the analysis task</a>';

        return getJSON(settings.perfserver, '/api/triggerables?task=' + analysisTaskId).then(function (response) {
            var status = response['status'];
            var triggerables = response['triggerables'] || [];
            if (status == 'TriggerableNotFoundForTask' || triggerables.length != 1) {
                message += ' (A/B testing was not available)';
                return;
            }
            if (status != 'OK')
                throw response;

            var triggerable = response['triggerables'][0];
            var commitSets = {};
            for (var repositoryId of triggerable['acceptedRepositories']) {
                var startRevision = range.startMeasurement.revisionForRepository(repositoryId);
                var endRevision = range.endMeasurement.revisionForRepository(repositoryId);
                if (startRevision == null || endRevision == null)
                    continue;
                commitSets[config.repositories[repositoryId].name] = [startRevision, endRevision];
            }

            var testData = {
                task: analysisTaskId,
                name: 'Confirming the ' + changeType,
                commitSets: commitSets,
                repetitionCount: Math.max(2, Math.min(8, Math.floor((range.endIndex - range.startIndex) / 4))),

                slaveName: settings.slave.name,
                slavePassword: settings.slave.password,
            };

            return postJSON(settings.perfserver, '/privileged-api/create-test-group', testData).then(function (response) {
                if (response['status'] != 'OK')
                    throw response;
                message += ' (triggered an A/B testing)';
            });
        }).catch(function (reason) {
            console.error(reason);
            message += ' (failed to create a new A/B testing)';
        }).then(function () {
            return postNotification(settings.notification.server, settings.notification.template, title, message).then(function () {
                console.log('  Sent a notification');
            }, function (reason) {
                console.error('  Failed to send a notification', reason);
            });
        });
    }).catch(function (reason) {
        console.error('  Failed to create an analysis task', reason);
    });
}

function findStrategyByLabel(list, label)
{
    for (var strategy of list) {
        if (strategy.label == label)
            return strategy;
    }
    return null;
}

function changeTypeForRange(config, range)
{
    var endValueIsLarger = range.relativeChangeInSegmentedValues > 0;
    return endValueIsLarger == config.smallerIsBetter ? 'regression' : 'progression';
}

function summarizeRange(config, range)
{
    return 'Potential ' + Math.abs(range.relativeChangeInSegmentedValues * 100).toPrecision(2) + '% '
        + changeTypeForRange(config, range) + ' between ' + formatTimeRange(range.startTime, range.endTime);
}

function formatTimeRange(start, end)
{
    var formatter = function (date) { return date.toISOString().replace('T', ' ').replace(/:\d{2}\.\d+Z$/, ''); }
    var formattedStart = formatter(start);
    var formattedEnd = formatter(end);
    if (start.toDateString() == end.toDateString())
        return formattedStart + ' and ' + formattedEnd.substring(formattedEnd.indexOf(' ') + 1);
    if (start.getFullYear() == end.getFullYear())
        return formattedStart + ' and ' + formattedEnd.substring(5);
    return formattedStart + ' and ' + formattedEnd;
}

function getJSON(server, path, data)
{
    return fetchJSON(server.scheme, {
        'hostname': server.host,
        'port': server.port,
        'auth': server.auth,
        'path': path,
        'method': 'GET',
    }, 'application/json');
}

function postJSON(server, path, data)
{
    return fetchJSON(server.scheme, {
        'hostname': server.host,
        'port': server.port,
        'auth': server.auth,
        'path': path,
        'method': 'POST',
    }, 'application/json', JSON.stringify(data));
}

function postNotification(server, template, title, message)
{
    var notification = instantiateNotificationTemplate(template, title, message);
    return fetchJSON(server.scheme, {
        'hostname': server.host,
        'port': server.port,
        'auth': server.auth,
        'path': server.path,
        'method': server.method,
    }, 'application/json', JSON.stringify(notification));
}

function instantiateNotificationTemplate(template, title, message)
{
    var instance = {};
    for (var name in template) {
        var value = template[name];
        if (typeof(value) === 'string')
            instance[name] = value.replace(/\$title/g, title).replace(/\$message/g, message);
        else if (typeof(template[name]) === 'object')
            instance[name] = instantiateNotificationTemplate(value, title, message);
        else
            instance[name] = value;
    }
    return instance;
}

function fetchJSON(schemeName, options, contentType, content) {
    var requester = schemeName == 'https' ? https : http;
    return new Promise(function (resolve, reject) {
        var request = requester.request(options, function (response) {
            var responseText = '';
            response.setEncoding('utf8');
            response.on('data', function (chunk) { responseText += chunk; });
            response.on('end', function () {
                try {
                    var json = JSON.parse(responseText);
                } catch (error) {
                    reject({error: error, responseText: responseText});
                }
                resolve(json);
            });
        });
        request.on('error', function (error) { reject(error); });
        if (contentType)
            request.setHeader('Content-Type', contentType);
        if (content)
            request.write(content);
        request.end();
    });
}

main(process.argv);
