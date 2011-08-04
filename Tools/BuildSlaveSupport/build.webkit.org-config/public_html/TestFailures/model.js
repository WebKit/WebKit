var model = model || {};

(function () {

var kCommitLogLength = 50;

model.state = {};
model.state.failureAnalysisByTest = {};
model.state.rebaselineQueue = [];
model.state.expectationsUpdateQueue = [];

function findAndMarkRevertedRevisions(commitDataList)
{
    var revertedRevisions = {};
    $.each(commitDataList, function(index, commitData) {
        if (commitData.revertedRevision)
            revertedRevisions[commitData.revertedRevision] = true;
    });
    $.each(commitDataList, function(index, commitData) {
        if (commitData.revision in revertedRevisions)
            commitData.wasReverted = true;
    });
}

model.queueForRebaseline = function(failureInfo)
{
    model.state.rebaselineQueue.push(failureInfo);
};

model.takeRebaselineQueue = function()
{
    var queue = model.state.rebaselineQueue;
    model.state.rebaselineQueue = [];
    return queue;
};

model.queueForExpectationUpdate = function(failureInfo)
{
    model.state.expectationsUpdateQueue.push(failureInfo);
};

model.takeExpectationUpdateQueue = function()
{
    var queue = model.state.expectationsUpdateQueue;
    model.state.expectationsUpdateQueue = [];
    return queue;
};

model.updateRecentCommits = function(callback)
{
    trac.recentCommitData('trunk', kCommitLogLength, function(commitDataList) {
        model.state.recentCommits = commitDataList;
        findAndMarkRevertedRevisions(model.state.recentCommits);
        callback();
    });
};

model.updateResultsByBuilder = function(callback)
{
    results.fetchResultsByBuilder(config.kBuilders, function(resultsByBuilder) {
        model.state.resultsByBuilder = resultsByBuilder;
        callback();
    });
};

model.analyzeUnexpectedFailures = function(callback)
{
    var unexpectedFailures = results.unexpectedFailuresByTest(model.state.resultsByBuilder);

    $.each(model.state.failureAnalysisByTest, function(testName, failureAnalysis) {
        if (!(testName in unexpectedFailures))
            delete model.state.failureAnalysisByTest[testName];
    });

    $.each(unexpectedFailures, function(testName, resultNodesByBuilder) {
        var builderNameList = base.keys(resultNodesByBuilder);
        results.unifyRegressionRanges(builderNameList, testName, function(oldestFailingRevision, newestPassingRevision) {
            var failureAnalysis = {
                'testName': testName,
                'resultNodesByBuilder': resultNodesByBuilder,
                'oldestFailingRevision': oldestFailingRevision,
                'newestPassingRevision': newestPassingRevision,
            };

            var previousFailureAnalysis = model.state.failureAnalysisByTest[testName];
            if (previousFailureAnalysis
                && previousFailureAnalysis.oldestFailingRevision <= failureAnalysis.oldestFailingRevision
                && previousFailureAnalysis.newestPassingRevision >= failureAnalysis.newestPassingRevision) {
                failureAnalysis.oldestFailingRevision = previousFailureAnalysis.oldestFailingRevision;
                failureAnalysis.newestPassingRevision = previousFailureAnalysis.newestPassingRevision;
            }

            model.state.failureAnalysisByTest[testName] = failureAnalysis;
            callback(failureAnalysis);
        });
    });
};

})();
