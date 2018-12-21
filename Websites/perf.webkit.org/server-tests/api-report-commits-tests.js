'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe("/api/report-commits/ with insert=true", function () {
    prepareServerTest(this);

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
                "previousCommit": "141977",
                "revision": "141978",
                "time": "2013-02-06T09:54:56.0Z",
                "author": {"name": "Mikhail Pozdnyakov", "account": "mikhail.pozdnyakov@intel.com"},
                "message": "another message",
            }
        ]
    };

    const subversionInvalidPreviousCommit = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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

    it("should reject error when slave name is missing", () => {
        return TestServer.remoteAPI().postJSON('/api/report-commits/', {}).then((response) => {
            assert.equal(response['status'], 'MissingSlaveName');
        });
    });

    it("should reject when there are no slaves", () => {
        return TestServer.remoteAPI().postJSON('/api/report-commits/', emptyReport).then((response) => {
            assert.equal(response['status'], 'SlaveNotFound');
            return TestServer.database().selectAll('commits');
        }).then((rows) => {
            assert.equal(rows.length, 0);
        });
    });

    it("should accept an empty report", () => {
        return addSlaveForReport(emptyReport).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', emptyReport);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
        });
    });

    it("should add a missing repository", () => {
        return addSlaveForReport(subversionCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectAll('repositories');
        }).then((rows) => {
            assert.equal(rows.length, 1);
            assert.equal(rows[0]['name'], subversionCommit.commits[0]['repository']);
        });
    });

    it("should store a commit from a valid slave", () => {
        return addSlaveForReport(subversionCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            const db = TestServer.database();
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
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
        });
    });

    it("should reject an invalid revision number", () => {
        return addSlaveForReport(subversionCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionInvalidCommit);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRevision');
            return TestServer.database().selectAll('commits');
        }).then((rows) => {
            assert.equal(rows.length, 0);
        });
    });

    it("should store two commits from a valid slave", () => {
        return addSlaveForReport(subversionTwoCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionTwoCommits);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            const db = TestServer.database();
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
            const commits = result[0];
            const committers = result[1];
            assert.equal(commits.length, 2);
            assert.equal(committers.length, 2);

            let reportedData = subversionTwoCommits.commits[0];
            assert.equal(commits[0]['revision'], reportedData['revision']);
            assert.equal(commits[0]['time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
            assert.equal(commits[0]['message'], reportedData['message']);
            assert.equal(commits[0]['committer'], committers[0]['id']);
            assert.equal(commits[0]['previous_commit'], null);
            assert.equal(committers[0]['name'], reportedData['author']['name']);
            assert.equal(committers[0]['account'], reportedData['author']['account']);

            reportedData = subversionTwoCommits.commits[1];
            assert.equal(commits[1]['revision'], reportedData['revision']);
            assert.equal(commits[1]['time'].toString(), new Date('2013-02-06 09:54:56.0').toString());
            assert.equal(commits[1]['message'], reportedData['message']);
            assert.equal(commits[1]['committer'], committers[1]['id']);
            assert.equal(commits[1]['previous_commit'], commits[0]['id']);
            assert.equal(committers[1]['name'], reportedData['author']['name']);
            assert.equal(committers[1]['account'], reportedData['author']['account']);
        });
    });

    it("should fail if previous commit is invalid", () => {
        return addSlaveForReport(subversionInvalidPreviousCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionInvalidPreviousCommit);
        }).then((response) => {
            assert.equal(response['status'], 'FailedToFindPreviousCommit');
            return TestServer.database().selectAll('commits');
        }).then((result) => {
            assert.equal(result.length, 0);
        });
    });

    it("should update an existing commit if there is one", () => {
        const db = TestServer.database();
        const reportedData = subversionCommit.commits[0];
        return addSlaveForReport(subversionCommit).then(() => {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'repository': 1, 'revision': reportedData['revision'], 'time': reportedData['time']})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
            const commits = result[0];
            const committers = result[1];

            assert.equal(commits.length, 1);
            assert.equal(committers.length, 1);
            assert.equal(commits[0]['message'], reportedData['message']);
            assert.equal(commits[0]['committer'], committers[0]['id']);
            assert.equal(committers[0]['name'], reportedData['author']['name']);
            assert.equal(committers[0]['account'], reportedData['author']['account']);
        });
    });

    it("should not update an unrelated commit", () => {
        const db = TestServer.database();
        const firstData = subversionTwoCommits.commits[0];
        const secondData = subversionTwoCommits.commits[1];
        return addSlaveForReport(subversionCommit).then(() => {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('commits', {'id': 2, 'repository': 1, 'revision': firstData['revision'], 'time': firstData['time']}),
                db.insert('commits', {'id': 3, 'repository': 1, 'revision': secondData['revision'], 'time': secondData['time']})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('commits'), db.selectAll('committers')]);
        }).then((result) => {
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
        });
    });

    it("should update an existing committer if there is one", () => {
        const db = TestServer.database();
        const author = subversionCommit.commits[0]['author'];
        return addSlaveForReport(subversionCommit).then(() => {
            return Promise.all([
                db.insert('repositories', {'id': 1, 'name': 'WebKit'}),
                db.insert('committers', {'repository': 1, 'account': author['account']}),
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommit);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('committers');
        }).then((committers) => {
            assert.equal(committers.length, 1);
            assert.equal(committers[0]['name'], author['name']);
            assert.equal(committers[0]['account'], author['account']);
        });
    });

    const sameRepositoryNameInOwnedCommitAndMajorCommit = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        return addSlaveForReport(sameRepositoryNameInOwnedCommitAndMajorCommit).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', sameRepositoryNameInOwnedCommitAndMajorCommit);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return TestServer.database().selectRows('repositories', {'name': 'WebKit'});
        }).then((result) => {
            assert.equal(result.length, 2);
            let osWebKit = result[0];
            let webkitRepository = result[1];
            assert.notEqual(osWebKit.id, webkitRepository.id);
            assert.equal(osWebKit.name, webkitRepository.name);
            assert.equal(webkitRepository.owner, null);
        });
    });

    const systemVersionCommitWithOwnedCommits = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        return addSlaveForReport(systemVersionCommitWithOwnedCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommitWithOwnedCommits);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectRows('commits', {'revision': 'Sierra16D32'}),
                db.selectRows('commits', {'message': 'WebKit Commit'}),
                db.selectRows('commits', {'message': 'JavaScriptCore commit'}),
                db.selectRows('repositories', {'name': 'OSX'}),
                db.selectRows('repositories', {'name': "WebKit"}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'})])
        }).then((result) => {
            assert.equal(result.length, 6);

            assert.equal(result[0].length, 1);
            const osxCommit = result[0][0];
            assert.notEqual(osxCommit, null);

            assert.equal(result[1].length, 1);
            const webkitCommit = result[1][0];
            assert.notEqual(webkitCommit, null);

            assert.equal(result[2].length, 1);
            const jscCommit = result[2][0];
            assert.notEqual(jscCommit, null);

            assert.equal(result[3].length, 1);
            const osxRepository = result[3][0];
            assert.notEqual(osxRepository, null);

            assert.equal(result[4].length, 1);
            const webkitRepository = result[4][0];
            assert.notEqual(webkitRepository, null);

            assert.equal(result[5].length, 1);
            const jscRepository = result[5][0];
            assert.notEqual(jscRepository, null);

            assert.equal(osxCommit.repository, osxRepository.id);
            assert.equal(webkitCommit.repository, webkitRepository.id);
            assert.equal(jscCommit.repository, jscRepository.id);
            assert.equal(osxRepository.owner, null);
            assert.equal(webkitRepository.owner, osxRepository.id);
            assert.equal(jscRepository.owner, osxRepository.id);

            return Promise.all([db.selectRows('commit_ownerships', {'owner': osxCommit.id, 'owned': webkitCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit.id, 'owned': jscCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commits', {'repository': webkitRepository.id})]);
        }).then((result) => {
            assert.equal(result.length, 3);

            assert.equal(result[0].length, 1);
            const ownerCommitForWebKitCommit = result[0][0];
            assert.notEqual(ownerCommitForWebKitCommit, null);

            assert.equal(result[1].length, 1);
            const ownerCommitForJSCCommit =  result[1][0];
            assert.notEqual(ownerCommitForJSCCommit, null);

            assert.equal(result[2].length, 1);
        });
    })

    const multipleSystemVersionCommitsWithOwnedCommits = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        return addSlaveForReport(multipleSystemVersionCommitsWithOwnedCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', multipleSystemVersionCommitsWithOwnedCommits);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectRows('commits', {'revision': 'Sierra16D32'}),
                db.selectRows('commits', {'revision': 'Sierra16C67'}),
                db.selectRows('commits', {'message': 'WebKit Commit'}),
                db.selectRows('commits', {'message': 'JavaScriptCore commit'}),
                db.selectRows('commits', {'message': 'new JavaScriptCore commit'}),
                db.selectRows('repositories', {'name': 'OSX'}),
                db.selectRows('repositories', {'name': "WebKit"}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'})])
        }).then((result) => {
            assert.equal(result.length, 8);

            assert.equal(result[0].length, 1);
            const osxCommit0 = result[0][0];
            assert.notEqual(osxCommit0, null);

            assert.equal(result[1].length, 1);
            const osxCommit1 = result[1][0];
            assert.notEqual(osxCommit1, null);

            assert.equal(result[2].length, 1);
            const webkitCommit = result[2][0];
            assert.notEqual(webkitCommit, null);

            assert.equal(result[3].length, 1);
            const jscCommit0 = result[3][0];
            assert.notEqual(jscCommit0, null);

            assert.equal(result[4].length, 1);
            const jscCommit1 = result[4][0];
            assert.notEqual(jscCommit1, null);

            assert.equal(result[5].length, 1)
            const osxRepository = result[5][0];
            assert.notEqual(osxRepository, null);
            assert.equal(osxRepository.owner, null);

            assert.equal(result[6].length, 1)
            const webkitRepository = result[6][0];
            assert.equal(webkitRepository.owner, osxRepository.id);

            assert.equal(result[7].length, 1);
            const jscRepository = result[7][0];
            assert.equal(jscRepository.owner, osxRepository.id);

            assert.equal(osxCommit0.repository, osxRepository.id);
            assert.equal(osxCommit1.repository, osxRepository.id);
            assert.equal(webkitCommit.repository, webkitRepository.id);
            assert.equal(jscCommit0.repository, jscRepository.id);
            assert.equal(jscCommit1.repository, jscRepository.id);
            assert.equal(osxRepository.owner, null);
            assert.equal(webkitRepository.owner, osxRepository.id);
            assert.equal(jscRepository.owner, osxRepository.id);

            return Promise.all([db.selectRows('commit_ownerships', {'owner': osxCommit0.id, 'owned': webkitCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit1.id, 'owned': webkitCommit.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit0.id, 'owned': jscCommit0.id}, {'sortBy': 'owner'}),
                db.selectRows('commit_ownerships', {'owner': osxCommit1.id, 'owned': jscCommit1.id}, {'sortBy': 'owner'}),
                db.selectRows('commits', {'repository': webkitRepository.id})]);
        }).then((result) => {
            assert.equal(result.length, 5);

            assert.equal(result[0].length, 1);
            const ownerCommitForWebKitCommit0 = result[0][0];
            assert.notEqual(ownerCommitForWebKitCommit0, null);

            assert.equal(result[1].length, 1);
            const ownerCommitForWebKitCommit1 = result[1][0];
            assert.notEqual(ownerCommitForWebKitCommit1, null);

            assert.equal(result[2].length, 1);
            const ownerCommitForJSCCommit0 = result[2][0];
            assert.notEqual(ownerCommitForJSCCommit0, null);

            assert.equal(result[3].length, 1);
            const ownerCommitForJSCCommit1 = result[3][0];
            assert.notEqual(ownerCommitForJSCCommit1, null);

            assert.equal(result[4].length, 1);
        });
    });

    const systemVersionCommitWithEmptyOwnedCommits = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        return addSlaveForReport(systemVersionCommitWithEmptyOwnedCommits).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommitWithEmptyOwnedCommits);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            const db = TestServer.database();
            return Promise.all([db.selectAll('commits'), db.selectAll('repositories'), db.selectAll('commit_ownerships', 'owner')]);
        }).then((result) => {
            let commits = result[0];
            let repositories = result[1];
            let commit_ownerships = result[2];
            assert.equal(commits.length, 1);
            assert.equal(repositories.length, 1);
            assert.equal(commits[0].repository, repositories[0].id);
            assert.equal(repositories[0].name, 'OSX');
            assert.equal(commit_ownerships.length, 0);
        });
    });

    const systemVersionCommitAndOwnedCommitWithTimestamp = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        return addSlaveForReport(systemVersionCommitAndOwnedCommitWithTimestamp).then(() => {
            return TestServer.remoteAPI().postJSON('/api/report-commits/', systemVersionCommitAndOwnedCommitWithTimestamp);
        }).then((response) => {
            assert.equal(response['status'], 'OwnedCommitShouldNotContainTimestamp');
        });
    });
});

