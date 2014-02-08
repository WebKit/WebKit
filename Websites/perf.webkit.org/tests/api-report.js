describe("/api/report", function () {
    var emptyReport = [{
        "buildNumber": "123",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {},
        "revisions": {
            "OS X": {
                "revision": "10.8.2 12C60"
            },
            "WebKit": {
                "revision": "141977",
                "timestamp": "2013-02-06T08:55:20.9Z"
            }
        }}];

    function addBuilder(report, callback) {
        queryAndFetchAll('INSERT INTO builders (builder_name, builder_password_hash) values ($1, $2)',
            [report[0].builderName, sha256(report[0].builderPassword)], callback);
    }

    it("should reject error when builder name is missing", function () {
        postJSON('/api/report/', [{"buildTime": "2013-02-28T10:12:03.388304"}], function (response) {
            assert.equal(response.statusCode, 200);
            assert.equal(JSON.parse(response.responseText)['status'], 'MissingBuilderName');
            notifyDone();
        });
    });

    it("should reject error when build time is missing", function () {
        addBuilder(emptyReport, function () {
            postJSON('/api/report/', [{"builderName": "someBuilder", "builderPassword": "somePassword"}], function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'MissingBuildTime');
                notifyDone();
            });
        });
    });

    it("should reject when there are no builders", function () {
        postJSON('/api/report/', emptyReport, function (response) {
            assert.equal(response.statusCode, 200);
            assert.notEqual(JSON.parse(response.responseText)['status'], 'OK');
            assert.equal(JSON.parse(response.responseText)['failureStored'], false);
            assert.equal(JSON.parse(response.responseText)['processedRuns'], 0);

            queryAndFetchAll('SELECT COUNT(*) from reports', [], function (rows) {
                assert.equal(rows[0].count, 0);
                notifyDone();
            });
        });
    });

    it("should store a report from a valid builder", function () {
        addBuilder(emptyReport, function () {
            postJSON('/api/report/', emptyReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                assert.equal(JSON.parse(response.responseText)['failureStored'], false);
                assert.equal(JSON.parse(response.responseText)['processedRuns'], 1);
                queryAndFetchAll('SELECT COUNT(*) from reports', [], function (rows) {
                    assert.equal(rows[0].count, 1);
                    notifyDone();
                });
            });
        });
    });

    it("should store the builder name but not the builder password", function () {
        addBuilder(emptyReport, function () {
            postJSON('/api/report/', emptyReport, function (response) {
                queryAndFetchAll('SELECT report_content from reports', [], function (rows) {
                    var storedContent = JSON.parse(rows[0].report_content);
                    assert.equal(storedContent['builderName'], emptyReport[0]['builderName']);
                    assert(!('builderPassword' in storedContent));
                    notifyDone();
                });
            });
        });
    });

    it("should add a build", function () {
        addBuilder(emptyReport, function () {
            postJSON('/api/report/', emptyReport, function (response) {
                queryAndFetchAll('SELECT * from builds', [], function (rows) {
                    assert.strictEqual(rows[0].build_number, 123);
                    notifyDone();
                });
            });
        });
    });

    it("should add the platform", function () {
        addBuilder(emptyReport, function () {
            postJSON('/api/report/', emptyReport, function (response) {
                queryAndFetchAll('SELECT * from platforms', [], function (rows) {
                    assert.strictEqual(rows[0].platform_name, 'Mountain Lion');
                    notifyDone();
                });
            });
        });
    });

    it("should add repositories and build revisions", function () {
        addBuilder(emptyReport, function () {
            postJSON('/api/report/', emptyReport, function (response) {
                queryAndFetchAll('SELECT * FROM repositories', [], function (rows) {
                    assert.deepEqual(rows.map(function (row) { return row['repository_name']; }), ['OS X', 'WebKit']);

                    var repositoryIdToName = {};
                    rows.forEach(function (row) { repositoryIdToName[row['repository_id']] = row['repository_name']; });
                    queryAndFetchAll('SELECT * FROM build_revisions', [], function (rows) {
                        var repositoryNameToRevisionRow = {};
                        rows.forEach(function (row) {
                            repositoryNameToRevisionRow[repositoryIdToName[row['revision_repository']]] = row;
                        });
                        assert.equal(repositoryNameToRevisionRow['OS X']['revision_value'], '10.8.2 12C60');
                        assert.equal(repositoryNameToRevisionRow['WebKit']['revision_value'], '141977');
                        assert.equal(repositoryNameToRevisionRow['WebKit']['revision_time'].toString(),
                            new Date('2013-02-06 08:55:20.9').toString());
                        notifyDone();
                    });
                });
            });
        });
    });

    it("should not create a duplicate build for the same build number if build times are close", function () {
        addBuilder(emptyReport, function () {
            emptyReport[0]['buildTime'] = '2013-02-28T10:12:04';
            postJSON('/api/report/', emptyReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');

                emptyReport[0]['buildTime'] = '2013-02-28T10:22:03';
                postJSON('/api/report/', emptyReport, function (response) {
                    assert.equal(response.statusCode, 200);
                    assert.equal(JSON.parse(response.responseText)['status'], 'OK');

                    queryAndFetchAll('SELECT * FROM builds', [], function (rows) {
                        assert.equal(rows.length, 1);
                        notifyDone();
                    });
                });
            });
        });
    });

    it("should create distinct builds for the same build number if build times are far apart", function () {
        addBuilder(emptyReport, function () {
            emptyReport[0]['buildTime'] = '2013-02-28T10:12:03';
            postJSON('/api/report/', emptyReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');

                queryAndFetchAll('SELECT * FROM builds', [], function (rows) {
                    assert.equal(rows.length, 1);

                    emptyReport[0]['buildTime'] = '2014-01-20T22:23:34';
                    postJSON('/api/report/', emptyReport, function (response) {
                        assert.equal(response.statusCode, 200);
                        assert.equal(JSON.parse(response.responseText)['status'], 'OK');

                        queryAndFetchAll('SELECT * FROM builds', [], function (rows) {
                            assert.equal(rows.length, 2);
                            notifyDone();
                        });
                    });
                });
            });
        });
    });

    it("should reject a report with mismatching revision info", function () {
        addBuilder(emptyReport, function () {
            emptyReport[0]['revisions'] = {
                "WebKit": {
                    "revision": "141977",
                    "timestamp": "2013-02-06T08:55:20.96Z"
                }
            }
            postJSON('/api/report/', emptyReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');

                emptyReport[0]['revisions'] = {
                    "WebKit": {
                        "revision": "150000",
                        "timestamp": "2013-05-13T10:50:29.6Z"
                    }
                }
                postJSON('/api/report/', emptyReport, function (response) {
                    assert.equal(response.statusCode, 200);
                    assert.notEqual(JSON.parse(response.responseText)['status'], 'OK');
                    assert(response.responseText.indexOf('141977') >= 0);
                    assert(response.responseText.indexOf('150000') >= 0);
                    assert.equal(JSON.parse(response.responseText)['failureStored'], true);
                    assert.equal(JSON.parse(response.responseText)['processedRuns'], 0);
                    notifyDone();
                });
            });
        });
    });

    var reportWithTwoLevelsOfAggregations = [{
        "buildNumber": "123",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "DummyPageLoading": {
                "metrics": {"Time": { "aggregators" : ["Arithmetic"], "current": [300, 310, 320, 330] }},
                "tests": {
                    "apple.com": {
                        "metrics": {"Time": { "current": [500, 510, 520, 530] }},
                        "url": "http://www.apple.com"
                    },
                    "webkit.org": {
                        "metrics": {"Time": { "current": [100, 110, 120, 130] }},
                        "url": "http://www.webkit.org"
                    }
                }
            },
            "DummyBenchmark": {
                "metrics": {"Time": ["Arithmetic"]},
                "tests": {
                    "DOM": {
                        "metrics": {"Time": ["Geometric", "Arithmetic"]},
                        "tests": {
                            "ModifyNodes": {"metrics": {"Time": { "current": [[11, 12, 13, 14, 15], [16, 17, 18, 19, 20], [21, 22, 23, 24, 25]] }}},
                            "TraverseNodes": {"metrics": {"Time": { "current": [[31, 32, 33, 34, 35], [36, 37, 38, 39, 40], [41, 42, 43, 44, 45]] }}}
                        }
                    },
                    "CSS": {"metrics": {"Time": { "current": [[101, 102, 103, 104, 105], [106, 107, 108, 109, 110], [111, 112, 113, 114, 115]] }}}
                }
            }
        },
        "revisions": {
            "OS X": {
                "revision": "10.8.2 12C60"
            },
            "WebKit": {
                "revision": "141977",
                "timestamp": "2013-02-06T08:55:20.9Z"
            }
        }}];

    function addBuilderAndMeanAggregators(report, callback) {
        addBuilder(report, function () {
            queryAndFetchAll('INSERT INTO aggregators (aggregator_name, aggregator_definition) values ($1, $2)',
                ['Arithmetic', 'values.reduce(function (a, b) { return a + b; }) / values.length'], function () {
                queryAndFetchAll('INSERT INTO aggregators (aggregator_name, aggregator_definition) values ($1, $2)',
                    ['Geometric', 'Math.pow(values.reduce(function (a, b) { return a * b; }), 1 / values.length)'], callback);
            });
        });
    }

    function fetchRunForMetric(testName, metricName,callback) {
        queryAndFetchAll('SELECT * FROM test_runs WHERE run_config IN'
            + '(SELECT config_id FROM test_configurations, test_metrics, tests WHERE config_metric = metric_id AND metric_test = test_id AND'
            + 'test_name = $1 AND metric_name = $2)',
            ['Arithmetic', 'values.reduce(function (a, b) { return a + b; }) / values.length'], function () {
            queryAndFetchAll('INSERT INTO aggregators (aggregator_name, aggregator_definition) values ($1, $2)',
                ['Geometric', 'Math.pow(values.reduce(function (a, b) { return a * b; }), 1 / values.length)'], callback);
        });
    }

    it("should reject when aggregators are missing", function () {
        addBuilder(reportWithTwoLevelsOfAggregations, function () {
            postJSON('/api/report/', reportWithTwoLevelsOfAggregations, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'AggregatorNotFound');
                assert.equal(JSON.parse(response.responseText)['failureStored'], true);
                notifyDone();
            });
        });
    });

    it("should add tests", function () {
        addBuilderAndMeanAggregators(reportWithTwoLevelsOfAggregations, function () {
            postJSON('/api/report/', reportWithTwoLevelsOfAggregations, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                assert.equal(JSON.parse(response.responseText)['failureStored'], false);
                queryAndFetchAll('SELECT * FROM tests', [], function (rows) {
                    assert.deepEqual(rows.map(function (row) { return row['test_name']; }).sort(),
                        ['CSS', 'DOM', 'DummyBenchmark', 'DummyPageLoading', 'ModifyNodes', 'TraverseNodes', 'apple.com', 'webkit.org']);
                    emptyReport[0].tests = {};
                    notifyDone();
                });
            });
        });
    });

    it("should add metrics", function () {
        addBuilderAndMeanAggregators(reportWithTwoLevelsOfAggregations, function () {
            postJSON('/api/report/', reportWithTwoLevelsOfAggregations, function (response) {
                queryAndFetchAll('SELECT * FROM tests, test_metrics LEFT JOIN aggregators ON metric_aggregator = aggregator_id WHERE metric_test = test_id',
                    [], function (rows) {
                    var testNameToMetrics = {};
                    rows.forEach(function (row) {
                        if (!(row['test_name'] in testNameToMetrics))
                            testNameToMetrics[row['test_name']] = new Array;
                        testNameToMetrics[row['test_name']].push([row['metric_name'], row['aggregator_name']]);
                    });
                    assert.deepEqual(testNameToMetrics['CSS'], [['Time', null]]);
                    assert.deepEqual(testNameToMetrics['DOM'].sort(), [['Time', 'Arithmetic'], ['Time', 'Geometric']]);
                    assert.deepEqual(testNameToMetrics['DummyBenchmark'], [['Time', 'Arithmetic']]);
                    assert.deepEqual(testNameToMetrics['DummyPageLoading'], [['Time', 'Arithmetic']]);
                    assert.deepEqual(testNameToMetrics['ModifyNodes'], [['Time', null]]);
                    assert.deepEqual(testNameToMetrics['TraverseNodes'], [['Time', null]]);
                    assert.deepEqual(testNameToMetrics['apple.com'], [['Time', null]]);
                    assert.deepEqual(testNameToMetrics['webkit.org'], [['Time', null]]);
                    notifyDone();
                });
            });
        });
    });

    function fetchTestRunIterationsForMetric(testName, metricName, callback) {
         queryAndFetchAll('SELECT * FROM tests, test_metrics, test_configurations, test_runs WHERE metric_test = test_id AND config_metric = metric_id'
            + ' AND run_config = config_id AND test_name = $1 AND metric_name = $2', [testName, metricName], function (runRows) {
                assert.equal(runRows.length, 1);
                var run = runRows[0];
                queryAndFetchAll('SELECT * FROM run_iterations WHERE iteration_run = $1 ORDER BY iteration_order', [run['run_id']], function (iterationRows) {
                    callback(run, iterationRows);
                });
            });
    }

    it("should store run values", function () {
        addBuilderAndMeanAggregators(reportWithTwoLevelsOfAggregations, function () {
            postJSON('/api/report/', reportWithTwoLevelsOfAggregations, function (response) {
                fetchTestRunIterationsForMetric('apple.com', 'Time', function (run, iterations) {
                    var runId = run['run_id'];
                    assert.deepEqual(iterations, [
                        {iteration_run: runId, iteration_order: 0, iteration_group: null, iteration_value: 500, iteration_relative_time: null},
                        {iteration_run: runId, iteration_order: 1, iteration_group: null, iteration_value: 510, iteration_relative_time: null},
                        {iteration_run: runId, iteration_order: 2, iteration_group: null, iteration_value: 520, iteration_relative_time: null},
                        {iteration_run: runId, iteration_order: 3, iteration_group: null, iteration_value: 530, iteration_relative_time: null}]);
                    var sum = 500 + 510 + 520 + 530;
                    assert.equal(run['run_mean_cache'], sum / iterations.length);
                    assert.equal(run['run_sum_cache'], sum);
                    assert.equal(run['run_square_sum_cache'], 500 * 500 + 510 * 510 + 520 * 520 + 530 * 530);

                    fetchTestRunIterationsForMetric('CSS', 'Time', function (run, iterations) {
                        var runId = run['run_id'];
                        assert.deepEqual(iterations, [
                            {iteration_run: runId, iteration_order: 0, iteration_group: 0, iteration_value: 101, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 1, iteration_group: 0, iteration_value: 102, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 2, iteration_group: 0, iteration_value: 103, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 3, iteration_group: 0, iteration_value: 104, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 4, iteration_group: 0, iteration_value: 105, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 5, iteration_group: 1, iteration_value: 106, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 6, iteration_group: 1, iteration_value: 107, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 7, iteration_group: 1, iteration_value: 108, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 8, iteration_group: 1, iteration_value: 109, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 9, iteration_group: 1, iteration_value: 110, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 10, iteration_group: 2, iteration_value: 111, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 11, iteration_group: 2, iteration_value: 112, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 12, iteration_group: 2, iteration_value: 113, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 13, iteration_group: 2, iteration_value: 114, iteration_relative_time: null},
                            {iteration_run: runId, iteration_order: 14, iteration_group: 2, iteration_value: 115, iteration_relative_time: null}]);
                        var sum = 0;
                        var squareSum = 0;
                        for (var value = 101; value <= 115; ++value) {
                            sum += value;
                            squareSum += value * value;
                        }
                        assert.equal(run['run_mean_cache'], sum / iterations.length);
                        assert.equal(run['run_sum_cache'], sum);
                        assert.equal(run['run_square_sum_cache'], squareSum);
                        notifyDone();
                    });
                });
            });
        });
    });

    it("should store aggregated run values", function () {
        addBuilderAndMeanAggregators(reportWithTwoLevelsOfAggregations, function () {
            postJSON('/api/report/', reportWithTwoLevelsOfAggregations, function (response) {
                fetchTestRunIterationsForMetric('DummyPageLoading', 'Time', function (run, iterations) {
                    var runId = run['run_id'];
                    var expectedValues = [(500 + 100) / 2, (510 + 110) / 2, (520 + 120) / 2, (530 + 130) / 2];
                    assert.deepEqual(iterations, [
                        {iteration_run: runId, iteration_order: 0, iteration_group: null, iteration_value: expectedValues[0], iteration_relative_time: null},
                        {iteration_run: runId, iteration_order: 1, iteration_group: null, iteration_value: expectedValues[1], iteration_relative_time: null},
                        {iteration_run: runId, iteration_order: 2, iteration_group: null, iteration_value: expectedValues[2], iteration_relative_time: null},
                        {iteration_run: runId, iteration_order: 3, iteration_group: null, iteration_value: expectedValues[3], iteration_relative_time: null}]);
                    var sum = expectedValues.reduce(function (sum, value) { return sum + value; }, 0);
                    assert.equal(run['run_mean_cache'], sum / iterations.length);
                    assert.equal(run['run_sum_cache'], sum);
                    assert.equal(run['run_square_sum_cache'], expectedValues.reduce(function (sum, value) { return sum + value * value; }, 0));
                    notifyDone();
                });
            });
        });
    });

    it("should be able to compute the aggregation of aggregated values", function () {
        addBuilderAndMeanAggregators(reportWithTwoLevelsOfAggregations, function () {
            postJSON('/api/report/', reportWithTwoLevelsOfAggregations, function (response) {
                fetchTestRunIterationsForMetric('DummyBenchmark', 'Time', function (run, iterations) {
                    var expectedIterations = [];
                    var sum = 0;
                    var squareSum = 0;
                    for (var i = 0; i < 15; ++i) {
                        var value = i + 1;
                        var DOMMean = ((10 + value) + (30 + value)) / 2;
                        var expectedValue = (DOMMean + 100 + value) / 2;
                        sum += expectedValue;
                        squareSum += expectedValue * expectedValue;
                        expectedIterations.push({iteration_run: run['run_id'],
                            iteration_order: i,
                            iteration_group: Math.floor(i / 5),
                            iteration_value: expectedValue,
                            iteration_relative_time: null});
                    }
                    assert.deepEqual(iterations, expectedIterations);
                    assert.equal(run['run_mean_cache'], sum / iterations.length);
                    assert.equal(run['run_sum_cache'], sum);
                    assert.equal(run['run_square_sum_cache'], squareSum);
                    notifyDone();
                });
            });
        });
    });

    var reportWithSameSubtestName = [{
        "buildNumber": "123",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "Suite1": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [1, 2, 3, 4, 5] }}
                    },
                    "test2": {
                        "metrics": {"Time": { "current": [6, 7, 8, 9, 10] }}
                    }
                }
            },
            "Suite2": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [11, 12, 13, 14, 15] }}
                    },
                    "test2": {
                        "metrics": {"Time": { "current": [16, 17, 18, 19, 20] }}
                    }
                }
            }
        }}];

    it("should be able to add a report with same subtest name", function () {
        addBuilderAndMeanAggregators(reportWithSameSubtestName, function () {
            postJSON('/api/report/', reportWithSameSubtestName, function (response) {
                assert.equal(response.statusCode, 200);
                try {
                    JSON.parse(response.responseText);
                } catch (error) {
                    assert.fail(error, null, response.responseText);
                }
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                assert.equal(JSON.parse(response.responseText)['failureStored'], false);
                notifyDone();
            });
        });
    });

    it("should be able to reuse the same test rows", function () {
        addBuilderAndMeanAggregators(reportWithSameSubtestName, function () {
            postJSON('/api/report/', reportWithSameSubtestName, function (response) {
                assert.equal(response.statusCode, 200);
                queryAndFetchAll('SELECT * FROM tests', [], function (testRows) {
                   assert.equal(testRows.length, 6);
                   reportWithSameSubtestName.buildNumber = "125";
                   reportWithSameSubtestName.buildTime = "2013-02-28T12:17:24.1";

                   postJSON('/api/report/', reportWithSameSubtestName, function (response) {
                       assert.equal(response.statusCode, 200);
                       queryAndFetchAll('SELECT * FROM tests', [], function (testRows) {
                              assert.equal(testRows.length, 6);
                              notifyDone();
                          });
                      });
                });
            });
        });
    });

    var reportWithSameSingleValue = [{
        "buildNumber": "123",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
            "suite": {
                "metrics": {"Combined": ["Arithmetic"]},
                "tests": {
                    "test1": {
                        "metrics": {"Combined": { "current": 3 }}
                    },
                    "test2": {
                        "metrics": {"Combined": { "current": 7 }}
                    }
                }
            },
        }}];

    it("should be able to add a report with single value results", function () {
        addBuilderAndMeanAggregators(reportWithSameSingleValue, function () {
            postJSON('/api/report/', reportWithSameSingleValue, function (response) {
                assert.equal(response.statusCode, 200);
                try {
                    JSON.parse(response.responseText);
                } catch (error) {
                    assert.fail(error, null, response.responseText);
                }
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                assert.equal(JSON.parse(response.responseText)['failureStored'], false);
                fetchTestRunIterationsForMetric('test1', 'Combined', function (run, iterations) {
                    assert.equal(run['run_iteration_count_cache'], 1);
                    assert.equal(run['run_mean_cache'], 3);
                    assert.equal(run['run_sum_cache'], 3);
                    assert.equal(run['run_square_sum_cache'], 9);
                    fetchTestRunIterationsForMetric('suite', 'Combined', function (run, iterations) {
                        assert.equal(run['run_iteration_count_cache'], 1);
                        assert.equal(run['run_mean_cache'], 5);
                        assert.equal(run['run_sum_cache'], 5);
                        assert.equal(run['run_square_sum_cache'], 25);
                        notifyDone();
                    });
                });
            });
        });
    });

    var reportWithSameValuePairs = [{
        "buildNumber": "123",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
                "test": {
                    "metrics": {"FrameRate": { "current": [[[0, 4], [100, 5], [205, 3]]] }}
                },
            },
        }];

    it("should be able to add a report with (relative time, value) pairs", function () {
        addBuilderAndMeanAggregators(reportWithSameValuePairs, function () {
            postJSON('/api/report/', reportWithSameValuePairs, function (response) {
                assert.equal(response.statusCode, 200);
                try {
                    JSON.parse(response.responseText);
                } catch (error) {
                    assert.fail(error, null, response.responseText);
                }
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                assert.equal(JSON.parse(response.responseText)['failureStored'], false);
                fetchTestRunIterationsForMetric('test', 'FrameRate', function (run, iterations) {
                    assert.equal(run['run_iteration_count_cache'], 3);
                    assert.equal(run['run_mean_cache'], 4);
                    assert.equal(run['run_sum_cache'], 12);
                    assert.equal(run['run_square_sum_cache'], 16 + 25 + 9);
                    var runId = run['run_id'];
                    assert.deepEqual(iterations, [
                        {iteration_run: runId, iteration_order: 0, iteration_group: null, iteration_value: 4, iteration_relative_time: 0},
                        {iteration_run: runId, iteration_order: 1, iteration_group: null, iteration_value: 5, iteration_relative_time: 100},
                        {iteration_run: runId, iteration_order: 2, iteration_group: null, iteration_value: 3, iteration_relative_time: 205}]);
                    notifyDone();
                });
            });
        });
    });
});
