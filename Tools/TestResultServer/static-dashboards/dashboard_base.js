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

// Generic utility functions.
function $(id)
{
    return document.getElementById(id);
}

function currentBuilderGroupCategory()
{
    switch (g_history.crossDashboardState.testType) {
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
    return g_history.crossDashboardState.group || Object.keys(currentBuilderGroupCategory())[0];
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

function isFlakinessDashboard()
{
    return string.endsWith(window.location.pathname, 'flakiness_dashboard.html');
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
    return g_history.isLayoutTestResults() ? LAYOUT_TEST_EXPECTATIONS_MAP_ : GTEST_EXPECTATIONS_MAP_;
}