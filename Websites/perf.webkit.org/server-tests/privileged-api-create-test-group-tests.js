'use strict';

const assert = require('assert');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

function createAnalysisTask(name, webkitRevisions = ["191622", "191623"])
{
    const reportWithRevision = [{
        "buildNumber": "124",
        "buildTime": "2015-10-27T15:34:51",
        "revisions": {
            "WebKit": {
                "revision": webkitRevisions[0],
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
                "revision": webkitRevisions[1],
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

function addTriggerableAndCreateTask(name, webkitRevisions)
{
    const report = {
        'slaveName': 'anotherSlave',
        'slavePassword': 'anotherPassword',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId()},
            {test: MockData.someTestId(), platform: MockData.otherPlatformId()},
        ],
        'repositoryGroups': [
            {name: 'os-only', acceptsRoot: true, repositories: [
                {repository: MockData.macosRepositoryId(), acceptsPatch: false},
            ]},
            {name: 'webkit-only', acceptsRoot: true, repositories: [
                {repository: MockData.webkitRepositoryId(), acceptsPatch: true},
            ]},
            {name: 'system-and-webkit', acceptsRoot: true, repositories: [
                {repository: MockData.macosRepositoryId(), acceptsPatch: false},
                {repository: MockData.webkitRepositoryId(), acceptsPatch: true}
            ]},
            {name: 'system-webkit-sjc', acceptsRoot: true, repositories: [
                {repository: MockData.macosRepositoryId(), acceptsPatch: false},
                {repository: MockData.jscRepositoryId(), acceptsPatch: false},
                {repository: MockData.webkitRepositoryId(), acceptsPatch: true}
            ]},
        ]
    };
    return MockData.addMockData(TestServer.database()).then(() => {
        return addSlaveForReport(report);
    }).then(() => {
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', report);
    }).then(() => {
        return createAnalysisTask(name, webkitRevisions);
    });
}

describe('/privileged-api/create-test-group', function () {
    prepareServerTest(this);
    TemporaryFile.inject();

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

    it('should return "InvalidRevisionSets" when a revision set is empty', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets: [{[webkit.id()]: {revision: '191622'}}, {}]}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidRevisionSets');
            });
        });
    });

    it('should return "InvalidRevisionSets" when the number of revision sets is less than two', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets: [{[webkit.id()]: {revision: '191622'}}]}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidRevisionSets');
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

    it('should return "RevisionNotFound" when revision sets contains an invalid revision', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '1a'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'RevisionNotFound');
            });
        });
    });

    it('should return "AmbiguousRevision" when there are multiple commits that match the specified revision string', () => {
        return addTriggerableAndCreateTask('some task', ['2ceda45d3cd63cde58d0dbf5767714e03d902e43', '2c71a8ddc1f661663ccfd1a29c633ba57e879533']).then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            const revisionSets = [{[webkit.id()]: {revision: '2ceda'}}, {[webkit.id()]: {revision: '2c'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'AmbiguousRevision');
            });
        });
    });

    it('should return "RevisionNotFound" when the end of a Git hash is specified', () => {
        return addTriggerableAndCreateTask('some task', ['2ceda45d3cd63cde58d0dbf5767714e03d902e43', '5471a8ddc1f661663ccfd1a29c633ba57e879533']).then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            const revisionSets = [{[webkit.id()]: {revision: '2ceda45d3cd63cde58d0dbf5767714e03d902e43'}}, {[webkit.id()]: {revision: '57e879533'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'RevisionNotFound');
            });
        });
    });

    it('should return "InvalidUploadedFile" when revision sets contains an invalid file ID', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, 'customRoots': ['1']}, {[webkit.id()]: {revision: '1'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidUploadedFile');
            });
        });
    });

    it('should return "InvalidRepository" when a revision set uses a repository name instead of a repository id', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const revisionSets = [{'WebKit': {revision: '191622'}}, {}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidRepository');
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

    it('should return "DuplicateTestGroupName" when there is already a test group of the same name', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const commitSets = {'WebKit': ['191622', '191623']};
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets}).then((content) => {
                assert(content['testGroupId']);
                return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets});
            }).then(() => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'DuplicateTestGroupName');
            });
        });
    });

    it('should return "InvalidOwnerRevision" when commit ownership is not valid', () => {
        let taskId;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            const ownedJSC = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore' && repository.ownerId())[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191621'}},
                {[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-9191', ownerRevision: '191622'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then(() => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidOwnerRevision');
        });
    });

    it('should return "InvalidCommitOwnership" when commit ownership is not valid', () => {
        let taskId;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            const ownedJSC = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore' && repository.ownerId())[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191622'}},
                {[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-9191', ownerRevision: '191622'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then(() => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidCommitOwnership');
        });
    });

    it('should allow a commit set in which owner of a commit is not in the same set', () => {
        let taskId;
        let groupId;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            const ownedJSC = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore' && repository.ownerId())[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191622'}},
                {[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-9191', ownerRevision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 1, revisionSets});
        }).then((content) => {
            assert.equal(content['status'], 'OK');
            groupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), groupId);
            assert.equal(group.repetitionCount(), 1);
            assert.ok(!group.needsNotification());
            const requests = group.buildRequests();
            assert.equal(requests.length, 4);
            assert(requests[0].isBuild());
            assert(!requests[0].isTest());
            assert(requests[1].isBuild());
            assert(!requests[1].isTest());
            assert(!requests[2].isBuild());
            assert(requests[2].isTest());
            assert(!requests[3].isBuild());
            assert(requests[3].isTest());

            const macos = Repository.findById(MockData.macosRepositoryId());
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [ownedJSC, webkit, macos]);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [ownedJSC, webkit, macos]);
            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set0.revisionForRepository(ownedJSC), 'owned-jsc-6161');
            assert.equal(set1.revisionForRepository(macos), '15A284');
            assert.equal(set1.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(ownedJSC), 'owned-jsc-9191');
        });
    });

    it('should allow a commit set in which repository of owner commit is not specified at all', () => {
        let taskId;
        let groupId;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            const jsc = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore' && repository.ownerId())[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191622'}},
                {[macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-9191', ownerRevision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 1, revisionSets});
        }).then((content) => {
            assert.equal(content['status'], 'OK');
            groupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), groupId);
            assert.equal(group.repetitionCount(), 1);
            assert.ok(!group.needsNotification());
            const requests = group.buildRequests();
            assert.equal(requests.length, 4);
            assert(requests[0].isBuild());
            assert(!requests[0].isTest());
            assert(requests[1].isBuild());
            assert(!requests[1].isTest());
            assert(!requests[2].isBuild());
            assert(requests[2].isTest());
            assert(!requests[3].isBuild());
            assert(requests[3].isTest());

            const macos = Repository.findById(MockData.macosRepositoryId());
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [ownedJSC, webkit, macos]);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [ownedJSC, macos]);
            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set0.revisionForRepository(ownedJSC), 'owned-jsc-6161');
            assert.equal(set1.revisionForRepository(macos), '15A284');
            assert.equal(set1.revisionForRepository(ownedJSC), 'owned-jsc-9191');
        });
    });

    it('should create a test group from commitSets with the repetition count of one when repetitionCount is omitted', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'macOS': ['15A284', '15A284'], 'WebKit': ['191622', '191623']}}).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchForTask(taskId, true);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 1);
                assert.ok(!group.needsNotification());
                const requests = group.buildRequests();
                assert.equal(requests.length, 2);

                const macos = Repository.findById(MockData.macosRepositoryId());
                const webkit = Repository.findById(MockData.webkitRepositoryId());
                const set0 = requests[0].commitSet();
                const set1 = requests[1].commitSet();
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [webkit, macos]);
                assert.equal(set0.revisionForRepository(macos), '15A284');
                assert.equal(set0.revisionForRepository(webkit), '191622');
                assert.equal(set1.revisionForRepository(macos), '15A284');
                assert.equal(set1.revisionForRepository(webkit), '191623');

                const repositoryGroup = requests[0].repositoryGroup();
                assert.equal(repositoryGroup.name(), 'system-and-webkit');
                assert.equal(requests[1].repositoryGroup(), repositoryGroup);
                assert(repositoryGroup.accepts(set0));
                assert(repositoryGroup.accepts(set1));
            });
        });
    });

    it('should create a test group from revisionSets with the repetition count of one when repetitionCount is omitted', () => {
        let webkit;
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
            const params = {name: 'test', task: taskId, revisionSets};
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', params).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchForTask(taskId, true);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 1);
                assert.ok(!group.needsNotification());
                const requests = group.buildRequests();
                assert.equal(requests.length, 2);

                const set0 = requests[0].commitSet();
                const set1 = requests[1].commitSet();
                assert.deepEqual(set0.repositories(), [webkit]);
                assert.deepEqual(set1.repositories(), [webkit]);
                assert.equal(set0.revisionForRepository(webkit), '191622');
                assert.equal(set1.revisionForRepository(webkit), '191623');

                const repositoryGroup = requests[0].repositoryGroup();
                assert.equal(repositoryGroup.name(), 'webkit-only');
                assert.equal(repositoryGroup, requests[1].repositoryGroup());
                assert(repositoryGroup.accepts(set0));
                assert(repositoryGroup.accepts(set1));
            });
        });
    });

    it('should create a test group with the repetition count of two with two repositories', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2,
                commitSets: {'WebKit': ['191622', '191623'], 'macOS': ['15A284', '15A284']}}).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchForTask(taskId, true);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
                assert.ok(!group.needsNotification());
                const requests = group.buildRequests();
                assert.equal(requests.length, 4);
                const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
                const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];

                const set0 = requests[0].commitSet();
                const set1 = requests[1].commitSet();
                assert.equal(requests[2].commitSet(), set0);
                assert.equal(requests[3].commitSet(), set1);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [webkit, macos]);
                assert.equal(set0.revisionForRepository(webkit), '191622');
                assert.equal(set0.revisionForRepository(macos), '15A284');
                assert.equal(set1.revisionForRepository(webkit), '191623');
                assert.equal(set1.revisionForRepository(macos), '15A284');
                assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));

                const repositoryGroup = requests[0].repositoryGroup();
                assert.equal(repositoryGroup.name(), 'system-and-webkit');
                assert.equal(requests[1].repositoryGroup(), repositoryGroup);
                assert.equal(requests[2].repositoryGroup(), repositoryGroup);
                assert.equal(requests[3].repositoryGroup(), repositoryGroup);
                assert(repositoryGroup.accepts(set0));
                assert(repositoryGroup.accepts(set1));
            });
        });
    });

    it('should create a test group using Git partial hashes', () => {
        let webkit;
        let macos;
        return addTriggerableAndCreateTask('some task', ['2ceda45d3cd63cde58d0dbf5767714e03d902e43', '5471a8ddc1f661663ccfd1a29c633ba57e879533']).then((taskId) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            macos = Repository.findById(MockData.macosRepositoryId());
            const revisionSets = [{[macos.id()]: {revision: '15A284'}, [webkit.id()]: {revision: '2ceda'}},
                {[macos.id()]: {revision: '15A284'}, [webkit.id()]: {revision: '5471a'}}];
            const params = {name: 'test', task: taskId, repetitionCount: 2, revisionSets};
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', params).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchForTask(taskId, true);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
                assert.ok(!group.needsNotification());
                const requests = group.buildRequests();
                assert.equal(requests.length, 4);

                const set0 = requests[0].commitSet();
                const set1 = requests[1].commitSet();
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [webkit, macos]);
                assert.equal(set0.revisionForRepository(webkit), '2ceda45d3cd63cde58d0dbf5767714e03d902e43');
                assert.equal(set0.revisionForRepository(macos), '15A284');
                assert.equal(set1.revisionForRepository(webkit), '5471a8ddc1f661663ccfd1a29c633ba57e879533');
                assert.equal(set1.revisionForRepository(macos), '15A284');

                const repositoryGroup0 = requests[0].repositoryGroup();
                assert.equal(repositoryGroup0.name(), 'system-and-webkit');
                assert.equal(repositoryGroup0, requests[2].repositoryGroup());
                const repositoryGroup1 = requests[1].repositoryGroup();
                assert.equal(repositoryGroup1, repositoryGroup0);
                assert(repositoryGroup0.accepts(set0));
                assert(repositoryGroup0.accepts(set1));
            });
        });
    });

    it('should create a test group using different repository groups if needed', () => {
        let webkit;
        let macos;
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            macos = Repository.findById(MockData.macosRepositoryId());
            const revisionSets = [{[macos.id()]: {revision: '15A284'}, [webkit.id()]: {revision: '191622'}},
                {[webkit.id()]: {revision: '191623'}}];
            const params = {name: 'test', task: taskId, repetitionCount: 2, revisionSets};
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', params).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchForTask(taskId, true);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
                assert.ok(!group.needsNotification());
                const requests = group.buildRequests();
                assert.equal(requests.length, 4);

                const set0 = requests[0].commitSet();
                const set1 = requests[1].commitSet();
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
                assert.deepEqual(set1.repositories(), [webkit]);
                assert.equal(set0.revisionForRepository(webkit), '191622');
                assert.equal(set0.revisionForRepository(macos), '15A284');
                assert.equal(set1.revisionForRepository(webkit), '191623');
                assert.equal(set1.revisionForRepository(macos), null);

                const repositoryGroup0 = requests[0].repositoryGroup();
                assert.equal(repositoryGroup0.name(), 'system-and-webkit');
                assert.equal(repositoryGroup0, requests[2].repositoryGroup());
                assert(repositoryGroup0.accepts(set0));
                assert(!repositoryGroup0.accepts(set1));

                const repositoryGroup1 = requests[1].repositoryGroup();
                assert.equal(repositoryGroup1.name(), 'webkit-only');
                assert.equal(repositoryGroup1, requests[3].repositoryGroup());
                assert(!repositoryGroup1.accepts(set0));
                assert(repositoryGroup1.accepts(set1));
            });
        });
    });

    it('should create a test group with a custom root', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            let insertedGroupId;
            const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            let uploadedFile;
            return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
                return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
            }).then((response) => {
                uploadedFile = response['uploadedFile'];
                const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}},
                    {[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, 'customRoots': [uploadedFile['id']]}];
                return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets}).then((content) => {
                    insertedGroupId = content['testGroupId'];
                    return TestGroup.fetchForTask(taskId, true);
                });
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
                assert.ok(!group.needsNotification());
                const requests = group.buildRequests();
                assert.equal(requests.length, 4);

                const set0 = requests[0].commitSet();
                const set1 = requests[1].commitSet();
                assert.equal(requests[2].commitSet(), set0);
                assert.equal(requests[3].commitSet(), set1);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
                assert.deepEqual(set0.customRoots(), []);
                assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [webkit, macos]);
                assert.deepEqual(set1.customRoots(), [UploadedFile.ensureSingleton(uploadedFile['id'], uploadedFile)]);
                assert.equal(set0.revisionForRepository(webkit), '191622');
                assert.equal(set0.revisionForRepository(webkit), set1.revisionForRepository(webkit));
                assert.equal(set0.commitForRepository(webkit), set1.commitForRepository(webkit));
                assert.equal(set0.revisionForRepository(macos), '15A284');
                assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
                assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
                assert(!set0.equals(set1));
            });
        });
    });

    it('should create a build test group with a patch', () => {
        let taskId;
        let webkit;
        let macos;
        let insertedGroupId;
        let uploadedFile;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const rawFile = response['uploadedFile'];
            uploadedFile = UploadedFile.ensureSingleton(rawFile.id, rawFile);
            const revisionSets = [{[webkit.id()]: {revision: '191622', patch: uploadedFile.id()}, [macos.id()]: {revision: '15A284'}},
                {[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then((content) => {
            insertedGroupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 2);
            assert.ok(!group.needsNotification());
            assert.equal(group.test(), Test.findById(MockData.someTestId()));
            assert.equal(group.platform(), Platform.findById(MockData.somePlatformId()));
            const requests = group.buildRequests();
            assert.equal(requests.length, 6);

            assert.equal(requests[0].isBuild(), true);
            assert.equal(requests[1].isBuild(), true);
            assert.equal(requests[2].isBuild(), false);
            assert.equal(requests[3].isBuild(), false);
            assert.equal(requests[4].isBuild(), false);
            assert.equal(requests[5].isBuild(), false);

            assert.equal(requests[0].isTest(), false);
            assert.equal(requests[1].isTest(), false);
            assert.equal(requests[2].isTest(), true);
            assert.equal(requests[3].isTest(), true);
            assert.equal(requests[4].isTest(), true);
            assert.equal(requests[5].isTest(), true);

            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.equal(requests[2].commitSet(), set0);
            assert.equal(requests[3].commitSet(), set1);
            assert.equal(requests[4].commitSet(), set0);
            assert.equal(requests[5].commitSet(), set1);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [webkit, macos]);
            assert.deepEqual(set1.customRoots(), []);
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set0.revisionForRepository(webkit), set1.revisionForRepository(webkit));
            assert.equal(set0.commitForRepository(webkit), set1.commitForRepository(webkit));
            assert.equal(set0.patchForRepository(webkit), uploadedFile);
            assert.equal(set1.patchForRepository(webkit), null);
            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
            assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
            assert.equal(set0.patchForRepository(macos), null);
            assert.equal(set1.patchForRepository(macos), null);
            assert(!set0.equals(set1));
        });
    });

    it('should create a build test group with a owned commits even when one of group does not contain an owned commit', () => {
        let taskId;
        let webkit;
        let jsc;
        let macos;
        let insertedGroupId;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            jsc = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore')[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}},
                {[webkit.id()]: {revision: '192736'}, [macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-9191', ownerRevision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then((content) => {
            insertedGroupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 2);
            assert.ok(!group.needsNotification());
            assert.equal(group.test(), Test.findById(MockData.someTestId()));
            assert.equal(group.platform(), Platform.findById(MockData.somePlatformId()));
            const requests = group.buildRequests();
            assert.equal(requests.length, 6);

            assert.equal(requests[0].isBuild(), true);
            assert.equal(requests[1].isBuild(), true);
            assert.equal(requests[2].isBuild(), false);
            assert.equal(requests[3].isBuild(), false);
            assert.equal(requests[4].isBuild(), false);
            assert.equal(requests[5].isBuild(), false);

            assert.equal(requests[0].isTest(), false);
            assert.equal(requests[1].isTest(), false);
            assert.equal(requests[2].isTest(), true);
            assert.equal(requests[3].isTest(), true);
            assert.equal(requests[4].isTest(), true);
            assert.equal(requests[5].isTest(), true);

            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.equal(requests[2].commitSet(), set0);
            assert.equal(requests[3].commitSet(), set1);
            assert.equal(requests[4].commitSet(), set0);
            assert.equal(requests[5].commitSet(), set1);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [jsc, webkit, macos]);
            assert.deepEqual(set1.customRoots(), []);
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), '192736');
            assert.equal(set0.patchForRepository(webkit), null);
            assert.equal(set1.patchForRepository(webkit), null);
            assert.equal(set0.requiresBuildForRepository(webkit), false);
            assert.equal(set1.requiresBuildForRepository(webkit), false);
            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
            assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
            assert.equal(set0.patchForRepository(macos), null);
            assert.equal(set1.patchForRepository(macos), null);
            assert.equal(set0.requiresBuildForRepository(macos), false);
            assert.equal(set1.requiresBuildForRepository(macos), false);
            assert.equal(set0.revisionForRepository(jsc), null);
            assert.equal(set1.revisionForRepository(jsc), 'owned-jsc-9191');
            assert.equal(set0.patchForRepository(jsc), null);
            assert.equal(set1.patchForRepository(jsc), null);
            assert.equal(set0.ownerRevisionForRepository(jsc), null);
            assert.equal(set1.ownerRevisionForRepository(jsc), '192736');
            assert.equal(set1.requiresBuildForRepository(jsc), true);
            assert(!set0.equals(set1));
        });
    });

    it('should create a test group with a owned commits even when no patch is specified', () => {
        let taskId;
        let webkit;
        let jsc;
        let macos;
        let insertedGroupId;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            jsc = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore')[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191622'}},
                {[webkit.id()]: {revision: '192736'}, [macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-9191', ownerRevision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then((content) => {
            insertedGroupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 2);
            assert.ok(!group.needsNotification());
            assert.equal(group.test(), Test.findById(MockData.someTestId()));
            assert.equal(group.platform(), Platform.findById(MockData.somePlatformId()));
            const requests = group.buildRequests();
            assert.equal(requests.length, 6);

            assert.equal(requests[0].isBuild(), true);
            assert.equal(requests[1].isBuild(), true);
            assert.equal(requests[2].isBuild(), false);
            assert.equal(requests[3].isBuild(), false);
            assert.equal(requests[4].isBuild(), false);
            assert.equal(requests[5].isBuild(), false);

            assert.equal(requests[0].isTest(), false);
            assert.equal(requests[1].isTest(), false);
            assert.equal(requests[2].isTest(), true);
            assert.equal(requests[3].isTest(), true);
            assert.equal(requests[4].isTest(), true);
            assert.equal(requests[5].isTest(), true);

            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.equal(requests[2].commitSet(), set0);
            assert.equal(requests[3].commitSet(), set1);
            assert.equal(requests[4].commitSet(), set0);
            assert.equal(requests[5].commitSet(), set1);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [jsc, webkit, macos]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [jsc, webkit, macos]);
            assert.deepEqual(set1.customRoots(), []);
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), '192736');
            assert.equal(set0.patchForRepository(webkit), null);
            assert.equal(set1.patchForRepository(webkit), null);
            assert.equal(set0.requiresBuildForRepository(webkit), false);
            assert.equal(set1.requiresBuildForRepository(webkit), false);
            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
            assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
            assert.equal(set0.patchForRepository(macos), null);
            assert.equal(set1.patchForRepository(macos), null);
            assert.equal(set0.requiresBuildForRepository(macos), false);
            assert.equal(set1.requiresBuildForRepository(macos), false);
            assert.equal(set0.revisionForRepository(jsc), 'owned-jsc-6161');
            assert.equal(set1.revisionForRepository(jsc), 'owned-jsc-9191');
            assert.equal(set0.patchForRepository(jsc), null);
            assert.equal(set1.patchForRepository(jsc), null);
            assert.equal(set0.ownerRevisionForRepository(jsc), '191622');
            assert.equal(set1.ownerRevisionForRepository(jsc), '192736');
            assert.equal(set0.requiresBuildForRepository(jsc), true);
            assert.equal(set1.requiresBuildForRepository(jsc), true);
            assert(!set0.equals(set1));
        });
    });

    it('should create a test group with a owned commits and a patch', () => {
        let taskId;
        let webkit;
        let macos;
        let jsc;
        let insertedGroupId;
        let uploadedFile;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            jsc = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore')[0];
            return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const rawFile = response['uploadedFile'];
            uploadedFile = UploadedFile.ensureSingleton(rawFile.id, rawFile);
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191622'}},
                {[webkit.id()]: {revision: '192736', patch: uploadedFile.id()}, [macos.id()]: {revision: '15A284'}, [jsc.id()]: {revision: 'owned-jsc-9191', ownerRevision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then((content) => {
            insertedGroupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 2);
            assert.ok(!group.needsNotification());
            assert.equal(group.test(), Test.findById(MockData.someTestId()));
            assert.equal(group.platform(), Platform.findById(MockData.somePlatformId()));
            const requests = group.buildRequests();
            assert.equal(requests.length, 6);

            assert.equal(requests[0].isBuild(), true);
            assert.equal(requests[1].isBuild(), true);
            assert.equal(requests[2].isBuild(), false);
            assert.equal(requests[3].isBuild(), false);
            assert.equal(requests[4].isBuild(), false);
            assert.equal(requests[5].isBuild(), false);

            assert.equal(requests[0].isTest(), false);
            assert.equal(requests[1].isTest(), false);
            assert.equal(requests[2].isTest(), true);
            assert.equal(requests[3].isTest(), true);
            assert.equal(requests[4].isTest(), true);
            assert.equal(requests[5].isTest(), true);

            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.equal(requests[2].commitSet(), set0);
            assert.equal(requests[3].commitSet(), set1);
            assert.equal(requests[4].commitSet(), set0);
            assert.equal(requests[5].commitSet(), set1);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [jsc, webkit, macos]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [jsc, webkit, macos]);
            assert.deepEqual(set1.customRoots(), []);

            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), '192736');
            assert.equal(set0.patchForRepository(webkit), null);
            assert.equal(set1.patchForRepository(webkit), uploadedFile);
            assert.equal(set0.ownerRevisionForRepository(webkit), null);
            assert.equal(set1.ownerRevisionForRepository(webkit), null);
            assert.equal(set0.requiresBuildForRepository(webkit), true);
            assert.equal(set1.requiresBuildForRepository(webkit), true);

            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
            assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
            assert.equal(set0.patchForRepository(macos), null);
            assert.equal(set1.patchForRepository(macos), null);
            assert.equal(set0.ownerRevisionForRepository(macos), null);
            assert.equal(set1.ownerRevisionForRepository(macos), null);
            assert.equal(set0.requiresBuildForRepository(macos), false);
            assert.equal(set1.requiresBuildForRepository(macos), false);

            assert.equal(set0.revisionForRepository(jsc), 'owned-jsc-6161');
            assert.equal(set1.revisionForRepository(jsc), 'owned-jsc-9191');
            assert.equal(set0.patchForRepository(jsc), null);
            assert.equal(set1.patchForRepository(jsc), null);
            assert.equal(set0.ownerRevisionForRepository(jsc), '191622');
            assert.equal(set1.ownerRevisionForRepository(jsc), '192736');
            assert.equal(set0.requiresBuildForRepository(jsc), true);
            assert.equal(set1.requiresBuildForRepository(jsc), true);
            assert(!set0.equals(set1));
        });
    });

    it('should still work even if components with same name but one is owned, one is not', () => {
        let taskId;
        let webkit;
        let macos;
        let jsc;
        let ownedJSC;
        let insertedGroupId;
        let uploadedFile;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            return MockData.addAnotherTriggerable(TestServer.database());
        }).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            jsc = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore' && !repository.ownerId())[0];
            ownedJSC = Repository.all().filter((repository) => repository.name() == 'JavaScriptCore' && repository.ownerId())[0];
            return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const rawFile = response['uploadedFile'];
            uploadedFile = UploadedFile.ensureSingleton(rawFile.id, rawFile);
            const revisionSets = [{[jsc.id()]: {revision: 'jsc-6161'}, [webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-6161', ownerRevision: '191622'}},
                {[jsc.id()]: {revision: 'jsc-9191'}, [webkit.id()]: {revision: '192736', patch: uploadedFile.id()}, [macos.id()]: {revision: '15A284'}, [ownedJSC.id()]: {revision: 'owned-jsc-9191', ownerRevision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then((content) => {
            insertedGroupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 2);
            assert.equal(group.test(), Test.findById(MockData.someTestId()));
            assert.ok(!group.needsNotification());
            assert.equal(group.platform(), Platform.findById(MockData.somePlatformId()));
            const requests = group.buildRequests();
            assert.equal(requests.length, 6);

            assert.equal(requests[0].isBuild(), true);
            assert.equal(requests[1].isBuild(), true);
            assert.equal(requests[2].isBuild(), false);
            assert.equal(requests[3].isBuild(), false);
            assert.equal(requests[4].isBuild(), false);
            assert.equal(requests[5].isBuild(), false);

            assert.equal(requests[0].isTest(), false);
            assert.equal(requests[1].isTest(), false);
            assert.equal(requests[2].isTest(), true);
            assert.equal(requests[3].isTest(), true);
            assert.equal(requests[4].isTest(), true);
            assert.equal(requests[5].isTest(), true);

            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.equal(requests[2].commitSet(), set0);
            assert.equal(requests[3].commitSet(), set1);
            assert.equal(requests[4].commitSet(), set0);
            assert.equal(requests[5].commitSet(), set1);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [jsc, ownedJSC, webkit, macos]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [jsc, ownedJSC, webkit, macos]);
            assert.deepEqual(set1.customRoots(), []);

            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), '192736');
            assert.equal(set0.patchForRepository(webkit), null);
            assert.equal(set1.patchForRepository(webkit), uploadedFile);
            assert.equal(set0.ownerRevisionForRepository(webkit), null);
            assert.equal(set1.ownerRevisionForRepository(webkit), null);
            assert.equal(set0.requiresBuildForRepository(webkit), true);
            assert.equal(set1.requiresBuildForRepository(webkit), true);

            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
            assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
            assert.equal(set0.patchForRepository(macos), null);
            assert.equal(set1.patchForRepository(macos), null);
            assert.equal(set0.ownerRevisionForRepository(macos), null);
            assert.equal(set1.ownerRevisionForRepository(macos), null);
            assert.equal(set0.requiresBuildForRepository(macos), false);
            assert.equal(set1.requiresBuildForRepository(macos), false);

            assert.equal(set0.revisionForRepository(ownedJSC), 'owned-jsc-6161');
            assert.equal(set1.revisionForRepository(ownedJSC), 'owned-jsc-9191');
            assert.equal(set0.patchForRepository(ownedJSC), null);
            assert.equal(set1.patchForRepository(ownedJSC), null);
            assert.equal(set0.ownerRevisionForRepository(ownedJSC), '191622');
            assert.equal(set1.ownerRevisionForRepository(ownedJSC), '192736');
            assert.equal(set0.requiresBuildForRepository(ownedJSC), true);
            assert.equal(set1.requiresBuildForRepository(ownedJSC), true);

            assert.equal(set0.revisionForRepository(jsc), 'jsc-6161');
            assert.equal(set1.revisionForRepository(jsc), 'jsc-9191');
            assert.equal(set0.patchForRepository(jsc), null);
            assert.equal(set1.patchForRepository(jsc), null);
            assert.equal(set0.ownerRevisionForRepository(jsc), null);
            assert.equal(set1.ownerRevisionForRepository(jsc), null);
            assert.equal(set0.requiresBuildForRepository(jsc), false);
            assert.equal(set1.requiresBuildForRepository(jsc), false);
            assert(!set0.equals(set1));
        });
    });

    it('should not create a build request to build a patch when the commit set does not have a patch', () => {
        let taskId;
        let webkit;
        let macos;
        let insertedGroupId;
        let uploadedFile;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const rawFile = response['uploadedFile'];
            uploadedFile = UploadedFile.ensureSingleton(rawFile.id, rawFile);
            const revisionSets = [{[macos.id()]: {revision: '15A284'}},
                {[webkit.id()]: {revision: '191622', patch: uploadedFile.id()}, [macos.id()]: {revision: '15A284'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then((content) => {
            insertedGroupId = content['testGroupId'];
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 2);
            assert.ok(!group.needsNotification());
            assert.equal(group.test(), Test.findById(MockData.someTestId()));
            assert.equal(group.platform(), Platform.findById(MockData.somePlatformId()));
            const requests = group.buildRequests();
            assert.equal(requests.length, 5);

            assert.equal(requests[0].isBuild(), true);
            assert.equal(requests[1].isBuild(), false);
            assert.equal(requests[2].isBuild(), false);
            assert.equal(requests[3].isBuild(), false);
            assert.equal(requests[4].isBuild(), false);

            assert.equal(requests[0].isTest(), false);
            assert.equal(requests[1].isTest(), true);
            assert.equal(requests[2].isTest(), true);
            assert.equal(requests[3].isTest(), true);
            assert.equal(requests[4].isTest(), true);

            const set0 = requests[0].commitSet();
            const set1 = requests[1].commitSet();
            assert.equal(requests[2].commitSet(), set0);
            assert.equal(requests[3].commitSet(), set1);
            assert.equal(requests[4].commitSet(), set0);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set0.repositories()), [webkit, macos]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(Repository.sortByNamePreferringOnesWithURL(set1.repositories()), [macos]);
            assert.deepEqual(set1.customRoots(), []);
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), null);
            assert.equal(set0.patchForRepository(webkit), uploadedFile);
            assert.equal(set0.revisionForRepository(macos), '15A284');
            assert.equal(set0.revisionForRepository(macos), set1.revisionForRepository(macos));
            assert.equal(set0.commitForRepository(macos), set1.commitForRepository(macos));
            assert.equal(set0.patchForRepository(macos), null);
            assert.equal(set1.patchForRepository(macos), null);
            assert(!set0.equals(set1));
        });
    });

    it('should return "PatchNotAccepted" when a patch is specified for a repository that does not accept a patch', () => {
        let taskId;
        let webkit;
        let macos;
        let uploadedFile;
        return addTriggerableAndCreateTask('some task').then((id) => taskId = id).then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            macos = Repository.all().filter((repository) => repository.name() == 'macOS')[0];
            return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const rawFile = response['uploadedFile'];
            uploadedFile = UploadedFile.ensureSingleton(rawFile.id, rawFile);
            const revisionSets = [{[webkit.id()]: {revision: '191622'}, [macos.id()]: {revision: '15A284', patch: uploadedFile.id()}},
                {[webkit.id()]: {revision: '192736'}, [macos.id()]: {revision: '15A284'}}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets});
        }).then(() => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'PatchNotAccepted');
        });
    });

    it('should create a test group with an analysis task with needs-notification flag set', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: true, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const [analysisTask, testGroups] = await Promise.all([AnalysisTask.fetchById(result['taskId']), TestGroup.fetchForTask(result['taskId'], true)]);
        assert.equal(analysisTask.name(), 'other task');

        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.repetitionCount(), 1);
        assert.ok(group.needsNotification());
        const requests = group.buildRequests();
        assert.equal(requests.length, 2);

        const set0 = requests[0].commitSet();
        const set1 = requests[1].commitSet();
        assert.deepEqual(set0.repositories(), [webkit]);
        assert.deepEqual(set0.customRoots(), []);
        assert.deepEqual(set1.repositories(), [webkit]);
        assert.deepEqual(set1.customRoots(), []);
        assert.equal(set0.revisionForRepository(webkit), '191622');
        assert.equal(set1.revisionForRepository(webkit), '191623');
    });

    it('should be able to create a test group with needs-notification flag unset', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const [analysisTask, testGroups] = await Promise.all([AnalysisTask.fetchById(result['taskId']), TestGroup.fetchForTask(result['taskId'], true)]);
        assert.equal(analysisTask.name(), 'other task');

        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.repetitionCount(), 1);
        assert.ok(!group.needsNotification());
        const requests = group.buildRequests();
        assert.equal(requests.length, 2);

        const set0 = requests[0].commitSet();
        const set1 = requests[1].commitSet();
        assert.deepEqual(set0.repositories(), [webkit]);
        assert.deepEqual(set0.customRoots(), []);
        assert.deepEqual(set1.repositories(), [webkit]);
        assert.deepEqual(set1.customRoots(), []);
        assert.equal(set0.revisionForRepository(webkit), '191622');
        assert.equal(set1.revisionForRepository(webkit), '191623');
    });

    it('should create a custom test group for an existing custom analysis task', () => {
        let firstResult;
        let secondResult;
        let webkit;
        let test = MockData.someTestId();
        return addTriggerableAndCreateTask('some task').then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
            return PrivilegedAPI.sendRequest('create-test-group',
                {name: 'test1', taskName: 'other task', platform: MockData.somePlatformId(), test, revisionSets});
        }).then((result) => {
            firstResult = result;
            const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '192736'}}];
            return PrivilegedAPI.sendRequest('create-test-group',
                {name: 'test2', task: result['taskId'], platform: MockData.otherPlatformId(), test, revisionSets, repetitionCount: 2});
        }).then((result) => {
            secondResult = result;
            assert.equal(firstResult['taskId'], secondResult['taskId']);
            return Promise.all([AnalysisTask.fetchById(result['taskId']), TestGroup.fetchForTask(result['taskId'], true)]);
        }).then((result) => {
            const [analysisTask, testGroups] = result;

            assert.equal(analysisTask.name(), 'other task');

            assert.equal(testGroups.length, 2);
            TestGroup.sortByName(testGroups);

            assert.equal(testGroups[0].name(), 'test1');
            assert.equal(testGroups[0].repetitionCount(), 1);
            let requests = testGroups[0].buildRequests();
            assert.equal(requests.length, 2);
            let set0 = requests[0].commitSet();
            let set1 = requests[1].commitSet();
            assert.deepEqual(set0.repositories(), [webkit]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(set1.repositories(), [webkit]);
            assert.deepEqual(set1.customRoots(), []);
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), '191623');

            assert.equal(testGroups[1].name(), 'test2');
            assert.equal(testGroups[1].repetitionCount(), 2);
            requests = testGroups[1].buildRequests();
            assert.equal(requests.length, 4);
            set0 = requests[0].commitSet();
            set1 = requests[1].commitSet();
            assert.deepEqual(requests[2].commitSet(), set0);
            assert.deepEqual(requests[3].commitSet(), set1);
            assert.deepEqual(set0.repositories(), [webkit]);
            assert.deepEqual(set0.customRoots(), []);
            assert.deepEqual(set1.repositories(), [webkit]);
            assert.deepEqual(set1.customRoots(), []);
            assert.equal(set0.revisionForRepository(webkit), '191622');
            assert.equal(set1.revisionForRepository(webkit), '192736');
        });
    });
});
