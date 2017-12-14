'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');
let MockModels = require('./resources/mock-v3-models.js').MockModels;

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

describe('BuildRequest', function () {
    MockModels.inject();

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