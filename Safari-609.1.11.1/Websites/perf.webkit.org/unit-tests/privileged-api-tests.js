'use strict';

const assert = require('assert');
require('../tools/js/v3-models.js');
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;
const NodePrivilegedAPI = require('../tools/js/privileged-api').PrivilegedAPI;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;

describe('BrowserPrivilegedAPI', () => {
    const requests = MockRemoteAPI.inject(null, BrowserPrivilegedAPI);

    beforeEach(() => {
        PrivilegedAPI._token = null;
    });

    describe('requestCSRFToken', () => {
        it('should generate a new token', () => {
            PrivilegedAPI.requestCSRFToken();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
        });

        it('should not generate a new token if the existing token had not expired', () => {
            const tokenRequest = PrivilegedAPI.requestCSRFToken();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            return tokenRequest.then((token) => {
                assert.equal(token, 'abc');
                PrivilegedAPI.requestCSRFToken();
                assert.equal(requests.length, 1);
            });
        });

        it('should generate a new token if the existing token had already expired', () => {
            const tokenRequest = PrivilegedAPI.requestCSRFToken();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() - 1,
            });
            return tokenRequest.then((token) => {
                assert.equal(token, 'abc');
                PrivilegedAPI.requestCSRFToken();
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/privileged-api/generate-csrf-token');
            });
        });
    });

    describe('sendRequest', () => {

        it('should generate a new token if no token had been fetched', () => {
            const promise = PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 100 * 1000,
            });
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/privileged-api/test');
            });
        });

        it('should not generate a new token if the existing token had not been expired', () => {
            PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/privileged-api/test');
                PrivilegedAPI.sendRequest('test2', {});
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '/privileged-api/test2');
            });
        });

        it('should reject immediately when a token generation had failed', () => {
            const request = PrivilegedAPI.sendRequest('test', {});
            let caught = false;
            request.catch(() => { caught = true; });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].reject({status: 'FailedToGenerateToken'});
            return new Promise((resolve) => setTimeout(resolve, 0)).then(() => {
                assert.equal(requests.length, 1);
                assert(caught);
            });
        });

        it('should re-generate token when it had become invalid', () => {
            PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].data.token, 'abc');
                assert.equal(requests[1].url, '/privileged-api/test');
                requests[1].reject('InvalidToken');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '/privileged-api/generate-csrf-token');
                requests[2].resolve({
                    token: 'def',
                    expiration: Date.now() + 3600 * 1000,
                });
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.equal(requests.length, 4);
                assert.equal(requests[3].data.token, 'def');
                assert.equal(requests[3].url, '/privileged-api/test');
            });
        });

        it('should not re-generate token when the re-fetched token was invalid', () => {
            const request = PrivilegedAPI.sendRequest('test', {});
            let caught = false;
            request.catch(() => caught = true);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].data.token, 'abc');
                assert.equal(requests[1].url, '/privileged-api/test');
                requests[1].reject('InvalidToken');
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '/privileged-api/generate-csrf-token');
                requests[2].resolve({
                    token: 'def',
                    expiration: Date.now() + 3600 * 1000,
                });
                return MockRemoteAPI.waitForRequest();
            }).then(() => {
                assert.equal(requests.length, 4);
                assert.equal(requests[3].data.token, 'def');
                assert.equal(requests[3].url, '/privileged-api/test');
                requests[3].reject('InvalidToken');
                return new Promise((resolve) => setTimeout(resolve, 0));
            }).then(() => {
                assert(caught);
                assert.equal(requests.length, 4);
            });
        });

    });

});

describe('NodePrivilegedAPI', () => {
    let requests = MockRemoteAPI.inject(null, NodePrivilegedAPI);
    beforeEach(() => {
        PrivilegedAPI.configure('slave_name', 'password');
    });

    describe('sendRequest', () => {
        it('should post slave name and password in data', async () => {
            const request = PrivilegedAPI.sendRequest('test', {foo: 'bar'});

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/test');
            assert.equal(requests[0].method, 'POST');
            assert.deepEqual(requests[0].data,  {foo: 'bar', slaveName: 'slave_name', slavePassword: 'password'});

            requests[0].resolve({test: 'success'});
            const result = await request;
            assert.deepEqual(result, {test: 'success'});
        });
    });

});
