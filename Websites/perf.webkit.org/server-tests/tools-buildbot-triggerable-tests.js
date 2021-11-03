'use strict';

const assert = require('assert');

const BuildbotTriggerable = require('../tools/js/buildbot-triggerable.js').BuildbotTriggerable;
const MockData = require('./resources/mock-data.js');
const MockRemoteAPI = require('../unit-tests/resources/mock-remote-api.js').MockRemoteAPI;
const TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const MockLogger = require('./resources/mock-logger.js').MockLogger;

function assertRequestAndResolve(request, method, url, content)
{
    assert.strictEqual(request.method, method);
    assert.strictEqual(request.url, url);
    request.resolve(content);
}

describe('BuildbotTriggerable', function () {
    prepareServerTest(this, 'node');

    beforeEach(function () {
        MockData.resetV3Models();
        MockRemoteAPI.reset('http://build.webkit.org');
    });

    describe('syncOnce', () => {
        it('should schedule the next build request when there are no pending builds', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['completed', 'running', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'completed');
                assert.strictEqual(BuildRequest.findById(701).status(), 'running');
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData(), MockData.finishedBuildData()]});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[2].url, '/api/v2/forceschedulers/force-some-builder-1');
                assert.deepStrictEqual(MockRemoteAPI.requests[2].data, {'id': '702', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '702', 'forcescheduler': 'force-some-builder-1'}});
                MockRemoteAPI.requests[2].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[3].resolve(MockData.pendingBuild())
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[4].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[4].resolve({'builds': [MockData.runningBuildData(), MockData.finishedBuildData()]});
                return syncPromise;
            }).then(() => {
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithSingleBuilder().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'completed');
                assert.strictEqual(BuildRequest.findById(701).status(), 'running');
                assert.strictEqual(BuildRequest.findById(702).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
            });
        });

        it('should not schedule the next build request when there is a pending build', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['completed', 'running', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let workerInfo = {name: 'sync-worker', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve(MockData.pendingBuild());
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[1].resolve({'builds' : [MockData.runningBuildData(), MockData.finishedBuildData()]});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[2].resolve(MockData.pendingBuild())
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[3].resolve({'builds' : [MockData.runningBuildData(), MockData.finishedBuildData()]});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'completed');
                assert.strictEqual(BuildRequest.findById(701).status(), 'running');
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithSingleBuilder().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'completed');
                assert.strictEqual(BuildRequest.findById(701).status(), 'running');
                assert.strictEqual(BuildRequest.findById(702).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
            });
        });

        it('should schedule the build request on a builder without a pending build if it\'s the first request in the group', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithTwoBuilders();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve(MockData.pendingBuild({buildRequestId: 999}));
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[2].resolve({});
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 5);
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[4].url, '/api/v2/forceschedulers/force-some-builder-2');
                assert.deepStrictEqual(MockRemoteAPI.requests[4].data, {'id': '700', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '700', 'forcescheduler': 'force-some-builder-2'}});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 7);
                assert.strictEqual(MockRemoteAPI.requests[5].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[5].resolve(MockData.pendingBuild({buildRequestId: 999}));
                assert.strictEqual(MockRemoteAPI.requests[6].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[6].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[6].resolve(MockData.pendingBuild({builderId: MockData.builderIDForName('some builder 2'), buildRequestId: 700, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 9);
                assert.strictEqual(MockRemoteAPI.requests[7].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[7].resolve({});
                assert.strictEqual(MockRemoteAPI.requests[8].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[8].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[8].resolve({});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
            });
        });

        it('should not schedule the first build request on a builder whose last build failed', async () => {
            const db = TestServer.database();
            await MockData.addMockData(db);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithSingleBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 1);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds' : [MockData.finishedBuildData({results: 1, buildRequestId: 699})]});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[2].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'pending');
            assert.equal(BuildRequest.findById(700).statusUrl(), null);
        });

        it('should schedule the first build request to the first available non-failing queue', async () => {
            const db = TestServer.database();
            await MockData.addMockData(db);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithTwoBuilders();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncOnce = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[1].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 4);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[2].resolve({'builds' : [MockData.finishedBuildData({results: 1, buildRequestId: 699})]});
            assert.equal(MockRemoteAPI.requests[3].method, 'GET');
            assert.equal(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[3].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 5);
            assert.equal(MockRemoteAPI.requests[4].method, 'POST');
            assert.equal(MockRemoteAPI.requests[4].url, '/api/v2/forceschedulers/force-some-builder-2');
            assert.deepEqual(MockRemoteAPI.requests[4].data, {'id': '700', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '700', 'forcescheduler': 'force-some-builder-2'}});
            MockRemoteAPI.requests[4].resolve('OK');

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 7);
            assert.equal(MockRemoteAPI.requests[5].method, 'GET');
            assert.equal(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[5].resolve({});
            assert.equal(MockRemoteAPI.requests[6].method, 'GET');
            assert.equal(MockRemoteAPI.requests[6].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[6].resolve(MockData.pendingBuild({builderId: MockData.builderIDForName('some builder 2'), buildRequestId: 700, buildbotBuildRequestId: 17}));

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 9);
            assert.equal(MockRemoteAPI.requests[7].method, 'GET');
            assert.equal(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[7].resolve({});
            assert.equal(MockRemoteAPI.requests[8].method, 'GET');
            assert.equal(MockRemoteAPI.requests[8].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[8].resolve({});

            await syncOnce;

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'pending');
            assert.equal(BuildRequest.findById(700).statusUrl(), null);
            assert.equal(BuildRequest.findById(701).status(), 'pending');
            assert.equal(BuildRequest.findById(701).statusUrl(), null);

            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'scheduled');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
            assert.equal(BuildRequest.findById(701).status(), 'pending');
            assert.equal(BuildRequest.findById(701).statusUrl(), null);
        });

        it('should not schedule a build request on a different builder than the one the first build request is pending', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                let config = MockData.mockTestSyncConfigWithTwoBuilders();
                let logger = new MockLogger;
                let workerInfo = {name: 'sync-worker', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve(MockData.pendingBuild({buildRequestId: 700, buildbotBuildRequestId: 17}));
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[2].resolve({});
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 6);
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[4].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[4].resolve(MockData.pendingBuild({buildRequestId: 700, buildbotBuildRequestId: 17}));
                assert.strictEqual(MockRemoteAPI.requests[5].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[5].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 8);
                assert.strictEqual(MockRemoteAPI.requests[6].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[6].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[6].resolve({});
                assert.strictEqual(MockRemoteAPI.requests[7].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[7].resolve({});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
            });
        });

        it('should update the status of a pending build and schedule a new build if the pending build had started running', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithTwoBuilders();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[2].resolve({'builds' : [MockData.runningBuildData({buildRequestId: 701}), MockData.finishedBuildData({buildRequestId: 700})]});
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 5);
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[4].url, '/api/v2/forceschedulers/force-some-builder-1');
                assert.deepStrictEqual(MockRemoteAPI.requests[4].data, {'id': '702', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '702', 'forcescheduler': 'force-some-builder-1'}});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 7);
                assert.strictEqual(MockRemoteAPI.requests[5].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[5].resolve(MockData.pendingBuild({buildRequestId: 702, buildbotBuildRequestId: 17}));
                assert.strictEqual(MockRemoteAPI.requests[6].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[6].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[6].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 9);
                assert.strictEqual(MockRemoteAPI.requests[7].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[7].resolve({'builds' : [MockData.runningBuildData({buildRequestId: 701}), MockData.finishedBuildData({buildRequestId: 700})]});
                assert.strictEqual(MockRemoteAPI.requests[8].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[8].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[8].resolve({});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'failed');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), MockData.statusUrl('some-builder-1', 123));
                assert.strictEqual(BuildRequest.findById(701).status(), 'running');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), MockData.statusUrl('some-builder-1', 124));
                assert.strictEqual(BuildRequest.findById(702).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
            });
        });

        it('should update the status of a scheduled build if the pending build had started running', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['scheduled', 'pending', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let workerInfo = {name: 'sync-worker', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 1);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve(MockData.pendingBuild({buildRequestId: 700, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 3);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[2].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[3].resolve(MockData.runningBuild({buildRequestId: 700}));
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'running');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), MockData.statusUrl('some-builder-1', 124));
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
            });
        });

        it('should schedule a build request on a builder without pending builds if the request belongs to a new test group', () => {
            const db = TestServer.database();
            let syncPromise;
            return Promise.all([
                MockData.addMockData(db, ['completed', 'pending', 'pending', 'pending']),
                MockData.addAnotherMockTestGroup(db, ['pending', 'pending', 'pending', 'pending'])
            ]).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithTwoBuilders();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve(MockData.pendingBuild({buildRequestId: 702, buildbotBuildRequestId: 17}));
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[2].resolve({'builds' : [MockData.runningBuildData({buildRequestId: 701}), MockData.finishedBuildData({buildRequestId: 700})]});
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 5);
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[4].url, '/api/v2/forceschedulers/force-some-builder-2');
                assert.deepStrictEqual(MockRemoteAPI.requests[4].data, {'id': '710', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '710', 'forcescheduler': 'force-some-builder-2'}});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 7);
                assert.strictEqual(MockRemoteAPI.requests[5].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[5].resolve(MockData.pendingBuild({buildRequestId: 702, buildbotBuildRequestId: 17}));
                assert.strictEqual(MockRemoteAPI.requests[6].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[6].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[6].resolve(MockData.pendingBuild({builderId: MockData.builderIDForName('some builder 2'), buildRequestId: 710, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 9);
                assert.strictEqual(MockRemoteAPI.requests[7].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[7].resolve({'builds' : [MockData.runningBuildData({buildRequestId: 701}), MockData.finishedBuildData({buildRequestId: 700})]});
                assert.strictEqual(MockRemoteAPI.requests[8].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[8].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[8].resolve({});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 8);
                assert.strictEqual(BuildRequest.findById(700).status(), 'completed');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(710).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(710).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(711).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(711).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(712).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(712).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(713).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(713).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 8);
                assert.strictEqual(BuildRequest.findById(700).status(), 'completed');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), MockData.statusUrl('some-builder-1', 123));
                assert.strictEqual(BuildRequest.findById(701).status(), 'running');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), MockData.statusUrl('some-builder-1', 124));
                assert.strictEqual(BuildRequest.findById(702).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(710).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(710).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(711).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(711).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(712).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(712).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(713).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(713).statusUrl(), null);
            });
        });

        it('should schedule a build request on the same scheduler the first request had ran', () => {
            const db = TestServer.database();
            let syncPromise;
            return Promise.all([
                MockData.addMockData(db, ['running', 'pending', 'pending', 'pending']),
                MockData.addAnotherMockTestGroup(db, ['running', 'pending', 'pending', 'pending'])
            ]).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithTwoBuilders();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[2].resolve(MockData.runningBuild({buildRequestId: 710}));
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[3].resolve(MockData.runningBuild({builderId: MockData.builderIDForName('some builder 2'), buildRequestId: 700}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 5);
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[4].url, '/api/v2/forceschedulers/force-some-builder-2');
                assert.deepStrictEqual(MockRemoteAPI.requests[4].data, {'id': '701', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '701', 'forcescheduler': 'force-some-builder-2'}});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 6);
                assert.strictEqual(MockRemoteAPI.requests[5].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[5].url, '/api/v2/forceschedulers/force-some-builder-1');
                assert.deepStrictEqual(MockRemoteAPI.requests[5].data, {'id': '711', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '711', 'forcescheduler': 'force-some-builder-1'}});
                MockRemoteAPI.requests[5].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 8);
                assert.strictEqual(MockRemoteAPI.requests[6].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[6].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[6].resolve(MockData.pendingBuild({buildRequestId: 711, buildbotBuildRequestId: 17}));
                assert.strictEqual(MockRemoteAPI.requests[7].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[7].url, MockData.pendingBuildsUrl('some builder 2'));
                MockRemoteAPI.requests[7].resolve(MockData.pendingBuild({builderId: MockData.builderIDForName('some builder 2'), buildRequestId: 701, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 10);
                assert.strictEqual(MockRemoteAPI.requests[8].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[8].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[8].resolve(MockData.runningBuild({buildRequestId: 710}));
                assert.strictEqual(MockRemoteAPI.requests[9].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[9].url, MockData.recentBuildsUrl('some builder 2', 2));
                MockRemoteAPI.requests[9].resolve(MockData.runningBuild({builderId: MockData.builderIDForName('some builder 2'), buildRequestId: 700}));
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 8);
                assert.strictEqual(BuildRequest.findById(700).status(), 'running');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(710).status(), 'running');
                assert.strictEqual(BuildRequest.findById(710).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(711).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(711).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(712).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(712).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(713).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(713).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 8);
                assert.strictEqual(BuildRequest.findById(700).status(), 'running');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), MockData.statusUrl('some builder 2', 124));
                assert.strictEqual(BuildRequest.findById(701).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(710).status(), 'running');
                assert.strictEqual(BuildRequest.findById(710).statusUrl(), MockData.statusUrl('some-builder-1', 124));
                assert.strictEqual(BuildRequest.findById(711).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(711).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(712).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(712).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(713).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(713).statusUrl(), null);
            });
        });

        it('should wait for POST to complete before trying to poll buildbot again', () => {
            const db = TestServer.database();
            const requests = MockRemoteAPI.requests;
            let syncPromise;
            return Promise.all([
                MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']),
                MockData.addAnotherMockTestGroup(db, ['pending', 'pending', 'pending', 'pending'])
            ]).then(() => Manifest.fetch()).then(() => {
                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 1);
                assert.strictEqual(requests[0].method, 'GET');
                assert.strictEqual(requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 2);
                assert.strictEqual(requests[1].method, 'GET');
                assert.strictEqual(requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 3);
                assert.strictEqual(requests[2].method, 'POST');
                assert.strictEqual(requests[2].url, '/api/v2/forceschedulers/force-some-builder-1');
                assert.deepStrictEqual(requests[2].data, {'id': '700', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '700', 'forcescheduler': 'force-some-builder-1'}});
                return new Promise((resolve) => setTimeout(resolve, 10));
            }).then(() => {
                assert.strictEqual(requests.length, 3);
                requests[2].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 4);
                assert.strictEqual(requests[3].method, 'GET');
                assert.strictEqual(requests[3].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[3].resolve(MockData.pendingBuild({buildRequestId: 700, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 5);
                assert.strictEqual(requests[4].method, 'GET');
                assert.strictEqual(requests[4].url, MockData.recentBuildsUrl('some-builder-1', 2));
                requests[4].resolve({});
                return syncPromise;
            }).then(() => {
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 8);
                assert.strictEqual(BuildRequest.findById(700).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(710).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(710).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(711).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(711).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(712).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(712).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(713).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(713).statusUrl(), null);
            });
        });

        it('should recover from multiple test groups running simultaneously', () => {
            const db = TestServer.database();
            const requests = MockRemoteAPI.requests;

            let syncPromise;
            let triggerable;
            return Promise.all([
                MockData.addMockData(db, ['completed', 'pending', 'pending', 'pending']),
                MockData.addAnotherMockTestGroup(db, ['completed', 'pending', 'pending', 'pending'])
            ]).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 1);
                assertRequestAndResolve(requests[0], 'GET', MockData.pendingBuildsUrl('some-builder-1'), {});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 2);
                assertRequestAndResolve(requests[1], 'GET', MockData.recentBuildsUrl('some-builder-1', 2),
                    {'builds' : [MockData.runningBuildData({buildRequestId: 700}), MockData.finishedBuildData({buildRequestId: 710})]});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 3);
                assertRequestAndResolve(requests[2], 'POST', '/api/v2/forceschedulers/force-some-builder-1');
                assert.deepStrictEqual(MockRemoteAPI.requests[2].data, {'id': '701', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '701', 'forcescheduler': 'force-some-builder-1'}});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 4);
                assertRequestAndResolve(requests[3], 'GET', MockData.pendingBuildsUrl('some-builder-1'),
                    MockData.pendingBuild({buildRequestId: 701, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 5);
                assertRequestAndResolve(requests[4], 'GET', MockData.recentBuildsUrl('some-builder-1', 2),
                    {'builds': [MockData.runningBuildData({buildRequestId: 700}), MockData.finishedBuildData({buildRequestId: 710})]});
                return syncPromise;
            }).then(() => {
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(requests[5], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 7);
                assertRequestAndResolve(requests[6], 'GET', MockData.pendingBuildsUrl('some-builder-1'), {});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 8);
                assertRequestAndResolve(requests[7], 'GET', MockData.recentBuildsUrl('some-builder-1', 2),
                    {'builds': [MockData.runningBuildData({buildRequestId: 701}), MockData.runningBuildData({buildRequestId: 700})]});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 9);
                assertRequestAndResolve(requests[8], 'GET', MockData.pendingBuildsUrl('some-builder-1'), {});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(requests.length, 10);
                assertRequestAndResolve(requests[9], 'GET', MockData.recentBuildsUrl('some-builder-1', 2),
                    {'builds': [MockData.runningBuildData({buildRequestId: 701}), MockData.runningBuildData({buildRequestId: 700})]});
                return syncPromise;
            });
        });

        it('should recover from missing failed build request', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['failed', 'pending', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 1);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 3);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[2].url, '/api/v2/forceschedulers/force-some-builder-1');
                assert.deepStrictEqual(MockRemoteAPI.requests[2].data, {'id': '701', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '701', 'forcescheduler': 'force-some-builder-1'}});
                MockRemoteAPI.requests[2].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[3].resolve(MockData.pendingBuild({buildRequestId: 701, buildbotBuildRequestId: 17}));
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 5);
                assert.strictEqual(MockRemoteAPI.requests[4].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[4].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[4].resolve({});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'failed');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'failed');
                assert.strictEqual(BuildRequest.findById(700).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(701).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/buildrequests/17');
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).statusUrl(), null);
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).statusUrl(), null);
            });
        });

        it('should update the status of a supposedly scheduled build that went missing', () => {
            const db = TestServer.database();
            let syncPromise;
            return MockData.addMockData(db, ['scheduled', 'pending', 'pending', 'pending']).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 1);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 3);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[2].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 4);
                assert.strictEqual(MockRemoteAPI.requests[3].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[3].resolve({});
                return syncPromise;
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'scheduled');
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(() => {
                assert.strictEqual(BuildRequest.all().length, 4);
                assert.strictEqual(BuildRequest.findById(700).status(), 'failed');
                assert.strictEqual(BuildRequest.findById(701).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(702).status(), 'pending');
                assert.strictEqual(BuildRequest.findById(703).status(), 'pending');
            });
        });

        it('should schedule a build request of an user created test group before ones created by automatic change detection', () => {
            const db = TestServer.database();
            let syncPromise;
            return Promise.all([
                MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']),
                MockData.addAnotherMockTestGroup(db, ['pending', 'pending', 'pending', 'pending'], 'rniwa'),
            ]).then(() => {
                return Manifest.fetch();
            }).then(() => {
                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                MockRemoteAPI.reset();
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 1);
                assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
                MockRemoteAPI.requests[0].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 2);
                assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
                assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.strictEqual(MockRemoteAPI.requests.length, 3);
                assert.strictEqual(MockRemoteAPI.requests[2].method, 'POST');
                assert.strictEqual(MockRemoteAPI.requests[2].url, '/api/v2/forceschedulers/force-some-builder-1');

                assert.deepStrictEqual(MockRemoteAPI.requests[2].data, {'id': '710', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '710', 'forcescheduler': 'force-some-builder-1'}});
                MockRemoteAPI.requests[2].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            });
        });

        it('should skip updating a completed build request whose test group has completed and not listed in a triggerable', async () => {
            await MockData.addMockBuildRequestsWithRoots(TestServer.database(), ['completed', 'completed', 'completed', 'completed', 'pending', 'pending', 'pending', 'pending']);
            await Manifest.fetch();
            const config = MockData.mockTestSyncConfigWithPatchAcceptingBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(BuildRequest.all().length, 4);
            let buildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'pending');
            assert.strictEqual(buildRequest.statusUrl(), null);

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.finishedBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.finishedBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();

            await syncPromise;
            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithPatchAcceptingBuilder().triggerableName);
            assert.strictEqual(BuildRequest.all().length, 8);
            buildRequest = BuildRequest.findById(800);
            let anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'completed');
            assert.strictEqual(anotherBuildRequest.status(), 'completed');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
            assert.strictEqual(anotherBuildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
        });

        it('should reuse the roots from a completed build request with the same commit set', async () => {
            await MockData.addMockBuildRequestsWithRoots(TestServer.database());
            await Manifest.fetch();
            const config = MockData.mockTestSyncConfigWithPatchAcceptingBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(BuildRequest.all().length, 8);
            let buildRequest = BuildRequest.findById(800);
            let anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'completed');
            assert.strictEqual(anotherBuildRequest.status(), 'pending');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
            assert.strictEqual(anotherBuildRequest.statusUrl(), null);
            let commitSet = buildRequest.commitSet();
            let anotherCommitSet = anotherBuildRequest.commitSet();
            assert.ok(commitSet.equalsIgnoringRoot(anotherCommitSet));
            assert.ok(!commitSet.equals(anotherCommitSet));

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();

            await syncPromise;
            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithPatchAcceptingBuilder().triggerableName);
            assert.strictEqual(BuildRequest.all().length, 8);
            buildRequest = BuildRequest.findById(800);
            anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'completed');
            assert.strictEqual(anotherBuildRequest.status(), 'completed');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
            assert.strictEqual(anotherBuildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
        });

        it('should defer scheduling a build request if there is a "running" build request with same commit set, but should schedule the build request if "running" build request with same commit set fails later on', async () => {
            await MockData.addMockBuildRequestsWithRoots(TestServer.database(),  ['running', 'scheduled', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending']);
            await Manifest.fetch();
            const config = MockData.mockTestSyncConfigWithPatchAcceptingBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(BuildRequest.all().length, 8);
            let buildRequest = BuildRequest.findById(800);
            let anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'running');
            assert.strictEqual(anotherBuildRequest.status(), 'pending');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
            assert.strictEqual(anotherBuildRequest.statusUrl(), null);
            let commitSet = buildRequest.commitSet();
            let anotherCommitSet = anotherBuildRequest.commitSet();
            assert.ok(commitSet.equalsIgnoringRoot(anotherCommitSet));
            assert.ok(!commitSet.equals(anotherCommitSet));

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.pendingBuild({buildRequestId: 801})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();

            await syncPromise;
            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithPatchAcceptingBuilder().triggerableName);
            assert.strictEqual(BuildRequest.all().length, 8);
            buildRequest = BuildRequest.findById(800);
            anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'failed');
            assert.strictEqual(anotherBuildRequest.status(), 'pending');
            assert.strictEqual(buildRequest.statusUrl(), MockData.statusUrl('some-builder-1', 123));
            assert.strictEqual(anotherBuildRequest.statusUrl(), null);

            const secondSyncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 1);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'POST');
            assert.strictEqual(MockRemoteAPI.requests[0].url, '/api/v2/forceschedulers/force-some-builder-2');
            assert.deepStrictEqual(MockRemoteAPI.requests[0].data, {'id': '900', 'jsonrpc': '2.0', 'method': 'force', 'params':
                {'wk': '191622', 'os': '10.11 15A284', 'wk-patch': 'http://localhost:8180/api/uploaded-file/100.txt',
                'build-request-id': '900', 'forcescheduler': 'force-some-builder-2'}});
        });

        it('should schedule a build request on a builder when its recent builds had not been fetched yet', async () => {
            const db = TestServer.database();
            await MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithSingleBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 1);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 2);
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds' : [
                MockData.finishedBuildData({results: 0, buildRequestId: 698, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 699, buildTag: 124})]});
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'POST');
            assert.strictEqual(MockRemoteAPI.requests[2].url, '/api/v2/forceschedulers/force-some-builder-1');

            assert.deepStrictEqual(MockRemoteAPI.requests[2].data, {'id': '700', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '700', 'forcescheduler': 'force-some-builder-1'}});
            MockRemoteAPI.requests[2].resolve('OK');
            assert(!BuildRequest.findById(699));
        });

        it('should defer scheduling requests from next configuration in sequential test group until the current configuration meets the initial requested iteration count', async () => {
            const db = TestServer.database();
            await MockData.addMockData(db, ['completed', 'failed', 'pending', 'pending'], true,
                ['http://build.webkit.org/#/builders/2/builds/123', 'http://build.webkit.org/#/builders/2/builds/124', null, null],
                [401, 401, 402, 402], 'sequential', true);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithTwoBuilders();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncOnce = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[1].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 4);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[2].resolve({'builds' : [
                MockData.finishedBuildData({results: 0, buildRequestId: 700, buildTag: 123}),
                MockData.finishedBuildData({results: 1, buildRequestId: 701, buildTag: 124})]});
            assert.equal(MockRemoteAPI.requests[3].method, 'GET');
            assert.equal(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[3].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 6);
            assert.equal(MockRemoteAPI.requests[4].method, 'GET');
            assert.equal(MockRemoteAPI.requests[4].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[4].resolve({});
            assert.equal(MockRemoteAPI.requests[5].method, 'GET');
            assert.equal(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[5].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 8);
            assert.equal(MockRemoteAPI.requests[6].method, 'GET');
            assert.equal(MockRemoteAPI.requests[6].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[6].resolve({'builds' : [
                MockData.finishedBuildData({results: 0, buildRequestId: 700, buildTag: 123}),
                MockData.finishedBuildData({results: 1, buildRequestId: 701, buildTag: 124})]});
            assert.equal(MockRemoteAPI.requests[7].method, 'GET');
            assert.equal(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[7].resolve({});

            await syncOnce;

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'completed');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(701).status(), 'failed');
            assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(702).status(), 'pending');
            assert.equal(BuildRequest.findById(703).status(), 'pending');

            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'completed');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(701).status(), 'failed');
            assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(702).status(), 'pending');
            assert.equal(BuildRequest.findById(703).status(), 'pending');
        });

        it('should not defer scheduling requests from next configuration in sequential test group if all iterations for current configuration failed', async () => {
            const db = TestServer.database();
            await MockData.addMockData(db, ['failed', 'failed', 'pending', 'pending'], true,
                ['http://build.webkit.org/#/builders/2/builds/123', 'http://build.webkit.org/#/builders/2/builds/124', null, null],
                [401, 401, 402, 402], 'sequential', true);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithTwoBuilders();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncOnce = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[1].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 4);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[2].resolve({'builds' : [
                    MockData.finishedBuildData({results: 1, buildRequestId: 700, buildTag: 123}),
                    MockData.finishedBuildData({results: 1, buildRequestId: 701, buildTag: 124})]});
            assert.equal(MockRemoteAPI.requests[3].method, 'GET');
            assert.equal(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[3].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 5);
            assert.equal(MockRemoteAPI.requests[4].method, 'POST');

            assert.strictEqual(MockRemoteAPI.requests[4].method, 'POST');
            assert.strictEqual(MockRemoteAPI.requests[4].url, '/api/v2/forceschedulers/force-some-builder-1');
            assert.deepStrictEqual(MockRemoteAPI.requests[4].data, {'id': '702', 'jsonrpc': '2.0', 'method': 'force', 'params':
                    {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '702', 'forcescheduler': 'force-some-builder-1'}});
            MockRemoteAPI.requests[4].resolve('OK');

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 7);
            assert.equal(MockRemoteAPI.requests[5].method, 'GET');
            assert.equal(MockRemoteAPI.requests[5].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[5].resolve({});
            assert.equal(MockRemoteAPI.requests[6].method, 'GET');
            assert.equal(MockRemoteAPI.requests[6].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[6].resolve({});

            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 9);
            assert.equal(MockRemoteAPI.requests[7].method, 'GET');
            assert.equal(MockRemoteAPI.requests[7].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[7].resolve({'builds' : [
                    MockData.runningBuildData({buildRequestId: 702, buildTag: 125}),
                    MockData.finishedBuildData({results: 1, buildRequestId: 701, buildTag: 124})]});
            assert.equal(MockRemoteAPI.requests[8].method, 'GET');
            assert.equal(MockRemoteAPI.requests[8].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[8].resolve({});

            await syncOnce;

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'failed');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(701).status(), 'failed');
            assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(702).status(), 'pending');
            assert.equal(BuildRequest.findById(703).status(), 'pending');

            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);

            assert.equal(BuildRequest.all().length, 4);
            assert.equal(BuildRequest.findById(700).status(), 'failed');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(701).status(), 'failed');
            assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(702).status(), 'running');
            assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/125');
            assert.equal(BuildRequest.findById(703).status(), 'pending');
        });

        it('should not schedule other build request on a builder if a sequential test group is scheduled on it and waiting for retry', async () => {
            const db = TestServer.database();
            await MockData.addMockData(db, ['failed', 'completed', 'pending', 'pending'], true,
                ['http://build.webkit.org/#/builders/2/builds/123', 'http://build.webkit.org/#/builders/2/builds/124', null, null],
                [401, 401, 402, 402], 'sequential', true);
            await MockData.addAnotherMockTestGroup(db);

            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithSingleBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncOnce = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 1);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds' : [
                MockData.finishedBuildData({results: 1, buildRequestId: 700, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 701, buildTag: 124})]});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[2].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 4)
            assert.equal(MockRemoteAPI.requests[3].method, 'GET');
            assert.equal(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[3].resolve({'builds' : [
                MockData.finishedBuildData({results: 1, buildRequestId: 700, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 701, buildTag: 124})]});

            await syncOnce;

            assert.equal(BuildRequest.all().length, 8);
            assert.equal(BuildRequest.findById(700).status(), 'failed');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(701).status(), 'completed');
            assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(702).status(), 'pending');
            assert.equal(BuildRequest.findById(703).status(), 'pending');
            assert.equal(BuildRequest.findById(710).status(), 'pending');
            assert.equal(BuildRequest.findById(711).status(), 'pending');
            assert.equal(BuildRequest.findById(712).status(), 'pending');
            assert.equal(BuildRequest.findById(713).status(), 'pending');

            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);

            assert.equal(BuildRequest.all().length, 8);
            assert.equal(BuildRequest.findById(700).status(), 'failed');
            assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(701).status(), 'completed');
            assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(702).status(), 'pending');
            assert.equal(BuildRequest.findById(703).status(), 'pending');
            assert.equal(BuildRequest.findById(710).status(), 'pending');
            assert.equal(BuildRequest.findById(711).status(), 'pending');
            assert.equal(BuildRequest.findById(712).status(), 'pending');
            assert.equal(BuildRequest.findById(713).status(), 'pending');
        });

        it('should not schedule a build request from another test group on a builder which is used by an alternating test group needing more retries', async () => {
            const db = TestServer.database();
            await Promise.all([MockData.addMockData(db, ['completed', 'completed', 'failed', 'completed'], true,
                ['http://build.webkit.org/#/builders/2/builds/121', 'http://build.webkit.org/#/builders/2/builds/122',
                 'http://build.webkit.org/#/builders/2/builds/123', 'http://build.webkit.org/#/builders/2/builds/124',],
                [401, 402, 401, 402], 'alternating', true),
                MockData.addAnotherMockTestGroup(db),
            ]);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithSingleBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncOnce = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 1);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds' : [
                MockData.finishedBuildData({results: 1, buildRequestId: 702, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 703, buildTag: 124})]});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[2].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 4)
            assert.equal(MockRemoteAPI.requests[3].method, 'GET');
            assert.equal(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[3].resolve({'builds' : [
                MockData.finishedBuildData({results: 1, buildRequestId: 702, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 703, buildTag: 124})]});

            await syncOnce;

            assert.equal(BuildRequest.all().length, 8);
            assert.equal(BuildRequest.findById(702).status(), 'failed');
            assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(703).status(), 'completed');
            assert.equal(BuildRequest.findById(703).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(710).status(), 'pending');
            assert.equal(BuildRequest.findById(711).status(), 'pending');
            assert.equal(BuildRequest.findById(712).status(), 'pending');
            assert.equal(BuildRequest.findById(713).status(), 'pending');

            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);

            assert.equal(BuildRequest.all().length, 8);
            assert.equal(BuildRequest.findById(702).status(), 'failed');
            assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(703).status(), 'completed');
            assert.equal(BuildRequest.findById(703).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(710).status(), 'pending');
            assert.equal(BuildRequest.findById(711).status(), 'pending');
            assert.equal(BuildRequest.findById(712).status(), 'pending');
            assert.equal(BuildRequest.findById(713).status(), 'pending');
        });

        it('should not schedule a build request from another test group on a builder which is used by a sequential test group needing more retries', async () => {
            const db = TestServer.database();
            await Promise.all([MockData.addMockData(db, ['completed', 'completed', 'failed', 'completed'], true,
                ['http://build.webkit.org/#/builders/2/builds/121', 'http://build.webkit.org/#/builders/2/builds/122',
                 'http://build.webkit.org/#/builders/2/builds/123', 'http://build.webkit.org/#/builders/2/builds/124',],
                [401, 401, 402, 402], 'sequential', true),
                MockData.addAnotherMockTestGroup(db),
            ]);
            await Manifest.fetch();

            const config = MockData.mockTestSyncConfigWithSingleBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncOnce = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 1);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[0].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 2);
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds' : [
                MockData.finishedBuildData({results: 1, buildRequestId: 702, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 703, buildTag: 124})]});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[2].resolve({});

            await MockRemoteAPI.waitForRequest();
            assert.equal(MockRemoteAPI.requests.length, 4)
            assert.equal(MockRemoteAPI.requests[3].method, 'GET');
            assert.equal(MockRemoteAPI.requests[3].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[3].resolve({'builds' : [
                MockData.finishedBuildData({results: 1, buildRequestId: 702, buildTag: 123}),
                MockData.finishedBuildData({results: 0, buildRequestId: 703, buildTag: 124})]});

            await syncOnce;

            assert.equal(BuildRequest.all().length, 8);
            assert.equal(BuildRequest.findById(702).status(), 'failed');
            assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(703).status(), 'completed');
            assert.equal(BuildRequest.findById(703).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(710).status(), 'pending');
            assert.equal(BuildRequest.findById(711).status(), 'pending');
            assert.equal(BuildRequest.findById(712).status(), 'pending');
            assert.equal(BuildRequest.findById(713).status(), 'pending');

            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);

            assert.equal(BuildRequest.all().length, 8);
            assert.equal(BuildRequest.findById(702).status(), 'failed');
            assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/123');
            assert.equal(BuildRequest.findById(703).status(), 'completed');
            assert.equal(BuildRequest.findById(703).statusUrl(), 'http://build.webkit.org/#/builders/2/builds/124');
            assert.equal(BuildRequest.findById(710).status(), 'pending');
            assert.equal(BuildRequest.findById(711).status(), 'pending');
            assert.equal(BuildRequest.findById(712).status(), 'pending');
            assert.equal(BuildRequest.findById(713).status(), 'pending');
        });

        it('should not reuse the root when a build request with same commit set is available but the build request has been scheduled', async () => {
            await MockData.addMockBuildRequestsWithRoots(TestServer.database());
            await Manifest.fetch();
            const config = MockData.mockTestSyncConfigWithPatchAcceptingBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(BuildRequest.all().length, 8);
            let buildRequest = BuildRequest.findById(800);
            let anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'completed');
            assert.strictEqual(anotherBuildRequest.status(), 'pending');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
            assert.strictEqual(anotherBuildRequest.statusUrl(), null);
            let commitSet = buildRequest.commitSet();
            let anotherCommitSet = anotherBuildRequest.commitSet();
            assert.ok(commitSet.equalsIgnoringRoot(anotherCommitSet));
            assert.ok(!commitSet.equals(anotherCommitSet));

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.strictEqual(MockRemoteAPI.requests.length, 3);
            assert.strictEqual(MockRemoteAPI.requests[0].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.strictEqual(MockRemoteAPI.requests[1].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.strictEqual(MockRemoteAPI.requests[2].method, 'GET');
            assert.strictEqual(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({'builds': [MockData.runningBuildData({buildRequestId: 900, builderId: 3})]});
            MockRemoteAPI.reset();

            await syncPromise;
            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithPatchAcceptingBuilder().triggerableName);
            assert.strictEqual(BuildRequest.all().length, 8);
            buildRequest = BuildRequest.findById(800);
            anotherBuildRequest = BuildRequest.findById(900);
            assert.strictEqual(buildRequest.status(), 'completed');
            assert.strictEqual(anotherBuildRequest.status(), 'running');
            assert.strictEqual(buildRequest.statusUrl(), 'http://build.webkit.org/buids/1');
            assert.strictEqual(anotherBuildRequest.statusUrl(), 'http://build.webkit.org/#/builders/3/builds/124');
        });

        it('should not update build requests from another triggerable', async () => {
            const db = TestServer.database();
            await MockData.addMockBuildRequestsForTwoTriggerablesUnderOneAnalysisTask(db, ['running', 'scheduled', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending']);
            await Manifest.fetch();
            const config = MockData.mockTestSyncConfigWithPatchAcceptingBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const syncPromise = triggerable.initSyncers().then(() => triggerable.syncOnce());
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            let buildRequest1000 = await db.selectFirstRow('build_requests', {id: 1000});
            assert.equal(buildRequest1000.status, 'running')
            assert.equal(BuildRequest.all().length, 8);

            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.pendingBuild({buildRequestId: 801})]});
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 800})]});
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.pendingBuildsUrl('some tester'));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.pendingBuildsUrl('some-builder-1'));
            MockRemoteAPI.requests[1].resolve({});
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.pendingBuildsUrl('some builder 2'));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();
            await MockRemoteAPI.waitForRequest();

            assert.equal(MockRemoteAPI.requests.length, 3);
            assert.equal(MockRemoteAPI.requests[0].method, 'GET');
            assert.equal(MockRemoteAPI.requests[0].url, MockData.recentBuildsUrl('some tester', 2));
            MockRemoteAPI.requests[0].resolve({});
            assert.equal(MockRemoteAPI.requests[1].method, 'GET');
            assert.equal(MockRemoteAPI.requests[1].url, MockData.recentBuildsUrl('some-builder-1', 2));
            MockRemoteAPI.requests[1].resolve({'builds': [MockData.runningBuildData({buildRequestId: 801}), MockData.finishedBuildData({buildRequestId: 800})]});
            assert.equal(MockRemoteAPI.requests[2].method, 'GET');
            assert.equal(MockRemoteAPI.requests[2].url, MockData.recentBuildsUrl('some builder 2', 2));
            MockRemoteAPI.requests[2].resolve({});
            MockRemoteAPI.reset();

            await syncPromise;
            await BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithPatchAcceptingBuilder().triggerableName);
            assert.equal(BuildRequest.all().length, 10);
            buildRequest1000 = await db.selectFirstRow('build_requests', {id: 1000});
            assert.equal(buildRequest1000.status, 'running')
        });
    });

    describe('updateTriggerables', () => {

        function refetchManifest()
        {
            MockData.resetV3Models();
            return TestServer.remoteAPI().getJSON('/api/manifest').then((content) => Manifest._didFetchManifest(content));
        }

        it('should update available triggerables', () => {
            const db = TestServer.database();
            let macos;
            let webkit;
            return MockData.addMockData(db).then(() => {
                return Manifest.fetch();
            }).then(() => {
                macos = Repository.findById(9);
                assert.strictEqual(macos.name(), 'macOS');
                webkit = Repository.findById(11);
                assert.strictEqual(webkit.name(), 'WebKit');
                assert.strictEqual(Triggerable.all().length, 1);

                const triggerable = Triggerable.all()[0];
                assert.strictEqual(triggerable.name(), 'build-webkit');

                const test = Test.findById(MockData.someTestId());
                const platform = Platform.findById(MockData.somePlatformId());
                assert.equal(Triggerable.findByTestConfiguration(test, platform), null);

                const groups = TriggerableRepositoryGroup.sortByName(triggerable.repositoryGroups());
                assert.strictEqual(groups.length, 1);
                assert.strictEqual(groups[0].name(), 'webkit-svn');
                assert.deepStrictEqual(groups[0].repositories(), [webkit, macos]);

                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                config.repositoryGroups = {
                    'system-and-roots': {
                        description: 'Custom Roots',
                        repositories: {'macOS': {}},
                        testProperties: {
                            'os': {'revision': 'macOS'},
                            'roots': {'roots': {}}
                        },
                        acceptsRoots: true
                    },
                    'system-and-webkit': {
                        repositories: {'WebKit': {'acceptsPatch': true}, 'macOS': {}},
                        testProperties: {
                            'os': {'revision': 'macOS'},
                            'wk': {'revision': 'WebKit'},
                            'roots': {'roots': {}},
                        },
                        buildProperties: {
                            'wk': {'revision': 'WebKit'},
                            'wk-patch': {'patch': 'WebKit'},
                        },
                        acceptsRoots: true
                    }
                }

                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const buildbotTriggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                const triggerablePromise = buildbotTriggerable.initSyncers().then(() => buildbotTriggerable.updateTriggerable());
                assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                return triggerablePromise;
            }).then(() => refetchManifest()).then(() => {
                assert.strictEqual(Triggerable.all().length, 1);

                let test = Test.findById(MockData.someTestId());
                let platform = Platform.findById(MockData.somePlatformId());
                let triggerable = Triggerable.findByTestConfiguration(test, platform);
                assert.strictEqual(triggerable.name(), 'build-webkit');

                const groups = TriggerableRepositoryGroup.sortByName(triggerable.repositoryGroups());
                assert.strictEqual(groups.length, 2);
                assert.strictEqual(groups[0].name(), 'system-and-roots');
                assert.strictEqual(groups[0].description(), 'Custom Roots');
                assert.deepStrictEqual(groups[0].repositories(), [macos]);
                assert.strictEqual(groups[0].acceptsCustomRoots(), true);
                assert.strictEqual(groups[1].name(), 'system-and-webkit');
                assert.deepStrictEqual(groups[1].repositories(), [webkit, macos]);
                assert.strictEqual(groups[1].acceptsCustomRoots(), true);

                const config = MockData.mockTestSyncConfigWithSingleBuilder();
                config.repositoryGroups = [ ];

                const logger = new MockLogger;
                const workerInfo = {name: 'sync-worker', password: 'password'};
                const buildbotTriggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
                const triggerablePromise = buildbotTriggerable.initSyncers().then(() => buildbotTriggerable.updateTriggerable());
                assertRequestAndResolve(MockRemoteAPI.requests[1], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
                return triggerablePromise;
            }).then(() => refetchManifest()).then(() => {
                assert.strictEqual(Triggerable.all().length, 1);
                const groups = TriggerableRepositoryGroup.sortByName(Triggerable.all()[0].repositoryGroups());
                assert.strictEqual(groups.length, 2);
                assert.strictEqual(groups[0].name(), 'system-and-roots');
                assert.deepStrictEqual(groups[0].repositories(), [macos]);
                assert.strictEqual(groups[1].name(), 'system-and-webkit');
                assert.deepStrictEqual(groups[1].repositories(), [webkit, macos]);
            })
        });
    });

    describe('getBuilderNameToIDMap', () => {

        it('should get Builder Name to ID Map', () => {
            const config = MockData.mockTestSyncConfigWithSingleBuilder();
            const logger = new MockLogger;
            const workerInfo = {name: 'sync-worker', password: 'password'};
            const buildbotTriggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, workerInfo, 2, logger);
            const getBuilderNameToIDMapPromise = buildbotTriggerable.getBuilderNameToIDMap();
            assertRequestAndResolve(MockRemoteAPI.requests[0], 'GET', MockData.buildbotBuildersURL(), MockData.mockBuildbotBuilders());
            getBuilderNameToIDMapPromise.then((builderNameToIDMap) => {
                assert.strictEqual(builderNameToIDMap["some builder"], 1)
                assert.strictEqual(builderNameToIDMap["some-builder-1"], 2)
                assert.strictEqual(builderNameToIDMap["some builder 2"], 3)
                assert.strictEqual(builderNameToIDMap["other builder"], 4)
                assert.strictEqual(builderNameToIDMap["some tester"], 5)
                assert.strictEqual(builderNameToIDMap["another tester"], 6)
            });
        });
    });
});
