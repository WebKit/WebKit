'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addWorkerForReport = require('./resources/common-operations.js').addWorkerForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe("/api/report-commits/ with insert=true", function () {
    prepareServerTest(this);

    const emptyReport = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
    };
    const subversionCommit = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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
    const subversionCommitWithRevisionIdentifier = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "141977",
                "revisionIdentifier": "127231@main",
                "time": "2013-02-06T08:55:20.9Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "some message",
            }
        ],
    };
    const subversionInvalidCommit = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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
                "previousCommit": "141977",
                "revision": "141978",
                "time": "2013-02-06T09:54:56.0Z",
                "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                "message": "another message",
            }
        ]
    };

    const subversionInvalidPreviousCommit = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "previousCommit": "99999",
                "revision": "12345",
                "time": "2013-02-06T08:55:20.9Z",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "some message",
            }
        ]
    }

    const duplicatedCommitRevisionIdentifierCommits = {
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
                "revisionIdentifier": "184276@main",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
        ]
    }

    const emptyRevisionIdentifierCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "revisionIdentifier": null,
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "previousCommit": "210948",
                "revisionIdentifier": null,
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
        ]
    }

    const emptyRevisionIdentifierAndValidRevisionIdentifierCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "revisionIdentifier": null,
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "previousCommit": "210948",
                "revisionIdentifier": "184276@main",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
        ]
    }

    const emptyRevisionIdentifierWithInvalidRevisionIdentifierCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "revisionIdentifier": null,
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "revisionIdentifier": "",
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
            {
                "repository": "WebKit",
                "revision": "210950",
                "revisionIdentifier": false,
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            },
            {
                "repository": "WebKit",
                "revision": "210950",
                "revisionIdentifier": 0,
                "time": "2017-01-20T03:23:50.645Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
                "message": "some message",
            }
        ]
    }

    const invalidCommitRevisionIdentifierCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "revisionIdentifier": "184276",
                "time": "2017-01-20T02:52:34.577Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
                "message": "a message",
            },
        ]
    }

    it("should reject error when worker name is missing", () => {
        return TestServer.remoteAPI().postJSON('/api/report-commits/', {}).then((response) => {
            assert.strictEqual(response['status'], 'MissingWorkerName');
        });
    });

    it("should reject when there are no workers", () => {
        return TestServer.remoteAPI().postJSON('/api/report-commits/', emptyReport).then((response) => {
            assert.strictEqual(response['status'], 'WorkerNotFound');
            return TestServer.database().selectAll('commits');
        }).then((rows) => {
            assert.strictEqual(rows.length, 0);
        });
    });

    it("should accept an empty report", () => {
        return addWorkerForReport(emptyReport).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', emptyReport);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
        });
    });

    it("should add a missing repository", () => {
        return addWorkerForReport(subversionCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return TestServer.database().selectAll('repositories');
        }).then((rows) => {
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(rows[0]['name'], subversionCommit.commits[0]['repository']);
        });
    });

    it("should store a commit from a valid worker", () => {
        return addWorkerForReport(subversionCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const db = TestServer.database();
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
            let commits = result[0];
            let committers = result[1];
            let reportedData = subversionCommit.commits[0];

            assert.strictEqual(commits.length, 1);
            assert.strictEqual(committers.length, 1);
            assert.strictEqual(commits[0]['revision'], reportedData['revision']);
            assert.strictEqual(commits[0]['time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
            assert.strictEqual(commits[0]['message'], reportedData['message']);
            assert.strictEqual(commits[0]['committer'], committers[0]['id']);
            assert.strictEqual(committers[0]['name'], reportedData['author']['name']);
            assert.strictEqual(committers[0]['account'], reportedData['author']['account']);
        });
    });

    it("should reject an invalid revision number", () => {
        return addWorkerForReport(subversionCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionInvalidCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRevision');
            return TestServer.database().selectAll('commits');
        }).then((rows) => {
            assert.strictEqual(rows.length, 0);
        });
    });

    it("should reject with invalid revision identifier with empty revision identifier", async () => {
        await addWorkerForReport(subversionCommit);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', emptyRevisionIdentifierWithInvalidRevisionIdentifierCommits);
        assert.strictEqual(response['status'], 'InvalidRevisionIdentifier');
        const rows = await TestServer.database().selectAll('commits');
        assert.strictEqual(rows.length, 0);
    });

    it("should reject an invalid revision identifier", async () => {
        await addWorkerForReport(subversionCommit);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', invalidCommitRevisionIdentifierCommits);
        assert.strictEqual(response['status'], 'InvalidRevisionIdentifier');
        const rows = await TestServer.database().selectAll('commits');
        assert.strictEqual(rows.length, 0);
    });

    it("should reject with duplicated commit revision identifiers", async () => {
        await addWorkerForReport(subversionCommit);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', duplicatedCommitRevisionIdentifierCommits);
        assert.strictEqual(response['status'], 'DuplicatedRevisionIdentifier');
        const rows = await TestServer.database().selectAll('commits');
        assert.strictEqual(rows.length, 0);
    });

    it("should store two commits with empty revision identifier", async () => {
        await addWorkerForReport(emptyRevisionIdentifierCommits);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', emptyRevisionIdentifierCommits);
        assert.strictEqual(response['status'], 'OK');
        const db = TestServer.database();
        const result = await Promise.all([db.selectAll('commits'), db.selectAll('committers')]);

        const commits = result[0];
        const committers = result[1];
        assert.strictEqual(commits.length, 2);
        assert.strictEqual(committers.length, 2);

        let reportedData = emptyRevisionIdentifierCommits.commits[0];
        assert.strictEqual(commits[0]['revision'], reportedData['revision']);
        assert.strictEqual(commits[0]['revision_identifier'], null);
        assert.strictEqual(commits[0]['time'].toString(), new Date('2017-01-20 02:52:34.577').toString());
        assert.strictEqual(commits[0]['message'], reportedData['message']);
        assert.strictEqual(commits[0]['committer'], committers[0]['id']);
        assert.strictEqual(commits[0]['previous_commit'], null);
        assert.strictEqual(committers[0]['name'], reportedData['author']['name']);
        assert.strictEqual(committers[0]['account'], reportedData['author']['account']);

        reportedData = emptyRevisionIdentifierCommits.commits[1];
        assert.strictEqual(commits[1]['revision'], reportedData['revision']);
        assert.strictEqual(commits[0]['revision_identifier'], null);
        assert.strictEqual(commits[1]['time'].toString(), new Date('2017-01-20 03:23:50.645').toString());
        assert.strictEqual(commits[1]['message'], reportedData['message']);
        assert.strictEqual(commits[1]['committer'], committers[1]['id']);
        assert.strictEqual(commits[1]['previous_commit'], commits[0]['id']);
        assert.strictEqual(committers[1]['name'], reportedData['author']['name']);
        assert.strictEqual(committers[1]['account'], reportedData['author']['account']);

    });

    it("should store two commits with one empty revision identifier and one valid revision identifier", async () => {
        await addWorkerForReport(emptyRevisionIdentifierAndValidRevisionIdentifierCommits);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', emptyRevisionIdentifierAndValidRevisionIdentifierCommits);
        assert.strictEqual(response['status'], 'OK');
        const db = TestServer.database();
        const result = await Promise.all([db.selectAll('commits'), db.selectAll('committers')]);

        const commits = result[0];
        const committers = result[1];
        assert.strictEqual(commits.length, 2);
        assert.strictEqual(committers.length, 2);

        let reportedData = emptyRevisionIdentifierAndValidRevisionIdentifierCommits.commits[0];
        assert.strictEqual(commits[0]['revision'], reportedData['revision']);
        assert.strictEqual(commits[0]['revision_identifier'], null);
        assert.strictEqual(commits[0]['time'].toString(),  new Date('2017-01-20 02:52:34.577').toString());
        assert.strictEqual(commits[0]['message'], reportedData['message']);
        assert.strictEqual(commits[0]['committer'], committers[0]['id']);
        assert.strictEqual(commits[0]['previous_commit'], null);
        assert.strictEqual(committers[0]['name'], reportedData['author']['name']);
        assert.strictEqual(committers[0]['account'], reportedData['author']['account']);

        reportedData = emptyRevisionIdentifierAndValidRevisionIdentifierCommits.commits[1];
        assert.strictEqual(commits[1]['revision'], reportedData['revision']);
        assert.strictEqual(commits[1]['revision_identifier'], reportedData['revisionIdentifier']);
        assert.strictEqual(commits[1]['time'].toString(),  new Date('2017-01-20 03:23:50.645').toString());
        assert.strictEqual(commits[1]['message'], reportedData['message']);
        assert.strictEqual(commits[1]['committer'], committers[1]['id']);
        assert.strictEqual(commits[1]['previous_commit'], commits[0]['id']);
        assert.strictEqual(committers[1]['name'], reportedData['author']['name']);
        assert.strictEqual(committers[1]['account'], reportedData['author']['account']);
    });

    it("should store two commits from a valid worker", () => {
        return addWorkerForReport(subversionTwoCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionTwoCommits);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const db = TestServer.database();
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
            const commits = result[0];
            const committers = result[1];
            assert.strictEqual(commits.length, 2);
            assert.strictEqual(committers.length, 2);

            let reportedData = subversionTwoCommits.commits[0];
            assert.strictEqual(commits[0]['revision'], reportedData['revision']);
            assert.strictEqual(commits[0]['time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
            assert.strictEqual(commits[0]['message'], reportedData['message']);
            assert.strictEqual(commits[0]['committer'], committers[0]['id']);
            assert.strictEqual(commits[0]['previous_commit'], null);
            assert.strictEqual(committers[0]['name'], reportedData['author']['name']);
            assert.strictEqual(committers[0]['account'], reportedData['author']['account']);

            reportedData = subversionTwoCommits.commits[1];
            assert.strictEqual(commits[1]['revision'], reportedData['revision']);
            assert.strictEqual(commits[1]['time'].toString(), new Date('2013-02-06 09:54:56.0').toString());
            assert.strictEqual(commits[1]['message'], reportedData['message']);
            assert.strictEqual(commits[1]['committer'], committers[1]['id']);
            assert.strictEqual(commits[1]['previous_commit'], commits[0]['id']);
            assert.strictEqual(committers[1]['name'], reportedData['author']['name']);
            assert.strictEqual(committers[1]['account'], reportedData['author']['account']);
        });
    });

    it("should fail if previous commit is invalid", () => {
        return addWorkerForReport(subversionInvalidPreviousCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionInvalidPreviousCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'FailedToFindPreviousCommit');
            return TestServer.database().selectAll('commits');
        }).then((result) => {
            assert.strictEqual(result.length, 0);
        });
    });

    it("should update an existing commit if there is one", () => {
        const db = TestServer.database();
        const reportedData = subversionCommit.commits[0];
        return addWorkerForReport(subversionCommit).then(() => {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': reportedData['revision'], 'time': reportedData['time']})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
            const commits = result[0];
            const committers = result[1];

            assert.strictEqual(commits.length, 1);
            assert.strictEqual(committers.length, 1);
            assert.strictEqual(commits[0]['message'], reportedData['message']);
            assert.strictEqual(commits[0]['committer'], committers[0]['id']);
            assert.strictEqual(committers[0]['name'], reportedData['author']['name']);
            assert.strictEqual(committers[0]['account'], reportedData['author']['account']);
        });
    });

    it("should update an existing commit with revision identifier if there is one", async () => {
        const db = TestServer.database();
        const reportedData = subversionCommitWithRevisionIdentifier.commits[0];
        await addWorkerForReport(subversionCommitWithRevisionIdentifier);
        await db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
        await db.insert('commits', {'repository': 1, 'revision': reportedData['revision'], 'time': reportedData['time']})
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommitWithRevisionIdentifier);
        assert.strictEqual(response['status'], 'OK');
        const commits = await db.selectAll('commits');
        const committers = await db.selectAll('committers');
        assert.strictEqual(commits.length, 1);
        assert.strictEqual(committers.length, 1);
        assert.strictEqual(commits[0]['message'], reportedData['message']);
        assert.strictEqual(commits[0]['revision_identifier'], reportedData['revisionIdentifier']);
        assert.strictEqual(commits[0]['committer'], committers[0]['id']);
        assert.strictEqual(committers[0]['name'], reportedData['author']['name']);
        assert.strictEqual(committers[0]['account'], reportedData['author']['account']);
    });

    it("should not update an unrelated commit", () => {
        const db = TestServer.database();
        const firstData = subversionTwoCommits.commits[0];
        const secondData = subversionTwoCommits.commits[1];
        return addWorkerForReport(subversionCommit).then(() => {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'id': 2, 'repository': 1, 'revision': firstData['revision'], 'time': firstData['time']}),
                db.insert('commits', {'id': 3, 'repository': 1, 'revision': secondData['revision'], 'time': secondData['time']})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
            const commits = result[0];
            const committers = result[1];

            assert.strictEqual(commits.length, 2);
            assert.strictEqual(committers.length, 1);
            assert.strictEqual(commits[0]['id'], 2);
            assert.strictEqual(commits[0]['message'], firstData['message']);
            assert.strictEqual(commits[0]['committer'], committers[0]['id']);
            assert.strictEqual(committers[0]['name'], firstData['author']['name']);
            assert.strictEqual(committers[0]['account'], firstData['author']['account']);

            assert.strictEqual(commits[1]['id'], 3);
            assert.strictEqual(commits[1]['message'], null);
            assert.strictEqual(commits[1]['committer'], null);
        });
    });

    it("should update an existing committer if there is one", () => {
        const db = TestServer.database();
        const author = subversionCommit.commits[0]['author'];
        return addWorkerForReport(subversionCommit).then(() => {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('committers', {'repository': 1, 'account': author['account']}),
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return db.selectAll('committers');
        }).then((committers) => {
            assert.strictEqual(committers.length, 1);
            assert.strictEqual(committers[0]['name'], author['name']);
            assert.strictEqual(committers[0]['account'], author['account']);
        });
    });

    const sameRepositoryNameInOwnedCommitAndMajorCommit = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    },
                    "JavaScriptCore": {
                        "revision": "141978",
                        "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                        "message": "JavaScriptCore commit",
                    }
                }
            },
            {
                "repository": "WebKit",
                "revision": "141978",
                "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                "message": "WebKit Commit",
            }
        ]
    };

    it("should distinguish between repositories with the same name but with a different owner.", () => {
        return addWorkerForReport(sameRepositoryNameInOwnedCommitAndMajorCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', sameRepositoryNameInOwnedCommitAndMajorCommit);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return TestServer.database().selectRows('repositories', {'name': 'WebKit'});
        }).then((result) => {
            assert.strictEqual(result.length, 2);
            let osWebKit = result[0];
            let webkitRepository = result[1];
            assert.notStrictEqual(osWebKit.id, webkitRepository.id);
            assert.strictEqual(osWebKit.name, webkitRepository.name);
            assert.strictEqual(webkitRepository.owner, null);
        });
    });

    const systemVersionCommitWithOwnedCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    },
                    "JavaScriptCore": {
                        "revision": "141978",
                        "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                        "message": "JavaScriptCore commit",
                    }
                }
            }
        ]
    };

    it("should accept inserting one commit with some owned commits", () => {
        const db = TestServer.database();
        return addWorkerForReport(systemVersionCommitWithOwnedCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommitWithOwnedCommits);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return Promise.all([db.selectRows('commits', {'revision': 'Sierra16D32'}),
                db.selectRows('commits', {'message': 'WebKit Commit'}),
                db.selectRows('commits', {'message': 'JavaScriptCore commit'}),
                db.selectRows('repositories', {'name': 'OSX'}),
                db.selectRows('repositories', {'name': "WebKit"}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'})])
        }).then((result) => {
            assert.strictEqual(result.length, 6);

            assert.strictEqual(result[0].length, 1);
            const osxCommit = result[0][0];
            assert.notStrictEqual(osxCommit, null);

            assert.strictEqual(result[1].length, 1);
            const webkitCommit = result[1][0];
            assert.notStrictEqual(webkitCommit, null);

            assert.strictEqual(result[2].length, 1);
            const jscCommit = result[2][0];
            assert.notStrictEqual(jscCommit, null);

            assert.strictEqual(result[3].length, 1);
            const osxRepository = result[3][0];
            assert.notStrictEqual(osxRepository, null);

            assert.strictEqual(result[4].length, 1);
            const webkitRepository = result[4][0];
            assert.notStrictEqual(webkitRepository, null);

            assert.strictEqual(result[5].length, 1);
            const jscRepository = result[5][0];
            assert.notStrictEqual(jscRepository, null);

            assert.strictEqual(osxCommit.repository, osxRepository.id);
            assert.strictEqual(webkitCommit.repository, webkitRepository.id);
            assert.strictEqual(jscCommit.repository, jscRepository.id);
            assert.strictEqual(osxRepository.owner, null);
            assert.strictEqual(webkitRepository.owner, osxRepository.id);
            assert.strictEqual(jscRepository.owner, osxRepository.id);

            return Promise.all([db.selectRows('commit_ownerships', {'owner': osxCommit.id, 'owned': webkitCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit.id, 'owned': jscCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commits', {'repository': webkitRepository.id})]);
        }).then((result) => {
            assert.strictEqual(result.length, 3);

            assert.strictEqual(result[0].length, 1);
            const ownerCommitForWebKitCommit = result[0][0];
            assert.notStrictEqual(ownerCommitForWebKitCommit, null);

            assert.strictEqual(result[1].length, 1);
            const ownerCommitForJSCCommit =  result[1][0];
            assert.notStrictEqual(ownerCommitForJSCCommit, null);

            assert.strictEqual(result[2].length, 1);
        });
    })

    const multipleSystemVersionCommitsWithOwnedCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 2,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    },
                    "JavaScriptCore": {
                        "revision": "141978",
                        "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                        "message": "JavaScriptCore commit",
                    }
                }
            },
            {
                "repository": "OSX",
                "revision": "Sierra16C67",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    },
                    "JavaScriptCore": {
                        "revision": "141999",
                        "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                        "message": "new JavaScriptCore commit",
                    }
                }
            }
        ]
    };

    it("should accept inserting multiple commits with multiple owned-commits", () => {
        const db = TestServer.database();
        return addWorkerForReport(multipleSystemVersionCommitsWithOwnedCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', multipleSystemVersionCommitsWithOwnedCommits);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return Promise.all([db.selectRows('commits', {'revision': 'Sierra16D32'}),
                db.selectRows('commits', {'revision': 'Sierra16C67'}),
                db.selectRows('commits', {'message': 'WebKit Commit'}),
                db.selectRows('commits', {'message': 'JavaScriptCore commit'}),
                db.selectRows('commits', {'message': 'new JavaScriptCore commit'}),
                db.selectRows('repositories', {'name': 'OSX'}),
                db.selectRows('repositories', {'name': "WebKit"}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'})])
        }).then((result) => {
            assert.strictEqual(result.length, 8);

            assert.strictEqual(result[0].length, 1);
            const osxCommit0 = result[0][0];
            assert.notStrictEqual(osxCommit0, null);

            assert.strictEqual(result[1].length, 1);
            const osxCommit1 = result[1][0];
            assert.notStrictEqual(osxCommit1, null);

            assert.strictEqual(result[2].length, 1);
            const webkitCommit = result[2][0];
            assert.notStrictEqual(webkitCommit, null);

            assert.strictEqual(result[3].length, 1);
            const jscCommit0 = result[3][0];
            assert.notStrictEqual(jscCommit0, null);

            assert.strictEqual(result[4].length, 1);
            const jscCommit1 = result[4][0];
            assert.notStrictEqual(jscCommit1, null);

            assert.strictEqual(result[5].length, 1)
            const osxRepository = result[5][0];
            assert.notStrictEqual(osxRepository, null);
            assert.strictEqual(osxRepository.owner, null);

            assert.strictEqual(result[6].length, 1)
            const webkitRepository = result[6][0];
            assert.strictEqual(webkitRepository.owner, osxRepository.id);

            assert.strictEqual(result[7].length, 1);
            const jscRepository = result[7][0];
            assert.strictEqual(jscRepository.owner, osxRepository.id);

            assert.strictEqual(osxCommit0.repository, osxRepository.id);
            assert.strictEqual(osxCommit1.repository, osxRepository.id);
            assert.strictEqual(webkitCommit.repository, webkitRepository.id);
            assert.strictEqual(jscCommit0.repository, jscRepository.id);
            assert.strictEqual(jscCommit1.repository, jscRepository.id);
            assert.strictEqual(osxRepository.owner, null);
            assert.strictEqual(webkitRepository.owner, osxRepository.id);
            assert.strictEqual(jscRepository.owner, osxRepository.id);

            return Promise.all([db.selectRows('commit_ownerships', {'owner': osxCommit0.id, 'owned': webkitCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit1.id, 'owned': webkitCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit0.id, 'owned': jscCommit0.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit1.id, 'owned': jscCommit1.id}, {'sortBy': 'owner'}),
                db.selectRows('commits', {'repository': webkitRepository.id})]);
        }).then((result) => {
            assert.strictEqual(result.length, 5);

            assert.strictEqual(result[0].length, 1);
            const ownerCommitForWebKitCommit0 = result[0][0];
            assert.notStrictEqual(ownerCommitForWebKitCommit0, null);

            assert.strictEqual(result[1].length, 1);
            const ownerCommitForWebKitCommit1 = result[1][0];
            assert.notStrictEqual(ownerCommitForWebKitCommit1, null);

            assert.strictEqual(result[2].length, 1);
            const ownerCommitForJSCCommit0 = result[2][0];
            assert.notStrictEqual(ownerCommitForJSCCommit0, null);

            assert.strictEqual(result[3].length, 1);
            const ownerCommitForJSCCommit1 = result[3][0];
            assert.notStrictEqual(ownerCommitForJSCCommit1, null);

            assert.strictEqual(result[4].length, 1);
        });
    });

    const systemVersionCommitWithEmptyOwnedCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                }
            }
        ]
    }

    it("should accept inserting one commit with no owned commits", () => {
        return addWorkerForReport(systemVersionCommitWithEmptyOwnedCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommitWithEmptyOwnedCommits);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const db = TestServer.database();
            return Promise.all([db.selectAll('commits'), db.selectAll('repositories'), db.selectAll('commit_ownerships', 'owner')]);
        }).then((result) => {
            let commits = result[0];
            let repositories = result[1];
            let commit_ownerships = result[2];
            assert.strictEqual(commits.length, 1);
            assert.strictEqual(repositories.length, 1);
            assert.strictEqual(commits[0].repository, repositories[0].id);
            assert.strictEqual(repositories[0].name, 'OSX');
            assert.strictEqual(commit_ownerships.length, 0);
        });
    });

    const commitWithHugeOrder = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 2147483648
            }
        ]
    }

    it("should accept interting one commit with a huge commit order", async () => {
        await addWorkerForReport(commitWithHugeOrder);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', commitWithHugeOrder);
        assert.strictEqual(response['status'], 'OK');
        const commits = await TestServer.database().selectAll('commits');
        assert.strictEqual(commits.length, 1);
        assert.strictEqual(parseInt(commits[0].order), 2147483648);
    });

    const systemVersionCommitAndOwnedCommitWithTimestamp = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "time": "2013-02-06T08:55:20.9Z",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    }
                }
            }
        ]
    }

    it("should reject inserting one commit with owned commits that contains timestamp", () => {
        return addWorkerForReport(systemVersionCommitAndOwnedCommitWithTimestamp).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommitAndOwnedCommitWithTimestamp);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OwnedCommitShouldNotContainTimestamp');
        });
    });

    const invalidOwnedCommitUseRevisionIdentifierAsRevison = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "127232@main",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    }
                }
            }
        ]
    };

    it("should reject inserting one commit with owned commits that use revision label as revision", async () => {
        await addWorkerForReport(invalidOwnedCommitUseRevisionIdentifierAsRevison);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', invalidOwnedCommitUseRevisionIdentifierAsRevison);
        assert.strictEqual(response['status'], 'InvalidRevision');
    });

    const invalidRevisionIdentifierOwnedCommit = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "revisionIdentifier": "127232",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    }
                }
            }
        ]
    };

    it("should reject inserting one commit with owned commits that have invalid revision label", async () => {
        await addWorkerForReport(invalidRevisionIdentifierOwnedCommit);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', invalidRevisionIdentifierOwnedCommit);
        assert.strictEqual(response['status'], 'InvalidRevisionIdentifier');
    });

    const invalidAuthorOwnedCommit = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "revisionIdentifier": "127232@main",
                        "author": null,
                        "message": "WebKit Commit",
                    }
                }
            }
        ]
    };
    it("should reject inserting one commit with owned commits that have invalid author", async () => {
        await addWorkerForReport(invalidAuthorOwnedCommit);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', invalidAuthorOwnedCommit);
        assert.strictEqual(response['status'], 'InvalidAuthorFormat');
    });

    const ownedCommitWithRevisionIdentifier = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "OSX",
                "revision": "Sierra16D32",
                "order": 1,
                "ownedCommits": {
                    "WebKit": {
                        "revision": "141978",
                        "revisionIdentifier": "127232@main",
                        "author": {"name": "Commit Queue", "account": "commit-queue@webkit.org"},
                        "message": "WebKit Commit",
                    }
                }
            }
        ]
    };

    it("should insert one commit with commit revision label", async () => {
        await addWorkerForReport(ownedCommitWithRevisionIdentifier);
        const response = await TestServer.remoteAPI().postJSON('/api/report-commits/', ownedCommitWithRevisionIdentifier);
        assert.strictEqual(response['status'], 'OK');
        const db = TestServer.database();
        const commit = await db.selectRows('commits', {'revision': '141978'});
        assert.strictEqual(commit[0].revision_identifier, ownedCommitWithRevisionIdentifier.commits[0].ownedCommits['WebKit'].revisionIdentifier);
    });
});

