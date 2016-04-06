'use strict';

function importFromV3(file, name) {
    const modelsDirectory = '../../public/v3/';

    global[name] = require(modelsDirectory + file)[name];
}

importFromV3('models/data-model.js', 'DataModelObject');
importFromV3('models/data-model.js', 'LabeledObject');

importFromV3('models/analysis-task.js', 'AnalysisTask');
importFromV3('models/bug-tracker.js', 'BugTracker');
importFromV3('models/build-request.js', 'BuildRequest');
importFromV3('models/builder.js', 'Build');
importFromV3('models/builder.js', 'Builder');
importFromV3('models/commit-log.js', 'CommitLog');
importFromV3('models/manifest.js', 'Manifest');
importFromV3('models/measurement-adaptor.js', 'MeasurementAdaptor');
importFromV3('models/measurement-cluster.js', 'MeasurementCluster');
importFromV3('models/measurement-set.js', 'MeasurementSet');
importFromV3('models/metric.js', 'Metric');
importFromV3('models/platform.js', 'Platform');
importFromV3('models/repository.js', 'Repository');
importFromV3('models/root-set.js', 'MeasurementRootSet');
importFromV3('models/root-set.js', 'RootSet');
importFromV3('models/test.js', 'Test');
importFromV3('models/test-group.js', 'TestGroup');

importFromV3('instrumentation.js', 'Instrumentation');

global.Statistics = require('../../public/shared/statistics.js');
