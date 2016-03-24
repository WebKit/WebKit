'use strict';

let assert = require('assert');

require('./resources/mock-remote-api.js');
require('../tools/js/v3-models.js');
require('./resources/mock-v3-models.js');

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

function createSampleBuildRequest()
{
    let rootSet = RootSet.ensureSingleton('4197', {roots: [
        {'id': '111127', 'time': 1456955807334, 'repository': webkit, 'revision': '197463'},
        {'id': '111237', 'time': 1456931874000, 'repository': sharedRepository, 'revision': '80229'},
        {'id': '88930', 'time': 0, 'repository': ios, 'revision': '13A452'},
    ]});

    let request = BuildRequest.ensureSingleton('16733', {'rootSet': rootSet, 'status': 'pending'});
    return request;
}

let samplePendingBuilds = [
    {
        'builderName': 'ABTest-iPad-RunBenchmark-Tests',
        'builds': [],
        'properties': [
            ['build_request_id', '16733', 'Force Build Form'],
            ['desired_image', '13A452', 'Force Build Form'],
            ['owner', '<unknown>', 'Force Build Form'],
            ['test_name', 'speedometer', 'Force Build Form'],
            ['reason', 'force build','Force Build Form'],
            [
                'roots_dict',
                JSON.stringify(sampleRootSetData),
                'Force Build Form'
            ],
            ['scheduler', 'ABTest-iPad-Performance-Tests-ForceScheduler', 'Scheduler']
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
    }
];

describe('BuildbotSyncer', function () {
    describe('fetchPendingBuilds', function () {
        BuildbotSyncer.fetchPendingBuilds
    });

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
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest());
            assert.deepEqual(Object.keys(properties), ['desired_image', 'roots_dict', 'test_name', 'forcescheduler', 'build_request_id']);
        });

        it('should preserve non-parametric property values', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest());
            assert.equal(properties['test_name'], 'speedometer');
            assert.equal(properties['forcescheduler'], 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');

            properties = syncers[1]._propertiesForBuildRequest(createSampleBuildRequest());
            assert.equal(properties['test_name'], 'jetstream');
            assert.equal(properties['forcescheduler'], 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');
        });

        it('should resolve "root"', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest());
            assert.equal(properties['desired_image'], '13A452');
        });

        it('should resolve "rootsExcluding"', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest());
            assert.equal(properties['roots_dict'], JSON.stringify(sampleRootSetData));
        });

        it('should set the property for the build request id', function () {
            let syncers = BuildbotSyncer._loadConfig('http://build.webkit.org/', sampleiOSConfig());
            let properties = syncers[0]._propertiesForBuildRequest(createSampleBuildRequest());
            assert.equal(properties['build_request_id'], createSampleBuildRequest().id());
        });
    });

});