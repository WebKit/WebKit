require('../../tools/js/v3-models.js');

var crypto = require('crypto');

MockData = {
    resetV3Models: function ()
    {
        Manifest.reset();
    },
    emptyTriggeragbleId() { return 1001; },
    someTestId() { return 200; },
    somePlatformId() { return 65; },
    otherPlatformId() { return 101; },
    macosRepositoryId() { return 9; },
    webkitRepositoryId() { return 11; },
    ownedJSCRepositoryId() { return 213; },
    jscRepositoryId() { return 222; },
    gitWebkitRepositoryId() { return 111; },
    sharedRepositoryId() { return 14; },
    buildbotBuildersURL() {return '/api/v2/builders'},
    pendingBuildsUrl: function (builderName) {
        const builderId = this.builderIDForName(builderName);
        return `/api/v2/builders/${builderId}/buildrequests?complete=false&claimed=false&property=*`;
    },
    recentBuildsUrl: function (builderName, count) {
        const builderId = this.builderIDForName(builderName);
        return `/api/v2/builders/${builderId}/builds?limit=${count}&order=-number&property=*`; 
    },
    statusUrl: function (builderName, buildId) {
        const builderId = this.builderIDForName(builderName);
        return `http://build.webkit.org/#/builders/${builderId}/builds/${buildId}`; 
    },
    addMockConfiguration: function (db)
    {
        return Promise.all([
            db.insert('build_triggerables', {id: 1000, name: 'build-webkit'}),
            db.insert('build_slaves', {id: 20, name: 'sync-slave', password_hash: crypto.createHash('sha256').update('password').digest('hex')}),
            db.insert('repositories', {id: this.macosRepositoryId(), name: 'macOS'}),
            db.insert('repositories', {id: this.webkitRepositoryId(), name: 'WebKit'}),
            db.insert('repositories', {id: this.sharedRepositoryId(), name: 'Shared'}),
            db.insert('repositories', {id: this.ownedJSCRepositoryId(), owner: this.webkitRepositoryId(), name: 'JavaScriptCore'}),
            db.insert('repositories', {id: this.jscRepositoryId(), name: 'JavaScriptCore'}),
            db.insert('triggerable_repository_groups', {id: 2001, name: 'webkit-svn', triggerable: 1000}),
            db.insert('triggerable_repositories', {repository: this.macosRepositoryId(), group: 2001}),
            db.insert('triggerable_repositories', {repository: this.webkitRepositoryId(), group: 2001}),
            db.insert('commits', {id: 87832, repository: this.macosRepositoryId(), revision: '10.11 15A284'}),
            db.insert('commits', {id: 93116, repository: this.webkitRepositoryId(), revision: '191622', time: (new Date(1445945816878)).toISOString()}),
            db.insert('commits', {id: 96336, repository: this.webkitRepositoryId(), revision: '192736', time: (new Date(1448225325650)).toISOString()}),
            db.insert('commits', {id: 111168, repository: this.sharedRepositoryId(), revision: '80229', time: '2016-03-02T23:17:54.3Z'}),
            db.insert('commits', {id: 111169, repository: this.sharedRepositoryId(), revision: '80230', time: '2016-03-02T23:37:18.0Z'}),
            db.insert('commits', {id: 11797, repository: this.jscRepositoryId(), revision: 'jsc-6161', time: '2016-03-02T23:19:55.3Z'}),
            db.insert('commits', {id: 12017, repository: this.jscRepositoryId(), revision: 'jsc-9191', time: '2016-05-02T23:13:57.1Z'}),
            db.insert('commits', {id: 1797, repository: this.ownedJSCRepositoryId(), revision: 'owned-jsc-6161', time: '2016-03-02T23:19:55.3Z'}),
            db.insert('commits', {id: 2017, repository: this.ownedJSCRepositoryId(), revision: 'owned-jsc-9191', time: '2016-05-02T23:13:57.1Z'}),
            db.insert('commit_ownerships', {owner: 93116, owned: 1797}),
            db.insert('commit_ownerships', {owner: 96336, owned: 2017}),
            db.insert('builds', {id: 901, number: '901', time: '2015-10-27T12:05:27.1Z'}),
            db.insert('platforms', {id: MockData.somePlatformId(), name: 'some platform'}),
            db.insert('platforms', {id: MockData.otherPlatformId(), name: 'other platform'}),
            db.insert('tests', {id: MockData.someTestId(), name: 'some test'}),
            db.insert('test_metrics', {id: 300, test: 200, name: 'some metric'}),
            db.insert('test_configurations', {id: 301, metric: 300, platform: MockData.somePlatformId(), type: 'current'}),
            db.insert('test_configurations', {id: 302, metric: 300, platform: MockData.otherPlatformId(), type: 'current'}),
            db.insert('test_runs', {id: 801, config: 301, build: 901, mean_cache: 100}),
        ]);
    },
    addMockData: function (db, statusList, needsNotification=true)
    {
        if (!statusList)
            statusList = ['pending', 'pending', 'pending', 'pending'];
        return Promise.all([
            this.addMockConfiguration(db),
            db.insert('commit_sets', {id: 401}),
            db.insert('commit_set_items', {set: 401, commit: 87832}),
            db.insert('commit_set_items', {set: 401, commit: 93116}),
            db.insert('commit_sets', {id: 402}),
            db.insert('commit_set_items', {set: 402, commit: 87832}),
            db.insert('commit_set_items', {set: 402, commit: 96336}),
            db.insert('analysis_tasks', {id: 500, platform: 65, metric: 300, name: 'some task',
                start_run: 801, start_run_time: '2015-10-27T12:05:27.1Z',
                end_run: 801, end_run_time: '2015-10-27T12:05:27.1Z'}),
            db.insert('analysis_test_groups', {id: 600, task: 500, name: 'some test group', initial_repetition_count: 4, needs_notification: needsNotification}),
            db.insert('build_requests', {id: 700, status: statusList[0], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 0, commit_set: 401}),
            db.insert('build_requests', {id: 701, status: statusList[1], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 1, commit_set: 402}),
            db.insert('build_requests', {id: 702, status: statusList[2], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 2, commit_set: 401}),
            db.insert('build_requests', {id: 703, status: statusList[3], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 3, commit_set: 402}),
        ]);
    },
    addAnotherTriggerable(db) {
        return Promise.all([
            db.insert('build_triggerables', {id: 2345, name: 'build-webkit-jsc'}),
            db.insert('triggerable_repository_groups', {id: 4002, name: 'mac-svnwebkit-jsc', triggerable: 2345}),
            db.insert('triggerable_repositories', {repository: this.macosRepositoryId(), group: 4002}),
            db.insert('triggerable_repositories', {repository: this.webkitRepositoryId(), group: 4002}),
            db.insert('triggerable_repositories', {repository: this.jscRepositoryId(), group: 4002}),
        ]);
    },
    addEmptyTriggerable(db)
    {
        return Promise.all([
            db.insert('build_triggerables', {id: this.emptyTriggeragbleId(), name: 'empty-triggerable'}),
            db.insert('repositories', {id: this.macosRepositoryId(), name: 'macOS'}),
            db.insert('repositories', {id: this.webkitRepositoryId(), name: 'WebKit'}),
            db.insert('repositories', {id: this.gitWebkitRepositoryId(), name: 'Git-WebKit'}),
            db.insert('platforms', {id: MockData.somePlatformId(), name: 'some platform'}),
            db.insert('tests', {id: MockData.someTestId(), name: 'some test'}),
            db.insert('test_metrics', {id: 5300, test: MockData.someTestId(), name: 'some metric'}),
            db.insert('test_configurations', {id: 5400, metric: 5300, platform: MockData.somePlatformId(), type: 'current'}),
        ]);
    },
    addMockTestGroupWithGitWebKit(db)
    {
        return Promise.all([
            db.insert('repositories', {id: this.gitWebkitRepositoryId(), name: 'Git-WebKit'}),
            db.insert('triggerable_repository_groups', {id: 2002, name: 'webkit-git', triggerable: 1000}),
            db.insert('triggerable_repositories', {repository: this.macosRepositoryId(), group: 2002}),
            db.insert('triggerable_repositories', {repository: this.gitWebkitRepositoryId(), group: 2002}),
            db.insert('commits', {id: 193116, repository: this.gitWebkitRepositoryId(), revision: '2ceda45d3cd63cde58d0dbf5767714e03d902e43', time: (new Date(1445945816878)).toISOString()}),
            db.insert('commits', {id: 196336, repository: this.gitWebkitRepositoryId(), revision: '8e294365a452a89785d6536ca7f0fc8a95fa152d', time: (new Date(1448225325650)).toISOString()}),
            db.insert('commit_sets', {id: 1401}),
            db.insert('commit_set_items', {set: 1401, commit: 87832}),
            db.insert('commit_set_items', {set: 1401, commit: 193116}),
            db.insert('commit_sets', {id: 1402}),
            db.insert('commit_set_items', {set: 1402, commit: 87832}),
            db.insert('commit_set_items', {set: 1402, commit: 196336}),
            db.insert('analysis_test_groups', {id: 1600, task: 500, name: 'test group with git', initial_repetition_count: 4}),
            db.insert('build_requests', {id: 1700, status: 'pending', triggerable: 1000, repository_group: 2002, platform: 65, test: 200, group: 1600, order: 0, commit_set: 1401}),
            db.insert('build_requests', {id: 1701, status: 'pending', triggerable: 1000, repository_group: 2002, platform: 65, test: 200, group: 1600, order: 1, commit_set: 1402}),
            db.insert('build_requests', {id: 1702, status: 'pending', triggerable: 1000, repository_group: 2002, platform: 65, test: 200, group: 1600, order: 2, commit_set: 1401}),
            db.insert('build_requests', {id: 1703, status: 'pending', triggerable: 1000, repository_group: 2002, platform: 65, test: 200, group: 1600, order: 3, commit_set: 1402}),
        ]);
    },
    addAnotherMockTestGroup: function (db, statusList, author)
    {
        if (!statusList)
            statusList = ['pending', 'pending', 'pending', 'pending'];
        const test = MockData.someTestId();
        const triggerable = 1000;
        const platform = 65;
        const repository_group = 2001;
        return Promise.all([
            db.insert('analysis_test_groups', {id: 601, task: 500, name: 'another test group', author, initial_repetition_count: 4}),
            db.insert('build_requests', {id: 713, status: statusList[3], triggerable, repository_group, platform, test, group: 601, order: 3, commit_set: 402}),
            db.insert('build_requests', {id: 710, status: statusList[0], triggerable, repository_group, platform, test, group: 601, order: 0, commit_set: 401}),
            db.insert('build_requests', {id: 712, status: statusList[2], triggerable, repository_group, platform, test, group: 601, order: 2, commit_set: 401}),
            db.insert('build_requests', {id: 711, status: statusList[1], triggerable, repository_group, platform, test, group: 601, order: 1, commit_set: 402}),
        ]);
    },
    addTestGroupWithOwnedCommits(db, statusList)
    {

        if (!statusList)
            statusList = ['pending', 'pending', 'pending', 'pending'];
        return Promise.all([
            this.addMockConfiguration(db),
            this.addAnotherTriggerable(db),
            db.insert('analysis_tasks', {id: 1080, platform: 65, metric: 300, name: 'some task with component test',
                start_run: 801, start_run_time: '2015-10-27T12:05:27.1Z',
                end_run: 801, end_run_time: '2015-10-27T12:05:27.1Z'}),
            db.insert('analysis_test_groups', {id: 900, task: 1080, name: 'some test group with component test', initial_repetition_count: 4}),
            db.insert('commit_sets', {id: 403}),
            db.insert('commit_set_items', {set: 403, commit: 87832}),
            db.insert('commit_set_items', {set: 403, commit: 93116}),
            db.insert('commit_set_items', {set: 403, commit: 1797, commit_owner: 93116, requires_build: true}),
            db.insert('commit_sets', {id: 404}),
            db.insert('commit_set_items', {set: 404, commit: 87832}),
            db.insert('commit_set_items', {set: 404, commit: 96336}),
            db.insert('commit_set_items', {set: 404, commit: 2017, commit_owner: 96336, requires_build: true}),
            db.insert('build_requests', {id: 704, status: statusList[0], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 900, order: 0, commit_set: 403}),
            db.insert('build_requests', {id: 705, status: statusList[1], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 900, order: 1, commit_set: 404}),
            db.insert('build_requests', {id: 706, status: statusList[2], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 900, order: 2, commit_set: 403}),
            db.insert('build_requests', {id: 707, status: statusList[3], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 900, order: 3, commit_set: 404}),
        ]);
    },
    addTestGroupWithOwnerCommitNotInCommitSet(db)
    {
        return Promise.all([
            this.addMockConfiguration(db),
            this.addAnotherTriggerable(db),
            db.insert('analysis_tasks', {id: 1080, platform: 65, metric: 300, name: 'some task with component test',
                start_run: 801, start_run_time: '2015-10-27T12:05:27.1Z',
                end_run: 801, end_run_time: '2015-10-27T12:05:27.1Z'}),
            db.insert('analysis_test_groups', {id: 900, task: 1080, name: 'some test group with component test', initial_repetition_count: 4}),
            db.insert('commit_sets', {id: 404}),
            db.insert('commit_set_items', {set: 404, commit: 87832}),
            db.insert('commit_set_items', {set: 404, commit: 96336}),
            db.insert('commit_set_items', {set: 404, commit: 2017, commit_owner: 93116, requires_build: true}),
            db.insert('build_requests', {id: 704, status: 'pending', triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 900, order: 0, commit_set: 404}),
        ]);
    },
    mockTestSyncConfigWithSingleBuilder: function ()
    {
        return {
            'triggerableName': 'build-webkit',
            'lookbackCount': 2,
            'buildRequestArgument': 'build-request-id',
            'repositoryGroups': {
                'webkit-svn': {
                    'repositories': {'WebKit': {}, 'macOS': {}},
                    'testProperties': {
                        'os': {'revision': 'macOS'},
                        'wk': {'revision': 'WebKit'},
                    }
                }
            },
            'types': {
                'some-test': {'test': ['some test']}
            },
            'builders': {
                'builder-1': {'builder': 'some-builder-1',
                     properties: {forcescheduler: 'force-some-builder-1'}}
            },
            'testConfigurations': [
                {
                    'platforms': ['some platform'],
                    'types': ['some-test'],
                    'builders': ['builder-1'],
                }
            ]
        }
    },
    mockTestSyncConfigWithTwoBuilders: function ()
    {
        return {
            'triggerableName': 'build-webkit',
            'lookbackCount': 2,
            'buildRequestArgument': 'build-request-id',
            'repositoryGroups': {
                'webkit-svn': {
                    'repositories': {'WebKit': {}, 'macOS': {}},
                    'testProperties': {
                        'os': {'revision': 'macOS'},
                        'wk': {'revision': 'WebKit'},
                    }
                }
            },
            'types': {
                'some-test': {'test': ['some test']},
            },
            'builders': {
                'builder-1': {'builder': 'some-builder-1', properties: {forcescheduler: 'force-some-builder-1'}},
                'builder-2': {'builder': 'some builder 2', properties: {forcescheduler: 'force-some-builder-2'}},
            },
            'testConfigurations': [
                {
                    'platforms': ['some platform'],
                    'types': ['some-test'],
                    'builders': ['builder-1', 'builder-2'],
                }
            ]
        }
    },
    mockBuildbotBuilders: function ()
    {
        return {
            "builders": [
                {
                    "builderid": 1,
                    "name": "some builder",
                },
                {
                    "builderid": 2,
                    "name": "some-builder-1",
                },
                {
                    "builderid": 3,
                    "name": "some builder 2",
                },
                {
                    "builderid": 4,
                    "name": "other builder",
                },
                {
                    "builderid": 5,
                    "name": "some tester",
                },
                {
                    "builderid": 6,
                    "name": "another tester",
                }
            ]
        }
    },
    builderIDForName: function(builderName)
    {
        for (let builder of this.mockBuildbotBuilders().builders) {
            if (builder.name == builderName)
                return builder.builderid;
        }
        return -1;
    },
    pendingBuildData(options)
    {
        return {
            "builderid": options.builderId || 2,
            "buildrequestid": options.buildbotBuildRequestId || 18,
            "buildsetid": 894720,
            "claimed": false,
            "claimed_at": null,
            "claimed_by_masterid": null,
            "complete": false,
            "complete_at": null,
            "priority": 0,
            "results": -1,
            "submitted_at": options.buildTime || (new Date('2016-03-23T03:49:43Z') / 1000),
            "waited_for": false,
            "properties": {
                "build-request-id": [(options.buildRequestId || 702).toString(), "Force Build Form"],
                "scheduler": ["ABTest-iPad-RunBenchmark-Tests-ForceScheduler", "Scheduler"],
                "wk": [options.webkitRevision || '191622', "Unknown"],
                "os": [options.osxRevision || '10.11 15A284', "Unknown"],
                "slavename": [options.workerName || "bot202", "Worker (deprecated)"],
                "workername": [options.workerName || "bot202", "Worker"]
            }
        };
    },
    pendingBuild(options)
    {
        options = options || {};
        return {
            "buildrequests": [this.pendingBuildData(options)]
        };
    },
    sampleBuildData(options, overrides)
    {
        options = options || {};
        overrides = overrides || {};
        return {
            "builderid": options.builderId || 2,
            "number":  options.buildNumber || 124, 
            "buildrequestid": options.buildbotBuildRequestId || 19,
            "complete": 'isComplete' in overrides ? overrides.isComplete : (options.isComplete || false),
            "complete_at": null,
            "buildid": options.buildid || 418744,
            "masterid": 1,
            "results": null,
            "started_at": new Date('2017-12-19T23:11:49Z') / 1000,
            "state_string": "building",
            "workerid": 41,
            "properties": {
                "build-request-id": [(options.buildRequestId || 701).toString(), "Force Build Form"],
                "os": [options.osxRevision || '10.11 15A284', "Unknown"],
                "wk": [options.webkitRevision || '192736', "Unknown"],
                "project": ['', "Unknown"],
                "repository": ['', "Unknown"],
                "revision": ['', "Unknown"],
                "slavename": [options.workerName || "bot202", "Worker (deprecated)"],
                "workername": [options.workerName || "bot202", "Worker"]
            }
        };   
    },
    runningBuildData(options)
    {
        return this.sampleBuildData(options);
    },
    runningBuild(options)
    {
        return {
            "builds": [this.runningBuildData(options)]
        };
    },
    finishedBuildData(options)
    {
        options = options || {};
        if (!options.buildRequestId)
            options.buildRequestId = 700;
        if (!options.buildNumber)
            options.buildNumber = 123;
        if (!options.webkitRevision)
            options.webkitRevision = '191622';
        return this.sampleBuildData(options, {isComplete: true});
    },
    finishedBuild(options)
    {
        return {
            "builds": [this.finishedBuildData(options)]
        };
    }
}

if (typeof module != 'undefined')
    module.exports = MockData;
