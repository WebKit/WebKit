var results = results || {};

(function() {

var kTestResultsServer = 'http://test-results.appspot.com/';
var kTestType = 'layout-tests';
var kResultsName = 'full_results.json';
var kMasterName = 'ChromiumWebkit';

var PASS = 'PASS';
var TIMEOUT = 'TIMEOUT';
var TEXT = 'TEXT';
var CRASH = 'CRASH';
var IMAGE = 'IMAGE';
var IMAGE_TEXT = 'IMAGE+TEXT';

var kFailingResults = [TIMEOUT, TEXT, CRASH, IMAGE, IMAGE_TEXT];

function isFailure(result)
{
    return kFailingResults.indexOf(result) != -1;
}

function isSuccess(result)
{
    return result === PASS;
}

function anyIsFailure(resultsList)
{
    return $.grep(resultsList, isFailure).length > 0;
}

function anyIsSuccess(resultsList)
{
    return $.grep(resultsList, isSuccess).length > 0;
}

function addImpliedExpectations(resultsList)
{
    if (resultsList.indexOf('FAIL') == -1)
        return resultsList;
    return resultsList.concat(kFailingResults);
}

function unexpectedResults(resultNode)
{
    var actualResults = resultNode.actual.split(' ');
    var expectedResults = addImpliedExpectations(resultNode.expected.split(' '))

    return $.grep(actualResults, function(result) {
        return expectedResults.indexOf(result) == -1;
    });
}

function isUnexpectedFailure(resultNode)
{
    if (anyIsSuccess(resultNode.actual.split(' ')))
        return false;
    return anyIsFailure(unexpectedResults(resultNode));
}

function isResultNode(node)
{
    return !!node.actual;
}

results.BuilderResults = function(resultsJSON)
{
    this.m_resultsJSON = resultsJSON;
};

results.BuilderResults.prototype.unexpectedFailures = function()
{
    return base.filterTree(this.m_resultsJSON.tests, isResultNode, isUnexpectedFailure);
};

results.unexpectedFailuresByTest = function(resultsByBuilder)
{
    unexpectedFailures = {};

    $.each(resultsByBuilder, function(buildName, builderResults) {
        $.each(builderResults.unexpectedFailures(), function(testName, resultNode) {
            unexpectedFailures[testName] = unexpectedFailures[testName] || {};
            unexpectedFailures[testName][buildName] = resultNode;
        });
    });

    return unexpectedFailures;
};

function resultsURL(builderName, name)
{
    return kTestResultsServer + 'testfile' +
          '?builder=' + builderName +
          '&master=' + kMasterName +
          '&testtype=' + kTestType +
          '&name=' + name;
}

results.fetchResultsForBuilder = function(builderName, onsuccess)
{
    $.ajax({
        url: resultsURL(builderName, kResultsName),
        dataType: 'jsonp',
        success: function(data) {
            onsuccess(new results.BuilderResults(data));
        }
    });
};

results.fetchResultsByBuilder = function(builderNameList, onsuccess)
{
    var resultsByBuilder = {}
    var requestsInFlight = builderNameList.length;
    $.each(builderNameList, function(index, builderName) {
        results.fetchResultsForBuilder(builderName, function(builderResults) {
            resultsByBuilder[builderName] = builderResults;
            --requestsInFlight;
            if (!requestsInFlight)
                onsuccess(resultsByBuilder);
        });
    });
};

})();