describe("/api/report-commits/ with insert=false", function () {
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
            }
        ],
        "insert": true,
    };

    const commitsUpdate = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "testability": "Breaks builds"
            }
        ],
        "insert": false,
    };

    const commitsUpdateWithMissingRepository = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "revision": "210948",
                "testability": "Breaks builds"
            }
        ],
        "insert": false,
    };

    const commitsUpdateWithoutAnyUpdate = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
        "commits": [
            {
                "repository": "WebKit",
                "revision": "210948"
            }
        ],
        "insert": false,
    };

    const commitsUpdateWithOwnedCommitsUpdate = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
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
        await addSlaveForReport(subversionCommits);
        await TestServer.remoteAPI().postJSON('/api/report-commits/', subversionCommits);
        const result = await TestServer.remoteAPI().getJSON('/api/commits/WebKit/');
        assert.equal(result['status'], 'OK');
        const commits = result['commits'];
        assert.equal(commits.length, 2);
        assert.equal(commits[0].testability, null);
        assert.equal(commits[1].testability, null);

        return commits;
    }

    async function setUpTestsWithExpectedStatus(commitsUpdate, expectedStatus)
    {
        await initialReportCommits();
        let result = await TestServer.remoteAPI().postJSON('/api/report-commits/', commitsUpdate);
        assert.equal(result['status'], expectedStatus);
        result = await TestServer.remoteAPI().getJSON('/api/commits/WebKit/');
        return result['commits'];
    }

    async function testWithExpectedFailure(commitsUpdate, expectedFailureName)
    {
        const commits = await setUpTestsWithExpectedStatus(commitsUpdate, expectedFailureName);
        assert.equal(commits.length, 2);
        assert.equal(commits[0].testability, null);
        assert.equal(commits[1].testability, null);
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
        assert.equal(commits[0].reported, true);
        assert.equal(commits[1].reported, true);

        await database.query('UPDATE commits SET commit_reported=false');
        let result = await TestServer.remoteAPI().postJSON('/api/report-commits/', commitsUpdate);
        assert.equal(result['status'], 'OK');

        commits = await database.selectAll('commits');
        assert.equal(commits[0].reported, false);
        assert.equal(commits[1].reported, false);
    });

    it("should be able to update commits message, time, order, author and previous commit", async () => {
        const commits = await setUpTestsWithExpectedStatus(commitsUpdate, 'OK');

        assert.equal(commits.length, 2);
        assert.equal(commits[0].testability, 'Breaks builds');
        assert.equal(commits[1].testability, 'Crashes WebKit');
        assert.equal(commits[0].message, 'another message');
        assert.equal(commits[1].message, 'another message');
        assert.equal(commits[0].authorName, 'Chris Dumez');
        assert.equal(commits[1].authorName, 'Zalan Bujtas');
        assert.equal(commits[0].order, 210948);
        assert.equal(commits[1].order, 210949);
        assert.equal(commits[0].time, 1484884354577);
        assert.equal(commits[1].time, 1484886230645);
        assert.equal(commits[1].previousCommit, commits[0].id);
    });
});