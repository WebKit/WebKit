'use strict';

const assert = require('assert');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const assertThrows = require('./resources/common-operations.js').assertThrows;

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

describe('/privileged-api/update-test-group', function(){
    prepareServerTest(this, 'node');
    beforeEach(() => {
        PrivilegedAPI.configure('test', 'password');
    });

    it('should return "SlaveNotFound" if invalid slave name and password combination is provided', async () => {
        await addTriggerableAndCreateTask('some task');
        PrivilegedAPI.configure('test', 'wrongpassword');

        await assertThrows('SlaveNotFound', () => PrivilegedAPI.sendRequest('update-test-group', {}));
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
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.repetitionCount(), 1);
        assert.equal(group.needsNotification(), true);
        assert.ok(!group.notificationSentAt());

        const notificationSentAt = new Date;
        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, needsNotification: false, notificationSentAt: notificationSentAt.toISOString()});

        const updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].needsNotification(), false);
        assert.equal(updatedGroups[0].notificationSentAt().toISOString(), notificationSentAt.toISOString());
    });

    it('should be able to update "needs_notification" field to true', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        let result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.repetitionCount(), 1);
        assert.equal(group.needsNotification(), false);
        assert.ok(!group.notificationSentAt());

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, needsNotification: true});

        const updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].needsNotification(), true);
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
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.mayNeedMoreRequests(), false);
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
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.mayNeedMoreRequests(), false);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: true});

        let updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].mayNeedMoreRequests(), true);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: false});

        updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].mayNeedMoreRequests(), false);
    });

    it('should clear "may_need_more_requests" when hiding a test group', async () => {
        await addTriggerableAndCreateTask('some task');
        const webkit = Repository.all().filter((repository) => repository.name() == 'WebKit')[0];
        const revisionSets = [{[webkit.id()]: {revision: '191622'}}, {[webkit.id()]: {revision: '191623'}}];
        const result = await PrivilegedAPI.sendRequest('create-test-group',
            {name: 'test', taskName: 'other task', platform: MockData.somePlatformId(), test: MockData.someTestId(), needsNotification: false, revisionSets});
        const insertedGroupId = result['testGroupId'];

        const testGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(testGroups.length, 1);
        const group = testGroups[0];
        assert.equal(group.id(), insertedGroupId);
        assert.equal(group.mayNeedMoreRequests(), false);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, mayNeedMoreRequests: true});

        let updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].mayNeedMoreRequests(), true);

        await PrivilegedAPI.sendRequest('update-test-group', {group: insertedGroupId, hidden: true});

        updatedGroups = await TestGroup.fetchForTask(result['taskId'], true);
        assert.equal(updatedGroups.length, 1);
        assert.equal(updatedGroups[0].mayNeedMoreRequests(), false);
        assert.equal(updatedGroups[0].isHidden(), true);
    });
});