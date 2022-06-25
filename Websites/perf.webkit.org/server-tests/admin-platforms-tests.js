'use strict';

const assert = require('assert');

const TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const submitReport = require('./resources/common-operations.js').submitReport;

describe("/admin/platforms", function () {
    prepareServerTest(this);

    function reportsForDifferentPlatforms()
    {
        return [
            {
                "buildTag": "3001",
                "buildTime": "2013-02-28T09:01:47",
                "builderName": "someBuilder",
                "builderPassword": "somePassword",
                "platform": "Mavericks",
                "tests": {"test": { "metrics": {"FrameRate": { "current": [[1, 1, 1], [1, 1, 1]] } } } },
            },
            {
                "buildTag": "3001",
                "buildTime": "2013-02-28T10:12:03",
                "builderName": "someBuilder",
                "builderPassword": "somePassword",
                "platform": "Mountain Lion",
                "tests": {"test": { "metrics": {"FrameRate": { "current": [[2, 2, 2], [2, 2, 2]] }, "Combined": { "current": [[3, 3, 3], [3, 3, 3]] }} } },
            },
            {
                "buildTag": "3003",
                "buildTime": "2013-02-28T12:56:26",
                "builderName": "someBuilder",
                "builderPassword": "somePassword",
                "platform": "Trunk Mountain Lion",
                "tests": {"test": { "metrics": {"FrameRate": { "current": [[4, 4, 4], [4, 4, 4]] } } } }
            }];
    } 

    it("should delete the platform that got merged into another one", () => {
        const db = TestServer.database();
        let oldPlatforms;
        return submitReport(reportsForDifferentPlatforms()).then(() => {
            return db.selectAll('platforms', 'name');
        }).then((platforms) => {
            oldPlatforms = platforms;
            assert.strictEqual(oldPlatforms.length, 3);
            assert.strictEqual(oldPlatforms[0]['name'], 'Mavericks');
            assert.strictEqual(oldPlatforms[1]['name'], 'Mountain Lion');
            assert.strictEqual(oldPlatforms[2]['name'], 'Trunk Mountain Lion');
        }).then(() => {
            return TestServer.remoteAPI().postFormUrlencodedData('/admin/platforms.php',
                {'action': 'merge', 'id': oldPlatforms[1]['id'], 'destination': oldPlatforms[2]['id']});
        }).then(() => {
            return db.selectAll('platforms');
        }).then((newPlatforms) => {
            assert.strictEqual(newPlatforms.length, 2);
            assert.deepStrictEqual(newPlatforms[0], oldPlatforms[0]);
            assert.deepStrictEqual(newPlatforms[1], oldPlatforms[2]);
        });
    });

    it("should move test runs from the merged platform to the destination platform", () => {
        let oldTestRuns;
        const queryForRuns = 'SELECT * FROM test_runs, test_configurations, platforms WHERE run_config = config_id AND config_platform = platform_id ORDER by run_mean_cache';
        const db = TestServer.database();
        return submitReport(reportsForDifferentPlatforms()).then(() => {
            return db.query(queryForRuns);
        }).then((result) => {
            oldTestRuns = result.rows;
            assert.strictEqual(oldTestRuns.length, 4);
            assert.strictEqual(oldTestRuns[0]['platform_name'], 'Mavericks');
            assert.strictEqual(oldTestRuns[0]['run_sum_cache'], 6);
            assert.strictEqual(oldTestRuns[1]['platform_name'], 'Mountain Lion');
            assert.strictEqual(oldTestRuns[1]['run_sum_cache'], 12);
            assert.strictEqual(oldTestRuns[2]['platform_name'], 'Mountain Lion');
            assert.strictEqual(oldTestRuns[2]['run_sum_cache'], 18);
            assert.strictEqual(oldTestRuns[3]['platform_name'], 'Trunk Mountain Lion');
            assert.strictEqual(oldTestRuns[3]['run_sum_cache'], 24);
        }).then(() => {
            return TestServer.remoteAPI().postFormUrlencodedData('/admin/platforms.php',
                {'action': 'merge', 'id': oldTestRuns[1]['platform_id'], 'destination': oldTestRuns[3]['platform_id']});
        }).then(() => {
            return db.query(queryForRuns);
        }).then((result) => {
            const newTestRuns = result.rows;
            assert.strictEqual(newTestRuns.length, 4);
            assert.strictEqual(newTestRuns[0]['run_id'], oldTestRuns[0]['run_id']);
            assert.strictEqual(newTestRuns[0]['platform_name'], 'Mavericks');
            assert.strictEqual(newTestRuns[0]['run_sum_cache'], 6);
            assert.strictEqual(newTestRuns[1]['run_id'], oldTestRuns[1]['run_id']);
            assert.strictEqual(newTestRuns[1]['platform_name'], 'Trunk Mountain Lion');
            assert.strictEqual(newTestRuns[1]['run_sum_cache'], 12);
            assert.strictEqual(newTestRuns[2]['run_id'], oldTestRuns[2]['run_id']);
            assert.strictEqual(newTestRuns[2]['platform_name'], 'Trunk Mountain Lion');
            assert.strictEqual(newTestRuns[2]['run_sum_cache'], 18);
            assert.strictEqual(newTestRuns[3]['run_id'], oldTestRuns[3]['run_id']);
            assert.strictEqual(newTestRuns[3]['platform_name'], 'Trunk Mountain Lion');
            assert.strictEqual(newTestRuns[3]['run_sum_cache'], 24);
            assert.strictEqual(newTestRuns[1]['run_config'], newTestRuns[3]['run_config']);
        });
    });

    it("should move test configurations from the merged platform to the destination platform", () => {
        let oldConfigs;
        const reports = reportsForDifferentPlatforms();
        reports[0]['tests'] = {"test": { "metrics": {"FrameRate": { "baseline": [[1, 1, 1], [1, 1, 1]] } } } };
        const queryForConfig = 'SELECT * from test_configurations, platforms, test_metrics'
            + ' where config_platform = platform_id and config_metric = metric_id and platform_name in ($1, $2) order by config_id';
        const db = TestServer.database();

        return submitReport(reports).then(() => {
            return db.query(queryForConfig, [reports[0]['platform'], reports[2]['platform']]);
        }).then((result) => {
            oldConfigs = result.rows;
            assert.strictEqual(oldConfigs.length, 2);
            assert.strictEqual(oldConfigs[0]['platform_name'], reports[0]['platform']);
            assert.strictEqual(oldConfigs[0]['metric_name'], 'FrameRate');
            assert.strictEqual(oldConfigs[0]['config_type'], 'baseline');
            assert.strictEqual(oldConfigs[1]['platform_name'], reports[2]['platform']);
            assert.strictEqual(oldConfigs[1]['metric_name'], 'FrameRate');
            assert.strictEqual(oldConfigs[1]['config_type'], 'current');
        }).then(() => {
            return TestServer.remoteAPI().postFormUrlencodedData('/admin/platforms.php',
                {'action': 'merge', 'id': oldConfigs[0]['platform_id'], 'destination': oldConfigs[1]['platform_id']});
        }).then(() => {
            return db.query(queryForConfig, [reports[0]['platform'], reports[2]['platform']]);
        }).then((result) => {
            const newConfigs = result.rows;
            assert.strictEqual(newConfigs.length, 2);
            assert.strictEqual(newConfigs[0]['platform_name'], reports[2]['platform']);
            assert.strictEqual(newConfigs[0]['metric_name'], 'FrameRate');
            assert.strictEqual(newConfigs[0]['config_type'], 'baseline');
            assert.strictEqual(newConfigs[1]['platform_name'], reports[2]['platform']);
            assert.strictEqual(newConfigs[1]['metric_name'], 'FrameRate');
            assert.strictEqual(newConfigs[1]['config_type'], 'current');
        });
    });

});
