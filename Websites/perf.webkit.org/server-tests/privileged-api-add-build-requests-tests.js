'use strict';

const assert = require('assert');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const assertThrows = require('../server-tests/resources/common-operations.js').assertThrows;

async function createAnalysisTask(name, webkitRevisions = ["191622", "191623"])
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
    await addSlaveForReport(reportWithRevision[0]);
    await remote.postJSON('/api/report/', reportWithRevision);
    await remote.postJSON('/api/report/', anotherReportWithRevision);
    await Manifest.fetch();
    const test = Test.findByPath(['some test', 'test1']);
    const platform = Platform.findByName('some platform');
    const configRow = await db.selectFirstRow('test_configurations', {metric: test.metrics()[0].id(), platform: platform.id()});
    const testRuns = await db.selectRows('test_runs', {config: configRow['id']});

    assert.equal(testRuns.length, 2);
    const content = await PrivilegedAPI.sendRequest('create-analysis-task', {
        name: name,
        startRun: testRuns[0]['id'],
        endRun: testRuns[1]['id'],
        needsNotification: true,
    });
    return content['taskId'];
}

async function addTriggerableAndCreateTask(name, webkitRevisions)
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
    await MockData.addMockData(TestServer.database());
    await addSlaveForReport(report);
    await TestServer.remoteAPI().postJSON('/api/update-triggerable/', report);
    await createAnalysisTask(name, webkitRevisions);
}

describe('/privileged-api/add-build-requests', function() {
    prepareServerTest(this, 'node');
    beforeEach(() => {
        PrivilegedAPI.configure('test', 'password');
    });

    it('should be able to add build requests to test group', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, repetitionCount: 2, revisionSets});
        const insertedGroupId = result['testGroupId'];

        await PrivilegedAPI.sendRequest('update-test-group', {'group': insertedGroupId, mayNeedMoreRequests: true});

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.mayNeedMoreRequests(), true);
        assert.equal(group.repetitionCount(), 2);
        assert.equal(group.initialRepetitionCount(), 2);

        await PrivilegedAPI.sendRequest('add-build-requests', {group: insertedGroupId, addCount: 2});

        const updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].repetitionCount(), 4);
        assert.equal(updatedGroups[0].initialRepetitionCount(), 2);
        assert.equal(group.mayNeedMoreRequests(), true);
        for (const commitSet of updatedGroups[0].requestedCommitSets()) {
            const buildRequests = updatedGroups[0].requestsForCommitSet(commitSet);
            assert.equal(buildRequests.length, 4);
        }
    });

    it('should not be able to add build requests to a hidden test group', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(),
                hidden: true, needsNotification: false, repetitionCount: 2, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.mayNeedMoreRequests(), false);
        assert.equal(group.repetitionCount(), 2);
        assert.equal(group.initialRepetitionCount(), 2);
        await group.updateHiddenFlag(true);

        await assertThrows('CannotAddToHiddenTestGroup', async () => await PrivilegedAPI.sendRequest('add-build-requests', {group: insertedGroupId, addCount: 2}))
    });
});