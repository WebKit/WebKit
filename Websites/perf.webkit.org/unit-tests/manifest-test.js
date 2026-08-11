'use strict';

const assert = require('assert');
require('../tools/js/v3-models.js');
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
const assertThrows = require('../server-tests/resources/common-operations').assertThrows;

const manifestSampleResponse = {
    siteTitle: 'some title',
    all: {},
    bugTrackers: {},
    builders: {},
    dashboard: {},
    dashboards: {},
    fileUploadSizeLimit: 1,
    maxRootReuseAgeInDays: null,
    metrics: {},
    platformGroups: {},
    repositories: {},
    testAgeToleranceInHours: null,
    tests: {},
    triggerables: {},
    summaryPages: [],
    status: 'OK'
}

describe('Manifest', () => {
    const requests = MockRemoteAPI.inject('https://perf.webkit.org', BrowserPrivilegedAPI);

    describe('fetchRawResponse', () => {
        it('should fetch from "/data/manifest.json" if the file is available', async () => {
            const fetchingTask = Manifest.fetchRawResponse();
            assert.equal(requests.length, 1);
            assert(requests[0].url, '/data/manifest.json')
            requests[0].resolve(manifestSampleResponse);

            const rawResponse = await fetchingTask;
            assert.deepEqual(rawResponse, manifestSampleResponse);
        });

        it('should fetch from api only when fetching "/data/manifest.json" returns 404', async () => {
            const fetchingTask = Manifest.fetchRawResponse();
            assert.equal(requests.length, 1);
            assert(requests[0].url, '/data/manifest.json')
            requests[0].reject(404);

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert(requests[1].url, '/data/manifest.json')
            requests[1].resolve(manifestSampleResponse);

            const rawResponse = await fetchingTask;
            assert.deepEqual(rawResponse, manifestSampleResponse);
        });

        it('should not fetch from api if fetching "/data/manifest.json" returns non-404', async () => {
            const fetchingTask = Manifest.fetchRawResponse();
            assert.equal(requests.length, 1);
            assert(requests[0].url, '/data/manifest.json')
            requests[0].reject(301);
            assertThrows('Failed to fetch manifest.json with 301', () => fetchingTask);
        });
    });
});