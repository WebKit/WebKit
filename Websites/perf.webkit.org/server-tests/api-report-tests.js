'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const MockData = require('./resources/mock-data.js');

describe("/api/report", function () {
    prepareServerTest(this);

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
                "macOS": {
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
                "macOS": {
                    "revision": "10.8.2 12C60"
                },
                "WebKit": {
                    "revision": "141977",
                    "timestamp": "2013-02-06T08:55:20.9Z"
                }
            }
        };
    }

    it("should reject error when builder name is missing", () => {
        return TestServer.remoteAPI().postJSON('/api/report/', [{"buildTime": "2013-02-28T10:12:03.388304"}]).then((response) => {
            assert.equal(response['status'], 'MissingBuilderName');
        });
    });

    it("should reject error when build time is missing", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [{"builderName": "someBuilder", "builderPassword": "somePassword"}]);
        }).then((response) => {
            assert.equal(response['status'], 'MissingBuildTime');
        });
    });

    it("should reject when there are no builders", () => {
        return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]).then((response) => {
            assert.equal(response['status'], 'BuilderNotFound');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 0);
            return TestServer.database().selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 0);
        });
    });

    it("should reject a report without a builder password", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            var report = [{
                "buildNumber": "123",
                "buildTime": "2013-02-28T10:12:03.388304",
                "builderName": "someBuilder",
                "tests": {},
                "revisions": {}}];
            return TestServer.remoteAPI().postJSON('/api/report/', report);
        }).then((response) => {
            assert.equal(response['status'], 'BuilderNotFound');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 0);
            return TestServer.database().selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 0);
        });
    });

    it("should store a report from a valid builder", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 1);
            return TestServer.database().selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 1);
            const submittedContent = emptyReport();
            const storedContent = JSON.parse(reports[0]['content']);

            delete submittedContent['builderPassword'];
            delete submittedContent['tests'];
            delete storedContent['tests'];
            assert.deepEqual(storedContent, submittedContent);
        });
    });

    it("should treat the slave password as the builder password if there is no matching slave", () => {
        let report = emptyReport();
        report['slavePassword'] = report['builderPassword'];
        delete report['builderPassword'];

        return addSlaveForReport(report).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [report]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 1);
            return TestServer.database().selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 1);
            const storedContent = JSON.parse(reports[0]['content']);

            delete report['slavePassword'];
            delete report['tests'];
            delete storedContent['tests'];
            assert.deepEqual(storedContent, report);
        });
    });

    it("should store a report from a valid slave", () => {
        return addSlaveForReport(emptySlaveReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptySlaveReport()]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            assert.equal(response['processedRuns'], 1);
            return TestServer.database().selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 1);
            const submittedContent = emptySlaveReport();
            const storedContent = JSON.parse(reports[0]['content']);

            delete submittedContent['slavePassword'];
            delete submittedContent['tests'];
            delete storedContent['tests'];
            assert.deepEqual(storedContent, submittedContent);
        });
    });

    it("should store the builder name but not the builder password", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then((response) => {
            return TestServer.database().selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 1);
            const storedContent = JSON.parse(reports[0]['content']);
            assert.equal(storedContent['builderName'], emptyReport()['builderName']);
            assert(!('builderPassword' in storedContent));
        });
    });

    it("should add a slave if there isn't one and the report was authenticated by a builder", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then((response) => {
            return TestServer.database().selectAll('build_slaves');
        }).then((slaves) => {
            assert.equal(slaves.length, 1);
            assert.equal(slaves[0]['name'], emptyReport()['slaveName']);
        });
    });

    it("should add a builder if there isn't one and the report was authenticated by a slave", () => {
        return addSlaveForReport(emptySlaveReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptySlaveReport()]);
        }).then((response) => {
            return TestServer.database().selectAll('builders');
        }).then((builders) => {
            assert.equal(builders.length, 1);
            assert.equal(builders[0]['name'], emptyReport()['builderName']);
        });
    });

    it("should add a build", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(() => {
            return TestServer.database().selectAll('builds');
        }).then((builds) => {
            assert.strictEqual(builds[0]['number'], 123);
        });
    });

    it("should add the platform", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then(() => {
            return TestServer.database().selectAll('platforms');
        }).then((platforms) => {
            assert.equal(platforms.length, 1);
            assert.equal(platforms[0]['name'], 'Mountain Lion');
        });
    });

    it("should add repositories and build revisions", () => {
        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [emptyReport()]);
        }).then((response) => {
            const db = TestServer.database();
            return Promise.all([
                db.selectAll('repositories'),
                db.selectAll('commits'),
                db.selectAll('build_commits', 'build_commit'),
            ]);
        }).then((result) => {
            const repositories = result[0];
            const commits = result[1];
            const buildCommitsRelations = result[2];
            assert.equal(repositories.length, 2);
            assert.deepEqual(repositories.map((row) => row['name']).sort(), ['WebKit', 'macOS']);

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

            assert.equal(repositoryNameToRevisionRow['macOS']['revision'], '10.8.2 12C60');
            assert.equal(repositoryNameToRevisionRow['WebKit']['revision'], '141977');
            assert.equal(repositoryNameToRevisionRow['WebKit']['time'].toString(),
                new Date('2013-02-06 08:55:20.9').toString());
        });
    });

    it("should not create a duplicate build for the same build number if build times are close", () => {
        const firstReport = emptyReport();
        firstReport['buildTime'] = '2013-02-28T10:12:04';
        const secondReport = emptyReport();
        secondReport['buildTime'] = '2013-02-28T10:22:03';

        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [firstReport]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [secondReport]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 1);
        });
    });

    it("should create distinct builds for the same build number if build times are far apart", () => {
        const firstReport = emptyReport();
        firstReport['buildTime'] = '2013-02-28T10:12:03';
        const secondReport = emptyReport();
        secondReport['buildTime'] = '2014-01-20T22:23:34';

        return addBuilderForReport(emptyReport()).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [firstReport]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [secondReport]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 2);
        });
    });

    it("should reject a report with mismatching revision info", () => {
        const firstReport = emptyReport();
        firstReport['revisions'] = {
            "WebKit": {
                "revision": "141977",
                "timestamp": "2013-02-06T08:55:20.96Z"
            }
        };

        const secondReport = emptyReport();
        secondReport['revisions'] = {
            "WebKit": {
                "revision": "150000",
                "timestamp": "2013-05-13T10:50:29.6Z"
            }
        };

        return addBuilderForReport(firstReport).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [firstReport]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [secondReport]);
        }).then((response) => {
            assert.equal(response['status'], 'MismatchingCommitRevision');
            assert(JSON.stringify(response).indexOf('141977') >= 0);
            assert(JSON.stringify(response).indexOf('150000') >= 0);
            assert.equal(response['failureStored'], true);
            assert.equal(response['processedRuns'], 0);
        });
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
            "macOS": {
                "revision": "10.8.2 12C60"
            },
            "WebKit": {
                "revision": "141977",
                "timestamp": "2013-02-06T08:55:20.9Z"
            }
        }};

    function reportAfterAddingBuilderAndAggregatorsWithResponse(report)
    {
        return addBuilderForReport(report).then(() => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('aggregators', {name: 'Arithmetic'}),
                db.insert('aggregators', {name: 'Geometric'}),
            ]);
        }).then(() => TestServer.remoteAPI().postJSON('/api/report/', [report]));
    }

    function reportAfterAddingBuilderAndAggregators(report)
    {
        return reportAfterAddingBuilderAndAggregatorsWithResponse(report).then((response) => {
            assert.equal(response['status'], 'OK');
            assert.equal(response['failureStored'], false);
            return response;
        });
    }

    function fetchRunForMetric(testName, metricName,callback) {
        queryAndFetchAll('SELECT * FROM test_runs WHERE run_config IN'
            + '(SELECT config_id FROM test_configurations, test_metrics, tests WHERE config_metric = metric_id AND metric_test = test_id AND'
            + 'test_name = $1 AND metric_name = $2)',
            ['Arithmetic', 'values.reduce(function (a, b) { return a + b; }) / values.length'], () => {
            queryAndFetchAll('INSERT INTO aggregators (aggregator_name, aggregator_definition) values ($1, $2)',
                ['Geometric', 'Math.pow(values.reduce(function (a, b) { return a * b; }), 1 / values.length)'], callback);
        });
    }

    it("should accept a report with aggregators", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations);
    });

    it("should add tests", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(() => {
            return TestServer.database().selectAll('tests');
        }).then((tests) => {
            assert.deepEqual(tests.map((row) => { return row['name']; }).sort(),
                ['CSS', 'DOM', 'DummyBenchmark', 'DummyPageLoading', 'ModifyNodes', 'TraverseNodes', 'apple.com', 'webkit.org']);
        });
    });

    it("should add metrics", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(() => {
            return TestServer.database().query('SELECT * FROM tests, test_metrics LEFT JOIN aggregators ON metric_aggregator = aggregator_id WHERE metric_test = test_id');
        }).then((result) => {
            const testNameToMetrics = {};
            result.rows.forEach((row) => {
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
        });
    });

    function fetchTestConfig(testName, metricName)
    {
        return TestServer.database().query(`SELECT * FROM tests, test_metrics, test_configurations
            WHERE test_id = metric_test AND metric_id = config_metric
            AND test_name = $1 AND metric_name = $2`, [testName, metricName]).then((result) => {
                assert.equal(result.rows.length, 1);
                return result.rows[0];
            });
    }

    function fetchTestRunIterationsForMetric(testName, metricName)
    {
        const db = TestServer.database();
        return fetchTestConfig(testName, metricName).then((config) => {
            return db.selectFirstRow('test_runs', {config: config['config_id']});
        }).then((run) => {
            return db.selectRows('run_iterations', {run: run['id']}, {sortBy: 'order'}).then((iterations) => {
                return {run: run, iterations: iterations};
            });
        });
    }

    it("should store run values", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(() => {
            return fetchTestRunIterationsForMetric('apple.com', 'Time');
        }).then((result) => {
            const run = result.run;
            const runId = run['id'];
            assert.deepEqual(result.iterations, [
                {run: runId, order: 0, group: null, value: 500, relative_time: null},
                {run: runId, order: 1, group: null, value: 510, relative_time: null},
                {run: runId, order: 2, group: null, value: 520, relative_time: null},
                {run: runId, order: 3, group: null, value: 530, relative_time: null}]);
            const sum = 500 + 510 + 520 + 530;
            assert.equal(run['mean_cache'], sum / result.iterations.length);
            assert.equal(run['sum_cache'], sum);
            assert.equal(run['square_sum_cache'], 500 * 500 + 510 * 510 + 520 * 520 + 530 * 530);
            return fetchTestRunIterationsForMetric('CSS', 'Time');
        }).then((result) => {
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
        });
    });

    it("should store aggregated run values", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(() => {
            return fetchTestRunIterationsForMetric('DummyPageLoading', 'Time');
        }).then((result) => {
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
        });
    });

    it("should be able to compute the aggregation of aggregated values", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithTwoLevelsOfAggregations).then(() => {
            return fetchTestRunIterationsForMetric('DummyBenchmark', 'Time');
        }).then((result) => {
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
        });
    });

    function makeReport(tests)
    {
        return {
            "buildNumber": "123",
            "buildTime": "2013-02-28T10:12:03.388304",
            "builderName": "someBuilder",
            "builderPassword": "somePassword",
            "platform": "Mountain Lion",
            tests,
            "revisions": {
                "macOS": {
                    "revision": "10.8.2 12C60"
                },
                "WebKit": {
                    "revision": "141977",
                    "timestamp": "2013-02-06T08:55:20.9Z"
                }
            }
        };
    }

    it("should be able to compute the aggregation of differently aggregated values", async () => {
        const reportWithDifferentAggregators = makeReport({
            "DummyBenchmark": {
                "metrics": {"Time": ["Arithmetic"]},
                "tests": {
                    "DOM": {
                        "metrics": {"Time": ["Total"]},
                        "tests": {
                            "ModifyNodes": {"metrics": {"Time": { "current": [[1, 2], [3, 4]] }}},
                            "TraverseNodes": {"metrics": {"Time": { "current": [[11, 12], [13, 14]] }}}
                        }
                    },
                    "CSS": {"metrics": {"Time": { "current": [[21, 22], [23, 24]] }}}
                }
            }
        });

        await reportAfterAddingBuilderAndAggregators(reportWithDifferentAggregators);
        const result = await fetchTestRunIterationsForMetric('DummyBenchmark', 'Time');

        const run = result.run;
        const runId = run['id'];
        const expectedIterations = [];
        let sum = 0;
        let squareSum = 0;
        for (let i = 0; i < 4; ++i) {
            const value = i + 1;
            const DOMTotal = (value + 10 + value);
            const expectedValue = (DOMTotal + (20 + value)) / 2;
            sum += expectedValue;
            squareSum += expectedValue * expectedValue;
            expectedIterations.push({run: runId, order: i, group: Math.floor(i / 2), value: expectedValue, relative_time: null});
        }
        assert.deepEqual(result.iterations, expectedIterations);
        assert.equal(run['mean_cache'], sum / result.iterations.length);
        assert.equal(run['sum_cache'], sum);
        assert.equal(run['square_sum_cache'], squareSum);
    });

    it("should skip subtests without any metric during aggregation", async () => {
        const reportWithSubtestMissingMatchingMetric = makeReport({
            "DummyBenchmark": {
                "metrics": {"Time": ["Arithmetic"]},
                "tests": {
                    "DOM": {
                        "metrics": {"Time": ["Total"]},
                        "tests": {
                            "ModifyNodes": {"metrics": {"Time": { "current": [[1, 2], [3, 4]] }}},
                            "TraverseNodes": {"metrics": {"Time": { "current": [[11, 12], [13, 14]] }}}
                        }
                    },
                    "CSS": {"metrics": {"Time": { "current": [[21, 22], [23, 24]] }}},
                    "AuxiliaryResult": {
                        "tests": {
                            "ASubTest": {
                                "metrics": {"Time": {"current": [31, 32]}}
                            }
                        }
                    }
                }
            }
        });

        await reportAfterAddingBuilderAndAggregators(reportWithSubtestMissingMatchingMetric);
        const benchmarkResult = await fetchTestRunIterationsForMetric('DummyBenchmark', 'Time');

        let run = benchmarkResult.run.id;
        assert.equal(benchmarkResult.iterations.length, 4);
        assert.deepEqual(benchmarkResult.iterations[0], {run, order: 0, group: 0, value: ((1 + 11) + 21) / 2, relative_time: null});
        assert.deepEqual(benchmarkResult.iterations[1], {run, order: 1, group: 0, value: ((2 + 12) + 22) / 2, relative_time: null});
        assert.deepEqual(benchmarkResult.iterations[2], {run, order: 2, group: 1, value: ((3 + 13) + 23) / 2, relative_time: null});
        assert.deepEqual(benchmarkResult.iterations[3], {run, order: 3, group: 1, value: ((4 + 14) + 24) / 2, relative_time: null});

        const sum = benchmarkResult.iterations.reduce((total, row) => total + row.value, 0);
        const squareSum = benchmarkResult.iterations.reduce((total, row) => total + row.value * row.value, 0);
        assert.equal(benchmarkResult.run['mean_cache'], sum / 4);
        assert.equal(benchmarkResult.run['sum_cache'], sum);
        assert.equal(benchmarkResult.run['square_sum_cache'], squareSum);

        const someTestResult = await fetchTestRunIterationsForMetric('ASubTest', 'Time');
        run = someTestResult.run.id;
        assert.deepEqual(someTestResult.iterations, [
            {run, order: 0, group: null, value: 31, relative_time: null},
            {run, order: 1, group: null, value: 32, relative_time: null},
        ]);
        assert.equal(someTestResult.run['mean_cache'], 31.5);
        assert.equal(someTestResult.run['sum_cache'], 63);
        assert.equal(someTestResult.run['square_sum_cache'], 31 * 31 + 32 * 32);
    });

    it("should skip subtests missing the matching metric during aggregation", async () => {
        const reportWithSubtestMissingMatchingMetric = makeReport({
            "DummyBenchmark": {
                "metrics": {"Time": ["Arithmetic"]},
                "tests": {
                    "DOM": {
                        "metrics": {"Time": ["Total"]},
                        "tests": {
                            "ModifyNodes": {"metrics": {"Time": { "current": [[1, 2], [3, 4]] }}},
                            "TraverseNodes": {"metrics": {"Time": { "current": [[11, 12], [13, 14]] }}}
                        }
                    },
                    "CSS": {"metrics": {"Time": { "current": [[21, 22], [23, 24]] }}},
                    "AuxiliaryResult": {
                        "metrics": {"Allocations": ["Total"]},
                        "tests": {
                            "SomeSubTest": {
                                "metrics": {"Allocations": {"current": [31, 32]}}
                            },
                            "OtherSubTest": {
                                "metrics": {"Allocations": {"current": [41, 42]}}
                            }
                        }
                    }
                }
            }
        });

        await reportAfterAddingBuilderAndAggregators(reportWithSubtestMissingMatchingMetric);
        const benchmarkResult = await fetchTestRunIterationsForMetric('DummyBenchmark', 'Time');

        let run = benchmarkResult.run.id;
        assert.equal(benchmarkResult.iterations.length, 4);
        assert.deepEqual(benchmarkResult.iterations[0], {run, order: 0, group: 0, value: ((1 + 11) + 21) / 2, relative_time: null});
        assert.deepEqual(benchmarkResult.iterations[1], {run, order: 1, group: 0, value: ((2 + 12) + 22) / 2, relative_time: null});
        assert.deepEqual(benchmarkResult.iterations[2], {run, order: 2, group: 1, value: ((3 + 13) + 23) / 2, relative_time: null});
        assert.deepEqual(benchmarkResult.iterations[3], {run, order: 3, group: 1, value: ((4 + 14) + 24) / 2, relative_time: null});

        const sum = benchmarkResult.iterations.reduce((total, row) => total + row.value, 0);
        const squareSum = benchmarkResult.iterations.reduce((total, row) => total + row.value * row.value, 0);
        assert.equal(benchmarkResult.run['mean_cache'], sum / 4);
        assert.equal(benchmarkResult.run['sum_cache'], sum);
        assert.equal(benchmarkResult.run['square_sum_cache'], squareSum);

        const someTestResult = await fetchTestRunIterationsForMetric('AuxiliaryResult', 'Allocations');
        run = someTestResult.run.id;
        assert.deepEqual(someTestResult.iterations, [
            {run, order: 0, group: null, value: 31 + 41, relative_time: null},
            {run, order: 1, group: null, value: 32 + 42, relative_time: null},
        ]);
        assert.equal(someTestResult.run['mean_cache'], (31 + 41 + 32 + 42) / 2);
        assert.equal(someTestResult.run['sum_cache'], 31 + 41 + 32 + 42);
        assert.equal(someTestResult.run['square_sum_cache'], Math.pow(31 + 41, 2) + Math.pow(32 + 42, 2));
    });

    it("should reject a report when there are more than one non-matching aggregators in a subtest", async () => {
        const reportWithAmbigiousAggregators = {
            "buildNumber": "123",
            "buildTime": "2013-02-28T10:12:03.388304",
            "builderName": "someBuilder",
            "builderPassword": "somePassword",
            "platform": "Mountain Lion",
            "tests": {
                "DummyBenchmark": {
                    "metrics": {"Time": ["Arithmetic"]},
                    "tests": {
                        "DOM": {
                            "metrics": {"Time": ["Total", "Geometric"]},
                            "tests": {
                                "ModifyNodes": {"metrics": {"Time": { "current": [[1, 2], [3, 4]] }}},
                                "TraverseNodes": {"metrics": {"Time": { "current": [[11, 12], [13, 14]] }}}
                            }
                        },
                        "CSS": {"metrics": {"Time": { "current": [[21, 22], [23, 24]] }}}
                    }
                }
            },
            "revisions": {
                "macOS": {
                    "revision": "10.8.2 12C60"
                },
                "WebKit": {
                    "revision": "141977",
                    "timestamp": "2013-02-06T08:55:20.9Z"
                }
            }};

        const response = await reportAfterAddingBuilderAndAggregatorsWithResponse(reportWithAmbigiousAggregators);
        assert.equal(response['status'], 'NoMatchingAggregatedValueInSubtest');
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

    it("should be able to add a report with same subtest name", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithSameSubtestName());
    });

    it("should be able to reuse the same test rows", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithSameSubtestName()).then(() => {
            return TestServer.database().selectAll('tests');
        }).then((tests) => {
            assert.equal(tests.length, 6);
            let newReport = reportWithSameSubtestName();
            newReport.buildNumber = "125";
            newReport.buildTime = "2013-02-28T12:17:24.1";
            return TestServer.remoteAPI().postJSON('/api/report/', [newReport]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('tests');
        }).then((tests) => {
            assert.equal(tests.length, 6);
        });
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

    it("should be able to add a report with single value results", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithSameSingleValue).then(() => {
            return fetchTestRunIterationsForMetric('test1', 'Combined');
        }).then((result) => {
            const run = result.run;
            assert.equal(run['iteration_count_cache'], 1);
            assert.equal(run['mean_cache'], 3);
            assert.equal(run['sum_cache'], 3);
            assert.equal(run['square_sum_cache'], 9);
            return fetchTestRunIterationsForMetric('suite', 'Combined');
        }).then((result) => {
            const run = result.run;
            assert.equal(run['iteration_count_cache'], 1);
            assert.equal(run['mean_cache'], 5);
            assert.equal(run['sum_cache'], 5);
            assert.equal(run['square_sum_cache'], 25);
        });
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

    it("should be able to add a report with (relative time, value) pairs", () => {
        return reportAfterAddingBuilderAndAggregators(reportWithSameValuePairs).then(() => {
            return fetchTestRunIterationsForMetric('test', 'FrameRate');
        }).then((result) => {
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
        });
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

    it("should update the last modified date of test configurations with new runs", () => {
        return addBuilderForReport(reportsUpdatingDifferentTests[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[0]]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return fetchTestConfig('test1', 'Time');
        }).then((originalConfig) => {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[2]]).then(() => {
                return fetchTestConfig('test1', 'Time');
            }).then((config) => {
                assert(originalConfig['config_runs_last_modified'] instanceof Date);
                assert(config['config_runs_last_modified'] instanceof Date);
                assert(+originalConfig['config_runs_last_modified'] < +config['config_runs_last_modified']);
            });
        });
    });

    it("should not update the last modified date of unrelated test configurations", () => {
        return addBuilderForReport(reportsUpdatingDifferentTests[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[0]]);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return fetchTestConfig('test1', 'Time');
        }).then((originalConfig) => {
            return TestServer.remoteAPI().postJSON('/api/report/', [reportsUpdatingDifferentTests[1]]).then((response) => {
                assert.equal(response['status'], 'OK');
                return fetchTestConfig('test1', 'Time');
            }).then((config) => {
                assert(originalConfig['config_runs_last_modified'] instanceof Date);
                assert(config['config_runs_last_modified'] instanceof Date);
                assert.equal(+originalConfig['config_runs_last_modified'], +config['config_runs_last_modified']);
            });
        });
    });

    const reportWithBuildRequest = {
        "buildNumber": "123",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "buildRequest": "700",
        "tests": {
            "test": {
                "metrics": {"FrameRate": { "current": [[[0, 4], [100, 5], [205, 3]]] }}
            },
        },
    };

    const anotherReportWithSameBuildRequest = {
        "buildNumber": "124",
        "buildTime": "2013-02-28T10:12:03.388304",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Lion",
        "buildRequest": "700",
        "tests": {
            "test": {
                "metrics": {"FrameRate": { "current": [[[0, 4], [100, 5], [205, 3]]] }}
            },
        },
    };

    it("should allow to report a build request", () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return reportAfterAddingBuilderAndAggregatorsWithResponse(reportWithBuildRequest);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
        });
    });

    it("should reject the report if the build request in the report has an existing associated build", () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return reportAfterAddingBuilderAndAggregatorsWithResponse(reportWithBuildRequest);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectRows('builds', {number: '123'});
        }).then((results) => {
            assert.equal(results.length, 1);
            return TestServer.database().selectRows('platforms', {name: 'Mountain Lion'});
        }).then((results) => {
            assert.equal(results.length, 1);
            return TestServer.remoteAPI().postJSON('/api/report/', [anotherReportWithSameBuildRequest]);
        }).then((response) => {
            assert.equal(response['status'], 'FailedToUpdateBuildRequest');
            return TestServer.database().selectRows('builds', {number: '124'});
        }).then((results) => {
            assert.equal(results.length, 0);
            return TestServer.database().selectRows('platforms', {name: 'Lion'});
        }).then((results) => {
            assert.equal(results.length, 0);
        });
    });

});
