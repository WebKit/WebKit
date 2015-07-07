describe("/admin/regenerate-manifest", function () {

    it("should generate an empty manifest when database is empty", function () {
        httpGet('/admin/regenerate-manifest', function (response) {
            assert.equal(response.statusCode, 200);
            httpGet('/data/manifest', function (response) {
                assert.equal(response.statusCode, 200);
                var manifest = JSON.parse(response.responseText);
                assert.deepEqual(manifest, {
                    all: [],
                    bugTrackers: [],
                    builders: [],
                    dashboard: [],
                    metrics: [],
                    repositories: [],
                    tests: []});
                notifyDone();
            });
        });
    });

    it("should generate manifest with bug trackers without repositories", function () {
        queryAndFetchAll("INSERT INTO bug_trackers (tracker_id, tracker_name, tracker_new_bug_url) VALUES"
            + " (1, 'Bugzilla', 'bugs.webkit.org')", [], function () {
            httpGet('/admin/regenerate-manifest', function (response) {
                assert.equal(response.statusCode, 200);
                httpGet('/data/manifest', function (response) {
                    assert.equal(response.statusCode, 200);
                    var manifest = JSON.parse(response.responseText);
                    assert.deepEqual(manifest['bugTrackers'], { 'Bugzilla': { newBugUrl: 'bugs.webkit.org', repositories: null } });
                    notifyDone();
                });
            });
        });
    });

    it("should generate manifest with bug trackers and repositories", function () {
        queryAndFetchAll("INSERT INTO repositories (repository_id, repository_name, repository_url, repository_blame_url) VALUES"
            + " (1, 'WebKit', 'trac.webkit.org', null), (2, 'Chromium', null, 'SomeBlameURL')", [], function () {
            queryAndFetchAll("INSERT INTO bug_trackers (tracker_id, tracker_name) VALUES (3, 'Bugzilla'), (4, 'Issue Tracker')", [], function () {
                queryAndFetchAll("INSERT INTO tracker_repositories (tracrepo_tracker, tracrepo_repository) VALUES (3, 1), (4, 1), (4, 2)", [], function () {
                    httpGet('/admin/regenerate-manifest', function (response) {
                        assert.equal(response.statusCode, 200);
                        httpGet('/data/manifest', function (response) {
                            assert.equal(response.statusCode, 200);
                            var manifest = JSON.parse(response.responseText);
                            assert.deepEqual(manifest['repositories'], {
                                'WebKit': { url: 'trac.webkit.org', blameUrl: null },
                                'Chromium': { url: null, blameUrl: 'SomeBlameURL' }
                            });
                            assert.deepEqual(manifest['bugTrackers']['Bugzilla'], { newBugUrl: null, repositories: ['WebKit'] });
                            assert.deepEqual(manifest['bugTrackers']['Issue Tracker'], { newBugUrl: null, repositories: ['WebKit', 'Chromium'] });
                            notifyDone();
                        });
                    });
                });
            });
        });
    });

    it("should generate manifest with builders", function () {
        queryAndFetchAll("INSERT INTO builders (builder_id, builder_name, builder_password_hash, builder_build_url) VALUES"
            + " (1, 'SomeBuilder', 'a', null), (2, 'SomeOtherBuilder', 'b', 'SomeURL')", [], function () {
            httpGet('/admin/regenerate-manifest', function (response) {
                assert.equal(response.statusCode, 200);
                httpGet('/data/manifest', function (response) {
                    assert.equal(response.statusCode, 200);
                    var manifest = JSON.parse(response.responseText);
                    assert.deepEqual(manifest['builders'], {
                        '1': { name: 'SomeBuilder', buildUrl: null },
                        '2': { name: 'SomeOtherBuilder', buildUrl: 'SomeURL' }
                    });
                    notifyDone();
                });
            });
        });
    });

    it("should generate manifest with tests", function () {
        queryAndFetchAll("INSERT INTO tests (test_id, test_name, test_parent) VALUES"
            + " (1, 'SomeTest', null), (2, 'SomeOtherTest', null),  (3, 'ChildTest', 1)", [], function () {
            httpGet('/admin/regenerate-manifest', function (response) {
                assert.equal(response.statusCode, 200);
                httpGet('/data/manifest', function (response) {
                    assert.equal(response.statusCode, 200);
                    var manifest = JSON.parse(response.responseText);
                    assert.deepEqual(manifest['tests'], {
                        '1': { name: 'SomeTest', url: null, parentId: null },
                        '2': { name: 'SomeOtherTest', url: null, parentId: null },
                        '3': { name: 'ChildTest', url: null, parentId: '1' }
                    });
                    notifyDone();
                });
            });
        });
    });

    it("should generate manifest with metrics", function () {
        queryAndFetchAll("INSERT INTO tests (test_id, test_name, test_parent) VALUES"
            + " (1, 'SomeTest', null), (2, 'SomeOtherTest', null),  (3, 'ChildTest', 1)", [], function () {
            queryAndFetchAll('INSERT INTO aggregators (aggregator_id, aggregator_name, aggregator_definition) values (4, $1, $2)',
                ['Total', 'values.reduce(function (a, b) { return a + b; })'], function () {
                queryAndFetchAll("INSERT INTO test_metrics (metric_id, metric_test, metric_name, metric_aggregator) VALUES"
                    + " (5, 1, 'Time', null), (6, 2, 'Time', 4), (7, 2, 'Malloc', 4)", [], function () {
                    httpGet('/admin/regenerate-manifest', function (response) {
                        assert.equal(response.statusCode, 200);
                        httpGet('/data/manifest', function (response) {
                            assert.equal(response.statusCode, 200);
                            var manifest = JSON.parse(response.responseText);
                            assert.deepEqual(manifest['metrics'], {
                                '5': { name: 'Time', test: '1', aggregator: null },
                                '6': { name: 'Time', test: '2', aggregator: 'Total' },
                                '7': { name: 'Malloc', test: '2', aggregator: 'Total' }
                            });
                            notifyDone();
                        });
                    });
                });
            });
        });
    });

    // FIXME: Test the generation of all and dashboard.
});
