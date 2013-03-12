// Copyright (C) 2012 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//         * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//         * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//         * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// @fileoverview Base JS file for pages that want to parse the results JSON
// from the testing bots. This deals with generic utility functions, visible
// history, popups and appending the script elements for the JSON files.
//
// The calling page is expected to implement the following "abstract"
// functions/objects:
var g_resourceLoader;

// Generates the contents of the dashboard. The page should override this with
// a function that generates the page assuming all resources have loaded.
function generatePage() {}

// Takes a key and a value and sets the g_currentState[key] = value iff key is
// a valid hash parameter and the value is a valid value for that key.
//
// @return {boolean} Whether the key what inserted into the g_currentState.
function handleValidHashParameter(key, value)
{
    return false;
}

// Default hash parameters for the page. The page should override this to create
// default states.
var g_defaultDashboardSpecificStateValues = {};


// The page should override this to modify page state due to
// changing query parameters.
// @param {Object} params New or modified query params as key: value.
// @return {boolean} Whether changing this parameter should cause generatePage to be called.
function handleQueryParameterChange(params)
{
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS
//////////////////////////////////////////////////////////////////////////////
var GTEST_EXPECTATIONS_MAP_ = {
    'P': 'PASS',
    'F': 'FAIL',
    'N': 'NO DATA',
    'X': 'SKIPPED'
};

var LAYOUT_TEST_EXPECTATIONS_MAP_ = {
    'P': 'PASS',
    'N': 'NO DATA',
    'X': 'SKIP',
    'T': 'TIMEOUT',
    'F': 'TEXT',
    'C': 'CRASH',
    'I': 'IMAGE',
    'Z': 'IMAGE+TEXT',
    // We used to glob a bunch of expectations into "O" as OTHER. Expectations
    // are more precise now though and it just means MISSING.
    'O': 'MISSING'
};

var FAILURE_EXPECTATIONS_ = {
    'T': 1,
    'F': 1,
    'C': 1,
    'I': 1,
    'Z': 1
};

// Map of parameter to other parameter it invalidates.
var CROSS_DB_INVALIDATING_PARAMETERS = {
    'testType': 'group'
};
var DB_SPECIFIC_INVALIDATING_PARAMETERS;

// Keys in the JSON files.
var WONTFIX_COUNTS_KEY = 'wontfixCounts';
var FIXABLE_COUNTS_KEY = 'fixableCounts';
var DEFERRED_COUNTS_KEY = 'deferredCounts';
var WONTFIX_DESCRIPTION = 'Tests never to be fixed (WONTFIX)';
var FIXABLE_DESCRIPTION = 'All tests for this release';
var DEFERRED_DESCRIPTION = 'All deferred tests (DEFER)';
var FIXABLE_COUNT_KEY = 'fixableCount';
var ALL_FIXABLE_COUNT_KEY = 'allFixableCount';
var CHROME_REVISIONS_KEY = 'chromeRevision';
var WEBKIT_REVISIONS_KEY = 'webkitRevision';
var TIMESTAMPS_KEY = 'secondsSinceEpoch';
var BUILD_NUMBERS_KEY = 'buildNumbers';
var TESTS_KEY = 'tests';
var ONE_DAY_SECONDS = 60 * 60 * 24;
var ONE_WEEK_SECONDS = ONE_DAY_SECONDS * 7;

// These should match the testtype uploaded to test-results.appspot.com.
// See http://test-results.appspot.com/testfile.
var TEST_TYPES = [
    'base_unittests',
    'browser_tests',
    'cacheinvalidation_unittests',
    'compositor_unittests',
    'content_browsertests',
    'content_unittests',
    'courgette_unittests',
    'crypto_unittests',
    'googleurl_unittests',
    'gfx_unittests',
    'gl_tests',
    'gpu_tests',
    'gpu_unittests',
    'installer_util_unittests',
    'interactive_ui_tests',
    'ipc_tests',
    'jingle_unittests',
    'layout-tests',
    'media_unittests',
    'mini_installer_test',
    'net_unittests',
    'printing_unittests',
    'remoting_unittests',
    'safe_browsing_tests',
    'sql_unittests',
    'sync_unit_tests',
    'sync_integration_tests',
    'test_shell_tests',
    'ui_tests',
    'unit_tests',
    'views_unittests',
    'webkit_unit_tests',
    'androidwebview_instrumentation_tests',
    'chromiumtestshell_instrumentation_tests',
    'contentshell_instrumentation_tests',
    'cc_unittests'
];

var RELOAD_REQUIRING_PARAMETERS = ['showAllRuns', 'group', 'testType'];

// Enum for indexing into the run-length encoded results in the JSON files.
// 0 is where the count is length is stored. 1 is the value.
var RLE = {
    LENGTH: 0,
    VALUE: 1
}

function isFailingResult(value)
{
    return 'FSTOCIZ'.indexOf(value) != -1;
}

// Takes a key and a value and sets the g_currentState[key] = value iff key is
// a valid hash parameter and the value is a valid value for that key. Handles
// cross-dashboard parameters then falls back to calling
// handleValidHashParameter for dashboard-specific parameters.
//
// @return {boolean} Whether the key what inserted into the g_currentState.
function handleValidHashParameterWrapper(key, value)
{
    switch(key) {
    case 'testType':
        validateParameter(g_crossDashboardState, key, value,
            function() { return TEST_TYPES.indexOf(value) != -1; });
        return true;

    case 'group':
        validateParameter(g_crossDashboardState, key, value,
            function() {
              return value in LAYOUT_TESTS_BUILDER_GROUPS ||
                  value in CHROMIUM_GPU_TESTS_BUILDER_GROUPS ||
                  value in CHROMIUM_INSTRUMENTATION_TESTS_BUILDER_GROUPS ||
                  value in CHROMIUM_GTESTS_BUILDER_GROUPS;
            });
        return true;

    case 'useTestData':
    case 'showAllRuns':
        g_crossDashboardState[key] = value == 'true';
        return true;

    default:
        return handleValidHashParameter(key, value);
    }
}

var g_defaultCrossDashboardStateValues = {
    group: null,
    showAllRuns: false,
    testType: 'layout-tests',
    useTestData: false,
}

// Generic utility functions.
function $(id)
{
    return document.getElementById(id);
}


function validateParameter(state, key, value, validateFn)
{
    if (validateFn())
        state[key] = value;
    else
        console.log(key + ' value is not valid: ' + value);
}

function queryHashAsMap()
{
    var hash = window.location.hash;
    var paramsList = hash ? hash.substring(1).split('&') : [];
    var paramsMap = {};
    var invalidKeys = [];
    for (var i = 0; i < paramsList.length; i++) {
        var thisParam = paramsList[i].split('=');
        if (thisParam.length != 2) {
            console.log('Invalid query parameter: ' + paramsList[i]);
            continue;
        }

        paramsMap[thisParam[0]] = decodeURIComponent(thisParam[1]);
    }

    // FIXME: remove support for mapping from the master parameter to the group
    // one once the waterfall starts to pass in the builder name instead.
    if (paramsMap.master) {
        paramsMap.group = LEGACY_BUILDER_MASTERS_TO_GROUPS[paramsMap.master];
        if (!paramsMap.group)
            console.log('ERROR: Unknown master name: ' + paramsMap.master);
        window.location.hash = window.location.hash.replace('master=' + paramsMap.master, 'group=' + paramsMap.group);
        delete paramsMap.master;
    }

    return paramsMap;
}

function parseParameter(parameters, key)
{
    if (!(key in parameters))
        return;
    var value = parameters[key];
    if (!handleValidHashParameterWrapper(key, value))
        console.log("Invalid query parameter: " + key + '=' + value);
}

function parseCrossDashboardParameters()
{
    g_crossDashboardState = {};
    var parameters = queryHashAsMap();
    for (parameterName in g_defaultCrossDashboardStateValues)
        parseParameter(parameters, parameterName);

    fillMissingValues(g_crossDashboardState, g_defaultCrossDashboardStateValues);
}

function parseDashboardSpecificParameters()
{
    g_currentState = {};
    var parameters = queryHashAsMap();
    for (parameterName in g_defaultDashboardSpecificStateValues)
        parseParameter(parameters, parameterName);
}

// @return {boolean} Whether to generate the page.
function parseParameters()
{
    var oldCrossDashboardState = g_crossDashboardState;
    var oldDashboardSpecificState = g_currentState;

    parseCrossDashboardParameters();
    
    // Some parameters require loading different JSON files when the value changes. Do a reload.
    if (Object.keys(oldCrossDashboardState).length) {
        for (var key in g_crossDashboardState) {
            if (oldCrossDashboardState[key] != g_crossDashboardState[key] && RELOAD_REQUIRING_PARAMETERS.indexOf(key) != -1) {
                window.location.reload();
                return false;
            }
        }
    }

    parseDashboardSpecificParameters();
    var dashboardSpecificDiffState = diffStates(oldDashboardSpecificState, g_currentState);

    fillMissingValues(g_currentState, g_defaultDashboardSpecificStateValues);

    // FIXME: dashboard_base shouldn't know anything about specific dashboard specific keys.
    if (dashboardSpecificDiffState.builder)
        delete g_currentState.tests;
    if (g_currentState.tests)
        delete g_currentState.builder;

    var shouldGeneratePage = true;
    if (Object.keys(dashboardSpecificDiffState).length)
        shouldGeneratePage = handleQueryParameterChange(dashboardSpecificDiffState);
    return shouldGeneratePage;
}

function diffStates(oldState, newState)
{
    // If there is no old state, everything in the current state is new.
    if (!oldState)
        return newState;

    var changedParams = {};
    for (curKey in newState) {
        var oldVal = oldState[curKey];
        var newVal = newState[curKey];
        // Add new keys or changed values.
        if (!oldVal || oldVal != newVal)
            changedParams[curKey] = newVal;
    }
    return changedParams;
}

function defaultValue(key)
{
    if (key in g_defaultDashboardSpecificStateValues)
        return g_defaultDashboardSpecificStateValues[key];
    return g_defaultCrossDashboardStateValues[key];
}

function fillMissingValues(to, from)
{
    for (var state in from) {
        if (!(state in to))
            to[state] = from[state];
    }
}

// FIXME: Rename this to g_dashboardSpecificState;
var g_currentState = {};
var g_crossDashboardState = {};
parseCrossDashboardParameters();

function isLayoutTestResults()
{
    return g_crossDashboardState.testType == 'layout-tests';
}

function isGPUTestResults()
{
    return g_crossDashboardState.testType == 'gpu_tests';
}

function currentBuilderGroupCategory()
{
    switch (g_crossDashboardState.testType) {
    case 'gl_tests':
    case 'gpu_tests':
        return CHROMIUM_GPU_TESTS_BUILDER_GROUPS;
    case 'layout-tests':
        return LAYOUT_TESTS_BUILDER_GROUPS;
    case 'test_shell_tests':
    case 'webkit_unit_tests':
        return TEST_SHELL_TESTS_BUILDER_GROUPS;
    case 'androidwebview_instrumentation_tests':
    case 'chromiumtestshell_instrumentation_tests':
    case 'contentshell_instrumentation_tests':
        return CHROMIUM_INSTRUMENTATION_TESTS_BUILDER_GROUPS;
    case 'cc_unittests':
        return CC_UNITTEST_BUILDER_GROUPS;
    default:
        return CHROMIUM_GTESTS_BUILDER_GROUPS;
    }
}

function currentBuilderGroupName()
{
    return g_crossDashboardState.group || Object.keys(currentBuilderGroupCategory())[0];
}

function currentBuilderGroup()
{
    return currentBuilderGroupCategory()[currentBuilderGroupName()];
}

function currentBuilders()
{
    return currentBuilderGroup().builders;
}

function isTipOfTreeWebKitBuilder()
{
    return currentBuilderGroup().isToTWebKit;
}

var g_resultsByBuilder = {};
var g_expectationsByPlatform = {};

// TODO(aboxhall): figure out whether this is a performance bottleneck and
// change calling code to understand the trie structure instead if necessary.
function flattenTrie(trie, prefix)
{
    var result = {};
    for (var name in trie) {
        var fullName = prefix ? prefix + "/" + name : name;
        var data = trie[name];
        if ("results" in data)
            result[fullName] = data;
        else {
            var partialResult = flattenTrie(data, fullName);
            for (var key in partialResult) {
                result[key] = partialResult[key];
            }
        }
    }
    return result;
}

function isTreeMap()
{
    return string.endsWith(window.location.pathname, 'treemap.html');
}

function isFlakinessDashboard()
{
    return string.endsWith(window.location.pathname, 'flakiness_dashboard.html');
}

function handleLocationChange()
{
    if (!g_resourceLoader.isLoadingComplete())
        return;

    if (parseParameters())
        generatePage();
}

window.onhashchange = handleLocationChange;

function combinedDashboardState()
{
    var combinedState = Object.create(g_currentState);
    for (var key in g_crossDashboardState)
        combinedState[key] = g_crossDashboardState[key];
    return combinedState;    
}

function invalidateQueryParameters(queryParamsAsState) {
    for (var key in queryParamsAsState) {
        if (key in CROSS_DB_INVALIDATING_PARAMETERS)
            delete g_crossDashboardState[CROSS_DB_INVALIDATING_PARAMETERS[key]];
        if (DB_SPECIFIC_INVALIDATING_PARAMETERS && key in DB_SPECIFIC_INVALIDATING_PARAMETERS)
            delete g_currentState[DB_SPECIFIC_INVALIDATING_PARAMETERS[key]];
    }
}

// Sets the page state. Takes varargs of key, value pairs.
function setQueryParameter(var_args)
{
    var queryParamsAsState = {};
    for (var i = 0; i < arguments.length; i += 2) {
        var key = arguments[i];
        queryParamsAsState[key] = arguments[i + 1];
    }

    invalidateQueryParameters(queryParamsAsState);

    var newState = combinedDashboardState();
    for (var key in queryParamsAsState) {
        newState[key] = queryParamsAsState[key];
    }

    // Note: We use window.location.hash rather that window.location.replace
    // because of bugs in Chrome where extra entries were getting created
    // when back button was pressed and full page navigation was occuring.
    // FIXME: file those bugs.
    window.location.hash = permaLinkURLHash(newState);
}

function permaLinkURLHash(opt_state)
{
    var state = opt_state || combinedDashboardState();
    return '#' + joinParameters(state);
}

function joinParameters(stateObject)
{
    var state = [];
    for (var key in stateObject) {
        var value = stateObject[key];
        if (value != defaultValue(key))
            state.push(key + '=' + encodeURIComponent(value));
    }
    return state.join('&');
}

// Create a new function with some of its arguements
// pre-filled.
// Taken from goog.partial in the Closure library.
// @param {Function} fn A function to partially apply.
// @param {...*} var_args Additional arguments that are partially
//         applied to fn.
// @return {!Function} A partially-applied form of the function bind() was
//         invoked as a method of.
function partial(fn, var_args)
{
    var args = Array.prototype.slice.call(arguments, 1);
    return function() {
        // Prepend the bound arguments to the current arguments.
        var newArgs = Array.prototype.slice.call(arguments);
        newArgs.unshift.apply(newArgs, args);
        return fn.apply(this, newArgs);
    };
};

// Returns the appropriate expectations map for the current testType.
function expectationsMap()
{
    return isLayoutTestResults() ? LAYOUT_TEST_EXPECTATIONS_MAP_ : GTEST_EXPECTATIONS_MAP_;
}

function toggleQueryParameter(param)
{
    setQueryParameter(param, !queryParameterValue(param));
}

function queryParameterValue(parameter)
{
    return g_currentState[parameter] || g_crossDashboardState[parameter];
}

// "Decompresses" the RLE-encoding of test results so that we can query it
// by build index and test name.
//
// @param {Object} results results for the current builder
// @return Object with these properties:
//     - testNames: array mapping test index to test names.
//     - resultsByBuild: array of builds, for each build a (sparse) array of test results by test index.
//     - flakyTests: array with the boolean value true at test indices that are considered flaky (more than one single-build failure).
//     - flakyDeltasByBuild: array of builds, for each build a count of flaky test results by expectation, as well as a total.
function decompressResults(builderResults)
{
    var builderTestResults = builderResults[TESTS_KEY];
    var buildCount = builderResults[FIXABLE_COUNTS_KEY].length;
    var resultsByBuild = new Array(buildCount);
    var flakyDeltasByBuild = new Array(buildCount);

    // Pre-sizing the test result arrays for each build saves us ~250ms
    var testCount = 0;
    for (var testName in builderTestResults)
        testCount++;
    for (var i = 0; i < buildCount; i++) {
        resultsByBuild[i] = new Array(testCount);
        resultsByBuild[i][testCount - 1] = undefined;
        flakyDeltasByBuild[i] = {};
    }

    // Using indices instead of the full test names for each build saves us
    // ~1500ms
    var testIndex = 0;
    var testNames = new Array(testCount);
    var flakyTests = new Array(testCount);

    // Decompress and "invert" test results (by build instead of by test) and
    // determine which are flaky.
    for (var testName in builderTestResults) {
        var oneBuildFailureCount = 0;

        testNames[testIndex] = testName;
        var testResults = builderTestResults[testName].results;
        for (var i = 0, rleResult, currentBuildIndex = 0; (rleResult = testResults[i]) && currentBuildIndex < buildCount; i++) {
            var count = rleResult[RLE.LENGTH];
            var value = rleResult[RLE.VALUE];

            if (count == 1 && value in FAILURE_EXPECTATIONS_)
                oneBuildFailureCount++;

            for (var j = 0; j < count; j++) {
                resultsByBuild[currentBuildIndex++][testIndex] = value;
                if (currentBuildIndex == buildCount)
                    break;
            }
        }

        if (oneBuildFailureCount > 2)
            flakyTests[testIndex] = true;

        testIndex++;
    }

    // Now that we know which tests are flaky, count the test results that are
    // from flaky tests for each build.
    testIndex = 0;
    for (var testName in builderTestResults) {
        if (!flakyTests[testIndex++])
            continue;

        var testResults = builderTestResults[testName].results;
        for (var i = 0, rleResult, currentBuildIndex = 0; (rleResult = testResults[i]) && currentBuildIndex < buildCount; i++) {
            var count = rleResult[RLE.LENGTH];
            var value = rleResult[RLE.VALUE];

            for (var j = 0; j < count; j++) {
                var buildTestResults = flakyDeltasByBuild[currentBuildIndex++];
                function addFlakyDelta(key)
                {
                    if (!(key in buildTestResults))
                        buildTestResults[key] = 0;
                    buildTestResults[key]++;
                }
                addFlakyDelta(value);
                if (value != 'P' && value != 'N')
                    addFlakyDelta('total');
                if (currentBuildIndex == buildCount)
                    break;
            }
        }
    }

    return {
        testNames: testNames,
        resultsByBuild: resultsByBuild,
        flakyTests: flakyTests,
        flakyDeltasByBuild: flakyDeltasByBuild
    };
}

window.addEventListener('load', function() {
    g_resourceLoader = new loader.Loader(handleLocationChange);
    g_resourceLoader.load();
}, false);
