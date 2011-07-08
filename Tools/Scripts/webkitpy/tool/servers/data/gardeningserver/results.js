var results = results || {};

(function() {

var kTestResultsServer = 'http://test-results.appspot.com/';
var kTestType = 'layout-tests';
var kResultsName = 'full_results.json';
var kMasterName = 'ChromiumWebkit';

var kLayoutTestResultsServer = 'http://build.chromium.org/f/chromium/layout_test_results/';
var kLayoutTestResultsPath = '/results/layout-test-results/';

var kPossibleSuffixList = [
    '-expected.png',
    '-actual.png',
    '-diff.png',
    '-expected.txt',
    '-actual.txt',
    '-diff.txt',
    // FIXME: Add support for these result types.
    // '-wdiff.html',
    // '-pretty-diff.html',
    // '-expected.html',
    // '-expected-mismatch.html',
    // '-expected.wav',
    // '-actual.wav',
    // ... and possibly more.
];

var PASS = 'PASS';
var TIMEOUT = 'TIMEOUT';
var TEXT = 'TEXT';
var CRASH = 'CRASH';
var IMAGE = 'IMAGE';
var IMAGE_TEXT = 'IMAGE+TEXT';

var kFailingResults = [TIMEOUT, TEXT, CRASH, IMAGE, IMAGE_TEXT];

// Kinds of results.
results.kActualKind = 'actual';
results.kExpectedKind = 'expected';
results.kDiffKind = 'diff';
results.kUnknownKind = 'unknown';

// Types of tests.
results.kImageType = 'image'
results.kTextType = 'text'
// FIXME: There are more types of tests.

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
    var unexpectedFailures = {};

    $.each(resultsByBuilder, function(buildName, builderResults) {
        $.each(builderResults.unexpectedFailures(), function(testName, resultNode) {
            unexpectedFailures[testName] = unexpectedFailures[testName] || {};
            unexpectedFailures[testName][buildName] = resultNode;
        });
    });

    return unexpectedFailures;
};

function resultsDirectoryForBuilder(builderName)
{
    return builderName.replace(/[ .()]/g, '_');
}

function resultsDirectoryURL(builderName)
{
    return kLayoutTestResultsServer + resultsDirectoryForBuilder(builderName) + kLayoutTestResultsPath;
}

results.resultKind = function(url)
{
    if (/-actual\.[a-z]+$/.test(url))
        return results.kActualKind;
    else if (/-expected\.[a-z]+$/.test(url))
        return results.kExpectedKind;
    else if (/diff\.[a-z]+$/.test(url))
        return results.kDiffKind;
    return results.kUnknownKind;
}

results.resultType = function(url)
{
    if (/\.png$/.test(url))
        return results.kImageType;
    return results.kTextType;
}

function sortResultURLsBySuffix(urls)
{
    var sortedURLs = [];
    $.each(kPossibleSuffixList, function(i, suffix) {
        $.each(urls, function(j, url) {
            if (!base.endsWith(url, suffix))
                return;
            sortedURLs.push(url);
        });
    });
    if (sortedURLs.length != urls.length)
        throw "sortResultURLsBySuffix failed to return the same number of URLs."
    return sortedURLs;
}

results.fetchResultsURLs = function(builderName, testName, callback)
{
    var stem = resultsDirectoryURL(builderName);
    var testNameStem = base.trimExtension(testName);

    var resultURLs = [];
    var requestsInFlight = kPossibleSuffixList.length;

    function checkComplete()
    {
        if (--requestsInFlight == 0)
            callback(sortResultURLsBySuffix(resultURLs));
    }

    $.each(kPossibleSuffixList, function(index, suffix) {
        var url = stem + testNameStem + suffix;
        base.probe(url, {
            success: function() {
                resultURLs.push(url);
                checkComplete();
            },
            error: checkComplete,
        });
    });
};

function resultsSummaryURL(builderName, name)
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
        url: resultsSummaryURL(builderName, kResultsName),
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
