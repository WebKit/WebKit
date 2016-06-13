'use strict';

const assert = require('assert');

let MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
require('../tools/js/v3-models.js');

describe('PrivilegedAPI', function () {
    let requests = MockRemoteAPI.inject();

    beforeEach(function () {
        PrivilegedAPI._token = null;
    })

    describe('requestCSRFToken', function () {
        it('should generate a new token', function () {
            PrivilegedAPI.requestCSRFToken();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
        });

        it('should not generate a new token if the existing token had not expired', function (done) {
            const tokenRequest = PrivilegedAPI.requestCSRFToken();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            tokenRequest.then(function (token) {
                assert.equal(token, 'abc');
                PrivilegedAPI.requestCSRFToken();
                assert.equal(requests.length, 1);
                done();
            }).catch(done);
        });

        it('should generate a new token if the existing token had already expired', function (done) {
            const tokenRequest = PrivilegedAPI.requestCSRFToken();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() - 1,
            });
            tokenRequest.then(function (token) {
                assert.equal(token, 'abc');
                PrivilegedAPI.requestCSRFToken();
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/privileged-api/generate-csrf-token');
                done();
            }).catch(done);
        });
    });
    
    describe('sendRequest', function () {

        it('should generate a new token if no token had been fetched', function (done) {
            PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 100 * 1000,
            });
            Promise.resolve().then(function () {
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/privileged-api/test');
                done();
            }).catch(done);
        });

        it('should not generate a new token if the existing token had not been expired', function (done) {
            PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            Promise.resolve().then(function () {
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/privileged-api/test');
                PrivilegedAPI.sendRequest('test2', {});
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '/privileged-api/test2');
                done();
            }).catch(done);
        });

        it('should reject immediately when a token generation had failed', function (done) {
            const request = PrivilegedAPI.sendRequest('test', {});
            let caught = false;
            request.catch(function () { caught = true; });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].reject({status: 'FailedToGenerateToken'});
            Promise.resolve().then(function () {
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 1);
                assert(caught);
                done();
            }).catch(done);
        });

        it('should re-generate token when it had become invalid', function (done) {
            PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            Promise.resolve().then(function () {
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].data.token, 'abc');
                assert.equal(requests[1].url, '/privileged-api/test');
                requests[1].reject('InvalidToken');
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '/privileged-api/generate-csrf-token');
                requests[2].resolve({
                    token: 'def',
                    expiration: Date.now() + 3600 * 1000,
                });
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 4);
                assert.equal(requests[3].data.token, 'def');
                assert.equal(requests[3].url, '/privileged-api/test');
                done();
            }).catch(done);
        });

        it('should not re-generate token when the re-fetched token was invalid', function (done) {
            PrivilegedAPI.sendRequest('test', {});
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });
            Promise.resolve().then(function () {
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].data.token, 'abc');
                assert.equal(requests[1].url, '/privileged-api/test');
                requests[1].reject('InvalidToken');
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '/privileged-api/generate-csrf-token');
                requests[2].resolve({
                    token: 'def',
                    expiration: Date.now() + 3600 * 1000,
                });
                return Promise.resolve();
            }).then(function () {
                assert.equal(requests.length, 4);
                assert.equal(requests[3].data.token, 'def');
                assert.equal(requests[3].url, '/privileged-api/test');
                requests[3].reject('InvalidToken');
            }).then(function () {
                assert.equal(requests.length, 4);
                done();
            }).catch(done);
        });

    });

});
