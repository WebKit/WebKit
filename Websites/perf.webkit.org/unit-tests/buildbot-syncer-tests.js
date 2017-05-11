'use strict';

let assert = require('assert');

require('../tools/js/v3-models.js');
let MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
let MockModels = require('./resources/mock-v3-models.js').MockModels;

let BuildbotBuildEntry = require('../tools/js/buildbot-syncer.js').BuildbotBuildEntry;
let BuildbotSyncer = require('../tools/js/buildbot-syncer.js').BuildbotSyncer;

function sampleiOSConfig()
{
    return {
        'slaveArgument': 'slavename',
        'buildRequestArgument': 'build_request_id',
        'repositoryGroups': {
            'ios-svn-webkit': {
                'repositories': {'WebKit': {}, 'iOS': {}},
                'properties': {
                    'desired_image': '<iOS>',
                    'opensource': '<WebKit>',
                }
            }
        },
        'types': {
            'speedometer': {
                'test': ['Speedometer'],
                'properties': {'test_name': 'speedometer'}
            },
            'jetstream': {
                'test': ['JetStream'],
                'properties': {'test_name': 'jetstream'}
            },
            'dromaeo-dom': {
                'test': ['Dromaeo', 'DOM Core Tests'],
                'properties': {'tests': 'dromaeo-dom'}
            },
        },
        'builders': {
            'iPhone-bench': {
                'builder': 'ABTest-iPhone-RunBenchmark-Tests',
                'properties': { 'forcescheduler': 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler' },
                'slaveList': ['ABTest-iPhone-0'],
            },
            'iPad-bench': {
                'builder': 'ABTest-iPad-RunBenchmark-Tests',
                'properties': { 'forcescheduler': 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler' },
                'slaveList': ['ABTest-iPad-0', 'ABTest-iPad-1'],
            }
        },
        'configurations': [
            {'type': 'speedometer', 'builder': 'iPhone-bench', 'platform': 'iPhone'},
            {'type': 'jetstream', 'builder': 'iPhone-bench', 'platform': 'iPhone'},
            {'type': 'dromaeo-dom', 'builder': 'iPhone-bench', 'platform': 'iPhone'},

            {'type': 'speedometer', 'builder': 'iPad-bench', 'platform': 'iPad'},
            {'type': 'jetstream', 'builder': 'iPad-bench', 'platform': 'iPad'},
        ]
    };
}

function sampleiOSConfigWithExpansions()
{
    return {
        "triggerableName": "build-webkit-ios",
        "buildRequestArgument": "build-request-id",
        "repositoryGroups": { },
        "types": {
            "iphone-plt": {
                "test": ["PLT-iPhone"],
                "properties": {"test_name": "plt"}
            },
            "ipad-plt": {
                "test": ["PLT-iPad"],
                "properties": {"test_name": "plt"}
            },
            "speedometer": {
                "test": ["Speedometer"],
                "properties": {"tests": "speedometer"}
            },
        },
        "builders": {
            "iphone": {
                "builder": "iPhone AB Tests",
                "properties": {"forcescheduler": "force-iphone-ab-tests"}
            },
            "ipad": {
                "builder": "iPad AB Tests",
                "properties": {"forcescheduler": "force-ipad-ab-tests"}
            },
        },
        "configurations": [
            {
                "builder": "iphone",
                "platforms": ["iPhone", "iOS 10 iPhone"],
                "types": ["iphone-plt", "speedometer"],
            },
            {
                "builder": "ipad",
                "platforms": ["iPad"],
                "types": ["ipad-plt", "speedometer"],
            },
        ]
    }
}

function smallConfiguration()
{
    return {
        'buildRequestArgument': 'id',
        'repositoryGroups': {
            'ios-svn-webkit': {
                'repositories': {'iOS': {}, 'WebKit': {}},
                'properties': {
                    'os': '<iOS>',
                    'wk': '<WebKit>'
                }
            }
        },
        'configurations': [{
            'builder': 'some builder',
            'platform': 'Some platform',
            'test': ['Some test']
        }]
    };
}

function smallPendingBuild()
{
    return {
        'builderName': 'some builder',
        'builds': [],
        'properties': [],
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
}

function smallInProgressBuild()
{
    return {
        'builderName': 'some builder',
        'builds': [],
        'properties': [],
        'currentStep': { },
        'eta': 123,
        'number': 456,
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
}

function smallFinishedBuild()
{
    return {
        'builderName': 'some builder',
        'builds': [],
        'properties': [],
        'currentStep': null,
        'eta': null,
        'number': 789,
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

function createSampleBuildRequest(platform, test)
{
    assert(platform instanceof Platform);
    assert(test instanceof Test);

    const webkit197463 = CommitLog.ensureSingleton('111127', {'id': '111127', 'time': 1456955807334, 'repository': MockModels.webkit, 'revision': '197463'});
    const shared111237 = CommitLog.ensureSingleton('111237', {'id': '111237', 'time': 1456931874000, 'repository': MockModels.sharedRepository, 'revision': '80229'});
    const ios13A452 = CommitLog.ensureSingleton('88930', {'id': '88930', 'time': 0, 'repository': MockModels.ios, 'revision': '13A452'});

    const commitSet = CommitSet.ensureSingleton('4197', {customRoots: [], revisionItems: [{commit: webkit197463}, {commit: shared111237}, {commit: ios13A452}]});

    return BuildRequest.ensureSingleton('16733-' + platform.id(), {'triggerable': MockModels.triggerable,
        repositoryGroup: MockModels.svnRepositoryGroup,
        'commitSet': commitSet, 'status': 'pending', 'platform': platform, 'test': test});
}

function samplePendingBuild(buildRequestId, buildTime, slaveName)
{
    return {
        'builderName': 'ABTest-iPad-RunBenchmark-Tests',
        'builds': [],
        'properties': [
            ['build_request_id', buildRequestId || '16733', 'Force Build Form'],
            ['desired_image', '13A452', 'Force Build Form'],
            ['owner', '<unknown>', 'Force Build Form'],
            ['test_name', 'speedometer', 'Force Build Form'],
            ['reason', 'force build','Force Build Form'],
            ['slavename', slaveName, ''],
            ['scheduler', 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler', 'Scheduler']
        ],
        'source': {
            'branch': '',
            'changes': [],
            'codebase': 'compiler-rt',
            'hasPatch': false,
            'project': '',
            'repository': '',
            'revision': ''
        },
        'submittedAt': buildTime || 1458704983
    };
}

function sampleInProgressBuild(slaveName)
{
    return {
        'blame': [],
        'builderName': 'ABTest-iPad-RunBenchmark-Tests',
        'currentStep': {
            'eta': 0.26548067698460565,
            'expectations': [['output', 845, 1315.0]],
            'hidden': false,
            'isFinished': false,
            'isStarted': true,
            'logs': [['stdio', 'https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614/steps/Some%20step/logs/stdio']],
            'name': 'Some step',
            'results': [null,[]],
            'statistics': {},
            'step_number': 1,
            'text': [''],
            'times': [1458718657.581628, null],
            'urls': {}
        },
        'eta': 6497.991612434387,
        'logs': [['stdio','https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614/steps/shell/logs/stdio']],
        'number': 614,
        'properties': [
            ['build_request_id', '16733', 'Force Build Form'],
            ['buildername', 'ABTest-iPad-RunBenchmark-Tests', 'Builder'],
            ['buildnumber', 614, 'Build'],
            ['desired_image', '13A452', 'Force Build Form'],
            ['owner', '<unknown>', 'Force Build Form'],
            ['reason', 'force build', 'Force Build Form'],
            ['scheduler', 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler', 'Scheduler'],
            ['slavename', slaveName || 'ABTest-iPad-0', 'BuildSlave'],
        ],
        'reason': 'A build was forced by \'<unknown>\': force build',
        'results': null,
        'slave': 'ABTest-iPad-0',
        'sourceStamps': [{'branch': '', 'changes': [], 'codebase': 'compiler-rt', 'hasPatch': false, 'project': '', 'repository': '', 'revision': ''}],
        'steps': [
            {
                'eta': null,
                'expectations': [['output',2309,2309.0]],
                'hidden': false,
                'isFinished': true,
                'isStarted': true,
                'logs': [['stdio', 'https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614/steps/shell/logs/stdio']],
                'name': 'Finished step',
                'results': [0, []],
                'statistics': {},
                'step_number': 0,
                'text': [''],
                'times': [1458718655.419865, 1458718655.453633],
                'urls': {}
            },
            {
                'eta': 0.26548067698460565,
                'expectations': [['output', 845, 1315.0]],
                'hidden': false,
                'isFinished': false,
                'isStarted': true,
                'logs': [['stdio', 'https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614/steps/Some%20step/logs/stdio']],
                'name': 'Some step',
                'results': [null,[]],
                'statistics': {},
                'step_number': 1,
                'text': [''],
                'times': [1458718657.581628, null],
                'urls': {}
            },
            {
                'eta': null,
                'expectations': [['output', null, null]],
                'hidden': false,
                'isFinished': false,
                'isStarted': false,
                'logs': [],
                'name': 'Some other step',
                'results': [null, []],
                'statistics': {},
                'step_number': 2,
                'text': [],
                'times': [null, null],
                'urls': {}
            },
        ],
        'text': [],
        'times': [1458718655.415821, null]
    };
}

function sampleFinishedBuild(buildRequestId, slaveName)
{
    return {
        'blame': [],
        'builderName': 'ABTest-iPad-RunBenchmark-Tests',
        'currentStep': null,
        'eta': null,
        'logs': [['stdio','https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755/steps/shell/logs/stdio']],
        'number': 1755,
        'properties': [
            ['build_request_id', buildRequestId || '18935', 'Force Build Form'],
            ['buildername', 'ABTest-iPad-RunBenchmark-Tests', 'Builder'],
            ['buildnumber', 1755, 'Build'],
            ['desired_image', '13A452', 'Force Build Form'],
            ['owner', '<unknown>', 'Force Build Form'],
            ['reason', 'force build', 'Force Build Form'],
            ['scheduler', 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler', 'Scheduler'],
            ['slavename', slaveName || 'ABTest-iPad-0', 'BuildSlave'],
        ],
        'reason': 'A build was forced by \'<unknown>\': force build',
        'results': 2,
        'slave': 'ABTest-iPad-0',
        'sourceStamps': [{'branch': '', 'changes': [], 'codebase': 'compiler-rt', 'hasPatch': false, 'project': '', 'repository': '', 'revision': ''}],
        'steps': [
            {
                'eta': null,
                'expectations': [['output',2309,2309.0]],
                'hidden': false,
                'isFinished': true,
                'isStarted': true,
                'logs': [['stdio', 'https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614/steps/shell/logs/stdio']],
                'name': 'Finished step',
                'results': [0, []],
                'statistics': {},
                'step_number': 0,
                'text': [''],
                'times': [1458718655.419865, 1458718655.453633],
                'urls': {}
            },
            {
                'eta': null,
                'expectations': [['output', 845, 1315.0]],
                'hidden': false,
                'isFinished': true,
                'isStarted': true,
                'logs': [['stdio', 'https://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614/steps/Some%20step/logs/stdio']],
                'name': 'Some step',
                'results': [null,[]],
                'statistics': {},
                'step_number': 1,
                'text': [''],
                'times': [1458718657.581628, null],
                'urls': {}
            },
            {
                'eta': null,
                'expectations': [['output', null, null]],
                'hidden': false,
                'isFinished': true,
                'isStarted': true,
                'logs': [],
                'name': 'Some other step',
                'results': [null, []],
                'statistics': {},
                'step_number': 2,
                'text': [],
                'times': [null, null],
                'urls': {}
            },
        ],
        'text': [],
        'times': [1458937478.25837, 1458946147.173785]
    };
}

describe('BuildbotSyncer', () => {
    MockModels.inject();
    let requests = MockRemoteAPI.inject('http://build.webkit.org');

    describe('_loadConfig', () => {

        it('should create BuildbotSyncer objects for a configuration that specify all required options', () => {
            assert.equal(BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration()).length, 1);
        });

        it('should throw when some required options are missing', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.configurations[0].builder;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            }, 'builder should be a required option');
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.configurations[0].platform;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            }, 'platform should be a required option');
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.configurations[0].test;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            }, 'test should be a required option');
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.buildRequestArgument;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            }, 'buildRequestArgument should be required');
        });

        it('should throw when a test name is not an array of strings', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.configurations[0].test = 'some test';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.configurations[0].test = [1];
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when properties is not an object', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.configurations[0].properties = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when propertie\'s values are malformed', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.configurations[0].properties = {'some': {'otherKey': 'some root'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.configurations[0].properties = {'some': {'root': ['a', 'b']}};
                BuildbotSyncer._loadConfig(RemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.configurations[0].properties = {'some': {'root': 1}};
                BuildbotSyncer._loadConfig(RemoteAPI, config);
            });
        });

        it('should create BuildbotSyncer objects for valid configurations', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());
            assert.equal(syncers.length, 2);
            assert.ok(syncers[0] instanceof BuildbotSyncer);
            assert.ok(syncers[1] instanceof BuildbotSyncer);
        });

        it('should parse builder names correctly', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());
            assert.equal(syncers[0].builderName(), 'ABTest-iPhone-RunBenchmark-Tests');
            assert.equal(syncers[1].builderName(), 'ABTest-iPad-RunBenchmark-Tests');
        });

        it('should parse test configurations correctly', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());

            let configurations = syncers[0].testConfigurations();
            assert.equal(configurations.length, 3);
            assert.equal(configurations[0].platform, MockModels.iphone);
            assert.equal(configurations[0].test, MockModels.speedometer);
            assert.equal(configurations[1].platform, MockModels.iphone);
            assert.equal(configurations[1].test, MockModels.jetstream);
            assert.equal(configurations[2].platform, MockModels.iphone);
            assert.equal(configurations[2].test, MockModels.domcore);

            configurations = syncers[1].testConfigurations();
            assert.equal(configurations.length, 2);
            assert.equal(configurations[0].platform, MockModels.ipad);
            assert.equal(configurations[0].test, MockModels.speedometer);
            assert.equal(configurations[1].platform, MockModels.ipad);
            assert.equal(configurations[1].test, MockModels.jetstream);
        });

        it('should parse test configurations with types and platforms expansions correctly', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfigWithExpansions());

            assert.equal(syncers.length, 2);

            let configurations = syncers[0].testConfigurations();
            assert.equal(configurations.length, 4);
            assert.equal(configurations[0].platform, MockModels.iphone);
            assert.equal(configurations[0].test, MockModels.iPhonePLT);
            assert.equal(configurations[1].platform, MockModels.iOS10iPhone);
            assert.equal(configurations[1].test, MockModels.iPhonePLT);
            assert.equal(configurations[2].platform, MockModels.iphone);
            assert.equal(configurations[2].test, MockModels.speedometer);
            assert.equal(configurations[3].platform, MockModels.iOS10iPhone);
            assert.equal(configurations[3].test, MockModels.speedometer);

            configurations = syncers[1].testConfigurations();
            assert.equal(configurations.length, 2);
            assert.equal(configurations[0].platform, MockModels.ipad);
            assert.equal(configurations[0].test, MockModels.iPadPLT);
            assert.equal(configurations[1].platform, MockModels.ipad);
            assert.equal(configurations[1].test, MockModels.speedometer);
        });

        it('should throw when repositoryGroups is not an object', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = 1;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group does not specify a dictionary of repositories', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'properties': {}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': 1}, 'properties': {}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group specifies an empty dictionary', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {}, 'properties': {}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group specifies an invalid repository name', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'InvalidRepositoryName': {}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group specifies a repository with a non-dictionary value', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': 1}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when the description of a repository group is not a string', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': [{'WebKit': {}}], 'description': 1}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': [{'WebKit': {}}], 'description': [1, 2]}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group does not specify a dictionary of properties', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, properties: 1}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, properties: 'hello'}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group refers to a non-existent repository in the properties dictionary', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, properties: {'wk': '<InvalidRepository>'}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group refers to a repository in the properties dictionary which is not listed in the list of repositories', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, properties: {'os': '<iOS>'}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group does not use a listed repository', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, properties: {}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });

        it('should throw when a repository group specifies non-boolean value to acceptsRoots', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, 'properties': {'webkit': '<WebKit>'}, acceptsRoots: 1}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, 'properties': {'webkit': '<WebKit>'}, acceptsRoots: []}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config);
            });
        });
    });

    describe('_propertiesForBuildRequest', () => {
        it('should include all properties specified in a given configuration', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.deepEqual(Object.keys(properties).sort(), ['build_request_id', 'desired_image', 'forcescheduler', 'opensource', 'test_name']);
        });

        it('should preserve non-parametric property values', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.equal(properties['test_name'], 'speedometer');
            assert.equal(properties['forcescheduler'], 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');

            properties = syncers[1]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.ipad, MockModels.jetstream));
            assert.equal(properties['test_name'], 'jetstream');
            assert.equal(properties['forcescheduler'], 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler');
        });

        it('should resolve "root"', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.equal(properties['desired_image'], '13A452');
        });

        it('should set the property for the build request id', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig());
            let request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            let properties = syncers[0]._propertiesForBuildRequest(request);
            assert.equal(properties['build_request_id'], request.id());
        });
    });

    describe('pullBuildbot', () => {
        it('should fetch pending builds from the right URL', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];
            assert.equal(syncer.builderName(), 'ABTest-iPad-RunBenchmark-Tests');
            let expectedURL = '/json/builders/ABTest-iPad-RunBenchmark-Tests/pendingBuilds';
            assert.equal(syncer.pathForPendingBuildsJSON(), expectedURL);
            syncer.pullBuildbot();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, expectedURL);
        });

        it('should fetch recent builds once pending builds have been fetched', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];
            assert.equal(syncer.builderName(), 'ABTest-iPad-RunBenchmark-Tests');

            syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/json/builders/ABTest-iPad-RunBenchmark-Tests/pendingBuilds');
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/json/builders/ABTest-iPad-RunBenchmark-Tests/builds/?select=-1');
            });
        });

        it('should fetch the right number of recent builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            syncer.pullBuildbot(3);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/json/builders/ABTest-iPad-RunBenchmark-Tests/pendingBuilds');
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/json/builders/ABTest-iPad-RunBenchmark-Tests/builds/?select=-1&select=-2&select=-3');
            });
        });

        it('should create BuildbotBuildEntry for pending builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];
            let promise = syncer.pullBuildbot();
            requests[0].resolve([samplePendingBuild()]);
            return promise.then((entries) => {
                assert.equal(entries.length, 1);
                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.ok(!entry.buildNumber());
                assert.ok(!entry.slaveName());
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/');
            });
        });

        it('should create BuildbotBuildEntry for in-progress builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            let promise = syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleInProgressBuild()});
                return promise;
            }).then((entries) => {
                assert.equal(entries.length, 1);
                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');
            });
        });

        it('should create BuildbotBuildEntry for finished builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            let promise = syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleFinishedBuild()});
                return promise;
            }).then((entries) => {
                assert.deepEqual(entries.length, 1);
                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');
            });
        });

        it('should create BuildbotBuildEntry for mixed pending, in-progress, finished, and missing builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild(123)]);

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleFinishedBuild(), [-2]: {'error': 'Not available'}, [-4]: sampleInProgressBuild()});
                return promise;
            }).then((entries) => {
                assert.deepEqual(entries.length, 3);

                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), null);
                assert.equal(entry.slaveName(), null);
                assert.equal(entry.buildRequestId(), 123);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/');

                entry = entries[1];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');

                entry = entries[2];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');
            });
        });

        it('should sort BuildbotBuildEntry by order', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild(456, 2), samplePendingBuild(123, 1)]);

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-3]: sampleFinishedBuild(), [-1]: {'error': 'Not available'}, [-2]: sampleInProgressBuild()});
                return promise;
            }).then((entries) => {
                assert.deepEqual(entries.length, 4);

                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), null);
                assert.equal(entry.slaveName(), null);
                assert.equal(entry.buildRequestId(), 123);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/');

                entry = entries[1];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), null);
                assert.equal(entry.slaveName(), null);
                assert.equal(entry.buildRequestId(), 456);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/');

                entry = entries[2];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');

                entry = entries[3];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');
            });
        });

        it('should override BuildbotBuildEntry for pending builds by in-progress builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild()]);

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleInProgressBuild()});
                return promise;
            }).then((entries) => {
                assert.equal(entries.length, 1);

                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');
            });
        });

        it('should override BuildbotBuildEntry for pending builds by finished builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild()]);

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleFinishedBuild(16733)});
                return promise;
            }).then((entries) => {
                assert.equal(entries.length, 1);

                let entry = entries[0];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');
            });
        });
    });

    describe('scheduleRequest', () => {
        it('should schedule a build request on a specified slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[0];

            const waitForRequest = MockRemoteAPI.waitForRequest();
            syncer.scheduleRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer), 'some-slave');
            return waitForRequest.then(() => {
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/builders/ABTest-iPhone-RunBenchmark-Tests/force');
                assert.equal(requests[0].method, 'POST');
                assert.deepEqual(requests[0].data, {
                    'build_request_id': '16733-' + MockModels.iphone.id(),
                    'desired_image': '13A452',
                    "opensource": "197463",
                    'forcescheduler': 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler',
                    'slavename': 'some-slave',
                    'test_name': 'speedometer'
                });
            });
        });
    });

    describe('scheduleRequestInGroupIfAvailable', () => {

        function pullBuildbotWithAssertion(syncer, pendingBuilds, inProgressAndFinishedBuilds)
        {
            const promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);
            requests[0].resolve(pendingBuilds);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve(inProgressAndFinishedBuilds);
                requests.length = 0;
                return promise;
            });
        }

        it('should schedule a build if builder has no builds if slaveList is not specified', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration())[0];

            return pullBuildbotWithAssertion(syncer, [], {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest));
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/builders/some%20builder/force');
                assert.equal(requests[0].method, 'POST');
                assert.deepEqual(requests[0].data, {id: '16733-' + MockModels.somePlatform.id(), 'os': '13A452', 'wk': '197463'});
            });
        });

        it('should schedule a build if builder only has finished builds if slaveList is not specified', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration())[0];

            return pullBuildbotWithAssertion(syncer, [], {[-1]: smallFinishedBuild()}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest));
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/builders/some%20builder/force');
                assert.equal(requests[0].method, 'POST');
                assert.deepEqual(requests[0].data, {id: '16733-' + MockModels.somePlatform.id(), 'os': '13A452', 'wk': '197463'});
            });
        });

        it('should not schedule a build if builder has a pending build if slaveList is not specified', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration())[0];

            return pullBuildbotWithAssertion(syncer, [smallPendingBuild()], {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest));
                assert.equal(requests.length, 0);
            });
        });

        it('should schedule a build if builder does not have pending or completed builds on the matching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[0];

            return pullBuildbotWithAssertion(syncer, [], {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/builders/ABTest-iPhone-RunBenchmark-Tests/force');
                assert.equal(requests[0].method, 'POST');
            });
        });

        it('should schedule a build if builder only has finished builds on the matching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            pullBuildbotWithAssertion(syncer, [], {[-1]: sampleFinishedBuild()}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/builders/ABTest-iPad-RunBenchmark-Tests/force');
                assert.equal(requests[0].method, 'POST');
            });
        });

        it('should not schedule a build if builder has a pending build on the maching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            pullBuildbotWithAssertion(syncer, [samplePendingBuild()], {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
                assert.equal(requests.length, 0);
            });
        });

        it('should schedule a build if builder only has a pending build on a non-maching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            return pullBuildbotWithAssertion(syncer, [samplePendingBuild(1, 1, 'another-slave')], {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
                assert.equal(requests.length, 1);
            });
        });

        it('should schedule a build if builder only has an in-progress build on the matching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            return pullBuildbotWithAssertion(syncer, [], {[-1]: sampleInProgressBuild()}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
                assert.equal(requests.length, 1);
            });
        });

        it('should schedule a build if builder has an in-progress build on another slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            return pullBuildbotWithAssertion(syncer, [], {[-1]: sampleInProgressBuild('other-slave')}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
                assert.equal(requests.length, 1);
            });
        });

        it('should not schedule a build if the request does not match any configuration', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[0];

            return pullBuildbotWithAssertion(syncer, [], {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
                assert.equal(requests.length, 0);
            });
        });

        it('should not schedule a build if a new request had been submitted to the same slave', (done) => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            pullBuildbotWithAssertion(syncer, [], {}).then(() => {
                syncer.scheduleRequest(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer), 'ABTest-iPad-0');
                syncer.scheduleRequest(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer), 'ABTest-iPad-1');
            }).then(() => {
                assert.equal(requests.length, 2);
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer));
            }).then(() => {
                assert.equal(requests.length, 2);
                done();
            }).catch(done);
        });

        it('should schedule a build if a new request had been submitted to another slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig())[1];

            return pullBuildbotWithAssertion(syncer, [], {}).then(() => {
                syncer.scheduleRequest(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer), 'ABTest-iPad-0');
                assert.equal(requests.length, 1);
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.ipad, MockModels.speedometer), 'ABTest-iPad-1');
                assert.equal(requests.length, 2);
            });
        });

        it('should not schedule a build if a new request had been submitted to the same builder without slaveList', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration())[0];

            return pullBuildbotWithAssertion(syncer, [], {}).then(() => {
                syncer.scheduleRequest(createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest), null);
                assert.equal(requests.length, 1);
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest));
                assert.equal(requests.length, 1);
            });
        });
    });
});
