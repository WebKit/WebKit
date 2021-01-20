'use strict';

const assert = require('assert');
require('../tools/js/v3-models.js');
const MockModels = require('./resources/mock-v3-models.js').MockModels;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
const NodePrivilegedAPI = require('../tools/js/privileged-api').PrivilegedAPI;
const createAdditionalBuildRequestsForTestGroupsWithFailedRequests = require('../tools/js/retry-failed-build-requests').createAdditionalBuildRequestsForTestGroupsWithFailedRequests;


function sampleTestGroup(config) {
    const needsNotification = config.needsNotification;
    const initialRepetitionCount = config.initialRepetitionCount;
    const mayNeedMoreRequests = config.mayNeedMoreRequests;
    const hidden = config.hidden;
    const statusList = config.statusList;

    return {
        "testGroups": [{
            "id": "2128",
            "task": "1376",
            "platform": "31",
            "name": "Confirm",
            "author": "rniwa",
            "createdAt": 1458688514000,
            hidden,
            needsNotification,
            "buildRequests": ["16985", "16986", "16987", "16988", "16989", "16990"],
            "commitSets": ["4255", "4256"],
            "notificationSentAt": null,
            initialRepetitionCount,
            mayNeedMoreRequests
        }],
        "buildRequests": [{
            "id": "16985",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "0",
            "commitSet": "4255",
            "status": statusList[0],
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16986",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "1",
            "commitSet": "4256",
            "status": statusList[1],
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16987",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "2",
            "commitSet": "4255",
            "status": statusList[2],
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16988",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "3",
            "commitSet": "4256",
            "status": statusList[3],
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16989",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "3",
            "commitSet": "4255",
            "status": statusList[4],
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16990",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "3",
            "commitSet": "4256",
            "status": statusList[5],
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }],
        "commitSets": [{
            "id": "4255",
            "revisionItems": [{"commit": "87832"}, {"commit": "93116"}],
            "customRoots": [],
        }, {
            "id": "4256",
            "revisionItems": [{"commit": "87832"}, {"commit": "96336"}],
            "customRoots": [],
        }],
        "commits": [{
            "id": "87832",
            "repository": "9",
            "revision": "10.11 15A284",
            "time": 0
        }, {
            "id": "93116",
            "repository": "11",
            "revision": "191622",
            "time": 1445945816878
        }, {
            "id": "87832",
            "repository": "9",
            "revision": "10.11 15A284",
            "time": 0
        }, {
            "id": "96336",
            "repository": "11",
            "revision": "192736",
            "time": 1448225325650
        }],
        "uploadedFiles": [],
        "status": "OK"
    };
}

describe('createAdditionalBuildRequestsForTestGroupsWithFailedRequests', () => {
    let requests = MockRemoteAPI.inject(null, NodePrivilegedAPI);
    MockModels.inject();
    beforeEach(() => {
        PrivilegedAPI.configure('slave_name', 'password');
    });

    it('should add one more build request when one of the existing requests failed', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: true, hidden: false,
            statusList: ["completed", "completed", "completed", "completed", "completed", "failed"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 1);

        assert.equal(requests[0].url, '/privileged-api/add-build-requests');
        assert.deepEqual(requests[0].data, {slaveName: 'slave_name', slavePassword: 'password', group: '2128', addCount: 1});
        requests[0].resolve();

        await MockRemoteAPI.waitForRequest();
        assert.equal(requests.length, 2);
        assert.equal(requests[1].url, '/api/test-groups/2128');
    });

    it('should add 2 more build requests when 2 failed build request found for a commit set', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: true, hidden: false,
            statusList: ["completed", "failed", "completed", "completed", "completed", "failed"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 1);

        assert.equal(requests[0].url, '/privileged-api/add-build-requests');
        assert.deepEqual(requests[0].data, {slaveName: 'slave_name', slavePassword: 'password', group: '2128', addCount: 2});
        requests[0].resolve();

        await MockRemoteAPI.waitForRequest();
        assert.equal(requests.length, 2);
        assert.equal(requests[1].url, '/api/test-groups/2128');
    });

    it('should not schedule more build requests when all requests for a commit set had failed', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: true, hidden: false,
            statusList: ["completed", "failed", "completed", "failed", "completed", "failed"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 1);

        assert.equal(requests[0].url, '/privileged-api/update-test-group');
        assert.deepEqual(requests[0].data, {slaveName: 'slave_name', slavePassword: 'password', group: '2128', mayNeedMoreRequests: false});
        requests[0].resolve();
    });

    it('should not schedule more build requests when "may_need_more_requests" is not set', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: false, hidden: false,
            statusList: ["completed", "failed", "completed", "completed", "completed", "failed"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 0);
    });

    it('should not schedule more build requests when build request is hidden', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: true, hidden: true,
            statusList: ["completed", "failed", "completed", "completed", "completed", "failed"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 1);

        assert.equal(requests[0].url, '/privileged-api/update-test-group');
        assert.deepEqual(requests[0].data, {slaveName: 'slave_name', slavePassword: 'password', group: '2128', mayNeedMoreRequests: false});
        requests[0].resolve();
    });

    it('should not schedule more build request when we\'ve already hit the maximum retry count', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: true, hidden: false,
            statusList: ["completed", "completed", "failed", "failed", "failed", "failed"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 1.5);
        assert.equal(requests.length, 1);

        assert.equal(requests[0].url, '/privileged-api/update-test-group');
        assert.deepEqual(requests[0].data, {slaveName: 'slave_name', slavePassword: 'password', group: '2128', mayNeedMoreRequests: false});
        requests[0].resolve();
    });

    it('should not schedule more when additional build requests are still pending', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 2, mayNeedMoreRequests: true, hidden: false,
            statusList: ["completed", "completed", "failed", "failed", "pending", "pending"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 1);

        assert.equal(requests[0].url, '/privileged-api/update-test-group');
        assert.deepEqual(requests[0].data, {slaveName: 'slave_name', slavePassword: 'password', group: '2128', mayNeedMoreRequests: false});
        requests[0].resolve();
    });

    it('should not clear mayNeedMoreRequest flag when one commit set has not got a successful run but have pending builds', async () => {
        const testGroupConfig = {needsNotification: false, initialRepetitionCount: 3, mayNeedMoreRequests: true, hidden: false,
            statusList: ["completed", "failed", "completed", "failed", "pending", "pending"]};
        const data = sampleTestGroup(testGroupConfig);
        const testGroups = TestGroup._createModelsFromFetchedTestGroups(data);
        createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, 3);
        assert.equal(requests.length, 0);
    });
});
