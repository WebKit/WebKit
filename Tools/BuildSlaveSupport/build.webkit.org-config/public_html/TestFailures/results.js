/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var results = results || {};

(function() {

var kTestResultsServer = 'http://test-results.appspot.com/';
var kTestResultsQuery = kTestResultsServer + 'testfile?'
var kTestType = 'layout-tests';
var kResultsName = 'full_results.json';
var kMasterName = 'ChromiumWebkit';

var kLayoutTestResultsServer = 'http://build.chromium.org/f/chromium/layout_test_results/';
var kLayoutTestResultsPath = '/results/layout-test-results/';

var PASS = 'PASS';
var TIMEOUT = 'TIMEOUT';
var TEXT = 'TEXT';
var CRASH = 'CRASH';
var IMAGE = 'IMAGE';
var IMAGE_TEXT = 'IMAGE+TEXT';

var kFailingResults = [TIMEOUT, TEXT, CRASH, IMAGE, IMAGE_TEXT];

var kExpectedImageSuffix = '-expected.png';
var kActualImageSuffix = '-actual.png';
var kImageDiffSuffix = '-diff.png';
var kTextDiffSuffix = '-diff.txt';
var kCrashLogSuffix = '-crash-log.txt';

var kPNGExtension = 'png';
var kTXTExtension = 'txt';

var kPreferredSuffixOrder = [
    kExpectedImageSuffix,
    kActualImageSuffix,
    kImageDiffSuffix,
    kTextDiffSuffix,
    kCrashLogSuffix,
    // FIXME: Add support for the rest of the result types.
];

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
        name: testName,
    };
}

function possibleSuffixListFor(failureTypeList)
{
    var suffixList = [];

    function pushImageSuffixes()
    {
        suffixList.push(kExpectedImageSuffix);
        suffixList.push(kActualImageSuffix);
        suffixList.push(kImageDiffSuffix);
    }

    function pushTextSuffixes()
    {
        // '-expected.txt',
        // '-actual.txt',
        suffixList.push(kTextDiffSuffix);
        // '-wdiff.html',
        // '-pretty-diff.html',
    }

    $.each(failureTypeList, function(index, failureType) {
        switch(failureType) {
        case IMAGE:
            pushImageSuffixes();
            break;
        case TEXT:
            pushTextSuffixes();
            break;
        case IMAGE_TEXT:
            pushImageSuffixes();
            pushTextSuffixes();
            break;
        case CRASH:
            suffixList.push(kCrashLogSuffix);
            break;
        default:
            // FIXME: Add support for the rest of the result types.
            // '-expected.html',
            // '-expected-mismatch.html',
            // '-expected.wav',
            // '-actual.wav',
            // ... and possibly more.
            break;
        }
    });

    return base.uniquifyArray(suffixList);
}

results.failureTypeToExtensionList = function(failureType)
{
    switch(failureType) {
    case IMAGE:
        return [kPNGExtension];
    case TEXT:
        return [kTXTExtension];
    case IMAGE_TEXT:
        return [kTXTExtension, kPNGExtension];
    default:
        // FIXME: Add support for the rest of the result types.
        // '-expected.html',
        // '-expected-mismatch.html',
        // '-expected.wav',
        // '-actual.wav',
        // ... and possibly more.
        return [];
    }
};

results.canRebaseline = function(failureTypeList)
{
    return failureTypeList.some(function(element) {
        return results.failureTypeToExtensionList(element).length > 0;
    });
};

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

var g_resultsCache = new base.AsynchronousCache(function(key, callback) {
    net.jsonp(kTestResultsServer + 'testfile?key=' + key, callback);
});

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

results.unexpectedResults = function(resultNode)
{
    var actualResults = resultNode.actual.split(' ');
    var expectedResults = addImpliedExpectations(resultNode.expected.split(' '))

    return $.grep(actualResults, function(result) {
        return expectedResults.indexOf(result) == -1;
    });
};

function isUnexpectedFailure(resultNode)
{
    if (!resultNode)
        return false;
    if (anyIsSuccess(resultNode.actual.split(' ')))
        return false;
    return anyIsFailure(results.unexpectedResults(resultNode));
}

function isUnexpectedSuccesses(resultNode)
{
    if (!resultNode)
        return false;
    if (anyIsFailure(resultNode.actual.split(' ')))
        return false;
    return anyIsSuccess(results.unexpectedResults(resultNode));
}

function isResultNode(node)
{
    return !!node.actual;
}

results.unexpectedFailures = function(resultsTree)
{
    return base.filterTree(resultsTree.tests, isResultNode, isUnexpectedFailure);
};

results.unexpectedSuccesses = function(resultsTree)
{
    return base.filterTree(resultsTree.tests, isResultNode, isUnexpectedSuccesses);
};

function resultsByTest(resultsByBuilder, filter)
{
    var resultsByTest = {};

    $.each(resultsByBuilder, function(builderName, resultsTree) {
        $.each(filter(resultsTree), function(testName, resultNode) {
            resultsByTest[testName] = resultsByTest[testName] || {};
            resultsByTest[testName][builderName] = resultNode;
        });
    });

    return resultsByTest;
}

results.unexpectedFailuresByTest = function(resultsByBuilder)
{
    return resultsByTest(resultsByBuilder, results.unexpectedFailures);
};

results.unexpectedSuccessesByTest = function(resultsByBuilder)
{
    return resultsByTest(resultsByBuilder, results.unexpectedSuccesses);
};

