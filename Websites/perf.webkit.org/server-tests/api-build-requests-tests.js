'use strict';

let assert = require('assert');
let TestServer = require('./resources/test-server.js');

describe('/api/build-requests', function () {
    this.timeout(10000);

    it('should return "TriggerableNotFound" when the database is empty', function (done) {
        TestServer.remoteAPI().fetchJSON('/api/build-requests/build-webkit').then(function (content) {
            assert.equal(content['status'], 'TriggerableNotFound');
            done();
        }).catch(done);
    });

    it('should return an empty list when there are no build requests', function (done) {
        TestServer.database().connect().then(function () {
            return TestServer.database().insert('build_triggerables', {name: 'build-webkit'});
        }).then(function () {
            return TestServer.remoteAPI().fetchJSON('/api/build-requests/build-webkit');
        }).then(function (content) {
            assert.equal(content['status'], 'OK');
            assert.deepEqual(content['buildRequests'], []);
            assert.deepEqual(content['rootSets'], []);
            assert.deepEqual(content['roots'], []);
            assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'rootSets', 'roots', 'status']);
            done();
        }).catch(done);
    });

    function addMockData(db)
    {
        return Promise.all([
            db.insert('build_triggerables', {id: 1, name: 'build-webkit'}),
            db.insert('repositories', {id: 9, name: 'OS X'}),
            db.insert('repositories', {id: 11, name: 'WebKit'}),
            db.insert('commits', {id: 87832, repository: 9, revision: '10.11 15A284'}),
            db.insert('commits', {id: 93116, repository: 11, revision: '191622', time: new Date(1445945816878)}),
            db.insert('commits', {id: 96336, repository: 11, revision: '192736', time: new Date(1448225325650)}),
            db.insert('platforms', {id: 65, name: 'some platform'}),
            db.insert('tests', {id: 200, name: 'some test'}),
            db.insert('test_metrics', {id: 300, test: 200, name: 'some metric'}),
            db.insert('root_sets', {id: 401}),
            db.insert('roots', {set: 401, commit: 87832}),
            db.insert('roots', {set: 401, commit: 93116}),
            db.insert('root_sets', {id: 402}),
            db.insert('roots', {set: 402, commit: 87832}),
            db.insert('roots', {set: 402, commit: 96336}),
            db.insert('analysis_tasks', {id: 500, platform: 65, metric: 300, name: 'some task'}),
            db.insert('analysis_test_groups', {id: 600, task: 500, name: 'some test group'}),
            db.insert('build_requests', {id: 700, triggerable: 1, platform: 65, test: 200, group: 600, order: 0, root_set: 401}),
            db.insert('build_requests', {id: 701, triggerable: 1, platform: 65, test: 200, group: 600, order: 1, root_set: 402}),
            db.insert('build_requests', {id: 702, triggerable: 1, platform: 65, test: 200, group: 600, order: 2, root_set: 401}),
            db.insert('build_requests', {id: 703, triggerable: 1, platform: 65, test: 200, group: 600, order: 3, root_set: 402}),
        ]);
    }

    it('should return build requets associated with a given triggerable with appropriate roots and rootSets', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return addMockData(db);
        }).then(function () {
            return TestServer.remoteAPI().fetchJSONWithStatus('/api/build-requests/build-webkit');
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

    it('should return support useLegacyIdResolution option', function (done) {
        let db = TestServer.database();
        db.connect().then(function () {
            return addMockData(db);
        }).then(function () {
            return TestServer.remoteAPI().fetchJSONWithStatus('/api/build-requests/build-webkit?useLegacyIdResolution=true');
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

});
