'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe("/api/measurement-set", function () {
    prepareServerTest(this);

    function queryPlatformAndMetric(platformName, metricName)
    {
        const db = TestServer.database();
        return Promise.all([
            db.selectFirstRow('platforms', {name: platformName}),
            db.selectFirstRow('test_metrics', {name: metricName}),
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
        "buildTime": "2013-02-28T15:34:51Z",
        "revisions": {
            "WebKit": {
                "revision": "144000",
                "timestamp": clusterTime(10.35645364537).toISOString(),
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
        "buildTime": "2013-02-28T21:45:17Z",
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
        "buildTime": "2013-02-28T23:07:25Z",
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

    it("should reject when platform ID is missing", () => {
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?metric=${result.metricId}`);
        }).then((response) => {
            assert.equal(response['status'], 'AmbiguousRequest');
        });
    });

    it("should reject when metric ID is missing", () => {
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?platform=${result.platformId}`);
        }).then((response) => {
            assert.equal(response['status'], 'AmbiguousRequest');
        });
    });

    it("should reject an invalid platform name", () => {
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?platform=${result.platformId}a&metric=${result.metricId}`);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidPlatform');
        });
    });

    it("should reject an invalid metric name", () => {
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return TestServer.remoteAPI().getJSON(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}b`);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidMetric');
        });
    });

    it("should return 404 when the report is empty", () => {
        const db = TestServer.database();
        return Promise.all([
            db.insert('tests', {id: 1, name: 'SomeTest'}),
            db.insert('tests', {id: 2, name: 'SomeOtherTest'}),
            db.insert('tests', {id: 3, name: 'ChildTest', parent: 1}),
            db.insert('tests', {id: 4, name: 'GrandChild', parent: 3}),
            db.insert('aggregators', {id: 200, name: 'Total'}),
            db.insert('test_metrics', {id: 5, test: 1, name: 'Time'}),
            db.insert('test_metrics', {id: 6, test: 2, name: 'Time', aggregator: 200}),
            db.insert('test_metrics', {id: 7, test: 2, name: 'Malloc', aggregator: 200}),
            db.insert('test_metrics', {id: 8, test: 3, name: 'Time'}),
            db.insert('test_metrics', {id: 9, test: 4, name: 'Time'}),
            db.insert('platforms', {id: 23, name: 'iOS 9 iPhone 5s'}),
            db.insert('platforms', {id: 46, name: 'Trunk Mavericks'}),
            db.insert('test_configurations', {id: 101, metric: 5, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 102, metric: 6, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 103, metric: 7, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 104, metric: 8, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 105, metric: 9, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 106, metric: 5, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 107, metric: 5, platform: 23, type: 'baseline'}),
        ]).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus(`/api/measurement-set/?platform=46&metric=5`).then((response) => {
                assert(false);
            }, (error) => {
                assert.equal(error, 404);
            });
        });
    });

    it("should be able to retrieve a reported value", () => {
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return TestServer.remoteAPI().getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((response) => {
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

            const currentRows = response['configurations']['current'];
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
        });
    });

    it("should return return the right IDs for measurement, build, and builder", () => {
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithBuildTime);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            const db = TestServer.database();
            return Promise.all([
                db.selectAll('test_runs'),
                db.selectAll('builds'),
                db.selectAll('builders'),
                TestServer.remoteAPI().getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`),
            ]);
        }).then((result) => {
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
        });
    });

    function queryPlatformAndMetricWithRepository(platformName, metricName, repositoryName)
    {
        const db = TestServer.database();
        return Promise.all([
            db.selectFirstRow('platforms', {name: platformName}),
            db.selectFirstRow('test_metrics', {name: metricName}),
            db.selectFirstRow('repositories', {name: repositoryName}),
        ]).then((result) => ({platformId: result[0]['id'], metricId: result[1]['id'], repositoryId: result[2]['id']}));
    }

    it("should order results by commit time", () => {
        const remote = TestServer.remoteAPI();
        let repositoryId;
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return remote.postJSON('/api/report/', reportWithBuildTime);
        }).then(() => {
            return remote.postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return queryPlatformAndMetricWithRepository('Mountain Lion', 'Time', 'WebKit');
        }).then((result) => {
            repositoryId = result.repositoryId;
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((response) => {
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
               revisions: [[1, repositoryId, '144000', null, revisionTime]],
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
        });
    });

    it("should order results by build time when commit times are missing", () => {
        const remote = TestServer.remoteAPI();
        let repositoryId;
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('commits', {'id': 2, 'repository': 1, 'revision': 'macOS 16A323', 'order': 0}),
                db.insert('commits', {'id': 3, 'repository': 1, 'revision': 'macOS 16C68', 'order': 1}),
            ]);
        }).then(() => {
            return remote.postJSON('/api/report/', [{
                "buildNumber": "1001",
                "buildTime": '2017-01-19 15:28:01',
                "revisions": {
                    "macOS": {
                        "revision": "macOS 16C68",
                    },
                },
                "builderName": "someBuilder",
                "builderPassword": "somePassword",
                "platform": "Sierra",
                "tests": { "Test": {"metrics": {"Time": { "baseline": [1, 2, 3, 4, 5] } } } },
            }]);
        }).then(() => {
            return remote.postJSON('/api/report/', [{
                "buildNumber": "1002",
                "buildTime": '2017-01-19 19:46:37',
                "revisions": {
                    "macOS": {
                        "revision": "macOS 16A323",
                    },
                },
                "builderName": "someBuilder",
                "builderPassword": "somePassword",
                "platform": "Sierra",
                "tests": { "Test": {"metrics": {"Time": { "baseline": [5, 6, 7, 8, 9] } } } },
            }]);
        }).then(() => {
            return queryPlatformAndMetricWithRepository('Sierra', 'Time', 'macOS');
        }).then((result) => {
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((response) => {
            const currentRows = response['configurations']['baseline'];
            assert.equal(currentRows.length, 2);
            assert.deepEqual(format(response['formatMap'], currentRows[0]), {
               mean: 3,
               iterationCount: 5,
               sum: 15,
               squareSum: 55,
               markedOutlier: false,
               revisions: [[3, 1, 'macOS 16C68', 1, 0]],
               commitTime: +Date.UTC(2017, 0, 19, 15, 28, 1),
               buildTime: +Date.UTC(2017, 0, 19, 15, 28, 1),
               buildNumber: '1001' });
            assert.deepEqual(format(response['formatMap'], currentRows[1]), {
                mean: 7,
                iterationCount: 5,
                sum: 35,
                squareSum: 255,
                markedOutlier: false,
                revisions: [[2, 1, 'macOS 16A323', 0, 0]],
                commitTime: +Date.UTC(2017, 0, 19, 19, 46, 37),
                buildTime: +Date.UTC(2017, 0, 19, 19, 46, 37),
                buildNumber: '1002' });
        });
    });

    function buildNumbers(parsedResult, config)
    {
        return parsedResult['configurations'][config].map((row) => format(parsedResult['formatMap'], row)['buildNumber']);
    }

    it("should include one data point after the current time range", () => {
        const remote = TestServer.remoteAPI();
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return remote.postJSON('/api/report/', reportWithAncentRevision);
        }).then(() => {
            return remote.postJSON('/api/report/', reportWithNewRevision);
        }).then(() => {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            assert.equal(response['clusterCount'], 2, 'should have two clusters');
            assert.deepEqual(buildNumbers(response, 'current'),
                [reportWithAncentRevision[0]['buildNumber'], reportWithNewRevision[0]['buildNumber']]);
        });
    });

    it("should always include one old data point before the current time range", () => {
        const remote = TestServer.remoteAPI();
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return remote.postJSON('/api/report/', reportWithBuildTime);
        }).then(() => {
            return remote.postJSON('/api/report/', reportWithAncentRevision);
        }).then(() => {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((response) => {
            assert.equal(response['clusterCount'], 2, 'should have two clusters');
            let currentRows = response['configurations']['current'];
            assert.equal(currentRows.length, 2, 'should contain two data points');
            assert.deepEqual(buildNumbers(response, 'current'), [reportWithAncentRevision[0]['buildNumber'], reportWithBuildTime[0]['buildNumber']]);
        });
    });

    it("should create cached results", () => {
        const remote = TestServer.remoteAPI();
        let cachePrefix;
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return remote.postJSON('/api/report/', reportWithAncentRevision);
        }).then(() => {
            return remote.postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return remote.postJSON('/api/report/', reportWithNewRevision);
        }).then(() => {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            cachePrefix = '/data/measurement-set-' + result.platformId + '-' + result.metricId;
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((newResult) => {
            return remote.getJSONWithStatus(`${cachePrefix}.json`).then((cachedResult) => {
                assert.deepEqual(newResult, cachedResult);
                return remote.getJSONWithStatus(`${cachePrefix}-${cachedResult['startTime']}.json`);
            }).then((oldResult) => {
                const oldBuildNumbers = buildNumbers(oldResult, 'current');
                const newBuildNumbers = buildNumbers(newResult, 'current');
                assert(oldBuildNumbers.length >= 2, 'The old cluster should contain at least two data points');
                assert(newBuildNumbers.length >= 2, 'The new cluster should contain at least two data points');
                assert.deepEqual(oldBuildNumbers.slice(oldBuildNumbers.length - 2), newBuildNumbers.slice(0, 2),
                    'Two conseqcutive clusters should share two data points');
            });
        });
    });

    it("should use lastModified timestamp identical to that in the manifest file", () => {
        const remote = TestServer.remoteAPI();
        return addBuilderForReport(reportWithBuildTime[0]).then(() => {
            return remote.postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return queryPlatformAndMetric('Mountain Lion', 'Time');
        }).then((result) => {
            return remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        }).then((primaryCluster) => {
            return remote.getJSONWithStatus('/api/manifest').then((content) => {
                const manifest = Manifest._didFetchManifest(content);

                const platform = Platform.findByName('Mountain Lion');
                assert.equal(Metric.all().length, 1);
                const metric = Metric.all()[0];
                assert.equal(platform.lastModified(metric), primaryCluster['lastModified']);
            });
        });
    });

    async function reportAfterAddingBuilderAndAggregatorsWithResponse(report)
    {
        await addBuilderForReport(report);
        const db = TestServer.database();
        await Promise.all([
            db.insert('aggregators', {name: 'Arithmetic'}),
            db.insert('aggregators', {name: 'Geometric'}),
        ]);
        return await TestServer.remoteAPI().postJSON('/api/report/', [report]);
    }

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

    it("should allow to report a build request", async () => {
        await MockData.addMockData(TestServer.database());
        let response = await reportAfterAddingBuilderAndAggregatorsWithResponse(reportWithBuildRequest);
        assert.equal(response['status'], 'OK');
        response = await TestServer.remoteAPI().getJSONWithStatus('/api/measurement-set/?analysisTask=500');
        assert.equal(response['status'], 'OK');
        assert.deepEqual(response['measurements'], [[1, 4, 3, 12, 50, [
                ['1', '9', '10.8.2 12C60', null, 0], ['2', '11', '141977', null, 1360140920900]],
            1, 1362046323388, '123', 1, 1, 'current']]);
    });

    const reportWithCommitsNeedsRoundCommitTimeAndBuildRequest = {
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
        "revisions": {
            "macOS": {
                "revision": "10.8.2 12C60",
                "timestamp": '2018-09-27T09:49:52.670499Z',
            },
            "WebKit": {
                "revision": "141977",
                "timestamp": '2018-09-27T09:49:52.670999Z',
            }
        }
    };

    it("commit time of commits from measurement set queried by analysis task should match the one from `/api/commits`", async () => {
        const expectedWebKitCommitTime = 1538041792671;
        const expectedMacOSCommitTime = 1538041792670;
        const remote = TestServer.remoteAPI();
        await MockData.addMockData(TestServer.database());
        let response = await reportAfterAddingBuilderAndAggregatorsWithResponse(reportWithCommitsNeedsRoundCommitTimeAndBuildRequest);
        assert.equal(response['status'], 'OK');

        const rawWebKitCommit = await remote.getJSONWithStatus(`/api/commits/WebKit/141977`);
        assert.equal(rawWebKitCommit.commits[0].time, expectedWebKitCommitTime);

        const rawMacOSCommit = await remote.getJSONWithStatus(`/api/commits/macOS/10.8.2%2012C60`);
        assert.equal(rawMacOSCommit.commits[0].time, expectedMacOSCommitTime);

        response = await TestServer.remoteAPI().getJSONWithStatus('/api/measurement-set/?analysisTask=500');
        assert.equal(response['status'], 'OK');
        assert.deepEqual(response['measurements'], [[1, 4, 3, 12, 50, [
            ['1', '9', '10.8.2 12C60', null, expectedMacOSCommitTime], ['2', '11', '141977', null, expectedWebKitCommitTime]],
            1, 1362046323388, '123', 1, 1, 'current']]);
    });

    const reportWithCommitsNeedsRoundCommitTime = {
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
        "revisions": {
            "macOS": {
                "revision": "10.8.2 12C60",
                "timestamp": '2018-09-27T09:49:52.670499Z',
            },
            "WebKit": {
                "revision": "141977",
                "timestamp": '2018-09-27T09:49:52.670999Z',
            }
        }
    };

    it("commit time of commits from measurement set queried by platform and metric should match the one from `/api/commits`", async () => {
        const expectedWebKitCommitTime = 1538041792671;
        const expectedMacOSCommitTime = 1538041792670;
        const remote = TestServer.remoteAPI();
        await addBuilderForReport(reportWithCommitsNeedsRoundCommitTime);
        await remote.postJSON('/api/report/', [reportWithCommitsNeedsRoundCommitTime]);

        const rawWebKitCommit = await remote.getJSONWithStatus(`/api/commits/WebKit/141977`);
        assert.equal(rawWebKitCommit.commits[0].time, expectedWebKitCommitTime);

        const rawMacOSCommit = await remote.getJSONWithStatus(`/api/commits/macOS/10.8.2%2012C60`);
        assert.equal(rawMacOSCommit.commits[0].time, expectedMacOSCommitTime);

        const result = await queryPlatformAndMetric('Mountain Lion', 'FrameRate');
        const primaryCluster = await remote.getJSONWithStatus(`/api/measurement-set/?platform=${result.platformId}&metric=${result.metricId}`);
        assert.deepEqual(primaryCluster.configurations.current, [[1, 4, 3, 12, 50, false, [
            [1, 1, '10.8.2 12C60', null, expectedMacOSCommitTime], [2, 2, '141977', null, expectedWebKitCommitTime]],
            expectedWebKitCommitTime, 1, 1362046323388, '123', 1]]);
    });
});
