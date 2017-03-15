'use strict';

require('../tools/js/v3-models.js');

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe("/privileged-api/update-run-status", function () {
    prepareServerTest(this);

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

    it("should be able to mark a run as an outlier", () => {
        const db = TestServer.database();
        let id;
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['mean_cache'], 11);
            assert.equal(runRows[0]['iteration_count_cache'], 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            id = runRows[0]['id'];
            return PrivilegedAPI.requestCSRFToken();
        }).then(() => {
            return PrivilegedAPI.sendRequest('update-run-status', {'run': id, 'markedOutlier': true, 'token': PrivilegedAPI._token});
        }).then(() => {
            return db.selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['mean_cache'], 11);
            assert.equal(runRows[0]['iteration_count_cache'], 1);
            assert.equal(runRows[0]['marked_outlier'], true);
        });
    });

    it("should reject when the token is not set in cookie", () => {
        const db = TestServer.database();
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            return PrivilegedAPI.requestCSRFToken();
        }).then(() => {
            RemoteAPI.clearCookies();
            return RemoteAPI.postJSONWithStatus('/privileged-api/update-run-status', {token: PrivilegedAPI._token});
        }).then(() => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidToken');
        });
    });

    it("should reject when the token in the request content is bad", () => {
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            return PrivilegedAPI.requestCSRFToken();
        }).then(() => {
            return RemoteAPI.postJSONWithStatus('/privileged-api/update-run-status', {token: 'bad'});
        }).then(() => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'InvalidToken');
        });
    });

    it("should be able to unmark a run as an outlier", () => {
        const db = TestServer.database();
        return addBuilderForReport(reportWithRevision[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', reportWithRevision);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
            return PrivilegedAPI.sendRequest('update-run-status', {'run': runRows[0]['id'], 'markedOutlier': true});
        }).then(() => {
            return db.selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], true);
            return PrivilegedAPI.sendRequest('update-run-status', {'run': runRows[0]['id'], 'markedOutlier': false});
        }).then(() => {
            return db.selectAll('test_runs');
        }).then((runRows) => {
            assert.equal(runRows.length, 1);
            assert.equal(runRows[0]['marked_outlier'], false);
        });
    });
});
