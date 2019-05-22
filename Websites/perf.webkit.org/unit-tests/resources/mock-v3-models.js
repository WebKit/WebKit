'use strict';

var MockModels = {
    inject: function () {
        beforeEach(function () {
            AnalysisTask._fetchAllPromise = null;
            AnalysisTask.clearStaticMap();
            CommitLog.clearStaticMap();
            Metric.clearStaticMap();
            Platform.clearStaticMap();
            Repository.clearStaticMap();
            CommitSet.clearStaticMap();
            Test.clearStaticMap();
            TestGroup.clearStaticMap();
            BuildRequest.clearStaticMap();
            Triggerable.clearStaticMap();
            MeasurementSet._set = null;

            MockModels.osx = Repository.ensureSingleton(9, {name: 'OS X'});
            MockModels.ios = Repository.ensureSingleton(22, {name: 'iOS'});
            MockModels.webkit = Repository.ensureSingleton(11, {name: 'WebKit', url: 'http://trac.webkit.org/changeset/$1'});
            MockModels.ownedWebkit = Repository.ensureSingleton(191, {name: 'WebKit', url: 'http://trac.webkit.org/changeset/$1', owner: 9});
            MockModels.sharedRepository = Repository.ensureSingleton(16, {name: 'Shared'});
            MockModels.ownerRepository = Repository.ensureSingleton(111, {name: 'Owner Repository'});
            MockModels.ownedRepository = Repository.ensureSingleton(112, {name: 'Owned Repository', owner: 111});
            MockModels.webkitGit = Repository.ensureSingleton(17, {name: 'WebKit-Git'});
            MockModels.builder = new Builder(176, {name: 'WebKit Perf Builder', buildUrl: 'http://build.webkit.org/builders/$builderName/$buildNumber'});

            MockModels.someTest = Test.ensureSingleton(1, {name: 'Some test'});
            MockModels.someMetric = Metric.ensureSingleton(2884, {name: 'Some metric', test: MockModels.someTest});
            MockModels.somePlatform = Platform.ensureSingleton(65, {name: 'Some platform', metrics: [MockModels.someMetric],
                lastModifiedByMetric: {'2884': 5000, '1158': 5000}});

            MockModels.speedometer = Test.ensureSingleton(1928, {name: 'Speedometer'});
            MockModels.jetstream = Test.ensureSingleton(1886, {name: 'JetStream'});
            MockModels.dromaeo = Test.ensureSingleton(2022, {name: 'Dromaeo'});
            MockModels.domcore = Test.ensureSingleton(2023, {name: 'DOM Core Tests', parentId: 2022});

            MockModels.iphone = Platform.ensureSingleton(12, {name: 'iPhone', metrics: []});
            MockModels.ipad = Platform.ensureSingleton(13, {name: 'iPad', metrics: []});
            MockModels.iOS10iPhone = Platform.ensureSingleton(14, {name: 'iOS 10 iPhone', metrics: []});

            MockModels.plt = Test.ensureSingleton(844, {name: 'Page Load Test'});
            MockModels.iPadPLT = Test.ensureSingleton(1444, {name: 'PLT-iPad'});
            MockModels.iPhonePLT = Test.ensureSingleton(1500, {name: 'PLT-iPhone'});
            MockModels.pltMean = Metric.ensureSingleton(1158, {name: 'Time', aggregator: 'Arithmetic', test: MockModels.plt});
            MockModels.elCapitan = Platform.ensureSingleton(31, {name: 'El Capitan', metrics: [MockModels.pltMean]});

            MockModels.osRepositoryGroup = new TriggerableRepositoryGroup(31, {
                name: 'ios',
                repositories: [{repository: MockModels.ios}]
            });
            MockModels.svnRepositoryGroup = new TriggerableRepositoryGroup(32, {
                name: 'ios-svn-webkit',
                repositories: [{repository: MockModels.ios}, {repository: MockModels.webkit, acceptsPatch: true}, {repository: MockModels.sharedRepository}],
                acceptsCustomRoots: true,
            });
            MockModels.gitRepositoryGroup = new TriggerableRepositoryGroup(33, {
                name: 'ios-git-webkit',
                repositories: [{repository: MockModels.ios}, {repository: MockModels.webkitGit}, {repository: MockModels.sharedRepository}]
            });
            MockModels.svnRepositoryWithOwnedRepositoryGroup = new TriggerableRepositoryGroup(34, {
                name: 'ios-svn-webkit-with-owned-commit',
                repositories: [{repository: MockModels.ios}, {repository: MockModels.webkit, acceptsPatch: true}, {repository: MockModels.ownerRepository}],
                acceptsCustomRoots: true,
            });
            MockModels.ownerRepositoryGroup = new TriggerableRepositoryGroup(35, {
                name: 'owner-repository',
                repositories: [{repository: MockModels.ownerRepository}],
                acceptsCustomRoots: true
            });
            MockModels.triggerable = new Triggerable(3, {name: 'build-webkit',
                repositoryGroups: [MockModels.osRepositoryGroup, MockModels.svnRepositoryGroup, MockModels.gitRepositoryGroup, MockModels.svnRepositoryWithOwnedRepositoryGroup, MockModels.ownerRepositoryGroup],
                configurations: [{test: MockModels.iPhonePLT, platform: MockModels.iphone}]});

        });
    }
}

if (typeof module != 'undefined')
    module.exports.MockModels = MockModels;
