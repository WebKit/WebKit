describe("/api/measurement-set", function () {
    function addBuilder(report, callback) {
        queryAndFetchAll('INSERT INTO builders (builder_name, builder_password_hash) values ($1, $2)',
            [report[0].builderName, sha256(report[0].builderPassword)], callback);
    }

    function queryPlatformAndMetric(platformName, metricName, callback) {
        queryAndFetchAll('SELECT * FROM platforms WHERE platform_name = $1', [platformName], function (platformRows) {
            queryAndFetchAll('SELECT * FROM test_metrics WHERE metric_name = $1', [metricName], function (metricRows) {
                callback(platformRows[0]['platform_id'], metricRows[0]['metric_id']);
            });
        });
    }

    function format(formatMap, row) {
        var result = {};
        for (var i = 0; i < formatMap.length; i++) {
            var key = formatMap[i];
            if (key == 'id' || key == 'build' || key == 'builder')
                continue;
            result[key] = row[i];
        }
        return result;
    }

    var clusterStart = config('clusterStart');
    clusterStart = +Date.UTC(clusterStart[0], clusterStart[1] - 1, clusterStart[2], clusterStart[3], clusterStart[4]);

    var clusterSize = config('clusterSize');
    var DAY = 24 * 3600 * 1000;
    var YEAR = 365.24 * DAY;
    var MONTH = 30 * DAY;
    clusterSize = clusterSize[0] * YEAR + clusterSize[1] * MONTH + clusterSize[2] * DAY;

    function clusterTime(index) { return new Date(clusterStart + clusterSize * index); }

    var reportWithBuildTime = [{
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

    var reportWithRevision = [{
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

    var reportWithNewRevision = [{
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

    var reportWithAncentRevision = [{
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

    it("should reject when platform ID is missing", function () {
        addBuilder(reportWithBuildTime, function () {
            postJSON('/api/report/', reportWithBuildTime, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryPlatformAndMetric('Mountain Lion', 'Time', function (platformId, metricId) {
                    httpGet('/api/measurement-set/?metric=' + metricId, function (response) {
                        assert.notEqual(JSON.parse(response.responseText)['status'], 'InvalidMetric');
                        notifyDone();
                    });
                });

            });
        });
    });

    it("should reject when metric ID is missing", function () {
        addBuilder(reportWithBuildTime, function () {
            postJSON('/api/report/', reportWithBuildTime, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryPlatformAndMetric('Mountain Lion', 'Time', function (platformId, metricId) {
                    httpGet('/api/measurement-set/?platform=' + platformId, function (response) {
                        assert.notEqual(JSON.parse(response.responseText)['status'], 'InvalidPlatform');
                        notifyDone();
                    });
                });

            });
        });
    });

    it("should reject an invalid platform name", function () {
        addBuilder(reportWithBuildTime, function () {
            postJSON('/api/report/', reportWithBuildTime, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryPlatformAndMetric('Mountain Lion', 'Time', function (platformId, metricId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + 'a&metric=' + metricId, function (response) {
                        assert.equal(JSON.parse(response.responseText)['status'], 'InvalidPlatform');
                        notifyDone();
                    });
                });

            });
        });
    });

    it("should reject an invalid metric name", function () {
        addBuilder(reportWithBuildTime, function () {
            postJSON('/api/report/', reportWithBuildTime, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryPlatformAndMetric('Mountain Lion', 'Time', function (platformId, metricId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId + 'b', function (response) {
                        assert.equal(JSON.parse(response.responseText)['status'], 'InvalidMetric');
                        notifyDone();
                    });
                });

            });
        });
    });

    it("should be able to retrieve a reported value", function () {
        addBuilder(reportWithBuildTime, function () {
            postJSON('/api/report/', reportWithBuildTime, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryPlatformAndMetric('Mountain Lion', 'Time', function (platformId, metricId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId, function (response) {
                        try {
                            var paresdResult = JSON.parse(response.responseText);
                        } catch (error) {
                            assert.fail(error, null, response.responseText);
                        }

                        var buildTime = +(new Date(reportWithBuildTime[0]['buildTime']));

                        assert.deepEqual(Object.keys(paresdResult).sort(),
                            ['clusterCount', 'clusterSize', 'clusterStart',
                              'configurations', 'elapsedTime', 'endTime', 'formatMap', 'lastModified', 'startTime', 'status']);
                        assert.equal(paresdResult['status'], 'OK');
                        assert.equal(paresdResult['clusterCount'], 1);
                        assert.deepEqual(paresdResult['formatMap'], [
                            'id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier',
                            'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder']);

                        assert.equal(paresdResult['startTime'], reportWithBuildTime.startTime);
                        assert(typeof(paresdResult['lastModified']) == 'number', 'lastModified time should be a numeric');

                        assert.deepEqual(Object.keys(paresdResult['configurations']), ['current']);

                        var currentRows = paresdResult['configurations']['current'];
                        assert.equal(currentRows.length, 1);
                        assert.equal(currentRows[0].length, paresdResult['formatMap'].length);
                        assert.deepEqual(format(paresdResult['formatMap'], currentRows[0]), {
                            mean: 3,
                            iterationCount: 5,
                            sum: 15,
                            squareSum: 55,
                            markedOutlier: false,
                            revisions: [],
                            commitTime: buildTime,
                            buildTime: buildTime,
                            buildNumber: '123'});
                        notifyDone();
                    });
                });

            });
        });
    });

    it("should return return the right IDs for measurement, build, and builder", function () {
        addBuilder(reportWithBuildTime, function () {
            postJSON('/api/report/', reportWithBuildTime, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryPlatformAndMetric('Mountain Lion', 'Time', function (platformId, metricId) {
                    queryAndFetchAll('SELECT * FROM test_runs', [], function (runs) {
                        assert.equal(runs.length, 1);
                        var measurementId = runs[0]['run_id'];
                        queryAndFetchAll('SELECT * FROM builds', [], function (builds) {
                            assert.equal(builds.length, 1);
                            var buildId = builds[0]['build_id'];
                            queryAndFetchAll('SELECT * FROM builders', [], function (builders) {
                                assert.equal(builders.length, 1);
                                var builderId = builders[0]['builder_id'];
                                httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId, function (response) {
                                    var paresdResult = JSON.parse(response.responseText);
                                    assert.equal(paresdResult['configurations']['current'].length, 1);
                                    var measurement = paresdResult['configurations']['current'][0];
                                    assert.equal(paresdResult['status'], 'OK');
                                    assert.equal(measurement[paresdResult['formatMap'].indexOf('id')], measurementId);
                                    assert.equal(measurement[paresdResult['formatMap'].indexOf('build')], buildId);
                                    assert.equal(measurement[paresdResult['formatMap'].indexOf('builder')], builderId);
                                    notifyDone();
                                });
                            });
                        });
                    });
                });
            });
        });
    });

    function postReports(reports, callback) {
        if (!reports.length)
            return callback();

        postJSON('/api/report/', reports[0], function (response) {
            assert.equal(response.statusCode, 200);
            assert.equal(JSON.parse(response.responseText)['status'], 'OK');

            postReports(reports.slice(1), callback);
        });
    }

    function queryPlatformAndMetricWithRepository(platformName, metricName, repositoryName, callback) {
        queryPlatformAndMetric(platformName, metricName, function (platformId, metricId) {
            queryAndFetchAll('SELECT * FROM repositories WHERE repository_name = $1', [repositoryName], function (rows) {
                callback(platformId, metricId, rows[0]['repository_id']);
            });
        });
    }

    it("should order results by commit time", function () {
        addBuilder(reportWithBuildTime, function () {
            postReports([reportWithBuildTime, reportWithRevision], function () {
                queryPlatformAndMetricWithRepository('Mountain Lion', 'Time', 'WebKit', function (platformId, metricId, repositoryId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId, function (response) {
                        var parsedResult = JSON.parse(response.responseText);
                        assert.equal(parsedResult['status'], 'OK');

                        var buildTime = +(new Date(reportWithBuildTime[0]['buildTime']));
                        var revisionTime = +(new Date(reportWithRevision[0]['revisions']['WebKit']['timestamp']));
                        var revisionBuildTime = +(new Date(reportWithRevision[0]['buildTime']));

                        var currentRows = parsedResult['configurations']['current'];
                        assert.equal(currentRows.length, 2);
                        assert.deepEqual(format(parsedResult['formatMap'], currentRows[0]), {
                           mean: 13,
                           iterationCount: 5,
                           sum: 65,
                           squareSum: 855,
                           markedOutlier: false,
                           revisions: [[1, repositoryId, '144000', revisionTime]],
                           commitTime: revisionTime,
                           buildTime: revisionBuildTime,
                           buildNumber: '124' });
                        assert.deepEqual(format(parsedResult['formatMap'], currentRows[1]), {
                            mean: 3,
                            iterationCount: 5,
                            sum: 15,
                            squareSum: 55,
                            markedOutlier: false,
                            revisions: [],
                            commitTime: buildTime,
                            buildTime: buildTime,
                            buildNumber: '123' });
                        notifyDone();
                    });
                });                
            });
        });
    });

    function buildNumbers(parsedResult, config) {
        return parsedResult['configurations'][config].map(function (row) {
            return format(parsedResult['formatMap'], row)['buildNumber'];
        });
    }

    it("should include one data point after the current time range", function () {
        addBuilder(reportWithBuildTime, function () {
            postReports([reportWithAncentRevision, reportWithNewRevision], function () {
                queryPlatformAndMetricWithRepository('Mountain Lion', 'Time', 'WebKit', function (platformId, metricId, repositoryId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId, function (response) {
                        var parsedResult = JSON.parse(response.responseText);
                        assert.equal(parsedResult['status'], 'OK');
                        assert.equal(parsedResult['clusterCount'], 2, 'should have two clusters');
                        assert.deepEqual(buildNumbers(parsedResult, 'current'),
                            [reportWithAncentRevision[0]['buildNumber'], reportWithNewRevision[0]['buildNumber']]);
                        notifyDone();
                    });
                });                
            });
        });
    });

    // FIXME: This test assumes a cluster step of 2-3 months
    it("should always include one old data point before the current time range", function () {
        addBuilder(reportWithBuildTime, function () {
            postReports([reportWithBuildTime, reportWithAncentRevision], function () {
                queryPlatformAndMetricWithRepository('Mountain Lion', 'Time', 'WebKit', function (platformId, metricId, repositoryId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId, function (response) {
                        var parsedResult = JSON.parse(response.responseText);
                        assert.equal(parsedResult['status'], 'OK');
                        assert.equal(parsedResult['clusterCount'], 2, 'should have two clusters');

                        var currentRows = parsedResult['configurations']['current'];
                        assert.equal(currentRows.length, 2, 'should contain at least two data points');
                        assert.deepEqual(buildNumbers(parsedResult, 'current'),
                            [reportWithAncentRevision[0]['buildNumber'], reportWithBuildTime[0]['buildNumber']]);
                        notifyDone();
                    });
                });                
            });
        });
    });

    // FIXME: This test assumes a cluster step of 2-3 months
    it("should create cache results", function () {
        addBuilder(reportWithBuildTime, function () {
            postReports([reportWithAncentRevision, reportWithRevision, reportWithNewRevision], function () {
                queryPlatformAndMetricWithRepository('Mountain Lion', 'Time', 'WebKit', function (platformId, metricId, repositoryId) {
                    httpGet('/api/measurement-set/?platform=' + platformId + '&metric=' + metricId, function (response) {
                        var parsedResult = JSON.parse(response.responseText);
                        assert.equal(parsedResult['status'], 'OK');
                        var cachePrefix = '/data/measurement-set-' + platformId + '-' + metricId;
                        httpGet(cachePrefix + '.json', function (response) {
                            var parsedCachedResult = JSON.parse(response.responseText);
                            assert.deepEqual(parsedResult, parsedCachedResult);

                            httpGet(cachePrefix + '-' + parsedResult['startTime'] + '.json', function (response) {
                                var parsedOldResult = JSON.parse(response.responseText);

                                var oldBuildNumbers = buildNumbers(parsedOldResult, 'current');
                                var newBuildNumbers = buildNumbers(parsedResult, 'current');
                                assert(oldBuildNumbers.length >= 2, 'The old cluster should contain at least two data points');
                                assert(newBuildNumbers.length >= 2, 'The new cluster should contain at least two data points');
                                assert.deepEqual(oldBuildNumbers.slice(oldBuildNumbers.length - 2), newBuildNumbers.slice(0, 2),
                                    'Two conseqcutive clusters should share two data points');

                                notifyDone();
                            });
                        });
                    });
                });                
            });
        });
    });

});
