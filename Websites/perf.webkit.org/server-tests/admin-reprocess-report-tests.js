'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const connectToDatabaseInEveryTest = require('./resources/common-operations.js').connectToDatabaseInEveryTest;

describe("/admin/reprocess-report", function () {
    this.timeout(1000);
    TestServer.inject();
    connectToDatabaseInEveryTest();

    const simpleReport = [{
        "buildNumber": "1986",
        "buildTime": "2013-02-28T10:12:03",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {
                "test": {
                    "metrics": {"FrameRate": { "current": [[1, 2, 3], [4, 5, 6]] }}
                },
            },
        }];

    it("should add build", function (done) {
        let db = TestServer.database();
        let reportId;
        addBuilderForReport(simpleReport[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', simpleReport);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('builds'), db.selectAll('reports')]);
        }).then(function (result) {
            const builds = result[0];
            const reports = result[1];
            assert.equal(builds.length, 1);
            assert.equal(builds[0]['number'], 1986);
            assert.equal(reports.length, 1);
            reportId = reports[0]['id'];
            assert.equal(reports[0]['build_number'], 1986);
            return db.query('UPDATE reports SET report_build = NULL; DELETE FROM builds');
        }).then(function () {
            return db.selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 0);
            return TestServer.remoteAPI().getJSONWithStatus(`/admin/reprocess-report?report=${reportId}`);
        }).then(function () {
            return db.selectAll('builds');
        }).then(function (builds) {
            assert.equal(builds.length, 1);
            assert.equal(builds[0]['number'], 1986);
            done();
        }).catch(done);
    });

    it("should not duplicate the reprocessed report", function (done) {
        let db = TestServer.database();
        let originalReprot;
        addBuilderForReport(simpleReport[0]).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report/', simpleReport);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 1);
            originalReprot = reports[0];
            return db.query('UPDATE reports SET report_build = NULL; DELETE FROM builds');
        }).then(function () {
            return TestServer.remoteAPI().getJSONWithStatus(`/admin/reprocess-report?report=${originalReprot['id']}`);
        }).then(function () {
            return db.selectAll('reports');
        }).then(function (reports) {
            assert.equal(reports.length, 1);
            const newPort = reports[0];
            originalReprot['committed_at'] = null;
            newPort['committed_at'] = null;
            assert.notEqual(originalReprot['build'], newPort['build']);
            originalReprot['build'] = null;
            newPort['build'] = null;
            assert.deepEqual(originalReprot, newPort);
            done();
        }).catch(done);
    });
});
