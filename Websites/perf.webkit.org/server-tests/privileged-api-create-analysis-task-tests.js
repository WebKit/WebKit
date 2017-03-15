'use strict';

let assert = require('assert');

let MockData = require('./resources/mock-data.js');
let TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

const reportWithRevision = [{
    "buildNumber": "124",
    "buildTime": "2015-10-27T15:34:51",
    "revisions": {
        "WebKit": {
            "revision": "191622",
            "timestamp": '2015-10-27T11:36:56.878473Z',
        },
    },
    "builderName": "someBuilder",
    "builderPassword": "somePassword",
    "platform": "some platform",
    "tests": {
        "Suite": {
            "metrics": {
                "Time": ["Arithmetic"],
            },
            "tests": {
                "test1": {
                    "metrics": {"Time": { "current": [11] }},
                }
            }
        },
    }}];

const anotherReportWithRevision = [{
    "buildNumber": "125",
    "buildTime": "2015-10-27T17:27:41",
    "revisions": {
        "WebKit": {
            "revision": "191623",
            "timestamp": '2015-10-27T16:38:10.768995Z',
        },
    },
    "builderName": "someBuilder",
    "builderPassword": "somePassword",
    "platform": "some platform",
    "tests": {
        "Suite": {
            "metrics": {
                "Time": ["Arithmetic"],
            },
            "tests": {
                "test1": {
                    "metrics": {"Time": { "current": [12] }},
                }
            }
        },
    }}];

describe('/privileged-api/create-analysis-task', function () {
    prepareServerTest(this);

    it('should return "MissingName" on an empty request', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {}).then((content) => {
            assert(false, 'should never be reached');
        }, (response) => {
            assert.equal(response['status'], 'MissingName');
        });
    });

    it('should return "InvalidStartRun" when startRun is missing but endRun is set', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', endRun: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (response) => {
            assert.equal(response['status'], 'InvalidStartRun');
        });
    });

    it('should return "InvalidEndRun" when endRun is missing but startRun is set', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (response) => {
            assert.equal(response['status'], 'InvalidEndRun');
        });
    });

    it('should return "InvalidStartRun" when startRun is not a valid integer', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: "foo", endRun: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (response) => {
            assert.equal(response['status'], 'InvalidStartRun');
        });
    });

    it('should return "InvalidEndRun" when endRun is not a valid integer', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: "foo"}).then((content) => {
            assert(false, 'should never be reached');
        }, (response) => {
            assert.equal(response['status'], 'InvalidEndRun');
        });
    });

    it('should return "InvalidStartRun" when startRun is invalid', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 100, endRun: 1}).then((content) => {
                assert(false, 'should never be reached');
            }, (response) => {
                assert.equal(response['status'], 'InvalidStartRun');
            });
        });
    });

    it('should return "InvalidEndRun" when endRun is invalid', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: 100}).then((content) => {
                assert(false, 'should never be reached');
            }, (response) => {
                assert.equal(response['status'], 'InvalidEndRun');
            });
        });
    });

    it('should return "InvalidTimeRange" when startRun and endRun are identical', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: 1}).then((content) => {
                assert(false, 'should never be reached');
            }, (response) => {
                assert.equal(response['status'], 'InvalidTimeRange');
            });
        });
    });

    it('should return "RunConfigMismatch" when startRun and endRun come from a different test configurations', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: 2}).then((content) => {
                assert(false, 'should never be reached');
            }, (response) => {
                assert.equal(response['status'], 'RunConfigMismatch');
            });
        });
    });

    it('should create an analysis task when name, startRun, and endRun are set properly', () => {
        const db = TestServer.database();
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        }).then(() => {
            return Manifest.fetch();
        }).then(() => {
            const test1 = Test.findByPath(['Suite', 'test1']);
            const platform = Platform.findByName('some platform');
            return db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: platform.id()});
        }).then((configRow) => {
            return db.selectRows('test_runs', {config: configRow['id']});
        }).then((testRuns) => {
            assert.equal(testRuns.length, 2);
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: testRuns[0]['id'], endRun: testRuns[1]['id']});
        }).then((content) => {
            return AnalysisTask.fetchById(content['taskId']);
        }).then((task) => {
            assert.equal(task.name(), 'hi');
            assert(!task.hasResults());
            assert(!task.hasPendingRequests());
            assert.deepEqual(task.bugs(), []);
            assert.deepEqual(task.causes(), []);
            assert.deepEqual(task.fixes(), []);
            assert.equal(task.changeType(), null);
            assert.equal(task.platform().label(), 'some platform');
            assert.equal(task.metric().test().label(), 'test1');
        });
    });

    it('should return "DuplicateAnalysisTask" when there is already an analysis task for the specified range', () => {
        const db = TestServer.database();
        let startId;
        let endId;
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        }).then(() => {
            return Manifest.fetch();
        }).then(() => {
            const test1 = Test.findByPath(['Suite', 'test1']);
            const platform = Platform.findByName('some platform');
            return db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: platform.id()});
        }).then((configRow) => {
            return db.selectRows('test_runs', {config: configRow['id']});
        }).then((testRuns) => {
            assert.equal(testRuns.length, 2);
            startId = testRuns[0]['id'];
            endId = testRuns[1]['id'];
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: startId, endRun: endId});
        }).then((content) => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: startId, endRun: endId}).then(() => {
                assert(false, 'should never be reached');
            }, (response) => {
                assert.equal(response['status'], 'DuplicateAnalysisTask');
            });
        }).then(() => {
            return db.selectAll('analysis_tasks');
        }).then((tasks) => {
            assert.equal(tasks.length, 1);
        });
    });

    it('should create an analysis task with analysis strategies when they are specified', () => {
        const db = TestServer.database();
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        }).then(() => {
            return Manifest.fetch();
        }).then(() => {
            const test1 = Test.findByPath(['Suite', 'test1']);
            const platform = Platform.findByName('some platform');
            return db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: platform.id()});
        }).then((configRow) => {
            return db.selectRows('test_runs', {config: configRow['id']});
        }).then((testRuns) => {
            assert.equal(testRuns.length, 2);
            return PrivilegedAPI.sendRequest('create-analysis-task', {
                name: 'hi',
                startRun: testRuns[0]['id'],
                endRun: testRuns[1]['id'],
                segmentationStrategy: "time series segmentation",
                testRangeStrategy: "student's t-test"});
        }).then(() => {
            return Promise.all([db.selectFirstRow('analysis_tasks'), db.selectAll('analysis_strategies')]);
        }).then((results) => {
            const [taskRow, strategies] = results;
            assert(taskRow['segmentation']);
            assert(taskRow['test_range']);

            const strategyIdMap = {};
            for (let strategy of strategies)
                strategyIdMap[strategy['id']] = strategy;

            assert.equal(strategyIdMap[taskRow['segmentation']]['name'], "time series segmentation");
            assert.equal(strategyIdMap[taskRow['test_range']]['name'], "student's t-test");
        });
    });

});
