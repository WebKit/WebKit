'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const BuildbotTriggerable = require('../tools/js/buildbot-triggerable.js').BuildbotTriggerable;
const MockData = require('./resources/mock-data.js');
const MockLogger = require('./resources/mock-logger.js').MockLogger;
const MockRemoteAPI = require('../unit-tests/resources/mock-remote-api.js').MockRemoteAPI;
const TestServer = require('./resources/test-server.js');
const {TestParameter} = require("../public/v3/models/test-parameter");
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
const addWorkerForReport = require('./resources/common-operations.js').addWorkerForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;

function configWithOneTesterTwoBuilders(testConfigurationsOverride = [{types: ['some'], platforms: ['some platform'], builders: ['builder-1'],
    supportedRepetitionTypes: ['alternating', 'sequential']}])
{
    return {
        triggerableName: 'build-webkit',
        lookbackCount: 2,
        buildRequestArgument: 'build-request-id',
        testParametersArgument: 'test-parameters',
        workerName: 'sync-worker',
        workerPassword: 'password',
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
                supportedRepetitionTypes: ['alternating', 'sequential']
            },
            'builder-2': {
                builder: 'some builder',
                properties: {forcescheduler: 'force-ab-builds'},
                supportedRepetitionTypes: ['alternating', 'sequential', 'paired-parallel']
            },
            'builder-3': {
                builder: 'other builder',
                properties: {forcescheduler: 'force-ab-builds'},
                supportedRepetitionTypes: ['alternating', 'sequential', 'paired-parallel']
            },
        },
        buildConfigurations: [
            {
                platforms: ['some platform'],
                builders: ['builder-2', 'builder-3'],
                supportedRepetitionTypes: ['alternating', 'sequential', 'paired-parallel']
            },
        ],
        testConfigurations: testConfigurationsOverride
    };
}

const configWithPlatformName = {
    triggerableName: 'build-webkit',
    lookbackCount: 2,
    buildRequestArgument: 'build-request-id',
    workerName: 'sync-worker',
    workerPassword: 'password',
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
            supportedRepetitionTypes: ['alternating', 'sequential']
        },
        'builder-2': {
            builder: 'some builder',
            properties: {forcescheduler: 'force-ab-builds'},
            supportedRepetitionTypes: ['alternating', 'sequential', 'paired-parallel']
        },
        'builder-3': {
            builder: 'other builder',
            properties: {forcescheduler: 'force-ab-builds'},
            supportedRepetitionTypes: ['alternating', 'sequential', 'paired-parallel']
        },
    },
    buildConfigurations: [
        {
            platforms: ['some platform'],
            builders: ['builder-2', 'builder-3'],
            supportedRepetitionTypes: ['alternating', 'sequential', 'paired-parallel'],
        },
    ],
    testConfigurations: [
        {
            types: ['some'],
            platforms: ['some platform'],
            builders: ['builder-1'],
            supportedRepetitionTypes: ['alternating', 'sequential'],
        },
    ],
};

const configWithTwoTesters = {
    triggerableName: 'build-webkit',
    lookbackCount: 2,
    buildRequestArgument: 'build-request-id',
    workerName: 'sync-worker',
    workerPassword: 'password',
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
            supportedRepetitionTypes: ['alternating', 'sequential']
        },
        'builder-2': {
            builder: 'another tester',
            properties: {forcescheduler: 'force-ab-builds'},
            supportedRepetitionTypes: ['alternating', 'sequential']
        },
    },
    testConfigurations: [
        {
            types: ['some'],
            platforms: ['some platform'],
            builders: ['builder-1', 'builder-2'],
            supportedRepetitionTypes: ['alternating', 'sequential'],
        },
    ]
};

function assertAndResolveRequest(request, method, url, contentToResolve)
{
    assert.strictEqual(request.method, method);
    assert.strictEqual(request.url, url);
    request.resolve(contentToResolve);
}

function createTriggerable(config = configWithOneTesterTwoBuilders())
{
    let triggerable;
    return MockData.addMockConfiguration(TestServer.database()).then(() => {
        return Manifest.fetch();
    }).then(() => {
        triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, {name: 'sync-worker', password: 'password'}, new MockLogger);
        const syncPromise = triggerable.initSyncers().then(() => triggerable.updateTriggerable());
        assertAndResolveRequest(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
        MockRemoteAPI.reset();
        return syncPromise;
    }).then(() => Manifest.fetch()).then(() => {
        return new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, {name: 'sync-worker', password: 'password'}, new MockLogger);
    });
}

