'use strict';

let assert = require('assert');

let MockData = require('./resources/mock-data.js');
let TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const assertThrows = require('./resources/common-operations.js').assertThrows;

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

const reportWithRevisionNoTimestamp = [{
    "buildNumber": "124",
    "buildTime": "2015-10-27T15:34:51",
    "revisions": {
        "WebKit": {
            "revision": "191622",
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

const anotherReportWithRevisionNoTimestamp = [{
    "buildNumber": "125",
    "buildTime": "2015-10-27T17:27:41",
    "revisions": {
        "WebKit": {
            "revision": "191623",
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

describe('/privileged-api/create-analysis-task with browser privileged api', function () {
    prepareServerTest(this);

    it('should return "MissingName" on an empty request', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'MissingName');
        });
    });

    it('should return "InvalidStartRun" when startRun is missing but endRun is set', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', endRun: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidStartRun');
        });
    });

    it('should return "InvalidEndRun" when endRun is missing but startRun is set', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidEndRun');
        });
    });

    it('should return "InvalidStartRun" when startRun is not a valid integer', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: "foo", endRun: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidStartRun');
        });
    });

    it('should return "InvalidEndRun" when endRun is not a valid integer', () => {
        return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: "foo"}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidEndRun');
        });
    });

    it('should return "InvalidStartRun" when startRun is invalid', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 100, endRun: 1}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidStartRun');
            });
        });
    });

    it('should return "InvalidEndRun" when endRun is invalid', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: 100}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidEndRun');
            });
        });
    });

    it('should return "InvalidTimeRange" when startRun and endRun are identical', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: 1}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidTimeRange');
            });
        });
    });

    it('should return "RunConfigMismatch" when startRun and endRun come from a different test configurations', () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(() => {
            return PrivilegedAPI.sendRequest('create-analysis-task', {name: 'hi', startRun: 1, endRun: 2}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'RunConfigMismatch');
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

    it('should create an analysis task and use build time as fallback when commit time is not available', () => {
        const db = TestServer.database();
        return addBuilderForReport(reportWithRevisionNoTimestamp[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevisionNoTimestamp);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevisionNoTimestamp);
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
            assert.equal(task.startTime(), 1445960091000);
            assert.equal(task.endTime(), 1445966861000);
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
            }, (error) => {
                assert.equal(error, 'DuplicateAnalysisTask');
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

    it('should failed with "TriggerableNotFoundForTask" when there is no matching triggerable', async () => {
        const db = TestServer.database();
        await addBuilderForReport(reportWithRevision[0]);
        await TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        await TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        await Manifest.fetch();

        const test1 = Test.findByPath(['Suite', 'test1']);
        const somePlatform = Platform.findByName('some platform');
        const configRow = await db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: somePlatform.id()});
        const testRuns = await db.selectRows('test_runs', {config: configRow['id']});
        assert.equal(testRuns.length, 2);

        const webkitRepositoryRow = await db.selectFirstRow('repositories', {name: 'WebKit'});
        const webkitId = webkitRepositoryRow.id;

        const oneRevisionSet = {[webkitId]: {revision: '191622'}};
        const anotherRevisionSet = {[webkitId]: {revision: '191623'}};

        await assertThrows('TriggerableNotFoundForTask', () =>
            PrivilegedAPI.sendRequest('create-analysis-task', {name: 'confirm', repetitionCount: 1,
                revisionSets: [oneRevisionSet, anotherRevisionSet],
                startRun: testRuns[0]['id'], endRun: testRuns[1]['id']}));
    });

    it('should create an analysis task with no test group when repetition count is 0', async () => {
        const db = TestServer.database();
        await addBuilderForReport(reportWithRevision[0]);
        await TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        await TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        await Manifest.fetch();

        const test1 = Test.findByPath(['Suite', 'test1']);
        const platform = Platform.findByName('some platform');
        const configRow = await db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: platform.id()});
        const testRuns = await db.selectRows('test_runs', {config: configRow['id']});
        assert.equal(testRuns.length, 2);

        const webkitRepositoryRow = await db.selectFirstRow('repositories', {name: 'WebKit'});
        const webkitId = webkitRepositoryRow.id;

        const oneRevisionSet = {[webkitId]: {revision: '191622'}};
        const anotherRevisionSet = {[webkitId]: {revision: '191623'}};

        const content = await PrivilegedAPI.sendRequest('create-analysis-task', {name: 'confirm', repetitionCount: 0,
            revisionSets: [oneRevisionSet, anotherRevisionSet],
            startRun: testRuns[0]['id'], endRun: testRuns[1]['id']});

        TestServer.cleanDataDirectory();
        await Manifest.fetch();

        const task = await AnalysisTask.fetchById(content['taskId']);
        assert.equal(task.name(), 'confirm');
        assert(!task.hasResults());
        assert(!task.hasPendingRequests());
        assert.deepEqual(task.bugs(), []);
        assert.deepEqual(task.causes(), []);
        assert.deepEqual(task.fixes(), []);
        assert.equal(task.changeType(), null);
        assert.equal(task.platform().label(), 'some platform');
        assert.equal(task.metric().test().label(), 'test1');

        const testGroups = await TestGroup.fetchForTask(task.id());
        assert.equal(testGroups.length, 0);
    });

    it('should create an analysis task with test group when commit set list and a positive repetition count is specified', async () => {
        const webkitId = 1;
        const platformId = 1;
        const test1Id = 2;
        const triggerableId = 1234;

        const db = TestServer.database();
        await db.insert('tests', {id: 1, name: 'Suite'});
        await db.insert('tests', {id: test1Id, name: 'test1', parent: 1});
        await db.insert('repositories', {id: webkitId, name: 'WebKit'});
        await db.insert('platforms', {id: platformId, name: 'some platform'});
        await db.insert('build_triggerables', {id: 1234, name: 'test-triggerable'});
        await db.insert('triggerable_repository_groups', {id: 2345, name: 'webkit-only', triggerable: triggerableId});
        await db.insert('triggerable_repositories', {repository: webkitId, group: 2345});
        await db.insert('triggerable_configurations', {test: test1Id, platform: platformId, triggerable: triggerableId});
        await addBuilderForReport(reportWithRevision[0]);

        await TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        await TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        await Manifest.fetch();

        let test1 = Test.findById(test1Id);
        let somePlatform = Platform.findById(platformId);
        const configRow = await db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: somePlatform.id()});
        const testRuns = await db.selectRows('test_runs', {config: configRow['id']});
        assert.equal(testRuns.length, 2);

        const oneRevisionSet = {[webkitId]: {revision: '191622'}};
        const anotherRevisionSet = {[webkitId]: {revision: '191623'}};

        const content = await PrivilegedAPI.sendRequest('create-analysis-task', {name: 'confirm', repetitionCount: 1,
            testGroupName: 'Confirm', revisionSets: [oneRevisionSet, anotherRevisionSet],
            startRun: testRuns[0]['id'], endRun: testRuns[1]['id']});

        const task = await AnalysisTask.fetchById(content['taskId']);
        assert.equal(task.name(), 'confirm');
        assert(!task.hasResults());
        assert(task.hasPendingRequests());
        assert.deepEqual(task.bugs(), []);
        assert.deepEqual(task.causes(), []);
        assert.deepEqual(task.fixes(), []);
        assert.equal(task.changeType(), null);
        assert.equal(task.platform().label(), 'some platform');
        assert.equal(task.metric().test().label(), 'test1');

        const testGroups = await TestGroup.fetchForTask(task.id());
        assert.equal(testGroups.length, 1);
        const testGroup = testGroups[0];
        assert.equal(testGroup.name(), 'Confirm');
        assert.ok(!testGroup.needsNotification());
        const buildRequests = testGroup.buildRequests();
        assert.equal(buildRequests.length, 2);

        assert.equal(buildRequests[0].triggerable().id(), triggerableId);
        assert.equal(buildRequests[0].triggerable().id(), triggerableId);

        assert.equal(buildRequests[0].testGroup(), testGroup);
        assert.equal(buildRequests[1].testGroup(), testGroup);

        assert.equal(buildRequests[0].platform(), task.platform());
        assert.equal(buildRequests[1].platform(), task.platform());

        assert.equal(buildRequests[0].analysisTaskId(), task.id());
        assert.equal(buildRequests[1].analysisTaskId(), task.id());

        assert.equal(buildRequests[0].test(), test1);
        assert.equal(buildRequests[1].test(), test1);

        assert.ok(!buildRequests[0].isBuild());
        assert.ok(!buildRequests[1].isBuild());
        assert.ok(buildRequests[0].isTest());
        assert.ok(buildRequests[1].isTest());

        const firstCommitSet = buildRequests[0].commitSet();
        const secondCommitSet = buildRequests[1].commitSet();
        const webkitRepository = Repository.findById(webkitId);
        assert.equal(firstCommitSet.commitForRepository(webkitRepository).revision(), '191622');
        assert.equal(secondCommitSet.commitForRepository(webkitRepository).revision(), '191623');
    });
});

