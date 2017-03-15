'use strict';

let assert = require('assert');

let MockData = require('./resources/mock-data.js');
let TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/build-requests', function () {
    prepareServerTest(this);

    it('should return "TriggerableNotFound" when the database is empty', () => {
        return TestServer.remoteAPI().getJSON('/api/build-requests/build-webkit').then((content) => {
            assert.equal(content['status'], 'TriggerableNotFound');
        });
    });

    it('should return an empty list when there are no build requests', () => {
        return TestServer.database().insert('build_triggerables', {name: 'build-webkit'}).then(() => {
            return TestServer.remoteAPI().getJSON('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.equal(content['status'], 'OK');
            assert.deepEqual(content['buildRequests'], []);
            assert.deepEqual(content['commitSets'], []);
            assert.deepEqual(content['commits'], []);
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status']);
        });
    });

    it('should return build requets associated with a given triggerable with appropriate commits and commitSets', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status']);

            assert.equal(content['commitSets'].length, 2);
            assert.equal(content['commitSets'][0].id, 401);
            assert.deepEqual(content['commitSets'][0].commits, ['87832', '93116']);
            assert.equal(content['commitSets'][1].id, 402);
            assert.deepEqual(content['commitSets'][1].commits, ['87832', '96336']);

            assert.equal(content['commits'].length, 3);
            assert.equal(content['commits'][0].id, 87832);
            assert.equal(content['commits'][0].repository, '9');
            assert.equal(content['commits'][0].revision, '10.11 15A284');
            assert.equal(content['commits'][1].id, 93116);
            assert.equal(content['commits'][1].repository, '11');
            assert.equal(content['commits'][1].revision, '191622');
            assert.equal(content['commits'][2].id, 96336);
            assert.equal(content['commits'][2].repository, '11');
            assert.equal(content['commits'][2].revision, '192736');

            assert.equal(content['buildRequests'].length, 4);
            assert.deepEqual(content['buildRequests'][0].id, 700);
            assert.deepEqual(content['buildRequests'][0].order, 0);
            assert.deepEqual(content['buildRequests'][0].platform, '65');
            assert.deepEqual(content['buildRequests'][0].commitSet, 401);
            assert.deepEqual(content['buildRequests'][0].status, 'pending');
            assert.deepEqual(content['buildRequests'][0].test, '200');

            assert.deepEqual(content['buildRequests'][1].id, 701);
            assert.deepEqual(content['buildRequests'][1].order, 1);
            assert.deepEqual(content['buildRequests'][1].platform, '65');
            assert.deepEqual(content['buildRequests'][1].commitSet, 402);
            assert.deepEqual(content['buildRequests'][1].status, 'pending');
            assert.deepEqual(content['buildRequests'][1].test, '200');

            assert.deepEqual(content['buildRequests'][2].id, 702);
            assert.deepEqual(content['buildRequests'][2].order, 2);
            assert.deepEqual(content['buildRequests'][2].platform, '65');
            assert.deepEqual(content['buildRequests'][2].commitSet, 401);
            assert.deepEqual(content['buildRequests'][2].status, 'pending');
            assert.deepEqual(content['buildRequests'][2].test, '200');

            assert.deepEqual(content['buildRequests'][3].id, 703);
            assert.deepEqual(content['buildRequests'][3].order, 3);
            assert.deepEqual(content['buildRequests'][3].platform, '65');
            assert.deepEqual(content['buildRequests'][3].commitSet, 402);
            assert.deepEqual(content['buildRequests'][3].status, 'pending');
            assert.deepEqual(content['buildRequests'][3].test, '200');
        });
    });

    it('should support useLegacyIdResolution option', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit?useLegacyIdResolution=true');
        }).then((content) => {
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status']);

            assert.equal(content['commitSets'].length, 2);
            assert.equal(content['commitSets'][0].id, 401);
            assert.deepEqual(content['commitSets'][0].commits, ['87832', '93116']);
            assert.equal(content['commitSets'][1].id, 402);
            assert.deepEqual(content['commitSets'][1].commits, ['87832', '96336']);

            assert.equal(content['commits'].length, 3);
            assert.equal(content['commits'][0].id, 87832);
            assert.equal(content['commits'][0].repository, 'OS X');
            assert.equal(content['commits'][0].revision, '10.11 15A284');
            assert.equal(content['commits'][1].id, 93116);
            assert.equal(content['commits'][1].repository, 'WebKit');
            assert.equal(content['commits'][1].revision, '191622');
            assert.equal(content['commits'][2].id, 96336);
            assert.equal(content['commits'][2].repository, 'WebKit');
            assert.equal(content['commits'][2].revision, '192736');

            assert.equal(content['buildRequests'].length, 4);
            assert.deepEqual(content['buildRequests'][0].id, 700);
            assert.deepEqual(content['buildRequests'][0].order, 0);
            assert.deepEqual(content['buildRequests'][0].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][0].commitSet, 401);
            assert.deepEqual(content['buildRequests'][0].status, 'pending');
            assert.deepEqual(content['buildRequests'][0].test, ['some test']);

            assert.deepEqual(content['buildRequests'][1].id, 701);
            assert.deepEqual(content['buildRequests'][1].order, 1);
            assert.deepEqual(content['buildRequests'][1].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][1].commitSet, 402);
            assert.deepEqual(content['buildRequests'][1].status, 'pending');
            assert.deepEqual(content['buildRequests'][1].test, ['some test']);

            assert.deepEqual(content['buildRequests'][2].id, 702);
            assert.deepEqual(content['buildRequests'][2].order, 2);
            assert.deepEqual(content['buildRequests'][2].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][2].commitSet, 401);
            assert.deepEqual(content['buildRequests'][2].status, 'pending');
            assert.deepEqual(content['buildRequests'][2].test, ['some test']);

            assert.deepEqual(content['buildRequests'][3].id, 703);
            assert.deepEqual(content['buildRequests'][3].order, 3);
            assert.deepEqual(content['buildRequests'][3].platform, 'some platform');
            assert.deepEqual(content['buildRequests'][3].commitSet, 402);
            assert.deepEqual(content['buildRequests'][3].status, 'pending');
            assert.deepEqual(content['buildRequests'][3].test, ['some test']);
        });
    });

    it('should be fetchable by BuildRequest.fetchForTriggerable', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
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
            assert.ok(buildRequests[0].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[0].hasFinished());
            assert.ok(!buildRequests[0].hasStarted());
            assert.ok(buildRequests[0].isPending());
            assert.equal(buildRequests[0].statusLabel(), 'Waiting');

            assert.equal(buildRequests[1].id(), 701);
            assert.equal(buildRequests[1].testGroupId(), 600);
            assert.equal(buildRequests[1].test(), test);
            assert.equal(buildRequests[1].platform(), platform);
            assert.equal(buildRequests[1].order(), 1);
            assert.ok(buildRequests[1].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[1].hasFinished());
            assert.ok(!buildRequests[1].hasStarted());
            assert.ok(buildRequests[1].isPending());
            assert.equal(buildRequests[1].statusLabel(), 'Waiting');

            assert.equal(buildRequests[2].id(), 702);
            assert.equal(buildRequests[2].testGroupId(), 600);
            assert.equal(buildRequests[2].test(), test);
            assert.equal(buildRequests[2].platform(), platform);
            assert.equal(buildRequests[2].order(), 2);
            assert.ok(buildRequests[2].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[2].hasFinished());
            assert.ok(!buildRequests[2].hasStarted());
            assert.ok(buildRequests[2].isPending());
            assert.equal(buildRequests[2].statusLabel(), 'Waiting');

            assert.equal(buildRequests[3].id(), 703);
            assert.equal(buildRequests[3].testGroupId(), 600);
            assert.equal(buildRequests[3].test(), test);
            assert.equal(buildRequests[3].platform(), platform);
            assert.equal(buildRequests[3].order(), 3);
            assert.ok(buildRequests[3].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(!buildRequests[3].hasStarted());
            assert.ok(buildRequests[3].isPending());
            assert.equal(buildRequests[3].statusLabel(), 'Waiting');

            let osx = Repository.findById(9);
            assert.equal(osx.name(), 'OS X');

            let webkit = Repository.findById(11);
            assert.equal(webkit.name(), 'WebKit');

            let firstCommitSet = buildRequests[0].commitSet();
            assert.equal(buildRequests[2].commitSet(), firstCommitSet);

            let secondCommitSet = buildRequests[1].commitSet();
            assert.equal(buildRequests[3].commitSet(), secondCommitSet);

            assert.equal(firstCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.equal(firstCommitSet.revisionForRepository(webkit), '191622');

            assert.equal(secondCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.equal(secondCommitSet.revisionForRepository(webkit), '192736');

            let osxCommit = firstCommitSet.commitForRepository(osx);
            assert.equal(osxCommit.revision(), '10.11 15A284');
            assert.equal(osxCommit, secondCommitSet.commitForRepository(osx));

            let firstWebKitCommit = firstCommitSet.commitForRepository(webkit);
            assert.equal(firstWebKitCommit.revision(), '191622');
            assert.equal(+firstWebKitCommit.time(), 1445945816878);

            let secondWebKitCommit = secondCommitSet.commitForRepository(webkit);
            assert.equal(secondWebKitCommit.revision(), '192736');
            assert.equal(+secondWebKitCommit.time(), 1448225325650);
        });
    });

    it('should not include a build request if all requests in the same group had been completed', () => {
        return MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.equal(buildRequests.length, 0);
        });
    });

    it('should not include a build request if all requests in the same group had been failed or cancled', () => {
        return MockData.addMockData(TestServer.database(), ['failed', 'failed', 'canceled', 'canceled']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.equal(buildRequests.length, 0);
        });
    });

    it('should include all build requests of a test group if one of the reqeusts in the group had not been finished', () => {
        return MockData.addMockData(TestServer.database(), ['completed', 'completed', 'scheduled', 'pending']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
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
        });
    });

    it('should include all build requests of a test group if one of the reqeusts in the group is still running', () => {
        return MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'running']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
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
        });
    });

    it('should order build requests based on test group creation time and order', () => {
        let db = TestServer.database();
        return Promise.all([MockData.addMockData(db), MockData.addAnotherMockTestGroup(db)]).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.equal(buildRequests.length, 8);
            assert.equal(buildRequests[0].id(), 700);
            assert.equal(buildRequests[0].testGroupId(), 600);
            assert.strictEqual(buildRequests[0].order(), 0);
            assert.equal(buildRequests[1].id(), 701);
            assert.equal(buildRequests[1].testGroupId(), 600);
            assert.strictEqual(buildRequests[1].order(), 1);
            assert.equal(buildRequests[2].id(), 702);
            assert.equal(buildRequests[2].testGroupId(), 600);
            assert.strictEqual(buildRequests[2].order(), 2);
            assert.equal(buildRequests[3].id(), 703);
            assert.equal(buildRequests[3].testGroupId(), 600);
            assert.strictEqual(buildRequests[3].order(), 3);

            assert.equal(buildRequests[4].id(), 710);
            assert.equal(buildRequests[4].testGroupId(), 601);
            assert.strictEqual(buildRequests[4].order(), 0);
            assert.equal(buildRequests[5].id(), 711);
            assert.equal(buildRequests[5].testGroupId(), 601);
            assert.strictEqual(buildRequests[5].order(), 1);
            assert.equal(buildRequests[6].id(), 712);
            assert.equal(buildRequests[6].testGroupId(), 601);
            assert.strictEqual(buildRequests[6].order(), 2);
            assert.equal(buildRequests[7].id(), 713);
            assert.equal(buildRequests[7].testGroupId(), 601);
            assert.strictEqual(buildRequests[7].order(), 3);
        });
    });

    it('should place build requests created by user before automatically created ones', () => {
        let db = TestServer.database();
        return Promise.all([MockData.addMockData(db), MockData.addAnotherMockTestGroup(db, null, 'rniwa')]).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.equal(buildRequests.length, 8);
            assert.equal(buildRequests[0].id(), 710);
            assert.equal(buildRequests[0].testGroupId(), 601);
            assert.strictEqual(buildRequests[0].order(), 0);
            assert.equal(buildRequests[1].id(), 711);
            assert.equal(buildRequests[1].testGroupId(), 601);
            assert.strictEqual(buildRequests[1].order(), 1);
            assert.equal(buildRequests[2].id(), 712);
            assert.equal(buildRequests[2].testGroupId(), 601);
            assert.strictEqual(buildRequests[2].order(), 2);
            assert.equal(buildRequests[3].id(), 713);
            assert.equal(buildRequests[3].testGroupId(), 601);
            assert.strictEqual(buildRequests[3].order(), 3);

            assert.equal(buildRequests[4].id(), 700);
            assert.equal(buildRequests[4].testGroupId(), 600);
            assert.strictEqual(buildRequests[4].order(), 0);
            assert.equal(buildRequests[5].id(), 701);
            assert.equal(buildRequests[5].testGroupId(), 600);
            assert.strictEqual(buildRequests[5].order(), 1);
            assert.equal(buildRequests[6].id(), 702);
            assert.equal(buildRequests[6].testGroupId(), 600);
            assert.strictEqual(buildRequests[6].order(), 2);
            assert.equal(buildRequests[7].id(), 703);
            assert.equal(buildRequests[7].testGroupId(), 600);
            assert.strictEqual(buildRequests[7].order(), 3);
        });
    });
});
