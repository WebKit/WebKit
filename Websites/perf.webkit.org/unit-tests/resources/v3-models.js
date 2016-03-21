'use strict';

function importFromV3(file, name) {
    const modelsDirectory = '../../public/v3/';

    global[name] = require(modelsDirectory + file)[name];
}

importFromV3('models/data-model.js', 'DataModelObject');
importFromV3('models/data-model.js', 'LabeledObject');

importFromV3('models/analysis-task.js', 'AnalysisTask');
importFromV3('models/builder.js', 'Build');
importFromV3('models/builder.js', 'Builder');
importFromV3('models/commit-log.js', 'CommitLog');
importFromV3('models/measurement-adaptor.js', 'MeasurementAdaptor');
importFromV3('models/metric.js', 'Metric');
importFromV3('models/platform.js', 'Platform');
importFromV3('models/repository.js', 'Repository');
importFromV3('models/root-set.js', 'MeasurementRootSet');
importFromV3('models/root-set.js', 'RootSet');
importFromV3('models/test.js', 'Test');

importFromV3('instrumentation.js', 'Instrumentation');

global.Statistics = require('../../public/shared/statistics.js');

beforeEach(function () {
    AnalysisTask._fetchAllPromise = null;
    AnalysisTask.clearStaticMap();
    CommitLog.clearStaticMap();
    RootSet.clearStaticMap();

    global.osx = Repository.ensureSingleton(9, {name: 'OS X'});
    global.webkit = Repository.ensureSingleton(11, {name: 'WebKit', url: 'http://trac.webkit.org/changeset/$1'});
    global.builder = new Builder(176, {name: 'WebKit Perf Builder', buildUrl: 'http://build.webkit.org/builders/$builderName/$buildNumber'});

    global.someTest = Test.ensureSingleton(1928, {name: 'Some test'});
    global.someMetric = Metric.ensureSingleton(2884, {name: 'Some metric', test: someTest});
    global.somePlatform = Platform.ensureSingleton(65, {name: 'Some platform', metrics: [someMetric]});
});
