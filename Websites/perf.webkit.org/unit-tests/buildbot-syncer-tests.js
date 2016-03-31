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
        'shared':
            {
                'arguments': {
                    'desired_image': {'root': 'iOS'},
                    'roots_dict': {'rootsExcluding': ['iOS']}
                },
                'slaveArgument': 'slavename',
                'buildRequestArgument': 'build_request_id'
            },
        'types': {
            'speedometer': {
                'test': ['Speedometer'],
                'arguments': {'test_name': 'speedometer'}
            },
            'jetstream': {
                'test': ['JetStream'],
                'arguments': {'test_name': 'jetstream'}
            },
            "dromaeo-dom": {
                "test": ["Dromaeo", "DOM Core Tests"],
                "arguments": {"tests": "dromaeo-dom"}
            },
        },
        'builders': {
            'iPhone-bench': {
                'builder': 'ABTest-iPhone-RunBenchmark-Tests',
                'arguments': { 'forcescheduler': 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler' }
            },
            'iPad-bench': {
                'builder': 'ABTest-iPad-RunBenchmark-Tests',
                'arguments': { 'forcescheduler': 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler' }
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

let sampleRootSetData = {
    'WebKit': {
        'id': '111127',
        'time': 1456955807334,
        'repository': 'WebKit',
        'revision': '197463',
    },
    'Shared': {
        'id': '111237',
        'time': 1456931874000,
        'repository': 'Shared',
        'revision': '80229',
    }
};

function createSampleBuildRequest(platform, test)
{
    assert(platform instanceof Platform);
    assert(test instanceof Test);

    let rootSet = RootSet.ensureSingleton('4197', {roots: [
        {'id': '111127', 'time': 1456955807334, 'repository': MockModels.webkit, 'revision': '197463'},
        {'id': '111237', 'time': 1456931874000, 'repository': MockModels.sharedRepository, 'revision': '80229'},
        {'id': '88930', 'time': 0, 'repository': MockModels.ios, 'revision': '13A452'},
    ]});

    let request = BuildRequest.ensureSingleton('16733', {'rootSet': rootSet, 'status': 'pending', 'platform': platform, 'test': test});
    return request;
}

function samplePendingBuild(buildRequestId)
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
            [
                'roots_dict',
                JSON.stringify(sampleRootSetData),
                'Force Build Form'
            ],
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
        'submittedAt': 1458704983
    };
}

function sampleInProgressBuild()
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
            ['roots_dict', JSON.stringify(sampleRootSetData), 'Force Build Form'],
            ['scheduler', 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler', 'Scheduler'],
            ['slavename', 'ABTest-iPad-0', 'BuildSlave'],
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

function sampleFinishedBuild(buildRequestId)
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
            ['roots_dict', JSON.stringify(sampleRootSetData), 'Force Build Form'],
            ['scheduler', 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler', 'Scheduler'],
            ['slavename', 'ABTest-iPad-0', 'BuildSlave'],
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

describe('BuildbotSyncer', function () {
    MockModels.inject();
    let requests = MockRemoteAPI.inject();

    describe('_loadConfig', function () {

        function smallConfiguration()
        {
            return {
                'builder': 'some builder',
                'platform': 'some platform',
                'test': ['some test'],
                'arguments': {},
                'buildRequestArgument': 'id'};
        }

        it('should create BuildbotSyncer objects for a configuration that specify all required options', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [smallConfiguration()]});
            assert.equal(syncers.length, 1);
        });

        it('should throw when some required options are missing', function () {
            assert.throws(function () {
                let config = smallConfiguration();
                delete config['builder'];
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            }, 'builder should be a required option');
            assert.throws(function () {
                let config = smallConfiguration();
                delete config['platform'];
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            }, 'platform should be a required option');
            assert.throws(function () {
                let config = smallConfiguration();
                delete config['test'];
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            }, 'test should be a required option');
            assert.throws(function () {
                let config = smallConfiguration();
                delete config['arguments'];
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                delete config['buildRequestArgument'];
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
        });

        it('should throw when a test name is not an array of strings', function () {
            assert.throws(function () {
                let config = smallConfiguration();
                config.test = 'some test';
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                config.test = [1];
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
        });

        it('should throw when arguments is not an object', function () {
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = 'hello';
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
        });

        it('should throw when arguments\'s values are malformed', function () {
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = {'some': {'root': 'some root', 'rootsExcluding': ['other root']}};
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = {'some': {'otherKey': 'some root'}};
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = {'some': {'root': ['a', 'b']}};
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = {'some': {'root': 1}};
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = {'some': {'rootsExcluding': 'a'}};
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
            assert.throws(function () {
                let config = smallConfiguration();
                config.arguments = {'some': {'rootsExcluding': [1]}};
                BuildbotSyncer._loadConfig('http://build.webkit.org/', {'configurations': [config]});
            });
        });

        it('should create BuildbotSyncer objects for valid configurations', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            assert.equal(syncers.length, 5);
            assert.ok(syncers[0] instanceof BuildbotSyncer);
            assert.ok(syncers[1] instanceof BuildbotSyncer);
            assert.ok(syncers[2] instanceof BuildbotSyncer);
            assert.ok(syncers[3] instanceof BuildbotSyncer);
            assert.ok(syncers[4] instanceof BuildbotSyncer);
        });

        it('should parse builder names correctly', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            assert.equal(syncers[0].builderName(), 'ABTest-iPhone-RunBenchmark-Tests');
            assert.equal(syncers[1].builderName(), 'ABTest-iPhone-RunBenchmark-Tests');
            assert.equal(syncers[2].builderName(), 'ABTest-iPhone-RunBenchmark-Tests');
            assert.equal(syncers[3].builderName(), 'ABTest-iPad-RunBenchmark-Tests');
            assert.equal(syncers[4].builderName(), 'ABTest-iPad-RunBenchmark-Tests');
        });

        it('should parse platform names correctly', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            assert.equal(syncers[0].platformName(), 'iPhone');
            assert.equal(syncers[1].platformName(), 'iPhone');
            assert.equal(syncers[2].platformName(), 'iPhone');
            assert.equal(syncers[3].platformName(), 'iPad');
            assert.equal(syncers[4].platformName(), 'iPad');
        });

        it('should parse test names correctly', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            assert.deepEqual(syncers[0].testPath(), ['Speedometer']);
            assert.deepEqual(syncers[1].testPath(), ['JetStream']);
            assert.deepEqual(syncers[2].testPath(), ['Dromaeo', 'DOM Core Tests']);
            assert.deepEqual(syncers[3].testPath(), ['Speedometer']);
            assert.deepEqual(syncers[4].testPath(), ['JetStream']);
        });
    });

    describe('_propertiesForBuildRequest', function () {
        it('should include all properties specified in a given configuration', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.deepEqual(Object.keys(properties), ['desired_image', 'roots_dict', 'test_name', 'forcescheduler', 'build_request_id']);
        });

        it('should preserve non-parametric property values', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.equal(properties['test_name'], 'speedometer');
            assert.equal(properties['forcescheduler'], 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');

            properties = syncers[1]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.equal(properties['test_name'], 'jetstream');
            assert.equal(properties['forcescheduler'], 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');
        });

        it('should resolve "root"', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.equal(properties['desired_image'], '13A452');
        });

        it('should resolve "rootsExcluding"', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest(MockModels.iphone, MockModels.speedometer));
            assert.equal(properties['roots_dict'], JSON.stringify(sampleRootSetData));
        });

        it('should set the property for the build request id', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            let properties = syncers[0]._propertiesForBuildRequest(request);
            assert.equal(properties['build_request_id'], request.id());
        });
    });

    describe('pullBuildbot', function () {
        it('should fetch pending builds from the right URL', function () {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];
            assert.equal(syncer.builderName(), 'ABTest-iPad-RunBenchmark-Tests');
            let expectedURL = 'http://build.webkit.org/json/builders/ABTest-iPad-RunBenchmark-Tests/pendingBuilds';
            assert.equal(syncer.urlForPendingBuildsJSON(), expectedURL);
            syncer.pullBuildbot();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, expectedURL);
        });

        it('should fetch recent builds once pending builds have been fetched', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];
            assert.equal(syncer.builderName(), 'ABTest-iPad-RunBenchmark-Tests');

            syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, 'http://build.webkit.org/json/builders/ABTest-iPad-RunBenchmark-Tests/pendingBuilds');
            requests[0].resolve([]);
            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, 'http://build.webkit.org/json/builders/ABTest-iPad-RunBenchmark-Tests/builds/?select=-1');
                done();
            }).catch(done);
        });

        it('should fetch the right number of recent builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];

            syncer.pullBuildbot(3);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, 'http://build.webkit.org/json/builders/ABTest-iPad-RunBenchmark-Tests/pendingBuilds');
            requests[0].resolve([]);
            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, 'http://build.webkit.org/json/builders/ABTest-iPad-RunBenchmark-Tests/builds/?select=-1&select=-2&select=-3');
                done();
            }).catch(done);
        });

        it('should create BuildbotBuildEntry for pending builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];
            let promise = syncer.pullBuildbot();
            requests[0].resolve([samplePendingBuild()]);
            promise.then(function (entries) {
                assert.deepEqual(Object.keys(entries), ['16733']);
                let entry = entries['16733'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.ok(!entry.buildNumber());
                assert.ok(!entry.slaveName());
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/');
                done();
            }).catch(done);
        });

        it('should create BuildbotBuildEntry for in-progress builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];

            let promise = syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            requests[0].resolve([]);
            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleInProgressBuild()});
            }).catch(done);

            promise.then(function (entries) {
                assert.deepEqual(Object.keys(entries), ['16733']);
                let entry = entries['16733'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');
                done();
            }).catch(done);
        });

        it('should create BuildbotBuildEntry for finished builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];

            let promise = syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            requests[0].resolve([]);
            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleFinishedBuild()});
            }).catch(done);

            promise.then(function (entries) {
                assert.deepEqual(Object.keys(entries), ['18935']);
                let entry = entries['18935'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');
                done();
            }).catch(done);
        });

        it('should create BuildbotBuildEntry for mixed pending, in-progress, finished, and missing builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild(123, 456)]);

            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleFinishedBuild(), [-2]: {'error': 'Not available'}, [-4]: sampleInProgressBuild()});
            }).catch(done);

            promise.then(function (entries) {
                assert.deepEqual(Object.keys(entries), ['123', '16733', '18935']);

                let entry = entries['123'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), null);
                assert.equal(entry.slaveName(), null);
                assert.equal(entry.buildRequestId(), 123);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/');

                entry = entries['16733'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');

                entry = entries['18935'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');

                done();
            }).catch(done);
        });

        it('should override BuildbotBuildEntry for pending builds by in-progress builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild()]);

            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleInProgressBuild()});
            }).catch(done);

            promise.then(function (entries) {
                assert.deepEqual(Object.keys(entries), ['16733']);

                let entry = entries['16733'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/614');

                done();
            }).catch(done);
        });

        it('should override BuildbotBuildEntry for pending builds by finished builds', function (done) {
            let syncer = BuildbotSyncer._loadConfig('http://build.webkit.org', sampleiOSConfig())[3];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve([samplePendingBuild()]);

            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                requests[1].resolve({[-1]: sampleFinishedBuild(16733)});
            }).catch(done);

            promise.then(function (entries) {
                assert.deepEqual(Object.keys(entries), ['16733']);

                let entry = entries['16733'];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/builders/ABTest-iPad-RunBenchmark-Tests/builds/1755');

                done();
            }).catch(done);
        });

    });
});
