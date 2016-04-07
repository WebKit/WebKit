'use strict';

const assert = require('assert');
const crypto = require('crypto');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const connectToDatabaseInEveryTest = require('./resources/common-operations.js').connectToDatabaseInEveryTest;

describe("/api/measurement-set", function () {
    this.timeout(1000);
    TestServer.inject();
    connectToDatabaseInEveryTest();

    function queryPlatformAndMetric(platformName, metricName)
    {
        const db = TestServer.database();
        return Promise.all([
            db.selectFirstRow('platforms', {name: 'Mountain Lion'}),
            db.selectFirstRow('test_metrics', {name: 'Time'}),
        ]).then(function (result) {
            return {platformId: result[0]['id'], metricId: result[1]['id']};
        });
    }

    function format(formatMap, row)
    {
        var result = {};
        for (var i = 0; i < formatMap.length; i++) {
            var key = formatMap[i];
            if (key == 'id' || key == 'build' || key == 'builder')
                continue;
            result[key] = row[i];
        }
        return result;
    }

    let clusterStart = TestServer.testConfig().clusterStart;
    clusterStart = +Date.UTC(clusterStart[0], clusterStart[1] - 1, clusterStart[2], clusterStart[3], clusterStart[4]);

    let clusterSize = TestServer.testConfig().clusterSize;
    const DAY = 24 * 3600 * 1000;
    const YEAR = 365.24 * DAY;
    const MONTH = 30 * DAY;
    clusterSize = clusterSize[0] * YEAR + clusterSize[1] * MONTH + clusterSize[2] * DAY;

    function clusterTime(index) { return new Date(clusterStart + clusterSize * index); }

    const reportWithBuildTime = [{
        "buildNumber": "123",
        "buildTime": clusterTime(7.8).toISOString(),
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "Suite": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [1, 2, 3, 4, 5] }}
                    },
                }
            },
        }}];
    reportWithBuildTime.startTime = +clusterTime(7);

    const reportWithRevision = [{
        "buildNumber": "124",
        "buildTime": "2013-02-28T15:34:51",
        "revisions": {
            "WebKit": {
                "revision": "144000",
                "timestamp": clusterTime(10.3).toISOString(),
            },
        },
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "Suite": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [11, 12, 13, 14, 15] }}
                    }
                }
            },
        }}];

    const reportWithNewRevision = [{
        "buildNumber": "125",
        "buildTime": "2013-02-28T21:45:17",
        "revisions": {
            "WebKit": {
                "revision": "160609",
                "timestamp": clusterTime(12.1).toISOString()
            },
        },
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "Suite": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [16, 17, 18, 19, 20] }}
                    }
                }
            },
        }}];

    const reportWithAncentRevision = [{
        "buildNumber": "126",
        "buildTime": "2013-02-28T23:07:25",
        "revisions": {
            "WebKit": {
                "revision": "137793",
                "timestamp": clusterTime(1.8).toISOString()
            },
        },
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "Suite": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [21, 22, 23, 24, 25] }}
                    }
                }
            },
        }}];

    it("should reject when platform ID is missing", function (done) {
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?metric=${result.metricId}`);
        }).then(function (response) {
            assert.equal(response['status'], 'AmbiguousRequest');
            done();
        }).catch(done);
    });

    it("should reject when metric ID is missing", function (done) {
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?platform=${result.platformId}`);
        }).then(function (response) {
            assert.equal(response['status'], 'AmbiguousRequest');
            done();
        }).catch(done);
    });

    it("should reject an invalid platform name", function (done) {
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?platform=${result.platformId}a&metric=${result.metricId}`);
        }).then(function (response) {
            assert.equal(response['status'], 'InvalidPlatform');
            done();
        }).catch(done);
    });

    it("should reject an invalid metric name", function (done) {
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}b`);
        }).then(function (response) {
            assert.equal(response['status'], 'InvalidMetric');
            done();
        }).catch(done);
    });

    it("should be able to retrieve a reported value", function (done) {
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return TestServer.remoteAPI().getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then(function (response) {
            const buildTime = +(new Date(reportWithBuildTime[0]['buildTime']));

            assert.deepEqual(Object.keys(response).sort(),
                ['clusterCount', 'clusterSize', 'clusterStart',
                  'configurations', 'elapsedTime', 'endTime', 'formatMap', 'lastModified', 'startTime', 'status']);
            assert.equal(response['status'], 'OK');
            assert.equal(response['clusterCount'], 1);
            assert.deepEqual(response['formatMap'], [
                'id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier',
                'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder']);

            assert.equal(response['startTime'], reportWithBuildTime.startTime);
            assert(typeof(response['lastModified']) == 'number', 'lastModified time should be a numeric');

            assert.deepEqual(Object.keys(response['configurations']), ['current']);

            var currentRows = response['configurations']['current'];
            assert.equal(currentRows.length, 1);
            assert.equal(currentRows[0].length, response['formatMap'].length);
            assert.deepEqual(format(response['formatMap'], currentRows[0]), {
                mean: 3,
                iterationCount: 5,
                sum: 15,
                squareSum: 55,
                markedOutlier: false,
                revisions: [],
                commitTime: buildTime,
                buildTime: buildTime,
                buildNumber: '123'});
            done();
        }).catch(done);
    });

    it("should return return the right IDs for measurement, build, and builder", function (done) {
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            const db = TestServer.database();
            return Promise.all([
                db.selectAll('test_runs'),
                db.selectAll('builds'),
                db.selectAll('builders'),
                TestServer.remoteAPI().getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`),
            ]);
        }).then(function (result) {
            const runs = result[0];
            const builds = result[1];
            const builders = result[2];
            const response = result[3];

            assert.equal(runs.length, 1);
            assert.equal(builds.length, 1);
            assert.equal(builders.length, 1);
            const measurementId = runs[0]['id'];
            const buildId = builds[0]['id'];
            const builderId = builders[0]['id'];

            assert.equal(response['configurations']['current'].length, 1);
            const measurement = response['configurations']['current'][0];
            assert.equal(response['status'], 'OK');

            assert.equal(measurement[response['formatMap'].indexOf('id')], measurementId);
            assert.equal(measurement[response['formatMap'].indexOf('build')], buildId);
            assert.equal(measurement[response['formatMap'].indexOf('builder')], builderId);

            done();
        }).catch(done);
    });

    function postReports(reports, callback)
    {
        if (!reports.length)
            return callback();

        postJSON('/api/report/', reports[0], function (response) {
            assert.equal(response.statusCode, 200);
            assert.equal(JSON.parse(response.responseText)['status'], 'OK');

            postReports(reports.slice(1), callback);
        });
    }

    function queryPlatformAndMetricWithRepository(platformName, metricName, repositoryName)
    {
        const db = TestServer.database();
        return Promise.all([
            db.selectFirstRow('platforms', {name: platformName}),
            db.selectFirstRow('test_metrics', {name: metricName}),
            db.selectFirstRow('repositories', {name: repositoryName}),
        ]).then(function (result) {
            return {platformId: result[0]['id'], metricId: result[1]['id'], repositoryId: result[2]['id']};
        });
    }

    it("should order results by commit time", function (done) {
        const remote = TestServer.remoteAPI();
        let repositoryId;
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return remote.postJSON('/api/report/', reportWithBuildTime);
        }).then(function () {
            return remote.postJSON('/api/report/', reportWithRevision);
        }).then(function () {
            return queryPlatformAndMetricWithRepository('Mountain Lion', 'Time', 'WebKit');
        }).then(function (result) {
            repositoryId = result.repositoryId;
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then(function (response) {
            const currentRows = response['configurations']['current'];
            const buildTime = +(new Date(reportWithBuildTime[0]['buildTime']));
            const revisionTime = +(new Date(reportWithRevision[0]['revisions']['WebKit']['timestamp']));
            const revisionBuildTime = +(new Date(reportWithRevision[0]['buildTime']));

            assert.equal(currentRows.length, 2);
            assert.deepEqual(format(response['formatMap'], currentRows[0]), {
               mean: 13,
               iterationCount: 5,
               sum: 65,
               squareSum: 855,
               markedOutlier: false,
               revisions: [[1, repositoryId, '144000', revisionTime]],
               commitTime: revisionTime,
               buildTime: revisionBuildTime,
               buildNumber: '124' });
            assert.deepEqual(format(response['formatMap'], currentRows[1]), {
                mean: 3,
                iterationCount: 5,
                sum: 15,
                squareSum: 55,
                markedOutlier: false,
                revisions: [],
                commitTime: buildTime,
                buildTime: buildTime,
                buildNumber: '123' });
            done();
        }).catch(done);
    });

    function buildNumbers(parsedResult, config)
    {
        return parsedResult['configurations'][config].map(function (row) {
            return format(parsedResult['formatMap'], row)['buildNumber'];
        });
    }

    it("should include one data point after the current time range", function (done) {
        const remote = TestServer.remoteAPI();
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return remote.postJSON('/api/report/', reportWithAncentRevision);
        }).then(function () {
            return remote.postJSON('/api/report/', reportWithNewRevision);
        }).then(function () {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            assert.equal(response['clusterCount'], 2, 'should have two clusters');
            assert.deepEqual(buildNumbers(response, 'current'),
                [reportWithAncentRevision[0]['buildNumber'], reportWithNewRevision[0]['buildNumber']]);
            done();
        }).catch(done);
    });

    it("should always include one old data point before the current time range", function (done) {
        const remote = TestServer.remoteAPI();
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return remote.postJSON('/api/report/', reportWithBuildTime);
        }).then(function () {
            return remote.postJSON('/api/report/', reportWithAncentRevision);
        }).then(function () {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then(function (response) {
            assert.equal(response['clusterCount'], 2, 'should have two clusters');
            let currentRows = response['configurations']['current'];
            assert.equal(currentRows.length, 2, 'should contain two data points');
            assert.deepEqual(buildNumbers(response, 'current'), [reportWithAncentRevision[0]['buildNumber'], reportWithBuildTime[0]['buildNumber']]);
            done();
        }).catch(done);
    });


    it("should create cache results", function (done) {
        const remote = TestServer.remoteAPI();
        let cachePrefix;
        addBuilderForReport(reportWithBuildTime[0]).then(function () {
            return remote.postJSON('/api/report/', reportWithAncentRevision);
        }).then(function () {
            return remote.postJSON('/api/report/', reportWithRevision);
        }).then(function () {
            return remote.postJSON('/api/report/', reportWithNewRevision);
        }).then(function () {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then(function (result) {
            cachePrefix = '/data/measurement-set-' + result.platformId + '-' + result.metricId;
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then(function (newResult) {
            return remote.getJSONWithStatus(`${cachePrefix}.json`).then(function (cachedResult) {
                assert.deepEqual(newResult, cachedResult);
                return remote.getJSONWithStatus(`${cachePrefix}-${cachedResult['startTime']}.json`);
            }).then(function (oldResult) {
                var oldBuildNumbers = buildNumbers(oldResult, 'current');
                var newBuildNumbers = buildNumbers(newResult, 'current');
                assert(oldBuildNumbers.length >= 2, 'The old cluster should contain at least two data points');
                assert(newBuildNumbers.length >= 2, 'The new cluster should contain at least two data points');
                assert.deepEqual(oldBuildNumbers.slice(oldBuildNumbers.length - 2), newBuildNumbers.slice(0, 2),
                    'Two conseqcutive clusters should share two data points');
                done();
            });
        }).catch(done);
    });

});
