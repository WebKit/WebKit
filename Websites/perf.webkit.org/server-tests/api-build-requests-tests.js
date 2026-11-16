'use strict';

let assert = require('assert');

let MockData = require('./resources/mock-data.js');
let TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const assertThrows = require('../server-tests/resources/common-operations').assertThrows;
const crypto = require('crypto');

describe('/api/build-requests', function () {
    prepareServerTest(this);

    it('should return "TriggerableNotFound" when the database is empty', () => {
        return TestServer.remoteAPI().getJSON('/api/build-requests/build-webkit').then((content) => {
            assert.strictEqual(content['status'], 'TriggerableNotFound');
        });
    });

    it('should return an empty list when there are no build requests', () => {
        return TestServer.database().insert('build_triggerables', {name: 'build-webkit'}).then(() => {
            return TestServer.remoteAPI().getJSON('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.strictEqual(content['status'], 'OK');
            assert.deepStrictEqual(content['buildRequests'], []);
            assert.deepStrictEqual(content['commitSets'], []);
            assert.deepStrictEqual(content['commits'], []);
            assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        });
    });

    it('should return build requests associated with a given triggerable with appropriate commits and commitSets with owned components', () => {
        return MockData.addTestGroupWithOwnedCommits(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);

            assert.strictEqual(content['commitSets'].length, 2);
            assert.strictEqual(parseInt(content['commitSets'][0].id), 403);
            assert.deepStrictEqual(content['commitSets'][0].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '1797', commitOwner: "93116", patch: null, requiresBuild: true, rootFile: null}]);
            assert.strictEqual(parseInt(content['commitSets'][1].id), 404);
            assert.deepStrictEqual(content['commitSets'][1].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
                {commit: '2017', commitOwner: "96336", patch: null, requiresBuild: true, rootFile: null}]);

            assert.strictEqual(content['commits'].length, 5);
            assert.deepStrictEqual(content['commits'][0], {commitOwner: null, id: '87832', repository: '9', revision: '10.11 15A284', revisionIdentifier: null, time: 0});
            assert.deepStrictEqual(content['commits'][1], {commitOwner: null, id: '93116', repository: '11', revision: '191622', revisionIdentifier: null, time: 1445945816878})
            assert.deepStrictEqual(content['commits'][2], {commitOwner: '93116', id: '1797', repository: '213', revision: 'owned-jsc-6161', revisionIdentifier: null, time: 1456960795300})
            assert.deepStrictEqual(content['commits'][3], {commitOwner: null, id: '96336', repository: '11', revision: '192736', revisionIdentifier: null, time: 1448225325650})
            assert.deepStrictEqual(content['commits'][4], {commitOwner: '96336', id: '2017', repository: '213', revision: 'owned-jsc-9191', revisionIdentifier: null, time: 1462230837100})

            assert.strictEqual(content['buildRequests'].length, 4);
            content['buildRequests'][0]['createdAt'] = 0;
            content['buildRequests'][1]['createdAt'] = 0;
            content['buildRequests'][2]['createdAt'] = 0;
            content['buildRequests'][3]['createdAt'] = 0;
            assert.deepStrictEqual(content['buildRequests'][0], {id: '704', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '0', commitSet: '403', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
            assert.deepStrictEqual(content['buildRequests'][1], {id: '705', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '1', commitSet: '404', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
            assert.deepStrictEqual(content['buildRequests'][2], {id: '706', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '2', commitSet: '403', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
            assert.deepStrictEqual(content['buildRequests'][3], {id: '707', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '3', commitSet: '404', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
       });
    });

    it('should return build requests associated with a given triggerable with appropriate commits with commit revision identifier and commitSets with owned components', async () => {
        await MockData.addTestGroupWithOwnedCommitsWithRevisionIdentifier(TestServer.database())
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 2);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 403);
        assert.deepStrictEqual(content['commitSets'][0].revisionItems,
            [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '1797', commitOwner: "193116", patch: null, requiresBuild: true, rootFile: null},
            {commit: '193116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},]);
        assert.strictEqual(parseInt(content['commitSets'][1].id), 404);
        assert.deepStrictEqual(content['commitSets'][1].revisionItems,
            [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '2017', commitOwner: "196336", patch: null, requiresBuild: true, rootFile: null},
            {commit: '196336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},]);

        assert.strictEqual(content['commits'].length, 5);
        assert.deepStrictEqual(content['commits'][0], {commitOwner: null, id: '87832', repository: '9', revision: '10.11 15A284', revisionIdentifier: null, time: 0});
        assert.deepStrictEqual(content['commits'][1], {commitOwner: '193116', id: '1797', repository: '213', revision: 'owned-jsc-6161', revisionIdentifier: null, time: 1456960795300})
        assert.deepStrictEqual(content['commits'][2], {commitOwner: null, id: '193116', repository: String(MockData.gitWebkitRepositoryId()), revision: '2ceda45d3cd63cde58d0dbf5767714e03d902e43', revisionIdentifier: '193116@main', time: 1445945816878})
        assert.deepStrictEqual(content['commits'][3], {commitOwner: '196336', id: '2017', repository: '213', revision: 'owned-jsc-9191', revisionIdentifier: null, time: 1462230837100})
        assert.deepStrictEqual(content['commits'][4], {commitOwner: null, id: '196336', repository: String(MockData.gitWebkitRepositoryId()), revision: '8e294365a452a89785d6536ca7f0fc8a95fa152d', revisionIdentifier: '196336@main', time: 1448225325650})

        assert.strictEqual(content['buildRequests'].length, 4);
        content['buildRequests'][0]['createdAt'] = 0;
        content['buildRequests'][1]['createdAt'] = 0;
        content['buildRequests'][2]['createdAt'] = 0;
        content['buildRequests'][3]['createdAt'] = 0;
        assert.deepStrictEqual(content['buildRequests'][0], {id: '704', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '0', commitSet: '403', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
        assert.deepStrictEqual(content['buildRequests'][1], {id: '705', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '1', commitSet: '404', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
        assert.deepStrictEqual(content['buildRequests'][2], {id: '706', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '2', commitSet: '403', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
        assert.deepStrictEqual(content['buildRequests'][3], {id: '707', task: '1080', triggerable: '1000', repositoryGroup: '2001', test: '200', platform: '65', testGroup: '900', order: '3', commitSet: '404', status: 'pending', statusDescription: null, url: null, build: null, createdAt: 0, testParameterSet: null});
    });

    it('reuse roots from existing build requests if the commits sets are equal except the existence of roots', async () => {
        await MockData.addMockBuildRequestsWithRoots(TestServer.database());
        let content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'pending');
        assert.strictEqual(content['buildRequests'][4].url, null);

        const updates = {900: {
            status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: parseInt(content['buildRequests'][0].id)}};
        const response = await TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        });

        content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);
        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'completed');
        assert.strictEqual(content['buildRequests'][4].url, 'http://build.webkit.org/buids/1');
    });

    it('should reuse root built for owned commit if same completed build request exists', async () => {
        await MockData.addTwoMockTestGroupWithOwnedCommits(TestServer.database());
        let content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 403);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 405);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '1797', commitOwner: '93116', patch: null, requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '1797', commitOwner: '93116', patch: null, requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 704);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 403);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 708);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 405);
        assert.strictEqual(content['buildRequests'][4].status, 'pending');
        assert.strictEqual(content['buildRequests'][4].url, null);

        const updates = {708: {
            status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: 704}};

        const response = await TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        });

        content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 403);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 405);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '1797', commitOwner: '93116', patch: null, requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '1797', commitOwner: '93116', patch: null, requiresBuild: true, rootFile: '101'}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 704);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 403);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 708);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 405);
        assert.strictEqual(content['buildRequests'][4].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
    });

    it('reuse roots from existing build requests if the commits sets are equal except the existence of custom roots', async () => {
        const db = TestServer.database();
        await MockData.addMockBuildRequestsWithRoots(db);
        await Promise.all([
            db.insert('uploaded_files', {id: 103, filename: 'custom-root-103', extension: '.tgz', size: 1, sha256: crypto.createHash('sha256').update('custom-root-103').digest('hex')}),
            db.insert('commit_set_items', {set: 500, commit: null, patch_file: null, requires_build: false, root_file: 103}),
            db.insert('uploaded_files', {id: 104, filename: 'custom-root-104', extension: '.tgz', size: 1, sha256: crypto.createHash('sha256').update('custom-root-104').digest('hex')}),
            db.insert('commit_set_items', {set: 600, commit: null, patch_file: null, requires_build: false, root_file: 104}),
        ]);
        let content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);
        assert.deepStrictEqual(content['commitSets'][0].customRoots, ['103']);
        assert.deepStrictEqual(content['commitSets'][2].customRoots, ['104']);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'pending');
        assert.strictEqual(content['buildRequests'][4].url, null);

        const updates = {900: {
                status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: parseInt(content['buildRequests'][0].id)}};
        const response = await TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        });

        content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);
        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][0].customRoots, ['103']);
        assert.deepStrictEqual(content['commitSets'][2].customRoots, ['104']);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'completed');
        assert.strictEqual(content['buildRequests'][4].url, 'http://build.webkit.org/buids/1');
    });

    it('should fail request with "CannotReuseDeletedRoot" if any root to reuse is deleted', async () => {
        await MockData.addMockBuildRequestsWithRoots(TestServer.database());
        await TestServer.database().query("UPDATE uploaded_files SET file_deleted_at = now() at time zone 'utc' WHERE file_id=101");
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.deepEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.equal(content['commitSets'].length, 4);
        assert.equal(content['commitSets'][0].id, 500);
        assert.equal(content['commitSets'][2].id, 600);

        assert.deepEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: 100, requiresBuild: true, rootFile: 101}]);
        assert.ok(content['uploadedFiles'][1].deletedAt);
        assert.deepEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: 100, requiresBuild: true, rootFile: null}]);

        assert.equal(content['buildRequests'].length, 8);
        assert.equal(content['buildRequests'][0].id, 800);
        assert.equal(content['buildRequests'][0].commitSet, 500);
        assert.equal(content['buildRequests'][0].status, 'completed');
        assert.equal(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.equal(content['buildRequests'][4].id, 900);
        assert.equal(content['buildRequests'][4].commitSet, 600);
        assert.equal(content['buildRequests'][4].status, 'pending');
        assert.equal(content['buildRequests'][4].url, null);

        const updates = {900: {
            status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: content['buildRequests'][0].id}};
        await assertThrows('CannotReuseDeletedRoot', () => TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        }));
    });

    it('should fail request with "CannotReuseDeletedRoot" if any root to reuse is deleted while updating commit set items ', async () => {
        await MockData.addMockBuildRequestsWithRoots(TestServer.database());
        await TestServer.database().query(`CREATE OR REPLACE FUNCTION emulate_file_purge() RETURNS TRIGGER AS $emulate_file_purge$
            BEGIN
                UPDATE uploaded_files SET file_deleted_at = (CURRENT_TIMESTAMP AT TIME ZONE 'UTC') WHERE file_id = NEW.commitset_root_file;
                RETURN NULL;
            END;
            $emulate_file_purge$ LANGUAGE plpgsql;`);
        await TestServer.database().query(`CREATE TRIGGER emulate_file_purge AFTER UPDATE OF commitset_root_file ON commit_set_items
            FOR EACH ROW EXECUTE PROCEDURE emulate_file_purge();`);
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'pending');
        assert.strictEqual(content['buildRequests'][4].url, null);

        const updates = {900: {
            status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: parseInt(content['buildRequests'][0].id)}};

        await assertThrows('CannotReuseDeletedRoot', () => TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        }));
    });

    it('should fail request with "NoMatchingCommitSetItem" if build request to reuse does not have patch', async () => {
        await MockData.addMockData(TestServer.database(), ['completed', 'pending', 'pending', 'pending'], true, ['http://build.webkit.org/buids/2', null, null, null]);
        await MockData.addMockBuildRequestsWithRoots(TestServer.database(), ['running', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending'], true, false);
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.strictEqual(content['commitSets'].length, 6);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 401);
        assert.strictEqual(parseInt(content['commitSets'][4].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
        assert.deepStrictEqual(content['commitSets'][4].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 12);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 700);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 401);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/2');
        assert.strictEqual(parseInt(content['buildRequests'][8].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][8].commitSet), 600);
        assert.strictEqual(content['buildRequests'][8].status, 'pending');
        assert.strictEqual(content['buildRequests'][8].url, null);

        const updates = {900: {
                status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: parseInt(content['buildRequests'][0].id)}};

        await assertThrows('NoMatchingCommitSetItem', () => TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        }));
    });

    it('should fail request with "CannotReuseRootWithNonMatchingCommitSets" if commit sets have different number of entries', async () => {
        await MockData.addMockData(TestServer.database(), ['completed', 'pending', 'pending', 'pending'], true, ['http://build.webkit.org/buids/2', null, null, null]);
        await MockData.addMockBuildRequestsWithRoots(TestServer.database(), ['running', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending'], true, false);
        await TestServer.database().insert('commit_set_items', {set: 401, commit: 111168})
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.strictEqual(content['commitSets'].length, 6);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 401);
        assert.strictEqual(parseInt(content['commitSets'][4].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '111168', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
        assert.deepStrictEqual(content['commitSets'][4].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 12);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 700);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 401);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/2');
        assert.strictEqual(parseInt(content['buildRequests'][8].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][8].commitSet), 600);
        assert.strictEqual(content['buildRequests'][8].status, 'pending');
        assert.strictEqual(content['buildRequests'][8].url, null);

        const updates = {900: {
                status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: parseInt(content['buildRequests'][0].id)}};

        await assertThrows('CannotReuseRootWithNonMatchingCommitSets', () => TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        }));
    });

    it('should fail request with "CanOnlyReuseCompletedBuildRequest" if build request to reuse is not completed', async () => {
        await MockData.addMockBuildRequestsWithRoots(TestServer.database(), ['running', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending', 'pending']);
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'running');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'pending');
        assert.strictEqual(content['buildRequests'][4].url, null);

        const updates = {900: {
            status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: parseInt(content['buildRequests'][0].id)}};
        await assertThrows('CanOnlyReuseCompletedBuildRequest', () => TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        }));
    });

    it('should fail request with "FailedToFindReuseBuildRequest" if the build request to reuse does not exist', async () => {
        await MockData.addMockBuildRequestsWithRoots(TestServer.database());
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);
        assert.strictEqual(content['commitSets'].length, 4);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 500);
        assert.strictEqual(parseInt(content['commitSets'][2].id), 600);

        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: '101'}]);
        assert.deepStrictEqual(content['commitSets'][2].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: '100', requiresBuild: true, rootFile: null}]);

        assert.strictEqual(content['buildRequests'].length, 8);
        assert.strictEqual(parseInt(content['buildRequests'][0].id), 800);
        assert.strictEqual(parseInt(content['buildRequests'][0].commitSet), 500);
        assert.strictEqual(content['buildRequests'][0].status, 'completed');
        assert.strictEqual(content['buildRequests'][0].url, 'http://build.webkit.org/buids/1');
        assert.strictEqual(parseInt(content['buildRequests'][4].id), 900);
        assert.strictEqual(parseInt(content['buildRequests'][4].commitSet), 600);
        assert.strictEqual(content['buildRequests'][4].status, 'pending');
        assert.strictEqual(content['buildRequests'][4].url, null);

        const updates = {900: {
            status: content['buildRequests'][0].status, url: content['buildRequests'][0].url, buildRequestForRootReuse: 999}};

        await assertThrows('FailedToFindbuildRequestForRootReuse', () => TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        }));
    });

    it('should return build requests associated with a given triggerable with appropriate commits and commitSets', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        }).then((content) => {
            assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);

            assert.strictEqual(content['commitSets'].length, 2);
            assert.strictEqual(parseInt(content['commitSets'][0].id), 401);
            assert.deepStrictEqual(content['commitSets'][0].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
            assert.strictEqual(parseInt(content['commitSets'][1].id), 402);
            assert.deepStrictEqual(content['commitSets'][1].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);

            assert.strictEqual(content['commits'].length, 3);
            assert.strictEqual(parseInt(content['commits'][0].id), 87832);
            assert.strictEqual(content['commits'][0].repository, '9');
            assert.strictEqual(content['commits'][0].revision, '10.11 15A284');
            assert.strictEqual(parseInt(content['commits'][1].id), 93116);
            assert.strictEqual(content['commits'][1].repository, '11');
            assert.strictEqual(content['commits'][1].revision, '191622');
            assert.strictEqual(parseInt(content['commits'][2].id), 96336);
            assert.strictEqual(content['commits'][2].repository, '11');
            assert.strictEqual(content['commits'][2].revision, '192736');

            assert.strictEqual(content['buildRequests'].length, 4);
            assert.deepStrictEqual(parseInt(content['buildRequests'][0].id), 700);
            assert.deepStrictEqual(parseInt(content['buildRequests'][0].order), 0);
            assert.deepStrictEqual(content['buildRequests'][0].platform, '65');
            assert.deepStrictEqual(parseInt(content['buildRequests'][0].commitSet), 401);
            assert.deepStrictEqual(content['buildRequests'][0].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][0].test, '200');

            assert.deepStrictEqual(parseInt(content['buildRequests'][1].id), 701);
            assert.deepStrictEqual(parseInt(content['buildRequests'][1].order), 1);
            assert.deepStrictEqual(content['buildRequests'][1].platform, '65');
            assert.deepStrictEqual(parseInt(content['buildRequests'][1].commitSet), 402);
            assert.deepStrictEqual(content['buildRequests'][1].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][1].test, '200');

            assert.deepStrictEqual(parseInt(content['buildRequests'][2].id), 702);
            assert.deepStrictEqual(parseInt(content['buildRequests'][2].order), 2);
            assert.deepStrictEqual(content['buildRequests'][2].platform, '65');
            assert.deepStrictEqual(parseInt(content['buildRequests'][2].commitSet), 401);
            assert.deepStrictEqual(content['buildRequests'][2].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][2].test, '200');

            assert.deepStrictEqual(parseInt(content['buildRequests'][3].id), 703);
            assert.deepStrictEqual(parseInt(content['buildRequests'][3].order), 3);
            assert.deepStrictEqual(content['buildRequests'][3].platform, '65');
            assert.deepStrictEqual(parseInt(content['buildRequests'][3].commitSet), 402);
            assert.deepStrictEqual(content['buildRequests'][3].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][3].test, '200');
        });
    });

    it('should return build requests associated with a given triggerable with appropriate commits, commitSets and testParameterSets', async () => {
        await MockData.addMockDataWithBuildAndTestTypeTestParameterSets(TestServer.database());
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');

        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);

        assert.strictEqual(content['commitSets'].length, 2);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 401);
        assert.deepStrictEqual(content['commitSets'][0].revisionItems,
            [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
        assert.strictEqual(parseInt(content['commitSets'][1].id), 402);
        assert.deepStrictEqual(content['commitSets'][1].revisionItems,
            [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);

        assert.strictEqual(content['commits'].length, 3);
        assert.strictEqual(parseInt(content['commits'][0].id), 87832);
        assert.strictEqual(content['commits'][0].repository, '9');
        assert.strictEqual(content['commits'][0].revision, '10.11 15A284');
        assert.strictEqual(parseInt(content['commits'][1].id), 93116);
        assert.strictEqual(content['commits'][1].repository, '11');
        assert.strictEqual(content['commits'][1].revision, '191622');
        assert.strictEqual(parseInt(content['commits'][2].id), 96336);
        assert.strictEqual(content['commits'][2].repository, '11');
        assert.strictEqual(content['commits'][2].revision, '192736');

        assert.strictEqual(content['testParameterSets'].length, 2);
        assert.strictEqual(parseInt(content['testParameterSets'][0].id), 1);
        assert.deepStrictEqual(content['testParameterSets'][0].testParameterItems,
            [{parameter: '1', value: true, file: null}]);
        assert.strictEqual(parseInt(content['testParameterSets'][1].id), 2);
        assert.deepStrictEqual(content['testParameterSets'][1].testParameterItems,
            [{parameter: '2', value: 'Xcode 14.3', file: null}, {parameter: '3', value: null, file: '1001'}]);

        assert.strictEqual(content['buildRequests'].length, 6);
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].id), 698);
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].order), -2);
        assert.deepStrictEqual(content['buildRequests'][0].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].commitSet), 401);
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].testParameterSet), 1);
        assert.deepStrictEqual(content['buildRequests'][0].status, 'pending');
        assert.deepStrictEqual(content['buildRequests'][0].test, null);

        assert.deepStrictEqual(parseInt(content['buildRequests'][1].id), 699);
        assert.deepStrictEqual(parseInt(content['buildRequests'][1].order), -1);
        assert.deepStrictEqual(content['buildRequests'][1].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][1].commitSet), 402);
        assert.deepStrictEqual(parseInt(content['buildRequests'][1].testParameterSet), 2);
        assert.deepStrictEqual(content['buildRequests'][1].status, 'pending');
        assert.deepStrictEqual(content['buildRequests'][1].test, null);

        assert.deepStrictEqual(parseInt(content['buildRequests'][2].id), 700);
        assert.deepStrictEqual(parseInt(content['buildRequests'][2].order), 0);
        assert.deepStrictEqual(content['buildRequests'][2].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][2].commitSet), 401);
        assert.deepStrictEqual(parseInt(content['buildRequests'][2].testParameterSet), 1);
        assert.deepStrictEqual(content['buildRequests'][2].status, 'pending');
        assert.deepStrictEqual(content['buildRequests'][2].test, '200');

        assert.deepStrictEqual(parseInt(content['buildRequests'][3].id), 701);
        assert.deepStrictEqual(parseInt(content['buildRequests'][3].order), 1);
        assert.deepStrictEqual(content['buildRequests'][3].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][3].commitSet), 402);
        assert.deepStrictEqual(parseInt(content['buildRequests'][3].testParameterSet), 2);
        assert.deepStrictEqual(content['buildRequests'][3].status, 'pending');
        assert.deepStrictEqual(content['buildRequests'][3].test, '200');

        assert.deepStrictEqual(parseInt(content['buildRequests'][4].id), 702);
        assert.deepStrictEqual(parseInt(content['buildRequests'][4].order), 2);
        assert.deepStrictEqual(content['buildRequests'][4].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][4].commitSet), 401);
        assert.deepStrictEqual(parseInt(content['buildRequests'][4].testParameterSet), 1);
        assert.deepStrictEqual(content['buildRequests'][4].status, 'pending');
        assert.deepStrictEqual(content['buildRequests'][4].test, '200');

        assert.deepStrictEqual(parseInt(content['buildRequests'][5].id), 703);
        assert.deepStrictEqual(parseInt(content['buildRequests'][5].order), 3);
        assert.deepStrictEqual(content['buildRequests'][5].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][5].commitSet), 402);
        assert.deepStrictEqual(parseInt(content['buildRequests'][5].testParameterSet), 2);
        assert.deepStrictEqual(content['buildRequests'][5].status, 'pending');
        assert.deepStrictEqual(content['buildRequests'][5].test, '200');
    });

    it('should still return build requests associated with a given triggerable when test group has all build requests finished with "mayNeedMoreRequest" flag to be trues', async () => {
        await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'failed'], true, null, null, 'alternating', true);
        const content = await TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit');
        assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);

        assert.strictEqual(content['commitSets'].length, 2);
        assert.strictEqual(parseInt(content['commitSets'][0].id), 401);
        assert.deepStrictEqual(content['commitSets'][0].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
        assert.strictEqual(parseInt(content['commitSets'][1].id), 402);
        assert.deepStrictEqual(content['commitSets'][1].revisionItems, [
            {commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null},
            {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);

        assert.strictEqual(content['commits'].length, 3);
        assert.strictEqual(parseInt(content['commits'][0].id), 87832);
        assert.strictEqual(content['commits'][0].repository, '9');
        assert.strictEqual(content['commits'][0].revision, '10.11 15A284');
        assert.strictEqual(parseInt(content['commits'][1].id), 93116);
        assert.strictEqual(content['commits'][1].repository, '11');
        assert.strictEqual(content['commits'][1].revision, '191622');
        assert.strictEqual(parseInt(content['commits'][2].id), 96336);
        assert.strictEqual(content['commits'][2].repository, '11');
        assert.strictEqual(content['commits'][2].revision, '192736');

        assert.strictEqual(content['buildRequests'].length, 4);
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].id), 700);
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].order), 0);
        assert.deepStrictEqual(content['buildRequests'][0].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][0].commitSet), 401);
        assert.deepStrictEqual(content['buildRequests'][0].status, 'completed');
        assert.deepStrictEqual(content['buildRequests'][0].test, '200');

        assert.deepStrictEqual(parseInt(content['buildRequests'][1].id), 701);
        assert.deepStrictEqual(parseInt(content['buildRequests'][1].order), 1);
        assert.deepStrictEqual(content['buildRequests'][1].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][1].commitSet), 402);
        assert.deepStrictEqual(content['buildRequests'][1].status, 'completed');
        assert.deepStrictEqual(content['buildRequests'][1].test, '200');

        assert.deepStrictEqual(parseInt(content['buildRequests'][2].id), 702);
        assert.deepStrictEqual(parseInt(content['buildRequests'][2].order), 2);
        assert.deepStrictEqual(content['buildRequests'][2].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][2].commitSet), 401);
        assert.deepStrictEqual(content['buildRequests'][2].status, 'completed');
        assert.deepStrictEqual(content['buildRequests'][2].test, '200');

        assert.deepStrictEqual(parseInt(content['buildRequests'][3].id), 703);
        assert.deepStrictEqual(parseInt(content['buildRequests'][3].order), 3);
        assert.deepStrictEqual(content['buildRequests'][3].platform, '65');
        assert.deepStrictEqual(parseInt(content['buildRequests'][3].commitSet), 402);
        assert.deepStrictEqual(content['buildRequests'][3].status, 'failed');
        assert.deepStrictEqual(content['buildRequests'][3].test, '200');
    });

    it('should support useLegacyIdResolution option', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus('/api/build-requests/build-webkit?useLegacyIdResolution=true');
        }).then((content) => {
            assert.deepStrictEqual(Object.keys(content).sort(), ['buildRequests', 'commitSets', 'commits', 'status', 'testParameterSets', 'uploadedFiles']);

            assert.strictEqual(content['commitSets'].length, 2);
            assert.strictEqual(parseInt(content['commitSets'][0].id), 401);
            assert.deepStrictEqual(content['commitSets'][0].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '93116', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);
            assert.strictEqual(parseInt(content['commitSets'][1].id), 402);
            assert.deepStrictEqual(content['commitSets'][1].revisionItems,
                [{commit: '87832', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}, {commit: '96336', commitOwner: null, patch: null, requiresBuild: false, rootFile: null}]);

            assert.strictEqual(content['commits'].length, 3);
            assert.strictEqual(parseInt(content['commits'][0].id), 87832);
            assert.strictEqual(content['commits'][0].repository, 'macOS');
            assert.strictEqual(content['commits'][0].revision, '10.11 15A284');
            assert.strictEqual(parseInt(content['commits'][1].id), 93116);
            assert.strictEqual(content['commits'][1].repository, 'WebKit');
            assert.strictEqual(content['commits'][1].revision, '191622');
            assert.strictEqual(parseInt(content['commits'][2].id), 96336);
            assert.strictEqual(content['commits'][2].repository, 'WebKit');
            assert.strictEqual(content['commits'][2].revision, '192736');

            assert.strictEqual(content['buildRequests'].length, 4);
            assert.deepStrictEqual(parseInt(content['buildRequests'][0].id), 700);
            assert.deepStrictEqual(parseInt(content['buildRequests'][0].order), 0);
            assert.deepStrictEqual(content['buildRequests'][0].platform, 'some platform');
            assert.deepStrictEqual(parseInt(content['buildRequests'][0].commitSet), 401);
            assert.deepStrictEqual(content['buildRequests'][0].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][0].test, ['some test']);

            assert.deepStrictEqual(parseInt(content['buildRequests'][1].id), 701);
            assert.deepStrictEqual(parseInt(content['buildRequests'][1].order), 1);
            assert.deepStrictEqual(content['buildRequests'][1].platform, 'some platform');
            assert.deepStrictEqual(parseInt(content['buildRequests'][1].commitSet), 402);
            assert.deepStrictEqual(content['buildRequests'][1].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][1].test, ['some test']);

            assert.deepStrictEqual(parseInt(content['buildRequests'][2].id), 702);
            assert.deepStrictEqual(parseInt(content['buildRequests'][2].order), 2);
            assert.deepStrictEqual(content['buildRequests'][2].platform, 'some platform');
            assert.deepStrictEqual(parseInt(content['buildRequests'][2].commitSet), 401);
            assert.deepStrictEqual(content['buildRequests'][2].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][2].test, ['some test']);

            assert.deepStrictEqual(parseInt(content['buildRequests'][3].id), 703);
            assert.deepStrictEqual(parseInt(content['buildRequests'][3].order), 3);
            assert.deepStrictEqual(content['buildRequests'][3].platform, 'some platform');
            assert.deepStrictEqual(parseInt(content['buildRequests'][3].commitSet), 402);
            assert.deepStrictEqual(content['buildRequests'][3].status, 'pending');
            assert.deepStrictEqual(content['buildRequests'][3].test, ['some test']);
        });
    });

    it('should be fetchable by BuildRequest.fetchForTriggerable for commitSets with owned commits', () => {
        return MockData.addTestGroupWithOwnedCommits(TestServer.database()).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 4);

            let test = Test.findById(200);
            assert(test);

            let platform = Platform.findById(65);
            assert(platform);

            assert.strictEqual(parseInt(buildRequests[0].id()), 704);
            assert.strictEqual(parseInt(buildRequests[0].testGroupId()), 900);
            assert.strictEqual(buildRequests[0].test(), test);
            assert.strictEqual(buildRequests[0].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[0].order()), 0);
            assert.ok(buildRequests[0].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[0].hasFinished());
            assert.ok(!buildRequests[0].hasStarted());
            assert.ok(buildRequests[0].isPending());
            assert.strictEqual(buildRequests[0].statusLabel(), 'Waiting');

            assert.strictEqual(parseInt(buildRequests[1].id()), 705);
            assert.strictEqual(parseInt(buildRequests[1].testGroupId()), 900);
            assert.strictEqual(buildRequests[1].test(), test);
            assert.strictEqual(buildRequests[1].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[1].order()), 1);
            assert.ok(buildRequests[1].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[1].hasFinished());
            assert.ok(!buildRequests[1].hasStarted());
            assert.ok(buildRequests[1].isPending());
            assert.strictEqual(buildRequests[1].statusLabel(), 'Waiting');

            assert.strictEqual(parseInt(buildRequests[2].id()), 706);
            assert.strictEqual(parseInt(buildRequests[2].testGroupId()), 900);
            assert.strictEqual(buildRequests[2].test(), test);
            assert.strictEqual(buildRequests[2].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[2].order()), 2);
            assert.ok(buildRequests[2].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[2].hasFinished());
            assert.ok(!buildRequests[2].hasStarted());
            assert.ok(buildRequests[2].isPending());
            assert.strictEqual(buildRequests[2].statusLabel(), 'Waiting');

            assert.strictEqual(parseInt(buildRequests[3].id()), 707);
            assert.strictEqual(parseInt(buildRequests[3].testGroupId()), 900);
            assert.strictEqual(buildRequests[3].test(), test);
            assert.strictEqual(buildRequests[3].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[3].order()), 3);
            assert.ok(buildRequests[3].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(!buildRequests[3].hasStarted());
            assert.ok(buildRequests[3].isPending());
            assert.strictEqual(buildRequests[3].statusLabel(), 'Waiting');

            const osx = Repository.findById(9);
            assert.strictEqual(osx.name(), 'macOS');

            const webkit = Repository.findById(11);
            assert.strictEqual(webkit.name(), 'WebKit');

            const jsc = Repository.findById(213);
            assert.strictEqual(jsc.name(), 'JavaScriptCore');

            const firstCommitSet = buildRequests[0].commitSet();
            assert.strictEqual(buildRequests[2].commitSet(), firstCommitSet);

            const secondCommitSet = buildRequests[1].commitSet();
            assert.strictEqual(buildRequests[3].commitSet(), secondCommitSet);

            assert.strictEqual(firstCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.strictEqual(firstCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(firstCommitSet.revisionForRepository(jsc), 'owned-jsc-6161');
            assert.strictEqual(firstCommitSet.ownerRevisionForRepository(jsc), '191622');

            assert.strictEqual(secondCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.strictEqual(secondCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(secondCommitSet.revisionForRepository(jsc), 'owned-jsc-9191');
            assert.strictEqual(secondCommitSet.ownerRevisionForRepository(jsc), '192736');

            const osxCommit = firstCommitSet.commitForRepository(osx);
            assert.strictEqual(osxCommit.revision(), '10.11 15A284');
            assert.strictEqual(osxCommit, secondCommitSet.commitForRepository(osx));

            const firstWebKitCommit = firstCommitSet.commitForRepository(webkit);
            assert.strictEqual(firstWebKitCommit.revision(), '191622');
            assert.strictEqual(+firstWebKitCommit.time(), 1445945816878);

            const secondWebKitCommit = secondCommitSet.commitForRepository(webkit);
            assert.strictEqual(secondWebKitCommit.revision(), '192736');
            assert.strictEqual(+secondWebKitCommit.time(), 1448225325650);

            const firstSJCCommit = firstCommitSet.commitForRepository(jsc);
            assert.strictEqual(firstSJCCommit.revision(), 'owned-jsc-6161');
            assert.strictEqual(+firstSJCCommit.time(), 1456960795300);

            const secondSJCCommit = secondCommitSet.commitForRepository(jsc);
            assert.strictEqual(secondSJCCommit.revision(), 'owned-jsc-9191');
            assert.strictEqual(+secondSJCCommit.time(), 1462230837100)
        });
    });

    it('a repository group of a build request should accepts the commit set of the same build request', async () => {
        await MockData.addTestGroupWithOwnerCommitNotInCommitSet(TestServer.database());
        await Manifest.fetch();
        const buildRequests = await BuildRequest.fetchForTriggerable('build-webkit');
        assert.strictEqual(buildRequests.length, 1);

        const test = Test.findById(200);
        assert(test);

        const platform = Platform.findById(65);
        assert(platform);

        const buildRequest = buildRequests[0];

        assert.strictEqual(parseInt(buildRequest.id()), 704);
        assert.strictEqual(parseInt(buildRequest.testGroupId()), 900);
        assert.strictEqual(buildRequest.test(), test);
        assert.strictEqual(buildRequest.platform(), platform);
        assert.strictEqual(parseInt(buildRequest.order()), 0);
        assert.ok(buildRequest.commitSet() instanceof CommitSet);
        assert.ok(!buildRequest.hasFinished());
        assert.ok(!buildRequest.hasStarted());
        assert.ok(buildRequest.isPending());
        assert.strictEqual(buildRequest.statusLabel(), 'Waiting');

        const osx = Repository.findById(9);
        assert.strictEqual(osx.name(), 'macOS');

        const webkit = Repository.findById(11);
        assert.strictEqual(webkit.name(), 'WebKit');

        const jsc = Repository.findById(213);
        assert.strictEqual(jsc.name(), 'JavaScriptCore');

        const commitSet = buildRequest.commitSet();

        assert.strictEqual(commitSet.revisionForRepository(osx), '10.11 15A284');
        assert.strictEqual(commitSet.revisionForRepository(webkit), '192736');
        assert.strictEqual(commitSet.revisionForRepository(jsc), 'owned-jsc-9191');
        assert.strictEqual(commitSet.ownerRevisionForRepository(jsc), '191622');
        assert.deepStrictEqual(commitSet.topLevelRepositories().sort((one, another) => parseInt(one.id()) - parseInt(another.id())), [osx, webkit]);
        assert.ok(buildRequest.repositoryGroup().accepts(commitSet));
    });

    it('should be fetchable by BuildRequest.fetchForTriggerable', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 4);

            let test = Test.findById(200);
            assert(test);

            let platform = Platform.findById(65);
            assert(platform);

            assert.strictEqual(parseInt(buildRequests[0].id()), 700);
            assert.strictEqual(parseInt(buildRequests[0].testGroupId()), 600);
            assert.strictEqual(buildRequests[0].test(), test);
            assert.strictEqual(buildRequests[0].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[0].order()), 0);
            assert.ok(buildRequests[0].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[0].hasFinished());
            assert.ok(!buildRequests[0].hasStarted());
            assert.ok(buildRequests[0].isPending());
            assert.strictEqual(buildRequests[0].statusLabel(), 'Waiting');

            assert.strictEqual(parseInt(buildRequests[1].id()), 701);
            assert.strictEqual(parseInt(buildRequests[1].testGroupId()), 600);
            assert.strictEqual(buildRequests[1].test(), test);
            assert.strictEqual(buildRequests[1].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[1].order()), 1);
            assert.ok(buildRequests[1].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[1].hasFinished());
            assert.ok(!buildRequests[1].hasStarted());
            assert.ok(buildRequests[1].isPending());
            assert.strictEqual(buildRequests[1].statusLabel(), 'Waiting');

            assert.strictEqual(parseInt(buildRequests[2].id()), 702);
            assert.strictEqual(parseInt(buildRequests[2].testGroupId()), 600);
            assert.strictEqual(buildRequests[2].test(), test);
            assert.strictEqual(buildRequests[2].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[2].order()), 2);
            assert.ok(buildRequests[2].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[2].hasFinished());
            assert.ok(!buildRequests[2].hasStarted());
            assert.ok(buildRequests[2].isPending());
            assert.strictEqual(buildRequests[2].statusLabel(), 'Waiting');

            assert.strictEqual(parseInt(buildRequests[3].id()), 703);
            assert.strictEqual(parseInt(buildRequests[3].testGroupId()), 600);
            assert.strictEqual(buildRequests[3].test(), test);
            assert.strictEqual(buildRequests[3].platform(), platform);
            assert.strictEqual(parseInt(buildRequests[3].order()), 3);
            assert.ok(buildRequests[3].commitSet() instanceof CommitSet);
            assert.ok(!buildRequests[3].hasFinished());
            assert.ok(!buildRequests[3].hasStarted());
            assert.ok(buildRequests[3].isPending());
            assert.strictEqual(buildRequests[3].statusLabel(), 'Waiting');

            let osx = Repository.findById(9);
            assert.strictEqual(osx.name(), 'macOS');

            let webkit = Repository.findById(11);
            assert.strictEqual(webkit.name(), 'WebKit');

            let firstCommitSet = buildRequests[0].commitSet();
            assert.strictEqual(buildRequests[2].commitSet(), firstCommitSet);

            let secondCommitSet = buildRequests[1].commitSet();
            assert.strictEqual(buildRequests[3].commitSet(), secondCommitSet);

            assert.strictEqual(firstCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.strictEqual(firstCommitSet.revisionForRepository(webkit), '191622');

            assert.strictEqual(secondCommitSet.revisionForRepository(osx), '10.11 15A284');
            assert.strictEqual(secondCommitSet.revisionForRepository(webkit), '192736');

            let osxCommit = firstCommitSet.commitForRepository(osx);
            assert.strictEqual(osxCommit.revision(), '10.11 15A284');
            assert.strictEqual(osxCommit, secondCommitSet.commitForRepository(osx));

            let firstWebKitCommit = firstCommitSet.commitForRepository(webkit);
            assert.strictEqual(firstWebKitCommit.revision(), '191622');
            assert.strictEqual(+firstWebKitCommit.time(), 1445945816878);

            let secondWebKitCommit = secondCommitSet.commitForRepository(webkit);
            assert.strictEqual(secondWebKitCommit.revision(), '192736');
            assert.strictEqual(+secondWebKitCommit.time(), 1448225325650);
        });
    });

    it('should not include a build request if all requests in the same group had been completed', () => {
        return MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 0);
        });
    });

    it('should not include a build request if all requests in the same group had been failed or cancled', () => {
        return MockData.addMockData(TestServer.database(), ['failed', 'failed', 'canceled', 'canceled']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 0);
        });
    });

    it('should include all build requests of a test group if one of the reqeusts in the group had not been finished', () => {
        return MockData.addMockData(TestServer.database(), ['completed', 'completed', 'scheduled', 'pending']).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 4);
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
            assert.strictEqual(buildRequests.length, 4);
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
            assert.strictEqual(buildRequests.length, 8);
            assert.strictEqual(parseInt(buildRequests[0].id()), 700);
            assert.strictEqual(parseInt(buildRequests[0].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[0].order()), 0);
            assert.strictEqual(parseInt(buildRequests[1].id()), 701);
            assert.strictEqual(parseInt(buildRequests[1].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[1].order()), 1);
            assert.strictEqual(parseInt(buildRequests[2].id()), 702);
            assert.strictEqual(parseInt(buildRequests[2].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[2].order()), 2);
            assert.strictEqual(parseInt(buildRequests[3].id()), 703);
            assert.strictEqual(parseInt(buildRequests[3].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[3].order()), 3);

            assert.strictEqual(parseInt(buildRequests[4].id()), 710);
            assert.strictEqual(parseInt(buildRequests[4].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[4].order()), 0);
            assert.strictEqual(parseInt(buildRequests[5].id()), 711);
            assert.strictEqual(parseInt(buildRequests[5].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[5].order()), 1);
            assert.strictEqual(parseInt(buildRequests[6].id()), 712);
            assert.strictEqual(parseInt(buildRequests[6].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[6].order()), 2);
            assert.strictEqual(parseInt(buildRequests[7].id()), 713);
            assert.strictEqual(parseInt(buildRequests[7].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[7].order()), 3);
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
            assert.strictEqual(triggerable.repositoryGroups().length, 2);
            groups = TriggerableRepositoryGroup.sortByName(triggerable.repositoryGroups());
            assert.strictEqual(groups[0].name(), 'webkit-git');
            assert.strictEqual(groups[1].name(), 'webkit-svn');
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 8);
            assert.strictEqual(parseInt(buildRequests[0].id()), 700);
            assert.strictEqual(buildRequests[0].repositoryGroup(), groups[1]);
            assert.strictEqual(parseInt(buildRequests[1].id()), 701);
            assert.strictEqual(buildRequests[1].repositoryGroup(), groups[1]);
            assert.strictEqual(parseInt(buildRequests[2].id()), 702);
            assert.strictEqual(buildRequests[2].repositoryGroup(), groups[1]);
            assert.strictEqual(parseInt(buildRequests[3].id()), 703);
            assert.strictEqual(buildRequests[3].repositoryGroup(), groups[1]);

            assert.strictEqual(parseInt(buildRequests[4].id()), 1700);
            assert.strictEqual(buildRequests[4].repositoryGroup(), groups[0]);
            assert.strictEqual(parseInt(buildRequests[5].id()), 1701);
            assert.strictEqual(buildRequests[5].repositoryGroup(), groups[0]);
            assert.strictEqual(parseInt(buildRequests[6].id()), 1702);
            assert.strictEqual(buildRequests[6].repositoryGroup(), groups[0]);
            assert.strictEqual(parseInt(buildRequests[7].id()), 1703);
            assert.strictEqual(buildRequests[7].repositoryGroup(), groups[0]);
        });
    });

    it('should place build requests created by user before automatically created ones', () => {
        let db = TestServer.database();
        return Promise.all([MockData.addMockData(db), MockData.addAnotherMockTestGroup(db, null, 'rniwa')]).then(() => {
            return Manifest.fetch();
        }).then(() => {
            return BuildRequest.fetchForTriggerable('build-webkit');
        }).then((buildRequests) => {
            assert.strictEqual(buildRequests.length, 8);
            assert.strictEqual(parseInt(buildRequests[0].id()), 710);
            assert.strictEqual(parseInt(buildRequests[0].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[0].order()), 0);
            assert.strictEqual(parseInt(buildRequests[1].id()), 711);
            assert.strictEqual(parseInt(buildRequests[1].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[1].order()), 1);
            assert.strictEqual(parseInt(buildRequests[2].id()), 712);
            assert.strictEqual(parseInt(buildRequests[2].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[2].order()), 2);
            assert.strictEqual(parseInt(buildRequests[3].id()), 713);
            assert.strictEqual(parseInt(buildRequests[3].testGroupId()), 601);
            assert.strictEqual(parseInt(buildRequests[3].order()), 3);

            assert.strictEqual(parseInt(buildRequests[4].id()), 700);
            assert.strictEqual(parseInt(buildRequests[4].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[4].order()), 0);
            assert.strictEqual(parseInt(buildRequests[5].id()), 701);
            assert.strictEqual(parseInt(buildRequests[5].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[5].order()), 1);
            assert.strictEqual(parseInt(buildRequests[6].id()), 702);
            assert.strictEqual(parseInt(buildRequests[6].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[6].order()), 2);
            assert.strictEqual(parseInt(buildRequests[7].id()), 703);
            assert.strictEqual(parseInt(buildRequests[7].testGroupId()), 600);
            assert.strictEqual(parseInt(buildRequests[7].order()), 3);
        });
    });

    it('should not return "UnknownBuildRequestStatus" when updating a canceled build request', () => {
        const updates = {'700': {status: 'canceled', url: 'http://build.webkit.org/someBuilder/builds'}};
        return MockData.addMockData(TestServer.database()).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
                'workerName': 'sync-worker',
                'workerPassword': 'password',
                'buildRequestUpdates': updates
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');

                assert.strictEqual(response['buildRequests'].length, 4);
                assert.deepStrictEqual(parseInt(response['buildRequests'][0].id), 700);
                assert.deepStrictEqual(parseInt(response['buildRequests'][0].order), 0);
                assert.deepStrictEqual(response['buildRequests'][0].platform, '65');
                assert.deepStrictEqual(parseInt(response['buildRequests'][0].commitSet), 401);
                assert.deepStrictEqual(response['buildRequests'][0].status, 'canceled');
                assert.deepStrictEqual(response['buildRequests'][0].test, '200');

                assert.deepStrictEqual(parseInt(response['buildRequests'][1].id), 701);
                assert.deepStrictEqual(parseInt(response['buildRequests'][1].order), 1);
                assert.deepStrictEqual(response['buildRequests'][1].platform, '65');
                assert.deepStrictEqual(parseInt(response['buildRequests'][1].commitSet), 402);
                assert.deepStrictEqual(response['buildRequests'][1].status, 'pending');
                assert.deepStrictEqual(response['buildRequests'][1].test, '200');

                assert.deepStrictEqual(parseInt(response['buildRequests'][2].id), 702);
                assert.deepStrictEqual(parseInt(response['buildRequests'][2].order), 2);
                assert.deepStrictEqual(response['buildRequests'][2].platform, '65');
                assert.deepStrictEqual(parseInt(response['buildRequests'][2].commitSet), 401);
                assert.deepStrictEqual(response['buildRequests'][2].status, 'pending');
                assert.deepStrictEqual(response['buildRequests'][2].test, '200');

                assert.deepStrictEqual(parseInt(response['buildRequests'][3].id), 703);
                assert.deepStrictEqual(parseInt(response['buildRequests'][3].order), 3);
                assert.deepStrictEqual(response['buildRequests'][3].platform, '65');
                assert.deepStrictEqual(parseInt(response['buildRequests'][3].commitSet), 402);
                assert.deepStrictEqual(response['buildRequests'][3].status, 'pending');
                assert.deepStrictEqual(response['buildRequests'][3].test, '200');
            }, () => {
                assert(false, 'Should not be reached');
            });
        });
    });

    it('should not update url or status_description if either is not specified while updating a build request with "failedIfNotCompleted"', async () => {
        const updates = {'700': {status: 'failedIfNotCompleted'}};
        const url = 'http://build.webkit.org/someBuilder/builds';
        await MockData.addMockData(TestServer.database(), ['running', 'pending', 'pending', 'pending']);
        await TestServer.database().query(`UPDATE build_requests SET request_url = '${url}' WHERE request_id = 700`);
        const response = await TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        });
        assert.equal(response['status'], 'OK');

        assert.equal(response['buildRequests'].length, 4);
        assert.deepEqual(response['buildRequests'][0].id, 700);
        assert.deepEqual(response['buildRequests'][0].url, url);
    });

    it('should not set "may_need_more_requests" for a hidden test group when updating a test group to failed', async () => {
        const updates = {'700': {status: 'failed'}};
        await MockData.addMockData(TestServer.database(), ['canceled', 'canceled', 'canceled', 'canceled'], false, null, null, 'alternating', false, true);

        let testGroup = await TestServer.database().selectFirstRow('analysis_test_groups', {id: 600});
        assert(!testGroup.may_need_more_requests);
        assert(testGroup.hidden);

        const response = await TestServer.remoteAPI().postJSONWithStatus('/api/build-requests/build-webkit', {
            'workerName': 'sync-worker',
            'workerPassword': 'password',
            'buildRequestUpdates': updates
        });
        assert.equal(response['status'], 'OK');

        testGroup = await TestServer.database().selectFirstRow('analysis_test_groups', {id: 600});
        assert(!testGroup.may_need_more_requests);
        assert(testGroup.hidden);
    });
});
