'use strict';

let assert = require('assert');

require('../tools/js/v3-models.js');
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;

const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
const MockModels = require('./resources/mock-v3-models.js').MockModels;

const BuildbotBuildEntry = require('../tools/js/buildbot-syncer.js').BuildbotBuildEntry;
const BuildbotSyncer = require('../tools/js/buildbot-syncer.js').BuildbotSyncer;

function sampleiOSConfig()
{
    return {
        'slaveArgument': 'slavename',
        'buildRequestArgument': 'build_request_id',
        'repositoryGroups': {
            'ios-svn-webkit': {
                'repositories': {'WebKit': {}, 'iOS': {}},
                'testProperties': {
                    'desired_image': {'revision': 'iOS'},
                    'opensource': {'revision': 'WebKit'},
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
                'properties': {'forcescheduler': 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler'},
                'slaveList': ['ABTest-iPhone-0'],
            },
            'iPad-bench': {
                'builder': 'ABTest-iPad-RunBenchmark-Tests',
                'properties': {'forcescheduler': 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler'},
                'slaveList': ['ABTest-iPad-0', 'ABTest-iPad-1'],
            },
            'iOS-builder': {
                'builder': 'ABTest-iOS-Builder',
                'properties': {'forcescheduler': 'ABTest-Builder-ForceScheduler'},
            },
        },
        'buildConfigurations': [
            {'builders': ['iOS-builder'], 'platforms': ['iPhone', 'iPad']},
        ],
        'testConfigurations': [
            {'builders': ['iPhone-bench'], 'types': ['speedometer', 'jetstream', 'dromaeo-dom'], 'platforms': ['iPhone']},
            {'builders': ['iPad-bench'], 'types': ['speedometer', 'jetstream'], 'platforms': ['iPad']},
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
                "properties": {"forcescheduler": "force-iphone-ab-tests"},
            },
            "iphone-2": {
                "builder": "iPhone 2 AB Tests",
                "properties": {"forcescheduler": "force-iphone-2-ab-tests"},
            },
            "ipad": {
                "builder": "iPad AB Tests",
                "properties": {"forcescheduler": "force-ipad-ab-tests"},
            },
        },
        "testConfigurations": [
            {
                "builders": ["iphone", "iphone-2"],
                "platforms": ["iPhone", "iOS 10 iPhone"],
                "types": ["iphone-plt", "speedometer"],
            },
            {
                "builders": ["ipad"],
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
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'wk': {'revision': 'WebKit'}
                }
            }
        },
        'types': {
            'some-test': {
                'test': ['Some test'],
            }
        },
        'builders': {
            'some-builder': {
                'builder': 'some builder',
                'properties': {'forcescheduler': 'some-builder-ForceScheduler'}
            }
        },
        'testConfigurations': [{
            'builders': ['some-builder'],
            'platforms': ['Some platform'],
            'types': ['some-test'],
        }]
    };
}

function builderNameToIDMap()
{
    return {
        'some builder' : '100',
        'ABTest-iPhone-RunBenchmark-Tests': '101',
        'ABTest-iPad-RunBenchmark-Tests': '102',
        'ABTest-iOS-Builder': '103',
        'iPhone AB Tests' : '104',
        'iPhone 2 AB Tests': '105',
        'iPad AB Tests': '106'
    };
}

function smallPendingBuild()
{
    return samplePendingBuildRequests(null, null, null, "some builder");
}

function smallInProgressBuild()
{
    return sampleInProgressBuild();
}

function smallFinishedBuild()
{
    return sampleFinishedBuild(null, null, "some builder");
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

function createSampleBuildRequestWithPatch(platform, test, order)
{
    assert(platform instanceof Platform);
    assert(!test || test instanceof Test);

    const webkit197463 = CommitLog.ensureSingleton('111127', {'id': '111127', 'time': 1456955807334, 'repository': MockModels.webkit, 'revision': '197463'});
    const shared111237 = CommitLog.ensureSingleton('111237', {'id': '111237', 'time': 1456931874000, 'repository': MockModels.sharedRepository, 'revision': '80229'});
    const ios13A452 = CommitLog.ensureSingleton('88930', {'id': '88930', 'time': 0, 'repository': MockModels.ios, 'revision': '13A452'});

    const patch = new UploadedFile(453, {'createdAt': new Date('2017-05-01T19:16:53Z'), 'filename': 'patch.dat', 'extension': '.dat', 'author': 'some user',
        size: 534637, sha256: '169463c8125e07c577110fe144ecd63942eb9472d438fc0014f474245e5df8a1'});

    const root = new UploadedFile(456, {'createdAt': new Date('2017-05-01T21:03:27Z'), 'filename': 'root.dat', 'extension': '.dat', 'author': 'some user',
        size: 16452234, sha256: '03eed7a8494ab8794c44b7d4308e55448fc56f4d6c175809ba968f78f656d58d'});

    const commitSet = CommitSet.ensureSingleton('53246456', {customRoots: [root], revisionItems: [{commit: webkit197463, patch, requiresBuild: true}, {commit: shared111237}, {commit: ios13A452}]});

    return BuildRequest.ensureSingleton(`6345645376-${order}`, {'triggerable': MockModels.triggerable,
        repositoryGroup: MockModels.svnRepositoryGroup,
        'commitSet': commitSet, 'status': 'pending', 'platform': platform, 'test': test, 'order': order});
}

function createSampleBuildRequestWithOwnedCommit(platform, test, order)
{
    assert(platform instanceof Platform);
    assert(!test || test instanceof Test);

    const webkit197463 = CommitLog.ensureSingleton('111127', {'id': '111127', 'time': 1456955807334, 'repository': MockModels.webkit, 'revision': '197463'});
    const owner111289 = CommitLog.ensureSingleton('111289', {'id': '111289', 'time': 1456931874000, 'repository': MockModels.ownerRepository, 'revision': 'owner-001'});
    const owned111222 = CommitLog.ensureSingleton('111222', {'id': '111222', 'time': 1456932774000, 'repository': MockModels.ownedRepository, 'revision': 'owned-002'});
    const ios13A452 = CommitLog.ensureSingleton('88930', {'id': '88930', 'time': 0, 'repository': MockModels.ios, 'revision': '13A452'});

    const root = new UploadedFile(456, {'createdAt': new Date('2017-05-01T21:03:27Z'), 'filename': 'root.dat', 'extension': '.dat', 'author': 'some user',
        size: 16452234, sha256: '03eed7a8494ab8794c44b7d4308e55448fc56f4d6c175809ba968f78f656d58d'});

    const commitSet = CommitSet.ensureSingleton('53246486', {customRoots: [root], revisionItems: [{commit: webkit197463}, {commit: owner111289}, {commit: owned111222, commitOwner: owner111289, requiresBuild: true}, {commit: ios13A452}]});

    return BuildRequest.ensureSingleton(`6345645370-${order}`, {'triggerable': MockModels.triggerable,
        repositoryGroup: MockModels.svnRepositoryWithOwnedRepositoryGroup,
        'commitSet': commitSet, 'status': 'pending', 'platform': platform, 'test': test, 'order': order});
}

function createSampleBuildRequestWithOwnedCommitAndPatch(platform, test, order)
{
    assert(platform instanceof Platform);
    assert(!test || test instanceof Test);

    const webkit197463 = CommitLog.ensureSingleton('111127', {'id': '111127', 'time': 1456955807334, 'repository': MockModels.webkit, 'revision': '197463'});
    const owner111289 = CommitLog.ensureSingleton('111289', {'id': '111289', 'time': 1456931874000, 'repository': MockModels.ownerRepository, 'revision': 'owner-001'});
    const owned111222 = CommitLog.ensureSingleton('111222', {'id': '111222', 'time': 1456932774000, 'repository': MockModels.ownedRepository, 'revision': 'owned-002'});
    const ios13A452 = CommitLog.ensureSingleton('88930', {'id': '88930', 'time': 0, 'repository': MockModels.ios, 'revision': '13A452'});

    const patch = new UploadedFile(453, {'createdAt': new Date('2017-05-01T19:16:53Z'), 'filename': 'patch.dat', 'extension': '.dat', 'author': 'some user',
        size: 534637, sha256: '169463c8125e07c577110fe144ecd63942eb9472d438fc0014f474245e5df8a1'});

    const commitSet = CommitSet.ensureSingleton('53246486', {customRoots: [], revisionItems: [{commit: webkit197463, patch, requiresBuild: true}, {commit: owner111289}, {commit: owned111222, commitOwner: owner111289, requiresBuild: true}, {commit: ios13A452}]});

    return BuildRequest.ensureSingleton(`6345645370-${order}`, {'triggerable': MockModels.triggerable,
        repositoryGroup: MockModels.svnRepositoryWithOwnedRepositoryGroup,
        'commitSet': commitSet, 'status': 'pending', 'platform': platform, 'test': test, 'order': order});
}

function samplePendingBuildRequestData(buildRequestId, buildTime, workerName, builderId)
{
    return {
        "builderid": builderId || 102,
        "buildrequestid": 17,
        "buildsetid": 894720,
        "claimed": false,
        "claimed_at": null,
        "claimed_by_masterid": null,
        "complete": false,
        "complete_at": null,
        "priority": 0,
        "results": -1,
        "submitted_at": buildTime || 1458704983,
        "waited_for": false,
        "properties": {
            "build_request_id": [buildRequestId || 16733, "Force Build Form"],
            "scheduler": ["ABTest-iPad-RunBenchmark-Tests-ForceScheduler", "Scheduler"],
            "slavename": [workerName, "Worker (deprecated)"],
            "workername": [workerName, "Worker"]
        }
    };
}

function samplePendingBuildRequests(buildRequestId, buildTime, workerName, builderName)
{
    return {
        "buildrequests" : [samplePendingBuildRequestData(buildRequestId, buildTime, workerName, builderNameToIDMap()[builderName])]
    };
}

function sampleBuildData(workerName, isComplete, buildRequestId, buildNumber, builderId)
{
    return {
        "builderid": builderId || 102,
        "number": buildNumber || 614,
        "buildrequestid": 17,
        "complete": isComplete,
        "complete_at": null,
        "buildid": 418744,
        "masterid": 1,
        "results": null,
        "started_at": 1513725109,
        "state_string": "building",
        "workerid": 41,
        "properties": {
            "build_request_id": [buildRequestId || 16733, "Force Build Form"],
            "platform": ["mac", "Unknown"],
            "scheduler": ["ABTest-iPad-RunBenchmark-Tests-ForceScheduler", "Scheduler"],
            "slavename": [workerName || "ABTest-iPad-0", "Worker (deprecated)"],
            "workername": [workerName || "ABTest-iPad-0", "Worker"]
        }
    };
}

function sampleInProgressBuildData(workerName)
{
    return sampleBuildData(workerName, false);
}

function sampleInProgressBuild(workerName)
{
    return {
        "builds": [sampleInProgressBuildData(workerName)]
    };
}

function sampleFinishedBuildData(buildRequestId, workerName, builderName)
{
    return sampleBuildData(workerName, true, buildRequestId || 18935, 1755, builderNameToIDMap()[builderName]);
}

function sampleFinishedBuild(buildRequestId, workerName, builderName)
{
    return {
        "builds": [sampleFinishedBuildData(buildRequestId, workerName, builderName)]
    };
}

describe('BuildbotSyncer', () => {
    MockModels.inject();
    const requests = MockRemoteAPI.inject('http://build.webkit.org', BrowserPrivilegedAPI);

    describe('_loadConfig', () => {

        it('should create BuildbotSyncer objects for a configuration that specify all required options', () => {
            assert.equal(BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration(), builderNameToIDMap()).length, 1);
        });

        it('should throw when some required options are missing', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.builders;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"some-builder" is not a valid builder in the configuration/);
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.types;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"some-test" is not a valid type in the configuration/);
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.testConfigurations[0].builders;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /The test configuration 1 does not specify "builders" as an array/);
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.testConfigurations[0].platforms;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /The test configuration 1 does not specify "platforms" as an array/);
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.testConfigurations[0].types;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /The test configuration 0 does not specify "types" as an array/);
            assert.throws(() => {
                const config = smallConfiguration();
                delete config.buildRequestArgument;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /buildRequestArgument must specify the name of the property used to store the build request ID/);
        });

        it('should throw when a test name is not an array of strings', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.testConfigurations[0].types = 'some test';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /The test configuration 0 does not specify "types" as an array/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.testConfigurations[0].types = [1];
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"1" is not a valid type in the configuration/);
        });

        it('should throw when properties is not an object', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.builders[Object.keys(config.builders)[0]].properties = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Build properties should be a dictionary/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.types[Object.keys(config.types)[0]].properties = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Build properties should be a dictionary/);
        });

        it('should throw when testProperties is specifed in a type or a builder', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                const firstType = Object.keys(config.types)[0];
                config.types[firstType].testProperties = {};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Unrecognized parameter "testProperties"/);
            assert.throws(() => {
                const config = smallConfiguration();
                const firstBuilder = Object.keys(config.builders)[0];
                config.builders[firstBuilder].testProperties = {};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Unrecognized parameter "testProperties"/);
        });

        it('should throw when buildProperties is specifed in a type or a builder', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                const firstType = Object.keys(config.types)[0];
                config.types[firstType].buildProperties = {};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Unrecognized parameter "buildProperties"/);
            assert.throws(() => {
                const config = smallConfiguration();
                const firstBuilder = Object.keys(config.builders)[0];
                config.builders[firstBuilder].buildProperties = {};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Unrecognized parameter "buildProperties"/);
        });

        it('should throw when properties for a type is malformed', () => {
            const firstType = Object.keys(smallConfiguration().types)[0];
            assert.throws(() => {
                const config = smallConfiguration();
                config.types[firstType].properties = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Build properties should be a dictionary/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.types[firstType].properties = {'some': {'otherKey': 'some root'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.types[firstType].properties = {'some': {'otherKey': 'some root'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.types[firstType].properties = {'some': {'revision': 'WebKit'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.types[firstType].properties = {'some': 1};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, / Build properties "some" specifies a non-string value of type "object"/);
        });

        it('should throw when properties for a builder is malformed', () => {
            const firstBuilder = Object.keys(smallConfiguration().builders)[0];
            assert.throws(() => {
                const config = smallConfiguration();
                config.builders[firstBuilder].properties = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Build properties should be a dictionary/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.builders[firstBuilder].properties = {'some': {'otherKey': 'some root'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.builders[firstBuilder].properties = {'some': {'otherKey': 'some root'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.builders[firstBuilder].properties = {'some': {'revision': 'WebKit'}};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.builders[firstBuilder].properties = {'some': 1};
                BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            }, /Build properties "some" specifies a non-string value of type "object"/);
        });

        it('should create BuildbotSyncer objects for valid configurations', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            assert.equal(syncers.length, 3);
            assert.ok(syncers[0] instanceof BuildbotSyncer);
            assert.ok(syncers[1] instanceof BuildbotSyncer);
            assert.ok(syncers[2] instanceof BuildbotSyncer);
        });

        it('should parse builder names correctly', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            assert.equal(syncers[0].builderName(), 'ABTest-iPhone-RunBenchmark-Tests');
            assert.equal(syncers[1].builderName(), 'ABTest-iPad-RunBenchmark-Tests');
            assert.equal(syncers[2].builderName(), 'ABTest-iOS-Builder');
        });

        it('should parse test configurations with build configurations correctly', () => {
            let syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());

            let configurations = syncers[0].testConfigurations();
            assert(syncers[0].isTester());
            assert.equal(configurations.length, 3);
            assert.equal(configurations[0].platform, MockModels.iphone);
            assert.equal(configurations[0].test, MockModels.speedometer);
            assert.equal(configurations[1].platform, MockModels.iphone);
            assert.equal(configurations[1].test, MockModels.jetstream);
            assert.equal(configurations[2].platform, MockModels.iphone);
            assert.equal(configurations[2].test, MockModels.domcore);
            assert.deepEqual(syncers[0].buildConfigurations(), []);

            configurations = syncers[1].testConfigurations();
            assert(syncers[1].isTester());
            assert.equal(configurations.length, 2);
            assert.equal(configurations[0].platform, MockModels.ipad);
            assert.equal(configurations[0].test, MockModels.speedometer);
            assert.equal(configurations[1].platform, MockModels.ipad);
            assert.equal(configurations[1].test, MockModels.jetstream);
            assert.deepEqual(syncers[1].buildConfigurations(), []);

            assert(!syncers[2].isTester());
            assert.deepEqual(syncers[2].testConfigurations(), []);
            configurations = syncers[2].buildConfigurations();
            assert.equal(configurations.length, 2);
            assert.equal(configurations[0].platform, MockModels.iphone);
            assert.equal(configurations[0].test, null);
            assert.equal(configurations[1].platform, MockModels.ipad);
            assert.equal(configurations[1].test, null);
        });

        it('should throw when a build configuration use the same builder as a test configuration', () => {
            assert.throws(() => {
                const config = sampleiOSConfig();
                config.buildConfigurations[0].builders = config.testConfigurations[0].builders;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            });
        });

        it('should parse test configurations with types and platforms expansions correctly', () => {
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfigWithExpansions(), builderNameToIDMap());

            assert.equal(syncers.length, 3);

            let configurations = syncers[0].testConfigurations();
            assert.equal(configurations.length, 4);
            assert.equal(configurations[0].platform, MockModels.iphone);
            assert.equal(configurations[0].test, MockModels.iPhonePLT);
            assert.equal(configurations[1].platform, MockModels.iphone);
            assert.equal(configurations[1].test, MockModels.speedometer);
            assert.equal(configurations[2].platform, MockModels.iOS10iPhone);
            assert.equal(configurations[2].test, MockModels.iPhonePLT);
            assert.equal(configurations[3].platform, MockModels.iOS10iPhone);
            assert.equal(configurations[3].test, MockModels.speedometer);
            assert.deepEqual(syncers[0].buildConfigurations(), []);

            configurations = syncers[1].testConfigurations();
            assert.equal(configurations.length, 4);
            assert.equal(configurations[0].platform, MockModels.iphone);
            assert.equal(configurations[0].test, MockModels.iPhonePLT);
            assert.equal(configurations[1].platform, MockModels.iphone);
            assert.equal(configurations[1].test, MockModels.speedometer);
            assert.equal(configurations[2].platform, MockModels.iOS10iPhone);
            assert.equal(configurations[2].test, MockModels.iPhonePLT);
            assert.equal(configurations[3].platform, MockModels.iOS10iPhone);
            assert.equal(configurations[3].test, MockModels.speedometer);
            assert.deepEqual(syncers[1].buildConfigurations(), []);

            configurations = syncers[2].testConfigurations();
            assert.equal(configurations.length, 2);
            assert.equal(configurations[0].platform, MockModels.ipad);
            assert.equal(configurations[0].test, MockModels.iPadPLT);
            assert.equal(configurations[1].platform, MockModels.ipad);
            assert.equal(configurations[1].test, MockModels.speedometer);
            assert.deepEqual(syncers[2].buildConfigurations(), []);
        });

        it('should throw when repositoryGroups is not an object', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = 1;
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /repositoryGroups must specify a dictionary from the name to its definition/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = 'hello';
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /repositoryGroups must specify a dictionary from the name to its definition/);
        });

        it('should throw when a repository group does not specify a dictionary of repositories', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {testProperties: {}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" does not specify a dictionary of repositories/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: 1}, testProperties: {}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" does not specify a dictionary of repositories/);
        });

        it('should throw when a repository group specifies an empty dictionary', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {}, testProperties: {}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" does not specify any repository/);
        });

        it('should throw when a repository group specifies an invalid repository name', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'InvalidRepositoryName': {}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"InvalidRepositoryName" is not a valid repository name/);
        });

        it('should throw when a repository group specifies a repository with a non-dictionary value', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': 1}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"WebKit" specifies a non-dictionary value/);
        });

        it('should throw when the description of a repository group is not a string', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': {}}, description: 1}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" have an invalid description/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': {}}, description: [1, 2]}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" have an invalid description/);
        });

        it('should throw when a repository group does not specify a dictionary of properties', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': {}}, testProperties: 1}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies the test configurations with an invalid type/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': {}}, testProperties: 'hello'}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies the test configurations with an invalid type/);
        });

        it('should throw when a repository group refers to a non-existent repository in the properties dictionary', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': {}}, testProperties: {'wk': {revision: 'InvalidRepository'}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" an invalid repository "InvalidRepository"/);
        });

        it('should throw when a repository group refers to a repository which is not listed in the list of repositories', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {repositories: {'WebKit': {}}, testProperties: {'os': {revision: 'iOS'}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" an invalid repository "iOS"/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}},
                    testProperties: {'wk': {revision: 'WebKit'}, 'install-roots': {'roots': {}}},
                    buildProperties: {'os': {revision: 'iOS'}},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" an invalid repository "iOS"/);
        });

        it('should throw when a repository group refers to a repository in building a patch which does not accept a patch', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}, 'iOS': {}},
                    testProperties: {'wk': {revision: 'WebKit'}, 'ios': {revision: 'iOS'}, 'install-roots': {'roots': {}}},
                    buildProperties: {'wk': {revision: 'WebKit'}, 'ios': {revision: 'iOS'}, 'wk-patch': {patch: 'iOS'}},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies a patch for "iOS" but it does not accept a patch/);
        });

        it('should throw when a repository group specifies a patch without specifying a revision', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}},
                    testProperties: {'wk': {revision: 'WebKit'}, 'install-roots': {'roots': {}}},
                    buildProperties: {'wk-patch': {patch: 'WebKit'}},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies a patch for "WebKit" but does not specify a revision/);
        });

        it('should throw when a repository group does not use a listed repository', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, testProperties: {}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" does not use some of the repositories listed in testing/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}},
                    testProperties: {'wk': {revision: 'WebKit'}, 'install-roots': {'roots': {}}},
                    buildProperties: {},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" does not use some of the repositories listed in building a patch/);
        });

        it('should throw when a repository group specifies non-boolean value to acceptsRoots', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, 'testProperties': {'webkit': {'revision': 'WebKit'}}, acceptsRoots: 1}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" contains invalid acceptsRoots value:/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {}}, 'testProperties': {'webkit': {'revision': 'WebKit'}}, acceptsRoots: []}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" contains invalid acceptsRoots value:/);
        });

        it('should throw when a repository group specifies non-boolean value to acceptsPatch', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {acceptsPatch: 1}}, 'testProperties': {'webkit': {'revision': 'WebKit'}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"WebKit" contains invalid acceptsPatch value:/);
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {acceptsPatch: []}}, 'testProperties': {'webkit': {'revision': 'WebKit'}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /"WebKit" contains invalid acceptsPatch value:/);
        });

        it('should throw when a repository group specifies a patch in testProperties', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {'repositories': {'WebKit': {acceptsPatch: true}},
                    'testProperties': {'webkit': {'revision': 'WebKit'}, 'webkit-patch': {'patch': 'WebKit'}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies a patch for "WebKit" in the properties for testing/);
        });

        it('should throw when a repository group specifies roots in buildProperties', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}},
                    testProperties: {'webkit': {revision: 'WebKit'}, 'install-roots': {'roots': {}}},
                    buildProperties: {'webkit': {revision: 'WebKit'}, 'patch': {patch: 'WebKit'}, 'install-roots': {roots: {}}},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies roots in the properties for building/);
        });

        it('should throw when a repository group that does not accept roots specifies roots in testProperties', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {}},
                    testProperties: {'webkit': {'revision': 'WebKit'}, 'install-roots': {'roots': {}}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies roots in a property but it does not accept roots/);
        });

        it('should throw when a repository group specifies buildProperties but does not accept roots', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}},
                    testProperties: {'webkit': {revision: 'WebKit'}},
                    buildProperties: {'webkit': {revision: 'WebKit'}, 'webkit-patch': {patch: 'WebKit'}}}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies the properties for building but does not accept roots in testing/);
        });

        it('should throw when a repository group specifies buildProperties but does not accept any patch', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {}},
                    testProperties: {'webkit': {'revision': 'WebKit'}, 'install-roots': {'roots': {}}},
                    buildProperties: {'webkit': {'revision': 'WebKit'}},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" specifies the properties for building but does not accept any patches/);
        });

        it('should throw when a repository group accepts roots but does not specify roots in testProperties', () => {
            assert.throws(() => {
                const config = smallConfiguration();
                config.repositoryGroups = {'some-group': {
                    repositories: {'WebKit': {acceptsPatch: true}},
                    testProperties: {'webkit': {revision: 'WebKit'}},
                    buildProperties: {'webkit': {revision: 'WebKit'}, 'webkit-patch': {patch: 'WebKit'}},
                    acceptsRoots: true}};
                BuildbotSyncer._loadConfig(MockRemoteAPI, config, builderNameToIDMap());
            }, /Repository group "some-group" accepts roots but does not specify roots in testProperties/);
        });
    });

    describe('_propertiesForBuildRequest', () => {
        it('should include all properties specified in a given configuration', () => {
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            const properties = syncers[0]._propertiesForBuildRequest(request, [request]);
            assert.deepEqual(Object.keys(properties).sort(), ['build_request_id', 'desired_image', 'forcescheduler', 'opensource', 'test_name']);
        });

        it('should preserve non-parametric property values', () => {
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            let request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            let properties = syncers[0]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['test_name'], 'speedometer');
            assert.equal(properties['forcescheduler'], 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');

            request = createSampleBuildRequest(MockModels.ipad, MockModels.jetstream);
            properties = syncers[1]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['test_name'], 'jetstream');
            assert.equal(properties['forcescheduler'], 'ABTest-iPad-RunBenchmark-Tests-ForceScheduler');
        });

        it('should resolve "root"', () => {
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            const properties = syncers[0]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['desired_image'], '13A452');
        });

        it('should resolve "revision"', () => {
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            const properties = syncers[0]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['opensource'], '197463');
        });

        it('should resolve "patch"', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['ios-svn-webkit'] = {
                'repositories': {'WebKit': {'acceptsPatch': true}, 'Shared': {}, 'iOS': {}},
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'webkit': {'revision': 'WebKit'},
                    'shared': {'revision': 'Shared'},
                    'roots': {'roots': {}},
                },
                'buildProperties': {
                    'webkit': {'revision': 'WebKit'},
                    'webkit-patch': {'patch': 'WebKit'},
                    'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-webkit'},
                    'build-webkit': {'ifRepositorySet': ['WebKit'], 'value': true},
                    'shared': {'revision': 'Shared'},
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const request = createSampleBuildRequestWithPatch(MockModels.iphone, null, -1);
            const properties = syncers[2]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['webkit-patch'], 'http://build.webkit.org/api/uploaded-file/453.dat');
            assert.equal(properties['checkbox'], 'build-webkit');
            assert.equal(properties['build-webkit'], true);
        });

        it('should resolve "ifBuilt"', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['ios-svn-webkit'] = {
                'repositories': {'WebKit': {}, 'Shared': {}, 'iOS': {}},
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'webkit': {'revision': 'WebKit'},
                    'shared': {'revision': 'Shared'},
                    'roots': {'roots': {}},
                    'test-custom-build': {'ifBuilt': [], 'value': ''},
                    'has-built-patch': {'ifBuilt': [], 'value': 'true'},
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const requestToBuild = createSampleBuildRequestWithPatch(MockModels.iphone, null, -1);
            const requestToTest = createSampleBuildRequestWithPatch(MockModels.iphone, MockModels.speedometer, 0);
            const otherRequestToTest = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);

            let properties = syncers[0]._propertiesForBuildRequest(requestToTest, [requestToTest]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['roots'], '[{"url":"http://build.webkit.org/api/uploaded-file/456.dat"}]');
            assert.equal(properties['test-custom-build'], undefined);
            assert.equal(properties['has-built-patch'], undefined);

            properties = syncers[0]._propertiesForBuildRequest(requestToTest, [requestToBuild, requestToTest]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['roots'], '[{"url":"http://build.webkit.org/api/uploaded-file/456.dat"}]');
            assert.equal(properties['test-custom-build'], '');
            assert.equal(properties['has-built-patch'], 'true');

            properties = syncers[0]._propertiesForBuildRequest(otherRequestToTest, [requestToBuild, otherRequestToTest, requestToTest]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['roots'], undefined);
            assert.equal(properties['test-custom-build'], undefined);
            assert.equal(properties['has-built-patch'], undefined);

        });

        it('should set the value for "ifBuilt" if the repository in the list appears', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['ios-svn-webkit'] = {
                'repositories': {'WebKit': {'acceptsPatch': true}, 'Shared': {}, 'iOS': {}},
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'webkit': {'revision': 'WebKit'},
                    'shared': {'revision': 'Shared'},
                    'roots': {'roots': {}},
                    'checkbox': {'ifBuilt': ['WebKit'], 'value': 'test-webkit'},
                    'test-webkit': {'ifBuilt': ['WebKit'], 'value': true}
                },
                'buildProperties': {
                    'webkit': {'revision': 'WebKit'},
                    'webkit-patch': {'patch': 'WebKit'},
                    'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-webkit'},
                    'build-webkit': {'ifRepositorySet': ['WebKit'], 'value': true},
                    'shared': {'revision': 'Shared'},
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const requestToBuild = createSampleBuildRequestWithPatch(MockModels.iphone, null, -1);
            const requestToTest = createSampleBuildRequestWithPatch(MockModels.iphone, MockModels.speedometer, 0);
            const properties = syncers[0]._propertiesForBuildRequest(requestToTest, [requestToBuild, requestToTest]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['roots'], '[{"url":"http://build.webkit.org/api/uploaded-file/456.dat"}]');
            assert.equal(properties['checkbox'], 'test-webkit');
            assert.equal(properties['test-webkit'], true);
        });

        it('should not set the value for "ifBuilt" if no build for the repository in the list appears', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['ios-svn-webkit-with-owned-commit'] = {
                'repositories': {'WebKit': {'acceptsPatch': true}, 'Owner Repository': {}, 'iOS': {}},
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'webkit': {'revision': 'WebKit'},
                    'owner-repo': {'revision': 'Owner Repository'},
                    'roots': {'roots': {}},
                    'checkbox': {'ifBuilt': ['WebKit'], 'value': 'test-webkit'},
                    'test-webkit': {'ifBuilt': ['WebKit'], 'value': true}
                },
                'buildProperties': {
                    'webkit': {'revision': 'WebKit'},
                    'webkit-patch': {'patch': 'WebKit'},
                    'owner-repo': {'revision': 'Owner Repository'},
                    'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-webkit'},
                    'build-webkit': {'ifRepositorySet': ['WebKit'], 'value': true},
                    'owned-commits': {'ownedRevisions': 'Owner Repository'}
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const requestToBuild =  createSampleBuildRequestWithOwnedCommit(MockModels.iphone, null, -1);
            const requestToTest = createSampleBuildRequestWithOwnedCommit(MockModels.iphone, MockModels.speedometer, 0);
            const properties = syncers[0]._propertiesForBuildRequest(requestToTest, [requestToBuild, requestToTest]);

            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['roots'], '[{"url":"http://build.webkit.org/api/uploaded-file/456.dat"}]');
            assert.equal(properties['checkbox'], undefined);
            assert.equal(properties['test-webkit'], undefined);
        });

        it('should resolve "ifRepositorySet" and "requiresBuild"', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['ios-svn-webkit-with-owned-commit'] = {
                'repositories': {'WebKit': {'acceptsPatch': true}, 'Owner Repository': {}, 'iOS': {}},
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'webkit': {'revision': 'WebKit'},
                    'owner-repo': {'revision': 'Owner Repository'},
                    'roots': {'roots': {}},
                },
                'buildProperties': {
                    'webkit': {'revision': 'WebKit'},
                    'webkit-patch': {'patch': 'WebKit'},
                    'owner-repo': {'revision': 'Owner Repository'},
                    'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-webkit'},
                    'build-webkit': {'ifRepositorySet': ['WebKit'], 'value': true},
                    'owned-commits': {'ownedRevisions': 'Owner Repository'}
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const request = createSampleBuildRequestWithOwnedCommit(MockModels.iphone, null, -1);
            const properties = syncers[2]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['owner-repo'], 'owner-001');
            assert.equal(properties['checkbox'], undefined);
            assert.equal(properties['build-webkit'], undefined);
            assert.deepEqual(JSON.parse(properties['owned-commits']), {'Owner Repository': [{revision: 'owned-002', repository: 'Owned Repository', ownerRevision: 'owner-001'}]});
        });

        it('should resolve "patch", "ifRepositorySet" and "requiresBuild"', () => {

            const config = sampleiOSConfig();
            config.repositoryGroups['ios-svn-webkit-with-owned-commit'] = {
                'repositories': {'WebKit': {'acceptsPatch': true}, 'Owner Repository': {}, 'iOS': {}},
                'testProperties': {
                    'os': {'revision': 'iOS'},
                    'webkit': {'revision': 'WebKit'},
                    'owner-repo': {'revision': 'Owner Repository'},
                    'roots': {'roots': {}},
                },
                'buildProperties': {
                    'webkit': {'revision': 'WebKit'},
                    'webkit-patch': {'patch': 'WebKit'},
                    'owner-repo': {'revision': 'Owner Repository'},
                    'checkbox': {'ifRepositorySet': ['WebKit'], 'value': 'build-webkit'},
                    'build-webkit': {'ifRepositorySet': ['WebKit'], 'value': true},
                    'owned-commits': {'ownedRevisions': 'Owner Repository'}
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const request = createSampleBuildRequestWithOwnedCommitAndPatch(MockModels.iphone, null, -1);
            const properties = syncers[2]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['webkit'], '197463');
            assert.equal(properties['owner-repo'], 'owner-001');
            assert.equal(properties['checkbox'], 'build-webkit');
            assert.equal(properties['build-webkit'], true);
            assert.equal(properties['webkit-patch'], 'http://build.webkit.org/api/uploaded-file/453.dat');
            assert.deepEqual(JSON.parse(properties['owned-commits']), {'Owner Repository': [{revision: 'owned-002', repository: 'Owned Repository', ownerRevision: 'owner-001'}]});
        });

        it('should allow to build with an owned component even if no repository accepts a patch in the triggerable repository group', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['owner-repository'] = {
                'repositories': {'Owner Repository': {}},
                'testProperties': {
                    'owner-repo': {'revision': 'Owner Repository'},
                    'roots': {'roots': {}},
                },
                'buildProperties': {
                    'owned-commits': {'ownedRevisions': 'Owner Repository'}
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const owner111289 = CommitLog.ensureSingleton('111289', {'id': '111289', 'time': 1456931874000, 'repository': MockModels.ownerRepository, 'revision': 'owner-001'});
            const owned111222 = CommitLog.ensureSingleton('111222', {'id': '111222', 'time': 1456932774000, 'repository': MockModels.ownedRepository, 'revision': 'owned-002'});
            const commitSet = CommitSet.ensureSingleton('53246486', {customRoots: [], revisionItems: [{commit: owner111289}, {commit: owned111222, commitOwner: owner111289, requiresBuild: true}]});
            const request = BuildRequest.ensureSingleton(`123123`, {'triggerable': MockModels.triggerable,
                repositoryGroup: MockModels.ownerRepositoryGroup,
                'commitSet': commitSet, 'status': 'pending', 'platform': MockModels.iphone, 'test': null, 'order': -1});

            const properties = syncers[2]._propertiesForBuildRequest(request, [request]);
            assert.deepEqual(JSON.parse(properties['owned-commits']), {'Owner Repository': [{revision: 'owned-002', repository: 'Owned Repository', ownerRevision: 'owner-001'}]});
        });

        it('should fail if build type build request does not have any build repository group template', () => {
            const config = sampleiOSConfig();
            config.repositoryGroups['owner-repository'] = {
                'repositories': {'Owner Repository': {}},
                'testProperties': {
                    'owner-repo': {'revision': 'Owner Repository'},
                    'roots': {'roots': {}},
                },
                'acceptsRoots': true,
            };
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, config, builderNameToIDMap());
            const owner1 = CommitLog.ensureSingleton('111289', {'id': '111289', 'time': 1456931874000, 'repository': MockModels.ownerRepository, 'revision': 'owner-001'});
            const owned2 = CommitLog.ensureSingleton('111222', {'id': '111222', 'time': 1456932774000, 'repository': MockModels.ownedRepository, 'revision': 'owned-002'});
            const commitSet = CommitSet.ensureSingleton('53246486', {customRoots: [], revisionItems: [{commit: owner1}, {commit: owned2, commitOwner: owner1, requiresBuild: true}]});
            const request = BuildRequest.ensureSingleton(`123123`, {'triggerable': MockModels.triggerable,
                repositoryGroup: MockModels.ownerRepositoryGroup,
                'commitSet': commitSet, 'status': 'pending', 'platform': MockModels.iphone, 'test': null, 'order': -1});

            assert.throws(() => syncers[2]._propertiesForBuildRequest(request, [request]),
                (error) => error.code === 'ERR_ASSERTION');
        });

        it('should set the property for the build request id', () => {
            const syncers = BuildbotSyncer._loadConfig(RemoteAPI, sampleiOSConfig(), builderNameToIDMap());
            const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            const properties = syncers[0]._propertiesForBuildRequest(request, [request]);
            assert.equal(properties['build_request_id'], request.id());
        });
    });


    describe('BuildbotBuildEntry', () => {
        it('should create BuildbotBuildEntry for pending build', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const buildbotData = samplePendingBuildRequests();
            const pendingEntries = buildbotData.buildrequests.map((entry) => new BuildbotBuildEntry(syncer, entry));

            assert.equal(pendingEntries.length, 1);
            const entry = pendingEntries[0];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.ok(!entry.buildNumber());
            assert.ok(!entry.workerName());
            assert.equal(entry.buildRequestId(), 16733);
            assert.ok(entry.isPending());
            assert.ok(!entry.isInProgress());
            assert.ok(!entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/buildrequests/17');
        });

        it('should create BuildbotBuildEntry for in-progress build', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const buildbotData = sampleInProgressBuild();
            const entries = buildbotData.builds.map((entry) => new BuildbotBuildEntry(syncer, entry));

            assert.equal(entries.length, 1);
            const entry = entries[0];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.equal(entry.buildNumber(), 614);
            assert.equal(entry.workerName(), 'ABTest-iPad-0');
            assert.equal(entry.buildRequestId(), 16733);
            assert.ok(!entry.isPending());
            assert.ok(entry.isInProgress());
            assert.ok(!entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');
        });

        it('should create BuildbotBuildEntry for finished build', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const buildbotData = sampleFinishedBuild();
            const entries = buildbotData.builds.map((entry) => new BuildbotBuildEntry(syncer, entry));

            assert.deepEqual(entries.length, 1);
            const entry = entries[0];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.equal(entry.buildNumber(), 1755);
            assert.equal(entry.workerName(), 'ABTest-iPad-0');
            assert.equal(entry.buildRequestId(), 18935);
            assert.ok(!entry.isPending());
            assert.ok(!entry.isInProgress());
            assert.ok(entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');
        });

        it('should create BuildbotBuildEntry for mix of in-progress and finished builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const buildbotData = {'builds': [sampleInProgressBuildData(), sampleFinishedBuildData()]};
            const entries = buildbotData.builds.map((entry) => new BuildbotBuildEntry(syncer, entry));

            assert.deepEqual(entries.length, 2);

            let entry = entries[0];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.equal(entry.buildNumber(), 614);
            assert.equal(entry.workerName(), 'ABTest-iPad-0');
            assert.equal(entry.buildRequestId(), 16733);
            assert.ok(!entry.isPending());
            assert.ok(entry.isInProgress());
            assert.ok(!entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');

            entry = entries[1];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.equal(entry.buildNumber(), 1755);
            assert.equal(entry.slaveName(), 'ABTest-iPad-0');
            assert.equal(entry.buildRequestId(), 18935);
            assert.ok(!entry.isPending());
            assert.ok(!entry.isInProgress());
            assert.ok(entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');
        });
    });

    describe('_pullRecentBuilds()', () => {
        it('should not fetch recent builds when count is zero', async () => {
            const syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const promise = syncer._pullRecentBuilds(0);
            assert.equal(requests.length, 0);
            const content = await promise;
            assert.deepEqual(content, []);
        });

        it('should pull the right number of recent builds', () => {
            const syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            syncer._pullRecentBuilds(12);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/v2/builders/102/builds?limit=12&order=-number&property=*');
        });

        it('should handle unexpected error while fetching recent builds', async () => {
            const syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const promise = syncer._pullRecentBuilds(2);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/v2/builders/102/builds?limit=2&order=-number&property=*');
            requests[0].resolve({'error': 'Unexpected error'});
            const content = await promise;
            assert.deepEqual(content, []);
        });

        it('should create BuildbotBuildEntry after fetching recent builds', async () => {
            const syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            const promise = syncer._pullRecentBuilds(2);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/v2/builders/102/builds?limit=2&order=-number&property=*');
            requests[0].resolve({'builds': [sampleFinishedBuildData(), sampleInProgressBuildData()]});

            const entries = await promise;
            assert.deepEqual(entries.length, 2);

            let entry = entries[0];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.equal(entry.buildNumber(), 1755);
            assert.equal(entry.workerName(), 'ABTest-iPad-0');
            assert.equal(entry.buildRequestId(), 18935);
            assert.ok(!entry.isPending());
            assert.ok(!entry.isInProgress());
            assert.ok(entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');

            entry = entries[1];
            assert.ok(entry instanceof BuildbotBuildEntry);
            assert.equal(entry.buildNumber(), 614);
            assert.equal(entry.slaveName(), 'ABTest-iPad-0');
            assert.equal(entry.buildRequestId(), 16733);
            assert.ok(!entry.isPending());
            assert.ok(entry.isInProgress());
            assert.ok(!entry.hasFinished());
            assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');
        });
    });

    describe('pullBuildbot', () => {
        it('should fetch pending builds from the right URL', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            assert.equal(syncer.builderName(), 'ABTest-iPad-RunBenchmark-Tests');
            let expectedURL = '/api/v2/builders/102/buildrequests?complete=false&claimed=false&property=*';
            assert.equal(syncer.pathForPendingBuilds(), expectedURL);
            syncer.pullBuildbot();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, expectedURL);
        });

        it('should fetch recent builds once pending builds have been fetched', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            assert.equal(syncer.builderName(), 'ABTest-iPad-RunBenchmark-Tests');

            syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/v2/builders/102/buildrequests?complete=false&claimed=false&property=*');
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/api/v2/builders/102/builds?limit=1&order=-number&property=*');
            });
        });

        it('should fetch the right number of recent builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            syncer.pullBuildbot(3);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/v2/builders/102/buildrequests?complete=false&claimed=false&property=*');
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '/api/v2/builders/102/builds?limit=3&order=-number&property=*');
            });
        });

        it('should create BuildbotBuildEntry for pending builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];
            let promise = syncer.pullBuildbot();
            requests[0].resolve(samplePendingBuildRequests());
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/buildrequests/17');
            });
        });

        it('should create BuildbotBuildEntry for in-progress builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            let promise = syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve(sampleInProgressBuild());
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');
            });
        });

        it('should create BuildbotBuildEntry for finished builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            let promise = syncer.pullBuildbot(1);
            assert.equal(requests.length, 1);
            requests[0].resolve([]);
            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve(sampleFinishedBuild());
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');
            });
        });

        it('should create BuildbotBuildEntry for mixed pending, in-progress, finished, and missing builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve(samplePendingBuildRequests(123));

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({'builds': [sampleFinishedBuildData(), sampleInProgressBuildData()]});
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/buildrequests/17');

                entry = entries[1];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');

                entry = entries[2];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');
            });
        });

        it('should sort BuildbotBuildEntry by order', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve({"buildrequests": [samplePendingBuildRequestData(456, 2), samplePendingBuildRequestData(123, 1)]});

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve({'builds': [sampleFinishedBuildData(), sampleInProgressBuildData()]});
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/buildrequests/17');

                entry = entries[1];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), null);
                assert.equal(entry.slaveName(), null);
                assert.equal(entry.buildRequestId(), 456);
                assert.ok(entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/#/buildrequests/17');

                entry = entries[2];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 614);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 16733);
                assert.ok(!entry.isPending());
                assert.ok(entry.isInProgress());
                assert.ok(!entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');

                entry = entries[3];
                assert.ok(entry instanceof BuildbotBuildEntry);
                assert.equal(entry.buildNumber(), 1755);
                assert.equal(entry.slaveName(), 'ABTest-iPad-0');
                assert.equal(entry.buildRequestId(), 18935);
                assert.ok(!entry.isPending());
                assert.ok(!entry.isInProgress());
                assert.ok(entry.hasFinished());
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');
            });
        });

        it('should override BuildbotBuildEntry for pending builds by in-progress builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve(samplePendingBuildRequests());

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve(sampleInProgressBuild());
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/614');
            });
        });

        it('should override BuildbotBuildEntry for pending builds by finished builds', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            let promise = syncer.pullBuildbot(5);
            assert.equal(requests.length, 1);

            requests[0].resolve(samplePendingBuildRequests());

            return MockRemoteAPI.waitForRequest().then(() => {
                assert.equal(requests.length, 2);
                requests[1].resolve(sampleFinishedBuild(16733));
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
                assert.equal(entry.url(), 'http://build.webkit.org/#/builders/102/builds/1755');
            });
        });
    });

    describe('scheduleBuildOnBuildbot', () => {
        it('should schedule a build request on Buildbot', async () => {
            const syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[0];
            const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            const properties = syncer._propertiesForBuildRequest(request, [request]);
            const promise  = syncer.scheduleBuildOnBuildbot(properties);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].method, 'POST');
            assert.equal(requests[0].url, '/api/v2/forceschedulers/ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');
            requests[0].resolve();
            await promise;
            assert.deepEqual(requests[0].data, {
                'id': '16733-' + MockModels.iphone.id(),
                'jsonrpc': '2.0',
                'method': 'force',
                'params': {
                    'build_request_id': '16733-' + MockModels.iphone.id(),
                    'desired_image': '13A452',
                    'opensource': '197463',
                    'forcescheduler': 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler',
                    'test_name': 'speedometer'
                }
            });
        });
    });

    describe('scheduleRequest', () => {
        it('should schedule a build request on a specified slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[0];

            const waitForRequest = MockRemoteAPI.waitForRequest();
            const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
            syncer.scheduleRequest(request, [request], 'some-slave');
            return waitForRequest.then(() => {
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/api/v2/forceschedulers/ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');
                assert.equal(requests[0].method, 'POST');
                assert.deepEqual(requests[0].data, {
                    'id': '16733-' + MockModels.iphone.id(),
                    'jsonrpc': '2.0',
                    'method': 'force',
                    'params': {
                        'build_request_id': '16733-' + MockModels.iphone.id(),
                        'desired_image': '13A452',
                        'opensource': '197463',
                        'forcescheduler': 'ABTest-iPhone-RunBenchmark-Tests-ForceScheduler',
                        'slavename': 'some-slave',
                        'test_name': 'speedometer'
                    }
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
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration(), builderNameToIDMap())[0];

            return pullBuildbotWithAssertion(syncer, {}, {}).then(() => {
                const request = createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest);
                syncer.scheduleRequestInGroupIfAvailable(request, [request]);
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/api/v2/forceschedulers/some-builder-ForceScheduler');
                assert.equal(requests[0].method, 'POST');
                assert.deepEqual(requests[0].data, {id: '16733-' + MockModels.somePlatform.id(), 'jsonrpc': '2.0', 'method': 'force',
                    'params': {id: '16733-' + MockModels.somePlatform.id(), 'forcescheduler': 'some-builder-ForceScheduler', 'os': '13A452', 'wk': '197463'}});
            });
        });

        it('should schedule a build if builder only has finished builds if slaveList is not specified', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration(), builderNameToIDMap())[0];

            return pullBuildbotWithAssertion(syncer, {}, smallFinishedBuild()).then(() => {
                const request = createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest);
                syncer.scheduleRequestInGroupIfAvailable(request, [request]);
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/api/v2/forceschedulers/some-builder-ForceScheduler');
                assert.equal(requests[0].method, 'POST');
                assert.deepEqual(requests[0].data, {id: '16733-' + MockModels.somePlatform.id(), 'jsonrpc': '2.0', 'method': 'force',
                    'params': {id: '16733-' + MockModels.somePlatform.id(), 'forcescheduler': 'some-builder-ForceScheduler', 'os': '13A452', 'wk': '197463'}});
            });
        });

        it('should not schedule a build if builder has a pending build if slaveList is not specified', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration(), builderNameToIDMap())[0];

            return pullBuildbotWithAssertion(syncer, smallPendingBuild(), {}).then(() => {
                syncer.scheduleRequestInGroupIfAvailable(createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest));
                assert.equal(requests.length, 0);
            });
        });

        it('should schedule a build if builder does not have pending or completed builds on the matching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[0];

            return pullBuildbotWithAssertion(syncer, {}, {}).then(() => {
                const request = createSampleBuildRequest(MockModels.iphone, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/api/v2/forceschedulers/ABTest-iPhone-RunBenchmark-Tests-ForceScheduler');
                assert.equal(requests[0].method, 'POST');
            });
        });

        it('should schedule a build if builder only has finished builds on the matching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            pullBuildbotWithAssertion(syncer, {}, sampleFinishedBuild()).then(() => {
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 1);
                assert.equal(requests[0].url, '/api/v2/forceschedulers/ABTest-iPad-RunBenchmark-Tests-ForceScheduler');
                assert.equal(requests[0].method, 'POST');
            });
        });

        it('should not schedule a build if builder has a pending build on the maching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            pullBuildbotWithAssertion(syncer, samplePendingBuildRequests(), {}).then(() => {
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 0);
            });
        });

        it('should schedule a build if builder only has a pending build on a non-maching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            return pullBuildbotWithAssertion(syncer, samplePendingBuildRequests(1, 1, 'another-slave'), {}).then(() => {
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 1);
            });
        });

        it('should schedule a build if builder only has an in-progress build on the matching slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            return pullBuildbotWithAssertion(syncer, {}, sampleInProgressBuild()).then(() => {
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 1);
            });
        });

        it('should schedule a build if builder has an in-progress build on another slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            return pullBuildbotWithAssertion(syncer, {}, sampleInProgressBuild('other-slave')).then(() => {
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 1);
            });
        });

        it('should not schedule a build if the request does not match any configuration', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[0];

            return pullBuildbotWithAssertion(syncer, {}, {}).then(() => {
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 0);
            });
        });

        it('should not schedule a build if a new request had been submitted to the same slave', (done) => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            pullBuildbotWithAssertion(syncer, {}, {}).then(() => {
                let request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequest(request, [request], 'ABTest-iPad-0');
                request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequest(request, [request], 'ABTest-iPad-1');
            }).then(() => {
                assert.equal(requests.length, 2);
                const request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
            }).then(() => {
                assert.equal(requests.length, 2);
                done();
            }).catch(done);
        });

        it('should schedule a build if a new request had been submitted to another slave', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, sampleiOSConfig(), builderNameToIDMap())[1];

            return pullBuildbotWithAssertion(syncer, {}, {}).then(() => {
                let request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer);
                syncer.scheduleRequest(request, [request], 'ABTest-iPad-0');
                assert.equal(requests.length, 1);
                request = createSampleBuildRequest(MockModels.ipad, MockModels.speedometer)
                syncer.scheduleRequestInGroupIfAvailable(request, [request], 'ABTest-iPad-1');
                assert.equal(requests.length, 2);
            });
        });

        it('should not schedule a build if a new request had been submitted to the same builder without slaveList', () => {
            let syncer = BuildbotSyncer._loadConfig(MockRemoteAPI, smallConfiguration(), builderNameToIDMap())[0];

            return pullBuildbotWithAssertion(syncer, {}, {}).then(() => {
                let request = createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest);
                syncer.scheduleRequest(request, [request], null);
                assert.equal(requests.length, 1);
                request = createSampleBuildRequest(MockModels.somePlatform, MockModels.someTest);
                syncer.scheduleRequestInGroupIfAvailable(request, [request], null);
                assert.equal(requests.length, 1);
            });
        });
    });
});
