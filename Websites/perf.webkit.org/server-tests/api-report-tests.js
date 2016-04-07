'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const connectToDatabaseInEveryTest = require('./resources/common-operations.js').connectToDatabaseInEveryTest;

describe("/api/report", function () {
    this.timeout(1000);
    TestServer.inject();
    connectToDatabaseInEveryTest();

    function emptyReport()
    {
        return {
            "buildNumber": "123",
            "buildTime": "2013-02-28T10:12:03.388304",
            "builderName": "someBuilder",
            "slaveName": "someSlave",
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
            }
        };
    }

    function emptySlaveReport()
    {
        return {
            "buildNumber": "123",
            "buildTime": "2013-02-28T10:12:03.388304",
            "builderName": "someBuilder",
            "slaveName": "someSlave",
            "slavePassword": "otherPassword",
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
            }
        };
    }

    it("should reject error when builder name is missing", function (done) {
        TestServer.remoteAPI().postJSON('/api/report/', [{"buildTime": "2013-02-28T10:12:03.388304"}]).then(function (response) {
            assert.equal(response['status'], 'MissingBuilderName');
            done();
        }).catch(done);
    });

    it("should reject error when build time is missing", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [{"builderName": "someBuilder", "builderPassword": "somePassword"}]);
        }).then(function (response) {
            assert.equal(response['status'], 'MissingBuildTime');
            done();
        });
    });

    it("should reject when there are no builders", function (done) {
        TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]).then(function (response) {
            assert.equal(response['status'], 'BuilderNotFound');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 0);
            return TestServer.database().selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 0);
            done();
        }).catch(done);
    });

    it("should reject a report without a builder password", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            var report = [{
                "buildNumber": "123",
                "buildTime": "2013-02-28T10:12:03.388304",
                "builderName": "someBuilder",
                "tests": {},
                "revisions": {}}];
            return TestServer.remoteAPI().postJSON('/api/report/', report);
        }).then(function (response) {
            assert.equal(response['status'], 'BuilderNotFound');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 0);
            return TestServer.database().selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 0);
            done();
        }).catch(done);
    });

    it("should store a report from a valid builder", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 1);
            return TestServer.database().selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 1);
            const submittedContent = emptyReport();
            const storedContent = JSON.parse(reports[0]['content']);

            delete submittedContent['builderPassword'];
            delete submittedContent['tests'];
            delete storedContent['tests'];
            assert.deepEqual(storedContent, submittedContent);

            done();
        }).catch(done);
    });

    it("should treat the slave password as the builder password if there is no matching slave", function (done) {
        let report = emptyReport();
        report['slavePassword'] = report['builderPassword'];
        delete report['builderPassword'];

        addSlaveForReport(report).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [report]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 1);
            return TestServer.database().selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 1);
            const storedContent = JSON.parse(reports[0]['content']);

            delete report['slavePassword'];
            delete report['tests'];
            delete storedContent['tests'];
            assert.deepEqual(storedContent, report);

            done();
        }).catch(done);
    });

    it("should store a report from a valid slave", function (done) {
        addSlaveForReport(emptySlaveReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptySlaveReport()]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 1);
            return TestServer.database().selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 1);
            const submittedContent = emptySlaveReport();
            const storedContent = JSON.parse(reports[0]['content']);

            delete submittedContent['slavePassword'];
            delete submittedContent['tests'];
            delete storedContent['tests'];
            assert.deepEqual(storedContent, submittedContent);

            done();
        }).catch(done);
    });

    it("should store the builder name but not the builder password", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(function (response) {
            return TestServer.database().selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 1);
            const storedContent = JSON.parse(reports[0]['content']);
            assert.equal(storedContent['builderName'], emptyReport()['builderName']);
            assert(!('builderPassword' in storedContent));
            done();
        }).catch(done);
    });

    it("should add a slave if there isn't one and the report was authenticated by a builder", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(function (response) {
            return TestServer.database().selectAll('build_slaves');
        }).then(function (slaves) {
            assert.equal(slaves.length, 1);
            assert.equal(slaves[0]['name'], emptyReport()['slaveName']);
            done();
        }).catch(done);
    });

    it("should add a builder if there isn't one and the report was authenticated by a slave", function (done) {
        addSlaveForReport(emptySlaveReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptySlaveReport()]);
        }).then(function (response) {
            return TestServer.database().selectAll('builders');
        }).then(function (builders) {
            assert.equal(builders.length, 1);
            assert.equal(builders[0]['name'], emptyReport()['builderName']);
            done();
        }).catch(done);
    });

    it("should add a build", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(function () {
            return TestServer.database().selectAll('builds');
        }).then(function (builds) {
            assert.strictEqual(builds[0]['number'], 123);
            done();
        }).catch(done);
    });

    it("should add the platform", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(function () {
            return TestServer.database().selectAll('platforms');
        }).then(function (platforms) {
            assert.equal(platforms.length, 1);
            assert.equal(platforms[0]['name'], 'Mountain Lion');
            done();
        }).catch(done);
    });

    it("should add repositories and build revisions", function (done) {
        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(function (response) {
            const db = TestServer.database();
            return Promise.all([
                db.selectAll('repositories'),
                db.selectAll('commits'),
                db.selectAll('build_commits', 'build_commit'),
            ]);
        }).then(function (result) {
            const repositories = result[0];
            const commits = result[1];
            const buildCommitsRelations = result[2];
            assert.equal(repositories.length, 2);
            assert.deepEqual(repositories.map(function (row) { return row['name']; }).sort(), ['OS X', 'WebKit']);

            assert.equal(commits.length, 2);
            assert.equal(buildCommitsRelations.length, 2);
            assert.equal(buildCommitsRelations[0]['build_commit'], commits[0]['id']);
            assert.equal(buildCommitsRelations[1]['build_commit'], commits[1]['id']);
            assert.equal(buildCommitsRelations[0]['commit_build'], buildCommitsRelations[1]['commit_build']);

            let repositoryIdToName = {};
            for (let repository of repositories)
                repositoryIdToName[repository['id']] = repository['name'];

            let repositoryNameToRevisionRow = {};
            for (let commit of commits)
                repositoryNameToRevisionRow[repositoryIdToName[commit['repository']]] = commit;

            assert.equal(repositoryNameToRevisionRow['OS X']['revision'], '10.8.2 12C60');
            assert.equal(repositoryNameToRevisionRow['WebKit']['revision'], '141977');
            assert.equal(repositoryNameToRevisionRow['WebKit']['time'].toString(),
                new Date('2013-02-06 08:55:20.9').toString());
            done();
        }).catch(done);
    });

    it("should not create a duplicate build for the same build number if build times are close", function (done) {
        let firstReport = emptyReport();
        firstReport['buildTime'] = '2013-02-28T10:12:04';
        let secondReport = emptyReport();
        secondReport['buildTime'] = '2013-02-28T10:22:03';

        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [firstReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [secondReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 1);
            done();
        }).catch(done);
    });

    it("should create distinct builds for the same build number if build times are far apart", function (done) {
        let firstReport = emptyReport();
        firstReport['buildTime'] = '2013-02-28T10:12:03';
        let secondReport = emptyReport();
        secondReport['buildTime'] = '2014-01-20T22:23:34';

        addBuilderForReport(emptyReport()).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [firstReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [secondReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 2);
            done();
        }).catch(done);
    });

    it("should reject a report with mismatching revision info", function (done) {
        let firstReport = emptyReport();
        firstReport['revisions'] = {
            "WebKit": {
                "revision": "141977",
                "timestamp": "2013-02-06T08:55:20.96Z"
            }
        };

        let secondReport = emptyReport();
        secondReport['revisions'] = {
            "WebKit": {
                "revision": "150000",
                "timestamp": "2013-05-13T10:50:29.6Z"
            }
        };

        addBuilderForReport(firstReport).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [firstReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [secondReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'MismatchingCommitRevision');
            assert(JSON.stringify(response).indexOf('141977') >= 0);
            assert(JSON.stringify(response).indexOf('150000') >= 0);
            assert.equal(response['failureStored'], true);
            assert.equal(response['processedRuns'], 0);
            done();
        }).catch(done);
    });

    const reportWithTwoLevelsOfAggregations = {
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
        }};

    function reportAfterAddingBuilderAndAggregators(report)
    {
        return addBuilderForReport(report).then(function () {
            const db = TestServer.database();
            return Promise.all([
                db.insert('aggregators', {name: 'Arithmetic'}),
                db.insert('aggregators', {name: 'Geometric'}),
            ]);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [report]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            return response;
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

    it("should accept a report with aggregators", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(function () {
            done();
        }).catch(done);
    });

    it("should add tests", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(function () {
            return TestServer.database().selectAll('tests');
        }).then(function (tests) {
            assert.deepEqual(tests.map(function (row) { return row['name']; }).sort(),
                ['CSS', 'DOM', 'DummyBenchmark', 'DummyPageLoading', 'ModifyNodes', 'TraverseNodes', 'apple.com', 'webkit.org']);
            done();
        }).catch(done);
    });

    it("should add metrics", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(function () {
            return TestServer.database().query('SELECT * FROM tests, test_metrics LEFT JOIN aggregators ON metric_aggregator = aggregator_id WHERE metric_test = test_id');
        }).then(function (result) {
            let testNameToMetrics = {};
            result.rows.forEach(function (row) {
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
            done();
        }).catch(done);
    });

    function fetchTestConfig(testName, metricName)
    {
        return TestServer.database().query(`SELECT * FROM tests, test_metrics, test_configurations
            WHERE test_id = metric_test AND metric_id = config_metric
            AND test_name = $1 AND metric_name = $2`, [testName, metricName]).then(function (result) {
                assert.equal(result.rows.length, 1);
                return result.rows[0];
            });
    }

    function fetchTestRunIterationsForMetric(testName, metricName)
    {
        const db = TestServer.database();
        return fetchTestConfig(testName, metricName).then(function (config) {
            return db.selectFirstRow('test_runs', {config: config['config_id']});
        }).then(function (run) {
            return db.selectRows('run_iterations', {run: run['id']}, {sortBy: 'order'}).then(function (iterations) {
                return {run: run, iterations: iterations};
            });
        });
    }

    it("should store run values", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(function () {
            return fetchTestRunIterationsForMetric('apple.com', 'Time');
        }).then(function (result) {
            const run = result.run;
            const runId = run['id'];
            assert.deepEqual(result.iterations, [
                {run: runId, order: 0, group: null, value: 500, relative_time: null},
                {run: runId, order: 1, group: null, value: 510, relative_time: null},
                {run: runId, order: 2, group: null, value: 520, relative_time: null},
                {run: runId, order: 3, group: null, value: 530, relative_time: null}]);
            var sum = 500 + 510 + 520 + 530;
            assert.equal(run['mean_cache'], sum / result.iterations.length);
            assert.equal(run['sum_cache'], sum);
            assert.equal(run['square_sum_cache'], 500 * 500 + 510 * 510 + 520 * 520 + 530 * 530);
            return fetchTestRunIterationsForMetric('CSS', 'Time');
        }).then(function (result) {
            const run = result.run;
            const runId = run['id'];
            assert.deepEqual(result.iterations, [
                {run: runId, order: 0, group: 0, value: 101, relative_time: null},
                {run: runId, order: 1, group: 0, value: 102, relative_time: null},
                {run: runId, order: 2, group: 0, value: 103, relative_time: null},
                {run: runId, order: 3, group: 0, value: 104, relative_time: null},
                {run: runId, order: 4, group: 0, value: 105, relative_time: null},
                {run: runId, order: 5, group: 1, value: 106, relative_time: null},
                {run: runId, order: 6, group: 1, value: 107, relative_time: null},
                {run: runId, order: 7, group: 1, value: 108, relative_time: null},
                {run: runId, order: 8, group: 1, value: 109, relative_time: null},
                {run: runId, order: 9, group: 1, value: 110, relative_time: null},
                {run: runId, order: 10, group: 2, value: 111, relative_time: null},
                {run: runId, order: 11, group: 2, value: 112, relative_time: null},
                {run: runId, order: 12, group: 2, value: 113, relative_time: null},
                {run: runId, order: 13, group: 2, value: 114, relative_time: null},
                {run: runId, order: 14, group: 2, value: 115, relative_time: null}]);
            let sum = 0;
            let squareSum = 0;
            for (let value = 101; value <= 115; ++value) {
                sum += value;
                squareSum += value * value;
            }
            assert.equal(run['mean_cache'], sum / result.iterations.length);
            assert.equal(run['sum_cache'], sum);
            assert.equal(run['square_sum_cache'], squareSum);
            done();
        }).catch(done);
    });

    it("should store aggregated run values", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(function () {
            return fetchTestRunIterationsForMetric('DummyPageLoading', 'Time');
        }).then(function (result) {
            const run = result.run;
            const runId = result.run['id'];
            const expectedValues = [(500 + 100) / 2, (510 + 110) / 2, (520 + 120) / 2, (530 + 130) / 2];
            assert.deepEqual(result.iterations, [
                {run: runId, order: 0, group: null, value: expectedValues[0], relative_time: null},
                {run: runId, order: 1, group: null, value: expectedValues[1], relative_time: null},
                {run: runId, order: 2, group: null, value: expectedValues[2], relative_time: null},
                {run: runId, order: 3, group: null, value: expectedValues[3], relative_time: null}]);
            const sum = expectedValues.reduce(function (sum, value) { return sum + value; }, 0);
            assert.equal(run['mean_cache'], sum / result.iterations.length);
            assert.equal(run['sum_cache'], sum);
            assert.equal(run['square_sum_cache'], expectedValues.reduce(function (sum, value) { return sum + value * value; }, 0));
            done();
        }).catch(done);
    });

    it("should be able to compute the aggregation of aggregated values", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(function () {
            return fetchTestRunIterationsForMetric('DummyBenchmark', 'Time');
        }).then(function (result) {
            const run = result.run;
            const runId = run['id'];
            const expectedIterations = [];
            let sum = 0;
            let squareSum = 0;
            for (let i = 0; i < 15; ++i) {
                const value = i + 1;
                const DOMMean = ((10 + value) + (30 + value)) / 2;
                const expectedValue = (DOMMean + 100 + value) / 2;
                sum += expectedValue;
                squareSum += expectedValue * expectedValue;
                expectedIterations.push({run: runId, order: i, group: Math.floor(i / 5), value: expectedValue, relative_time: null});
            }
            assert.deepEqual(result.iterations, expectedIterations);
            assert.equal(run['mean_cache'], sum / result.iterations.length);
            assert.equal(run['sum_cache'], sum);
            assert.equal(run['square_sum_cache'], squareSum);
            done();
        }).catch(done);
    });

    function reportWithSameSubtestName()
    {
        return {
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
            }
        };
    }

    it("should be able to add a report with same subtest name", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithSameSubtestName()).then(function () {
            done();
        }).catch(done);
    });

    it("should be able to reuse the same test rows", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithSameSubtestName()).then(function () {
            return TestServer.database().selectAll('tests');
        }).then(function (tests) {
            assert.equal(tests.length, 6);
            let newReport = reportWithSameSubtestName();
            newReport.buildNumber = "125";
            newReport.buildTime = "2013-02-28T12:17:24.1";
            return TestServer.remoteAPI().postJSON('/api/report/', [newReport]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('tests');
        }).then(function (tests) {
            assert.equal(tests.length, 6);
            done();
        }).catch(done);
    });

    const reportWithSameSingleValue = {
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
        }};

    it("should be able to add a report with single value results", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithSameSingleValue).then(function () {
            return fetchTestRunIterationsForMetric('test1', 'Combined');
        }).then(function (result) {
            const run = result.run;
            assert.equal(run['iteration_count_cache'], 1);
            assert.equal(run['mean_cache'], 3);
            assert.equal(run['sum_cache'], 3);
            assert.equal(run['square_sum_cache'], 9);
            return fetchTestRunIterationsForMetric('suite', 'Combined');
        }).then(function (result) {
            const run = result.run;
            assert.equal(run['iteration_count_cache'], 1);
            assert.equal(run['mean_cache'], 5);
            assert.equal(run['sum_cache'], 5);
            assert.equal(run['square_sum_cache'], 25);
            done();
        }).catch(done);
    });

    const reportWithSameValuePairs = {
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
        };

    it("should be able to add a report with (relative time, value) pairs", function (done) {
        reportAfterAddingBuilderAndAggregators(reportWithSameValuePairs).then(function () {
            return fetchTestRunIterationsForMetric('test', 'FrameRate');
        }).then(function (result) {
            const run = result.run;
            assert.equal(run['iteration_count_cache'], 3);
            assert.equal(run['mean_cache'], 4);
            assert.equal(run['sum_cache'], 12);
            assert.equal(run['square_sum_cache'], 16 + 25 + 9);

            const runId = run['id'];
            assert.deepEqual(result.iterations, [
                {run: runId, order: 0, group: null, value: 4, relative_time: 0},
                {run: runId, order: 1, group: null, value: 5, relative_time: 100},
                {run: runId, order: 2, group: null, value: 3, relative_time: 205}]);
            done();
        }).catch(done);
    });

    const reportsUpdatingDifferentTests = [
        {
            "buildNumber": "123",
            "buildTime": "2013-02-28T10:12:03",
            "builderName": "someBuilder",
            "builderPassword": "somePassword",
            "platform": "Mountain Lion",
            "tests": {"test1": {"metrics": {"Time": {"current": 3}}}}
        },
        {
            "buildNumber": "124",
            "buildTime": "2013-02-28T11:31:21",
            "builderName": "someBuilder",
            "builderPassword": "somePassword",
            "platform": "Mountain Lion",
            "tests": {"test2": {"metrics": {"Time": {"current": 3}}}}
        },
        {
            "buildNumber": "125",
            "buildTime": "2013-02-28T12:45:34",
            "builderName": "someBuilder",
            "builderPassword": "somePassword",
            "platform": "Mountain Lion",
            "tests": {"test1": {"metrics": {"Time": {"current": 3}}}}
        },
    ];

    it("should update the last modified date of test configurations with new runs", function (done) {
        addBuilderForReport(reportsUpdatingDifferentTests[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[0]]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return fetchTestConfig('test1', 'Time');
        }).then(function (originalConfig) {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[2]]).then(function () {
                return fetchTestConfig('test1', 'Time');
            }).then(function (config) {
                assert(originalConfig['config_runs_last_modified'] instanceof Date);
                assert(config['config_runs_last_modified'] instanceof Date);
                assert(+originalConfig['config_runs_last_modified'] < +config['config_runs_last_modified']);
                done();
            });
        }).catch(done);
    });

    it("should not update the last modified date of unrelated test configurations", function (done) {
        addBuilderForReport(reportsUpdatingDifferentTests[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[0]]);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return fetchTestConfig('test1', 'Time');
        }).then(function (originalConfig) {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[1]]).then(function (response) {
                assert.equal(response['status'], 'OK');
                return fetchTestConfig('test1', 'Time');
            }).then(function (config) {
                assert(originalConfig['config_runs_last_modified'] instanceof Date);
                assert(config['config_runs_last_modified'] instanceof Date);
                assert.equal(+originalConfig['config_runs_last_modified'], +config['config_runs_last_modified']);
                done();
            });
        }).catch(done);
    });
});
