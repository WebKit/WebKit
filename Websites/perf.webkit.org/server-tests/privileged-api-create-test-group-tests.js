'use strict';

let assert = require('assert');

let MockData = require('./resources/mock-data.js');
let TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

function createAnalysisTask(name)
{
    const reportWithRevision = [{
        "buildNumber": "124",
        "buildTime": "2015-10-27T15:34:51",
        "revisions": {
            "WebKit": {
                "revision": "191622",
                "timestamp": '2015-10-27T11:36:56.878473Z',
            },
            "macOS": {
                "revision": "15A284",
            }
        },
        "builderName": "someBuilder",
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "platform": "some platform",
        "tests": {
            "some test": {
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
            "macOS": {
                "revision": "15A284",
            }
        },
        "builderName": "someBuilder",
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "platform": "some platform",
        "tests": {
            "some test": {
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

    const db = TestServer.database();
    const remote = TestServer.remoteAPI();
    return addSlaveForReport(reportWithRevision[0]).then(() => {
        return remote.postJSON('/api/report/', reportWithRevision);
    }).then(() => {
        return remote.postJSON('/api/report/', anotherReportWithRevision);
    }).then((result) => {
        return Manifest.fetch();
    }).then(() => {
        const test = Test.findByPath(['some test', 'test1']);
        const platform = Platform.findByName('some platform');
        return db.selectFirstRow('test_configurations', {metric: test.metrics()[0].id(), platform: platform.id()});
    }).then((configRow) => {
        return db.selectRows('test_runs', {config: configRow['id']});
    }).then((testRuns) => {
        assert.equal(testRuns.length, 2);
        return PrivilegedAPI.sendRequest('create-analysis-task', {
            name: name,
            startRun: testRuns[0]['id'],
            endRun: testRuns[1]['id'],
        });
    }).then((content) => content['taskId']);
}

function addTriggerableAndCreateTask(name)
{
    const report = {
        'slaveName': 'anotherSlave',
        'slavePassword': 'anotherPassword',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId()}
        ],
    };
    return MockData.addMockData(TestServer.database()).then(() => {
        return addSlaveForReport(report);
    }).then(() => {
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', report);
    }).then(() => {
        return createAnalysisTask(name);
    });
}

describe('/privileged-api/create-test-group', function () {
    prepareServerTest(this);

    it('should return "InvalidName" on an empty request', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidName');
        });
    });

    it('should return "InvalidTask" when task is not specified', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', commitSets: [[1]]}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidTask');
        });
    });

    it('should return "InvalidTask" when task is not a valid integer', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 'foo', commitSets: [[1]]}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidTask');
        });
    });

    it('should return "InvalidCommitSets" when commit sets are not specified', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 1, repetitionCount: 1}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidCommitSets');
        });
    });

    it('should return "InvalidCommitSets" when commit sets is empty', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 1, repetitionCount: 1, commitSets: {}}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidCommitSets');
        });
    });

    it('should return "InvalidTask" when there is no matching task', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 1, repetitionCount: 1, commitSets: {'WebKit': []}}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidTask');
        });
    });

    it('should return "InvalidRepetitionCount" when repetitionCount is not a valid integer', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 1, repetitionCount: 'foo', commitSets: {'WebKit': []}}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidRepetitionCount');
        });
    });

    it('should return "InvalidRepetitionCount" when repetitionCount is a negative integer', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 1, repetitionCount: -5, commitSets: {'WebKit': []}}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidRepetitionCount');
        });
    });

    it('should return "InvalidTask" when there is no matching task', () => {
        return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: 1, commitSets: {'WebKit': []}}).then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidTask');
        });
    });

    it('should return "TriggerableNotFoundForTask" when there is no matching triggerable', () => {
        return createAnalysisTask('some task').then((taskId) => {
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'WebKit': []}}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'TriggerableNotFoundForTask');
            });
        });
    });

    it('should return "InvalidCommitSets" when each repository specifies zero revisions', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'WebKit': []}}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidCommitSets');
            });
        });
    });

    it('should return "RepositoryNotFound" when commit sets contains an invalid repository', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'Foo': []}}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'RepositoryNotFound');
            });
        });
    });

    it('should return "RevisionNotFound" when commit sets contains an invalid revision', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'WebKit': ['1']}}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'RevisionNotFound');
            });
        });
    });

    it('should return "InvalidCommitSets" when commit sets contains an inconsistent number of revisions', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'WebKit': ['191622', '191623'], 'macOS': ['15A284']}}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidCommitSets');
            });
        });
    });

    it('should create a test group with the repetition count of one when repetitionCount is omitted', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'WebKit': ['191622', '191623']}}).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchByTask(taskId);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 1);
                const requests = group.buildRequests();
                assert.equal(requests.length, 2);
                const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
                assert.deepEqual(requests[0].commitSet().repositories(), [webkit]);
                assert.deepEqual(requests[1].commitSet().repositories(), [webkit]);
                assert.equal(requests[0].commitSet().revisionForRepository(webkit), '191622');
                assert.equal(requests[1].commitSet().revisionForRepository(webkit), '191623');
            });
        });
    });

    it('should create a test group with the repetition count of two with two repositories', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2,
                commitSets: {'WebKit': ['191622', '191623'], 'macOS': ['15A284', '15A284']}}).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchByTask(taskId);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
                const requests = group.buildRequests();
                assert.equal(requests.length, 4);
                const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
                const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
                const set1 = requests[0].commitSet();
                const set2 = requests[1].commitSet();
                assert.equal(requests[2].commitSet(), set1);
                assert.equal(requests[3].commitSet(), set2);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [webkit, macos]);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set2.repositories()), [webkit, macos]);
                assert.equal(set1.revisionForRepository(webkit), '191622');
                assert.equal(set1.revisionForRepository(macos), '15A284');
                assert.equal(set2.revisionForRepository(webkit), '191623');
                assert.equal(set2.revisionForRepository(macos), '15A284');
                assert.equal(set1.commitForRepository(macos), set2.commitForRepository(macos));
            });
        });
    });

});
