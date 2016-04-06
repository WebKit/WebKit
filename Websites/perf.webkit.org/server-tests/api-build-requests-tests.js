'use strict';

let assert = require('assert');

let MockData = require('./resources/mock-data.js');
let TestServer = require('./resources/test-server.js');

describe('/api/build-requests', function () {
    this.timeout(10000);
    TestServer.inject();

    beforeEach(function () {
        MockData.resetV3Models();
    });

    it('should return "TriggerableNotFound" when the database is empty', function (done) {
        TestServer.remoteAPI().getJSON('/api/build-requests/build-webkit').then(function (content) {
            assert.equal(content['status'], 'TriggerableNotFound');
            done();
        }).catch(done);
    });

    it('should return an empty list when there are no build requests', function (done) {
        TestServer.database().connect().then(function () {
            return TestServer.database().insert('build_triggerables', {name: 'build-webkit'});
        }).then(function () {
            return TestServer.remoteAPI().getJSON('/api/build-requests/build-webkit');
        }).then(function (content) {
            assert.equal(content['status'], 'OK');
            assert.deepEqual(content['buildRequests'], []);
            assert.deepEqual(content['rootSets'], []);
            assert.deepEqual(content['roots'], []);
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'rootSets', 'roots', 'status']);
            done();
        }).catch(done);
    });

    it('should return build requets associated with a given triggerable with appropriate roots and rootSets', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db);
        }).then(function () {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        }).then(function (content) {
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'rootSets', 'roots', 'status']);

            assert.equal(content['rootSets'].length, 2);
            assert.equal(content['rootSets'][0].id, 401);
            assert.deepEqual(content['rootSets'][0].roots, ['87832', '93116']);
            assert.equal(content['rootSets'][1].id, 402);
            assert.deepEqual(content['rootSets'][1].roots, ['87832', '96336']);

            assert.equal(content['roots'].length, 3);
            assert.equal(content['roots'][0].id, 87832);
            assert.equal(content['roots'][0].repository, '9');
            assert.equal(content['roots'][0].revision, '10.11 15A284');
            assert.equal(content['roots'][1].id, 93116);
            assert.equal(content['roots'][1].repository, '11');
            assert.equal(content['roots'][1].revision, '191622');
            assert.equal(content['roots'][2].id, 96336);
            assert.equal(content['roots'][2].repository, '11');
            assert.equal(content['roots'][2].revision, '192736');

            assert.equal(content['buildRequests'].length, 4);
            assert.deepEqual(content['buildRequests'][0].id, 700);
            assert.deepEqual(content['buildRequests'][0].order, 0);
            assert.deepEqual(content['buildRequests'][0].platform, '65');
            assert.deepEqual(content['buildRequests'][0].rootSet, 401);
            assert.deepEqual(content['buildRequests'][0].status, 'pending');
            assert.deepEqual(content['buildRequests'][0].test, '200');

            assert.deepEqual(content['buildRequests'][1].id, 701);
            assert.deepEqual(content['buildRequests'][1].order, 1);
            assert.deepEqual(content['buildRequests'][1].platform, '65');
            assert.deepEqual(content['buildRequests'][1].rootSet, 402);
            assert.deepEqual(content['buildRequests'][1].status, 'pending');
            assert.deepEqual(content['buildRequests'][1].test, '200');

            assert.deepEqual(content['buildRequests'][2].id, 702);
            assert.deepEqual(content['buildRequests'][2].order, 2);
            assert.deepEqual(content['buildRequests'][2].platform, '65');
            assert.deepEqual(content['buildRequests'][2].rootSet, 401);
            assert.deepEqual(content['buildRequests'][2].status, 'pending');
            assert.deepEqual(content['buildRequests'][2].test, '200');

            assert.deepEqual(content['buildRequests'][3].id, 703);
            assert.deepEqual(content['buildRequests'][3].order, 3);
            assert.deepEqual(content['buildRequests'][3].platform, '65');
            assert.deepEqual(content['buildRequests'][3].rootSet, 402);
            assert.deepEqual(content['buildRequests'][3].status, 'pending');
            assert.deepEqual(content['buildRequests'][3].test, '200');
            done();
        }).catch(done);
    });

    it('should support useLegacyIdResolution option', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db);
        }).then(function () {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit?useLegacyIdResolution=true');
        }).then(function (content) {
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'rootSets', 'roots', 'status']);

            assert.equal(content['rootSets'].length, 2);
            assert.equal(content['rootSets'][0].id, 401);
            assert.deepEqual(content['rootSets'][0].roots, ['87832', '93116']);
            assert.equal(content['rootSets'][1].id, 402);
            assert.deepEqual(content['rootSets'][1].roots, ['87832', '96336']);

            assert.equal(content['roots'].length, 3);
            assert.equal(content['roots'][0].id, 87832);
            assert.equal(content['roots'][0].repository, 'OS X');
            assert.equal(content['roots'][0].revision, '10.11 15A284');
            assert.equal(content['roots'][1].id, 93116);
            assert.equal(content['roots'][1].repository, 'WebKit');
            assert.equal(content['roots'][1].revision, '191622');
            assert.equal(content['roots'][2].id, 96336);
            assert.equal(content['roots'][2].repository, 'WebKit');
            assert.equal(content['roots'][2].revision, '192736');

            assert.equal(content['buildRequests'].length, 4);
            assert.deepEqual(content['buildRequests'][0].id, 700);
            assert.deepEqual(content['buildRequests'][0].order, 0);
            assert.deepEqual(content['buildRequests'][0].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][0].rootSet, 401);
            assert.deepEqual(content['buildRequests'][0].status, 'pending');
            assert.deepEqual(content['buildRequests'][0].test, ['some test']);

            assert.deepEqual(content['buildRequests'][1].id, 701);
            assert.deepEqual(content['buildRequests'][1].order, 1);
            assert.deepEqual(content['buildRequests'][1].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][1].rootSet, 402);
            assert.deepEqual(content['buildRequests'][1].status, 'pending');
            assert.deepEqual(content['buildRequests'][1].test, ['some test']);

            assert.deepEqual(content['buildRequests'][2].id, 702);
            assert.deepEqual(content['buildRequests'][2].order, 2);
            assert.deepEqual(content['buildRequests'][2].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][2].rootSet, 401);
            assert.deepEqual(content['buildRequests'][2].status, 'pending');
            assert.deepEqual(content['buildRequests'][2].test, ['some test']);

            assert.deepEqual(content['buildRequests'][3].id, 703);
            assert.deepEqual(content['buildRequests'][3].order, 3);
            assert.deepEqual(content['buildRequests'][3].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][3].rootSet, 402);
            assert.deepEqual(content['buildRequests'][3].status, 'pending');
            assert.deepEqual(content['buildRequests'][3].test, ['some test']);
            done();
        }).catch(done);
    });

    it('should be fetchable by BuildRequest.fetchForTriggerable', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db);
        }).then(function () {
            return Manifest.fetch();
        }).then(function () {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then(function (buildRequests) {
            assert.equal(buildRequests.length, 4);

            let test = Test.findById(200);
            assert(test);

            let platform = Platform.findById(65);
            assert(platform);

            assert.equal(buildRequests[0].id(), 700);
            assert.equal(buildRequests[0].testGroupId(), 600);
            assert.equal(buildRequests[0].test(), test);
            assert.equal(buildRequests[0].platform(), platform);
            assert.equal(buildRequests[0].order(), 0);
            assert.ok(buildRequests[0].rootSet() instanceof RootSet);
            assert.ok(!buildRequests[0].hasFinished());
            assert.ok(!buildRequests[0].hasStarted());
            assert.ok(buildRequests[0].isPending());
            assert.equal(buildRequests[0].statusLabel(), 'Waiting to be scheduled');

            assert.equal(buildRequests[1].id(), 701);
            assert.equal(buildRequests[1].testGroupId(), 600);
            assert.equal(buildRequests[1].test(), test);
            assert.equal(buildRequests[1].platform(), platform);
            assert.equal(buildRequests[1].order(), 1);
            assert.ok(buildRequests[1].rootSet() instanceof RootSet);
            assert.ok(!buildRequests[1].hasFinished());
            assert.ok(!buildRequests[1].hasStarted());
            assert.ok(buildRequests[1].isPending());
            assert.equal(buildRequests[1].statusLabel(), 'Waiting to be scheduled');

            assert.equal(buildRequests[2].id(), 702);
            assert.equal(buildRequests[2].testGroupId(), 600);
            assert.equal(buildRequests[2].test(), test);
            assert.equal(buildRequests[2].platform(), platform);
            assert.equal(buildRequests[2].order(), 2);
            assert.ok(buildRequests[2].rootSet() instanceof RootSet);
            assert.ok(!buildRequests[2].hasFinished());
            assert.ok(!buildRequests[2].hasStarted());
            assert.ok(buildRequests[2].isPending());
            assert.equal(buildRequests[2].statusLabel(), 'Waiting to be scheduled');

            assert.equal(buildRequests[3].id(), 703);
            assert.equal(buildRequests[3].testGroupId(), 600);
            assert.equal(buildRequests[3].test(), test);
            assert.equal(buildRequests[3].platform(), platform);
            assert.equal(buildRequests[3].order(), 3);
            assert.ok(buildRequests[3].rootSet() instanceof RootSet);
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(!buildRequests[3].hasStarted());
            assert.ok(buildRequests[3].isPending());
            assert.equal(buildRequests[3].statusLabel(), 'Waiting to be scheduled');

            let osx = Repository.findById(9);
            assert.equal(osx.name(), 'OS X');

            let webkit = Repository.findById(11);
            assert.equal(webkit.name(), 'WebKit');

            let firstRootSet = buildRequests[0].rootSet();
            assert.equal(buildRequests[2].rootSet(), firstRootSet);

            let secondRootSet = buildRequests[1].rootSet();
            assert.equal(buildRequests[3].rootSet(), secondRootSet);

            assert.equal(firstRootSet.revisionForRepository(osx), '10.11 15A284');
            assert.equal(firstRootSet.revisionForRepository(webkit), '191622');

            assert.equal(secondRootSet.revisionForRepository(osx), '10.11 15A284');
            assert.equal(secondRootSet.revisionForRepository(webkit), '192736');

            let osxCommit = firstRootSet.commitForRepository(osx);
            assert.equal(osxCommit.revision(), '10.11 15A284');
            assert.equal(osxCommit, secondRootSet.commitForRepository(osx));

            let firstWebKitCommit = firstRootSet.commitForRepository(webkit);
            assert.equal(firstWebKitCommit.revision(), '191622');
            assert.equal(+firstWebKitCommit.time(), 1445945816878);

            let secondWebKitCommit = secondRootSet.commitForRepository(webkit);
            assert.equal(secondWebKitCommit.revision(), '192736');
            assert.equal(+secondWebKitCommit.time(), 1448225325650);

            done();
        }).catch(done);
    });

    it('should not include a build request if all requests in the same group had been completed', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db, ['completed', 'completed', 'completed', 'completed']);
        }).then(function () {
            return Manifest.fetch();
        }).then(function () {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then(function (buildRequests) {
            assert.equal(buildRequests.length, 0);
            done();
        }).catch(done);
    });

    it('should not include a build request if all requests in the same group had been failed or cancled', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db, ['failed', 'failed', 'canceled', 'canceled']);
        }).then(function () {
            return Manifest.fetch();
        }).then(function () {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then(function (buildRequests) {
            assert.equal(buildRequests.length, 0);
            done();
        }).catch(done);
    });

    it('should include all build requests of a test group if one of the reqeusts in the group had not been finished', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db, ['completed', 'completed', 'scheduled', 'pending']);
        }).then(function () {
            return Manifest.fetch();
        }).then(function () {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then(function (buildRequests) {
            assert.equal(buildRequests.length, 4);
            assert.ok(buildRequests[0].hasFinished());
            assert.ok(buildRequests[0].hasStarted());
            assert.ok(!buildRequests[0].isPending());
            assert.ok(buildRequests[1].hasFinished());
            assert.ok(buildRequests[1].hasStarted());
            assert.ok(!buildRequests[1].isPending());
            assert.ok(!buildRequests[2].hasFinished());
            assert.ok(buildRequests[2].hasStarted());
            assert.ok(!buildRequests[2].isPending());
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(!buildRequests[3].hasStarted());
            assert.ok(buildRequests[3].isPending());
            done();
        }).catch(done);
    });

    it('should include all build requests of a test group if one of the reqeusts in the group is still running', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return MockData.addMockData(db, ['completed', 'completed', 'completed', 'running']);
        }).then(function () {
            return Manifest.fetch();
        }).then(function () {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then(function (buildRequests) {
            assert.equal(buildRequests.length, 4);
            assert.ok(buildRequests[0].hasFinished());
            assert.ok(buildRequests[0].hasStarted());
            assert.ok(!buildRequests[0].isPending());
            assert.ok(buildRequests[1].hasFinished());
            assert.ok(buildRequests[1].hasStarted());
            assert.ok(!buildRequests[1].isPending());
            assert.ok(buildRequests[2].hasFinished());
            assert.ok(buildRequests[2].hasStarted());
            assert.ok(!buildRequests[2].isPending());
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(buildRequests[3].hasStarted());
            assert.ok(!buildRequests[3].isPending());
            done();
        }).catch(done);
    });

    it('should order build requests based on test group and order', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return Promise.all([MockData.addMockData(db), MockData.addAnotherMockTestGroup(db)])
        }).then(function () {
            return Manifest.fetch();
        }).then(function () {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then(function (buildRequests) {
            assert.equal(buildRequests.length, 8);
            assert.equal(buildRequests[0].id(), 710);
            assert.equal(buildRequests[0].testGroupId(), 599);
            assert.equal(buildRequests[1].id(), 711);
            assert.equal(buildRequests[1].testGroupId(), 599);
            assert.equal(buildRequests[2].id(), 712);
            assert.equal(buildRequests[2].testGroupId(), 599);
            assert.equal(buildRequests[3].id(), 713);
            assert.equal(buildRequests[3].testGroupId(), 599);
            done();
        }).catch(done);
    });

});
