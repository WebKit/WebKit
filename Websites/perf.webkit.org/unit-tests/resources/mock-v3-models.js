'use strict';

beforeEach(function () {
    AnalysisTask._fetchAllPromise = null;
    AnalysisTask.clearStaticMap();
    CommitLog.clearStaticMap();
    RootSet.clearStaticMap();
    TestGroup.clearStaticMap();
    BuildRequest.clearStaticMap();

    global.osx = Repository.ensureSingleton(9, {name: 'OS X'});
    global.ios = Repository.ensureSingleton(22, {name: 'iOS'});
    global.webkit = Repository.ensureSingleton(11, {name: 'WebKit', url: 'http://trac.webkit.org/changeset/$1'});
    global.sharedRepository = Repository.ensureSingleton(16, {name: 'Shared'});
    global.builder = new Builder(176, {name: 'WebKit Perf Builder', buildUrl: 'http://build.webkit.org/builders/$builderName/$buildNumber'});

    global.someTest = Test.ensureSingleton(1928, {name: 'Some test'});
    global.someMetric = Metric.ensureSingleton(2884, {name: 'Some metric', test: someTest});
    global.somePlatform = Platform.ensureSingleton(65, {name: 'Some platform', metrics: [someMetric]});
});
