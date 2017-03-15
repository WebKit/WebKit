'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addBuilderForReport = require('./resources/common-operations.js').addBuilderForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe("/admin/reprocess-report", function () {
    prepareServerTest(this);

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

    const simpleReportWithRevisions = [{
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
        "revisions": {
                "WebKit": {
                    "timestamp": "2017-03-01T09:38:44.826833Z",
                    "revision": "213214"
                }
            }
        }];

    it("should still create new repository when repository ownerships are different", () => {
        let db = TestServer.database();
        return addBuilderForReport(simpleReportWithRevisions[0]).then(() => {
            return db.insert('repositories', {'name': 'WebKit', 'owner': 1});
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', simpleReportWithRevisions);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectRows('repositories', {'name': 'WebKit'});
        }).then((repositories) => {
            assert.equal(repositories.length, 2);
            const webkitRepsitoryId = repositories[0].owner == 1 ? repositories[1].id : repositories[0].id;
            return db.selectRows('commits', {'revision': '213214', 'repository': webkitRepsitoryId});
        }).then((result) => {
            assert(result.length, 1);
        });
    });

    it("should add build", () => {
        let db = TestServer.database();
        let reportId;
        return addBuilderForReport(simpleReport[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', simpleReport);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('builds'), db.selectAll('reports')]);
        }).then((result) => {
            const builds = result[0];
            const reports = result[1];
            assert.equal(builds.length, 1);
            assert.equal(builds[0]['number'], 1986);
            assert.equal(reports.length, 1);
            reportId = reports[0]['id'];
            assert.equal(reports[0]['build_number'], 1986);
            return db.query('UPDATE reports SET report_build = NULL; DELETE FROM builds');
        }).then(() => {
            return db.selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 0);
            return TestServer.remoteAPI().getJSONWithStatus(`/admin/reprocess-report?report=${reportId}`);
        }).then(() => {
            return db.selectAll('builds');
        }).then((builds) => {
            assert.equal(builds.length, 1);
            assert.equal(builds[0]['number'], 1986);
        });
    });

    it("should not duplicate the reprocessed report", () => {
        let db = TestServer.database();
        let originalReprot;
        return addBuilderForReport(simpleReport[0]).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report/', simpleReport);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 1);
            originalReprot = reports[0];
            return db.query('UPDATE reports SET report_build = NULL; DELETE FROM builds');
        }).then(() => {
            return TestServer.remoteAPI().getJSONWithStatus(`/admin/reprocess-report?report=${originalReprot['id']}`);
        }).then(() => {
            return db.selectAll('reports');
        }).then((reports) => {
            assert.equal(reports.length, 1);
            const newPort = reports[0];
            originalReprot['committed_at'] = null;
            newPort['committed_at'] = null;
            assert.notEqual(originalReprot['build'], newPort['build']);
            originalReprot['build'] = null;
            newPort['build'] = null;
            assert.deepEqual(originalReprot, newPort);
        });
    });
});
