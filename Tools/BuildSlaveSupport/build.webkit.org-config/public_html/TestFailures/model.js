var model = model || {};

(function () {

var kCommitLogLength = 20;

model.state = {};

model.updateRecentCommits = function(callback)
{
    trac.recentCommitData('trunk', kCommitLogLength, function(commitDataList) {
        model.state.recentCommits = commitDataList;
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
