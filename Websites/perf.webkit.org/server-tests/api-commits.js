'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe("/api/commits/", function () {
    prepareServerTest(this);

    const subversionCommits = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
            {
                "repository": "WebKit",
                "previousCommit": "210949",
                "revision": "210950",
                "time": "2017-01-20T03:49:37.887Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "another message",
            }
        ]
    }

    const systemVersionCommits = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "16D32",
                "order": 6
            },
            {
                "repository": "OSX",
                "revision": "16C68",
                "order": 5
            },
            {
                "repository": "OSX",
                "revision": "16C67",
                "order": 4
            },
            {
                "repository": "OSX",
                "revision": "16B2657",
                "order": 3
            },
            {
                "repository": "OSX",
                "revision": "16B2555",
                "order": 2
            },
            {
                "repository": "OSX",
                "revision": "16A323",
                "order": 1
            }
        ]
    }

    const notYetReportedCommit = {
        revision: '210951',
        time: '2017-01-20T03:56:20.045Z'
    }

    function assertCommitIsSameAsOneSubmitted(commit, submitted)
    {
        assert.equal(commit['revision'], submitted['revision']);
        assert.equal(new Date(commit['time']).toISOString(), submitted['time']);
        assert.equal(commit['message'], submitted['message']);
        assert.equal(commit['authorName'], submitted['author']['name']);
        assert.equal(commit['authorEmail'], submitted['author']['account']);
        if(submitted['previousCommit']) {
            assert.ok(commit['previousCommit']);
        } else {
            assert.equal(commit['previousCommit'], null);
        }
    }

    describe('/api/commits/<repository>/', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit').then((response) => {
                assert.equal(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return an empty result when there are no reported commits", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit');
            }).then((response) => {
                assert.equal(response['status'], 'OK');
                assert.deepEqual(response['commits'], []);
            });
        });

        it("should return the list of all commits for a given repository", () => {
            return addSlaveForReport(subversionCommits).then(() => {
                return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommits);
            }).then(function (response) {
                assert.equal(response['status'], 'OK');
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');

                const commits = result['commits'];
                assert.equal(commits.length, 3);
                const submittedCommits = subversionCommits['commits'];
                assertCommitIsSameAsOneSubmitted(commits[0], submittedCommits[0]);
                assertCommitIsSameAsOneSubmitted(commits[1], submittedCommits[1]);
                assertCommitIsSameAsOneSubmitted(commits[2], submittedCommits[2]);
                assert.equal(commits[2]['previousCommit'], commits[1]['id']);
            });
        });

        it("should return the list of ordered commits for a given repository", () => {
            return addSlaveForReport(subversionCommits).then(() => {
                return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommits);
            }).then(function (response) {
                assert.equal(response['status'], 'OK');
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                const commits = result['commits'];
                const submittedCommits = systemVersionCommits['commits'];
                assert.equal(commits.length, submittedCommits.length);
                assert.equal(commits[0]['revision'], submittedCommits[5]['revision']);
                assert.equal(commits[1]['revision'], submittedCommits[4]['revision']);
                assert.equal(commits[2]['revision'], submittedCommits[3]['revision']);
                assert.equal(commits[3]['revision'], submittedCommits[2]['revision']);
                assert.equal(commits[4]['revision'], submittedCommits[1]['revision']);
                assert.equal(commits[5]['revision'], submittedCommits[0]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/oldest', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/oldest').then((response) => {
                assert.equal(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return an empty results when there are no commits", () => {
            return TestServer.database().insert('repositories', {'id': 1, 'name': 'WebKit'}).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/oldest');
            }).then((response) => {
                assert.equal(response['status'], 'OK');
                assert.deepEqual(response['commits'], []);
            });
        });

        it("should return the oldest commit", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/oldest');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][0]);
            });
        });

        it("should return the oldest commit based on 'commit_order' when 'commit_time' is missing", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(systemVersionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', systemVersionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/OSX/oldest');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], systemVersionCommits['commits'][5]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/latest', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/latest').then((response) => {
                assert.equal(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return an empty results when there are no commits", () => {
            return TestServer.database().insert('repositories', {'id': 1, 'name': 'WebKit'}).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/latest');
            }).then((response) => {
                assert.equal(response['status'], 'OK');
                assert.deepEqual(response['commits'], []);
            });
        });

        it("should return the oldest commit", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/latest');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'].slice().pop());
            });
        });

        it("should return the latest commit based on 'commit_order' when 'commit_time' is missing", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(systemVersionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', systemVersionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/OSX/latest');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], systemVersionCommits['commits'][0]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/last-reported', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/last-reported').then((response) => {
                assert.equal(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return an empty result when there are no reported commits", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/last-reported');
            }).then((response) => {
                assert.equal(response['status'], 'OK');
                assert.deepEqual(response['commits'], []);
            });
        });

        it("should return an empty results when there are no reported commits", () => {
            return TestServer.database().insert('repositories', {'id': 1, 'name': 'WebKit'}).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/last-reported');
            }).then((response) => {
                assert.equal(response['status'], 'OK');
                assert.deepEqual(response['commits'], []);
            });
        });

        it("should return the oldest reported commit", () => {
            const db = TestServer.database();
            const remote = TestServer.remoteAPI();
            return Promise.all([
                addSlaveForReport(subversionCommits),
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': notYetReportedCommit.revision, 'time': notYetReportedCommit.time}),
            ]).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/last-reported');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'].slice().pop());
            });
        });

        it("should return the last reported commit based on 'commit_order' when 'commit_time' is missing", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(systemVersionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', systemVersionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/OSX/last-reported');
            }).then(function (result) {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], systemVersionCommits['commits'][0]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/<commit>', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/210949').then((response) => {
                assert.equal(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return UnknownCommit when one of the specified commit does not exist in the database", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/210949');
            }).then((response) => {
                assert.equal(response['status'], 'UnknownCommit');
            });
        });

        it("should return the commit even if it had not been reported", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/210950');
            }).then((result) => {
                assert.equal(result['status'], 'OK');
                assert.equal(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], {
                    previousCommit: null,
                    revision: '210950',
                    time: '2017-01-20T03:49:37.887Z',
                    author: {name: null, account: null},
                    message: null,
                });
            });
        });

        it("should return the full result for a reported commit", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/210949');
            }).then((result) => {
                assert.equal(result['status'], 'OK');
                assert.deepEqual(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][1]);
            });
        });

    });

    describe('/api/commits/<repository>/?from=<commit-1>&to=<commit-2>', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?from=210900&to=211000').then((response) => {
                assert.equal(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return UnknownCommit when one of the specified commit does not exist in the database", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?from=210900&to=211000');
            }).then((response) => {
                assert.equal(response['status'], 'UnknownCommit');
            });
        });

        it("should return an empty result when commits in the specified range have not reported", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210949', 'time': '2017-01-20T03:23:50.645Z'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?from=210949&to=210950');
            }).then((response) => {
                assert.equal(response['status'], 'OK');
                assert.deepEqual(response['commits'], []);
            });
        });

        it("should return reported commits in the specified range", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210949', 'time': '2017-01-20T03:23:50.645Z', 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z', 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?from=210949&to=210950');
            }).then((result) => {
                assert.equal(result['status'], 'OK');
                assert.deepEqual(result['commits'].length, 2);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], {
                    previousCommit: null,
                    revision: '210949',
                    time: '2017-01-20T03:23:50.645Z',
                    author: {name: null, account: null},
                    message: null,
                });
                assertCommitIsSameAsOneSubmitted(result['commits'][1], {
                    previousCommit: null,
                    revision: '210950',
                    time: '2017-01-20T03:49:37.887Z',
                    author: {name: null, account: null},
                    message: null,
                });
            });
        });

        it("should not include a revision not within the specified range", () => {
            const remote = TestServer.remoteAPI();
            return addSlaveForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/?from=210948&to=210949');
            }).then((result) => {
                assert.equal(result['status'], 'OK');
                assert.deepEqual(result['commits'].length, 2);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][0]);
                assertCommitIsSameAsOneSubmitted(result['commits'][1], subversionCommits['commits'][1]);
            });
        });

    });

});
