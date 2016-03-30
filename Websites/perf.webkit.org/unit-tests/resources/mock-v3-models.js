'use strict';

var MockModels = {
    inject: function () {
        beforeEach(function () {
            AnalysisTask._fetchAllPromise = null;
            AnalysisTask.clearStaticMap();
            CommitLog.clearStaticMap();
            Metric.clearStaticMap();
            RootSet.clearStaticMap();
            Test.clearStaticMap();
            TestGroup.clearStaticMap();
            BuildRequest.clearStaticMap();

            MockModels.osx = Repository.ensureSingleton(9, {name: 'OS X'});
            MockModels.ios = Repository.ensureSingleton(22, {name: 'iOS'});
            MockModels.webkit = Repository.ensureSingleton(11, {name: 'WebKit', url: 'http://trac.webkit.org/changeset/$1'});
            MockModels.sharedRepository = Repository.ensureSingleton(16, {name: 'Shared'});
            MockModels.builder = new Builder(176, {name: 'WebKit Perf Builder', buildUrl: 'http://build.webkit.org/builders/$builderName/$buildNumber'});

            MockModels.someTest = Test.ensureSingleton(1, {name: 'Some test'});
            MockModels.someMetric = Metric.ensureSingleton(2884, {name: 'Some metric', test: MockModels.someTest});
            MockModels.somePlatform = Platform.ensureSingleton(65, {name: 'Some platform', metrics: [MockModels.someMetric]});

            MockModels.speedometer = Test.ensureSingleton(1928, {name: 'Speedometer'});
            MockModels.jetstream = Test.ensureSingleton(1886, {name: 'JetStream'});
            MockModels.dromaeo = Test.ensureSingleton(2022, {name: 'Dromaeo'});
            MockModels.domcore = Test.ensureSingleton(2023, {name: 'DOM Core Tests', parentId: 2022});

            MockModels.iphone = Platform.ensureSingleton(12, {name: 'iPhone', metrics: []});
            MockModels.ipad = Platform.ensureSingleton(13, {name: 'iPad', metrics: []});

            MockModels.plt = Test.ensureSingleton(844, {name: 'Page Load Test'});
            MockModels.pltMean = Metric.ensureSingleton(1158, {name: 'Time', aggregator: 'Arithmetic', test: MockModels.plt});
            MockModels.elCapitan = Platform.ensureSingleton(31, {name: 'El Capitan', metrics: [MockModels.pltMean]});
        });
    }
}

if (typeof module != 'undefined')
    module.exports.MockModels = MockModels;
