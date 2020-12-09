'use strict';

const assert = require('assert');
const crypto = require('crypto');

require('../tools/js/v3-models.js');
const MockModels = require('./resources/mock-v3-models.js').MockModels;
const NodePrivilegedAPI = require('../tools/js/privileged-api.js').PrivilegedAPI;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;

function sampleBuildRequestData()
{
    return {
        "buildRequests": [{
            "id": "16985",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "0",
            "commitSet": "4255",
            "status": "pending",
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

function oneTestGroup()
{
    return {
        "testGroups": [{
            "id": "2128",
            "task": "1376",
            "platform": "31",
            "name": "Confirm",
            "author": "rniwa",
            "createdAt": 1458688514000,
            "hidden": false,
            "needsNotification": true,
            "buildRequests": ["16985", "16986", "16987", "16988"],
            "commitSets": ["4255", "4256"],
            "notificationSentAt": null,
            'initialRepetitionCount': 1
        }],
        "buildRequests": [{
            "id": "16985",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "-2",
            "commitSet": "4255",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16986",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "-1",
            "commitSet": "4256",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16987",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "0",
            "commitSet": "4255",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16988",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "3",
            "commitSet": "4256",
            "status": "pending",
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

function threeTestGroups(secondTestGroupOverrides, thirdTestGroupOverrides)
{
    if (!secondTestGroupOverrides)
        secondTestGroupOverrides = {};
    if (!thirdTestGroupOverrides)
        thirdTestGroupOverrides = {};
    const yesterday = Date.now() - 24 * 3600 * 1000;
    return {
        "testGroups": [{
            "id": "2128",
            "task": "1376",
            "platform": "32",
            "name": "Confirm",
            "author": "rniwa",
            "createdAt": 1458688514000,
            "hidden": false,
            "needsNotification": true,
            "buildRequests": ["16985", "16986", "16987", "16988"],
            "commitSets": ["4255", "4256"],
            "notificationSentAt": null,
            'initialRepetitionCount': 1
        }, {
            "id": "2129",
            "task": secondTestGroupOverrides.task || '1376',
            "platform": secondTestGroupOverrides.platform || '32',
            "name": "Confirm",
            "author": "rniwa",
            "createdAt": 1458688514000,
            "hidden": false,
            "needsNotification": true,
            "buildRequests": ["16989", "16990", "16991", "16992"],
            "commitSets": ["4255", "4256"],
            "notificationSentAt": null,
            'initialRepetitionCount': 1
        }, {
            "id": "2130",
            "task": thirdTestGroupOverrides.task || '1376',
            "platform": thirdTestGroupOverrides.platform || '32',
            "name": "Confirm",
            "author": "rniwa",
            "createdAt": 1458688514000,
            "hidden": false,
            "needsNotification": true,
            "buildRequests": ["16993", "16994", "16995", "16996"],
            "commitSets": ["4255", "4256"],
            "notificationSentAt": null,
            'initialRepetitionCount': 1
        }],
        "buildRequests": [{
            "id": "16985",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "32",
            "testGroup": "2128",
            "order": "-2",
            "commitSet": "4255",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16986",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "32",
            "testGroup": "2128",
            "order": "-1",
            "commitSet": "4256",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16987",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "32",
            "testGroup": "2128",
            "order": "0",
            "commitSet": "4255",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16988",
            "triggerable": "3",
            "task": "1376",
            "test": "844",
            "platform": "32",
            "testGroup": "2128",
            "order": "1",
            "commitSet": "4256",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16989",
            "triggerable": "3",
            "task": secondTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": secondTestGroupOverrides.platform || '32',
            "testGroup": "2129",
            "order": "-2",
            "commitSet": "4255",
            "status": secondTestGroupOverrides.status && secondTestGroupOverrides.status[0] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16990",
            "triggerable": "3",
            "task": secondTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": secondTestGroupOverrides.platform || '32',
            "testGroup": "2129",
            "order": "-1",
            "commitSet": "4256",
            "status": secondTestGroupOverrides.status && secondTestGroupOverrides.status[1] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16991",
            "triggerable": "3",
            "task": secondTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": secondTestGroupOverrides.platform || '32',
            "testGroup": "2129",
            "order": "0",
            "commitSet": "4255",
            "status": secondTestGroupOverrides.status && secondTestGroupOverrides.status[2] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16992",
            "triggerable": "3",
            "task": secondTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": secondTestGroupOverrides.platform || '32',
            "testGroup": "2129",
            "order": "3",
            "commitSet": "4256",
            "status": secondTestGroupOverrides.status && secondTestGroupOverrides.status[3] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16993",
            "triggerable": "3",
            "task": thirdTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": thirdTestGroupOverrides.platform || '32',
            "testGroup": "2130",
            "order": "-2",
            "commitSet": "4255",
            "status": thirdTestGroupOverrides.status && thirdTestGroupOverrides.status[0] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688513000
        }, {
            "id": "16994",
            "triggerable": "3",
            "task": thirdTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": thirdTestGroupOverrides.platform || '32',
            "testGroup": "2130",
            "order": "-1",
            "commitSet": "4256",
            "status": thirdTestGroupOverrides.status && thirdTestGroupOverrides.status[1] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16995",
            "triggerable": "3",
            "task": thirdTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": thirdTestGroupOverrides.platform || '32',
            "testGroup": "2130",
            "order": "0",
            "commitSet": "4255",
            "status": thirdTestGroupOverrides.status && thirdTestGroupOverrides.status[2] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16996",
            "triggerable": "3",
            "task": secondTestGroupOverrides.task || '1376',
            "test": "844",
            "platform": secondTestGroupOverrides.platform || '32',
            "testGroup": "2130",
            "order": "1",
            "commitSet": "4256",
            "status": thirdTestGroupOverrides.status && thirdTestGroupOverrides.status[3] || 'pending',
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }],
        "commitSets": [{
            "id": "4255",
            "revisionItems": [{"commit": "87832", rootFile: 101}, {"commit": "93116"}],
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
        "uploadedFiles": [{id: 101, filename: 'root-101', extension: '.tgz', size: 1,
            createdAt: yesterday, sha256: crypto.createHash('sha256').update('root-101').digest('hex')}],
        "status": "OK"
    };
}

describe('BuildRequest', function () {
    MockModels.inject();

    describe('findBuildRequestWithSameRoots', () => {
        const requests = MockRemoteAPI.inject('https://perf.webkit.org');

        it('should return null when the build request is not build type build request', async () => {
            const data = sampleBuildRequestData();
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const result = await request.findBuildRequestWithSameRoots();
            assert.equal(result, null);
        });

        it('should return null if this there is no other test group under current analysis task', async () => {
            const data = oneTestGroup();
            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(oneTestGroup());

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            const result = await promise;
            assert.equal(result, null);
        });

        it('should return completed build request if a completed reusable build request found', async () => {
            const overrides = {
                task: '1376',
                platform: '32',
                status: ['completed', 'pending', 'pending', 'pending']
            }
            const data = threeTestGroups(overrides);
            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(overrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            const result = await promise;
            assert.equal(result, BuildRequest.findById(16989))
        });

        it('should not reuse a root that is older than "maxReuseRootAge"', async () => {
            const overrides = {
                task: '1376',
                platform: '32',
                status: ['completed', 'pending', 'pending', 'pending']
            }
            const data = threeTestGroups(overrides);

            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(overrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 0.5});

            const result = await promise;
            assert.equal(result, null)
        });

        it('should not use cache while fetching test groups under analysis task', async () => {
            const overrides = {
                task: '1376',
                platform: '32',
                status: ['completed', 'pending', 'pending', 'pending']
            };
            const data = threeTestGroups(overrides);
            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            let promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(overrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            let result = await promise;
            assert.equal(result, BuildRequest.findById(16989))

            MockRemoteAPI.reset();
            promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(overrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            result = await promise;
            assert.equal(result, BuildRequest.findById(16989))
        });

        it('should only allow identical platform if the platform is not under a platform group', async () => {
            const overrides = {
                task: '1376',
                platform: '33',
                status: ['completed', 'pending', 'pending', 'pending']
            };
            const data = threeTestGroups(overrides);
            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            Platform.ensureSingleton('33', {id: '33', metrics: [], name: 'another platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(overrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            const result = await promise;
            assert.equal(result, null);
        });

        it('should allow different platform if the platforms are under the same platform group', async () => {
            const overrides = {
                task: '1376',
                platform: '33',
                status: ['completed', 'pending', 'pending', 'pending']
            };
            const data = threeTestGroups(overrides);
            const platformId = data.buildRequests[0].platform;
            const platformGroup = PlatformGroup.ensureSingleton('1', {id: 1, name: 'some platform group'});
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform', group: platformGroup});
            Platform.ensureSingleton('33', {id: '33', metrics: [], name: 'another platform', group: platformGroup});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(overrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            const result = await promise;
            assert.equal(result, BuildRequest.findById(16989))
        });

        it('should in favor of running build request over scheduled build request', async () => {
            const secondOverrides = {
                task: '1376',
                platform: '32',
                status: ['scheduled', 'pending', 'pending', 'pending']
            };
            const thirdOverrides = {
                task: '1376',
                platform: '32',
                status: ['running', 'pending', 'pending', 'pending']
            }
            const data = threeTestGroups(secondOverrides, thirdOverrides);
            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(secondOverrides, thirdOverrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            const result = await promise;
            assert.equal(result, BuildRequest.findById(16993))
        });

        it('should in favor of running build request which has earlier creation time', async () => {
            const secondOverrides = {
                task: '1376',
                platform: '32',
                status: ['running', 'pending', 'pending', 'pending']
            };
            const thirdOverrides = {
                task: '1376',
                platform: '32',
                status: ['running', 'pending', 'pending', 'pending']
            }
            const data = threeTestGroups(secondOverrides, thirdOverrides);
            const platformId = data.buildRequests[0].platform;
            Platform.ensureSingleton(platformId, {id: platformId, metrics: [], name: 'some platform'});
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            const promise = request.findBuildRequestWithSameRoots();
            assert.equal(requests.length, 1);

            assert.equal(requests[0].url, '/api/test-groups?task=1376');
            assert.equal(requests[0].method, 'GET');
            requests[0].resolve(threeTestGroups(secondOverrides, thirdOverrides));

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/data/manifest.json');
            assert.equal(requests[1].method, 'GET');
            requests[1].resolve({maxRootReuseAgeInDays: 30});

            const result = await promise;
            assert.equal(result, BuildRequest.findById(16993));
            assert.ok(BuildRequest.findById(16993).createdAt() < BuildRequest.findById(16989).createdAt());
        });
    });

    describe('waitingTime', function () {
        it('should return "0 minutes" when the reference time is identically equal to createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '0 minutes');
        });

        it('should return "1 minute" when the reference time is exactly one minute head of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - 60 * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '1 minute');
        });

        it('should return "1 minute" when the reference time is 75 seconds head of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - 75 * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '1 minute');
        });

        it('should return "2 minutes" when the reference time is 118 seconds head of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - 118 * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '2 minutes');
        });

        it('should return "75 minutes" when the reference time is 75 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - 75 * 60 * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '75 minutes');
        });

        it('should return "1 hour 58 minutes" when the reference time is 118 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - 118 * 60 * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '1 hour 58 minutes');
        });

        it('should return "3 hours 2 minutes" when the reference time is 182 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - 182 * 60 * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '3 hours 2 minutes');
        });

        it('should return "27 hours 14 minutes" when the reference time is 27 hours 14 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - (27 * 3600 + 14 * 60) * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '27 hours 14 minutes');
        });

        it('should return "2 days 3 hours" when the reference time is 51 hours 14 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - (51 * 3600 + 14 * 60) * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '2 days 3 hours');
        });

        it('should return "2 days 0 hours" when the reference time is 48 hours 1 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - (48 * 3600 + 1 * 60) * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '2 days 0 hours');
        });

        it('should return "2 days 2 hours" when the reference time is 49 hours 59 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - (49 * 3600 + 59 * 60) * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '2 days 2 hours');
        });

        it('should return "2 weeks 6 days" when the reference time is 20 days 5 hours 21 minutes ahead of createdAt', function () {
            const data = sampleBuildRequestData();
            const now = Date.now();
            data.buildRequests[0].createdAt = now - ((20 * 24 + 5) * 3600 + 21 * 60) * 1000;
            const request = BuildRequest.constructBuildRequestsFromData(data)[0];
            assert.equal(request.waitingTime(now), '2 weeks 6 days');
        });

    });

    describe('formatTimeInterval', () => {
        it('should return "0 minutes" when formatting for 0 second in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(0), '0 minutes');
        });

        it('should return "1 minute" when formatting for 60 seconds in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(60 * 1000), '1 minute');
        });

        it('should return "1 minute" when formatting for  75 seconds in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(75 * 1000), '1 minute');
        });

        it('should return "2 minutes" when formatting for 118 seconds in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(118 * 1000), '2 minutes');
        });

        it('should return "75 minutes" when formatting for 75 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(75 * 60 * 1000), '75 minutes');
        });

        it('should return "1 hour 58 minutes" when formatting for 118 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(118 * 60 * 1000), '1 hour 58 minutes');
        });

        it('should return "3 hours 2 minutes" when formatting for 182 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(182 * 60 * 1000), '3 hours 2 minutes');
        });

        it('should return "27 hours 14 minutes" when formatting for 27 hours 14 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval((27 * 3600 + 14 * 60) * 1000), '27 hours 14 minutes');
        });

        it('should return "2 days 3 hours" when formatting for 51 hours 14 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval((51 * 3600 + 14 * 60) * 1000), '2 days 3 hours');
        });

        it('should return "2 days 0 hours" when formatting for 48 hours 1 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval((48 * 3600 + 1 * 60) * 1000), '2 days 0 hours');
        });

        it('should return "2 days 2 hours" when formatting for 49 hours 59 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval((49 * 3600 + 59 * 60) * 1000), '2 days 2 hours');
        });

        it('should return "2 weeks 6 days" when formatting for 20 days 5 hours 21 minutes in million seconds', () => {
            assert.equal(BuildRequest.formatTimeInterval(((20 * 24 + 5) * 3600 + 21 * 60) * 1000), '2 weeks 6 days');
        });
    });

});