describe("/api/report-commits/ with insert=false", function () {
    prepareServerTest(this);

    const subversionCommits = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
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
            }
        ],
        "insert": true,
    };

    const commitsUpdate = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "testability": "Breaks builds",
                "message": "another message",
                "order": 210948,
                "time": "2017-01-20T03:52:34.577Z",
                "author": {"name": "Chris Dumez", "account": "cdumez@apple.com"},
            },
            {
                "repository": "WebKit",
                "revision": "210949",
                "previousCommit": "210948",
                "testability": "Crashes WebKit",
                "message": "another message",
                "order": 210949,
                "time": "2017-01-20T04:23:50.645Z",
                "author": {"name": "Zalan Bujtas", "account": "zalan@apple.com"},
            }
        ],
        "insert": false,
    };


    const commitsUpdateWithMissingRevision = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "testability": "Breaks builds"
            }
        ],
        "insert": false,
    };

    const commitsUpdateWithMissingRepository = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "revision": "210948",
                "testability": "Breaks builds"
            }
        ],
        "insert": false,
    };

    const commitsUpdateWithoutAnyUpdate = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948"
            }
        ],
        "insert": false,
    };

    const commitsUpdateWithOwnedCommitsUpdate = {
        "workerName": "someWorker",
        "workerPassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948",
                "ownedCommits": [],
            }
        ],
        "insert": false,
    };

    async function initialReportCommits()
    {
        await addWorkerForReport(subversionCommits);
        await TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommits);
        const result = await TestServer.remoteAPI().getJSON('/api/commits/WebKit/');
        assert.strictEqual(result['status'], 'OK');
        const commits = result['commits'];
        assert.strictEqual(commits.length, 2);
        assert.strictEqual(commits[0].testability, null);
        assert.strictEqual(commits[1].testability, null);

        return commits;
    }

    async function setUpTestsWithExpectedStatus(commitsUpdate, expectedStatus)
    {
        await initialReportCommits();
        let result = await TestServer.remoteAPI().postJSON('/api/report-commits/', commitsUpdate);
        assert.strictEqual(result['status'], expectedStatus);
        result = await TestServer.remoteAPI().getJSON('/api/commits/WebKit/');
        return result['commits'];
    }

    async function testWithExpectedFailure(commitsUpdate, expectedFailureName)
    {
        const commits = await setUpTestsWithExpectedStatus(commitsUpdate, expectedFailureName);
        assert.strictEqual(commits.length, 2);
        assert.strictEqual(commits[0].testability, null);
        assert.strictEqual(commits[1].testability, null);
    }

    it('should fail with "MissingRevision" if commit update does not have commit revision specified', async () => {
        await testWithExpectedFailure(commitsUpdateWithMissingRevision, 'MissingRevision');
    });

    it('should fail with "MissingRepositoryName" if commit update does not have commit revision specified', async () => {
        await testWithExpectedFailure(commitsUpdateWithMissingRepository, 'MissingRepositoryName');
    });

    it('should fail with "NothingToUpdate" if commit update does not have commit revision specified', async () => {
        await testWithExpectedFailure(commitsUpdateWithoutAnyUpdate, 'NothingToUpdate');
    });

    it('should fail with "AttemptToUpdateOwnedCommits" when trying to update owned commits info', async () => {
        await testWithExpectedFailure(commitsUpdateWithOwnedCommitsUpdate, 'AttemptToUpdateOwnedCommits');
    });

    it('should not set reported to true if it was not set to true before', async () => {
        const database = TestServer.database();
        await initialReportCommits();

        let commits = await database.selectAll('commits');
        assert.strictEqual(commits[0].reported, true);
        assert.strictEqual(commits[1].reported, true);

        await database.query('UPDATE commits SET commit_reported=false');
        let result = await TestServer.remoteAPI().postJSON('/api/report-commits/', commitsUpdate);
        assert.strictEqual(result['status'], 'OK');

        commits = await database.selectAll('commits');
        assert.strictEqual(commits[0].reported, false);
        assert.strictEqual(commits[1].reported, false);
    });

    it("should be able to update commits message, time, order, author and previous commit", async () => {
        const commits = await setUpTestsWithExpectedStatus(commitsUpdate, 'OK');

        assert.strictEqual(commits.length, 2);
        assert.strictEqual(commits[0].testability, 'Breaks builds');
        assert.strictEqual(commits[1].testability, 'Crashes WebKit');
        assert.strictEqual(commits[0].message, 'another message');
        assert.strictEqual(commits[1].message, 'another message');
        assert.strictEqual(commits[0].authorName, 'Chris Dumez');
        assert.strictEqual(commits[1].authorName, 'Zalan Bujtas');
        assert.strictEqual(parseInt(commits[0].order), 210948);
        assert.strictEqual(parseInt(commits[1].order), 210949);
        assert.strictEqual(commits[0].time, 1484884354577);
        assert.strictEqual(commits[1].time, 1484886230645);
        assert.strictEqual(commits[1].previousCommit, commits[0].id);
    });
});