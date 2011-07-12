var results = results || {};

(function() {

var kTestResultsServer = 'http://test-results.appspot.com/';
var kTestResultsQuery = kTestResultsServer + 'testfile?'
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

function resultsParameters(builderName, testName)
{
    return {
        builder: builderName,
        master: kMasterName,
        testtype: kTestType,
        name: name,
    };
}

function resultsSummaryURL(builderName, testName)
{
    return kTestResultsQuery + $.param(resultsParameters(builderName, testName));
}

function directoryOfResultsSummaryURL(builderName, testName)
{
    var parameters = resultsParameters(builderName, testName);
    parameters['dir'] = 1;
    return kTestResultsQuery + $.param(parameters);
}

function ResultsCache()
{
    this._cache = {};
}

ResultsCache.prototype._fetch = function(key, callback)
{
    var self = this;

    var url = kTestResultsServer + 'testfile?key=' + key;
    base.jsonp(url, function (resultsTree) {
        self._cache[key] = resultsTree;
        callback(resultsTree);
    });
};

// Warning! This function can call callback either synchronously or asynchronously.
// FIXME: Consider using setTimeout to make this method always asynchronous.
ResultsCache.prototype.get = function(key, callback)
{
    if (key in this._cache) {
        callback(this._cache[key]);
        return;
    }
    this._fetch(key, callback);
};

var g_resultsCache = new ResultsCache();

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
    this._resultsJSON = resultsJSON;
};

results.BuilderResults.prototype.unexpectedFailures = function()
{
    return base.filterTree(this._resultsJSON.tests, isResultNode, isUnexpectedFailure);
};

results.unexpectedFailuresByTest = function(resultsByBuilder)
{
    var unexpectedFailures = {};

    $.each(resultsByBuilder, function(builderName, builderResults) {
        $.each(builderResults.unexpectedFailures(), function(testName, resultNode) {
            unexpectedFailures[testName] = unexpectedFailures[testName] || {};
            unexpectedFailures[testName][builderName] = resultNode;
        });
    });

    return unexpectedFailures;
};

results.collectUnexpectedResults = function(dictionaryOfResultNodes)
{
    var collectedResults = {};
    var results = [];
    $.each(dictionaryOfResultNodes, function(key, resultNode) {
        results = results.concat(unexpectedResults(resultNode));
    });
    return base.uniquifyArray(results);
};

function TestHistoryWalker(builderName, testName)
{
    this._builderName = builderName;
    this._testName = testName;
    this._indexOfNextKeyToFetch = 0;
    this._keyList = [];
}

TestHistoryWalker.prototype.init = function(callback)
{
    var self = this;

    base.jsonp(directoryOfResultsSummaryURL(self._builderName, kResultsName), function(keyList) {
        self._keyList = keyList.map(function (element) { return element.key; });
        callback();
    });
};

TestHistoryWalker.prototype._fetchNextResultNode = function(callback)
{
    var self = this;

    if (self._indexOfNextKeyToFetch >= self._keyList) {
        callback(0, null);
        return;
    }

    var key = self._keyList[self._indexOfNextKeyToFetch];
    ++self._indexOfNextKeyToFetch;
    g_resultsCache.get(key, function(resultsTree) {
        var resultNode = results.resultNodeForTest(resultsTree, self._testName);
        var revision = parseInt(resultsTree['revision'])
        if (isNaN(revision))
            revision = 0;
        callback(revision, resultNode);
    });
};

TestHistoryWalker.prototype.walkHistory = function(callback)
{
    var self = this;
    self._fetchNextResultNode(function(revision, resultNode) {
        var shouldContinue = callback(revision, resultNode);
        if (!shouldContinue)
            return;
        self.walkHistory(callback);
    });
}

results.regressionRangeForFailure = function(builderName, testName, callback)
{
    var oldestFailingRevision = 0;
    var newestPassingRevision = 0;

    var historyWalker = new TestHistoryWalker(builderName, testName);
    historyWalker.init(function() {
        historyWalker.walkHistory(function(revision, resultNode) {
            if (!resultNode) {
                newestPassingRevision = revision;
                callback(oldestFailingRevision, newestPassingRevision);
                return false;
            }
            if (isUnexpectedFailure(resultNode)) {
                oldestFailingRevision = revision;
                return true;
            }
            if (!oldestFailingRevision)
                return true;  // We need to keep looking for a failing revision.
            newestPassingRevision = revision;
            callback(oldestFailingRevision, newestPassingRevision);
            return false;
        });
    });
};

results.resultNodeForTest = function(resultsTree, testName)
{
    var testNamePath = testName.split('/');
    var currentNode = resultsTree['tests'];
    $.each(testNamePath, function(index, segmentName) {
        if (!currentNode)
            return;
        currentNode = (segmentName in currentNode) ? currentNode[segmentName] : null;
    });
    return currentNode;
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

results.fetchResultsForBuilder = function(builderName, onsuccess)
{
    base.jsonp(resultsSummaryURL(builderName, kResultsName), function(resultsTree) {
        onsuccess(new results.BuilderResults(resultsTree));
    });
};

results.fetchResultsByBuilder = function(builderNameList, onsuccess)
{
    var resultsByBuilder = {}
    var requestsInFlight = builderNameList.length;
    $.each(builderNameList, function(index, builderName) {
        results.fetchResultsForBuilder(builderName, function(resultsTree) {
            resultsByBuilder[builderName] = resultsTree;
            --requestsInFlight;
            if (!requestsInFlight)
                onsuccess(resultsByBuilder);
        });
    });
};

})();
