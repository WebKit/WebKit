'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addWorkerForReport = require('./resources/common-operations.js').addWorkerForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const submitReport = require('./resources/common-operations.js').submitReport;

describe("/api/commits/", function () {
    prepareServerTest(this);

    const subversionCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "revisionIdentifier": "184276@main",
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "revisionIdentifier": "184277@main",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
            {
                "repository": "WebKit",
                "previousCommit": "210949",
                "revision": "210950",
                "revisionIdentifier": "184278@main",
                "time": "2017-01-20T03:49:37.887Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "another message",
            },
        ]
    };

    const subcersionCommitsWithFakeRevisionIdentifier = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "revisionIdentifier": "184276@main",
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "revisionIdentifier": "184277@main",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
            {
                "repository": "WebKit",
                "previousCommit": "210949",
                "revision": "210950",
                "revisionIdentifier": "184278@main",
                "time": "2017-01-20T03:49:37.887Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "another message",
            },
            {
                "repository": "WebKit",
                "revision": "210951",
                "revisionIdentifier": "184278@something",
                "time": "2017-01-20T03:49:40.887Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "another message",
            }
        ]
    };

    const commitsOnePrefixOfTheOther = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "21094",
                "revisionIdentifier": "184272@main",
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "revisionIdentifier": "184277@main",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            }
        ]
    }

    const systemVersionCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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

    const report = [{
        "buildTag": "124",
        "buildTime": "2015-10-27T15:34:51",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "some platform",
        "tests": {"Speedometer-2": {"metrics": {"Score": {"current": [[100]]}}}},
        "revisions": {
            "WebKit": {
                "timestamp": "2017-01-20T02:52:34.577Z",
                "revision": "210948"
            }
        }
    }];

    function assertCommitIsSameAsOneSubmitted(commit, submitted)
    {
        assert.strictEqual(commit['revision'], submitted['revision']);
        assert.strictEqual(new Date(commit['time']).toISOString(), submitted['time']);
        assert.strictEqual(commit['message'], submitted['message']);
        assert.strictEqual(commit['authorName'], submitted['author']['name']);
        assert.strictEqual(commit['authorEmail'], submitted['author']['account']);
        if(submitted['previousCommit']) {
            assert.ok(commit['previousCommit']);
        } else {
            assert.strictEqual(commit['previousCommit'], null);
        }
    }

    describe('/api/commits/<repository>/', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit').then((response) => {
                assert.strictEqual(response['status'], 'RepositoryNotFound');
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
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return the list of all commits for a given repository", () => {
            return addWorkerForReport(subversionCommits).then(() => {
                return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommits);
            }).then(function (response) {
                assert.strictEqual(response['status'], 'OK');
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');

                const commits = result['commits'];
                assert.strictEqual(commits.length, 3);
                const submittedCommits = subversionCommits['commits'];
                assertCommitIsSameAsOneSubmitted(commits[0], submittedCommits[0]);
                assertCommitIsSameAsOneSubmitted(commits[1], submittedCommits[1]);
                assertCommitIsSameAsOneSubmitted(commits[2], submittedCommits[2]);
                assert.strictEqual(commits[2]['previousCommit'], commits[1]['id']);
            });
        });

        it("should return the list of ordered commits for a given repository", () => {
            return addWorkerForReport(subversionCommits).then(() => {
                return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommits);
            }).then(function (response) {
                assert.strictEqual(response['status'], 'OK');
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                const commits = result['commits'];
                const submittedCommits = systemVersionCommits['commits'];
                assert.strictEqual(commits.length, submittedCommits.length);
                assert.strictEqual(commits[0]['revision'], submittedCommits[5]['revision']);
                assert.strictEqual(commits[1]['revision'], submittedCommits[4]['revision']);
                assert.strictEqual(commits[2]['revision'], submittedCommits[3]['revision']);
                assert.strictEqual(commits[3]['revision'], submittedCommits[2]['revision']);
                assert.strictEqual(commits[4]['revision'], submittedCommits[1]['revision']);
                assert.strictEqual(commits[5]['revision'], submittedCommits[0]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/oldest', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/oldest').then((response) => {
                assert.strictEqual(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return an empty results when there are no commits", () => {
            return TestServer.database().insert('repositories', {'id': 1, 'name': 'WebKit'}).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/oldest');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return the oldest commit", () => {
            const remote = TestServer.remoteAPI();
            return addWorkerForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/oldest');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][0]);
            });
        });

        it("should return the oldest commit based on 'commit_order' when 'commit_time' is missing", () => {
            const remote = TestServer.remoteAPI();
            return addWorkerForReport(systemVersionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', systemVersionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/OSX/oldest');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
                assert.strictEqual(result['commits'][0]['revision'], systemVersionCommits['commits'][5]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/latest', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/latest').then((response) => {
                assert.strictEqual(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return an empty results when there are no commits", () => {
            return TestServer.database().insert('repositories', {'id': 1, 'name': 'WebKit'}).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/latest');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return the oldest commit", () => {
            const remote = TestServer.remoteAPI();
            return addWorkerForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/latest');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'].slice().pop());
            });
        });

        it("should return the latest commit based on 'commit_order' when 'commit_time' is missing", () => {
            const remote = TestServer.remoteAPI();
            return addWorkerForReport(systemVersionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', systemVersionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/OSX/latest');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
                assert.strictEqual(result['commits'][0]['revision'], systemVersionCommits['commits'][0]['revision']);
            });
        });

        it("should always return a commit as long as there is an existing 'current' type test run for a given platform", async () => {
            const remote = TestServer.remoteAPI();
            const db = TestServer.database();
            await db.insert('tests', {name: 'A-Test'});
            await submitReport(report);
            await db.query(`DELETE FROM tests WHERE test_name = 'A-Test'`);

            const platforms = await db.selectAll('platforms');
            assert.strictEqual(platforms.length, 1);

            const test_metrics = await db.selectAll('test_metrics');
            assert.strictEqual(test_metrics.length, 1);

            const tests = await db.selectAll('tests');
            assert.strictEqual(tests.length, 1);

            assert(test_metrics[0].id != tests[0].id);

            const response = await remote.getJSON(`/api/commits/WebKit/latest?platform=${platforms[0].id}`);
            assert(response.commits.length);
        });
    });

    describe('/api/commits/<repository>/last-reported', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/last-reported').then((response) => {
                assert.strictEqual(response['status'], 'RepositoryNotFound');
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
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return an empty results when there are no reported commits", () => {
            return TestServer.database().insert('repositories', {'id': 1, 'name': 'WebKit'}).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/last-reported');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return the oldest reported commit", () => {
            const db = TestServer.database();
            const remote = TestServer.remoteAPI();
            return Promise.all([
                addWorkerForReport(subversionCommits),
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': notYetReportedCommit.revision, 'time': notYetReportedCommit.time}),
            ]).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/last-reported');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'].slice().pop());
            });
        });

        it("should return the last reported commit based on 'commit_order' when 'commit_time' is missing", () => {
            const remote = TestServer.remoteAPI();
            return addWorkerForReport(systemVersionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', systemVersionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/OSX/last-reported');
            }).then(function (result) {
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
                assert.strictEqual(result['commits'][0]['revision'], systemVersionCommits['commits'][0]['revision']);
            });
        });
    });

    describe('/api/commits/<repository>/last-reported?from=<start_order>&to=<end_order>', () => {
        it("should return a list of commit in given valid order range", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'OSX'}),
                db.insert('commits', {'repository': 1, 'revision': 'Sierra16C67', 'order': 367, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': 'Sierra16C68', 'order': 368, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': 'Sierra16C69', 'order': 369, 'reported': false}),
                db.insert('commits', {'repository': 1, 'revision': 'Sierra16D32', 'order': 432, 'reported': true})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=367&to=370');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                const results = response['commits'];
                assert.strictEqual(results.length, 1);
                const commit = results[0];
                assert.strictEqual(commit.revision, 'Sierra16C68');
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=370&to=367');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                const results = response['commits'];
                assert.strictEqual(results.length, 0);
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=200&to=299');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                const results = response['commits'];
                assert.strictEqual(results.length, 0);
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=369&to=432');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                const results = response['commits'];
                assert.strictEqual(results.length, 1);
                const commit = results[0];
                assert.strictEqual(commit.revision, 'Sierra16D32');
            });
        });
    });

    describe('/api/commits/<repository>/<commit>', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/210949').then((response) => {
                assert.strictEqual(response['status'], 'RepositoryNotFound');
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
                assert.strictEqual(response['status'], 'UnknownCommit');
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
                assert.strictEqual(result['status'], 'OK');
                assert.strictEqual(result['commits'].length, 1);
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
            return addWorkerForReport(subversionCommits).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/210949');
            }).then((result) => {
                assert.strictEqual(result['status'], 'OK');
                assert.deepStrictEqual(result['commits'].length, 1);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][1]);
            });
        });

        it("should return the full result for a reported commit with prefix-match to be false", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subversionCommits);
            await remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            const result = await remote.getJSON('/api/commits/WebKit/210949?prefix-match=false');
            assert.strictEqual(result['status'], 'OK');
            assert.deepStrictEqual(result['commits'].length, 1);
            assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][1]);
        });

        it("should return the full result for a reported commit with prefix-match to be true", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subversionCommits);
            await remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            const result = await remote.getJSON('/api/commits/WebKit/210949?prefix-match=true');
            assert.strictEqual(result['status'], 'OK');
            assert.deepStrictEqual(result['commits'].length, 1);
            assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][1]);
        });

        it("should return 'AmbiguousRevisionPrefix' when more than one commits are found for a revision prefix", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subversionCommits);
            await remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            const result = await remote.getJSON('/api/commits/WebKit/21094?prefix-match=true');
            assert.strictEqual(result['status'], 'AmbiguousRevisionPrefix');
        });

        it("should not return 'AmbiguousRevisionPrefix' when there is a commit revision extract matches specified revision prefix", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(commitsOnePrefixOfTheOther);
            await remote.postJSONWithStatus('/api/report-commits/', commitsOnePrefixOfTheOther);
            const result = await remote.getJSON('/api/commits/WebKit/21094?prefix-match=true');
            assert.strictEqual(result['status'], 'OK');
            assert.deepStrictEqual(result['commits'].length, 1);
            assertCommitIsSameAsOneSubmitted(result['commits'][0], commitsOnePrefixOfTheOther['commits'][0]);
        });

        it("should return 'UnknownCommit' when no commit is found for a revision prefix", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subversionCommits);
            await remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            const result = await remote.getJSON('/api/commits/WebKit/21090?prefix-match=true');
            assert.strictEqual(result['status'], 'UnknownCommit');
        });

        it("should not match prefix and return 'UnkownCommit' when svn commit starts with 'r' prefix and there is no exact match", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subversionCommits);
            await remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            const result = await remote.getJSON('/api/commits/WebKit/r21095?prefix-match=true');
            assert.strictEqual(result['status'], 'UnknownCommit');
        });

        it("should handle commit revision with space", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'OS X'}),
                db.insert('commits', {'repository': 1, 'revision': '10.11.10 Sierra16C67', 'order': 367, 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OS%20X/10.11.10%20Sierra16C67');
            }).then((results) => {
                assert.strictEqual(results.status, 'OK');
                assert.strictEqual(results.commits.length, 1);

                const commit = results.commits[0];
                assert.strictEqual(parseInt(commit.id), 1);
                assert.strictEqual(commit.revision, '10.11.10 Sierra16C67');
            });
        });

        it("should return commit with commit revision label", async () => {
            await addWorkerForReport(subversionCommits);
            const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommits);
            assert.strictEqual(response['status'], 'OK');
            const result = await TestServer.remoteAPI().getJSON(`/api/commits/WebKit/${subversionCommits.commits[0].revisionIdentifier}`);
            assert.strictEqual(result['status'], 'OK');
            assert.strictEqual(result.commits.length, 1);
            assertCommitIsSameAsOneSubmitted(result.commits[0], subversionCommits.commits[0]);
        });

        it("should return 'AmbiguousRevisionPrefix' when more than one commits are found for a revision label prefix", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subcersionCommitsWithFakeRevisionIdentifier);
            await remote.postJSONWithStatus('/api/report-commits/', subcersionCommitsWithFakeRevisionIdentifier);
            const result = await remote.getJSON('/api/commits/WebKit/184278@?prefix-match=true');
            assert.strictEqual(result['status'], 'AmbiguousRevisionPrefix');
        });

        it("should not return 'AmbiguousRevisionPrefix' when there is a commit revision label extract matches specified revision prefix", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subcersionCommitsWithFakeRevisionIdentifier);
            await remote.postJSONWithStatus('/api/report-commits/', subcersionCommitsWithFakeRevisionIdentifier);
            const result = await remote.getJSON('/api/commits/WebKit/184278@main?prefix-match=true');
            assert.strictEqual(result['status'], 'OK');
            assert.deepStrictEqual(result['commits'].length, 1);
            assertCommitIsSameAsOneSubmitted(result['commits'][0], subcersionCommitsWithFakeRevisionIdentifier['commits'][2]);
        });

        it("should return 'UnknownCommit' when no commit is found for a revision label prefix", async () => {
            const remote = TestServer.remoteAPI();
            await addWorkerForReport(subcersionCommitsWithFakeRevisionIdentifier);
            await remote.postJSONWithStatus('/api/report-commits/', subcersionCommitsWithFakeRevisionIdentifier);
            const result = await remote.getJSON('/api/commits/WebKit/184278@x?prefix-match=true');
            assert.strictEqual(result['status'], 'UnknownCommit');
        });
    });

    describe('/api/commits/<repository>/owned-commits?owner-revision=<commit>', () => {
        it("should return owned commits for a given commit", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('repositories', {'id': 2, 'name': 'WebKit', 'owner': 1}),
                db.insert('commits', {'id': 1, 'repository': 1, 'revision': '10.12 16A323', order: 1, 'reported': true}),
                db.insert('commits', {'id': 2, 'repository': 2, 'revision': '210950', 'reported': true}),
                db.insert('commit_ownerships', {'owner': 1, 'owned': 2})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/1/owned-commits?owner-revision=10.12%2016A323')
            }).then((results) => {
                assert.strictEqual(results.status, 'OK');
                assert.strictEqual(results.commits.length, 1);

                const ownedCommit = results.commits[0];
                assert.strictEqual(parseInt(ownedCommit.repository), 2);
                assert.strictEqual(ownedCommit.revision, '210950');
                assert.strictEqual(parseInt(ownedCommit.id), 2);
            });
        });

        it("should return an empty list of commits if no owned-commit is associated with given commit", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('repositories', {'id': 2, 'name': 'WebKit'}),
                db.insert('commits', {'id': 1, 'repository': 1, 'revision': '10.12 16A323', order: 1, 'reported': true}),
                db.insert('commits', {'id': 2, 'repository': 2, 'revision': '210950', 'reported': true})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/1/owned-commits?owner-revision=10.12%2016A323')
            }).then((results) => {
                assert.strictEqual(results.status, 'OK');
                assert.deepStrictEqual(results.commits, []);
            });
        });

        it("should return an empty list if commit revision is invalid", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('repositories', {'id': 2, 'name': 'WebKit'}),
                db.insert('commits', {'id': 1, 'repository': 1, 'revision': '10.12 16A323', order: 1, 'reported': true}),
                db.insert('commits', {'id': 2, 'repository': 2, 'revision': '210950', 'reported': true})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/1/owned-commits?owner-revision=10.12%2016A324')
            }).then((results) => {
                assert.strictEqual(results.status, 'OK');
                assert.strictEqual(results.commits.length, 0);
            });
        })
    });

    describe('/api/commits/<repository>/?precedingRevision=<commit-1>&lastRevision=<commit-2>', () => {
        it("should return RepositoryNotFound when there are no matching repository", () => {
            return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?from=210900&to=211000').then((response) => {
                assert.strictEqual(response['status'], 'RepositoryNotFound');
            });
        });

        it("should return UnknownCommit when one of the specified commit does not exist in the database", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'})
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?precedingRevision=210900&lastRevision=211000');
            }).then((response) => {
                assert.strictEqual(response['status'], 'UnknownCommit');
            });
        });

        it("should return an empty result when commits in the specified range have not been reported", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210949', 'time': '2017-01-20T03:23:50.645Z'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?precedingRevision=210949&lastRevision=210950');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return InvalidCommitRange when the specified range is backwards", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210949', 'time': '2017-01-20T03:23:50.645Z'}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z'}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?precedingRevision=210950&lastRevision=210949');
            }).then((response) => {
                assert.strictEqual(response['status'], 'InvalidCommitRange');
            });
        });

        it("should return use the commit order when time is not specified", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16A323', order: 1, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2555', order: 2, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2657', order: 3, 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/macOS/?precedingRevision=10.12%2016A323&lastRevision=10.12%2016B2657');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'].map((commit) => commit['revision']), ['10.12 16B2555', '10.12 16B2657']);
            });
        });

        it("should return InconsistentCommits when precedingRevision specifies a time but lastRevision does not", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16A323', time: '2017-01-20T03:23:50.645Z', order: 1, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2555', order: 2, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2657', order: 3, 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/macOS/?precedingRevision=10.12%2016A323&lastRevision=10.12%2016B2657');
            }).then((response) => {
                assert.strictEqual(response['status'], 'InconsistentCommits');
            });
        });

        it("should return InconsistentCommits when precedingRevision does not specify a time has a time but lastRevision does", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16A323', order: 1, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2555', order: 2, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2657', time: '2017-01-20T03:23:50.645Z', order: 3, 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/macOS/?precedingRevision=10.12%2016A323&lastRevision=10.12%2016B2657');
            }).then((response) => {
                assert.strictEqual(response['status'], 'InconsistentCommits');
            });
        });

        it("should return empty results when precedingRevision does not specify a time or an order has a time but lastRevision does", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16A323', 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2555', order: 2, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2657', order: 3, 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/macOS/?precedingRevision=10.12%2016A323&lastRevision=10.12%2016B2657');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return empty results when precedingRevision an order has a time but lastRevision does not", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'macOS'}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16A323', order: 1, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2555', order: 2, 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '10.12 16B2657', 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/macOS/?precedingRevision=10.12%2016A323&lastRevision=10.12%2016B2657');
            }).then((response) => {
                assert.strictEqual(response['status'], 'OK');
                assert.deepStrictEqual(response['commits'], []);
            });
        });

        it("should return reported commits in the specified range", () => {
            const db = TestServer.database();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210948', 'time': '2017-01-20T02:52:34.577Z', 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '210949', 'time': '2017-01-20T03:23:50.645Z', 'reported': true}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z', 'reported': true}),
            ]).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/WebKit/?precedingRevision=210948&lastRevision=210950');
            }).then((result) => {
                assert.strictEqual(result['status'], 'OK');
                assert.deepStrictEqual(result['commits'].length, 2);
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

        it("should return reported commits in the specified revision label range", async () => {
            const db = TestServer.database();
            await db.insert('repositories', {'id': 1, 'name': 'WebKit'});
            await db.insert('commits', {'repository': 1, 'revision': '210948', 'revision_identifier': '184276@main', 'time': '2017-01-20T02:52:34.577Z', 'reported': true});
            await db.insert('commits', {'repository': 1, 'revision': '210949', 'revision_identifier': '184277@main', 'time': '2017-01-20T03:23:50.645Z', 'reported': true});
            await db.insert('commits', {'repository': 1, 'revision': '210950', 'revision_identifier': '184278@main', 'time': '2017-01-20T03:49:37.887Z', 'reported': true});
            const result = await TestServer.remoteAPI().getJSON('/api/commits/WebKit/?precedingRevision=184276@main&lastRevision=184278@main');
            assert.strictEqual(result['status'], 'OK');
            assert.deepStrictEqual(result['commits'].length, 2);
            assertCommitIsSameAsOneSubmitted(result['commits'][0], {
                previousCommit: null,
                revision: '210949',
                revisionIdentifier: '184289@main',
                time: '2017-01-20T03:23:50.645Z',
                author: {name: null, account: null},
                message: null,
            });
            assertCommitIsSameAsOneSubmitted(result['commits'][1], {
                previousCommit: null,
                revision: '210950',
                revisionIdentifier: '184290@main',
                time: '2017-01-20T03:49:37.887Z',
                author: {name: null, account: null},
                message: null,
            });
        });

        it("should not include a revision not within the specified range", () => {
            const db = TestServer.database();
            const remote = TestServer.remoteAPI();
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': '210947', 'time': '2017-01-20T02:38:45.485Z', 'reported': false}),
                db.insert('commits', {'repository': 1, 'revision': '210948', 'time': '2017-01-20T02:52:34.577Z', 'reported': false}),
                db.insert('commits', {'repository': 1, 'revision': '210949', 'time': '2017-01-20T03:23:50.645Z', 'reported': false}),
                db.insert('commits', {'repository': 1, 'revision': '210950', 'time': '2017-01-20T03:49:37.887Z', 'reported': false}),
            ]).then(() => {
                return addWorkerForReport(subversionCommits);
            }).then(() => {
                return remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            }).then(() => {
                return remote.getJSON('/api/commits/WebKit/?precedingRevision=210947&lastRevision=210949');
            }).then((result) => {
                assert.strictEqual(result['status'], 'OK');
                assert.deepStrictEqual(result['commits'].length, 2);
                assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][0]);
                assertCommitIsSameAsOneSubmitted(result['commits'][1], subversionCommits['commits'][1]);
            });
        });

        it("should not include a revision not within the specified commit revision label range", async () => {
            const db = TestServer.database();
            const remote = TestServer.remoteAPI();
            await db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
            await db.insert('commits', {'repository': 1, 'revision': '210947', 'revision_identifier': '184275@main', 'time': '2017-01-20T02:38:45.485Z', 'reported': false});
            await db.insert('commits', {'repository': 1, 'revision': '210948', 'revision_identifier': '184276@main', 'time': '2017-01-20T02:52:34.577Z', 'reported': false});
            await db.insert('commits', {'repository': 1, 'revision': '210949', 'revision_identifier': '184277@main', 'time': '2017-01-20T03:23:50.645Z', 'reported': false});
            await db.insert('commits', {'repository': 1, 'revision': '210950', 'revision_identifier': '184278@main', 'time': '2017-01-20T03:49:37.887Z', 'reported': false});
            await addWorkerForReport(subversionCommits);
            await remote.postJSONWithStatus('/api/report-commits/', subversionCommits);
            const result = await remote.getJSON('/api/commits/WebKit/?precedingRevision=184275@main&lastRevision=184277@main');
            assert.strictEqual(result['status'], 'OK');
            assert.deepStrictEqual(result['commits'].length, 2);
            assertCommitIsSameAsOneSubmitted(result['commits'][0], subversionCommits['commits'][0]);
            assertCommitIsSameAsOneSubmitted(result['commits'][1], subversionCommits['commits'][1]);
        });

    });

});
