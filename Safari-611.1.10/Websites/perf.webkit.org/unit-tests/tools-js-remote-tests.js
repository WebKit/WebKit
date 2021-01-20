'use strict';

const assert = require('assert');

const RemoteAPI = require('../tools/js/remote.js').RemoteAPI;

describe('RemoteAPI', function () {
    describe('configure', function () {
        it('should accept a valid configuration', function () {
            let remote = new RemoteAPI();
            assert.doesNotThrow(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80}); });
        });

        it('should reject a scheme other than http and https', function () {
            let remote = new RemoteAPI();
            assert.doesNotThrow(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80}); });
            assert.doesNotThrow(function () { remote.configure({scheme: 'https', host: 'build.webkit.org', port: 9999}); });
            assert.throws(function () { remote.configure({scheme: 'ftp', host: 'build.webkit.org', port: 9999}); });
        });

        it('should accept a configuration without port', function () {
            let remote = new RemoteAPI();
            assert.doesNotThrow(function () { remote.configure({scheme: 'http', host: 'build.webkit.org'}); });
        });

        it('should reject non-string value for scheme', function () {
            let remote = new RemoteAPI();
            assert.throws(function () { remote.configure({scheme: 1, host: 'build.webkit.org', port: 80}); });
            assert.throws(function () { remote.configure({scheme: {}, host: 'build.webkit.org', port: 80}); });
            assert.throws(function () { remote.configure({scheme: [], host: 'build.webkit.org', port: 80}); });
        });

        it('should reject non-string value for host', function () {
            let remote = new RemoteAPI();
            assert.throws(function () { remote.configure({scheme: 'http', host: 1, port: 80}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: {}, port: 80}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: [], port: 80}); });
        });

        it('should reject non-object value for auth', function () {
            let remote = new RemoteAPI();
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: 'a'}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: 1}); });
        });

        it('should accept a configuration with auth', function () {
            let remote = new RemoteAPI();
            assert.doesNotThrow(function () {
                remote.configure({
                    scheme: 'http',
                    host: 'build.webkit.org',
                    port: 80,
                    auth: {
                        'username': 'a',
                        'password': 'b',
                    },
                });
            });
        });

        it('should reject auth without username or password or when either is not a string', function () {
            let remote = new RemoteAPI();
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: {}}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: {username: 'a'}}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: {password: 'b'}}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: {username: 1, password: 'a'}}); });
            assert.throws(function () { remote.configure({scheme: 'http', host: 'build.webkit.org', port: 80, auth: {username: 'a', password: 1}}); });
        });
    });

    describe('url', function () {
        it('should be able to serialize http://build.webkit.org', function () {
            let remote = new RemoteAPI({scheme: 'http', host: 'build.webkit.org', port: 80});
            assert.equal(remote.url('/json/builders'), 'http://build.webkit.org/json/builders');
        });

        it('should be able to serialize http://build.webkit.org when port is not specified', function () {
            let remote = new RemoteAPI({scheme: 'http', host: 'build.webkit.org'});
            assert.equal(remote.url('/json/builders'), 'http://build.webkit.org/json/builders');
        });

        it('should be able to serialize https://build.webkit.org when port is 80', function () {
            let remote = new RemoteAPI({scheme: 'http', host: 'build.webkit.org', port: 80});
            assert.equal(remote.url('/json/builders'), 'http://build.webkit.org/json/builders');
        });

        it('should be able to serialize http://build.webkit.org:8080', function () {
            let remote = new RemoteAPI({scheme: 'http', host: 'build.webkit.org', port: 8080});
            assert.equal(remote.url('/json/builders'), 'http://build.webkit.org:8080/json/builders');
        });

        it('should be able to serialize https://build.webkit.org when port is not specified', function () {
            let remote = new RemoteAPI({scheme: 'https', host: 'build.webkit.org'});
            assert.equal(remote.url('/json/builders'), 'https://build.webkit.org/json/builders');
        });

        it('should be able to serialize https://build.webkit.org when port is 443', function () {
            let remote = new RemoteAPI({scheme: 'https', host: 'build.webkit.org', port: 443});
            assert.equal(remote.url('/json/builders'), 'https://build.webkit.org/json/builders');
        });

        it('should be able to serialize https://build.webkit.org:8443', function () {
            let remote = new RemoteAPI({scheme: 'https', host: 'build.webkit.org', port: 8443});
            assert.equal(remote.url('/json/builders'), 'https://build.webkit.org:8443/json/builders');
        });

        it('should automatically prefix the path with /', function () {
            let remote = new RemoteAPI({scheme: 'http', host: 'build.webkit.org'});
            assert.equal(remote.url('json/builders'), 'http://build.webkit.org/json/builders');
        });
    });
});