function createTestGroup(task_name='custom task') {
    const someTest = Test.findById(MockData.someTestId());
    const webkit = Repository.findById(MockData.webkitRepositoryId());
    const set1 = new CustomCommitSet;
    set1.setRevisionForRepository(webkit, '191622');
    const set2 = new CustomCommitSet;
    set2.setRevisionForRepository(webkit, '192736');
    return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, 'alternating', [set1, set2]).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    });
}

async function createTestGroupWithPatch(repetitionType = 'alternating')
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
    const task = await TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, repetitionType, [set1, set2]);

    return TestGroup.findAllByTask(task.id())[0];
}

async function createTestGroupWithTestParameters(testParameters=null)
{
    const someTest = Test.findById(MockData.someTestId());
    const webkit = Repository.findById(MockData.webkitRepositoryId());
    const set1 = new CustomCommitSet;
    set1.setRevisionForRepository(webkit, '191622');
    const set2 = new CustomCommitSet;
    set2.setRevisionForRepository(webkit, '192736');
    if (!testParameters) {
        const diagnoseParameter = TestParameter.findByName('diagnose');
        const parameterSet = new CustomTestParameterSet;
        parameterSet.set(diagnoseParameter, {value: true});
        testParameters = [parameterSet, parameterSet];
    }

    const task = await TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, 'alternating', [set1, set2], testParameters);

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
    return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, 'alternating', [set1, set2]).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    });
}

