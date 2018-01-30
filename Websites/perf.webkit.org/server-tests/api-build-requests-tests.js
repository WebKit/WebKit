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
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'uploadedFiles']);
        });
    });

    it('should return build requests associated with a given triggerable with appropriate commits and commitSets with owned components', () => {
        return MockData.addTestGroupWithOwnedCommits(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'uploadedFiles']);

            assert.equal(content['commitSets'].length, 2);
            assert.equal(content['commitSets'][0].id, 403);
            assert.deepEqual(content['commitSets'][0].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '1797', commitOwner: "93116", patch: null, requiresBuild: true, rootFile: null}]);
            assert.equal(content['commitSets'][1].id, 404);
            assert.deepEqual(content['commitSets'][1].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '2017', commitOwner: "96336", patch: null, requiresBuild: true, rootFile: null}]);

            assert.equal(content['commits'].length, 5);
            assert.deepEqual(content['commits'][0], {commitOwner: null, id: 87832, repository: '9', revision: '10.11 15A284', time: 0});
            assert.deepEqual(content['commits'][1], {commitOwner: null, id: 93116, repository: '11', revision: '191622', time: 1445945816878})
            assert.deepEqual(content['commits'][2], {commitOwner: '93116', id: '1797', repository: '213', revision: 'owned-jsc-6161', time: 1456960795300})
            assert.deepEqual(content['commits'][3], {commitOwner: null, id: 96336, repository: '11', revision: '192736', time: 1448225325650})
            assert.deepEqual(content['commits'][4], {commitOwner: '96336', id: 2017, repository: '213', revision: 'owned-jsc-9191', time: 1462230837100})

            assert.equal(content['buildRequests'].length, 4);
            content['buildRequests'][0]['createdAt'] = 0;
            content['buildRequests'][1]['createdAt'] = 0;
            content['buildRequests'][2]['createdAt'] = 0;
            content['buildRequests'][3]['createdAt'] = 0;
            assert.deepEqual(content['buildRequests'][0], {id: '704', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '0', commitSet: '403', status: 'pending', url: null, build: null, createdAt: 0});
            assert.deepEqual(content['buildRequests'][1], {id: '705', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '1', commitSet: '404', status: 'pending', url: null, build: null, createdAt: 0});
            assert.deepEqual(content['buildRequests'][2], {id: '706', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '2', commitSet: '403', status: 'pending', url: null, build: null, createdAt: 0});
            assert.deepEqual(content['buildRequests'][3], {id: '707', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '3', commitSet: '404', status: 'pending', url: null, build: null, createdAt: 0});
       });
    });

    it('should return build requests associated with a given triggerable with appropriate commits and commitSets', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'uploadedFiles']);

            assert.equal(content['commitSets'].length, 2);
            assert.equal(content['commitSets'][0].id, 401);
            assert.deepEqual(content['commitSets'][0].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
            assert.equal(content['commitSets'][1].id, 402);
            assert.deepEqual(content['commitSets'][1].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);

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
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'uploadedFiles']);

            assert.equal(content['commitSets'].length, 2);
            assert.equal(content['commitSets'][0].id, 401);
            assert.deepEqual(content['commitSets'][0].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
            assert.equal(content['commitSets'][1].id, 402);
            assert.deepEqual(content['commitSets'][1].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);

            assert.equal(content['commits'].length, 3);
            assert.equal(content['commits'][0].id, 87832);
            assert.equal(content['commits'][0].repository, 'macOS');
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

    it('should be fetchable by BuildRequest.fetchForTriggerable for commitSets with owned commits', () => {
        return MockData.addTestGroupWithOwnedCommits(TestServer.database()).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.equal(buildRequests.length, 4);

            let test = Test.findById(200);
            assert(test);

            let platform = Platform.findById(65);
            assert(platform);

            assert.equal(buildRequests[0].id(), 704);
            assert.equal(buildRequests[0].testGroupId(), 900);
            assert.equal(buildRequests[0].test(), test);
            assert.equal(buildRequests[0].platform(), platform);
            assert.equal(buildRequests[0].order(), 0);
            assert.ok(buildRequests[0].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[0].hasFinished());
            assert.ok(!buildRequests[0].hasStarted());
            assert.ok(buildRequests[0].isPending());
            assert.equal(buildRequests[0].statusLabel(), 'Waiting');

            assert.equal(buildRequests[1].id(), 705);
            assert.equal(buildRequests[1].testGroupId(), 900);
            assert.equal(buildRequests[1].test(), test);
            assert.equal(buildRequests[1].platform(), platform);
            assert.equal(buildRequests[1].order(), 1);
            assert.ok(buildRequests[1].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[1].hasFinished());
            assert.ok(!buildRequests[1].hasStarted());
            assert.ok(buildRequests[1].isPending());
            assert.equal(buildRequests[1].statusLabel(), 'Waiting');

            assert.equal(buildRequests[2].id(), 706);
            assert.equal(buildRequests[2].testGroupId(), 900);
            assert.equal(buildRequests[2].test(), test);
            assert.equal(buildRequests[2].platform(), platform);
            assert.equal(buildRequests[2].order(), 2);
            assert.ok(buildRequests[2].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[2].hasFinished());
            assert.ok(!buildRequests[2].hasStarted());
            assert.ok(buildRequests[2].isPending());
            assert.equal(buildRequests[2].statusLabel(), 'Waiting');

            assert.equal(buildRequests[3].id(), 707);
            assert.equal(buildRequests[3].testGroupId(), 900);
            assert.equal(buildRequests[3].test(), test);
            assert.equal(buildRequests[3].platform(), platform);
            assert.equal(buildRequests[3].order(), 3);
            assert.ok(buildRequests[3].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(!buildRequests[3].hasStarted());
            assert.ok(buildRequests[3].isPending());
            assert.equal(buildRequests[3].statusLabel(), 'Waiting');

            const osx = Repository.findById(9);
            assert.equal(osx.name(), 'macOS');

            const webkit = Repository.findById(11);
            assert.equal(webkit.name(), 'WebKit');

            const jsc = Repository.findById(213);
            assert.equal(jsc.name(), 'JavaScriptCore');

            const firstCommitSet = buildRequests[0].commitSet();
            assert.equal(buildRequests[2].commitSet(), firstCommitSet);

            const secondCommitSet = buildRequests[1].commitSet();
            assert.equal(buildRequests[3].commitSet(), secondCommitSet);

            assert.equal(firstCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.equal(firstCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(firstCommitSet.revisionForRepository(jsc), 'owned-jsc-6161');
            assert.equal(firstCommitSet.ownerRevisionForRepository(jsc), '191622');

            assert.equal(secondCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.equal(secondCommitSet.revisionForRepository(webkit), '192736');
            assert.equal(secondCommitSet.revisionForRepository(jsc), 'owned-jsc-9191');
            assert.equal(secondCommitSet.ownerRevisionForRepository(jsc), '192736');

            const osxCommit = firstCommitSet.commitForRepository(osx);
            assert.equal(osxCommit.revision(), '10.11 15A284');
            assert.equal(osxCommit, secondCommitSet.commitForRepository(osx));

            const firstWebKitCommit = firstCommitSet.commitForRepository(webkit);
            assert.equal(firstWebKitCommit.revision(), '191622');
            assert.equal(+firstWebKitCommit.time(), 1445945816878);

            const secondWebKitCommit = secondCommitSet.commitForRepository(webkit);
            assert.equal(secondWebKitCommit.revision(), '192736');
            assert.equal(+secondWebKitCommit.time(), 1448225325650);

            const firstSJCCommit = firstCommitSet.commitForRepository(jsc);
            assert.equal(firstSJCCommit.revision(), 'owned-jsc-6161');
            assert.equal(+firstSJCCommit.time(), 1456960795300);

            const secondSJCCommit = secondCommitSet.commitForRepository(jsc);
            assert.equal(secondSJCCommit.revision(), 'owned-jsc-9191');
            assert.equal(+secondSJCCommit.time(), 1462230837100)
        });
    });

    it('a repository group of a build request should accepts the commit set of the same build request', async () => {
        await MockData.addTestGroupWithOwnerCommitNotInCommitSet(TestServer.database());
        await Manifest.fetch();
        const buildRequests = await BuildRequest.fetchForTriggerable('build-webkit');
        assert.equal(buildRequests.length, 1);

        const test = Test.findById(200);
        assert(test);

        const platform = Platform.findById(65);
        assert(platform);

        const buildRequest = buildRequests[0];

        assert.equal(buildRequest.id(), 704);
        assert.equal(buildRequest.testGroupId(), 900);
        assert.equal(buildRequest.test(), test);
        assert.equal(buildRequest.platform(), platform);
        assert.equal(buildRequest.order(), 0);
        assert.ok(buildRequest.commitSet() instanceof CommitSet);
        assert.ok(!buildRequest.hasFinished());
        assert.ok(!buildRequest.hasStarted());
        assert.ok(buildRequest.isPending());
        assert.equal(buildRequest.statusLabel(), 'Waiting');

        const osx = Repository.findById(9);
        assert.equal(osx.name(), 'macOS');

        const webkit = Repository.findById(11);
        assert.equal(webkit.name(), 'WebKit');

        const jsc = Repository.findById(213);
        assert.equal(jsc.name(), 'JavaScriptCore');

        const commitSet = buildRequest.commitSet();

        assert.equal(commitSet.revisionForRepository(osx), '10.11 15A284');
        assert.equal(commitSet.revisionForRepository(webkit), '192736');
        assert.equal(commitSet.revisionForRepository(jsc), 'owned-jsc-9191');
        assert.equal(commitSet.ownerRevisionForRepository(jsc), '191622');
        assert.deepEqual(commitSet.topLevelRepositories().sort((one, another) => one.id() < another.id()), [osx, webkit]);
        assert.ok(buildRequest.repositoryGroup().accepts(commitSet));
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
            assert.equal(osx.name(), 'macOS');

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

    it('should specify the repository group for build requests if set', () => {
        const db = TestServer.database();
        let groups;
        return MockData.addMockData(db).then(() => {
            return MockData.addMockTestGroupWithGitWebKit(db);
        }).then(() => {
            return Manifest.fetch();
        }).then(() => {
            const triggerable = Triggerable.all().find((triggerable) => triggerable.name() == 'build-webkit');
            assert.equal(triggerable.repositoryGroups().length, 2);
            groups = TriggerableRepositoryGroup.sortByName(triggerable.repositoryGroups());
            assert.equal(groups[0].name(), 'webkit-git');
            assert.equal(groups[1].name(), 'webkit-svn');
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.equal(buildRequests.length, 8);
            assert.equal(buildRequests[0].id(), 700);
            assert.equal(buildRequests[0].repositoryGroup(), groups[1]);
            assert.equal(buildRequests[1].id(), 701);
            assert.equal(buildRequests[1].repositoryGroup(), groups[1]);
            assert.equal(buildRequests[2].id(), 702);
            assert.equal(buildRequests[2].repositoryGroup(), groups[1]);
            assert.equal(buildRequests[3].id(), 703);
            assert.equal(buildRequests[3].repositoryGroup(), groups[1]);

            assert.equal(buildRequests[4].id(), 1700);
            assert.equal(buildRequests[4].repositoryGroup(), groups[0]);
            assert.equal(buildRequests[5].id(), 1701);
            assert.equal(buildRequests[5].repositoryGroup(), groups[0]);
            assert.equal(buildRequests[6].id(), 1702);
            assert.equal(buildRequests[6].repositoryGroup(), groups[0]);
            assert.equal(buildRequests[7].id(), 1703);
            assert.equal(buildRequests[7].repositoryGroup(), groups[0]);
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

    it('should not return "UnknownBuildRequestStatus" when updating a canceled build request', () => {
        const updates = {'700': {status: 'canceled', url: 'http://build.webkit.org/someBuilder/builds'}};
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
                'slaveName': 'sync-slave',
                'slavePassword': 'password',
                'buildRequestUpdates': updates
            }).then((response) => {
                assert.equal(response['status'], 'OK');

                assert.equal(response['buildRequests'].length, 4);
                assert.deepEqual(response['buildRequests'][0].id, 700);
                assert.deepEqual(response['buildRequests'][0].order, 0);
                assert.deepEqual(response['buildRequests'][0].platform, '65');
                assert.deepEqual(response['buildRequests'][0].commitSet, 401);
                assert.deepEqual(response['buildRequests'][0].status, 'canceled');
                assert.deepEqual(response['buildRequests'][0].test, '200');

                assert.deepEqual(response['buildRequests'][1].id, 701);
                assert.deepEqual(response['buildRequests'][1].order, 1);
                assert.deepEqual(response['buildRequests'][1].platform, '65');
                assert.deepEqual(response['buildRequests'][1].commitSet, 402);
                assert.deepEqual(response['buildRequests'][1].status, 'pending');
                assert.deepEqual(response['buildRequests'][1].test, '200');

                assert.deepEqual(response['buildRequests'][2].id, 702);
                assert.deepEqual(response['buildRequests'][2].order, 2);
                assert.deepEqual(response['buildRequests'][2].platform, '65');
                assert.deepEqual(response['buildRequests'][2].commitSet, 401);
                assert.deepEqual(response['buildRequests'][2].status, 'pending');
                assert.deepEqual(response['buildRequests'][2].test, '200');

                assert.deepEqual(response['buildRequests'][3].id, 703);
                assert.deepEqual(response['buildRequests'][3].order, 3);
                assert.deepEqual(response['buildRequests'][3].platform, '65');
                assert.deepEqual(response['buildRequests'][3].commitSet, 402);
                assert.deepEqual(response['buildRequests'][3].status, 'pending');
                assert.deepEqual(response['buildRequests'][3].test, '200');
            }, () => {
                assert(false, 'Should not be reached');
            });
        });
    });
});
