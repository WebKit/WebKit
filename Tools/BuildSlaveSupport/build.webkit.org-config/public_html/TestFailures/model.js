var model = model || {};

(function () {

var kCommitLogLength = 50;

model.state = {};

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
    $.each(unexpectedFailures, function(testName, resultNodesByBuilder) {
        var builderNameList = base.keys(resultNodesByBuilder);
        results.unifyRegressionRanges(builderNameList, testName, function(oldestFailingRevision, newestPassingRevision) {
            var failureAnalysis = {
                'testName': testName,
                'resultNodesByBuilder': resultNodesByBuilder,
                'oldestFailingRevision': oldestFailingRevision,
                'newestPassingRevision': newestPassingRevision,
            };
            callback(failureAnalysis);
        });
    });
};

})();
