'use strict';

const assert = require('assert');
const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const assertThrows = require('./resources/common-operations.js').assertThrows;
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
            assert.deepStrictEqual(content.testParameterSets, []);
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
            assert.deepStrictEqual(content.testParameterSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should list all test groups with pending notification', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']);
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/ready-for-notification');
            assert.strictEqual(content.testGroups.length, 1);
            assert.deepStrictEqual(content.testParameterSets, []);
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
            assert.deepStrictEqual(testGroup.testParameterSets, [null, null, null, null]);
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
            assert.deepStrictEqual(content.testParameterSets, []);
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
            assert.deepStrictEqual(content.testParameterSets, []);
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
            assert.deepStrictEqual(content.testParameterSets, []);
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

        it('should list test group with test parameter sets', async () => {
            const database = TestServer.database();
            await MockData.addMockDataWithBuildAndTestTypeTestParameterSets(database);

            const data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.deepStrictEqual(data.testParameterSets, [
                {id: '1', testParameterItems: [{parameter: '1', value: true, file: null}]},
                {id: '2', testParameterItems: [{parameter: '2', value: 'Xcode 14.3', file: null},
                        {parameter: '3', value: null, file: '1001'}]}]);

            assert.strictEqual(data.testGroups.length, 1);
            const testGroup = data.testGroups[0];
            assert.strictEqual(parseInt(testGroup.id), 600);
            assert.strictEqual(parseInt(testGroup.task), 500);
            assert.strictEqual(testGroup.name, 'some test group');
            assert.strictEqual(testGroup.author, null);
            assert.ok(!testGroup.hidden);
            assert.ok(!testGroup.needsNotification);
            assert.ok(!testGroup.mayNeedMoreRequests);
            assert.strictEqual(parseInt(testGroup.platform), 65);
            assert.deepStrictEqual(testGroup.buildRequests, ['698', '699', '700','701', '702', '703']);
            assert.deepStrictEqual(testGroup.commitSets, ['401', '402', '401', '402', '401', '402']);
            assert.deepStrictEqual(testGroup.testParameterSets, ['1', '2', '1', '2', '1', '2']);
        });

        it('should list test group with test parameter set only on one side', async () => {
            const database = TestServer.database();
            await MockData.addMockDataWithBuildAndTestTypeTestParameterSets(database, [null, 2, null, 2, null, 2]);

            const data =  await TestServer.remoteAPI().getJSON('/api/test-groups/600');
            assert.deepStrictEqual(data.testParameterSets, [{id: '2', testParameterItems: [
                {parameter: '2', value: 'Xcode 14.3', file: null}, {parameter: '3', value: null, file: '1001'}]}]);

            assert.strictEqual(data.testGroups.length, 1);
            const testGroup = data.testGroups[0];
            assert.strictEqual(parseInt(testGroup.id), 600);
            assert.strictEqual(parseInt(testGroup.task), 500);
            assert.strictEqual(testGroup.name, 'some test group');
            assert.strictEqual(testGroup.author, null);
            assert.ok(!testGroup.hidden);
            assert.ok(!testGroup.needsNotification);
            assert.ok(!testGroup.mayNeedMoreRequests);
            assert.strictEqual(parseInt(testGroup.platform), 65);
            assert.deepStrictEqual(testGroup.buildRequests, ['698', '699', '700','701', '702', '703']);
            assert.deepStrictEqual(testGroup.commitSets, ['401', '402', '401', '402', '401', '402']);
            assert.deepStrictEqual(testGroup.testParameterSets, [null, '2', null, '2', null, '2']);
        });
    });

    describe('/api/test-groups/need-more-requests', () => {
        it('should list all test groups potentially need additional build requests', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'failed', 'completed'], false);
            await TestServer.database().query('UPDATE analysis_test_groups SET testgroup_may_need_more_requests = TRUE WHERE testgroup_id = 600');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/need-more-requests');
            assert.strictEqual(content.testGroups.length, 1);
            assert.deepStrictEqual(content.testParameterSets, []);
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
            assert.deepStrictEqual(testGroup.testParameterSets, [null, null, null, null]);
        });

        it('should list all test groups may need additional build requests', async () => {
            await MockData.addMockData(TestServer.database(), ['completed', 'completed', 'completed', 'completed']);
            await MockData.addAnotherMockTestGroup(TestServer.database(), ['completed', 'completed', 'failed', 'completed'], 'webkit');
            await TestServer.database().query('UPDATE analysis_test_groups SET testgroup_may_need_more_requests = TRUE WHERE testgroup_id = 600');
            await TestServer.database().query('UPDATE analysis_test_groups SET testgroup_may_need_more_requests = TRUE WHERE testgroup_id = 601');
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups/need-more-requests');
            assert.strictEqual(content.testGroups.length, 2);
            assert.deepStrictEqual(content.testParameterSets, []);

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
            assert.deepStrictEqual(oneTestGroup.testParameterSets, [null, null, null, null]);

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
            assert.deepStrictEqual(anotherTestGroup.testParameterSets, [null, null, null, null]);
        });
    });

    describe('/api/test-groups?(task=<task_id>|buildRequest=<id>)', () => {
        it('should reject with "TaskOrBuildRequestIdNotSpecified" if no argument is specified', async () => {
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups');
            assert.strictEqual(content['status'], 'TaskOrBuildRequestIdNotSpecified');
        });

        it('should reject with "CannotSpecifyBothTaskAndBuildRequestIds" if both build request and task id are specified', async () => {
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups?task=500&buildRequest=700');
            assert.strictEqual(content['status'], 'CannotSpecifyBothTaskAndBuildRequestIds');
        });

        it('should return an empty list when task id is invalid', async () => {
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups?task=1000000');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.testParameterSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should return an empty list when build request id is invalid', async () => {
            await MockData.addMockData(TestServer.database());
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups?buildRequest=1000000');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups, []);
            assert.deepStrictEqual(content.buildRequests, []);
            assert.deepStrictEqual(content.commitSets, []);
            assert.deepStrictEqual(content.testParameterSets, []);
            assert.deepStrictEqual(content.commits, []);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should return test group information for a task id', async () => {
            await MockData.addMockData(TestServer.database());
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups?task=500');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups.length, 1);
            assert.deepStrictEqual(content.testGroups[0].id, '600');
            assert.deepStrictEqual(content.buildRequests.length, 4);
            assert.deepStrictEqual(content.buildRequests[0].id, '700');
            assert.deepStrictEqual(content.commitSets.length, 2);
            assert.deepStrictEqual(content.testParameterSets, []);
            assert.deepStrictEqual(content.commits.length, 3);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });

        it('should return test group information for a given build request', async () => {
            await MockData.addMockData(TestServer.database());
            const content = await TestServer.remoteAPI().getJSON('/api/test-groups?buildRequest=700');
            assert.strictEqual(content.status, 'OK');
            assert.deepStrictEqual(content.testGroups.length, 1);
            assert.deepStrictEqual(content.testGroups[0].id, '600');
            assert.deepStrictEqual(content.buildRequests.length, 4);
            assert.deepStrictEqual(content.buildRequests[0].id, '700');
            assert.deepStrictEqual(content.commitSets.length, 2);
            assert.deepStrictEqual(content.testParameterSets, []);
            assert.deepStrictEqual(content.commits.length, 3);
            assert.deepStrictEqual(content.uploadedFiles, []);
        });
    });
});