describe('/privileged-api/create-analysis-task with node privileged api', function () {
    prepareServerTest(this, 'node');
    beforeEach(() => {
        PrivilegedAPI.configure('test', 'password');
    });

    it('should return "SlaveNotFound" when incorrect slave user and password combination is provided and no analysis task, test group or build request should be created', async () => {
        PrivilegedAPI.configure('test', 'wrongpassword');
        const db = TestServer.database();
        await addBuilderForReport(reportWithRevision[0]);
        await TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        await TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        await Manifest.fetch();

        const test1 = Test.findByPath(['Suite', 'test1']);
        const somePlatform = Platform.findByName('some platform');
        const configRow = await db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: somePlatform.id()});
        const testRuns = await db.selectRows('test_runs', {config: configRow['id']});
        assert.equal(testRuns.length, 2);

        const webkitRepositoryRow = await db.selectFirstRow('repositories', {name: 'WebKit'});
        const webkitId = webkitRepositoryRow.id;

        const oneRevisionSet = {[webkitId]: {revision: '191622'}};
        const anotherRevisionSet = {[webkitId]: {revision: '191623'}};

        await assertThrows('SlaveNotFound', () =>
            PrivilegedAPI.sendRequest('create-analysis-task', {name: 'confirm', repetitionCount: 1,
                revisionSets: [oneRevisionSet, anotherRevisionSet],
                startRun: testRuns[0]['id'], endRun: testRuns[1]['id']}));

        const allAnalysisTasks = await db.selectRows('analysis_tasks');
        assert.ok(!allAnalysisTasks.length);

        const allTestGroups = await db.selectRows('analysis_test_groups');
        assert.ok(!allTestGroups.length);

        const allBuildRequests = await db.selectRows('build_requests');
        assert.ok(!allBuildRequests.length);
    });

    it('should create an analysis task with test group when commit set list and a positive repetition count is specified', async () => {
        const webkitId = 1;
        const platformId = 1;
        const test1Id = 2;
        const triggerableId = 1234;

        const db = TestServer.database();
        await db.insert('tests', {id: 1, name: 'Suite'});
        await db.insert('tests', {id: test1Id, name: 'test1', parent: 1});
        await db.insert('repositories', {id: webkitId, name: 'WebKit'});
        await db.insert('platforms', {id: platformId, name: 'some platform'});
        await db.insert('build_triggerables', {id: 1234, name: 'test-triggerable'});
        await db.insert('triggerable_repository_groups', {id: 2345, name: 'webkit-only', triggerable: triggerableId});
        await db.insert('triggerable_repositories', {repository: webkitId, group: 2345});
        await db.insert('triggerable_configurations', {test: test1Id, platform: platformId, triggerable: triggerableId});
        await addBuilderForReport(reportWithRevision[0]);

        await TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        await TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        await Manifest.fetch();

        let test1 = Test.findById(test1Id);
        let somePlatform = Platform.findById(platformId);
        const configRow = await db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: somePlatform.id()});
        const testRuns = await db.selectRows('test_runs', {config: configRow['id']});
        assert.equal(testRuns.length, 2);

        const oneRevisionSet = {[webkitId]: {revision: '191622'}};
        const anotherRevisionSet = {[webkitId]: {revision: '191623'}};

        const content = await PrivilegedAPI.sendRequest('create-analysis-task', {name: 'confirm', repetitionCount: 1,
            testGroupName: 'Confirm', revisionSets: [oneRevisionSet, anotherRevisionSet],
            startRun: testRuns[0]['id'], endRun: testRuns[1]['id']});

        const task = await AnalysisTask.fetchById(content['taskId']);
        assert.equal(task.name(), 'confirm');
        assert(!task.hasResults());
        assert(task.hasPendingRequests());
        assert.deepEqual(task.bugs(), []);
        assert.deepEqual(task.causes(), []);
        assert.deepEqual(task.fixes(), []);
        assert.equal(task.changeType(), null);
        assert.equal(task.platform().label(), 'some platform');
        assert.equal(task.metric().test().label(), 'test1');

        const testGroups = await TestGroup.fetchForTask(task.id());
        assert.equal(testGroups.length, 1);
        const testGroup = testGroups[0];
        assert.equal(testGroup.name(), 'Confirm');
        assert.ok(!testGroup.needsNotification());
        const buildRequests = testGroup.buildRequests();
        assert.equal(buildRequests.length, 2);

        assert.equal(buildRequests[0].triggerable().id(), triggerableId);
        assert.equal(buildRequests[0].triggerable().id(), triggerableId);

        assert.equal(buildRequests[0].testGroup(), testGroup);
        assert.equal(buildRequests[1].testGroup(), testGroup);

        assert.equal(buildRequests[0].platform(), task.platform());
        assert.equal(buildRequests[1].platform(), task.platform());

        assert.equal(buildRequests[0].analysisTaskId(), task.id());
        assert.equal(buildRequests[1].analysisTaskId(), task.id());

        assert.equal(buildRequests[0].test(), test1);
        assert.equal(buildRequests[1].test(), test1);

        assert.ok(!buildRequests[0].isBuild());
        assert.ok(!buildRequests[1].isBuild());
        assert.ok(buildRequests[0].isTest());
        assert.ok(buildRequests[1].isTest());

        const firstCommitSet = buildRequests[0].commitSet();
        const secondCommitSet = buildRequests[1].commitSet();
        const webkitRepository = Repository.findById(webkitId);
        assert.equal(firstCommitSet.commitForRepository(webkitRepository).revision(), '191622');
        assert.equal(secondCommitSet.commitForRepository(webkitRepository).revision(), '191623');
    });

    it('should create an analysis task with test group and respect the "needsNotification" flag in the http request', async () => {
        const webkitId = 1;
        const platformId = 1;
        const test1Id = 2;
        const triggerableId = 1234;

        const db = TestServer.database();
        await db.insert('tests', {id: 1, name: 'Suite'});
        await db.insert('tests', {id: test1Id, name: 'test1', parent: 1});
        await db.insert('repositories', {id: webkitId, name: 'WebKit'});
        await db.insert('platforms', {id: platformId, name: 'some platform'});
        await db.insert('build_triggerables', {id: 1234, name: 'test-triggerable'});
        await db.insert('triggerable_repository_groups', {id: 2345, name: 'webkit-only', triggerable: triggerableId});
        await db.insert('triggerable_repositories', {repository: webkitId, group: 2345});
        await db.insert('triggerable_configurations', {test: test1Id, platform: platformId, triggerable: triggerableId});
        await addBuilderForReport(reportWithRevision[0]);

        await TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        await TestServer.remoteAPI().postJSON('/api/report/', anotherReportWithRevision);
        await Manifest.fetch();

        let test1 = Test.findById(test1Id);
        let somePlatform = Platform.findById(platformId);
        const configRow = await db.selectFirstRow('test_configurations', {metric: test1.metrics()[0].id(), platform: somePlatform.id()});
        const testRuns = await db.selectRows('test_runs', {config: configRow['id']});
        assert.equal(testRuns.length, 2);

        const oneRevisionSet = {[webkitId]: {revision: '191622'}};
        const anotherRevisionSet = {[webkitId]: {revision: '191623'}};

        const content = await PrivilegedAPI.sendRequest('create-analysis-task', {name: 'confirm', repetitionCount: 1,
            testGroupName: 'Confirm', revisionSets: [oneRevisionSet, anotherRevisionSet],
            startRun: testRuns[0]['id'], endRun: testRuns[1]['id'], needsNotification: true});

        const task = await AnalysisTask.fetchById(content['taskId']);
        assert.equal(task.name(), 'confirm');
        assert(!task.hasResults());
        assert(task.hasPendingRequests());
        assert.deepEqual(task.bugs(), []);
        assert.deepEqual(task.causes(), []);
        assert.deepEqual(task.fixes(), []);
        assert.equal(task.changeType(), null);
        assert.equal(task.platform().label(), 'some platform');
        assert.equal(task.metric().test().label(), 'test1');

        const testGroups = await TestGroup.fetchForTask(task.id());
        assert.equal(testGroups.length, 1);
        const testGroup = testGroups[0];
        assert.equal(testGroup.name(), 'Confirm');
        assert.ok(testGroup.needsNotification());
        const buildRequests = testGroup.buildRequests();
        assert.equal(buildRequests.length, 2);

        assert.equal(buildRequests[0].triggerable().id(), triggerableId);
        assert.equal(buildRequests[0].triggerable().id(), triggerableId);

        assert.equal(buildRequests[0].testGroup(), testGroup);
        assert.equal(buildRequests[1].testGroup(), testGroup);

        assert.equal(buildRequests[0].platform(), task.platform());
        assert.equal(buildRequests[1].platform(), task.platform());

        assert.equal(buildRequests[0].analysisTaskId(), task.id());
        assert.equal(buildRequests[1].analysisTaskId(), task.id());

        assert.equal(buildRequests[0].test(), test1);
        assert.equal(buildRequests[1].test(), test1);

        assert.ok(!buildRequests[0].isBuild());
        assert.ok(!buildRequests[1].isBuild());
        assert.ok(buildRequests[0].isTest());
        assert.ok(buildRequests[1].isTest());

        const firstCommitSet = buildRequests[0].commitSet();
        const secondCommitSet = buildRequests[1].commitSet();
        const webkitRepository = Repository.findById(webkitId);
        assert.equal(firstCommitSet.commitForRepository(webkitRepository).revision(), '191622');
        assert.equal(secondCommitSet.commitForRepository(webkitRepository).revision(), '191623');
    });
});
