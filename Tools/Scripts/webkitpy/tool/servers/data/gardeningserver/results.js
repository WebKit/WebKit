var results = results || {};

(function() {

var kTestResultsServer = 'http://test-results.appspot.com/';
var kTestType = 'layout-tests';
var kResultsName = 'full_results.json';
var kMasterName = 'ChromiumWebkit';
var kFailingResults = ['TIMEOUT', 'TEXT', 'CRASH', 'IMAGE','IMAGE+TEXT'];

function isFailure(result)
{
    return kFailingResults.indexOf(result) != -1;
}

function anyIsFailure(resultsList)
{
    return $.grep(resultsList, isFailure).length > 0;
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
    return anyIsFailure(unexpectedResults(resultNode));
}

function isResultNode(node)
{
    return !!node.actual;
}

results.BuilderResults = function(m_resultsJSON)
{
    this.m_resultsJSON = m_resultsJSON;
}

results.BuilderResults.prototype.unexpectedFailures = function() {
    return base.filterTree(this.m_resultsJSON.tests, isResultNode, isUnexpectedFailure);
}

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
}

})();
