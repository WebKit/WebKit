'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const BuildbotTriggerable = require('../tools/js/buildbot-triggerable.js').BuildbotTriggerable;
const MockData = require('./resources/mock-data.js');
const MockLogger = require('./resources/mock-logger.js').MockLogger;
const MockRemoteAPI = require('../unit-tests/resources/mock-remote-api.js').MockRemoteAPI;
const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;

const configWithOneTesterTwoBuilders = {
    triggerableName: 'build-webkit',
    lookbackCount: 2,
    buildRequestArgument: 'build-request-id',
    slaveName: 'sync-slave',
    slavePassword: 'password',
    repositoryGroups: {
        'webkit': {
            repositories: {'WebKit': {acceptsPatch: true}},
            testProperties: {'wk': {'revision': 'WebKit'}, 'roots': {'roots': {}}},
            buildProperties: {'wk': {'revision': 'WebKit'}, 'wk-patch': {'patch': 'WebKit'},
                'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-wk'},
                'build-wk': {'ifRepositorySet': ['WebKit'], 'value': true},
                'owned-commits': {'ownedRevisions': 'WebKit'}},
            acceptsRoots: true,
        }
    },
    types: {
        'some': {
            test: ['some test'],
            properties: {'test': 'some-test'},
        }
    },
    builders: {
        'builder-1': {
            builder: 'some tester',
            properties: {forcescheduler: 'force-ab-tests'},
        },
        'builder-2': {
            builder: 'some builder',
            properties: {forcescheduler: 'force-ab-builds'},
        },
        'builder-3': {
            builder: 'other builder',
            properties: {forcescheduler: 'force-ab-builds'},
        },
    },
    buildConfigurations: [
        {platforms: ['some platform'], builders: ['builder-2', 'builder-3']},
    ],
    testConfigurations: [
        {types: ['some'], platforms: ['some platform'], builders: ['builder-1']},
    ],
};

const configWithPlatformName = {
    triggerableName: 'build-webkit',
    lookbackCount: 2,
    buildRequestArgument: 'build-request-id',
    slaveName: 'sync-slave',
    slavePassword: 'password',
    platformArgument: 'platform-name',
    repositoryGroups: {
        'webkit': {
            repositories: {'WebKit': {acceptsPatch: true}},
            testProperties: {'wk': {'revision': 'WebKit'}, 'roots': {'roots': {}}},
            buildProperties: {'wk': {'revision': 'WebKit'}, 'wk-patch': {'patch': 'WebKit'},
                'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-wk'},
                'build-wk': {'ifRepositorySet': ['WebKit'], 'value': true},
                'owned-commits': {'ownedRevisions': 'WebKit'}},
            acceptsRoots: true,
        }
    },
    types: {
        'some': {
            test: ['some test'],
            properties: {'test': 'some-test'},
        }
    },
    builders: {
        'builder-1': {
            builder: 'some tester',
            properties: {forcescheduler: 'force-ab-tests'},
        },
        'builder-2': {
            builder: 'some builder',
            properties: {forcescheduler: 'force-ab-builds'},
        },
        'builder-3': {
            builder: 'other builder',
            properties: {forcescheduler: 'force-ab-builds'},
        },
    },
    buildConfigurations: [
        {platforms: ['some platform'], builders: ['builder-2', 'builder-3']},
    ],
    testConfigurations: [
        {types: ['some'], platforms: ['some platform'], builders: ['builder-1']},
    ],
};

const configWithTwoTesters = {
    triggerableName: 'build-webkit',
    lookbackCount: 2,
    buildRequestArgument: 'build-request-id',
    slaveName: 'sync-slave',
    slavePassword: 'password',
    repositoryGroups: {
        'webkit': {
            repositories: {'WebKit': {acceptsPatch: true}},
            testProperties: {'wk': {'revision': 'WebKit'}, 'roots': {'roots': {}}},
            buildProperties: {'wk': {'revision': 'WebKit'}, 'wk-patch': {'patch': 'WebKit'},
                'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-wk'},
                'build-wk': {'ifRepositorySet': ['WebKit'], 'value': true},
                'owned-commits': {'ownedRevisions': 'WebKit'}},
            acceptsRoots: true,
        }
    },
    types: {
        'some': {
            test: ['some test'],
            properties: {'test': 'some-test'},
        }
    },
    builders: {
        'builder-1': {
            builder: 'some tester',
            properties: {forcescheduler: 'force-ab-tests'},
        },
        'builder-2': {
            builder: 'another tester',
            properties: {forcescheduler: 'force-ab-builds'},
        },
    },
    testConfigurations: [
        {types: ['some'], platforms: ['some platform'], builders: ['builder-1', 'builder-2']},
    ]
};

