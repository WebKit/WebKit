'use strict';

function importFromV3(file, name)
{
    const modelsDirectory = '../../public/v3/';

    global[name] = require(modelsDirectory + file)[name];
}

importFromV3('models/data-model.js', 'DataModelObject');
importFromV3('models/data-model.js', 'LabeledObject');

importFromV3('models/analysis-task.js', 'AnalysisTask');
importFromV3('models/analysis-results.js', 'AnalysisResults');
importFromV3('models/bug.js', 'Bug');
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
importFromV3('models/commit-set.js', 'MeasurementCommitSet');
importFromV3('models/commit-set.js', 'CommitSet');
importFromV3('models/commit-set.js', 'CustomCommitSet');
importFromV3('models/commit-set.js', 'IntermediateCommitSet');
importFromV3('models/test.js', 'Test');
importFromV3('models/test-group.js', 'TestGroup');
importFromV3('models/time-series.js', 'TimeSeries');
importFromV3('models/triggerable.js', 'Triggerable');
importFromV3('models/triggerable.js', 'TriggerableRepositoryGroup');
importFromV3('models/uploaded-file.js', 'UploadedFile');

importFromV3('instrumentation.js', 'Instrumentation');
importFromV3('lazily-evaluated-function.js', 'LazilyEvaluatedFunction');
importFromV3('commit-set-range-bisector.js', 'CommitSetRangeBisector');
importFromV3('async-task.js', 'AsyncTask');

global.Statistics = require('../../public/shared/statistics.js');