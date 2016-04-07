'use strict';

const assert = require('assert');
const crypto = require('crypto');

const TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const connectToDatabaseInEveryTest = require('./resources/common-operations.js').connectToDatabaseInEveryTest;

describe("/api/report-commits/", function () {
    this.timeout(1000);
    TestServer.inject();
    connectToDatabaseInEveryTest();

    const emptyReport = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
    };
    const subversionCommit = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "141977",
                "time": "2013-02-06T08:55:20.9Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "some message",
            }
        ],
    };
    const subversionInvalidCommit = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "_141977",
                "time": "2013-02-06T08:55:20.9Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "some message",
            }
        ],
    };
    const subversionTwoCommits = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "141977",
                "time": "2013-02-06T08:55:20.9Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "some message",
            },
            {
                "repository": "WebKit",
                "parent": "141977",
                "revision": "141978",
                "time": "2013-02-06T09:54:56.0Z",
                "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                "message": "another message",
            }
        ]
    }

    it("should reject error when slave name is missing", function (done) {
        TestServer.remoteAPI().postJSON('/api/report-commits/', {}).then(function (response) {
            assert.equal(response['status'], 'MissingSlaveName');
            done();
        }).catch(done);
    });

    it("should reject when there are no slaves", function (done) {
        TestServer.remoteAPI().postJSON('/api/report-commits/', emptyReport).then(function (response) {
            assert.equal(response['status'], 'SlaveNotFound');
            return TestServer.database().selectAll('commits');
        }).then(function (rows) {
            assert.equal(rows.length, 0);
            done();
        }).catch(done);
    });

    it("should accept an empty report", function (done) {
        addSlaveForReport(emptyReport).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', emptyReport);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            done();
        }).catch(done);
    });

    it("should add a missing repository", function (done) {
        const db = TestServer.database();
        addSlaveForReport(subversionCommit).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('repositories');
        }).then(function (rows) {
            assert.equal(rows.length, 1);
            assert.equal(rows[0]['name'], subversionCommit.commits[0]['repository']);
            done();
        }).catch(done);
    });

    it("should store a commit from a valid slave", function (done) {
        const db = TestServer.database();
        addSlaveForReport(subversionCommit).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then(function (result) {
            let commits = result[0];
            let committers = result[1];
            let reportedData = subversionCommit.commits[0];

            assert.equal(commits.length, 1);
            assert.equal(committers.length, 1);
            assert.equal(commits[0]['revision'], reportedData['revision']);
            assert.equal(commits[0]['time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
            assert.equal(commits[0]['message'], reportedData['message']);
            assert.equal(commits[0]['committer'], committers[0]['id']);
            assert.equal(committers[0]['name'], reportedData['author']['name']);
            assert.equal(committers[0]['account'], reportedData['author']['account']);

            done();
        }).catch(done);
    });

    it("should reject an invalid revision number", function (done) {
        addSlaveForReport(subversionCommit).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionInvalidCommit);
        }).then(function (response) {
            assert.equal(response['status'], 'InvalidRevision');
            return TestServer.database().selectAll('commits');
        }).then(function (rows) {
            assert.equal(rows.length, 0);
            done();
        }).catch(done);
    });

    it("should store two commits from a valid slave", function (done) {
        const db = TestServer.database();
        addSlaveForReport(subversionTwoCommits).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionTwoCommits);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then(function (result) {
            const commits = result[0];
            const committers = result[1];
            assert.equal(commits.length, 2);
            assert.equal(committers.length, 2);

            let reportedData = subversionTwoCommits.commits[0];
            assert.equal(commits[0]['revision'], reportedData['revision']);
            assert.equal(commits[0]['time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
            assert.equal(commits[0]['message'], reportedData['message']);
            assert.equal(commits[0]['committer'], committers[0]['id']);
            assert.equal(committers[0]['name'], reportedData['author']['name']);
            assert.equal(committers[0]['account'], reportedData['author']['account']);

            reportedData = subversionTwoCommits.commits[1];
            assert.equal(commits[1]['revision'], reportedData['revision']);
            assert.equal(commits[1]['time'].toString(), new Date('2013-02-06 09:54:56.0').toString());
            assert.equal(commits[1]['message'], reportedData['message']);
            assert.equal(commits[1]['committer'], committers[1]['id']);
            assert.equal(committers[1]['name'], reportedData['author']['name']);
            assert.equal(committers[1]['account'], reportedData['author']['account']);

            done();
        }).catch(done);
    });

    it("should update an existing commit if there is one", function (done) {
        const db = TestServer.database();
        const reportedData = subversionCommit.commits[0];
        addSlaveForReport(subversionCommit).then(function () {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': reportedData['revision'], 'time': reportedData['time']})
            ]);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then(function (result) {
            const commits = result[0];
            const committers = result[1];

            assert.equal(commits.length, 1);
            assert.equal(committers.length, 1);
            assert.equal(commits[0]['message'], reportedData['message']);
            assert.equal(commits[0]['committer'], committers[0]['id']);
            assert.equal(committers[0]['name'], reportedData['author']['name']);
            assert.equal(committers[0]['account'], reportedData['author']['account']);

            done();
        }).catch(done);
    });

    it("should not update an unrelated commit", function (done) {
        const db = TestServer.database();
        const firstData = subversionTwoCommits.commits[0];
        const secondData = subversionTwoCommits.commits[1];
        addSlaveForReport(subversionCommit).then(function () {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'id': 2, 'repository': 1, 'revision': firstData['revision'], 'time': firstData['time']}),
                db.insert('commits', {'id': 3, 'repository': 1, 'revision': secondData['revision'], 'time': secondData['time']})
            ]);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then(function (result) {
            const commits = result[0];
            const committers = result[1];

            assert.equal(commits.length, 2);
            assert.equal(committers.length, 1);
            assert.equal(commits[0]['id'], 2);
            assert.equal(commits[0]['message'], firstData['message']);
            assert.equal(commits[0]['committer'], committers[0]['id']);
            assert.equal(committers[0]['name'], firstData['author']['name']);
            assert.equal(committers[0]['account'], firstData['author']['account']);
            
            assert.equal(commits[1]['id'], 3);
            assert.equal(commits[1]['message'], null);
            assert.equal(commits[1]['committer'], null);

            done();
        }).catch(done);
    });

    it("should update an existing committer if there is one", function (done) {
        const db = TestServer.database();
        const author = subversionCommit.commits[0]['author'];
        addSlaveForReport(subversionCommit).then(function () {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('committers', {'repository': 1, 'account': author['account']}),
            ]);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('committers');
        }).then(function (committers) {
            assert.equal(committers.length, 1);
            assert.equal(committers[0]['name'], author['name']);
            assert.equal(committers[0]['account'], author['account']);
            done();
        }).catch(done);
    });

});