function assertAndResolveRequest(request, method, url, contentToResolve)
{
    assert.equal(request.method, method);
    assert.equal(request.url, url);
    request.resolve(contentToResolve);
}

function createTriggerable(config = configWithOneTesterTwoBuilders)
{
    let triggerable;
    return MockData.addMockConfiguration(TestServer.database()).then(() => {
        return Manifest.fetch();
    }).then(() => {
        triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, {name: 'sync-slave', password: 'password'}, new MockLogger);
        const syncPromise = triggerable.initSyncers().then(() => triggerable.updateTriggerable());
        assertAndResolveRequest(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
        MockRemoteAPI.reset();
        return syncPromise;
    }).then(() => Manifest.fetch()).then(() => {
        return new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, {name: 'sync-slave', password: 'password'}, new MockLogger);
    });
}

function createTestGroup(task_name='custom task') {
    const someTest = Test.findById(MockData.someTestId());
    const webkit = Repository.findById(MockData.webkitRepositoryId());
    const set1 = new CustomCommitSet;
    set1.setRevisionForRepository(webkit, '191622');
    const set2 = new CustomCommitSet;
    set2.setRevisionForRepository(webkit, '192736');
    return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, [set1, set2]).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    });
}

async function createTestGroupWihPatch()
{
    const patchFile = await TemporaryFile.makeTemporaryFile('patch.dat', 'patch file');
    const originalPrivilegedAPI = global.PrivilegedAPI;
    BrowserPrivilegedAPI._token = null;
    global.PrivilegedAPI = BrowserPrivilegedAPI;
    const uploadedPatchFile = await UploadedFile.uploadFile(patchFile);
    global.PrivilegedAPI = originalPrivilegedAPI;

    const someTest = Test.findById(MockData.someTestId());
    const webkit = Repository.findById(MockData.webkitRepositoryId());
    const set1 = new CustomCommitSet;
    set1.setRevisionForRepository(webkit, '191622', uploadedPatchFile);
    const set2 = new CustomCommitSet;
    set2.setRevisionForRepository(webkit, '191622');
    const task = await TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, [set1, set2]);

    return TestGroup.findAllByTask(task.id())[0];
}

function createTestGroupWihOwnedCommit()
{
    const someTest = Test.findById(MockData.someTestId());
    const webkit = Repository.findById(MockData.webkitRepositoryId());
    const ownedSJC = Repository.findById(MockData.ownedJSCRepositoryId());
    const set1 = new CustomCommitSet;
    set1.setRevisionForRepository(webkit, '191622');
    set1.setRevisionForRepository(ownedSJC, 'owned-jsc-6161', null, '191622');
    const set2 = new CustomCommitSet;
    set2.setRevisionForRepository(webkit, '192736');
    set2.setRevisionForRepository(ownedSJC, 'owned-jsc-9191', null, '192736');
    return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, [set1, set2]).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    });
}

function uploadRoot(buildRequestId, buildNumber, repositoryList = ["WebKit"], buildTime = '2017-05-10T02:54:08.666')
{
    return TemporaryFile.makeTemporaryFile(`root${buildNumber}.dat`, `root for build ${buildNumber} and repository list at ${buildTime}`).then((rootFile) => {
        return TestServer.remoteAPI().postFormData('/api/upload-root/', {
            slaveName: 'sync-slave',
            slavePassword: 'password',
            builderName: 'some builder',
            buildNumber: buildNumber,
            buildTime: buildTime,
            buildRequest: buildRequestId,
            rootFile,
            repositoryList: JSON.stringify(repositoryList),
        });
    });
}

