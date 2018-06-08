'use strict';

const assert = require('assert');
const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/test-groups', function () {
    prepareServerTest(this);

    describe('/api/test-groups/ready-for-notification', () => {

        it('should give an empty list if there is not existing test group at all', async () => {
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.equal(content.status, 'OK');
            assert.deepEqual(content.testGroups, []);
            assert.deepEqual(content.buildRequests, []);
            assert.deepEqual(content.commitSets, []);
            assert.deepEqual(content.commits, []);
            assert.deepEqual(content.uploadedFiles, []);
        });

        it('should not include a test group with "canceled" state in at least one build request', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'canceled']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.equal(content.status, 'OK');
            assert.deepEqual(content.testGroups, []);
            assert.deepEqual(content.buildRequests, []);
            assert.deepEqual(content.commitSets, []);
            assert.deepEqual(content.commits, []);
            assert.deepEqual(content.uploadedFiles, []);
        });

        it('should list all test groups with pending notification', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.equal(content.testGroups.length, 1);
            const testGroup = content.testGroups[0];
            assert.equal(testGroup.id, 600);
            assert.equal(testGroup.task, 500);
            assert.equal(testGroup.name, 'some test group');
            assert.equal(testGroup.author, null);
            assert.equal(testGroup.hidden, false);
            assert.equal(testGroup.needsNotification, true);
            assert.equal(testGroup.platform, 65);
            assert.deepEqual(testGroup.buildRequests, ['700','701', '702', '703']);
            assert.deepEqual(testGroup.commitSets, ['401', '402', '401', '402']);
        });

        it('should not list hidden test group', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'completed']);
            await database.query('UPDATE analysis_test_groups SET testgroup_hidden = TRUE WHERE testgroup_id = 600');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.equal(content.status, 'OK');
            assert.deepEqual(content.testGroups, []);
            assert.deepEqual(content.buildRequests, []);
            assert.deepEqual(content.commitSets, []);
            assert.deepEqual(content.commits, []);
            assert.deepEqual(content.uploadedFiles, []);
        });

        it('should not list test groups without needs notification flag', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'completed']);
            await database.query('UPDATE analysis_test_groups SET testgroup_needs_notification = FALSE WHERE testgroup_id = 600');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.equal(content.status, 'OK');
            assert.deepEqual(content.testGroups, []);
            assert.deepEqual(content.buildRequests, []);
            assert.deepEqual(content.commitSets, []);
            assert.deepEqual(content.commits, []);
            assert.deepEqual(content.uploadedFiles, []);
        });

        it('should not list a test group that has some incompleted build requests', async () => {
            const database = TestServer.database();
            await MockData.addMockData(database, ['completed', 'completed', 'completed', 'running']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.equal(content.status, 'OK');
            assert.deepEqual(content.testGroups, []);
            assert.deepEqual(content.buildRequests, []);
            assert.deepEqual(content.commitSets, []);
            assert.deepEqual(content.commits, []);
            assert.deepEqual(content.uploadedFiles, []);
        });
    });
});