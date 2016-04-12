'use strict';

let assert = require('assert');

let BuildbotTriggerable = require('../tools/js/buildbot-triggerable.js').BuildbotTriggerable;
let MockData = require('./resources/mock-data.js');
let MockRemoteAPI = require('../unit-tests/resources/mock-remote-api.js').MockRemoteAPI;
let TestServer = require('./resources/test-server.js');

class MockLogger {
    constructor()
    {
        this._logs = [];
    }

    log(text) { this._logs.push(text); }
    error(text) { this._logs.push(text); }
}

describe('BuildbotTriggerable', function () {
    this.timeout(1000);
    TestServer.inject();

    beforeEach(function () {
        MockData.resetV3Models();
        MockRemoteAPI.reset('http://build.webkit.org');
    });

    describe('syncOnce', function () {
        it('should schedule the next build request when there are no pending builds', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['completed', 'running', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'completed');
                assert.equal(BuildRequest.findById(701).status(), 'running');
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[1].resolve({[-1]: MockData.runningBuild(), [-2]: MockData.finishedBuild()});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[2].method, 'POST');
                assert.equal(MockRemoteAPI.requests[2].url, '/builders/some-builder-1/force');
                assert.deepEqual(MockRemoteAPI.requests[2].data, {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '702'});
                MockRemoteAPI.requests[2].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[3].resolve([MockData.pendingBuild()])
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[4].method, 'GET');
                assert.equal(MockRemoteAPI.requests[4].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[4].resolve({[-1]: MockData.runningBuild(), [-2]: MockData.finishedBuild()});
                return syncPromise;
            }).then(function () {
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithSingleBuilder().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'completed');
                assert.equal(BuildRequest.findById(701).status(), 'running');
                assert.equal(BuildRequest.findById(702).status(), 'scheduled');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                done();
            }).catch(done);
        });

        it('should not schedule the next build request when there is a pending build', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['completed', 'running', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([MockData.pendingBuild()]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[1].resolve({[-1]: MockData.runningBuild(), [-2]: MockData.finishedBuild()});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[2].resolve([MockData.pendingBuild()])
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({[-1]: MockData.runningBuild(), [-2]: MockData.finishedBuild()});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'completed');
                assert.equal(BuildRequest.findById(701).status(), 'running');
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithSingleBuilder().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'completed');
                assert.equal(BuildRequest.findById(701).status(), 'running');
                assert.equal(BuildRequest.findById(702).status(), 'scheduled');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                done();
            }).catch(done);
        });

        it('should schedule the build request on a builder without a pending build if it\'s the first request in the group', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithTwoBuilders();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([MockData.pendingBuild({buildRequestId: 999})]);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[1].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[2].resolve({});
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 5);
                assert.equal(MockRemoteAPI.requests[4].method, 'POST');
                assert.equal(MockRemoteAPI.requests[4].url, '/builders/some%20builder%202/force');
                assert.deepEqual(MockRemoteAPI.requests[4].data, {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '700'});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 7);
                assert.equal(MockRemoteAPI.requests[5].method, 'GET');
                assert.equal(MockRemoteAPI.requests[5].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[5].resolve([MockData.pendingBuild({buildRequestId: 999})]);
                assert.equal(MockRemoteAPI.requests[6].method, 'GET');
                assert.equal(MockRemoteAPI.requests[6].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[6].resolve([MockData.pendingBuild({builder: 'some builder 2', buildRequestId: 700})]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 9);
                assert.equal(MockRemoteAPI.requests[7].method, 'GET');
                assert.equal(MockRemoteAPI.requests[7].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[7].resolve({});
                assert.equal(MockRemoteAPI.requests[8].method, 'GET');
                assert.equal(MockRemoteAPI.requests[8].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[8].resolve({});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'pending');
                assert.equal(BuildRequest.findById(700).statusUrl(), null);
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'scheduled');
                assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/builders/some%20builder%202/');
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                done();
            }).catch(done);
        });

        it('should not schedule a build request on a different builder than the one the first build request is pending', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithTwoBuilders();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([MockData.pendingBuild({buildRequestId: 700})]);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[1].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[2].resolve({});
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 6);
                assert.equal(MockRemoteAPI.requests[4].method, 'GET');
                assert.equal(MockRemoteAPI.requests[4].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[4].resolve([MockData.pendingBuild({buildRequestId: 700})]);
                assert.equal(MockRemoteAPI.requests[5].method, 'GET');
                assert.equal(MockRemoteAPI.requests[5].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[5].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 8);
                assert.equal(MockRemoteAPI.requests[6].method, 'GET');
                assert.equal(MockRemoteAPI.requests[6].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[6].resolve({});
                assert.equal(MockRemoteAPI.requests[7].method, 'GET');
                assert.equal(MockRemoteAPI.requests[7].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[7].resolve({});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'pending');
                assert.equal(BuildRequest.findById(700).statusUrl(), null);
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'scheduled');
                assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/');
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                done();
            }).catch(done);
        });

        it('should update the status of a pending build and schedule a new build if the pending build had started running', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithTwoBuilders();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([]);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[1].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[2].resolve({[-1]: MockData.runningBuild({buildRequestId: 701}), [-2]: MockData.finishedBuild({buildRequestId: 700})});
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 5);
                assert.equal(MockRemoteAPI.requests[4].method, 'POST');
                assert.equal(MockRemoteAPI.requests[4].url, '/builders/some-builder-1/force');
                assert.deepEqual(MockRemoteAPI.requests[4].data, {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '702'});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 7);
                assert.equal(MockRemoteAPI.requests[5].method, 'GET');
                assert.equal(MockRemoteAPI.requests[5].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[5].resolve([MockData.pendingBuild({buildRequestId: 702})]);
                assert.equal(MockRemoteAPI.requests[6].method, 'GET');
                assert.equal(MockRemoteAPI.requests[6].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[6].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 9);
                assert.equal(MockRemoteAPI.requests[7].method, 'GET');
                assert.equal(MockRemoteAPI.requests[7].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[7].resolve({[-1]: MockData.runningBuild({buildRequestId: 701}), [-2]: MockData.finishedBuild({buildRequestId: 700})});
                assert.equal(MockRemoteAPI.requests[8].method, 'GET');
                assert.equal(MockRemoteAPI.requests[8].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[8].resolve({});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'pending');
                assert.equal(BuildRequest.findById(700).statusUrl(), null);
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'failed');
                assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/builds/123');
                assert.equal(BuildRequest.findById(701).status(), 'running');
                assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/builds/124');
                assert.equal(BuildRequest.findById(702).status(), 'scheduled');
                assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                done();
            }).catch(done);
        });

        it('should update the status of a scheduled build if the pending build had started running', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['scheduled', 'pending', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 1);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([MockData.pendingBuild({buildRequestId: 700})]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 3);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[2].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({[-1]: MockData.runningBuild({buildRequestId: 700})});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'scheduled');
                assert.equal(BuildRequest.findById(700).statusUrl(), null);
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'running');
                assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/builds/124');
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                done();
            }).catch(done);
        });

        it('should schedule a build request on a builder without pending builds if the request belongs to a new test group', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return Promise.all([
                    MockData.addMockData(db, ['completed', 'pending', 'pending', 'pending']),
                    MockData.addAnotherMockTestGroup(db, ['pending', 'pending', 'pending', 'pending'])
                ]);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithTwoBuilders();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([MockData.pendingBuild({buildRequestId: 702})]);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[1].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[2].resolve({[-1]: MockData.runningBuild({buildRequestId: 701}), [-2]: MockData.finishedBuild({buildRequestId: 700})});
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 5);
                assert.equal(MockRemoteAPI.requests[4].method, 'POST');
                assert.equal(MockRemoteAPI.requests[4].url, '/builders/some%20builder%202/force');
                assert.deepEqual(MockRemoteAPI.requests[4].data, {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '710'});
                MockRemoteAPI.requests[4].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 7);
                assert.equal(MockRemoteAPI.requests[5].method, 'GET');
                assert.equal(MockRemoteAPI.requests[5].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[5].resolve([MockData.pendingBuild({buildRequestId: 702})]);
                assert.equal(MockRemoteAPI.requests[6].method, 'GET');
                assert.equal(MockRemoteAPI.requests[6].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[6].resolve([MockData.pendingBuild({builder: 'some builder 2', buildRequestId: 710})]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 9);
                assert.equal(MockRemoteAPI.requests[7].method, 'GET');
                assert.equal(MockRemoteAPI.requests[7].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[7].resolve({[-1]: MockData.runningBuild({buildRequestId: 701}), [-2]: MockData.finishedBuild({buildRequestId: 700})});
                assert.equal(MockRemoteAPI.requests[8].method, 'GET');
                assert.equal(MockRemoteAPI.requests[8].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[8].resolve({});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 8);
                assert.equal(BuildRequest.findById(700).status(), 'completed');
                assert.equal(BuildRequest.findById(700).statusUrl(), null);
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                assert.equal(BuildRequest.findById(710).status(), 'pending');
                assert.equal(BuildRequest.findById(710).statusUrl(), null);
                assert.equal(BuildRequest.findById(711).status(), 'pending');
                assert.equal(BuildRequest.findById(711).statusUrl(), null);
                assert.equal(BuildRequest.findById(712).status(), 'pending');
                assert.equal(BuildRequest.findById(712).statusUrl(), null);
                assert.equal(BuildRequest.findById(713).status(), 'pending');
                assert.equal(BuildRequest.findById(713).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 8);
                assert.equal(BuildRequest.findById(700).status(), 'completed');
                assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/builds/123');
                assert.equal(BuildRequest.findById(701).status(), 'running');
                assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/builds/124');
                assert.equal(BuildRequest.findById(702).status(), 'scheduled');
                assert.equal(BuildRequest.findById(702).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                assert.equal(BuildRequest.findById(710).status(), 'scheduled');
                assert.equal(BuildRequest.findById(710).statusUrl(), 'http://build.webkit.org/builders/some%20builder%202/');
                assert.equal(BuildRequest.findById(711).status(), 'pending');
                assert.equal(BuildRequest.findById(711).statusUrl(), null);
                assert.equal(BuildRequest.findById(712).status(), 'pending');
                assert.equal(BuildRequest.findById(712).statusUrl(), null);
                assert.equal(BuildRequest.findById(713).status(), 'pending');
                assert.equal(BuildRequest.findById(713).statusUrl(), null);
                done();
            }).catch(done);
        });

        it('should schedule a build request on the same scheduler the first request had ran', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return Promise.all([
                    MockData.addMockData(db, ['running', 'pending', 'pending', 'pending']),
                    MockData.addAnotherMockTestGroup(db, ['running', 'pending', 'pending', 'pending'])
                ]);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithTwoBuilders();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([]);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[1].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[2].resolve({[-1]: MockData.runningBuild({buildRequestId: 710})});
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({[-1]: MockData.runningBuild({builder: 'some builder 2', buildRequestId: 700})});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 6);
                assert.equal(MockRemoteAPI.requests[4].method, 'POST');
                assert.equal(MockRemoteAPI.requests[4].url, '/builders/some%20builder%202/force');
                assert.deepEqual(MockRemoteAPI.requests[4].data, {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '701'});
                MockRemoteAPI.requests[4].resolve('OK');
                assert.equal(MockRemoteAPI.requests[5].method, 'POST');
                assert.equal(MockRemoteAPI.requests[5].url, '/builders/some-builder-1/force');
                assert.deepEqual(MockRemoteAPI.requests[5].data, {'wk': '192736', 'os': '10.11 15A284', 'build-request-id': '711'});
                MockRemoteAPI.requests[5].resolve('OK');
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 8);
                assert.equal(MockRemoteAPI.requests[6].method, 'GET');
                assert.equal(MockRemoteAPI.requests[6].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[6].resolve([MockData.pendingBuild({buildRequestId: 711})]);
                assert.equal(MockRemoteAPI.requests[7].method, 'GET');
                assert.equal(MockRemoteAPI.requests[7].url, '/json/builders/some%20builder%202/pendingBuilds');
                MockRemoteAPI.requests[7].resolve([MockData.pendingBuild({builder: 'some builder 2',buildRequestId: 701})]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 10);
                assert.equal(MockRemoteAPI.requests[8].method, 'GET');
                assert.equal(MockRemoteAPI.requests[8].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[8].resolve({[-1]: MockData.runningBuild({buildRequestId: 710})});
                assert.equal(MockRemoteAPI.requests[9].method, 'GET');
                assert.equal(MockRemoteAPI.requests[9].url, '/json/builders/some%20builder%202/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[9].resolve({[-1]: MockData.runningBuild({builder: 'some builder 2', buildRequestId: 700})});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 8);
                assert.equal(BuildRequest.findById(700).status(), 'running');
                assert.equal(BuildRequest.findById(700).statusUrl(), null);
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(701).statusUrl(), null);
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                assert.equal(BuildRequest.findById(710).status(), 'running');
                assert.equal(BuildRequest.findById(710).statusUrl(), null);
                assert.equal(BuildRequest.findById(711).status(), 'pending');
                assert.equal(BuildRequest.findById(711).statusUrl(), null);
                assert.equal(BuildRequest.findById(712).status(), 'pending');
                assert.equal(BuildRequest.findById(712).statusUrl(), null);
                assert.equal(BuildRequest.findById(713).status(), 'pending');
                assert.equal(BuildRequest.findById(713).statusUrl(), null);
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 8);
                assert.equal(BuildRequest.findById(700).status(), 'running');
                assert.equal(BuildRequest.findById(700).statusUrl(), 'http://build.webkit.org/builders/some%20builder%202/builds/124');
                assert.equal(BuildRequest.findById(701).status(), 'scheduled');
                assert.equal(BuildRequest.findById(701).statusUrl(), 'http://build.webkit.org/builders/some%20builder%202/');
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(702).statusUrl(), null);
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                assert.equal(BuildRequest.findById(703).statusUrl(), null);
                assert.equal(BuildRequest.findById(710).status(), 'running');
                assert.equal(BuildRequest.findById(710).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/builds/124');
                assert.equal(BuildRequest.findById(711).status(), 'scheduled');
                assert.equal(BuildRequest.findById(711).statusUrl(), 'http://build.webkit.org/builders/some-builder-1/');
                assert.equal(BuildRequest.findById(712).status(), 'pending');
                assert.equal(BuildRequest.findById(712).statusUrl(), null);
                assert.equal(BuildRequest.findById(713).status(), 'pending');
                assert.equal(BuildRequest.findById(713).statusUrl(), null);
                done();
            }).catch(done);
        });

        it('should update the status of a supposedly scheduled build that went missing', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return MockData.addMockData(db, ['scheduled', 'pending', 'pending', 'pending']);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 1);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 3);
                assert.equal(MockRemoteAPI.requests[2].method, 'GET');
                assert.equal(MockRemoteAPI.requests[2].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[2].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 4);
                assert.equal(MockRemoteAPI.requests[3].method, 'GET');
                assert.equal(MockRemoteAPI.requests[3].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[3].resolve({});
                return syncPromise;
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'scheduled');
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                return BuildRequest.fetchForTriggerable(MockData.mockTestSyncConfigWithTwoBuilders().triggerableName);
            }).then(function () {
                assert.equal(BuildRequest.all().length, 4);
                assert.equal(BuildRequest.findById(700).status(), 'failed');
                assert.equal(BuildRequest.findById(701).status(), 'pending');
                assert.equal(BuildRequest.findById(702).status(), 'pending');
                assert.equal(BuildRequest.findById(703).status(), 'pending');
                done();
            }).catch(done);
        });

        it('should schedule a build request of an user created test group before ones created by automatic change detection', function (done) {
            let db = TestServer.database();
            let syncPromise;
            db.connect().then(function () {
                return Promise.all([
                    MockData.addMockData(db, ['pending', 'pending', 'pending', 'pending']),
                    MockData.addAnotherMockTestGroup(db, ['pending', 'pending', 'pending', 'pending'], 'rniwa'),
                ]);
            }).then(function () {
                return Manifest.fetch();
            }).then(function () {
                let config = MockData.mockTestSyncConfigWithSingleBuilder();
                let logger = new MockLogger;
                let slaveInfo = {name: 'sync-slave', password: 'password'};
                let triggerable = new BuildbotTriggerable(config, TestServer.remoteAPI(), MockRemoteAPI, slaveInfo, logger);
                syncPromise = triggerable.syncOnce();
                syncPromise.catch(done);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 1);
                assert.equal(MockRemoteAPI.requests[0].method, 'GET');
                assert.equal(MockRemoteAPI.requests[0].url, '/json/builders/some-builder-1/pendingBuilds');
                MockRemoteAPI.requests[0].resolve([]);
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 2);
                assert.equal(MockRemoteAPI.requests[1].method, 'GET');
                assert.equal(MockRemoteAPI.requests[1].url, '/json/builders/some-builder-1/builds/?select=-1&select=-2');
                MockRemoteAPI.requests[1].resolve({});
                return MockRemoteAPI.waitForRequest();
            }).then(function () {
                assert.equal(MockRemoteAPI.requests.length, 3);
                assert.equal(MockRemoteAPI.requests[2].method, 'POST');
                assert.equal(MockRemoteAPI.requests[2].url, '/builders/some-builder-1/force');
                assert.deepEqual(MockRemoteAPI.requests[2].data, {'wk': '191622', 'os': '10.11 15A284', 'build-request-id': '710'});
                MockRemoteAPI.requests[2].resolve('OK');
                done();
            }).catch(done);
        });
    });
});
