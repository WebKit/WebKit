'use strict';

require('../tools/js/v3-models.js');

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const connectToDatabaseInEveryTest = require('./resources/common-operations.js').connectToDatabaseInEveryTest;

describe("/privileged-api/update-run-status", function () {
    this.timeout(1000);
    TestServer.inject();
    connectToDatabaseInEveryTest();

    const reportWithRevision = [{
        "buildNumber": "124",
        "buildTime": "2013-02-28T15:34:51",
        "revisions": {
            "WebKit": {
                "revision": "191622",
                "timestamp": (new Date(1445945816878)).toISOString(),
            },
        },
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "some platform",
        "tests": {
            "Suite": {
                "tests": {
                    "test1": {
                        "metrics": {"Time": { "current": [11] }}
                    }
                }
            },
        }}];

    it("should be able to mark a run as an outlier", function (done) {
        const db = TestServer.database();
        let id;
        addBuilderForReport(reportWithRevision[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['mean_cache'], 11);
            assert.equal(runRows[0]['iteration_count_cache'], 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            id = runRows[0]['id'];
            return PrivilegedAPI.requestCSRFToken();
        }).then(function () {
            return PrivilegedAPI.sendRequest('update-run-status', {'run': id, 'markedOutlier': true, 'token': PrivilegedAPI._token});
        }).then(function () {
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['mean_cache'], 11);
            assert.equal(runRows[0]['iteration_count_cache'], 1);
            assert.equal(runRows[0]['marked_outlier'], true);
            done();
        }).catch(done);
    });

    it("should reject when the token is not set in cookie", function (done) {
        const db = TestServer.database();
        addBuilderForReport(reportWithRevision[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            return PrivilegedAPI.requestCSRFToken();
        }).then(function () {
            RemoteAPI.clearCookies();
            return RemoteAPI.postJSONWithStatus('/privileged-api/update-run-status', {token: PrivilegedAPI._token});
        }).then(function () {
            assert(false, 'PrivilegedAPI.sendRequest should reject');
        }, function (response) {
            assert.equal(response['status'], 'InvalidToken');
            done();
        }).catch(done);
    });

    it("should reject when the token in the request content is bad", function (done) {
        const db = TestServer.database();
        addBuilderForReport(reportWithRevision[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            return PrivilegedAPI.requestCSRFToken();
        }).then(function () {
            return RemoteAPI.postJSONWithStatus('/privileged-api/update-run-status', {token: 'bad'});
        }).then(function () {
            assert(false, 'PrivilegedAPI.sendRequest should reject');
        }, function (response) {
            assert.equal(response['status'], 'InvalidToken');
            done();
        }).catch(done);
    });

    it("should be able to unmark a run as an outlier", function (done) {
        const db = TestServer.database();
        addBuilderForReport(reportWithRevision[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            return PrivilegedAPI.sendRequest('update-run-status', {'run': runRows[0]['id'], 'markedOutlier': true});
        }).then(function () {
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], true);
            return PrivilegedAPI.sendRequest('update-run-status', {'run': runRows[0]['id'], 'markedOutlier': false});
        }).then(function () {
            return db.selectAll('test_runs');
        }).then(function (runRows) {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            done();
        }).catch(done);
    });
});
