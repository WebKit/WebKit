'use strict';

const assert = require('assert');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
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
        'repositoryGroups': [
            {name: 'webkit-only', repositories: [MockData.webkitRepositoryId()]},
            {name: 'system-and-webkit', repositories: [MockData.macosRepositoryId(), MockData.webkitRepositoryId()]},
        ]
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
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets: [{[webkit.id()]: '191622'}, {}]}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidRevisionSets');
            });
        });
    });

    it('should return "InvalidRevisionSets" when the number of revision sets is less than two', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets: [{[webkit.id()]: '191622'}]}).then((content) => {
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
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets: [{[webkit.id()]: '191622'}, {[webkit.id()]: '1'}]}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'RevisionNotFound');
            });
        });
    });

    it('should return "InvalidUploadedFile" when revision sets contains an invalid file ID', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            const webkit = Repository.all().find((repository) => repository.name() == 'WebKit');
            const revisionSets = [{[webkit.id()]: '191622', 'customRoots': ['1']}, {[webkit.id()]: '1'}];
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets}).then((content) => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'InvalidUploadedFile');
            });
        });
    });

    it('should return "InvalidRepository" when a revision set uses a repository name instead of a repository id', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, revisionSets: [{'WebKit': '191622'}, {}]}).then((content) => {
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

    it('should create a test group from commitSets with the repetition count of one when repetitionCount is omitted', () => {
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, commitSets: {'macOS': ['15A284', '15A284'], 'WebKit': ['191622', '191623']}}).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchByTask(taskId);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 1);
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
            const params = {name: 'test', task: taskId, revisionSets: [{[webkit.id()]: '191622'}, {[webkit.id()]: '191623'}]};
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', params).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchByTask(taskId);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 1);
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

    it('should create a test group using different repository groups if needed', () => {
        let webkit;
        let macos;
        return addTriggerableAndCreateTask('some task').then((taskId) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            macos = Repository.findById(MockData.macosRepositoryId());
            const params = {name: 'test', task: taskId, repetitionCount: 2,
                revisionSets: [{[macos.id()]: '15A284', [webkit.id()]: '191622'}, {[webkit.id()]: '191623'}]};
            let insertedGroupId;
            return PrivilegedAPI.sendRequest('create-test-group', params).then((content) => {
                insertedGroupId = content['testGroupId'];
                return TestGroup.fetchByTask(taskId);
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
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
                const revisionSets = [{[webkit.id()]: '191622', [macos.id()]: '15A284'},
                    {[webkit.id()]: '191622', [macos.id()]: '15A284', 'customRoots': [uploadedFile['id']]}];
                return PrivilegedAPI.sendRequest('create-test-group', {name: 'test', task: taskId, repetitionCount: 2, revisionSets}).then((content) => {
                    insertedGroupId = content['testGroupId'];
                    return TestGroup.fetchByTask(taskId);
                });
            }).then((testGroups) => {
                assert.equal(testGroups.length, 1);
                const group = testGroups[0];
                assert.equal(group.id(), insertedGroupId);
                assert.equal(group.repetitionCount(), 2);
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

    it('should create a test group with an analysis task', () => {
        let insertedGroupId;
        let webkit;
        return addTriggerableAndCreateTask('some task').then(() => {
            webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
            const revisionSets = [{[webkit.id()]: '191622'}, {[webkit.id()]: '191623'}];
            return PrivilegedAPI.sendRequest('create-test-group',
                {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), revisionSets});
        }).then((result) => {
            insertedGroupId = result['testGroupId'];
            return Promise.all([AnalysisTask.fetchById(result['taskId']), TestGroup.fetchByTask(result['taskId'])]);
        }).then((result) => {
            const [analysisTask, testGroups] = result;

            assert.equal(analysisTask.name(), 'other task');

            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group.id(), insertedGroupId);
            assert.equal(group.repetitionCount(), 1);
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
    });

});
