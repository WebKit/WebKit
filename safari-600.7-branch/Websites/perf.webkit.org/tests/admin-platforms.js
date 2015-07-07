describe("/admin/platforms", function () {
    var reportsForDifferentPlatforms = [
    {
        "buildNumber": "3001",
        "buildTime": "2013-02-28T09:01:47",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mavericks",
        "tests": {"test": { "metrics": {"FrameRate": { "current": [[1, 1, 1], [1, 1, 1]] } } } },
    },
    {
        "buildNumber": "3001",
        "buildTime": "2013-02-28T10:12:03",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Mountain Lion",
        "tests": {"test": { "metrics": {"FrameRate": { "current": [[2, 2, 2], [2, 2, 2]] }, "Combined": { "current": [[3, 3, 3], [3, 3, 3]] }} } },
    },
    {
        "buildNumber": "3003",
        "buildTime": "2013-02-28T12:56:26",
        "builderName": "someBuilder",
        "builderPassword": "somePassword",
        "platform": "Trunk Mountain Lion",
        "tests": {"test": { "metrics": {"FrameRate": { "current": [[4, 4, 4], [4, 4, 4]] } } } }
    }];

    function submitReport(report, callback) {
        queryAndFetchAll('INSERT INTO builders (builder_name, builder_password_hash) values ($1, $2)',
            [report[0].builderName, sha256(report[0].builderPassword)], function () {
                postJSON('/api/report/', reportsForDifferentPlatforms, function (response) {
                    callback();
                });
            });
    }

    it("should delete the platform that got merged into another one", function () {
        submitReport(reportsForDifferentPlatforms, function () {
            queryAndFetchAll('SELECT * FROM platforms ORDER by platform_name', [], function (oldPlatforms) {
                assert.equal(oldPlatforms.length, 3);
                assert.equal(oldPlatforms[0]['platform_name'], 'Mavericks');
                assert.equal(oldPlatforms[1]['platform_name'], 'Mountain Lion');
                assert.equal(oldPlatforms[2]['platform_name'], 'Trunk Mountain Lion');
                httpPost('/admin/platforms.php', {'action': 'merge', 'id': oldPlatforms[1]['platform_id'], 'destination': oldPlatforms[2]['platform_id']}, function (response) {
                    assert.equal(response.statusCode, 200);
                    queryAndFetchAll('SELECT * FROM platforms ORDER by platform_name', [], function (newPlatforms) {
                        assert.deepEqual(newPlatforms, [oldPlatforms[0], oldPlatforms[2]]);
                        notifyDone();
                    });
                });
            });
        });
    });

    it("should move test runs from the merged platform to the destination platform", function () {
        submitReport(reportsForDifferentPlatforms, function () {
            var queryForRuns = 'SELECT * FROM test_runs, test_configurations, platforms WHERE run_config = config_id AND config_platform = platform_id ORDER by run_mean_cache';
            queryAndFetchAll(queryForRuns, [], function (oldTestRuns) {
                assert.equal(oldTestRuns.length, 4);
                assert.equal(oldTestRuns[0]['platform_name'], 'Mavericks');
                assert.equal(oldTestRuns[0]['run_sum_cache'], 6);
                assert.equal(oldTestRuns[1]['platform_name'], 'Mountain Lion');
                assert.equal(oldTestRuns[1]['run_sum_cache'], 12);
                assert.equal(oldTestRuns[2]['platform_name'], 'Mountain Lion');
                assert.equal(oldTestRuns[2]['run_sum_cache'], 18);
                assert.equal(oldTestRuns[3]['platform_name'], 'Trunk Mountain Lion');
                assert.equal(oldTestRuns[3]['run_sum_cache'], 24);
                httpPost('/admin/platforms.php', {'action': 'merge', 'id': oldTestRuns[1]['platform_id'], 'destination': oldTestRuns[3]['platform_id']}, function (response) {
                    assert.equal(response.statusCode, 200);
                    queryAndFetchAll(queryForRuns, [], function (newTestRuns) {
                        assert.equal(newTestRuns.length, 4);
                        assert.equal(newTestRuns[0]['run_id'], oldTestRuns[0]['run_id']);
                        assert.equal(newTestRuns[0]['platform_name'], 'Mavericks');
                        assert.equal(newTestRuns[0]['run_sum_cache'], 6);
                        assert.equal(newTestRuns[1]['run_id'], oldTestRuns[1]['run_id']);
                        assert.equal(newTestRuns[1]['platform_name'], 'Trunk Mountain Lion');
                        assert.equal(newTestRuns[1]['run_sum_cache'], 12);
                        assert.equal(newTestRuns[2]['run_id'], oldTestRuns[2]['run_id']);
                        assert.equal(newTestRuns[2]['platform_name'], 'Trunk Mountain Lion');
                        assert.equal(newTestRuns[2]['run_sum_cache'], 18);
                        assert.equal(newTestRuns[3]['run_id'], oldTestRuns[3]['run_id']);
                        assert.equal(newTestRuns[3]['platform_name'], 'Trunk Mountain Lion');
                        assert.equal(newTestRuns[3]['run_sum_cache'], 24);
                        assert.equal(newTestRuns[1]['run_config'], newTestRuns[3]['run_config']);
                        notifyDone();
                    });
                });
            });
        });
    });

});
