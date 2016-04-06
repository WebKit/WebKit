require('../../tools/js/v3-models.js');

var crypto = require('crypto');

MockData = {
    resetV3Models: function ()
    {
        AnalysisTask._fetchAllPromise = null;
        AnalysisTask.clearStaticMap();
        BuildRequest.clearStaticMap();
        CommitLog.clearStaticMap();
        Metric.clearStaticMap();
        Platform.clearStaticMap();
        Repository.clearStaticMap();
        RootSet.clearStaticMap();
        Test.clearStaticMap();
        TestGroup.clearStaticMap();
    },
    addMockData: function (db, statusList)
    {
        if (!statusList)
            statusList = ['pending', 'pending', 'pending', 'pending'];
        return Promise.all([
            db.insert('build_triggerables', {id: 1, name: 'build-webkit'}),
            db.insert('build_slaves', {id: 2, name: 'sync-slave', password_hash: crypto.createHash('sha256').update('password').digest('hex')}),
            db.insert('repositories', {id: 9, name: 'OS X'}),
            db.insert('repositories', {id: 11, name: 'WebKit'}),
            db.insert('commits', {id: 87832, repository: 9, revision: '10.11 15A284'}),
            db.insert('commits', {id: 93116, repository: 11, revision: '191622', time: (new Date(1445945816878)).toISOString()}),
            db.insert('commits', {id: 96336, repository: 11, revision: '192736', time: (new Date(1448225325650)).toISOString()}),
            db.insert('platforms', {id: 65, name: 'some platform'}),
            db.insert('tests', {id: 200, name: 'some test'}),
            db.insert('test_metrics', {id: 300, test: 200, name: 'some metric'}),
            db.insert('test_configurations', {id: 301, metric: 300, platform: 65, type: 'current'}),
            db.insert('root_sets', {id: 401}),
            db.insert('roots', {set: 401, commit: 87832}),
            db.insert('roots', {set: 401, commit: 93116}),
            db.insert('root_sets', {id: 402}),
            db.insert('roots', {set: 402, commit: 87832}),
            db.insert('roots', {set: 402, commit: 96336}),
            db.insert('analysis_tasks', {id: 500, platform: 65, metric: 300, name: 'some task'}),
            db.insert('analysis_test_groups', {id: 600, task: 500, name: 'some test group'}),
            db.insert('build_requests', {id: 700, status: statusList[0], triggerable: 1, platform: 65, test: 200, group: 600, order: 0, root_set: 401}),
            db.insert('build_requests', {id: 701, status: statusList[1], triggerable: 1, platform: 65, test: 200, group: 600, order: 1, root_set: 402}),
            db.insert('build_requests', {id: 702, status: statusList[2], triggerable: 1, platform: 65, test: 200, group: 600, order: 2, root_set: 401}),
            db.insert('build_requests', {id: 703, status: statusList[3], triggerable: 1, platform: 65, test: 200, group: 600, order: 3, root_set: 402}),
        ]);
    },
    addAnotherMockTestGroup: function (db, statusList)
    {
        if (!statusList)
            statusList = ['pending', 'pending', 'pending', 'pending'];
        return Promise.all([
            db.insert('analysis_test_groups', {id: 599, task: 500, name: 'another test group'}),
            db.insert('build_requests', {id: 713, status: statusList[3], triggerable: 1, platform: 65, test: 200, group: 599, order: 3, root_set: 402}),
            db.insert('build_requests', {id: 710, status: statusList[0], triggerable: 1, platform: 65, test: 200, group: 599, order: 0, root_set: 401}),
            db.insert('build_requests', {id: 712, status: statusList[2], triggerable: 1, platform: 65, test: 200, group: 599, order: 2, root_set: 401}),
            db.insert('build_requests', {id: 711, status: statusList[1], triggerable: 1, platform: 65, test: 200, group: 599, order: 1, root_set: 402}),
        ]);
    },
    mockTestSyncConfigWithSingleBuilder: function ()
    {
        return {
            'triggerableName': 'build-webkit',
            'lookbackCount': 2,
            'configurations': [
                {
                    'platform': 'some platform',
                    'test': ['some test'],
                    'builder': 'some-builder-1',
                    'arguments': {
                        'wk': {'root': 'WebKit'},
                        'os': {'root': 'OS X'},
                    },
                    'buildRequestArgument': 'build-request-id',
                }
            ]
        }
    },
    mockTestSyncConfigWithTwoBuilders: function ()
    {
        return {
            'triggerableName': 'build-webkit',
            'lookbackCount': 2,
            'configurations': [
                {
                    'platform': 'some platform',
                    'test': ['some test'],
                    'builder': 'some-builder-1',
                    'arguments': {
                        'wk': {'root': 'WebKit'},
                        'os': {'root': 'OS X'},
                    },
                    'buildRequestArgument': 'build-request-id',
                },
                {
                    'platform': 'some platform',
                    'test': ['some test'],
                    'builder': 'some-builder-2',
                    'arguments': {
                        'wk': {'root': 'WebKit'},
                        'os': {'root': 'OS X'},
                    },
                    'buildRequestArgument': 'build-request-id',
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
            'number': 124,
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
            'number': 123,
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
