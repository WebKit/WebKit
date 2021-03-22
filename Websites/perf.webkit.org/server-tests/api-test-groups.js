'use strict';

const assert = require('assert');
const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/test-groups', function () {
    prepareServerTest(this, 'node');

    describe('/api/test-groups/ready-for-notification', () => {
        it('should give an empty list if there is not existing test group at all', async () => {
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should not include a test group with "canceled" state in at least one build request', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'canceled']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should list all test groups with pending notification', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.testGroups.length, 1);
            const testGroup = content.testGroups[0];
            assert.strictEqual(parseInt(testGroup.id), 600);
            assert.strictEqual(parseInt(testGroup.task), 500);
            assert.strictEqual(testGroup.name, 'some test group');
            assert.strictEqual(testGroup.author, null);
            assert.strictEqual(testGroup.hidden, false);
            assert.strictEqual(testGroup.needsNotification, true);
            assert.strictEqual(parseInt(testGroup.platform), 65);
            assert.deepStrictEqual(testGroup.buildRequests, ['700','701', '702', '703']);
            assert.deepStrictEqual(testGroup.commitSets, ['401', '402', '401', '402']);
        });

        it('should not list hidden test group', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'completed']);
            await database.query('UPDATE analysis_test_groups SET testgroup_hidden = TRUE WHERE testgroup_id = 600');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should not list test groups without needs notification flag', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'completed']);
            await database.query('UPDATE analysis_test_groups SET testgroup_needs_notification = FALSE WHERE testgroup_id = 600');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should not list a test group that has some incompleted build requests', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'running']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });
    });

    describe('/api/test-groups/<triggerables>', () => {
        it('should set "testgroup_may_need_more_requests" when a build request is set to "failed" due to "failedIfNotCompleted"', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'running']);

            let data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.ok(!data['testGroups'][0].mayNeedMoreRequests);
            await TestServer.remoteAPI().postJSON('/api/build-requests/build-webkit', {workerName: 'test', workerPassword: 'password',
                buildRequestUpdates: {703: {status: 'failedIfNotCompleted', url: 'http://webkit.org/build/1'}}});
            data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.ok(data['testGroups'][0].mayNeedMoreRequests);
        });

        it('should set "testgroup_may_need_more_requests" when a build request is set to "failed"', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'running']);

            let data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.ok(!data['testGroups'][0].mayNeedMoreRequests);
            await TestServer.remoteAPI().postJSON('/api/build-requests/build-webkit', {workerName: 'test', workerPassword: 'password',
                buildRequestUpdates: {703: {status: 'failed', url: 'http://webkit.org/build/1'}}});
            data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.ok(data['testGroups'][0].mayNeedMoreRequests);
        });

        it('should set "testgroup_may_need_more_requests" to all test groups those have failed build request', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'running']);
            await MockData.addAnotherMockTestGroup(database, ['completed', 'completed', 'completed', 'running'], 'webkit');

            let data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.ok(!data['testGroups'][0].mayNeedMoreRequests);
            data =  await TestServer.remoteAPI().getJSON('/api/test-groups/601');
            assert.ok(!data['testGroups'][0].mayNeedMoreRequests);

            await TestServer.remoteAPI().postJSON('/api/build-requests/build-webkit', {workerName: 'test', workerPassword: 'password',
                buildRequestUpdates: {
                    703: {status: 'failed', url: 'http://webkit.org/build/1'},
                    713: {status: 'failedIfNotCompleted', url: 'http://webkit.org/build/11'},
                }});

            data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.ok(data['testGroups'][0].mayNeedMoreRequests);
            data =  await TestServer.remoteAPI().getJSON('/api/test-groups/601');
            assert.ok(data['testGroups'][0].mayNeedMoreRequests);
        });
    });

    describe('/api/test-groups/need-more-requests', () => {
        it('should list all test groups potentially need additional build requests', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'failed', 'completed'], false);
            await TestServer.database().query('UPDATE analysis_test_groups SET testgroup_may_need_more_requests = TRUE WHERE testgroup_id = 600');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/need-more-requests');
            assert.strictEqual(content.testGroups.length, 1);
            const testGroup = content.testGroups[0];
            assert.strictEqual(parseInt(testGroup.id), 600);
            assert.strictEqual(parseInt(testGroup.task), 500);
            assert.strictEqual(testGroup.name, 'some test group');
            assert.strictEqual(testGroup.author, null);
            assert.ok(!testGroup.hidden);
            assert.ok(!testGroup.needsNotification);
            assert.ok(testGroup.mayNeedMoreRequests);
            assert.strictEqual(parseInt(testGroup.platform), 65);
            assert.deepStrictEqual(testGroup.buildRequests, ['700','701', '702', '703']);
            assert.deepStrictEqual(testGroup.commitSets, ['401', '402', '401', '402']);
        });

        it('should list all test groups may need additional build requests', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']);
            await MockData.addAnotherMockTestGroup(TestServer.database(), ['completed', 'completed', 'failed', 'completed'], 'webkit');
            await TestServer.database().query('UPDATE analysis_test_groups SET testgroup_may_need_more_requests = TRUE WHERE testgroup_id = 600');
            await TestServer.database().query('UPDATE analysis_test_groups SET testgroup_may_need_more_requests = TRUE WHERE testgroup_id = 601');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/need-more-requests');
            assert.strictEqual(content.testGroups.length, 2);

            const oneTestGroup = content.testGroups[0];
            assert.strictEqual(parseInt(oneTestGroup.id), 600);
            assert.strictEqual(parseInt(oneTestGroup.task), 500);
            assert.strictEqual(oneTestGroup.name, 'some test group');
            assert.strictEqual(oneTestGroup.author, null);
            assert.ok(!oneTestGroup.hidden);
            assert.ok(oneTestGroup.needsNotification);
            assert.ok(oneTestGroup.mayNeedMoreRequests);
            assert.strictEqual(parseInt(oneTestGroup.platform), 65);
            assert.deepStrictEqual(oneTestGroup.buildRequests, ['700','701', '702', '703']);
            assert.deepStrictEqual(oneTestGroup.commitSets, ['401', '402', '401', '402']);

            const anotherTestGroup = content.testGroups[1];
            assert.strictEqual(parseInt(anotherTestGroup.id), 601);
            assert.strictEqual(parseInt(anotherTestGroup.task), 500);
            assert.strictEqual(anotherTestGroup.name, 'another test group');
            assert.strictEqual(anotherTestGroup.author, 'webkit');
            assert.ok(!anotherTestGroup.hidden);
            assert.ok(!anotherTestGroup.needsNotification);
            assert.ok(anotherTestGroup.mayNeedMoreRequests);
            assert.strictEqual(parseInt(anotherTestGroup.platform), 65);
            assert.deepStrictEqual(anotherTestGroup.buildRequests, ['710','711', '712', '713']);
            assert.deepStrictEqual(anotherTestGroup.commitSets, ['401', '402', '401', '402']);
        });
    });
});