describe('sync-buildbot', function () {
    prepareServerTest(this, 'node');
    TemporaryFile.inject();

    beforeEach(() => {
        MockRemoteAPI.reset('http://build.webkit.org');
        PrivilegedAPI.configure('test', 'password');
    });


    it('should not schedule on another builder if the build was scheduled on one builder before', () => {
        const requests = MockRemoteAPI.requests;
        let firstTestGroup;
        let secondTestGroup;
        let syncPromise;
        let triggerable;
        let taskId = null;
        let anotherTaskId = null;
        return createTriggerable(configWithTwoTesters).then((newTriggerable) => {
            triggerable = newTriggerable;
            return Promise.all([createTestGroup(), createTestGroup('another custom task')]);
        }).then((testGroups) => {
            firstTestGroup = testGroups[0];
            secondTestGroup = testGroups[1];
            taskId = firstTestGroup.task().id();
            anotherTaskId = secondTestGroup.task().id();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 2);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), MockData.pendingBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: 5}));
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('another tester'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 4);
            assertAndResolveRequest(requests[2], 'GET', MockData.recentBuildsUrl('some tester', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: 1}));
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('another tester', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[4], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.pendingBuildsUrl('another tester'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 8);
            assertAndResolveRequest(requests[6], 'GET', MockData.recentBuildsUrl('some tester', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some tester'), buildRequestId: 5}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some tester'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[7], 'GET', MockData.recentBuildsUrl('another tester', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());

            const buildRequest = testGroup.buildRequests()[0];
            assert.equal(testGroup.buildRequests().length, 4);
            assert(!buildRequest.isBuild());
            assert(buildRequest.isTest());

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(!otherBuildRequest.isBuild());
            assert(otherBuildRequest.isTest());

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');

            return TestGroup.fetchForTask(anotherTaskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());

            const buildRequest = testGroup.buildRequests()[0];
            assert.equal(testGroup.buildRequests().length, 4);
            assert(!buildRequest.isBuild());
            assert(buildRequest.isTest());

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(!otherBuildRequest.isBuild());
            assert(otherBuildRequest.isTest());

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
        });
    });

    it('should schedule a build to build a patch', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.statusUrl(), null);
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'wk-patch': RemoteAPI.url('/api/uploaded-file/1.dat'), 'checkbox': 'build-wk', 'build-wk': true, 'build-request-id': '1', 'forcescheduler': 'force-ab-builds'}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Running');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(buildRequest.id(), 123);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Running');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(otherBuildRequest.id(), 124);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Completed');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.equal(otherWebkitRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot]);
        });
    });

    it('should schedule a build with platform in build property', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable(configWithPlatformName).then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.statusUrl(), null);
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'wk-patch': RemoteAPI.url('/api/uploaded-file/1.dat'), 'build-request-id': '1', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true, 'platform-name': 'some platform'}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Running');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(buildRequest.id(), 123);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true, 'platform-name': 'some platform'}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Running');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(otherBuildRequest.id(), 124);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Completed');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.equal(otherWebkitRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot]);
        });
    });

    it('should schedule a build to test after building a patch', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        let firstRoot = null;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert.equal(buildRequest.id(), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.buildId(), null);
            assert.deepEqual(buildRequest.commitSet().allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.equal(otherBuildRequest.id(), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.buildId(), null);
            assert.deepEqual(otherBuildRequest.commitSet().allRootFiles(), []);

            return uploadRoot(1, 45);
        }).then(() => {
            return uploadRoot(2, 46);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.notEqual(buildRequest.buildId(), null);
            const roots = buildRequest.commitSet().allRootFiles();
            assert.equal(roots.length, 1);
            firstRoot = roots[0];
            assert.deepEqual(roots[0].filename(), 'root45.dat');

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Completed');
            assert.notEqual(otherBuildRequest.buildId(), null);
            const otherRoots = otherBuildRequest.commitSet().allRootFiles();
            assert.equal(otherRoots.length, 1);
            assert.deepEqual(otherRoots[0].filename(), 'root46.dat');
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2})]
            });
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-tests', 'OK');
            assert.deepEqual(requests[6].data, {'id': '3', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'test': 'some-test', 'wk': '191622', 'build-request-id': '3','forcescheduler': 'force-ab-tests', 'roots': JSON.stringify([{url: firstRoot.url()}])}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), [
                MockData.pendingBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: 3}),
            ]);
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2})]
            });
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        });
    });

    it('should not schedule a build to test while building a patch', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert.equal(buildRequest.id(), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.statusUrl(), null);
            assert.equal(buildRequest.buildId(), null);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.equal(otherBuildRequest.id(), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return Promise.all([MockRemoteAPI.waitForRequest(), uploadRoot(1, 123)]);
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})
                ]
            });
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 9);
            assertAndResolveRequest(requests[6], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 12);
            assertAndResolveRequest(requests[9], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                     MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2, buildNumber: 1002}),
                     MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);

            const testGroup = testGroups[0];
            const buildRequest = testGroup.buildRequests()[0];
            assert.equal(buildRequest.id(), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), null);
            assert.notEqual(buildRequest.buildId(), null);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.equal(otherBuildRequest.id(), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Running');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 1002));
            assert.equal(otherBuildRequest.buildId(), null);
        });
    });

    it('should cancel builds for testing when a build to build a patch fails', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert.equal(buildRequest.id(), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.statusUrl(), null);
            assert.equal(buildRequest.buildId(), null);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.equal(otherBuildRequest.id(), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('other builder'), buildRequestId: 1, buildNumber: 312}));
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 9);
            assertAndResolveRequest(requests[6], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 12);
            assertAndResolveRequest(requests[9], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('other builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('other builder'), buildRequestId: 1, buildNumber: 312}));
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);

            const buildReqeusts = testGroups[0].buildRequests();
            assert(buildReqeusts[0].isBuild());
            assert(!buildReqeusts[0].isTest());
            assert.equal(buildReqeusts[0].statusLabel(), 'Failed');
            assert.equal(buildReqeusts[0].statusUrl(), MockData.statusUrl('other builder', 312));
            assert.equal(buildReqeusts[0].buildId(), null);

            assert(buildReqeusts[1].isBuild());
            assert(!buildReqeusts[1].isTest());
            assert.equal(buildReqeusts[1].statusLabel(), 'Failed');
            assert.equal(buildReqeusts[1].statusUrl(), null);
            assert.equal(buildReqeusts[1].buildId(), null);

            function assertTestBuildHasFailed(request)
            {
                assert(!request.isBuild());
                assert(request.isTest());
                assert.equal(request.statusLabel(), 'Failed');
                assert.equal(request.statusUrl(), null);
                assert.equal(request.buildId(), null);
            }

            assertTestBuildHasFailed(buildReqeusts[2]);
            assertTestBuildHasFailed(buildReqeusts[3]);
        });
    });

    it('should schedule a build to build binary for owned commits', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihOwnedCommit();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.statusUrl(), null);
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '1', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-6161","repository":"JavaScriptCore","ownerRevision":"191622"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Running');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(buildRequest.id(), 123).then(() => uploadRoot(buildRequest.id(), 123, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666'));
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '192736', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-9191","repository":"JavaScriptCore","ownerRevision":"192736"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Running');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(otherBuildRequest.id(), 124).then(() => uploadRoot(otherBuildRequest.id(), 124, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666'));
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Completed');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.equal(otherWebkitRoot.filename(), 'root124.dat');
            const otherJSCRoot = otherCommitSet.rootForRepository(ownedJSC);
            assert(otherJSCRoot instanceof UploadedFile);
            assert.equal(otherJSCRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot, otherJSCRoot]);
        });
    });

    it('should not update a build request to "completed" until all roots needed in the commit set are uploaded', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWihOwnedCommit();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Waiting');
            assert.equal(buildRequest.statusUrl(), null);
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '1', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-6161","repository":"JavaScriptCore","ownerRevision":"191622"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Running');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(buildRequest.id(), 123);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Running');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);
            return uploadRoot(buildRequest.id(), 123, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository((ownedJSC));
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Waiting');
            assert.equal(otherBuildRequest.statusUrl(), null);
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '192736', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-9191","repository":"JavaScriptCore","ownerRevision":"192736"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.equal(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Running');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(otherBuildRequest.id(), 124);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Running');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.equal(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.equal(otherWebkitRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot]);
            return uploadRoot(otherBuildRequest.id(), 124, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.equal(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.equal(buildRequest.statusLabel(), 'Completed');
            assert.equal(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.equal(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.equal(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.equal(otherBuildRequest.statusLabel(), 'Completed');
            assert.equal(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.equal(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.equal(otherWebkitRoot.filename(), 'root124.dat');
            const otherJSCRoot = otherCommitSet.rootForRepository(ownedJSC);
            assert(otherJSCRoot instanceof UploadedFile);
            assert.equal(otherJSCRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot, otherJSCRoot]);
        });
    });
});