results.collectUnexpectedResults = function(dictionaryOfResultNodes)
{
    var collectedResults = [];
    $.each(dictionaryOfResultNodes, function(key, resultNode) {
        collectedResults = collectedResults.concat(results.unexpectedResults(resultNode));
    });
    return base.uniquifyArray(collectedResults);
};

function walkHistory(builderName, testName, callback)
{
    var indexOfNextKeyToFetch = 0;
    var keyList = [];

    function continueWalk()
    {
        if (indexOfNextKeyToFetch >= keyList.length) {
            processResultNode(0, null);
            return;
        }

        var key = keyList[indexOfNextKeyToFetch];
        ++indexOfNextKeyToFetch;
        g_resultsCache.get(key, function(resultsTree) {
            var resultNode = results.resultNodeForTest(resultsTree, testName);
            var revision = parseInt(resultsTree['revision'])
            if (isNaN(revision))
                revision = 0;
            processResultNode(revision, resultNode);
        });
    }

    function processResultNode(revision, resultNode)
    {
        var shouldContinue = callback(revision, resultNode);
        if (!shouldContinue)
            return;
        continueWalk();
    }

    net.jsonp(directoryOfResultsSummaryURL(builderName, kResultsName), function(directory) {
        keyList = directory.map(function (element) { return element.key; });
        continueWalk();
    });
}

results.regressionRangeForFailure = function(builderName, testName, callback)
{
    var oldestFailingRevision = 0;
    var newestPassingRevision = 0;

    walkHistory(builderName, testName, function(revision, resultNode) {
        if (!revision) {
            callback(oldestFailingRevision, newestPassingRevision);
            return false;
        }
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
};

function mergeRegressionRanges(regressionRanges)
{
    var mergedRange = {};

    mergedRange.oldestFailingRevision = 0;
    mergedRange.newestPassingRevision = 0;

    $.each(regressionRanges, function(builderName, range) {
        if (!range.oldestFailingRevision || !range.newestPassingRevision)
            return

        if (!mergedRange.oldestFailingRevision)
            mergedRange.oldestFailingRevision = range.oldestFailingRevision;
        if (!mergedRange.newestPassingRevision)
            mergedRange.newestPassingRevision = range.newestPassingRevision;

        if (range.oldestFailingRevision < mergedRange.oldestFailingRevision)
            mergedRange.oldestFailingRevision = range.oldestFailingRevision;
        if (range.newestPassingRevision > mergedRange.newestPassingRevision)
            mergedRange.newestPassingRevision = range.newestPassingRevision;
    });
    return mergedRange;
}

results.unifyRegressionRanges = function(builderNameList, testName, callback)
{
    var queriesInFlight = builderNameList.length;
    if (!queriesInFlight)
        callback(0, 0);

    var regressionRanges = {};
    $.each(builderNameList, function(index, builderName) {
        results.regressionRangeForFailure(builderName, testName, function(oldestFailingRevision, newestPassingRevision) {
            var range = {};
            range.oldestFailingRevision = oldestFailingRevision;
            range.newestPassingRevision = newestPassingRevision;
            regressionRanges[builderName] = range;

            --queriesInFlight;
            if (!queriesInFlight) {
                var mergedRange = mergeRegressionRanges(regressionRanges);
                callback(mergedRange.oldestFailingRevision, mergedRange.newestPassingRevision);
            }
        });
    });
};

results.countFailureOccurances = function(builderNameList, testName, callback)
{
    var queriesInFlight = builderNameList.length;
    if (!queriesInFlight)
        callback(0);

    var failureCount = 0;
    $.each(builderNameList, function(index, builderName) {
        walkHistory(builderName, testName, function(revision, resultNode) {
            if (isUnexpectedFailure(resultNode)) {
                ++failureCount;
                return true;
            }

            --queriesInFlight;
            if (!queriesInFlight)
                callback(failureCount);
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
    $.each(kPreferredSuffixOrder, function(i, suffix) {
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

results.fetchResultsURLs = function(failureInfo, callback)
{
    var stem = resultsDirectoryURL(failureInfo.builderName);
    var testNameStem = base.trimExtension(failureInfo.testName);

    var suffixList = possibleSuffixListFor(failureInfo.failureTypeList);

    var resultURLs = [];
    var requestsInFlight = suffixList.length;

    if (!requestsInFlight) {
        callback([]);
        return;
    }

    function checkComplete()
    {
        if (--requestsInFlight == 0)
            callback(sortResultURLsBySuffix(resultURLs));
    }

    $.each(suffixList, function(index, suffix) {
        var url = stem + testNameStem + suffix;
        net.probe(url, {
            success: function() {
                resultURLs.push(url);
                checkComplete();
            },
            error: checkComplete,
        });
    });
};

results.fetchResultsForBuilder = function(builderName, callback)
{
    net.jsonp(resultsSummaryURL(builderName, kResultsName), function(resultsTree) {
        callback(resultsTree);
    });
};

results.fetchResultsByBuilder = function(builderNameList, callback)
{
    var resultsByBuilder = {}
    var requestsInFlight = builderNameList.length;
    $.each(builderNameList, function(index, builderName) {
        results.fetchResultsForBuilder(builderName, function(resultsTree) {
            resultsByBuilder[builderName] = resultsTree;
            --requestsInFlight;
            if (!requestsInFlight)
                callback(resultsByBuilder);
        });
    });
};

})();
