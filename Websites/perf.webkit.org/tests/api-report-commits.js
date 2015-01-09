describe("/api/report-commits/", function () {
    var emptyReport = {
        "slaveName": "someSlave",
        "slavePassword": "somePassword",
    };
    var subversionCommit = {
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
    var subversionInvalidCommit = {
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
    var subversionTwoCommits = {
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

    function addSlave(report, callback) {
        queryAndFetchAll('INSERT INTO build_slaves (slave_name, slave_password_hash) values ($1, $2)',
            [report.slaveName, sha256(report.slavePassword)], callback);
    }

    it("should reject error when slave name is missing", function () {
        postJSON('/api/report-commits/', {}, function (response) {
            assert.equal(response.statusCode, 200);
            assert.equal(JSON.parse(response.responseText)['status'], 'MissingSlaveName');
            notifyDone();
        });
    });

    it("should reject when there are no slaves", function () {
        postJSON('/api/report-commits/', emptyReport, function (response) {
            assert.equal(response.statusCode, 200);
            assert.notEqual(JSON.parse(response.responseText)['status'], 'OK');

            queryAndFetchAll('SELECT COUNT(*) from commits', [], function (rows) {
                assert.equal(rows[0].count, 0);
                notifyDone();
            });
        });
    });

    it("should accept an empty report", function () {
        addSlave(emptyReport, function () {
            postJSON('/api/report-commits/', emptyReport, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                notifyDone();
            });
        });
    });

    it("should add a missing repository", function () {
        addSlave(subversionCommit, function () {
            postJSON('/api/report-commits/', subversionCommit, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryAndFetchAll('SELECT * FROM repositories', [], function (rows) {
                    assert.equal(rows.length, 1);
                    assert.equal(rows[0]['repository_name'], subversionCommit.commits[0]['repository']);
                    notifyDone();
                });
            });
        });
    });

    it("should store a commit from a valid slave", function () {
        addSlave(subversionCommit, function () {
            postJSON('/api/report-commits/', subversionCommit, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryAndFetchAll('SELECT * FROM commits JOIN committers ON commit_committer = committer_id', [], function (rows) {
                    assert.equal(rows.length, 1);
                    var reportedData = subversionCommit.commits[0];
                    assert.equal(rows[0]['commit_revision'], reportedData['revision']);
                    assert.equal(rows[0]['commit_time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
                    assert.equal(rows[0]['committer_name'], reportedData['author']['name']);
                    assert.equal(rows[0]['committer_account'], reportedData['author']['account']);
                    assert.equal(rows[0]['commit_message'], reportedData['message']);
                    notifyDone();
                });
            });
        });
    });

    it("should reject an invalid revision number", function () {
        addSlave(subversionCommit, function () {
            subversionCommit
            postJSON('/api/report-commits/', subversionInvalidCommit, function (response) {
                assert.equal(response.statusCode, 200);
                assert.notEqual(JSON.parse(response.responseText)['status'], 'OK');
                queryAndFetchAll('SELECT * FROM commits', [], function (rows) {
                    assert.equal(rows.length, 0);
                    notifyDone();
                });
            });
        });
    });

    it("should store two commits from a valid slave", function () {
        addSlave(subversionTwoCommits, function () {
            postJSON('/api/report-commits/', subversionTwoCommits, function (response) {
                assert.equal(response.statusCode, 200);
                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                queryAndFetchAll('SELECT * FROM commits JOIN committers ON commit_committer = committer_id ORDER BY commit_time', [], function (rows) {
                    assert.equal(rows.length, 2);
                    var reportedData = subversionTwoCommits.commits[0];
                    assert.equal(rows[0]['commit_revision'], reportedData['revision']);
                    assert.equal(rows[0]['commit_time'].toString(), new Date('2013-02-06 08:55:20.9').toString());
                    assert.equal(rows[0]['committer_name'], reportedData['author']['name']);
                    assert.equal(rows[0]['committer_account'], reportedData['author']['account']);
                    assert.equal(rows[0]['commit_message'], reportedData['message']);
                    var reportedData = subversionTwoCommits.commits[1];
                    assert.equal(rows[1]['commit_revision'], reportedData['revision']);
                    assert.equal(rows[1]['commit_time'].toString(), new Date('2013-02-06 09:54:56.0').toString());
                    assert.equal(rows[1]['committer_name'], reportedData['author']['name']);
                    assert.equal(rows[1]['committer_account'], reportedData['author']['account']);
                    assert.equal(rows[1]['commit_message'], reportedData['message']);
                    notifyDone();
                });
            });
        });
    });

    it("should update an existing commit if there is one", function () {
        queryAndFetchAll('INSERT INTO repositories (repository_name) VALUES ($1) RETURNING *', ['WebKit'], function (repositories) {
            var repositoryId = repositories[0]['repository_id'];
            var reportedData = subversionCommit.commits[0];
            queryAndFetchAll('INSERT INTO commits (commit_repository, commit_revision, commit_time) VALUES ($1, $2, $3) RETURNING *',
                [repositoryId, reportedData['revision'], reportedData['time']], function (existingCommits) {
                var commitId = existingCommits[0]['commit_id'];
                assert.equal(existingCommits[0]['commit_message'], null);
                addSlave(subversionCommit, function () {
                    postJSON('/api/report-commits/', subversionCommit, function (response) {
                        assert.equal(response.statusCode, 200);
                        assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                        queryAndFetchAll('SELECT * FROM commits JOIN committers ON commit_committer = committer_id', [], function (rows) {
                            assert.equal(rows.length, 1);
                            var reportedData = subversionCommit.commits[0];
                            assert.equal(rows[0]['committer_name'], reportedData['author']['name']);
                            assert.equal(rows[0]['committer_account'], reportedData['author']['account']);
                            assert.equal(rows[0]['commit_message'], reportedData['message']);
                            notifyDone();
                        });
                    });
                });
            });
        });
    });

    it("should not update an unrelated commit", function () {
        queryAndFetchAll('INSERT INTO repositories (repository_name) VALUES ($1) RETURNING *', ['WebKit'], function (repositories) {
            var repositoryId = repositories[0]['repository_id'];
            var reportedData = subversionTwoCommits.commits[1];
            queryAndFetchAll('INSERT INTO commits (commit_repository, commit_revision, commit_time) VALUES ($1, $2, $3) RETURNING *',
                [repositoryId, reportedData['revision'], reportedData['time']], function (existingCommits) {
                reportedData = subversionTwoCommits.commits[0];
                queryAndFetchAll('INSERT INTO commits (commit_repository, commit_revision, commit_time) VALUES ($1, $2, $3) RETURNING *',
                    [repositoryId, reportedData['revision'], reportedData['time']], function () {
                        addSlave(subversionCommit, function () {
                            postJSON('/api/report-commits/', subversionCommit, function (response) {
                                assert.equal(response.statusCode, 200);
                                assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                                queryAndFetchAll('SELECT * FROM commits LEFT OUTER JOIN committers ON commit_committer = committer_id ORDER BY commit_time', [], function (rows) {
                                    assert.equal(rows.length, 2);
                                    assert.equal(rows[0]['committer_name'], reportedData['author']['name']);
                                    assert.equal(rows[0]['committer_account'], reportedData['author']['account']);
                                    assert.equal(rows[0]['commit_message'], reportedData['message']);
                                    assert.equal(rows[1]['committer_name'], null);
                                    assert.equal(rows[1]['committer_account'], null);
                                    assert.equal(rows[1]['commit_message'], null);
                                    notifyDone();
                                });
                            });
                        });
                });
            });
        });
    });

    it("should update an existing committer if there is one", function () {
        queryAndFetchAll('INSERT INTO repositories (repository_id, repository_name) VALUES (1, \'WebKit\')', [], function () {
            var author = subversionCommit.commits[0]['author'];
            queryAndFetchAll('INSERT INTO committers (committer_repository, committer_account) VALUES (1, $1)', [author['account']], function () {
                addSlave(subversionCommit, function () {
                    postJSON('/api/report-commits/', subversionCommit, function (response) {
                        assert.equal(response.statusCode, 200);
                        assert.equal(JSON.parse(response.responseText)['status'], 'OK');
                        queryAndFetchAll('SELECT * FROM committers', [], function (rows) {
                            assert.equal(rows.length, 1);
                            assert.equal(rows[0]['committer_name'], author['name']);
                            assert.equal(rows[0]['committer_account'], author['account']);
                            notifyDone();
                        });
                    });
                });
            });
        });
    });

});
