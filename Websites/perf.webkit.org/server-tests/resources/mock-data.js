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
    gitWebkitRepositoryId() { return 111; },
    sharedRepositoryId() { return 14; },
    addMockConfiguration: function (db)
    {
        return Promise.all([
            db.insert('build_triggerables', {id: 1000, name: 'build-webkit'}),
            db.insert('build_slaves', {id: 20, name: 'sync-slave', password_hash: crypto.createHash('sha256').update('password').digest('hex')}),
            db.insert('repositories', {id: this.macosRepositoryId(), name: 'macOS'}),
            db.insert('repositories', {id: this.webkitRepositoryId(), name: 'WebKit'}),
            db.insert('repositories', {id: this.sharedRepositoryId(), name: 'Shared'}),
            db.insert('triggerable_repository_groups', {id: 2001, name: 'webkit-svn', triggerable: 1000}),
            db.insert('triggerable_repositories', {repository: this.macosRepositoryId(), group: 2001}),
            db.insert('triggerable_repositories', {repository: this.webkitRepositoryId(), group: 2001}),
            db.insert('commits', {id: 87832, repository: this.macosRepositoryId(), revision: '10.11 15A284'}),
            db.insert('commits', {id: 93116, repository: this.webkitRepositoryId(), revision: '191622', time: (new Date(1445945816878)).toISOString()}),
            db.insert('commits', {id: 96336, repository: this.webkitRepositoryId(), revision: '192736', time: (new Date(1448225325650)).toISOString()}),
            db.insert('commits', {id: 111168, repository: this.sharedRepositoryId(), revision: '80229', time: '2016-03-02T23:17:54.3Z'}),
            db.insert('commits', {id: 111169, repository: this.sharedRepositoryId(), revision: '80230', time: '2016-03-02T23:37:18.0Z'}),
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
    addMockData: function (db, statusList)
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
            db.insert('analysis_test_groups', {id: 600, task: 500, name: 'some test group'}),
            db.insert('build_requests', {id: 700, status: statusList[0], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 0, commit_set: 401}),
            db.insert('build_requests', {id: 701, status: statusList[1], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 1, commit_set: 402}),
            db.insert('build_requests', {id: 702, status: statusList[2], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 2, commit_set: 401}),
            db.insert('build_requests', {id: 703, status: statusList[3], triggerable: 1000, repository_group: 2001, platform: 65, test: 200, group: 600, order: 3, commit_set: 402}),
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
            db.insert('analysis_test_groups', {id: 1600, task: 500, name: 'test group with git'}),
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
            db.insert('analysis_test_groups', {id: 601, task: 500, name: 'another test group', author}),
            db.insert('build_requests', {id: 713, status: statusList[3], triggerable, repository_group, platform, test, group: 601, order: 3, commit_set: 402}),
            db.insert('build_requests', {id: 710, status: statusList[0], triggerable, repository_group, platform, test, group: 601, order: 0, commit_set: 401}),
            db.insert('build_requests', {id: 712, status: statusList[2], triggerable, repository_group, platform, test, group: 601, order: 2, commit_set: 401}),
            db.insert('build_requests', {id: 711, status: statusList[1], triggerable, repository_group, platform, test, group: 601, order: 1, commit_set: 402}),
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
                'builder-1': {'builder': 'some-builder-1'},
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
                'builder-1': {'builder': 'some-builder-1'},
                'builder-2': {'builder': 'some builder 2'},
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
    pendingBuild(options)
    {
        options = options || {};
        return {
            'builderName': options.builder || 'some-builder-1',
            'builds': [],
            'properties': [
                ['wk', options.webkitRevision || '191622'],
                ['os', options.osxRevision || '10.11 15A284'],
                ['build-request-id', (options.buildRequestId || 702).toString(), ]
            ],
            'source': {
                'branch': '',
                'changes': [],
                'codebase': 'WebKit',
                'hasPatch': false,
                'project': '',
                'repository': '',
                'revision': ''
            },
        };
    },
    runningBuild(options)
    {
        options = options || {};
        return {
            'builderName': options.builder || 'some-builder-1',
            'builds': [],
            'properties': [
                ['wk', options.webkitRevision || '192736'],
                ['os', options.osxRevision || '10.11 15A284'],
                ['build-request-id', (options.buildRequestId || 701).toString(), ]
            ],
            'currentStep': {},
            'eta': 721,
            'number': options.buildNumber || 124,
            'source': {
                'branch': '',
                'changes': [],
                'codebase': 'WebKit',
                'hasPatch': false,
                'project': '',
                'repository': '',
                'revision': ''
            },
        };
    },
    finishedBuild(options)
    {
        options = options || {};
        return {
            'builderName': options.builder || 'some-builder-1',
            'builds': [],
            'properties': [
                ['wk', options.webkitRevision || '191622'],
                ['os', options.osxRevision || '10.11 15A284'],
                ['build-request-id', (options.buildRequestId || 700).toString(), ]
            ],
            'currentStep': null,
            'eta': null,
            'number': options.buildNumber || 123,
            'source': {
                'branch': '',
                'changes': [],
                'codebase': 'WebKit',
                'hasPatch': false,
                'project': '',
                'repository': '',
                'revision': ''
            },
            'times': [0, 1],
        };
    }
}

if (typeof module != 'undefined')
    module.exports = MockData;
