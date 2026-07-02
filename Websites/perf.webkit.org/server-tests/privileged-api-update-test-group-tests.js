'use strict';

const assert = require('assert');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const addWorkerForReport = require('./resources/common-operations.js').addWorkerForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const assertThrows = require('./resources/common-operations.js').assertThrows;

async function createAnalysisTask(name, webkitRevisions = ["191622", "191623"])
{
    const reportWithRevision = [{
        "buildTag": "124",
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
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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
        "buildTag": "125",
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
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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
    await addWorkerForReport(reportWithRevision[0]);
    await remote.postJSON('/api/report/', reportWithRevision);
    await remote.postJSON('/api/report/', anotherReportWithRevision);
    await Manifest.fetch();
    const test = Test.findByPath(['some test', 'test1']);
    const platform = Platform.findByName('some platform');
    const configRow = await db.selectFirstRow('test_configurations', {metric: test.metrics()[0].id(), platform: platform.id()});
    const testRuns = await db.selectRows('test_runs', {config: configRow['id']});

    assert.strictEqual(testRuns.length, 2);
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
        'workerName': 'anotherWorker',
        'workerPassword': 'anotherPassword',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId(), supportedRepetitionTypes: ['alternating', 'sequential']},
            {test: MockData.someTestId(), platform: MockData.otherPlatformId(), supportedRepetitionTypes: ['alternating', 'sequential']},
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
    await addWorkerForReport(report);
    await TestServer.remoteAPI().postJSON('/api/update-triggerable/', report);
    await createAnalysisTask(name, webkitRevisions);
}

describe('/privileged-api/update-test-group', function(){
    prepareServerTest(this, 'node');
    beforeEach(() => {
        PrivilegedAPI.configure('test', 'password');
    });

    it('should return "WorkerNotFound" if invalid worker name and password combination is provided', async () => {
        await addTriggerableAndCreateTask('some task');
        PrivilegedAPI.configure('test', 'wrongpassword');

        await assertThrows('WorkerNotFound', () => PrivilegedAPI.sendRequest('update-test-group', {}));
    });

    it('should return "TestGroupNotSpecified" if test group is not specified', async () => {
        await addTriggerableAndCreateTask('some task');

        await assertThrows('TestGroupNotSpecified', () => PrivilegedAPI.sendRequest('update-test-group', {}));
    });

    it('should return "NotificationSentAtFieldShouldOnlyBeSetWhenNeedsNotificationIsFalse" if "needsNotification" is false but "notificationSentAt" is not set', async () => {
        await addTriggerableAndCreateTask('some task');

        await assertThrows('NotificationSentAtFieldShouldOnlyBeSetWhenNeedsNotificationIsFalse', () =>
            PrivilegedAPI.sendRequest('update-test-group', {group: 1, needsNotification: false}));
    });

    it('should return "NotificationSentAtFieldShouldOnlyBeSetWhenNeedsNotificationIsFalse" if "needsNotification" is true but "notificationSentAt" is set', async () => {
        await addTriggerableAndCreateTask('some task');

        await assertThrows('NotificationSentAtFieldShouldOnlyBeSetWhenNeedsNotificationIsFalse', () =>
            PrivilegedAPI.sendRequest('update-test-group', {group: 1, needsNotification: true, notificationSentAt: (new Date).toISOString()}));
    });

    it('should be able to update "needs_notification" field to false', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: true, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.initialRepetitionCount(), 1);
        assert.strictEqual(group.needsNotification(), true);
        assert.ok(!group.notificationSentAt());

        const notificationSentAt = new Date;
        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, needsNotification: false, notificationSentAt: notificationSentAt.toISOString()});

        const updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].needsNotification(), false);
        assert.strictEqual(updatedGroups[0].notificationSentAt().toISOString(), notificationSentAt.toISOString());
    });

    it('should be able to update "needs_notification" field to true', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.initialRepetitionCount(), 1);
        assert.strictEqual(group.needsNotification(), false);
        assert.ok(!group.notificationSentAt());

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, needsNotification: true});

        const updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].needsNotification(), true);
        assert.ok(!group.notificationSentAt());
    });

    it('should create a test group with "may_need_more_requests" field defaults to false', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        const result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.mayNeedMoreRequests(), false);
        assert.ok(!group.notificationSentAt());

    });

    it('should be able to update "may_need_more_requests" field to true and false', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        const result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.mayNeedMoreRequests(), false);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: true});

        let updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].mayNeedMoreRequests(), true);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: false});

        updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].mayNeedMoreRequests(), false);
    });

    it('should clear "may_need_more_requests" when hiding a test group', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        const result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.mayNeedMoreRequests(), false);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: true});

        let updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].mayNeedMoreRequests(), true);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, hidden: true});

        updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].mayNeedMoreRequests(), false);
        assert.strictEqual(updatedGroups[0].isHidden(), true);
    });

    it('should be able to cancel an alternating test group and clear the "may need more requests" flag', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        const result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(),
            test: MockData.someTestId(), needsNotification: false, repetitionType: 'alternating', revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.mayNeedMoreRequests(), false);
        assert.strictEqual(group.repetitionType(), 'alternating');

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: true});

        let updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].mayNeedMoreRequests(), true);
        let testGroup = updatedGroups[0];
        assert.strictEqual(testGroup.mayNeedMoreRequests(), true);
        let buildRequests = testGroup._orderedBuildRequests();
        assert.strictEqual(buildRequests.length, 2);
        assert(buildRequests[0].isPending());
        assert(buildRequests[1].isPending());

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, cancel: true});

        updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        testGroup = updatedGroups[0];
        assert.strictEqual(testGroup.mayNeedMoreRequests(), false);
        assert(testGroup.hasFinished());
        buildRequests = testGroup._orderedBuildRequests();
        assert.strictEqual(buildRequests.length, 2);
        assert.strictEqual(buildRequests[0].status(), 'canceled');
        assert.strictEqual(buildRequests[1].status(), 'canceled');
    });

    it('should be able to cancel a sequential test group and clear the "may need more requests" flag', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        const result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(),
            test: MockData.someTestId(), needsNotification: false, repetitionType: 'sequential', revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(testGroups.length, 1);
        const group = testGroups[0];
        assert.strictEqual(group.id(), insertedGroupId);
        assert.strictEqual(group.mayNeedMoreRequests(), false);
        assert.strictEqual(group.repetitionType(), 'sequential');

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: true});

        let updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        assert.strictEqual(updatedGroups[0].mayNeedMoreRequests(), true);
        let testGroup = updatedGroups[0];
        assert.strictEqual(testGroup.mayNeedMoreRequests(), true);
        let buildRequests = testGroup._orderedBuildRequests();
        assert.strictEqual(buildRequests.length, 2);
        assert(buildRequests[0].isPending());
        assert(buildRequests[1].isPending());

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, cancel: true});

        updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.strictEqual(updatedGroups.length, 1);
        testGroup = updatedGroups[0];
        assert.strictEqual(testGroup.mayNeedMoreRequests(), false);
        assert(testGroup.hasFinished());
        buildRequests = testGroup._orderedBuildRequests();
        assert.strictEqual(buildRequests.length, 2);
        assert.strictEqual(buildRequests[0].status(), 'canceled');
        assert.strictEqual(buildRequests[1].status(), 'canceled');
    });
});