function uploadRoot(buildRequestId, buildTag, repositoryList = ["WebKit"], buildTime = '2017-05-10T02:54:08.666')
{
    return TemporaryFile.makeTemporaryFile(`root${buildTag}.dat`, `root for build ${buildTag} and repository list at ${buildTime}`).then((rootFile) => {
        return TestServer.remoteAPI().postFormData('/api/upload-root/', {
            workerName: 'sync-worker',
            workerPassword: 'password',
            builderName: 'some builder',
            buildTag: buildTag,
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

    it('should be able to schedule a "paired-parallel" build request for building a patch on buildbot', async () => {
        let syncPromise;
        const triggerable = await createTriggerable(configWithOneTesterTwoBuilders([
            { builders: ['builder-1'], types: ['some'], platforms: ['some platform'], supportedRepetitionTypes: ['alternating', 'paired-parallel'], testParameters: ['diagnose']}]));
        const firstBuildNumber = 123;
        const secondBuildNumber = 124;
        let testGroup = await createTestGroupWithTestParameters();
        assert.strictEqual(testGroup.buildRequests().length, 4);

        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        await resolveSyncerToBuildBotRequests([
            [
                { method: 'GET', url: MockData.buildbotBuildersURL(), resolve: MockData.mockBuildbotBuilders() },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: MockData.finishedBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: 1, buildTag: firstBuildNumber}) },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ],
            [
                {
                    method: 'POST', url: '/api/v2/forceschedulers/force-ab-tests', resolve: 'OK',
                    data: {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params': {'wk': '192736', 'build-request-id': '2', 'forcescheduler': 'force-ab-tests', 'test': 'some-test', 'test-parameters': JSON.stringify({diagnose: {value: true}})}}
                },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {
                        builds: [
                            MockData.runningBuildData({builderId: MockData.builderIDForName('some tester'), buildRequestId: 2, buildTag: secondBuildNumber}),
                            MockData.finishedBuildData({builderId: MockData.builderIDForName('some tester'), buildRequestId: 1, buildTag: firstBuildNumber}),
                        ]} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ]
        ]);
        await syncPromise;
    });

    it('should not schedule on another builder if the build was scheduled on one builder before', () => {
        const requests = MockRemoteAPI.requests;
        let firstTestGroup;
        let firstTestGroupFirstBuildRequest;
        let secondTestGroup;
        let secondTestGroupFirstBuildRequest;
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
            firstTestGroupFirstBuildRequest = firstTestGroup.buildRequests()[0].id();
            secondTestGroupFirstBuildRequest = secondTestGroup.buildRequests()[0].id();
            taskId = firstTestGroup.task().id();
            anotherTaskId = secondTestGroup.task().id();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 2);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), MockData.pendingBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: secondTestGroupFirstBuildRequest}));
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('another tester'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 4);
            assertAndResolveRequest(requests[2], 'GET', MockData.recentBuildsUrl('some tester', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: firstTestGroupFirstBuildRequest}));
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('another tester', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[4], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.pendingBuildsUrl('another tester'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 8);
            assertAndResolveRequest(requests[6], 'GET', MockData.recentBuildsUrl('some tester', 2), {
                'builds': [
                    MockData.runningBuildData({builderId: MockData.builderIDForName('some tester'), buildRequestId: secondTestGroupFirstBuildRequest}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some tester'), buildRequestId: firstTestGroupFirstBuildRequest})]
            });
            assertAndResolveRequest(requests[7], 'GET', MockData.recentBuildsUrl('another tester', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());

            const buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 4);
            assert(!buildRequest.isBuild());
            assert(buildRequest.isTest());

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(!otherBuildRequest.isBuild());
            assert(otherBuildRequest.isTest());

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');

            return TestGroup.fetchForTask(anotherTaskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());

            const buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 4);
            assert(!buildRequest.isBuild());
            assert(buildRequest.isTest());

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(!otherBuildRequest.isBuild());
            assert(otherBuildRequest.isTest());

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
        });
    });

    it('should keep updating build status', async () => {
        const requests = MockRemoteAPI.requests;
        let syncPromise;
        const triggerable = await createTriggerable();
        const firstBuildNumber = 123;
        const secondBuildNumber = 124;
        let testGroup = await createTestGroupWithPatch();

        const taskId = testGroup.task().id();
        let webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        let buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
        assert.strictEqual(buildRequest.statusUrl(), null);
        assert.strictEqual(buildRequest.statusDescription(), null);
        assert.strictEqual(buildRequest.buildId(), null);

        let commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        let webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        assert.strictEqual(commitSet.rootForRepository(webkit), null);
        assert.deepEqual(commitSet.allRootFiles(), []);

        let otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
        assert.strictEqual(otherBuildRequest.statusUrl(), null);
        assert.strictEqual(otherBuildRequest.statusDescription(), null);
        assert.strictEqual(otherBuildRequest.buildId(), null);

        let otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.deepEqual(otherCommitSet.allRootFiles(), []);

        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
        MockRemoteAPI.reset();
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 3);
        assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
        assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
        assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 6);
        assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
        assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
        assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 7);
        assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
        assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'wk-patch': RemoteAPI.url('/api/uploaded-file/1.dat'), 'checkbox': 'build-wk', 'build-wk': true, 'build-request-id': '1', 'forcescheduler': 'force-ab-builds'}});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 10);
        assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
        assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
            MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
        assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 13);
        assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
        assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
            MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildNumber: firstBuildNumber, statusDescription: 'Building WebKit'}));
        assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
        await syncPromise;

        let testGroups = await TestGroup.fetchForTask(taskId, true);

        assert.strictEqual(testGroups.length, 1);
        testGroup = testGroups[0];
        webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Running');
        assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
        assert.strictEqual(buildRequest.statusDescription(), 'Building WebKit');
        assert.strictEqual(buildRequest.buildId(), null);

        commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        assert.strictEqual(commitSet.rootForRepository(webkit), null);
        assert.deepEqual(commitSet.allRootFiles(), []);

        otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
        assert.strictEqual(otherBuildRequest.statusUrl(), null);
        assert.strictEqual(otherBuildRequest.statusDescription(), null);
        assert.strictEqual(otherBuildRequest.buildId(), null);

        otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.deepEqual(otherCommitSet.allRootFiles(), []);

        MockRemoteAPI.reset();
        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
        MockRemoteAPI.reset();
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 3);
        assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
        assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
        assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 6);
        assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
        assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
            MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber, statusDescription: 'Compiling WTF'}));
        assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 9);
        assertAndResolveRequest(requests[6], 'GET', MockData.pendingBuildsUrl('some tester'), {});
        assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some builder'), {});
        assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('other builder'), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 12);
        assertAndResolveRequest(requests[9], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
        assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some builder', 2),
            MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber, statusDescription: 'Compiling WTF'}));
        assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
        await syncPromise;

        await TestGroup.fetchForTask(taskId, true);

        assert.strictEqual(testGroups.length, 1);
        testGroup = testGroups[0];
        webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Running');
        assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', firstBuildNumber));
        assert.strictEqual(buildRequest.statusDescription(), 'Compiling WTF');
        assert.strictEqual(buildRequest.buildId(), null);

        commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        assert.strictEqual(commitSet.rootForRepository(webkit), null);
        assert.deepEqual(commitSet.allRootFiles(), []);

        otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
        assert.strictEqual(otherBuildRequest.statusUrl(), null);
        assert.strictEqual(otherBuildRequest.statusDescription(), null);
        assert.strictEqual(otherBuildRequest.buildId(), null);

        otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.deepEqual(otherCommitSet.allRootFiles(), []);

        await uploadRoot(parseInt(buildRequest.id()), firstBuildNumber);

        testGroups = await TestGroup.fetchForTask(taskId, true);

        assert.strictEqual(testGroups.length, 1);
        testGroup = testGroups[0];
        webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Completed');
        assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', firstBuildNumber));
        assert.strictEqual(buildRequest.statusDescription(), 'Compiling WTF');
        assert.notEqual(buildRequest.buildId(), null);

        commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        let webkitRoot = commitSet.rootForRepository(webkit);
        assert(webkitRoot instanceof UploadedFile);
        assert.strictEqual(webkitRoot.filename(), 'root123.dat');
        assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

        otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
        assert.strictEqual(otherBuildRequest.statusUrl(), null);
        assert.strictEqual(otherBuildRequest.statusDescription(), null);
        assert.strictEqual(otherBuildRequest.buildId(), null);

        otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.deepEqual(otherCommitSet.allRootFiles(), []);

        MockRemoteAPI.reset();
        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
        MockRemoteAPI.reset();
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 3);
        assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
        assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
        assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 6);
        assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
        assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
            MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber}));
        assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 7);
        assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
        assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true}});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 10);
        assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
        assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
        assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
        await MockRemoteAPI.waitForRequest();

        assert.strictEqual(requests.length, 13);
        assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
        assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2), {
            'builds': [
                MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2, buildTag: secondBuildNumber}),
                MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber})]
        });
        assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
        await syncPromise;

        await TestGroup.fetchForTask(taskId, true);

        assert.strictEqual(testGroups.length, 1);
        testGroup = testGroups[0];
        webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Completed');
        assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', firstBuildNumber));
        assert.strictEqual(buildRequest.statusDescription(), null);
        assert.notEqual(buildRequest.buildId(), null);

        commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        webkitRoot = commitSet.rootForRepository(webkit);
        assert(webkitRoot instanceof UploadedFile);
        assert.strictEqual(webkitRoot.filename(), 'root123.dat');
        assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

        otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
        assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', secondBuildNumber));
        assert.strictEqual(otherBuildRequest.statusDescription(), null);
        assert.strictEqual(otherBuildRequest.buildId(), null);

        otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.deepEqual(otherCommitSet.allRootFiles(), []);
        await uploadRoot(parseInt(otherBuildRequest.id()), 124);

        testGroups = await TestGroup.fetchForTask(taskId, true);

        assert.strictEqual(testGroups.length, 1);
        testGroup = testGroups[0];
        webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Completed');
        assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', firstBuildNumber));
        assert.strictEqual(buildRequest.statusDescription(), null);
        assert.notEqual(buildRequest.buildId(), null);

        commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        webkitRoot = commitSet.rootForRepository(webkit);
        assert(webkitRoot instanceof UploadedFile);
        assert.strictEqual(webkitRoot.filename(), 'root123.dat');
        assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

        otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), 'Completed');
        assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', secondBuildNumber));
        assert.strictEqual(otherBuildRequest.statusDescription(), null);
        assert.notEqual(otherBuildRequest.buildId(), null);

        otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
        assert(otherWebkitRoot instanceof UploadedFile);
        assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
        assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot]);
    });

    async function resolveSyncerToBuildBotRequests(requestResolutionList)
    {
        const requests = MockRemoteAPI.requests;
        let resolutionIndexOffset = 0;
        for (let i = 0; i < requestResolutionList.length; i++) {
            const resolutions = requestResolutionList[i];
            assert.strictEqual(requests.length, resolutionIndexOffset + resolutions.length);
            resolutions.forEach((resolution, index) => {
                assertAndResolveRequest(requests[resolutionIndexOffset + index], resolution.method, resolution.url, resolution.resolve);
                if ('data' in resolution)
                    assert.deepEqual(requests[resolutionIndexOffset + index].data, resolution.data);
            });
            resolutionIndexOffset += resolutions.length;
            if (i != requestResolutionList.length - 1)
                await MockRemoteAPI.waitForRequest();
        }
        MockRemoteAPI.reset();
    }

    function validateFirstTwoBuildRequestsInTestGroup(testGroup, buildRequestOverride = {}, otherBuildRequestOverride = {})
    {
        const webkit = Repository.findById(MockData.webkitRepositoryId());
        assert.strictEqual(testGroup.buildRequests().length, 6);

        const buildRequest = testGroup.buildRequests()[0];
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), buildRequestOverride.statusLabel || 'Waiting');
        assert.strictEqual(buildRequest.statusUrl(), buildRequestOverride.statusUrl || null);
        assert.strictEqual(buildRequest.statusDescription(), buildRequestOverride.statusDescription || null);
        assert.strictEqual(buildRequest.buildId(), buildRequestOverride.buildId || null);

        const commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        const webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        if (!buildRequestOverride.webkitRootUploaded) {
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);
        }
        else {
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);
        }

        const otherBuildRequest = testGroup.buildRequests()[1];
        assert(otherBuildRequest.isBuild());
        assert(!otherBuildRequest.isTest());
        assert.strictEqual(otherBuildRequest.statusLabel(), otherBuildRequestOverride.statusLabel || 'Waiting');
        assert.strictEqual(otherBuildRequest.statusUrl(), otherBuildRequestOverride.statusUrl || null);
        assert.strictEqual(otherBuildRequest.statusDescription(), otherBuildRequestOverride.statusDescription || null);
        assert.strictEqual(otherBuildRequest.buildId(), otherBuildRequestOverride.buildId || null);

        const otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);

        if (!otherBuildRequestOverride.webkitRootUploaded) {
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);
        }
        else {
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot]);
        }
    }

    it('should be able to schedule a "paired-parallel" build request for building a patch on buildbot', async () => {
        let syncPromise;
        const triggerable = await createTriggerable(configWithOneTesterTwoBuilders([
            { builders: ['builder-1'], types: ['some'], platforms: ['some platform'], supportedRepetitionTypes: ['alternating', 'paired-parallel'] }]));
        const firstBuildNumber = 123;
        const secondBuildNumber = 124;
        let testGroup = await createTestGroupWithPatch('paired-parallel');

        const taskId = testGroup.task().id();
        assert.strictEqual(testGroup.buildRequests().length, 6);
        validateFirstTwoBuildRequestsInTestGroup(testGroup);

        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        await resolveSyncerToBuildBotRequests([
            [
                { method: 'GET', url: MockData.buildbotBuildersURL(), resolve: MockData.mockBuildbotBuilders() },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber, statusDescription: 'Compiling WTF'}) },
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber, statusDescription: 'Compiling WTF'}) },
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ]
        ]);
        await syncPromise;

        let testGroups = await TestGroup.fetchForTask(taskId, true);
        assert.strictEqual(testGroups.length, 1);
        assert.strictEqual(testGroups[0].buildRequests().length, 6);
        validateFirstTwoBuildRequestsInTestGroup(testGroups[0], {statusLabel: 'Running', statusUrl: MockData.statusUrl('some builder', firstBuildNumber), statusDescription: 'Compiling WTF'});

        await uploadRoot(parseInt(testGroups[0].buildRequests()[0].id()), firstBuildNumber);
        testGroups = await TestGroup.fetchForTask(taskId, true);
        assert.strictEqual(testGroups.length, 1);
        validateFirstTwoBuildRequestsInTestGroup(testGroups[0], {statusLabel: 'Completed', statusUrl: MockData.statusUrl('some builder', firstBuildNumber), statusDescription: 'Compiling WTF', buildId: '1', webkitRootUploaded: true});

        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        await resolveSyncerToBuildBotRequests([
            [
                { method: 'GET', url: MockData.buildbotBuildersURL(), resolve: MockData.mockBuildbotBuilders() },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: '1', buildTag: firstBuildNumber}) },
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ],
            [
                {
                    method: 'POST', url: '/api/v2/forceschedulers/force-ab-builds', resolve: 'OK',
                    data: {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params': {'wk': '191622', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true}}
                },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: {
                    builds: [
                        MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2, buildTag: secondBuildNumber}),
                        MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber})
                    ]} },
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ]
        ]);
        await syncPromise;

        await TestGroup.fetchForTask(taskId, true);
        assert.strictEqual(testGroups.length, 1);
        validateFirstTwoBuildRequestsInTestGroup(testGroups[0], {statusLabel: 'Completed', statusUrl: MockData.statusUrl('some builder', firstBuildNumber), buildId: '1', webkitRootUploaded: true},
            {statusLabel: 'Running', statusUrl: MockData.statusUrl('some builder', secondBuildNumber)});

        await uploadRoot(parseInt(testGroups[0].buildRequests()[1].id()), 124);
        testGroups = await TestGroup.fetchForTask(taskId, true);
        assert.strictEqual(testGroups.length, 1);
        validateFirstTwoBuildRequestsInTestGroup(testGroups[0], {statusLabel: 'Completed', statusUrl: MockData.statusUrl('some builder', firstBuildNumber), buildId: '1', webkitRootUploaded: true},
            {statusLabel: 'Completed', statusUrl: MockData.statusUrl('some builder', secondBuildNumber), buildId: '2', webkitRootUploaded: true});

        syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
        await resolveSyncerToBuildBotRequests([
            [
                { method: 'GET', url: MockData.buildbotBuildersURL(), resolve: MockData.mockBuildbotBuilders() },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: { builds: [
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2, buildTag: secondBuildNumber}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber})]}},
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.pendingBuildsUrl('some tester'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('some builder'), resolve: {} },
                { method: 'GET', url: MockData.pendingBuildsUrl('other builder'), resolve: {} },
            ],
            [
                { method: 'GET', url: MockData.recentBuildsUrl('some tester', 2), resolve: {} },
                { method: 'GET', url: MockData.recentBuildsUrl('some builder', 2), resolve: { builds: [
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2, buildTag: secondBuildNumber}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1, buildTag: firstBuildNumber})]}},
                { method: 'GET', url: MockData.recentBuildsUrl('other builder', 2), resolve: {} },
            ]
        ]);
        await syncPromise;

        await TestGroup.fetchForTask(taskId, true);
        assert.strictEqual(testGroups.length, 1);
        assert.strictEqual(testGroups[0].buildRequests().length, 6);

        const buildRequest = testGroups[0].buildRequests()[2];
        assert(buildRequest.isTest());
        assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
    });

    it('should schedule a build to build a patch', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWithPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.statusUrl(), null);
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'wk-patch': RemoteAPI.url('/api/uploaded-file/1.dat'), 'checkbox': 'build-wk', 'build-wk': true, 'build-request-id': '1', 'forcescheduler': 'force-ab-builds'}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Running');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(buildRequest.id()), 123);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
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
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(otherBuildRequest.id()), 124);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Completed');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
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
            return createTestGroupWithPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.statusUrl(), null);
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'wk-patch': RemoteAPI.url('/api/uploaded-file/1.dat'), 'build-request-id': '1', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true, 'platform-name': 'some platform'}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Running');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(buildRequest.id()), 123);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'checkbox': 'build-wk', 'build-wk': true, 'platform-name': 'some platform'}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
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
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(otherBuildRequest.id()), 124);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            const webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Completed');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
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
            return createTestGroupWithPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(parseInt(buildRequest.id()), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.buildId(), null);
            assert.deepEqual(buildRequest.commitSet().allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.strictEqual(parseInt(otherBuildRequest.id()), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.buildId(), null);
            assert.deepEqual(otherBuildRequest.commitSet().allRootFiles(), []);

            return uploadRoot(1, 45);
        }).then(() => {
            return uploadRoot(2, 46);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.notEqual(buildRequest.buildId(), null);
            const roots = buildRequest.commitSet().allRootFiles();
            assert.strictEqual(roots.length, 1);
            firstRoot = roots[0];
            assert.deepEqual(roots[0].filename(), 'root45.dat');

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Completed');
            assert.notEqual(otherBuildRequest.buildId(), null);
            const otherRoots = otherBuildRequest.commitSet().allRootFiles();
            assert.strictEqual(otherRoots.length, 1);
            assert.deepEqual(otherRoots[0].filename(), 'root46.dat');
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}),
                    MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2})]
            });
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-tests', 'OK');
            assert.deepEqual(requests[6].data, {'id': '3', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'test': 'some-test', 'wk': '191622', 'build-request-id': '3','forcescheduler': 'force-ab-tests', 'roots': JSON.stringify([{url: firstRoot.url()}])}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), [
                MockData.pendingBuild({builderId: MockData.builderIDForName('some tester'), buildRequestId: 3}),
            ]);
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
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
            return createTestGroupWithPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(parseInt(buildRequest.id()), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.statusUrl(), null);
            assert.strictEqual(buildRequest.buildId(), null);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.strictEqual(parseInt(otherBuildRequest.id()), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return Promise.all([MockRemoteAPI.waitForRequest(), uploadRoot(1, 123)]);
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
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
            assert.strictEqual(requests.length, 9);
            assertAndResolveRequest(requests[6], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 12);
            assertAndResolveRequest(requests[9], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some builder', 2), {
                'builds': [
                     MockData.runningBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 2, buildTag: 1002}),
                     MockData.finishedBuildData({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1})]
            });
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);

            const testGroup = testGroups[0];
            const buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(parseInt(buildRequest.id()), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/#/builders/1/builds/123');
            assert.notEqual(buildRequest.buildId(), null);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.strictEqual(parseInt(otherBuildRequest.id()), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 1002));
            assert.strictEqual(otherBuildRequest.buildId(), null);
        });
    });

    it('should cancel builds for testing when a build to build a patch fails', () => {
        const requests = MockRemoteAPI.requests;
        let triggerable;
        let taskId = null;
        let syncPromise;
        return createTriggerable().then((newTriggerable) => {
            triggerable = newTriggerable;
            return createTestGroupWithPatch();
        }).then((testGroup) => {
            taskId = testGroup.task().id();
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(parseInt(buildRequest.id()), 1);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.statusUrl(), null);
            assert.strictEqual(buildRequest.buildId(), null);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert.strictEqual(parseInt(otherBuildRequest.id()), 2);
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('other builder'), buildRequestId: 1, buildTag: 312}));
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 9);
            assertAndResolveRequest(requests[6], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 12);
            assertAndResolveRequest(requests[9], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('other builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('other builder'), buildRequestId: 1, buildTag: 312}));
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);

            const buildReqeusts = testGroups[0].buildRequests();
            assert(buildReqeusts[0].isBuild());
            assert(!buildReqeusts[0].isTest());
            assert.strictEqual(buildReqeusts[0].statusLabel(), 'Failed');
            assert.strictEqual(buildReqeusts[0].statusUrl(), MockData.statusUrl('other builder', 312));
            assert.strictEqual(buildReqeusts[0].buildId(), null);

            assert(buildReqeusts[1].isBuild());
            assert(!buildReqeusts[1].isTest());
            assert.strictEqual(buildReqeusts[1].statusLabel(), 'Failed');
            assert.strictEqual(buildReqeusts[1].statusUrl(), null);
            assert.strictEqual(buildReqeusts[1].buildId(), null);

            function assertTestBuildHasFailed(request)
            {
                assert(!request.isBuild());
                assert(request.isTest());
                assert.strictEqual(request.statusLabel(), 'Failed');
                assert.strictEqual(request.statusUrl(), null);
                assert.strictEqual(request.buildId(), null);
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
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.statusUrl(), null);
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '1', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-6161","repository":"JavaScriptCore","ownerRevision":"191622"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Running');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(buildRequest.id()), 123).then(() => uploadRoot(parseInt(buildRequest.id()), 123, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666'));
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '192736', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-9191","repository":"JavaScriptCore","ownerRevision":"192736"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
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
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(otherBuildRequest.id()), 124).then(() => uploadRoot(parseInt(otherBuildRequest.id()), 124, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666'));
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Completed');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
            const otherJSCRoot = otherCommitSet.rootForRepository(ownedJSC);
            assert(otherJSCRoot instanceof UploadedFile);
            assert.strictEqual(otherJSCRoot.filename(), 'root124.dat');
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
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(buildRequest.statusUrl(), null);
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2), {});
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '1', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'build-request-id': '1', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-6161","repository":"JavaScriptCore","ownerRevision":"191622"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'),
                MockData.pendingBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
            assertAndResolveRequest(requests[10], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[11], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.runningBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[12], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return syncPromise;
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Running');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            assert.deepEqual(commitSet.allRootFiles(), []);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(buildRequest.id()), 123);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Running');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);
            return uploadRoot(parseInt(buildRequest.id()), 123, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository((ownedJSC));
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Waiting');
            assert.strictEqual(otherBuildRequest.statusUrl(), null);
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            MockRemoteAPI.reset();
            syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertAndResolveRequest(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 3);
            assertAndResolveRequest(requests[0], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[1], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[2], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 6);
            assertAndResolveRequest(requests[3], 'GET', MockData.recentBuildsUrl('some tester', 2), {});
            assertAndResolveRequest(requests[4], 'GET', MockData.recentBuildsUrl('some builder', 2),
                MockData.finishedBuild({builderId: MockData.builderIDForName('some builder'), buildRequestId: 1}));
            assertAndResolveRequest(requests[5], 'GET', MockData.recentBuildsUrl('other builder', 2), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 7);
            assertAndResolveRequest(requests[6], 'POST', '/api/v2/forceschedulers/force-ab-builds', 'OK');
            assert.deepEqual(requests[6].data, {'id': '2', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '192736', 'build-request-id': '2', 'forcescheduler': 'force-ab-builds', 'owned-commits': `{"WebKit":[{"revision":"owned-jsc-9191","repository":"JavaScriptCore","ownerRevision":"192736"}]}`}});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 10);
            assertAndResolveRequest(requests[7], 'GET', MockData.pendingBuildsUrl('some tester'), {});
            assertAndResolveRequest(requests[8], 'GET', MockData.pendingBuildsUrl('some builder'), {});
            assertAndResolveRequest(requests[9], 'GET', MockData.pendingBuildsUrl('other builder'), {});
            return MockRemoteAPI.waitForRequest();
        }).then(() => {
            assert.strictEqual(requests.length, 13);
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
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return uploadRoot(parseInt(otherBuildRequest.id()), 124);
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Running');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.strictEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot]);
            return uploadRoot(parseInt(otherBuildRequest.id()), 124, [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
        }).then(() => {
            return TestGroup.fetchForTask(taskId, true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const testGroup = testGroups[0];
            const webkit = Repository.findById(MockData.webkitRepositoryId());
            const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const buildRequest = testGroup.buildRequests()[0];
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert.strictEqual(buildRequest.statusLabel(), 'Completed');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(commitSet.ownerRevisionForRepository(ownedJSC), commitSet.revisionForRepository(webkit));
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert(webkitRoot instanceof UploadedFile);
            assert.strictEqual(webkitRoot.filename(), 'root123.dat');
            const jscRoot = commitSet.rootForRepository(ownedJSC);
            assert(jscRoot instanceof UploadedFile);
            assert.strictEqual(jscRoot.filename(), 'root123.dat');
            assert.deepEqual(commitSet.allRootFiles(), [webkitRoot, jscRoot]);

            const otherBuildRequest = testGroup.buildRequests()[1];
            assert(otherBuildRequest.isBuild());
            assert(!otherBuildRequest.isTest());
            assert.strictEqual(otherBuildRequest.statusLabel(), 'Completed');
            assert.strictEqual(otherBuildRequest.statusUrl(), MockData.statusUrl('some builder', 124));
            assert.notEqual(otherBuildRequest.buildId(), null);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.ownerRevisionForRepository(ownedJSC), otherCommitSet.revisionForRepository(webkit));
            const otherWebkitRoot = otherCommitSet.rootForRepository(webkit);
            assert(otherWebkitRoot instanceof UploadedFile);
            assert.strictEqual(otherWebkitRoot.filename(), 'root124.dat');
            const otherJSCRoot = otherCommitSet.rootForRepository(ownedJSC);
            assert(otherJSCRoot instanceof UploadedFile);
            assert.strictEqual(otherJSCRoot.filename(), 'root124.dat');
            assert.deepEqual(otherCommitSet.allRootFiles(), [otherWebkitRoot, otherJSCRoot]);
        });
    });
});
